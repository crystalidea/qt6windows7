// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qffmpegwindowcapture_uwp_p.h"
#include "qffmpegsurfacecapturegrabber_p.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qthread.h>
#include <QtCore/private/qfactorycacheregistration_p.h>
#include <QtCore/private/qsystemerror_p.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qwindow.h>
#include <QtGui/qpa/qplatformscreen_p.h>
#include <QtMultimedia/qabstractvideobuffer.h>
#include <QtMultimedia/qvideoframe.h>
#include <QtMultimedia/private/qcapturablewindow_p.h>
#include <QtMultimedia/private/qmultimediautils_p.h>
#include <QtMultimedia/private/qvideoframe_p.h>

#include <unknwn.h>
#include <winrt/base.h>
// Workaround for Windows SDK bug.
// See https://github.com/microsoft/Windows.UI.Composition-Win32-Samples/issues/47
namespace winrt::impl
{
template <typename Async>
auto wait_for(Async const& async, Windows::Foundation::TimeSpan const& timeout);
}
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <Windows.Graphics.Capture.h>
#include <Windows.Graphics.Capture.Interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <D3d11.h>
#include <dwmapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>

#include <memory>
#include <system_error>

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace Windows::Graphics::DirectX::Direct3D11;

using winrt::check_hresult;
using winrt::com_ptr;
using winrt::guid_of;

namespace {

Q_LOGGING_CATEGORY(qLcWindowCaptureUwp, "qt.multimedia.ffmpeg.windowcapture.uwp");

// Windows 7 backport: CreateDirect3D11DeviceFromDXGIDevice() was introduced in Windows 8.1.
// d3d11.dll itself is available on Windows 7 SP1 with the Platform Update (KB2670838), so the
// module is found and only this one export is missing - but a static import of it is enough to
// make the whole ffmpeg media plugin unloadable on Windows 7 (ERROR_PROC_NOT_FOUND). Qt then
// reports no multimedia backend at all, so playback stops working for a function that is only
// ever needed for window capture. Resolve it at run time instead: behaviour is unchanged on
// Windows 8.1 and later, and on Windows 7 only window capture fails, through the regular
// hresult error path the callers already handle.
HRESULT createDirect3D11DeviceFromDXGIDevice(IDXGIDevice *dxgiDevice, IInspectable **graphicsDevice)
{
    using CreateDeviceFn = HRESULT(WINAPI *)(IDXGIDevice *, IInspectable **);

    static const CreateDeviceFn createDevice = []() -> CreateDeviceFn {
        // LoadLibraryExW, not LoadLibraryW: this plugin already imports the former, so
        // resolving the export here adds no import of its own. d3d11.dll is statically
        // imported anyway (D3D11CreateDevice), so this only bumps its reference count.
        const HMODULE d3d11 = ::LoadLibraryExW(L"d3d11.dll", nullptr, 0);
        if (!d3d11)
            return nullptr;

        return reinterpret_cast<CreateDeviceFn>(
                ::GetProcAddress(d3d11, "CreateDirect3D11DeviceFromDXGIDevice"));
    }();

    if (!createDevice)
        return E_NOTIMPL;

    return createDevice(dxgiDevice, graphicsDevice);
}

winrt::Windows::Graphics::SizeInt32 getWindowSize(HWND hwnd)
{
    RECT windowRect{};
    ::GetWindowRect(hwnd, &windowRect);

    return { windowRect.right - windowRect.left, windowRect.bottom - windowRect.top };
}

QSize asQSize(winrt::Windows::Graphics::SizeInt32 size)
{
    return { size.Width, size.Height };
}

struct MultithreadedApartment
{
    MultithreadedApartment(const MultithreadedApartment &) = delete;
    MultithreadedApartment &operator=(const MultithreadedApartment &) = delete;

    MultithreadedApartment() { winrt::init_apartment(); }
    ~MultithreadedApartment() { winrt::uninit_apartment(); }
};

class QUwpTextureVideoBuffer : public QAbstractVideoBuffer
{
public:
    QUwpTextureVideoBuffer(com_ptr<IDXGISurface> &&surface) : m_surface(surface) { }

    ~QUwpTextureVideoBuffer() override { Q_ASSERT(m_mapMode == QVideoFrame::NotMapped); }

    MapData map(QVideoFrame::MapMode mode) override
    {
        if (m_mapMode != QVideoFrame::NotMapped)
            return {};

        if (mode == QVideoFrame::ReadOnly) {
            DXGI_MAPPED_RECT rect = {};
            HRESULT hr = m_surface->Map(&rect, DXGI_MAP_READ);
            if (SUCCEEDED(hr)) {
                DXGI_SURFACE_DESC desc = {};
                hr = m_surface->GetDesc(&desc);

                MapData md = {};
                md.planeCount = 1;
                md.bytesPerLine[0] = rect.Pitch;
                md.data[0] = rect.pBits;
                md.dataSize[0] = rect.Pitch * desc.Height;

                m_mapMode = QVideoFrame::ReadOnly;

                return md;
            } else {
                qCDebug(qLcWindowCaptureUwp)
                        << "Failed to map DXGI surface" << QSystemError::windowsComString(hr);
                return {};
            }
        }

        return {};
    }

    void unmap() override
    {
        if (m_mapMode == QVideoFrame::NotMapped)
            return;

        const HRESULT hr = m_surface->Unmap();
        if (FAILED(hr))
            qCDebug(qLcWindowCaptureUwp)
                    << "Failed to unmap surface" << QSystemError::windowsComString(hr);

        m_mapMode = QVideoFrame::NotMapped;
    }

    QVideoFrameFormat format() const override { return {}; }

private:
    QVideoFrame::MapMode m_mapMode = QVideoFrame::NotMapped;
    com_ptr<IDXGISurface> m_surface;
};

struct WindowGrabber
{
    WindowGrabber() = default;

    WindowGrabber(IDXGIAdapter1 *adapter, HWND hwnd)
        : m_captureWindow{ hwnd }, m_frameSize{ getWindowSize(hwnd) }
    {
        check_hresult(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0,
                                        D3D11_SDK_VERSION, m_device.put(), nullptr, nullptr));

        const auto captureItem = createCaptureItem(hwnd);

        m_framePool = Direct3D11CaptureFramePool::CreateFreeThreaded(
                getCaptureDevice(m_device), m_pixelFormat, 1,
                captureItem.Size());

        m_session = m_framePool.CreateCaptureSession(captureItem);

        // If supported, enable cursor capture
        if (const auto session2 = m_session.try_as<IGraphicsCaptureSession2>())
            session2.IsCursorCaptureEnabled(true);

        // If supported, disable colored border around captured window to match other platforms
        if (const auto session3 = m_session.try_as<IGraphicsCaptureSession3>())
            session3.IsBorderRequired(false);

        m_session.StartCapture();
    }

    ~WindowGrabber()
    {
        m_framePool.Close();
        m_session.Close();
    }

    com_ptr<IDXGISurface> tryGetFrame()
    {
        const Direct3D11CaptureFrame frame = m_framePool.TryGetNextFrame();
        if (!frame) {

            // Stop capture and report failure if window was closed. If we don't stop,
            // testing shows that either we don't get any frames, or we get blank frames.
            // Emitting an error will prevent this inconsistent behavior, and makes the
            // Windows implementation behave like the Linux implementation
            if (!IsWindow(m_captureWindow))
                throw std::runtime_error("Window was closed");

            // Blank frames may come spuriously if no new window texture
            // is available yet.
            return {};
        }

        if (m_frameSize != frame.ContentSize()) {
            m_frameSize = frame.ContentSize();
            m_framePool.Recreate(getCaptureDevice(m_device), m_pixelFormat, 1, frame.ContentSize());
            return {};
        }

        return copyTexture(m_device, frame.Surface());
    }

private:
    static GraphicsCaptureItem createCaptureItem(HWND hwnd)
    {
        const auto factory = winrt::get_activation_factory<GraphicsCaptureItem>();
        const auto interop = factory.as<IGraphicsCaptureItemInterop>();

        GraphicsCaptureItem item = { nullptr };
        winrt::hresult status = S_OK;

        // Attempt to create capture item with retry, because this occasionally fails,
        // particularly in unit tests. When the failure code is E_INVALIDARG, it
        // seems to help to sleep for a bit and retry. See QTBUG-116025.
        constexpr int maxRetry = 10;
        constexpr std::chrono::milliseconds retryDelay{ 100 };
        for (int retryNum = 0; retryNum < maxRetry; ++retryNum) {

            status = interop->CreateForWindow(hwnd, winrt::guid_of<GraphicsCaptureItem>(),
                                              winrt::put_abi(item));

            if (status != E_INVALIDARG)
                break;

            qCWarning(qLcWindowCaptureUwp)
                    << "Failed to create capture item:"
                    << QString::fromStdWString(winrt::hresult_error(status).message().c_str())
                    << "Retry number" << retryNum;

            if (retryNum + 1 < maxRetry)
                QThread::sleep(retryDelay);
        }

        // Throw if we fail to create the capture item
        check_hresult(status);

        return item;
    }

    static IDirect3DDevice getCaptureDevice(const com_ptr<ID3D11Device> &d3dDevice)
    {
        const auto dxgiDevice = d3dDevice.as<IDXGIDevice>();

        com_ptr<IInspectable> device;
        // Windows 7 backport: resolved at run time, see the helper above.
        check_hresult(createDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), device.put()));

        return device.as<IDirect3DDevice>();
    }

    static com_ptr<IDXGISurface> copyTexture(const com_ptr<ID3D11Device> &device,
                                             const IDirect3DSurface &capturedTexture)
    {
        const auto dxgiInterop{ capturedTexture.as<IDirect3DDxgiInterfaceAccess>() };
        if (!dxgiInterop)
            return {};

        com_ptr<IDXGISurface> dxgiSurface;
        check_hresult(dxgiInterop->GetInterface(guid_of<IDXGISurface>(), dxgiSurface.put_void()));

        DXGI_SURFACE_DESC desc = {};
        check_hresult(dxgiSurface->GetDesc(&desc));

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = desc.Width;
        texDesc.Height = desc.Height;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.Usage = D3D11_USAGE_STAGING;
        texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        texDesc.MiscFlags = 0;
        texDesc.BindFlags = 0;
        texDesc.ArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.SampleDesc = { 1, 0 };

        com_ptr<ID3D11Texture2D> texture;
        check_hresult(device->CreateTexture2D(&texDesc, nullptr, texture.put()));

        com_ptr<ID3D11DeviceContext> ctx;
        device->GetImmediateContext(ctx.put());
        ctx->CopyResource(texture.get(), dxgiSurface.as<ID3D11Resource>().get());

        return texture.as<IDXGISurface>();
    }

    MultithreadedApartment m_comApartment{};
    HWND m_captureWindow{};
    winrt::Windows::Graphics::SizeInt32 m_frameSize{};
    com_ptr<ID3D11Device> m_device;
    Direct3D11CaptureFramePool m_framePool{ nullptr };
    GraphicsCaptureSession m_session{ nullptr };
    const DirectXPixelFormat m_pixelFormat = DirectXPixelFormat::R8G8B8A8UIntNormalized;
};

} // namespace

class QFFmpegWindowCaptureUwp::Grabber : public QFFmpegSurfaceCaptureGrabber
{
public:
    Grabber(QFFmpegWindowCaptureUwp &capture, HWND hwnd)
        : m_hwnd(hwnd),
          m_format(QVideoFrameFormat(asQSize(getWindowSize(hwnd)),
                                     QVideoFrameFormat::Format_RGBX8888))
    {
        const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
        m_adapter = getAdapter(monitor);

        const qreal refreshRate = getMonitorRefreshRateHz(monitor);

        m_format.setStreamFrameRate(refreshRate);
        setFrameRate(refreshRate);

        addFrameCallback(capture, &QFFmpegWindowCaptureUwp::newVideoFrame);
        connect(this, &Grabber::errorUpdated, &capture, &QFFmpegWindowCaptureUwp::updateError);
    }

    ~Grabber() override { stop(); }

    QVideoFrameFormat frameFormat() const { return m_format; }

protected:

    void initializeGrabbingContext() override
    {
        if (!m_adapter || !IsWindow(m_hwnd))
            return; // Error already logged

        try {
            m_windowGrabber = std::make_unique<WindowGrabber>(m_adapter.get(), m_hwnd);

            QFFmpegSurfaceCaptureGrabber::initializeGrabbingContext();
        } catch (const winrt::hresult_error &err) {

            const QString message = QLatin1String("Unable to capture window: ")
                    + QString::fromWCharArray(err.message().c_str());

            updateError(InternalError, message);
        }
    }

    void finalizeGrabbingContext() override
    {
        QFFmpegSurfaceCaptureGrabber::finalizeGrabbingContext();
        m_windowGrabber = nullptr;
    }

    QVideoFrame grabFrame() override
    {
        try {
            com_ptr<IDXGISurface> texture = m_windowGrabber->tryGetFrame();
            if (!texture)
                return {}; // No frame available yet

            const QSize size = getTextureSize(texture);

            m_format.setFrameSize(size);

            return QVideoFramePrivate::createFrame(
                    std::make_unique<QUwpTextureVideoBuffer>(std::move(texture)), m_format);

        } catch (const winrt::hresult_error &err) {

            const QString message = QLatin1String("Window capture failed: ")
                    + QString::fromWCharArray(err.message().c_str());

            updateError(InternalError, message);
        } catch (const std::runtime_error& e) {
            updateError(CaptureFailed, QString::fromLatin1(e.what()));
        }

        return {};
    }

private:
    static com_ptr<IDXGIAdapter1> getAdapter(HMONITOR handle)
    {
        com_ptr<IDXGIFactory1> factory;
        check_hresult(CreateDXGIFactory1(guid_of<IDXGIFactory1>(), factory.put_void()));

        com_ptr<IDXGIAdapter1> adapter;
        for (quint32 i = 0; factory->EnumAdapters1(i, adapter.put()) == S_OK; adapter = nullptr, i++) {
            com_ptr<IDXGIOutput> output;
            for (quint32 j = 0; adapter->EnumOutputs(j, output.put()) == S_OK; output = nullptr, j++) {
                DXGI_OUTPUT_DESC desc = {};
                HRESULT hr = output->GetDesc(&desc);
                if (hr == S_OK && desc.Monitor == handle)
                    return adapter;
            }
        }
        return {};
    }

    static QSize getTextureSize(const com_ptr<IDXGISurface> &surf)
    {
        if (!surf)
            return {};

        DXGI_SURFACE_DESC desc;
        check_hresult(surf->GetDesc(&desc));

        return { static_cast<int>(desc.Width), static_cast<int>(desc.Height) };
    }

    static qreal getMonitorRefreshRateHz(HMONITOR handle)
    {
        DWORD count = 0;
        if (GetNumberOfPhysicalMonitorsFromHMONITOR(handle, &count)) {
            std::vector<PHYSICAL_MONITOR> monitors{ count };
            if (GetPhysicalMonitorsFromHMONITOR(handle, count, monitors.data())) {
                for (const auto &monitor : std::as_const(monitors)) {
                    MC_TIMING_REPORT screenTiming = {};
                    if (GetTimingReport(monitor.hPhysicalMonitor, &screenTiming)) {
                        // Empirically we found that GetTimingReport does not return
                        // the frequency in updates per second as documented, but in
                        // updates per 100 seconds.
                        return static_cast<qreal>(screenTiming.dwVerticalFrequencyInHZ) / 100.0;
                    }
                }
            }
        }
        return DefaultScreenCaptureFrameRate;
    }

    HWND m_hwnd{};
    com_ptr<IDXGIAdapter1> m_adapter{};
    std::unique_ptr<WindowGrabber> m_windowGrabber;
    QVideoFrameFormat m_format;
};

QFFmpegWindowCaptureUwp::QFFmpegWindowCaptureUwp() : QPlatformSurfaceCapture(WindowSource{})
{
    qCDebug(qLcWindowCaptureUwp) << "Creating UWP screen capture";
}

QFFmpegWindowCaptureUwp::~QFFmpegWindowCaptureUwp() = default;

static QString isCapturableWindow(HWND hwnd)
{
    if (!IsWindow(hwnd))
        return u"Invalid window handle"_s;

    if (hwnd == GetShellWindow())
        return u"Cannot capture the shell window"_s;

    wchar_t className[MAX_PATH] = {};
    GetClassName(hwnd, className, MAX_PATH);
    if (QString::fromWCharArray(className).length() == 0)
        return u"Cannot capture windows without a class name"_s;

    if (!IsWindowVisible(hwnd))
        return u"Cannot capture invisible windows"_s;

    if (GetAncestor(hwnd, GA_ROOT) != hwnd)
        return u"Can only capture root windows"_s;

    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    if (style & WS_DISABLED)
        return u"Cannot capture disabled windows"_s;

    const LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW)
        return u"No tooltips"_s;

    DWORD cloaked = FALSE;
    const HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (SUCCEEDED(hr) && cloaked == DWM_CLOAKED_SHELL)
        return u"Cannot capture cloaked windows"_s;

    return {};
}

bool QFFmpegWindowCaptureUwp::setActiveInternal(bool active)
{
    if (static_cast<bool>(m_grabber) == active)
        return false;

    if (m_grabber) {
        m_grabber.reset();
        return true;
    }

    const auto window = source<WindowSource>();
    const auto handle = QCapturableWindowPrivate::handle(window);

    const auto hwnd = reinterpret_cast<HWND>(handle ? handle->id : 0);
    if (const QString error = isCapturableWindow(hwnd); !error.isEmpty()) {
        updateError(InternalError, error);
        return false;
    }

    m_grabber = std::make_unique<Grabber>(*this, hwnd);
    m_grabber->start();

    return true;
}

bool QFFmpegWindowCaptureUwp::isSupported()
{
    return GraphicsCaptureSession::IsSupported();
}

QVideoFrameFormat QFFmpegWindowCaptureUwp::frameFormat() const
{
    if (m_grabber)
        return m_grabber->frameFormat();
    return {};
}

QT_END_NAMESPACE

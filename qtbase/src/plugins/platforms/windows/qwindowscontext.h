// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QWINDOWSCONTEXT_H
#define QWINDOWSCONTEXT_H

#include "qtwindowsglobal.h"
#include <QtCore/qt_windows.h>

#include <QtCore/qscopedpointer.h>
#include <QtCore/qsharedpointer.h>
#include <QtCore/qloggingcategory.h>

#define STRICT_TYPED_ITEMIDS
#include <shlobj.h>
#include <shlwapi.h>
#include <shellscalingapi.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcQpaWindow)
Q_DECLARE_LOGGING_CATEGORY(lcQpaEvents)
Q_DECLARE_LOGGING_CATEGORY(lcQpaGl)
Q_DECLARE_LOGGING_CATEGORY(lcQpaMime)
Q_DECLARE_LOGGING_CATEGORY(lcQpaInputMethods)
Q_DECLARE_LOGGING_CATEGORY(lcQpaDialogs)
Q_DECLARE_LOGGING_CATEGORY(lcQpaMenus)
Q_DECLARE_LOGGING_CATEGORY(lcQpaTablet)
Q_DECLARE_LOGGING_CATEGORY(lcQpaAccessibility)
Q_DECLARE_LOGGING_CATEGORY(lcQpaUiAutomation)
Q_DECLARE_LOGGING_CATEGORY(lcQpaTrayIcon)
Q_DECLARE_LOGGING_CATEGORY(lcQpaScreen)

class QWindow;
class QPlatformScreen;
class QPlatformWindow;
class QPlatformKeyMapper;
class QWindowsMenuBar;
class QWindowsScreenManager;
class QWindowsTabletSupport;
class QWindowsWindow;
class QWindowsMimeRegistry;
struct QWindowCreationContext;
struct QWindowsContextPrivate;
class QPoint;
class QKeyEvent;
class QPointingDevice;

struct QWindowsUser32DLL
{
    inline void init();
    inline bool supportsPointerApi();

    typedef BOOL (WINAPI *EnableMouseInPointer)(BOOL);
    typedef BOOL (WINAPI *GetPointerType)(UINT32, PVOID);
    typedef BOOL (WINAPI *GetPointerInfo)(UINT32, PVOID);
    typedef BOOL (WINAPI *GetPointerDeviceRects)(HANDLE, RECT *, RECT *);
    typedef BOOL (WINAPI *GetPointerTouchInfo)(UINT32, PVOID);
    typedef BOOL (WINAPI *GetPointerFrameTouchInfo)(UINT32, UINT32 *, PVOID);
    typedef BOOL (WINAPI *GetPointerFrameTouchInfoHistory)(UINT32, UINT32 *, UINT32 *, PVOID);
    typedef BOOL (WINAPI *GetPointerPenInfo)(UINT32, PVOID);
    typedef BOOL (WINAPI *GetPointerPenInfoHistory)(UINT32, UINT32 *, PVOID);
    typedef BOOL (WINAPI *SkipPointerFrameMessages)(UINT32);
    typedef BOOL (WINAPI *SetProcessDPIAware)();
    typedef BOOL (WINAPI *SetProcessDpiAwarenessContext)(HANDLE);
    typedef BOOL (WINAPI *AddClipboardFormatListener)(HWND);
    typedef BOOL (WINAPI *RemoveClipboardFormatListener)(HWND);
    typedef BOOL (WINAPI *GetDisplayAutoRotationPreferences)(DWORD *);
    typedef BOOL (WINAPI *SetDisplayAutoRotationPreferences)(DWORD);
    typedef BOOL (WINAPI *AdjustWindowRectExForDpi)(LPRECT,DWORD,BOOL,DWORD,UINT);
    typedef BOOL (WINAPI *EnableNonClientDpiScaling)(HWND);
    typedef DPI_AWARENESS_CONTEXT (WINAPI *GetWindowDpiAwarenessContext)(HWND);
    typedef DPI_AWARENESS (WINAPI *GetAwarenessFromDpiAwarenessContext)(int);
    typedef BOOL (WINAPI *SystemParametersInfoForDpi)(UINT, UINT, PVOID, UINT, UINT);
    typedef int  (WINAPI *GetDpiForWindow)(HWND);
    typedef BOOL (WINAPI *GetSystemMetricsForDpi)(INT, UINT);
    typedef BOOL (WINAPI *AreDpiAwarenessContextsEqual)(DPI_AWARENESS_CONTEXT, DPI_AWARENESS_CONTEXT);
    typedef DPI_AWARENESS_CONTEXT (WINAPI *GetThreadDpiAwarenessContext)();
    typedef BOOL (WINAPI *IsValidDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
    
    // Windows pointer functions (Windows 8 or later).
    EnableMouseInPointer enableMouseInPointer = nullptr;
    GetPointerType getPointerType = nullptr;
    GetPointerInfo getPointerInfo = nullptr;
    GetPointerDeviceRects getPointerDeviceRects = nullptr;
    GetPointerTouchInfo getPointerTouchInfo = nullptr;
    GetPointerFrameTouchInfo getPointerFrameTouchInfo = nullptr;
    GetPointerFrameTouchInfoHistory getPointerFrameTouchInfoHistory = nullptr;
    GetPointerPenInfo getPointerPenInfo = nullptr;
    GetPointerPenInfoHistory getPointerPenInfoHistory = nullptr;
    SkipPointerFrameMessages skipPointerFrameMessages = nullptr;

    // Windows Vista onwards
    SetProcessDPIAware setProcessDPIAware = nullptr;

    // Windows 10 version 1607 onwards
    GetDpiForWindow getDpiForWindow = nullptr;
    GetThreadDpiAwarenessContext getThreadDpiAwarenessContext = nullptr;
    IsValidDpiAwarenessContext isValidDpiAwarenessContext = nullptr;

    // Windows 10 version 1703 onwards
    SetProcessDpiAwarenessContext setProcessDpiAwarenessContext = nullptr;
    AreDpiAwarenessContextsEqual areDpiAwarenessContextsEqual = nullptr;

    // Clipboard listeners are present on Windows Vista onwards
    // but missing in MinGW 4.9 stub libs. Can be removed in MinGW 5.
    AddClipboardFormatListener addClipboardFormatListener = nullptr;
    RemoveClipboardFormatListener removeClipboardFormatListener = nullptr;

    // Rotation API
    GetDisplayAutoRotationPreferences getDisplayAutoRotationPreferences = nullptr;
    SetDisplayAutoRotationPreferences setDisplayAutoRotationPreferences = nullptr;

    AdjustWindowRectExForDpi adjustWindowRectExForDpi = nullptr;
    EnableNonClientDpiScaling enableNonClientDpiScaling = nullptr;
    GetWindowDpiAwarenessContext getWindowDpiAwarenessContext = nullptr;
    GetAwarenessFromDpiAwarenessContext getAwarenessFromDpiAwarenessContext = nullptr;
    SystemParametersInfoForDpi systemParametersInfoForDpi = nullptr;
    GetSystemMetricsForDpi getSystemMetricsForDpi = nullptr;
};

// Shell scaling library (Windows 8.1 onwards)
struct QWindowsShcoreDLL
{
    void init();
    inline bool isValid() const { return getProcessDpiAwareness && setProcessDpiAwareness && getDpiForMonitor; }

    typedef HRESULT (WINAPI *GetProcessDpiAwareness)(HANDLE,PROCESS_DPI_AWARENESS *);
    typedef HRESULT (WINAPI *SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS);
    typedef HRESULT (WINAPI *GetDpiForMonitor)(HMONITOR,int,UINT *,UINT *);

    GetProcessDpiAwareness getProcessDpiAwareness = nullptr;
    SetProcessDpiAwareness setProcessDpiAwareness = nullptr;
    GetDpiForMonitor getDpiForMonitor = nullptr;
};

class QWindowsContext
{
    Q_DISABLE_COPY_MOVE(QWindowsContext)
public:
    using HandleBaseWindowHash = QHash<HWND, QWindowsWindow *>;

    enum SystemInfoFlags
    {
        SI_RTL_Extensions = 0x1,
        SI_SupportsTouch = 0x2
    };

    // Verbose flag set by environment variable QT_QPA_VERBOSE
    static int verbose;

    explicit QWindowsContext();
    ~QWindowsContext();

    bool initTouch();
    bool initTouch(unsigned integrationOptions); // For calls from QWindowsIntegration::QWindowsIntegration() only.
    void registerTouchWindows();
    bool initTablet();
    bool disposeTablet();

    bool initPowerNotificationHandler();

    int defaultDPI() const;

    static QString classNamePrefix();
    QString registerWindowClass(const QWindow *w);
    QString registerWindowClass(QString cname, WNDPROC proc,
                                unsigned style = 0, HBRUSH brush = nullptr,
                                bool icon = false);
    HWND createDummyWindow(const QString &classNameIn,
                           const wchar_t *windowName,
                           WNDPROC wndProc = nullptr, DWORD style = WS_OVERLAPPED);

    HDC displayContext() const;
    int screenDepth() const;

    static QWindowsContext *instance();

    void addWindow(HWND, QWindowsWindow *w);
    void removeWindow(HWND);

    QWindowsWindow *findClosestPlatformWindow(HWND) const;
    QWindowsWindow *findPlatformWindow(HWND) const;
    QWindowsWindow *findPlatformWindow(const QWindowsMenuBar *mb) const;
    QWindow *findWindow(HWND) const;
    QWindowsWindow *findPlatformWindowAt(HWND parent, const QPoint &screenPoint,
                                             unsigned cwex_flags) const;

    static bool shouldHaveNonClientDpiScaling(const QWindow *window);

    QWindow *windowUnderMouse() const;
    void clearWindowUnderMouse();

    inline bool windowsProc(HWND hwnd, UINT message,
                            QtWindows::WindowsEventType et,
                            WPARAM wParam, LPARAM lParam, LRESULT *result,
                            QWindowsWindow **platformWindowPtr);

    QWindow *keyGrabber() const;
    void setKeyGrabber(QWindow *hwnd);

    QSharedPointer<QWindowCreationContext> setWindowCreationContext(const QSharedPointer<QWindowCreationContext> &ctx);
    QSharedPointer<QWindowCreationContext> windowCreationContext() const;

    static void setTabletAbsoluteRange(int a);

    static bool setProcessDpiAwareness(QtWindows::DpiAwareness dpiAwareness);
    static QtWindows::DpiAwareness processDpiAwareness();
    static QtWindows::DpiAwareness windowDpiAwareness(HWND hwnd);

    void setDetectAltGrModifier(bool a);

    // Returns a combination of SystemInfoFlags
    unsigned systemInfo() const;

    bool useRTLExtensions() const;
    QPlatformKeyMapper *keyMapper() const;

    HandleBaseWindowHash &windows();

    static bool isSessionLocked();

    QWindowsMimeRegistry &mimeConverter() const;
    QWindowsScreenManager &screenManager();
    QWindowsTabletSupport *tabletSupport() const;

    static QWindowsUser32DLL user32dll;
    static QWindowsShcoreDLL shcoredll;

    bool asyncExpose() const;
    void setAsyncExpose(bool value);

    static void forceNcCalcSize(HWND hwnd);

    static bool systemParametersInfo(unsigned action, unsigned param, void *out, unsigned dpi = 0);
    static bool systemParametersInfoForScreen(unsigned action, unsigned param, void *out,
                                              const QPlatformScreen *screen = nullptr);
    static bool systemParametersInfoForWindow(unsigned action, unsigned param, void *out,
                                              const QPlatformWindow *win = nullptr);
    static bool nonClientMetrics(NONCLIENTMETRICS *ncm, unsigned dpi = 0);
    static bool nonClientMetricsForScreen(NONCLIENTMETRICS *ncm,
                                          const QPlatformScreen *screen = nullptr);
    static bool nonClientMetricsForWindow(NONCLIENTMETRICS *ncm,
                                          const QPlatformWindow *win = nullptr);

    static DWORD readAdvancedExplorerSettings(const wchar_t *subKey, DWORD defaultValue);

    static bool filterNativeEvent(MSG *msg, LRESULT *result);
    static bool filterNativeEvent(QWindow *window, MSG *msg, LRESULT *result);

private:
    void handleFocusEvent(QtWindows::WindowsEventType et, QWindowsWindow *w);
#ifndef QT_NO_CONTEXTMENU
    bool handleContextMenuEvent(QWindow *window, const MSG &msg);
#endif
    void handleExitSizeMove(QWindow *window);
    void unregisterWindowClasses();

    QScopedPointer<QWindowsContextPrivate> d;
    static QWindowsContext *m_instance;
};

extern "C" LRESULT QT_WIN_CALLBACK qWindowsWndProc(HWND, UINT, WPARAM, LPARAM);

QT_END_NAMESPACE

#endif // QWINDOWSCONTEXT_H

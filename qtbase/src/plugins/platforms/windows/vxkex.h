#pragma once

#include <shtypes.h>
#include <winuser.h>
#include <windef.h>
#include <shellscalingapi.h>

#define MDT_MAXIMUM_DPI 3

namespace vxkex {

// Defined further down; the metric helpers above it need the system DPI as the
// baseline to convert from (see the comment in GetSystemMetricsForDpi).
static UINT GetDpiForSystem(VOID);

// Windows 7 backport: on a DPI-aware process GetSystemMetrics() already returns
// values scaled to the system DPI, so the baseline to convert away from is the
// system DPI, not the fixed 96 baseline. Qt asks these helpers for the 96-DPI
// logical value and then applies its own device pixel ratio on top; dividing by
// 96 here (Dpi == 96, so a no-op) left the metric at system scale, and Qt scaled
// it a second time - scrollbars and window frames came out 1.5x too large at 150%.
// The reference implementation (VxKex) divides by the system DPI throughout.
static INT GetSystemMetricsForDpi(
    IN  INT     Index,
    IN  UINT    Dpi)
{
    INT Value;

    Value = GetSystemMetrics(Index);

    switch (Index) {
    case SM_CXVSCROLL:
    case SM_CYHSCROLL:
    case SM_CYCAPTION:
    case SM_CYVTHUMB:
    case SM_CXHTHUMB:
    case SM_CXICON:
    case SM_CYICON:
    case SM_CXCURSOR:
    case SM_CYCURSOR:
    case SM_CYMENU:
    case SM_CYVSCROLL:
    case SM_CXHSCROLL:
    case SM_CXSIZE:
    case SM_CYSIZE:
    case SM_CXFRAME:
    case SM_CYFRAME:
    case SM_CXICONSPACING:
    case SM_CYICONSPACING:
    case SM_CXSMICON:
    case SM_CYSMICON:
    case SM_CYSMCAPTION:
    case SM_CXSMSIZE:
    case SM_CYSMSIZE:
    case SM_CXMENUSIZE:
    case SM_CYMENUSIZE:
    case SM_CXMENUCHECK:
    case SM_CYMENUCHECK:
    case SM_CXPADDEDBORDER:
        // Pixel values that have to be rescaled. The exact set matches the one
        // Windows 10's own GetSystemMetricsForDpi scales.
        {
            UINT SystemDpi = vxkex::GetDpiForSystem();

            if (SystemDpi == 0)
                SystemDpi = USER_DEFAULT_SCREEN_DPI;

            Value *= Dpi;
            Value /= SystemDpi;
        }
        break;
    }

    return Value;
}

static BOOL SystemParametersInfoForDpi(
    IN      UINT    Action,
    IN      UINT    Parameter,
    IN OUT  PVOID   Data,
    IN      UINT    WinIni,
    IN      UINT    Dpi)
{
    // Windows 7 backport: same reasoning as GetSystemMetricsForDpi - the values
    // SystemParametersInfo() returns on a DPI-aware process are already at system
    // scale, so convert away from the system DPI, not from 96. The fonts have to
    // be rescaled too (Qt reads lfMessageFont from here as its default font); the
    // previous port left them untouched, so at 150% the UI font stayed 1.5x too big.
    //
    // SystemDpi MUST be signed: lfHeight is negative, and dividing a negative LONG
    // by an unsigned int reinterprets it as a huge positive value, so the font
    // height blew up to millions of pixels and every glyph rendered as a box.
    INT SystemDpi = vxkex::GetDpiForSystem();

    if (SystemDpi == 0)
        SystemDpi = USER_DEFAULT_SCREEN_DPI;

    switch (Action) {
    case SPI_GETICONTITLELOGFONT:
        {
            BOOL Success;
            PLOGFONT LogFont;

            Success = SystemParametersInfo(Action, Parameter, Data, 0);

            if (Success) {
                LogFont = (PLOGFONT) Data;

                LogFont->lfWidth  *= Dpi;
                LogFont->lfHeight *= Dpi;
                LogFont->lfWidth  /= SystemDpi;
                LogFont->lfHeight /= SystemDpi;
            }

            return Success;
        }
    case SPI_GETICONMETRICS:
        {
            BOOL Success;
            PICONMETRICS IconMetrics;

            Success = SystemParametersInfo(Action, Parameter, Data, 0);

            if (Success) {
                IconMetrics = (PICONMETRICS) Data;

                IconMetrics->iHorzSpacing *= Dpi;
                IconMetrics->iVertSpacing *= Dpi;
                IconMetrics->iHorzSpacing /= SystemDpi;
                IconMetrics->iVertSpacing /= SystemDpi;
            }

            return Success;
        }
    case SPI_GETNONCLIENTMETRICS:
        {
            BOOL Success;
            PNONCLIENTMETRICS NonClientMetrics;

            Success = SystemParametersInfo(Action, Parameter, Data, 0);

            if (Success) {
                NonClientMetrics = (PNONCLIENTMETRICS) Data;

                NonClientMetrics->iBorderWidth          *= Dpi;
                NonClientMetrics->iScrollWidth          *= Dpi;
                NonClientMetrics->iScrollHeight         *= Dpi;
                NonClientMetrics->iCaptionWidth         *= Dpi;
                NonClientMetrics->iCaptionHeight        *= Dpi;
                NonClientMetrics->iSmCaptionWidth       *= Dpi;
                NonClientMetrics->iSmCaptionHeight      *= Dpi;
                NonClientMetrics->iMenuWidth            *= Dpi;
                NonClientMetrics->iMenuHeight           *= Dpi;
                NonClientMetrics->iPaddedBorderWidth    *= Dpi;

                NonClientMetrics->lfCaptionFont.lfWidth     *= Dpi;
                NonClientMetrics->lfCaptionFont.lfHeight    *= Dpi;
                NonClientMetrics->lfMenuFont.lfWidth        *= Dpi;
                NonClientMetrics->lfMenuFont.lfHeight       *= Dpi;
                NonClientMetrics->lfMessageFont.lfWidth     *= Dpi;
                NonClientMetrics->lfMessageFont.lfHeight    *= Dpi;
                NonClientMetrics->lfSmCaptionFont.lfWidth   *= Dpi;
                NonClientMetrics->lfSmCaptionFont.lfHeight  *= Dpi;
                NonClientMetrics->lfStatusFont.lfWidth      *= Dpi;
                NonClientMetrics->lfStatusFont.lfHeight     *= Dpi;

                NonClientMetrics->iBorderWidth          /= SystemDpi;
                NonClientMetrics->iScrollWidth          /= SystemDpi;
                NonClientMetrics->iScrollHeight         /= SystemDpi;
                NonClientMetrics->iCaptionWidth         /= SystemDpi;
                NonClientMetrics->iCaptionHeight        /= SystemDpi;
                NonClientMetrics->iSmCaptionWidth       /= SystemDpi;
                NonClientMetrics->iSmCaptionHeight      /= SystemDpi;
                NonClientMetrics->iMenuWidth            /= SystemDpi;
                NonClientMetrics->iMenuHeight           /= SystemDpi;
                NonClientMetrics->iPaddedBorderWidth    /= SystemDpi;

                NonClientMetrics->lfCaptionFont.lfWidth     /= SystemDpi;
                NonClientMetrics->lfCaptionFont.lfHeight    /= SystemDpi;
                NonClientMetrics->lfMenuFont.lfWidth        /= SystemDpi;
                NonClientMetrics->lfMenuFont.lfHeight       /= SystemDpi;
                NonClientMetrics->lfMessageFont.lfWidth     /= SystemDpi;
                NonClientMetrics->lfMessageFont.lfHeight    /= SystemDpi;
                NonClientMetrics->lfSmCaptionFont.lfWidth   /= SystemDpi;
                NonClientMetrics->lfSmCaptionFont.lfHeight  /= SystemDpi;
                NonClientMetrics->lfStatusFont.lfWidth      /= SystemDpi;
                NonClientMetrics->lfStatusFont.lfHeight     /= SystemDpi;
            }

            return Success;
        }
    default:
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
}

static HRESULT GetScaleFactorForMonitor(
    IN  HMONITOR                Monitor,
    OUT DEVICE_SCALE_FACTOR    *ScaleFactor)
{
    HDC DeviceContext;
    ULONG LogPixelsX;

    DeviceContext = GetDC(NULL);
    if (!DeviceContext) {
        *ScaleFactor = SCALE_100_PERCENT;
        return S_OK;
    }

    LogPixelsX = GetDeviceCaps(DeviceContext, LOGPIXELSX);
    ReleaseDC(NULL, DeviceContext);

    // DEVICE_SCALE_FACTOR is a percentage - SCALE_150_PERCENT is 150 - so the
    // DPI has to be divided by the baseline, not the other way round. The
    // reciprocal produced 66 for a 150% display, which is not even a value of
    // the enumeration.
    *ScaleFactor = (DEVICE_SCALE_FACTOR) (LogPixelsX * SCALE_100_PERCENT / USER_DEFAULT_SCREEN_DPI);
    return S_OK;
}

static HRESULT GetDpiForMonitor(
    IN  HMONITOR            Monitor,
    IN  MONITOR_DPI_TYPE    DpiType,
    OUT UINT *              DpiX,
    OUT UINT *              DpiY)
{
    HDC DeviceContext;

    if (DpiType >= MDT_MAXIMUM_DPI) {
        return E_INVALIDARG;
    }

    if (!DpiX || !DpiY) {
        return E_INVALIDARG;
    }

    if (!IsProcessDPIAware()) {
        *DpiX = USER_DEFAULT_SCREEN_DPI;
        *DpiY = USER_DEFAULT_SCREEN_DPI;
        return S_OK;
    }

    DeviceContext = GetDC(NULL);
    if (!DeviceContext) {
        *DpiX = USER_DEFAULT_SCREEN_DPI;
        *DpiY = USER_DEFAULT_SCREEN_DPI;
        return S_OK;
    }

    *DpiX = GetDeviceCaps(DeviceContext, LOGPIXELSX);
    *DpiY = GetDeviceCaps(DeviceContext, LOGPIXELSY);

    // Nothing to fold in for MDT_EFFECTIVE_DPI: Windows 7 has one system-wide
    // DPI setting and no per-monitor scaling, so what GetDeviceCaps reports to a
    // DPI-aware process already is the effective DPI - 144 on a 150% display.
    //
    // Scaling it a second time is what pinned the result to 96 at every setting
    // other than 100%, leaving Qt at devicePixelRatio 1: icons and widget
    // metrics stayed unscaled, while text did not, since the system font comes
    // from SPI_GETNONCLIENTMETRICS already expressed in real pixels.

    ReleaseDC(NULL, DeviceContext);
    return S_OK;
}

static UINT GetDpiForSystem(
    VOID)
{
    HDC DeviceContext;
    ULONG LogPixelsX;

    if (!IsProcessDPIAware()) {
        return 96;
    }

    DeviceContext = GetDC(NULL);
    if (!DeviceContext) {
        return 96;
    }

    LogPixelsX = GetDeviceCaps(DeviceContext, LOGPIXELSX);
    ReleaseDC(NULL, DeviceContext);

    return LogPixelsX;
}

static UINT GetDpiForWindow(
    IN  HWND    Window)
{
    if (!IsWindow(Window)) {
        return 0;
    }

    return vxkex::GetDpiForSystem();
}

static BOOL AdjustWindowRectExForDpi(
    IN OUT  LPRECT  Rect,
    IN      ULONG   WindowStyle,
    IN      BOOL    HasMenu,
    IN      ULONG   WindowExStyle,
    IN      ULONG   Dpi)
{
    // Windows 7 backport: rescale the rectangle from the system DPI to the
    // requested one, then let the plain AdjustWindowRectEx add the (system-DPI)
    // frame. Same baseline reasoning as the metric helpers above. Everything here
    // is kept signed - the rectangle edges go negative once the frame is added,
    // and unsigned division would corrupt them the same way it did the font.
    INT SystemDpi;

    if (!Rect) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    SystemDpi = vxkex::GetDpiForSystem();

    if (SystemDpi == 0)
        SystemDpi = USER_DEFAULT_SCREEN_DPI;

    Rect->left   = Rect->left   * (INT) Dpi / SystemDpi;
    Rect->top    = Rect->top    * (INT) Dpi / SystemDpi;
    Rect->right  = Rect->right  * (INT) Dpi / SystemDpi;
    Rect->bottom = Rect->bottom * (INT) Dpi / SystemDpi;

    return AdjustWindowRectEx(
        Rect,
        WindowStyle,
        HasMenu,
        WindowExStyle);
}

static BOOL SetDisplayAutoRotationPreferences(
    IN  ORIENTATION_PREFERENCE  Orientation)
{
    return TRUE;
}

static BOOL GetDisplayAutoRotationPreferences(
    OUT ORIENTATION_PREFERENCE * Orientation)
{
    *Orientation = ORIENTATION_PREFERENCE_NONE;
    return TRUE;
}

// scaling.c

static BOOL SetProcessDpiAwarenessContext(
    IN  DPI_AWARENESS_CONTEXT   DpiContext)
{
    if ((ULONG_PTR)DpiContext == (ULONG_PTR)DPI_AWARENESS_CONTEXT_UNAWARE) {
        //NOTHING;
    } else if ((ULONG_PTR)DpiContext == (ULONG_PTR)DPI_AWARENESS_CONTEXT_SYSTEM_AWARE ||
             (ULONG_PTR)DpiContext == (ULONG_PTR)DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE ||
             (ULONG_PTR)DpiContext == (ULONG_PTR)DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) {
        SetProcessDPIAware();
    } else {
        return FALSE;
    }

    return TRUE;
}

static BOOL AreDpiAwarenessContextsEqual(
    IN  DPI_AWARENESS_CONTEXT   Value1,
    IN  DPI_AWARENESS_CONTEXT   Value2)
{
    return (Value1 == Value2);
}

static BOOL IsValidDpiAwarenessContext(
    IN  DPI_AWARENESS_CONTEXT   Value)
{
    if ((ULONG_PTR)Value == (ULONG_PTR)DPI_AWARENESS_CONTEXT_UNAWARE ||
        (ULONG_PTR)Value == (ULONG_PTR)DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED ||
        (ULONG_PTR)Value == (ULONG_PTR)DPI_AWARENESS_CONTEXT_SYSTEM_AWARE ||
        (ULONG_PTR)Value == (ULONG_PTR)DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE ||
        (ULONG_PTR)Value == (ULONG_PTR)DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) {
        return TRUE;
    } else {
        return FALSE;
    }
}

static BOOL EnableNonClientDpiScaling(
    IN  HWND    Window)
{
    return TRUE;
}

static DPI_AWARENESS_CONTEXT GetThreadDpiAwarenessContext(
    VOID)
{
    if (IsProcessDPIAware()) {
        return DPI_AWARENESS_CONTEXT_SYSTEM_AWARE;
    } else {
        return DPI_AWARENESS_CONTEXT_UNAWARE;
    }
}

static DPI_AWARENESS_CONTEXT GetWindowDpiAwarenessContext(
    IN  HWND    Window)
{
    ULONG WindowThreadId;
    ULONG WindowProcessId;

    WindowThreadId = GetWindowThreadProcessId(Window, &WindowProcessId);
    if (!WindowThreadId) {
        return 0;
    }

    // looks like there's a bug in vxkex, here should be == instead of =
    // and if is always true
    // anyway I don't want to deal with Windows kernel mode structures here

    if (1) { //if (WindowProcessId = (ULONG) NtCurrentTeb()->ClientId.UniqueProcess) {
        return vxkex::GetThreadDpiAwarenessContext();
    }

    return DPI_AWARENESS_CONTEXT_UNAWARE;
}

// pointer.c

static BOOL GetPointerType(
    IN  UINT32               PointerId,
    OUT POINTER_INPUT_TYPE  *PointerType)
{
    *PointerType = PT_MOUSE;
    return TRUE;
}

static BOOL GetPointerFrameTouchInfo(
    IN      UINT32   PointerId,
    IN OUT  UINT32   *PointerCount,
    OUT     LPVOID  TouchInfo)
{
    return FALSE;
}

static BOOL GetPointerFrameTouchInfoHistory(
    IN      UINT32   PointerId,
    IN OUT  UINT32   *EntriesCount,
    IN OUT  UINT32   *PointerCount,
    OUT     LPVOID   TouchInfo)
{
    return FALSE;
}

static BOOL GetPointerPenInfo(
    IN  UINT32   PointerId,
    OUT LPVOID  PenInfo)
{
    return FALSE;
}

static BOOL GetPointerPenInfoHistory(
    IN      UINT32  PointerId,
    IN OUT  UINT32  *EntriesCount,
    OUT     LPVOID  PenInfo)
{
    return FALSE;
}

static BOOL SkipPointerFrameMessages(
    IN  UINT32   PointerId)
{
    return TRUE;
}

static BOOL GetPointerDeviceRects(
    IN  HANDLE  Device,
    OUT LPRECT  PointerDeviceRect,
    OUT LPRECT  DisplayRect)
{
    PointerDeviceRect->top = 0;
    PointerDeviceRect->left = 0;
    PointerDeviceRect->bottom = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    PointerDeviceRect->right = GetSystemMetrics(SM_CXVIRTUALSCREEN);

    DisplayRect->top = 0;
    DisplayRect->left = 0;
    DisplayRect->bottom = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    DisplayRect->right = GetSystemMetrics(SM_CXVIRTUALSCREEN);

    return TRUE;
}

static BOOL GetPointerInfo(
    IN  DWORD           PointerId,
    OUT POINTER_INFO    *PointerInfo)
{
    PointerInfo->pointerType = PT_MOUSE;
    PointerInfo->pointerId = PointerId;
    PointerInfo->frameId = 0;
    PointerInfo->pointerFlags = POINTER_FLAG_NONE;
    PointerInfo->sourceDevice = NULL;
    PointerInfo->hwndTarget = NULL;
    GetCursorPos(&PointerInfo->ptPixelLocation);
    GetCursorPos(&PointerInfo->ptHimetricLocation);
    GetCursorPos(&PointerInfo->ptPixelLocationRaw);
    GetCursorPos(&PointerInfo->ptHimetricLocationRaw);
    PointerInfo->dwTime = 0;
    PointerInfo->historyCount = 1;
    PointerInfo->InputData = 0;
    PointerInfo->dwKeyStates = 0;
    PointerInfo->PerformanceCount = 0;
    PointerInfo->ButtonChangeType = POINTER_CHANGE_NONE;

    return TRUE;
}

} // namespace vxkex

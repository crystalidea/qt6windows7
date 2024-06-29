#pragma once

#include <shtypes.h>
#include <winuser.h>
#include <windef.h>
#include <shellscalingapi.h>

#define MDT_MAXIMUM_DPI 3

namespace vxkex {

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
    case SM_CXMIN:
    case SM_CXMINTRACK:
    case SM_CYMIN:
    case SM_CYMINTRACK:
    case SM_CXSIZE:
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
        // These are pixel values that have to be scaled according to DPI.
        Value *= Dpi;
        Value /= USER_DEFAULT_SCREEN_DPI;
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
    switch (Action) {
    case SPI_GETICONTITLELOGFONT:
        return SystemParametersInfo(Action, Parameter, Data, 0);
    case SPI_GETICONMETRICS:
        {
            BOOL Success;
            PICONMETRICS IconMetrics;

            Success = SystemParametersInfo(Action, Parameter, Data, 0);

            if (Success) {
                IconMetrics = (PICONMETRICS) Data;

                IconMetrics->iHorzSpacing *= Dpi;
                IconMetrics->iVertSpacing *= Dpi;
                IconMetrics->iHorzSpacing /= USER_DEFAULT_SCREEN_DPI;
                IconMetrics->iVertSpacing /= USER_DEFAULT_SCREEN_DPI;
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

                NonClientMetrics->iBorderWidth          /= USER_DEFAULT_SCREEN_DPI;
                NonClientMetrics->iScrollWidth          /= USER_DEFAULT_SCREEN_DPI;
                NonClientMetrics->iScrollHeight         /= USER_DEFAULT_SCREEN_DPI;
                NonClientMetrics->iCaptionWidth         /= USER_DEFAULT_SCREEN_DPI;
                NonClientMetrics->iCaptionHeight        /= USER_DEFAULT_SCREEN_DPI;
                NonClientMetrics->iSmCaptionWidth       /= USER_DEFAULT_SCREEN_DPI;
                NonClientMetrics->iSmCaptionHeight      /= USER_DEFAULT_SCREEN_DPI;
                NonClientMetrics->iMenuWidth            /= USER_DEFAULT_SCREEN_DPI;
                NonClientMetrics->iMenuHeight           /= USER_DEFAULT_SCREEN_DPI;
                NonClientMetrics->iPaddedBorderWidth    /= USER_DEFAULT_SCREEN_DPI;
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

    *ScaleFactor = (DEVICE_SCALE_FACTOR) (9600 / LogPixelsX);
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

    if (DpiType == MDT_EFFECTIVE_DPI) {
        DEVICE_SCALE_FACTOR ScaleFactor;

        // We have to multiply the DPI values by the scaling factor.
        vxkex::GetScaleFactorForMonitor(Monitor, &ScaleFactor);

        *DpiX *= ScaleFactor;
        *DpiY *= ScaleFactor;
        *DpiX /= 100;
        *DpiY /= 100;
    }

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
    // I'm not sure how to implement this function properly.
    // If it turns out to be important, I'll have to do some testing
    // on a Win10 VM.

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
    switch ((ULONG_PTR)DpiContext) {
    case (ULONG_PTR)DPI_AWARENESS_CONTEXT_UNAWARE:
        //NOTHING;
        break;
    case (ULONG_PTR)DPI_AWARENESS_CONTEXT_SYSTEM_AWARE:
    case (ULONG_PTR)DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE:
    case (ULONG_PTR)DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2:
        SetProcessDPIAware();
        break;
    default:
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
    switch ((ULONG_PTR)Value) {
    case (ULONG_PTR)DPI_AWARENESS_CONTEXT_UNAWARE:
    case (ULONG_PTR)DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED:
    case (ULONG_PTR)DPI_AWARENESS_CONTEXT_SYSTEM_AWARE:
    case (ULONG_PTR)DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE:
    case (ULONG_PTR)DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2:
        return TRUE;
    default:
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

// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QLEGACYSHIMS_P_H
#define QLEGACYSHIMS_P_H

#include <shtypes.h>
#include <winuser.h>
#include <windef.h>
#include <shellscalingapi.h>
#include <winternl.h>

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#define MDT_MAXIMUM_DPI 3

namespace QLegacyShims {
	static INT WINAPI GetSystemMetricsForDpi(
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
	
	static BOOL WINAPI SystemParametersInfoForDpi(
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

	static HRESULT WINAPI GetScaleFactorForMonitor(
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

	static HRESULT WINAPI GetDpiForMonitor(
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
			QLegacyShims::GetScaleFactorForMonitor(Monitor, &ScaleFactor);

			*DpiX *= ScaleFactor;
			*DpiY *= ScaleFactor;
			*DpiX /= 100;
			*DpiY /= 100;
		}

		ReleaseDC(NULL, DeviceContext);
		return S_OK;
	}

	static UINT WINAPI GetDpiForSystem(
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

	static UINT WINAPI GetDpiForWindow(
		IN  HWND    Window)
	{
		if (!IsWindow(Window)) {
			return 0;
		}

		return QLegacyShims::GetDpiForSystem();
	}

	static BOOL WINAPI AdjustWindowRectExForDpi(
		IN OUT  LPRECT  Rect,
		IN      ULONG   WindowStyle,
		IN      BOOL    HasMenu,
		IN      ULONG   WindowExStyle,
		IN      ULONG   Dpi)
	{
		return AdjustWindowRectEx(
			Rect,
			WindowStyle,
			HasMenu,
			WindowExStyle);
	}

	static BOOL WINAPI SetProcessDpiAwarenessContext(
		IN  DPI_AWARENESS_CONTEXT   DpiContext)
	{
		if ((ULONG_PTR)DpiContext == (ULONG_PTR)DPI_AWARENESS_CONTEXT_UNAWARE) {
		} else if ((ULONG_PTR)DpiContext == (ULONG_PTR)DPI_AWARENESS_CONTEXT_SYSTEM_AWARE ||
				 (ULONG_PTR)DpiContext == (ULONG_PTR)DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE ||
				 (ULONG_PTR)DpiContext == (ULONG_PTR)DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) {
			SetProcessDPIAware();
		} else {
			return FALSE;
		}

		return TRUE;
	}

	static BOOL WINAPI AreDpiAwarenessContextsEqual(
		IN  DPI_AWARENESS_CONTEXT   Value1,
		IN  DPI_AWARENESS_CONTEXT   Value2)
	{
		return (Value1 == Value2);
	}

	static BOOL WINAPI IsValidDpiAwarenessContext(
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

	static BOOL WINAPI EnableNonClientDpiScaling(
		IN  HWND    Window)
	{
		return TRUE;
	}

	static DPI_AWARENESS_CONTEXT WINAPI GetThreadDpiAwarenessContext(
		VOID)
	{
		if (IsProcessDPIAware()) {
			return DPI_AWARENESS_CONTEXT_SYSTEM_AWARE;
		} else {
			return DPI_AWARENESS_CONTEXT_UNAWARE;
		}
	}

	static DPI_AWARENESS_CONTEXT WINAPI GetWindowDpiAwarenessContext(
		IN  HWND    Window)
	{
		ULONG WindowThreadId;
		ULONG WindowProcessId;

		WindowThreadId = GetWindowThreadProcessId(Window, &WindowProcessId);
		if (!WindowThreadId) {
			return 0;
		}

		if (WindowProcessId == GetCurrentProcessId()) {
			return QLegacyShims::GetThreadDpiAwarenessContext();
		}

		return DPI_AWARENESS_CONTEXT_UNAWARE;
	}
	
	static DPI_AWARENESS WINAPI GetAwarenessFromDpiAwarenessContext(
		IN  DPI_AWARENESS_CONTEXT  Value)
	{
		if (QLegacyShims::AreDpiAwarenessContextsEqual(Value, DPI_AWARENESS_CONTEXT_UNAWARE))
			return DPI_AWARENESS_UNAWARE;
		if (QLegacyShims::AreDpiAwarenessContextsEqual(Value, DPI_AWARENESS_CONTEXT_SYSTEM_AWARE))
			return DPI_AWARENESS_SYSTEM_AWARE;
		if (QLegacyShims::AreDpiAwarenessContextsEqual(Value, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE))
			return DPI_AWARENESS_PER_MONITOR_AWARE;
		if (QLegacyShims::AreDpiAwarenessContextsEqual(Value, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
			return DPI_AWARENESS_PER_MONITOR_AWARE;
		if (QLegacyShims::AreDpiAwarenessContextsEqual(Value, DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED))
			return DPI_AWARENESS_UNAWARE;

		return DPI_AWARENESS_UNAWARE;
	}
	
	static HRESULT WINAPI SetProcessDpiAwareness(
		IN  PROCESS_DPI_AWARENESS  value)
	{
		if (value == PROCESS_SYSTEM_DPI_AWARE || value == PROCESS_PER_MONITOR_DPI_AWARE) {
			if (SetProcessDPIAware())
				return S_OK;
			else
				return E_FAIL;
		}
		if (value == PROCESS_DPI_UNAWARE)
			return S_OK;

		return E_INVALIDARG;
	}

	static HRESULT WINAPI GetProcessDpiAwareness(
		IN  HANDLE hProcess,
		OUT PROCESS_DPI_AWARENESS *value)
	{
		UNREFERENCED_PARAMETER(hProcess);
		
		if (!value)
			return E_INVALIDARG;

		if (IsProcessDPIAware()) {
			*value = PROCESS_SYSTEM_DPI_AWARE;
		} else {
			*value = PROCESS_DPI_UNAWARE;
		}

		return S_OK;
	}
	
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

	static BOOL WINAPI EnableMouseInPointer(
		IN  BOOL    fEnable)
	{
		UNREFERENCED_PARAMETER(fEnable);
		return TRUE;
	}
	
	static BOOL WINAPI GetPointerTouchInfo(
		IN  UINT32              pointerId,
		OUT POINTER_TOUCH_INFO *touchInfo)
	{
		UNREFERENCED_PARAMETER(pointerId);
		UNREFERENCED_PARAMETER(touchInfo);
		return FALSE;
	}
}

#endif // QLEGACYSHIMS_P_H
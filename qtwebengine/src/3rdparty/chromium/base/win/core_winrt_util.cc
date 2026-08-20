// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/core_winrt_util.h"

#include <windows.h>

#include "base/threading/scoped_thread_priority.h"

namespace {

// Windows 7 backport: these entry points are exported by combase.dll, which is
// not present on Windows 7. Calling them through the api-ms-win-core-winrt-*
// import libraries puts them in the load-time import table and keeps the whole
// library from loading there. Resolve them at run time instead, exactly as
// Chromium did up to 109.0.5414.120, so that on Windows 8 and later the same
// functions are used as before and on Windows 7 the WinRT-backed features
// report themselves unavailable through their existing error paths.
FARPROC LoadComBaseFunctionForCoreWinRTUtil(const char* function_name) {
  static HMODULE const handle = []() {
    // Mitigate the issues caused by loading DLLs on a background thread
    // (http://crbug/973868).
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    return ::LoadLibraryEx(L"combase.dll", nullptr,
                           LOAD_LIBRARY_SEARCH_SYSTEM32);
  }();
  return handle ? ::GetProcAddress(handle, function_name) : nullptr;
}

decltype(&::RoActivateInstance) GetRoActivateInstanceFunction() {
  static decltype(&::RoActivateInstance) const function =
      reinterpret_cast<decltype(&::RoActivateInstance)>(
          LoadComBaseFunctionForCoreWinRTUtil("RoActivateInstance"));
  return function;
}

decltype(&::RoGetActivationFactory) GetRoGetActivationFactoryFunction() {
  static decltype(&::RoGetActivationFactory) const function =
      reinterpret_cast<decltype(&::RoGetActivationFactory)>(
          LoadComBaseFunctionForCoreWinRTUtil("RoGetActivationFactory"));
  return function;
}

}  // namespace

namespace base::win {

HRESULT RoGetActivationFactory(HSTRING class_id,
                               const IID& iid,
                               void** out_factory) {
  auto get_factory_func = GetRoGetActivationFactoryFunction();
  if (!get_factory_func)
    return E_FAIL;
  return get_factory_func(class_id, iid, out_factory);
}

HRESULT RoActivateInstance(HSTRING class_id, IInspectable** instance) {
  auto activate_instance_func = GetRoActivateInstanceFunction();
  if (!activate_instance_func)
    return E_FAIL;
  return activate_instance_func(class_id, instance);
}

}  // namespace base::win

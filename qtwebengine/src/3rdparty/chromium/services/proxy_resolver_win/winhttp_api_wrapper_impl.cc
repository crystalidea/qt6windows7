// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver_win/winhttp_api_wrapper_impl.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/check_op.h"

namespace proxy_resolver_win {

// TODO(https://crbug.com/1032820): Capture telemetry for WinHttp APIs if
// interesting.

namespace {

// Windows 7 backport: the asynchronous proxy resolution APIs below were
// introduced in Windows 8, and importing them keeps the whole library from
// loading on Windows 7. Resolve them at run time instead. Nothing reaches these
// wrappers there anyway: WindowsSystemProxyResolutionService::IsSupported()
// requires Windows 10 1607 or later, and proxy resolution falls back to
// WinHttpGetIEProxyConfigForCurrentUser() plus PAC evaluation, both of which
// work on Windows 7.
FARPROC LoadWinHttpFunction(const char* function_name) {
  static HMODULE const handle =
      ::LoadLibraryExW(L"winhttp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  return handle ? ::GetProcAddress(handle, function_name) : nullptr;
}

DWORD WinHttpCreateProxyResolverDynamic(HINTERNET session,
                                        HINTERNET* out_resolver_handle) {
  using WinHttpCreateProxyResolverFn = DWORD(WINAPI*)(HINTERNET, HINTERNET*);
  static const auto create_proxy_resolver_fn =
      reinterpret_cast<WinHttpCreateProxyResolverFn>(
          LoadWinHttpFunction("WinHttpCreateProxyResolver"));
  if (!create_proxy_resolver_fn)
    return ERROR_NOT_SUPPORTED;
  return create_proxy_resolver_fn(session, out_resolver_handle);
}

DWORD WinHttpGetProxyForUrlExDynamic(HINTERNET resolver_handle,
                                     PCWSTR url,
                                     WINHTTP_AUTOPROXY_OPTIONS* autoproxy_options,
                                     DWORD_PTR context) {
  using WinHttpGetProxyForUrlExFn =
      DWORD(WINAPI*)(HINTERNET, PCWSTR, WINHTTP_AUTOPROXY_OPTIONS*, DWORD_PTR);
  static const auto get_proxy_for_url_ex_fn =
      reinterpret_cast<WinHttpGetProxyForUrlExFn>(
          LoadWinHttpFunction("WinHttpGetProxyForUrlEx"));
  if (!get_proxy_for_url_ex_fn)
    return ERROR_NOT_SUPPORTED;
  return get_proxy_for_url_ex_fn(resolver_handle, url, autoproxy_options,
                                 context);
}

DWORD WinHttpGetProxyResultDynamic(HINTERNET resolver_handle,
                                   WINHTTP_PROXY_RESULT* proxy_result) {
  using WinHttpGetProxyResultFn = DWORD(WINAPI*)(HINTERNET,
                                                 WINHTTP_PROXY_RESULT*);
  static const auto get_proxy_result_fn =
      reinterpret_cast<WinHttpGetProxyResultFn>(
          LoadWinHttpFunction("WinHttpGetProxyResult"));
  if (!get_proxy_result_fn)
    return ERROR_NOT_SUPPORTED;
  return get_proxy_result_fn(resolver_handle, proxy_result);
}

void WinHttpFreeProxyResultDynamic(WINHTTP_PROXY_RESULT* proxy_result) {
  using WinHttpFreeProxyResultFn = VOID(WINAPI*)(WINHTTP_PROXY_RESULT*);
  static const auto free_proxy_result_fn =
      reinterpret_cast<WinHttpFreeProxyResultFn>(
          LoadWinHttpFunction("WinHttpFreeProxyResult"));
  if (free_proxy_result_fn)
    free_proxy_result_fn(proxy_result);
}

}  // namespace

ScopedIEConfig::ScopedIEConfig() = default;
ScopedIEConfig::~ScopedIEConfig() {
  if (ie_config.lpszAutoConfigUrl)
    GlobalFree(ie_config.lpszAutoConfigUrl);
  if (ie_config.lpszProxy)
    GlobalFree(ie_config.lpszProxy);
  if (ie_config.lpszProxyBypass)
    GlobalFree(ie_config.lpszProxyBypass);
}

WinHttpAPIWrapperImpl::WinHttpAPIWrapperImpl() = default;
WinHttpAPIWrapperImpl::~WinHttpAPIWrapperImpl() {
  if (session_handle_)
    std::ignore = CallWinHttpSetStatusCallback(nullptr);
  CloseSessionHandle();
}

bool WinHttpAPIWrapperImpl::CallWinHttpOpen() {
  DCHECK_EQ(nullptr, session_handle_);
  session_handle_ = ::WinHttpOpen(nullptr, WINHTTP_ACCESS_TYPE_NO_PROXY,
                                  WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
  return (session_handle_ != nullptr);
}

bool WinHttpAPIWrapperImpl::CallWinHttpSetTimeouts(int resolve_timeout,
                                                   int connect_timeout,
                                                   int send_timeout,
                                                   int receive_timeout) {
  DCHECK_NE(nullptr, session_handle_);
  return (!!::WinHttpSetTimeouts(session_handle_, resolve_timeout,
                                 connect_timeout, send_timeout,
                                 receive_timeout));
}

bool WinHttpAPIWrapperImpl::CallWinHttpSetStatusCallback(
    WINHTTP_STATUS_CALLBACK internet_callback) {
  DCHECK_NE(nullptr, session_handle_);
  const WINHTTP_STATUS_CALLBACK winhttp_status_callback =
      ::WinHttpSetStatusCallback(
          session_handle_, internet_callback,
          WINHTTP_CALLBACK_FLAG_REQUEST_ERROR |
              WINHTTP_CALLBACK_FLAG_GETPROXYFORURL_COMPLETE,
          0);
  return (winhttp_status_callback != WINHTTP_INVALID_STATUS_CALLBACK);
}

bool WinHttpAPIWrapperImpl::CallWinHttpGetIEProxyConfigForCurrentUser(
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* ie_proxy_config) {
  return !!::WinHttpGetIEProxyConfigForCurrentUser(ie_proxy_config);
}

bool WinHttpAPIWrapperImpl::CallWinHttpCreateProxyResolver(
    HINTERNET* out_resolver_handle) {
  DCHECK_NE(nullptr, session_handle_);
  const DWORD result =
      WinHttpCreateProxyResolverDynamic(session_handle_, out_resolver_handle);
  return (result == ERROR_SUCCESS);
}

bool WinHttpAPIWrapperImpl::CallWinHttpGetProxyForUrlEx(
    HINTERNET resolver_handle,
    const std::string& url,
    WINHTTP_AUTOPROXY_OPTIONS* autoproxy_options,
    DWORD_PTR context) {
  const std::wstring wide_url(url.begin(), url.end());
  // TODO(https://crbug.com/1032820): Upgrade to WinHttpGetProxyForUrlEx2()
  // if there is a clear reason to do so.
  const DWORD result = WinHttpGetProxyForUrlExDynamic(
      resolver_handle, wide_url.data(), autoproxy_options, context);
  return (result == ERROR_IO_PENDING);
}

bool WinHttpAPIWrapperImpl::CallWinHttpGetProxyResult(
    HINTERNET resolver_handle,
    WINHTTP_PROXY_RESULT* proxy_result) {
  const DWORD result =
      WinHttpGetProxyResultDynamic(resolver_handle, proxy_result);
  return (result == ERROR_SUCCESS);
}

VOID WinHttpAPIWrapperImpl::CallWinHttpFreeProxyResult(
    WINHTTP_PROXY_RESULT* proxy_result) {
  WinHttpFreeProxyResultDynamic(proxy_result);
}

void WinHttpAPIWrapperImpl::CallWinHttpCloseHandle(HINTERNET internet_handle) {
  ::WinHttpCloseHandle(internet_handle);
}

void WinHttpAPIWrapperImpl::CloseSessionHandle() {
  if (session_handle_) {
    CallWinHttpCloseHandle(session_handle_);
    session_handle_ = nullptr;
  }
}

}  // namespace proxy_resolver_win

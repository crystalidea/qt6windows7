// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_ETW_ETW_PROVIDER_WIN_H_
#define V8_LIBPLATFORM_ETW_ETW_PROVIDER_WIN_H_

// This file defines all the ETW Provider functions.
#include <windows.h>
#ifndef VOID
#define VOID void
#endif
// Windows 7 backport: TraceLoggingRegister() calls TraceLoggingSetInformation()
// to attach provider traits, and because Chromium targets Windows 10 the SDK
// header resolves that to a direct call to EventSetInformation(), which only
// exists from Windows 8 on. That static import alone keeps Qt6WebEngineCore.dll
// from loading on Windows 7. Mode 2 is the SDK's own documented behaviour for
// pre-Windows 8 targets: look "EventSetInformation" up through
// GetModuleHandleExW/GetProcAddress and return an error when it is absent.
// TraceLogging is explicitly documented to work correctly without it, so ETW
// tracing keeps registering and writing events on every Windows version, and
// nothing changes on Windows 8 and later beyond one indirection at startup.
// This is the only place in the tree that includes the header.
#ifndef TLG_HAVE_EVENT_SET_INFORMATION
#define TLG_HAVE_EVENT_SET_INFORMATION 2
#endif
#include <TraceLoggingProvider.h>
#include <evntprov.h>
#include <evntrace.h>  // defines TRACE_LEVEL_* and EVENT_TRACE_TYPE_*

#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
#endif

#ifndef V8_ETW_GUID
#define V8_ETW_GUID \
  0x57277741, 0x3638, 0x4A4B, 0xBD, 0xBA, 0x0A, 0xC6, 0xE4, 0x5D, 0xA5, 0x6C
#endif  // V8_ETW_GUID

#define V8_DECLARE_TRACELOGGING_PROVIDER(v8Provider) \
  TRACELOGGING_DECLARE_PROVIDER(v8Provider);

#define V8_DEFINE_TRACELOGGING_PROVIDER(v8Provider) \
  TRACELOGGING_DEFINE_PROVIDER(v8Provider, "V8.js", (V8_ETW_GUID));

#endif  // V8_LIBPLATFORM_ETW_ETW_PROVIDER_WIN_H_

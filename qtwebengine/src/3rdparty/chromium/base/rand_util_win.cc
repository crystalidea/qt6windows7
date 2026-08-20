// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <limits>

#include "base/check.h"
#include "base/feature_list.h"
#include "third_party/boringssl/src/include/openssl/crypto.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

// Prototype for ProcessPrng.
// See: https://learn.microsoft.com/en-us/windows/win32/seccng/processprng
extern "C" {
BOOL WINAPI ProcessPrng(PBYTE pbData, SIZE_T cbData);
}

namespace base {

namespace internal {

namespace {

// The BoringSSl helpers are duplicated in rand_util_fuchsia.cc and
// rand_util_posix.cc.
std::atomic<bool> g_use_boringssl;

BASE_FEATURE(kUseBoringSSLForRandBytes,
             "UseBoringSSLForRandBytes",
             FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

void ConfigureBoringSSLBackedRandBytesFieldTrial() {
  g_use_boringssl.store(FeatureList::IsEnabled(kUseBoringSSLForRandBytes),
                        std::memory_order_relaxed);
}

bool UseBoringSSLForRandBytes() {
  return g_use_boringssl.load(std::memory_order_relaxed);
}

}  // namespace internal

namespace {

// Windows 7 backport: ProcessPrng was introduced in Windows 10.
// bcryptprimitives.dll itself is present on Windows 7, so LoadLibraryW() below
// succeeds and only the export lookup fails - which the original code turns
// into a CHECK failure, aborting the process (int 3) the first time random
// bytes are requested.
//
// Fall back to RtlGenRandom(), which is what Chromium used before it switched
// to ProcessPrng. That switch was made to avoid opening a handle to
// \Device\KsecDD in the sandboxed renderer (see the comment below); on
// Windows 7 that handle is opened during sandbox warmup anyway.
using RtlGenRandomFn = BOOLEAN(WINAPI*)(PVOID RandomBuffer,
                                        ULONG RandomBufferLength);

RtlGenRandomFn GetRtlGenRandom() {
  HMODULE hmod = LoadLibraryW(L"advapi32.dll");
  CHECK(hmod);
  RtlGenRandomFn rtl_gen_random_fn =
      reinterpret_cast<RtlGenRandomFn>(GetProcAddress(hmod,
                                                      "SystemFunction036"));
  CHECK(rtl_gen_random_fn);
  return rtl_gen_random_fn;
}

// Import bcryptprimitives!ProcessPrng rather than cryptbase!RtlGenRandom to
// avoid opening a handle to \\Device\KsecDD in the renderer.
// Windows 7 backport: returns null when the export is missing, leaving the
// caller to use RtlGenRandom() instead of aborting.
decltype(&ProcessPrng) GetProcessPrng() {
  HMODULE hmod = LoadLibraryW(L"bcryptprimitives.dll");
  CHECK(hmod);
  decltype(&ProcessPrng) process_prng_fn =
      reinterpret_cast<decltype(&ProcessPrng)>(
          GetProcAddress(hmod, "ProcessPrng"));
  return process_prng_fn;
}

void RandBytes(span<uint8_t> output, bool avoid_allocation) {
  if (!avoid_allocation && internal::UseBoringSSLForRandBytes()) {
    // Ensure BoringSSL is initialized so it can use things like RDRAND.
    CRYPTO_library_init();
    // BoringSSL's RAND_bytes always returns 1. Any error aborts the program.
    (void)RAND_bytes(output.data(), output.size());
    return;
  }

  static decltype(&ProcessPrng) process_prng_fn = GetProcessPrng();
  if (!process_prng_fn) {
    // Windows 7 backport: RtlGenRandom takes a ULONG length, so feed it in
    // chunks.
    static RtlGenRandomFn rtl_gen_random_fn = GetRtlGenRandom();
    BYTE* cursor = static_cast<BYTE*>(output.data());
    size_t remaining = output.size();
    while (remaining > 0) {
      const ULONG chunk = static_cast<ULONG>(
          std::min<size_t>(remaining, std::numeric_limits<ULONG>::max()));
      const BOOLEAN success = rtl_gen_random_fn(cursor, chunk);
      CHECK(success);
      cursor += chunk;
      remaining -= chunk;
    }
    return;
  }

  BOOL success =
      process_prng_fn(static_cast<BYTE*>(output.data()), output.size());
  // ProcessPrng is documented to always return TRUE.
  CHECK(success);
}

}  // namespace

void RandBytes(span<uint8_t> output) {
  RandBytes(output, /*avoid_allocation=*/false);
}

void RandBytes(void* output, size_t output_length) {
  RandBytes(make_span(reinterpret_cast<uint8_t*>(output), output_length),
            /*avoid_allocation=*/false);
}

namespace internal {

double RandDoubleAvoidAllocation() {
  uint64_t number;
  RandBytes(as_writable_bytes(make_span(&number, 1u)),
            /*avoid_allocation=*/true);
  // This transformation is explained in rand_util.cc.
  return (number >> 11) * 0x1.0p-53;
}

}  // namespace internal

}  // namespace base

/* Copyright (c) 2014, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <openssl/rand.h>

#include "../fipsmodule/rand/internal.h"

#if defined(OPENSSL_RAND_WINDOWS)

#include <limits.h>
#include <stdlib.h>

OPENSSL_MSVC_PRAGMA(warning(push, 3))

#include <windows.h>

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) && \
    !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#include <bcrypt.h>
OPENSSL_MSVC_PRAGMA(comment(lib, "bcrypt.lib"))
#endif  // WINAPI_PARTITION_APP && !WINAPI_PARTITION_DESKTOP

OPENSSL_MSVC_PRAGMA(warning(pop))

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) && \
    !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

void CRYPTO_init_sysrand(void) {}

void CRYPTO_sysrand(uint8_t *out, size_t requested) {
  while (requested > 0) {
    ULONG output_bytes_this_pass = ULONG_MAX;
    if (requested < output_bytes_this_pass) {
      output_bytes_this_pass = (ULONG)requested;
    }
    if (!BCRYPT_SUCCESS(BCryptGenRandom(
            /*hAlgorithm=*/NULL, out, output_bytes_this_pass,
            BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
      abort();
    }
    requested -= output_bytes_this_pass;
    out += output_bytes_this_pass;
  }
}

#else

// See: https://learn.microsoft.com/en-us/windows/win32/seccng/processprng
typedef BOOL (WINAPI *ProcessPrngFunction)(PBYTE pbData, SIZE_T cbData);
static ProcessPrngFunction g_processprng_fn = NULL;

// Windows 7 backport: ProcessPrng only exists from Windows 10 on, while
// bcryptprimitives.dll itself is present on Windows 7 - so LoadLibraryW()
// succeeds and only the export lookup fails, which the original code turned
// into an unconditional abort(). That abort fires the first time anything asks
// BoringSSL for random bytes, which in practice is as soon as the network stack
// starts, taking the whole browser process with it.
//
// RtlGenRandom (advapi32!SystemFunction036) is the same system RNG BoringSSL
// used before it switched to ProcessPrng. The switch was made to avoid opening
// a handle to \Device\KsecDD from inside the Chromium sandbox; on Windows 7 the
// sandbox warmup opens that handle before the token is lowered, so the fallback
// works there too.
typedef BOOLEAN (WINAPI *RtlGenRandomFunction)(PVOID RandomBuffer,
                                               ULONG RandomBufferLength);
static RtlGenRandomFunction g_rtlgenrandom_fn = NULL;

static void init_processprng(void) {
  HMODULE hmod = LoadLibraryW(L"bcryptprimitives");
  if (hmod != NULL) {
    g_processprng_fn = (ProcessPrngFunction)GetProcAddress(hmod, "ProcessPrng");
  }
  if (g_processprng_fn != NULL) {
    return;
  }

  // Windows 7 backport: fall back to RtlGenRandom.
  HMODULE advapi = LoadLibraryW(L"advapi32.dll");
  if (advapi != NULL) {
    g_rtlgenrandom_fn =
        (RtlGenRandomFunction)GetProcAddress(advapi, "SystemFunction036");
  }
  if (g_rtlgenrandom_fn == NULL) {
    abort();
  }
}

void CRYPTO_init_sysrand(void) {
  static CRYPTO_once_t once = CRYPTO_ONCE_INIT;
  CRYPTO_once(&once, init_processprng);
}

void CRYPTO_sysrand(uint8_t *out, size_t requested) {
  CRYPTO_init_sysrand();
  // On non-UWP configurations, use ProcessPrng instead of BCryptGenRandom
  // to avoid accessing resources that may be unavailable inside the
  // Chromium sandbox. See https://crbug.com/74242
  if (g_processprng_fn != NULL) {
    if (!g_processprng_fn(out, requested)) {
      abort();
    }
    return;
  }

  // Windows 7 backport: RtlGenRandom takes a ULONG length, so feed it the
  // request in chunks.
  while (requested > 0) {
    ULONG output_bytes_this_pass = ULONG_MAX;
    if (requested < output_bytes_this_pass) {
      output_bytes_this_pass = (ULONG)requested;
    }
    if (!g_rtlgenrandom_fn(out, output_bytes_this_pass)) {
      abort();
    }
    requested -= output_bytes_this_pass;
    out += output_bytes_this_pass;
  }
}

#endif  // WINAPI_PARTITION_APP && !WINAPI_PARTITION_DESKTOP

void CRYPTO_sysrand_for_seed(uint8_t *out, size_t requested) {
  CRYPTO_sysrand(out, requested);
}

#endif  // OPENSSL_RAND_WINDOWS

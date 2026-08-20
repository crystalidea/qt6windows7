// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/random.h"

#include <cstddef>
#include <cstdint>

#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_FUCHSIA)
#include <zircon/syscalls.h>
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <asm/unistd.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif BUILDFLAG(IS_MAC)
#include <sys/random.h>
#include <unistd.h>
#elif BUILDFLAG(IS_NACL)
#include <nacl/nacl_random.h>
#endif

#if BUILDFLAG(IS_POSIX)
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#if BUILDFLAG(IS_WIN)
// Prototype for ProcessPrng.
// See: https://learn.microsoft.com/en-us/windows/win32/seccng/processprng
extern "C" {
BOOL WINAPI ProcessPrng(PBYTE pbData, SIZE_T cbData);
}
#endif

namespace ipcz::reference_drivers {

namespace {

#if BUILDFLAG(IS_WIN)
// Windows 7 backport: ProcessPrng was introduced in Windows 10, while
// bcryptprimitives.dll itself is present on Windows 7, so only the export
// lookup fails. ABSL_ASSERT compiles to nothing in release builds, which would
// leave a null function pointer to be called below. Fall back to RtlGenRandom()
// instead, which is what Chromium used before it switched to ProcessPrng.
using RtlGenRandomFn = BOOLEAN(WINAPI*)(PVOID RandomBuffer,
                                        ULONG RandomBufferLength);

RtlGenRandomFn GetRtlGenRandom() {
  HMODULE hmod = LoadLibraryW(L"advapi32.dll");
  ABSL_ASSERT(hmod);
  RtlGenRandomFn rtl_gen_random_fn = reinterpret_cast<RtlGenRandomFn>(
      GetProcAddress(hmod, "SystemFunction036"));
  ABSL_ASSERT(rtl_gen_random_fn);
  return rtl_gen_random_fn;
}

decltype(&ProcessPrng) GetProcessPrng() {
  HMODULE hmod = LoadLibraryW(L"bcryptprimitives.dll");
  ABSL_ASSERT(hmod);
  if (!hmod) {
    return nullptr;
  }
  decltype(&ProcessPrng) process_prng_fn =
      reinterpret_cast<decltype(&ProcessPrng)>(
          GetProcAddress(hmod, "ProcessPrng"));
  return process_prng_fn;
}
#endif

#if defined(OS_POSIX) && !BUILDFLAG(IS_MAC)
void RandomBytesFromDevUrandom(absl::Span<uint8_t> destination) {
  static int urandom_fd = [] {
    for (;;) {
      int result = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
      if (result >= 0) {
        return result;
      }
      ABSL_ASSERT(errno == EINTR);
    }
  }();

  while (!destination.empty()) {
    ssize_t result = read(urandom_fd, destination.data(), destination.size());
    if (result < 0) {
      ABSL_ASSERT(errno == EINTR);
      continue;
    }
    destination.remove_prefix(result);
  }
}
#endif

}  // namespace

void RandomBytes(absl::Span<uint8_t> destination) {
#if BUILDFLAG(IS_WIN)
  static decltype(&ProcessPrng) process_prng_fn = GetProcessPrng();
  if (process_prng_fn) {
    process_prng_fn(destination.data(), destination.size());
  } else {
    // Windows 7 backport: RtlGenRandom takes a ULONG length, so feed it in
    // chunks.
    static RtlGenRandomFn rtl_gen_random_fn = GetRtlGenRandom();
    uint8_t* cursor = destination.data();
    size_t remaining = destination.size();
    while (remaining > 0) {
      const ULONG chunk = static_cast<ULONG>(
          remaining > 0xFFFFFFFFu ? 0xFFFFFFFFu : remaining);
      rtl_gen_random_fn(cursor, chunk);
      cursor += chunk;
      remaining -= chunk;
    }
  }
#elif BUILDFLAG(IS_FUCHSIA)
  zx_cprng_draw(destination.data(), destination.size());
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  while (!destination.empty()) {
    ssize_t result =
        syscall(__NR_getrandom, destination.data(), destination.size(), 0);
    if (result == -1 && errno == EINTR) {
      continue;
    } else if (result > 0) {
      destination.remove_prefix(result);
    } else {
      RandomBytesFromDevUrandom(destination);
      return;
    }
  }
#elif BUILDFLAG(IS_MAC)
  const bool ok = getentropy(destination.data(), destination.size()) == 0;
  ABSL_ASSERT(ok);
#elif BUILDFLAG(IS_IOS)
  RandomBytesFromDevUrandom(destination);
#elif BUILDFLAG(IS_NACL)
  while (!destination.empty()) {
    size_t nread;
    nacl_secure_random(destination.data(), destination.size(), &nread);
    destination.remove_prefix(nread);
  }
#else
#error "Unsupported platform"
#endif
}

}  // namespace ipcz::reference_drivers

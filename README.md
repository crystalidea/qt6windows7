This repository provides a backport of Qt 6, tailored for compatibility with Windows 7, 8 and 8.1. It contains patched source files, along with some additional required files.

Each top-level folder here mirrors one Qt repository: apply a patch set by copying the contents of that folder over your checkout of the same name, replacing the existing files. `qtbase` is the backport proper and is always needed; `qtmultimedia` and `qtwebengine` are only needed if you build those modules. Every module is covered in its own section below.

The most recent supported version is **6.8.4** however many older versions are supported as well (see **Older versions** section).

This approach builds upon the methodology discussed in this forum [thread](https://forum.qt.io/topic/133002/qt-creator-6-0-1-and-qt-6-2-2-running-on-windows-7/60) but offers significant enhancements, including important fallbacks to the default Qt 6 behavior when running on newer versions of Windows.

You can compile it yourself using your preferred compiler and build options or can use our [compile_win.pl](https://github.com/crystalidea/qt-build-tools/tree/master/6.8.1) build script, which utilizes Visual C++ 2022 and includes OpenSSL 3.0.13 statically linked. Alternatively, you can download our [prebuild Qt dlls](https://github.com/crystalidea/qt6windows7/releases), which also include the Qt Designer binary for demonstration purposes.

**Qt 6.8.4 designer running on Windows 7**:

![Qt Designer](designer.png)

### qtbase

The backport proper, and the part everything else on this page assumes is in place. Stock Qt 6 imports a good number of Windows 8/8.1/10 entry points statically, so it cannot even be loaded on Windows 7. Nearly every patch here follows the same recipe: look the function up with `GetProcAddress()` at run time and fall back to the older API when it is missing, so that newer Windows keeps taking exactly the path it took before.

**corelib**

- `io/qstandardpaths_win.cpp` — low-integrity process detection is a Windows 8 concept; report false on Windows 7 instead. Also fixes the buffer-size probe of `GetTokenInformation()`.
- `kernel/qeventdispatcher_win.cpp` — `SetCoalescableTimer()`, falling back to plain `SetTimer()`.
- `kernel/qfunctions_win.cpp` — `GetCurrentPackageFullName()`; without it the process is simply not a packaged app.
- `thread/qfutex_p.h`, `thread/qmutex.cpp`, `thread/qmutex_p.h`, `thread/qmutex_win.cpp` (new) — the futex path uses `WaitOnAddress()` (Windows 8), so it is disabled and `QMutex` gets an event-based Windows implementation instead.
- `thread/qthread_win.cpp` — `SetThreadDescription()` (Windows 10) for thread names, falling back to the classic debugger exception.

**gui**

- `rhi/qrhid3d11.cpp`, `rhi/qrhid3d11_p.h` — `CreateDXGIFactory2()` (Windows 8), plus a separate swapchain path for Windows 7.
- `rhi/qrhid3d12.cpp` — `CreateDXGIFactory2()`, `D3D12CreateDevice()` and `D3D12GetDebugInterface()`; when they are absent the D3D12 backend just reports itself unavailable.
- `text/windows/qwindowsfontdatabasebase.cpp` — `SystemParametersInfoForDpi()` (Windows 10), falling back to `SystemParametersInfo()`.

**network**

- `kernel/qdnslookup_win.cpp` — `DnsQueryEx()` (Windows 8); the older `DnsQuery()` path is restored for Windows 7.

**platform plugin (windows)**

- `vxkex.h` (new) — Windows 7 stand-ins for the per-monitor DPI helpers (`GetSystemMetricsForDpi()`, `AdjustWindowRectExForDpi()` and friends), which simply scale the DPI-unaware originals.
- `qwindowscontext.h`, `qwindowscontext.cpp` — the central place where the optional user32/shcore entry points are resolved: the pointer input API (Windows 8), the per-DPI metrics and the shcore DPI awareness calls (Windows 8.1/10). The rest of the plugin asks this struct instead of calling the imports directly.
- `qwindowsdrag.cpp`, `qwindowskeymapper.cpp`, `qwindowspointerhandler.cpp`, `qwindowsscreen.cpp`, `qwindowswindow.cpp`, `qwindowsintegration.cpp` — use those resolved pointers, with the `vxkex.h` fallbacks where a DPI-aware metric is needed.
- `qwindowstheme.cpp` — accent colours come from WinRT `UISettings` on Windows 10; on Windows 7 the palette falls back to the system colours.
- `qwin10helpers.cpp` — loads `combase.dll` dynamically instead of importing it, so the WinRT helpers degrade gracefully when it is not there.
- `uiautomation/qwindowsuiawrapper_p.h`, `qwindowsuiawrapper.cpp` (new), `qwindowsuiamainprovider.cpp`, `qwindowsuiaaccessibility.cpp` — accessibility goes through a wrapper that resolves the UI Automation entry points at run time, since Windows 7 ships an older `uiautomationcore.dll`.

**widgets**

- `styles/qwindowsstyle.cpp` — the same per-DPI metric helpers as above, via `vxkex.h`.

One more file completes the set: `corelib/platform/windows/qt_winrtbase_p.h` resolves the C++/WinRT entry points (`RoGetActivationFactory()` and friends) through `combase.dll` at run time. Without it the WinRT imports alone keep the process from starting on Windows 7 — and because other modules compile against this header too, it is what lets qtmultimedia use WinRT without extra patches of its own.

### qtmultimedia

Playing media needs both the FFmpeg plugin, which would not load at all, and the WASAPI audio backend, which could not open a device and then crashed on shutdown. Four patches are provided in the `qtmultimedia` folder.

The first two concern the WinRT window capture support, compiled in whenever the `cpp_winrt` feature is enabled. Without them the plugin cannot be loaded on Windows 7, which leaves Qt Multimedia without a backend — `QMediaPlayer` reports itself unavailable and nothing plays.

- `src/plugins/multimedia/ffmpeg/CMakeLists.txt`

  The plugin linked `WindowsApp.lib`, the UWP umbrella import library. Linking it makes every plain kernel32 function resolve through `api-ms-win-core-*` API sets instead of KERNEL32.dll, and several of those sets only exist from Windows 8 onwards (`libraryloader-l1-2-0` is Windows 8.1; `synch-l1-2-0`, `localization-l1-2-0`, `heap-l2-1-0` and `processthreads-l1-1-1` are Windows 8), so the plugin cannot be loaded on Windows 7 at all. Every other Qt module imports KERNEL32.dll directly. Nothing is lost by dropping it: the WinRT entry points the plugin needs are resolved at run time through the `qt_winrtbase_p.h` replacement from the qtbase part of this backport, which `qffmpegwindowcapture_uwp.cpp` picks up via `qfactorycacheregistration_p.h`, so no static WinRT imports are left and `WindowsApp.lib` was only acting as the kernel32 umbrella.

- `src/plugins/multimedia/ffmpeg/qffmpegwindowcapture_uwp.cpp`

  The same plugin statically imports `CreateDirect3D11DeviceFromDXGIDevice` from d3d11.dll, and that export was only added in Windows 8.1. d3d11.dll itself is available on Windows 7 SP1 with the Platform Update (KB2670838, which patched qtbase needs anyway), so the module is found and just this one export is missing — enough to make the whole plugin unloadable with `ERROR_PROC_NOT_FOUND`. The function only hands a D3D11 device to the WinRT capture API, so the entire media backend was being lost for a feature unrelated to playback. The patch resolves it at run time: nothing changes on Windows 8.1 and later, while on Windows 7 only window capture fails, through the same `hresult` error path its callers already handle.

Both keep the `cpp_winrt` feature enabled, so WinRT window capture is still built and behaves exactly as before on Windows 8.1 and later; only the two Windows 8-and-later dependencies are moved from load time to run time. Configuring Qt with `-no-feature-cpp-winrt` would sidestep both problems without patching qtmultimedia, but it drops the feature everywhere, including on the newer Windows versions where it works — which is what this backport exists to avoid.

Audio needs the WASAPI backend. `createAudioClient()` activates `IAudioClient3`, an interface that only exists from Windows 10 on, so on Windows 7 activation fails and no audio device can be opened at all — and worse, `openAudioClient()` then leaves the audio client empty while the stream object lives on, so `QWASAPIAudioSinkStream::stop()` still called `audioClientStop()` on it and crashed the application with an access violation as soon as playback was closed.

- `src/multimedia/windows/qwindowsaudioutils.cpp`, `qwindowsaudioutils_p.h`, `qwindowsaudiosink_p.h`, `qwindowsaudiosource_p.h`

  The audio client is now held as the base `IAudioClient`, which has been around since Vista and carries every method the playback and capture paths actually use. `createAudioClient()` still asks for `IAudioClient3` first, so on Windows 10 and later the very same object is obtained as before and nothing changes there; only when that fails does it fall back to `IAudioClient`. Setting the endpoint role goes through `IAudioClient2::SetClientProperties` and is therefore skipped when only the base interface is available — on Windows 7 the role stays at its default. The `audioClientStart/Stop/Reset()` helpers additionally return false on an empty client instead of dereferencing it, which is what their callers already expect from a failed call and which fixes the crash on close. Both playback and capture go through these helpers, so both are covered.

Verified with Qt 6.8.4: video plays with sound on Windows 7 SP1.

### qtwebengine (Qt WebEngine and Qt PDF)

Neither module works on Windows 7 out of the box, even with patched qtbase. Qt PDF needs the two patches described at the end of this section; Qt WebEngine needs those plus the rest of the `qtwebengine` folder.

The reason there is so much to do here is that Chromium dropped Windows 7 and 8 in M110, and Qt 6.8 carries Chromium 122. Almost every patch below is therefore not an invention but a restoration: the same file in Chromium 109.0.5414.120 — the last release that supported Windows 7 — resolved the entry point at run time or skipped it behind a version check, and M110 deleted that code. Each patch is marked in place with a `Windows 7 backport:` comment, and the handful of cases where 109 has no equivalent are called out below. As everywhere else in this repository, the fallback only engages when `GetProcAddress()` comes back empty, so Windows 8 and later keep taking exactly the path they take today.

**Loading the library at all**

Stock `Qt6WebEngineCore.dll` imports about thirty entry points that Windows 7 does not have, so it cannot be loaded there — and neither can `QtWebEngineProcess.exe`, `webenginedriver.exe` or `qwebengine_convert_dict.exe`, which pull the same code in statically. Whole API-set DLLs are involved, not just individual exports, and an absent API set is as fatal as an absent function.

- `base/win/core_winrt_util.cc`, `base/win/scoped_winrt_initializer.cc`, `base/win/scoped_hstring.cc`, `base/win/hstring_reference.cc` — the WinRT and HSTRING entry points (`RoInitialize()`, `RoGetActivationFactory()`, `WindowsCreateString()` and friends), imported through `api-ms-win-core-winrt-l1-1-0.dll` and `api-ms-win-core-winrt-string-l1-1-0.dll`. They are resolved out of `combase.dll` instead, which is the same code the API set forwards to on newer Windows. On Windows 7 they are simply missing, and the WinRT-backed features — Web Bluetooth, WinRT MIDI, WinRT geolocation, `UISettings` accent colours, the on-screen keyboard — return the failures their callers already handle. This mirrors what `qt_winrtbase_p.h` does for qtbase.
- `base/win/win_util.cc` — the same WinRT wrapper for tablet-mode detection, plus `GetProcessMitigationPolicy()` (Windows 8) and `SetProcessDpiAwareness()` (shcore, Windows 8.1). Where win32k syscalls cannot be disabled at all, user32 and gdi32 are by definition available; where shcore is absent, the existing `SetProcessDPIAware()` fallback runs, which is all Windows 7 offers.
- `base/time/time_win.cc` — `QueryUnbiasedInterruptTimePrecise()` (`api-ms-win-core-realtime-l1-1-1.dll`, Windows 10), falling back to `QueryUnbiasedInterruptTime()`, in kernel32 since Windows 7. Same clock, same 100 ns units, same unbiased semantics — only tick resolution instead of interpolated. This one has no 109 equivalent: `LiveTicks` did not exist yet.
- `base/power_monitor/speed_limit_observer_win.cc`, `third_party/crashpad/crashpad/snapshot/win/system_snapshot_win.cc` — `CallNtPowerInformation()`, which current SDKs route through `api-ms-win-power-base-l1-1-0.dll`. It is loaded from `powrprof.dll`, where it has lived since Windows XP and where the API set forwards to anyway.
- `base/power_monitor/power_monitor_device_source_win.cc` — `RegisterSuspendResumeNotification()` (Windows 8). On Windows 7 registration is not needed at all: `WM_POWERBROADCAST`/`PBT_APMSUSPEND` reaches every top-level window, and registering is only required for modern-standby machines, which is exactly what the comment above the call says.
- `base/memory/discardable_shared_memory.cc`, `base/allocator/.../page_allocator_internals_win.h` — `DiscardVirtualMemory()` (Windows 8.1). The `VirtualAlloc(MEM_RESET)` fallback is already written directly below each call, because the function is buggy on Windows 10 SP0; v8 in the same source tree still resolves it this way.
- `base/files/file_util_win.cc` — `PrefetchVirtualMemory()` (Windows 8), falling back to the existing `PreReadFileSlow()`, which warms the same pages with a sequential read.
- `base/threading/platform_thread_win.cc`, `base/process/process_win.cc` — `SetThreadInformation()` and `SetProcessInformation()` (Windows 8). They configure thread memory priority and EcoQoS power throttling, neither of which exists on Windows 7; both call sites already treat failure as expected, one of them saying so in its own comment.
- `base/trace_event/trace_logging_minimal_win.cc` — `EventSetInformation()` (Windows 8), which attaches provider traits to an ETW provider. The call is documented in place as best-effort; Microsoft's own message-compiler output offers this very `GetProcAddress()` mode for pre-Windows 8 targets. `EventRegister()`/`EventWrite()` are unaffected, so tracing keeps working.
- `v8/src/libplatform/etw/etw-provider-win.h` — the same `EventSetInformation()`, reached from a completely different direction and easy to miss: v8 registers its ETW providers through the SDK's `TraceLoggingProvider.h`, whose `TraceLoggingRegister()` calls `TraceLoggingSetInformation()` to attach provider traits. Because Chromium targets Windows 10, the header compiles that into a direct call. Setting `TLG_HAVE_EVENT_SET_INFORMATION` to 2 before including it selects the SDK's own documented pre-Windows 8 behaviour — look the function up through `GetModuleHandleExW`/`GetProcAddress` and return an error when it is absent. TraceLogging is documented to work correctly without it, so providers keep registering and events keep being written. This header is the only place in the whole tree that includes `TraceLoggingProvider.h`, which is what makes the one-line fix sufficient.
- `media/midi/midi_manager_winrt.cc` — `CM_Get_DevNode_PropertyW()` (Windows 8). The WinRT MIDI backend is behind a feature that is disabled by default, and Windows 7 uses the winmm-based manager regardless.
- `services/proxy_resolver_win/winhttp_api_wrapper_impl.cc` — the asynchronous proxy resolution APIs (`WinHttpCreateProxyResolver()` and friends, Windows 8). `WindowsSystemProxyResolutionService::IsSupported()` already requires Windows 10 1607, so nothing reaches them; proxy resolution goes through `WinHttpGetIEProxyConfigForCurrentUser()` and PAC evaluation, both fine on Windows 7.
- `ui/display/win/screen_win.cc`, `third_party/webrtc/.../win/screen_capture_utils.cc` — `GetDpiForMonitor()` (shcore, Windows 8.1). Both callers already fall back to the system DPI, which on Windows 7 is not an approximation but the correct answer: per-monitor DPI does not exist there.
- `ui/gfx/win/d3d_shared_fence.cc`, `gpu/command_buffer/service/dxgi_shared_handle_manager.cc` — `CompareObjectHandles()`. This is the one function with no Windows 7 equivalent at all, since it wraps `NtCompareObjects()` and that kernel has no such call. Both uses need NT handles for D3D11 shared resources, i.e. Windows 8 and later, so they are unreachable on Windows 7; the fallbacks are conservative anyway — compare the handle values in one case, skip a consistency assertion in the other.
- `sandbox/win/src/process_mitigations.cc` — `GetProcessMitigationPolicy()`, `SetProcessMitigationPolicy()`, `SetDefaultDllDirectories()` and `SetThreadInformation()`, restored to the 109 arrangement together with its version gates. With the supported-mitigations mask reading back as zero, no mitigation attribute is attached when a child process is created, so the sandbox falls back to the restricted token, job object, alternate desktop and integrity level — precisely the profile Chromium used on Windows 7. The 32-bit `DWORD`-sized mitigation mask that Windows 7 expects comes back too; without it `UpdateProcThreadAttribute()` fails and no child process starts at all on x86. One genuine loss: `SetDefaultDllDirectories()` needs KB2533623 on Windows 7, and where that update is missing the DLL search-order hardening is skipped rather than the process refusing to start.
- `sandbox/win/src/app_container_base.cc` — `CreateAppContainerProfile()` and `DeriveAppContainerSidFromAppContainerName()` (Windows 8), loaded from `userenv.dll` at run time. Qt already disables AppContainer entirely (`sandbox/features.cc` returns false under `TOOLKIT_QT`), so this is purely about the import.

**Crashes that remain once it loads**

- `base/rand_util_win.cc`, `sandbox/policy/win/sandbox_warmup.cc`, `third_party/ipcz/src/reference_drivers/random.cc`

  Three more copies of the `bcryptprimitives!ProcessPrng` pattern already patched for Qt PDF, and each one is fatal on its own. The DLL exists on Windows 7 so `LoadLibraryW()` succeeds and only the export lookup fails, which the surrounding `CHECK` turns into a deliberate abort — in `base::RandBytes()` that is the first random number anything asks for, and in the sandbox warmup it is every sandboxed process at startup, since the `WinSboxWarmupProcessPrng` feature is enabled by default. All three fall back to `RtlGenRandom()` (`advapi32!SystemFunction036`), which is what Chromium used before the switch and which is the same system DRBG. The switch away from it was made to avoid opening a handle to `\Device\KsecDD` in the renderer; on Windows 7 the warmup opens that handle before the token is lowered anyway, which is exactly what the untaken branch of that feature already did. The ipcz copy is the nastiest of the three — it asserts with `ABSL_ASSERT`, which compiles to nothing in release builds and would leave a null function pointer to be called. That file is not part of the current build, and is patched defensively.

- `sandbox/win/src/startup_information_helper.h`, `startup_information_helper.cc`, `target_process.cc`

  The job object is handed to the child through `PROC_THREAD_ATTRIBUTE_JOB_LIST`, an attribute that only exists from Windows 10 on. On Windows 7 `UpdateProcThreadAttribute()` rejects it, `BuildStartupInformation()` fails and **no renderer, GPU or utility process can be started at all**. Below Windows 10 the attribute is now left out and the still-suspended target is assigned with `AssignProcessToJobObject()` before it is resumed, which is how Chromium did it up to 109; `CREATE_BREAKAWAY_FROM_JOB` comes back for pre-Windows 8, where nested jobs do not exist. The only difference is that the process exists outside the job for the moment between creation and assignment, while suspended.

**Delay-loaded Media Foundation entry points**

- `media/base/win/dxgi_device_manager.cc`, `media/renderers/win/media_foundation_renderer.cc`, `media/gpu/windows/media_foundation_video_encode_accelerator_win.cc`

  `MFCreateDXGIDeviceManager()`, `MFCreateDXGISurfaceBuffer()` and `MFLockDXGIDeviceManager()`/`MFUnlockDXGIDeviceManager()` were added in Windows 8. These are delay-loaded, so they do not stop the library from loading — instead `mfplat.dll` is found, the export is not, and the delay-load helper faults the first time a camera is opened or hardware encoding is attempted. Each is resolved by hand and reported as a plain `HRESULT` failure, which the callers already handle: capture falls back to CPU frames, encoding to software, and the Media Foundation renderer to the ordinary pipeline.

**Qt PDF**

- `src/pdf/configure/BUILD.root.gn.in`

  QtPdf linked `dloadhelper.lib`, the delay-load helper intended for UWP builds. It calls `kernel32!ResolveDelayLoadedAPI` and `DelayLoadFailureHook` through *static* imports, and both were introduced in Windows 8, so `Qt6Pdf.dll` could not be loaded at all on Windows 7 (the plugin fails with `ERROR_PROC_NOT_FOUND` — "The specified procedure could not be found"). Chromium itself links `delayimp.lib` for non-UWP builds and the patch does the same. `delayimp.lib` looks the OS helper up at run time and falls back to its own implementation when it is absent, so delay loading keeps working on every Windows version.

- `src/3rdparty/chromium/base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/rand_util_win.cc`

  PartitionAlloc obtains random bytes through `bcryptprimitives!ProcessPrng`, which exists only on Windows 10 and later. The DLL itself is present on Windows 7, so `LoadLibraryW()` succeeds and only the export lookup fails — which the surrounding `CHECK` turns into a deliberate abort (`STATUS_BREAKPOINT`), crashing the application the first time a PDF is opened. The patch falls back to `RtlGenRandom` (`advapi32!SystemFunction036`), which is what Chromium used before it switched to `ProcessPrng`. `ProcessPrng` is still preferred whenever it is available, so behaviour on Windows 10 and later is unchanged.

Verified with Qt 6.8.4: viewing PDFs works on Windows 7 SP1.

The Qt WebEngine part of this section is **not verified on Windows 7 yet** — it is derived from the import table of a real 6.8.4 build and from the Chromium 109 sources, but the patched build has still to be run there. Treat it as a starting point rather than a finished port, and please report what you find. A rebuild should end with no Windows 8-or-later imports left in `Qt6WebEngineCore.dll`, which is worth checking before anything else; the first run is best done with `QTWEBENGINE_DISABLE_SANDBOX=1` so that a sandbox problem can be told apart from everything else.

### Other modules

Many other Qt 6 modules need no patches at all: built against patched qtbase, they run on Windows 7 as they are. Verified:

- qt5compat
- qtimageformats
- qtsvg
- qttools
- ... please let me know which work and which don't !

### Known issues:

- QRhi using DirectX 11/12 is not ported
- Qt WebEngine is patched but not yet verified on Windows 7 (see the qtwebengine section)
- On Windows 7, Qt WebEngine gives up the features the OS never had: WinRT-backed ones (Web Bluetooth, WinRT MIDI, WinRT geolocation), hardware video encoding and Media Foundation camera capture through DXGI, per-monitor DPI, and the Windows 8-and-later sandbox mitigations
- `SetDefaultDllDirectories()` needs KB2533623 on Windows 7; without that update the sandbox skips its DLL search-order hardening (KB2670838, already required by patched qtbase, is needed for the GPU stack)

### Older versions:

- [Qt 6.8.3](https://github.com/crystalidea/qt6windows7/releases/tag/v6.8.3)
- [Qt 6.8.2](https://github.com/crystalidea/qt6windows7/releases/tag/v6.8.2)
- [Qt 6.8.1](https://github.com/crystalidea/qt6windows7/releases/tag/v6.8.1)
- [Qt 6.8.0](https://github.com/crystalidea/qt6windows7/releases/tag/v6.8.0)
- [Qt 6.7.2](https://github.com/crystalidea/qt6windows7/releases/tag/v6.7.2)
- [Qt 6.6.3](https://github.com/crystalidea/qt6windows7/releases/tag/v6.6.3)
- [Qt 6.6.2](https://github.com/crystalidea/qt6windows7/releases/tag/v6.6.2)
- [Qt 6.6.1](https://github.com/crystalidea/qt6windows7/releases/tag/v6.6.1)
- [Qt 6.6.0](https://github.com/crystalidea/qt6windows7/releases/tag/v6.6.0)
- [Qt 6.5.3](https://github.com/crystalidea/qt6windows7/releases/tag/6.5.3-win7)
- [Qt 6.5.1](https://github.com/crystalidea/qt6windows7/releases/tag/6.5.1-win7)

### Packaging the result

`make_win7_archive.pl` builds the archive published with each release — the one you get from the link at the top of this page — out of a Qt installation compiled with this backport:

```
perl make_win7_archive.pl
```

Run without arguments it packs every installation it finds in `C:\qt6` and `C:\qt6_x64` — the two assets of a release in one go. Pass a path to pack just one, and a name after it to choose the file name.

The architecture is read from `Qt6Core.dll`, so a 32-bit installation produces `qt6_x86_to_run_on_windows7.7z` and a 64-bit one `qt6_x64_to_run_on_windows7.7z`, each with the matching runtime.

It collects the Qt libraries from `bin`, the plugins applications load by path (`platforms`, `styles`, `imageformats` and `multimedia`, each skipped when the module was not built), and Qt Designer, which is a quick way to tell whether a build really runs on Windows 7. Debug builds are left out; a plain name match would not do here, since `qdirect2d.dll` and a few others genuinely end in a 'd', so a library counts as a debug build only when its release twin sits next to it.

The Visual C++ runtime is packed as well, taken from the newest redistributable installed alongside the compiler, for the architecture Qt was built for. That is not just convenience: a redistributable **older** than the toolset that compiled Qt is unsupported, and it fails by crashing rather than by refusing to load — a Windows 7 machine carrying 14.36 will start a Designer built with 14.44 and then fault inside `MSVCP140.dll`. Shipping the matching runtime keeps the archive self-contained. Set `$include_msvc_runtime` to 0 in the script to leave it out.

### License

The repository shares Qt Community Edition terms which imply [Open-Source terms and conditions (GPL and LGPL)](https://www.qt.io/licensing/open-source-lgpl-obligations?hsLang=en).

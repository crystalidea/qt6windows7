This is Qt 6.6.2 backport that runs on Windows 7 (what?). The repository contains patched source files from the qtbase module. 
Approach is based on this [forum thread](https://forum.qt.io/topic/133002/qt-creator-6-0-1-and-qt-6-2-2-running-on-windows-7/60) but better: many improvements amongst important fallbacks to default Qt 6 behaviour when running on newer Windows.

You can use [our prebuild binaries](https://github.com/crystalidea/qt6windows7/releases) (we used Visual C++ 2019 with OpenSSL 3.0.13 statically linked, see [compile_win.pl](https://github.com/crystalidea/qt-build-tools/tree/master/6.6.2) script) or compile Qt yourself.

Known issues:

- Scalled UI is somewhat too big for 125% scalling option set in Windows 7. Probably can be be tweaked with dpi settings (?)
- QRhi using DirectX 11/12 is not ported

Qt 6.6 designer running on Windows 7:

![sshot-533](https://github.com/crystalidea/qt6windows7/assets/2600624/86b1763a-13d3-4b93-b4a9-654c70838ef2)

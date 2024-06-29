This is Qt 6.7.2 backport that runs on Windows 7 (what?). The repository contains patched source files from the qtbase module. 
Approach is based on this [forum thread](https://forum.qt.io/topic/133002/qt-creator-6-0-1-and-qt-6-2-2-running-on-windows-7/60) but better: many improvements amongst important fallbacks to default Qt 6 behaviour when running on newer Windows.

You can use [our prebuild binaries](https://github.com/crystalidea/qt6windows7/releases) (we used Visual C++ 2019 with OpenSSL 3.0.13 statically linked, see [compile_win.pl](https://github.com/crystalidea/qt-build-tools/tree/master/6.6.2) script) or compile Qt yourself using compiler and build options you need.

**Qt 6.7.2 designer running on Windows 7**:

![image](https://github.com/crystalidea/qt6windows7/assets/2600624/41f4291a-082a-41e8-a09d-c3b9e7f36e9e)

### Known issues:

- QRhi using DirectX 11/12 is not ported
- setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Round) is recommended

### Older versions:

- [Qt 6.6.3](https://github.com/crystalidea/qt6windows7/releases/tag/v6.6.3)
- [Qt 6.6.2](https://github.com/crystalidea/qt6windows7/releases/tag/v6.6.2)
- [Qt 6.6.1](https://github.com/crystalidea/qt6windows7/releases/tag/v6.6.1)
- [Qt 6.6.0](https://github.com/crystalidea/qt6windows7/releases/tag/v6.6.0)
- [Qt 6.5.3](https://github.com/crystalidea/qt6windows7/releases/tag/6.5.3-win7)
- [Qt 6.5.1](https://github.com/crystalidea/qt6windows7/releases/tag/6.5.1-win7)

### License

The repository shares Qt Community Edition terms which imply [Open-Source terms and conditions (GPL and LGPL)](https://www.qt.io/licensing/open-source-lgpl-obligations?hsLang=en).

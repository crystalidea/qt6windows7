This is Qt 6.7.2 **qtbase module** backport that runs on Windows 7/8. The repository contains patched source files from the qtbase module. 
Approach is based on this [forum thread](https://forum.qt.io/topic/133002/qt-creator-6-0-1-and-qt-6-2-2-running-on-windows-7/60) but better: many improvements amongst important fallbacks to default Qt 6 behaviour when running on newer Windows.

- After replacing qtbase source files with the patched ones from this repository, you can compile Qt yourself using compiler and build options you need.
- You can use our [compile_win.pl](https://github.com/crystalidea/qt-build-tools/tree/master/6.7.2) build script (uses Visual C++ 2022 with OpenSSL 3.0.13 statically linked)
- You can download our [prebuild Qt dlls](https://github.com/crystalidea/qt6windows7/releases) which also have Qt Designer binary for demonstration

**Qt 6.7.2 designer running on Windows 7**:

![image](https://github.com/crystalidea/qt6windows7/assets/2600624/41f4291a-082a-41e8-a09d-c3b9e7f36e9e)

### Known issues:

- QRhi using DirectX 11/12 is not ported

### Older versions:

- [Qt 6.6.3](https://github.com/crystalidea/qt6windows7/releases/tag/v6.6.3)
- [Qt 6.6.2](https://github.com/crystalidea/qt6windows7/releases/tag/v6.6.2)
- [Qt 6.6.1](https://github.com/crystalidea/qt6windows7/releases/tag/v6.6.1)
- [Qt 6.6.0](https://github.com/crystalidea/qt6windows7/releases/tag/v6.6.0)
- [Qt 6.5.3](https://github.com/crystalidea/qt6windows7/releases/tag/6.5.3-win7)
- [Qt 6.5.1](https://github.com/crystalidea/qt6windows7/releases/tag/6.5.1-win7)

### License

The repository shares Qt Community Edition terms which imply [Open-Source terms and conditions (GPL and LGPL)](https://www.qt.io/licensing/open-source-lgpl-obligations?hsLang=en).

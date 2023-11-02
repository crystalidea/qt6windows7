This is Qt 6.5.3 backport to able to run on Windows 7. It's qtbase with all source modifications already applied. 
Approach is based on this [forum thread](https://forum.qt.io/topic/133002/qt-creator-6-0-1-and-qt-6-2-2-running-on-windows-7/60) but better: it has fallbacks to default Qt 6 behaviour when running on newer Windows.

You basically need to compile Qt yourself (we use the [compile_win](https://github.com/crystalidea/qt-build-tools/tree/master/6.5.3) script).

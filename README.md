This is Qt 6.6.0 backport that runs Windows 7 (can you believe that?). Here's qtbase with all source modifications already applied. 
Approach is based on this [forum thread](https://forum.qt.io/topic/133002/qt-creator-6-0-1-and-qt-6-2-2-running-on-windows-7/60) but better: many improvements amongst important fallbacks to default Qt 6 behaviour when running on newer Windows.

You basically need to compile Qt yourself (we use the [compile_win](https://github.com/crystalidea/qt-build-tools/tree/master/6.6.0) script).

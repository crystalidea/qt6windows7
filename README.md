This is Qt 6.6.1 backport that runs on Windows 7 (what?). The repository is the qtbase module source code with all required modifications already applied. 
Approach is based on this [forum thread](https://forum.qt.io/topic/133002/qt-creator-6-0-1-and-qt-6-2-2-running-on-windows-7/60) but better: many improvements amongst important fallbacks to default Qt 6 behaviour when running on newer Windows.

You basically need to compile Qt yourself (we use the [compile_win](https://github.com/crystalidea/qt-build-tools/tree/master/6.6.0) script).

The only issue noticed is that scalled UI is somewhat too big for 125% scalling option set in Windows 7. Something should be tweaked with dpi settings probably.

Qt 6.6 designer running on Windows 7:

![image](https://github.com/crystalidea/qt6windows7/assets/2600624/4c5ad13f-db6e-4684-8184-9615e4e55461)


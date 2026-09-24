// Ported from: Authored — shared Qt test fixture for DisplayTestApp tests
// Provides the qtApp() singleton initializer and ensureAppReady() helper shared across test
// translation units. Both are defined ONCE in CommandTest.cpp; other TUs include this header
// for the declarations.
#pragma once

#include <QApplication>

struct QtApp {
    QApplication app;
    QtApp();
};

// Returns the singleton QApplication fixture. Defined in CommandTest.cpp.
QtApp& qtApp();

// Per-test helper: ensures QApplication + App::Application singletons exist. MainWindow ctor
// reads App::GetApplication().GetUserParameter() for the status-bar/dock parameter group
// (FreeCAD MainWindow.cpp:84). Defined in CommandTest.cpp.
// Ported from: Authored — Qt test fixture pattern (mirrors the per-TU static helpers that
//              used to live in GuiApplicationTest.cpp / CommandWiringTest.cpp / MDIChromeTest.cpp).
void ensureAppReady();

namespace Gui {
class MainWindow;
}

// RAII：把栈上 MainWindow 注入 Gui::Application 单例，析构时恢复 nullptr。
// 单例持有悬空主窗口指针会让后续测试 UB（Windows 实测 SEH/段错误非确定性
// 爆发；mac 被堆布局侥幸掩盖）。实现在 CommandTest.cpp（单份）。
// Ported from: Authored — Qt test fixture pattern.
struct MainWindowGuard {
    explicit MainWindowGuard(Gui::MainWindow* mw);
    ~MainWindowGuard();
};

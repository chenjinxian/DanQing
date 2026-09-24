// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test harness exists in itwinjs-core/imodel-native for a
//           shared QApplication fixture; DanQing test-only helper (tests/ only).
//
// QtApp — shared QApplication fixture for dqApp tests.
//
// Viewport derives from QWidget, so any test constructing one needs a live
// QApplication. FrustumApiTest.cpp / ViewToolTest.cpp register gtest global
// environments that create one at program start (before any TEST body runs);
// qtApp() therefore normally just references the existing instance via qApp.
// It only constructs a QApplication itself when none exists yet (e.g. a future
// test binary without those envs), so it never creates a second application
// object.
#pragma once

#include <QApplication>

class QtApp {
public:
    QtApp() {
        if (qApp == nullptr) {
            // Leaked intentionally: the QApplication must outlive every QWidget
            // (including test Viewports) for the process lifetime.
            (void)new QApplication(s_argc, s_argv);
        }
    }
    QtApp(QtApp const&) = delete;
    QtApp& operator=(QtApp const&) = delete;

    QApplication* app() const { return qApp; }

private:
    inline static int s_argc = 1;
    inline static char s_arg0[] = "dqAppTest";
    inline static char* s_argv[] = {s_arg0, nullptr};
};

// Returns the process-wide QtApp fixture (function-local static, created once).
inline QtApp& qtApp()
{
    static QtApp s_instance;
    return s_instance;
}

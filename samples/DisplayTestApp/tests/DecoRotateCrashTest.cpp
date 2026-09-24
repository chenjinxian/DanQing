// DecoRotateCrashTest — Deco 示例加载后点 Rotate 按钮的崩溃复现（2026-09-17 用户
// 报告：Decoration Geometry Example 加载 + View Tools Rotate → SEGV）。
// Authored: no reference test exists（渲染/工具链交互回归——场景 = DTA 用户
// 操作序：Deco 装装饰 → Rotate 工具安装（changeViewport → handle 实例化 +
// focusIn → setCursor(rotate) → invalidateDecorations → 重绘经 GLCanvasContext
// flush 装饰））。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"
#include "DecorationGeometryExample.h"

#include <dqApp/Viewport.h>
#include <dqApp/ViewTool.h>
#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>

#include <vector>

// SEH 崩溃符号化探针（DANQING_CRASH_STACK 门控——与 PickHiliteSelectionTest.cpp:59-93
// 同款：gtest 的 "Stack trace:" 为空，dbghelp 自己抓帧符号化）。
// Authored: forensic affordance，无参考等价物。
#include <windows.h>
#include <dbghelp.h>
static LONG WINAPI rcCrashPrinter(EXCEPTION_POINTERS* ep)
{
    static HANDLE s_process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
    SymInitialize(s_process, nullptr, TRUE);
    fprintf(stderr, "[CRASH] code=0x%08x addr=%p\n",
            ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
    void* frames[32] = {};
    WORD const n = CaptureStackBackTrace(0, 32, frames, nullptr);
    for (WORD i = 0; i < n; ++i) {
        char buf[sizeof(SYMBOL_INFO) + 256] = {};
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(buf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        DWORD64 disp = 0;
        if (SymFromAddr(s_process, reinterpret_cast<DWORD64>(frames[i]), &disp, sym))
            fprintf(stderr, "[CRASH] #%u %s+0x%llx\n", i, sym->Name,
                    static_cast<unsigned long long>(disp));
        else
            fprintf(stderr, "[CRASH] #%u %p\n", i, frames[i]);
    }
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
static bool const s_rcCrashHook = [] {
    if (getenv("DANQING_CRASH_STACK"))
        SetUnhandledExceptionFilter(rcCrashPrinter);
    return true;
}();

namespace { struct QtEnvRC { QtEnvRC() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvRC s_qtRC;

namespace {
void spinRC(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}
}  // namespace

namespace {
// SEH-translated spin helper — __try/__except requires a no-unwind scope, so the
// SEH block lives in its own function (DecoRotateCrashTest.cpp's test body holds
// Qt objects whose dtors must not be bypassed by __except).
void spinWithSehProbe(int ms)
{
    __try {
        qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
        while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    } __except (rcCrashPrinter(GetExceptionInformation()), EXCEPTION_CONTINUE_SEARCH) {
        fprintf(stderr, "[RC] SEH during frame loop\n");
    }
}
}  // namespace

TEST(DecoRotateCrash, DecoThenRotateDoesNotCrash)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinRC(400);

    // Deco 示例（Surface.ts:155-165 入口）。env 门控：不装时验证"仅 Rotate"
    // 是否也崩（二分 Deco×Rotate 的组合）。
    if (!getenv("DANQING_RC_NODECO")) {
        Gui::openDecorationGeometryExample(view);
        spinRC(2500);
        fprintf(stderr, "[RC] deco installed\n");
    }

    // Rotate 工具安装（Viewer.ts:350-353 的 DtaToolBars 按钮路径——
    // runViewTool(new RotateViewTool(vp, false)) 等价物）。
    auto* vp = view.getUeViewport();
    ASSERT_NE(vp, nullptr);
    fprintf(stderr, "[RC] pre-rotate\n");
    auto* tool = new dqApp::RotateViewTool(vp, /*oneShot=*/false);
    fprintf(stderr, "[RC] tool created\n");
    ASSERT_TRUE(tool->run());
    fprintf(stderr, "[RC] tool running\n");

    // 让渲染循环跑若干帧（Rotate 工具的装饰 + 光标切换应不崩）。
    // DANQING_CRASH_STACK 下改用 SEH 翻译——把 gtest 的"Stack trace: 空"换成
    // dbghelp 符号化帧（catch 块内栈未解开，CaptureStackBackTrace 仍有效）。
    bool const crashStack = getenv("DANQING_CRASH_STACK") != nullptr;
    if (crashStack)
        spinWithSehProbe(800);
    else
        spinRC(800);
    fprintf(stderr, "[RC] frames done\n");

    // 清理：工具退出 + 视口关闭（closeEvent 的 decorator 清理链在
    // DecorationGeometryExample.cpp 的析构注释里）。
    dqApp::Application::Get().GetToolAdmin().exitViewTool();
    delete tool;
    view.close();
    spinRC(200);
    fprintf(stderr, "[RC] teardown done\n");
}

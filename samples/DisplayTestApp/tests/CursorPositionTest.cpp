// CursorPositionTest — locate 光圈位置 = 鼠标 client 坐标（2026-09-17 光圈不
// 跟手/位置错误取证）。合成已知 client 坐标的 mouseMove → ToolAdmin 事件链 →
// 读回 decorations 的 canvasDecorations[0].position——须等于合成坐标（CSS px，
// 无 DPR 二次缩放）。
// Authored: no reference test exists（坐标系回归——Qt client px ↔ decoration
// position 的 DPR 链，参考浏览器 canvas 是 CSS px 恒等）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QMouseEvent>

#include "View3DInventor.h"

#include <dqApp/Viewport.h>
#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>

#include <vector>

namespace { struct QtEnvCP { QtEnvCP() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvCP s_qtCP;

namespace {
void spinCP(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}
}  // namespace

TEST(CursorPosition, LocateCircleFollowsMousePosition)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinCP(400);

    auto* vp = view.getUeViewport();
    ASSERT_NE(vp, nullptr);

    // 选择工具安装（PickHiliteSelectionTest.cpp:205-211 模式——OnInitialized
    // 注册 + 注册表构造）。这是 locate 光圈唯一的状态源（initLocateElements →
    // setLocateCircleOn）。测试 harness 不经 main() 的 Application::Startup——
    // ToolAdmin 装饰器注册在 Startup 内（Application.cpp:72-73），此处补装。
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();   // registers "Select" (idempotent)
    dqApp::Application::Get().GetViewManager().AddDecorator(&admin);
    auto* selTool = admin.GetRegistry().Create("Select");
    ASSERT_NE(selTool, nullptr);
    selTool->onPostInstall();   // initLocateElements → locateCircleOn=true

    // 合成 MouseMove 到已知 client 坐标（PickHilite HoverMotion 同款路径）。
    QPoint const target(400, 300);
    QMouseEvent move(QEvent::MouseMove, target, vp->mapToGlobal(target),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(vp, &move);

    // 事件队列由 Application::EventLoop 驱动（Application.cpp:197-206：
    // processEvent → RenderLoop），经 16ms QTimer 泵——spin 必须覆盖至少一个
    // timer 周期（16ms），且渲染帧在失效后跑（isRedrawNeeded 消费）。
    spinCP(500);
    vp->RenderFrame();

    auto& ta = dqApp::Application::Get().GetToolAdmin();
    printf("[CURSOR-TEST] isLocateCircleOn=%d cursorView=%p vp=%p\n",
           ta.isLocateCircleOn() ? 1 : 0, (void*)ta.cursorView(), (void*)vp);
    printf("[CURSOR-TEST] decorators=%zu\n",
           dqApp::Application::Get().GetViewManager().GetDecorators().size());

    // 装饰重建由 InvalidateDecorations 驱动（motion → invalidate → RenderFrame
    // 的 CollectDecorations）。spin 后手动帧，确保失效已消费。
    vp->InvalidateDecorations();
    vp->RenderFrame();

    // 读回 decorations 的 canvasDecorations——locate 光圈的 position 须等于
    // 合成坐标（400,300）。DPR 双乘的检出：若 position ≈ (400*dpr, 300*dpr)
    // 即 flush 侧重复缩放。
    auto const& decs = vp->GetDecorations().canvasDecorations;
    ASSERT_FALSE(decs.empty()) << "locate circle not collected — the motion → "
                                  "InvalidateDecorations → decorate chain is broken";
    ASSERT_TRUE(decs[0].position.has_value());
    printf("[CURSOR-TEST] target=(400,300) decoration.position=(%.1f,%.1f) dpr=%.2f\n",
           decs[0].position->x, decs[0].position->y,
           static_cast<double>(vp->devicePixelRatioF()));

    EXPECT_NEAR(decs[0].position->x, 400.0, 2.0)
        << "locate circle x = " << decs[0].position->x << " (expected 400 client px)";
    EXPECT_NEAR(decs[0].position->y, 300.0, 2.0)
        << "locate circle y = " << decs[0].position->y << " (expected 300 client px)";

    // 清理（PickHiliteSelectionTest.cpp:272-273 模式）。
    dqApp::Application::Get().GetToolAdmin().setPrimitiveTool(nullptr);
    delete selTool;
    view.close();
    spinCP(200);
}

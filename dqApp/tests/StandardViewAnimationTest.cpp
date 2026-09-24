// Authored: no reference test exists in itwinjs-core for real-window frustum-change
//           animation (FrustumAnimator has no NonPublished test; StandardViewTool
//           tests assert tool lifecycle only)。真窗口回归：标准视图切换经
//           StandardViewTool → synchWithView({animateFrustumChange:true})
//           （ViewTool.ts:3521）安装 FrustumAnimator 并在 time.normal（1.0s，
//           Viewport.ts:3129）内从起始姿态插值到目标姿态（FrustumAnimator.ts:127-134）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QThread>
#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/StandardView.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/ViewTool.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <cstdio>
namespace { struct QtEnv3 { QtEnv3() { if (!qApp) { static int argc=1; static char n[]="t"; static char* av[]={n,nullptr}; new QApplication(argc,av);} } }; }
static QtEnv3 s_qt3;

TEST(StandardViewAnimationTest, RotationSwitchAnimatesAndCompletes)
{
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(-1000,-1000,-100, 1000,1000,100);
    auto conn = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
    auto* vp = dqApp::Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);
    vp->resize(300, 200);
    vp->move(0, 0);
    vp->show();
    {
        auto& app = dqApp::Application::Get();
        if (!app.isInitialized()) {
            dqApp::Application::Options opts;
            opts.applicationId = "StandardViewAnimationTest";
            opts.applicationVersion = "1.0";
            opts.noRender = true;
            ASSERT_TRUE(app.Startup(opts));
        }
        app.GetViewManager().AddViewport(vp);
    }
    // 首帧若干——_lastPose 基线建立（ValidateRenderPlan :3602）。
    for (int i = 0; i < 20; ++i) { QCoreApplication::processEvents(); QThread::msleep(20); }

    auto* v3 = vp->GetView()->AsViewState3d();
    ASSERT_NE(v3, nullptr);
    auto const beginRot = v3->getRotation();
    auto const frontRot = dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::Front);
    ASSERT_FALSE(beginRot.IsAlmostEqual(frontRot));

    // DTA 切换路径（DtaToolBars.cpp:203 runViewTool 同款）：tool->run() 安装并
    // onPostInstall（ViewTool.ts:3503-3525）。
    auto* tool = new dqApp::StandardViewTool(vp, dqApp::StandardViewId::Front);
    bool const installed = tool->run();
    if (!installed) delete tool;
    ASSERT_TRUE(installed);

    // :3521 — animateFrustumChange 必须安装 FrustumAnimator。
    EXPECT_NE(vp->getAnimator(), nullptr)
        << "synchWithView({animateFrustumChange:true}) must install a FrustumAnimator";

    // 动画中段（~300ms，time.normal=1.0s 的早段）：姿态既非起点也非终点（插值中）。
    for (int i = 0; i < 15; ++i) { QCoreApplication::processEvents(); QThread::msleep(20); }
    auto const midRot = v3->getRotation();
    EXPECT_FALSE(midRot.IsAlmostEqual(frontRot))
        << "mid-animation rotation must still be interpolating (not yet Front)";
    EXPECT_FALSE(midRot.IsAlmostEqual(beginRot))
        << "mid-animation rotation must have left the begin pose";

    // 完成（~1.4s 总耗时 > time.normal 1.0s）：动画器清空，旋转 == Front。
    for (int i = 0; i < 60; ++i) { QCoreApplication::processEvents(); QThread::msleep(20); }
    EXPECT_EQ(vp->getAnimator(), nullptr) << "animator must be removed on completion";
    EXPECT_TRUE(v3->getRotation().IsAlmostEqual(frontRot))
        << "final rotation must equal the target standard rotation";

    dqApp::Application::Get().GetViewManager().DropViewport(vp);
    delete vp;
}

// 不勾动画选项（默认 ViewChangeOptions）时，切换不得安装动画器（synchWithView
// :3595 `true === options.animateFrustumChange`）。
TEST(StandardViewAnimationTest, NoAnimatorWithoutOption)
{
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(-1000,-1000,-100, 1000,1000,100);
    auto conn = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
    auto* vp = dqApp::Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);
    vp->resize(300, 200);
    vp->show();
    {
        auto& app = dqApp::Application::Get();
        if (!app.isInitialized()) {
            dqApp::Application::Options opts;
            opts.applicationId = "StandardViewAnimationTest";
            opts.applicationVersion = "1.0";
            opts.noRender = true;
            ASSERT_TRUE(app.Startup(opts));
        }
        app.GetViewManager().AddViewport(vp);
    }
    for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); QThread::msleep(20); }

    // 默认选项（animateFrustumChange 未设置）→ 不安装动画器。
    vp->synchWithView(dqApp::ViewChangeOptions{});
    EXPECT_EQ(vp->getAnimator(), nullptr);

    dqApp::Application::Get().GetViewManager().DropViewport(vp);
    delete vp;
}

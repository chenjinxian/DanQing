// NavigateTest — M-O(3) P1 Walk/Fly/LookAndMove 引擎锁（NavigateMotion 数学
// + ViewNavigate 模态分流 + ViewLookAndMove 键盘累积 + 相机归位 + 注册面）。
//
// 锚定（真实读过的参考行号）：
//   - NavigateMotion :1747-1921（takeElevator :1765 / modifyPitchAngle
//     :1770 / pan :1898 / travel :1903 / resetToLevel :1911）；
//   - ViewNavigate :1924-2003（getNavigateMode :1932-1936 修饰键模态 / animate
//     :1939-1954 frustum × transform / onReinitialize :1956-1984 walk 相机
//     归位）；
//   - ViewWalk :2951-2990 / ViewFly :2992-3032（getNavigateMotion 模态合成）；
//   - ViewLookAndMove :2006-2645（onKeyTransition :2587-2645 累加 -1..1 /
//     changeWalkVelocity :2528 / getMaxLinearVelocity :2156）；
//   - LookAndMoveTool/WalkViewTool/FlyViewTool :3107-3199（toolId + 提示键）。
//
// RED（M-O(3) P1 落地前）：NavigateMotion/ViewNavigate/三 handle/三工具类型
// 不存在——编译期缺 API 红（3h 同款先例）。
//
// Authored: no reference test exists in itwinjs-core for NavigateMotion/
//           ViewNavigate（walk/fly 无 JS 单测；行为锚定如上）。
#include <gtest/gtest.h>

#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ToolSettings.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqApp/ViewTool.h>

#include <dqGeom/Angle.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <cmath>

namespace {

// LookToolTest 同款夹具（本 TU 局部——避免跨 TU harness）。
struct NavigateAndViewport {
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    dqApp::Viewport* vp = nullptr;
};

NavigateAndViewport navigateBuildViewport()
{
    NavigateAndViewport r;
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(
        dqGeom::Point3d::From(-100.0, -100.0, -100.0),
        dqGeom::Point3d::From(100.0, 100.0, 100.0));
    r.imodel = dqApp::BlankConnection::create(props);
    r.view = dqApp::SpatialViewState::CreateBlank(
        r.imodel.Get(), dqGeom::Point3d::From(0.0, 0.0, 0.0),
        dqGeom::Vector3d::From(200.0, 200.0, 200.0));
    r.vp = dqApp::Viewport::Create(nullptr, r.view);
    r.view->SetExtents(dqGeom::Vector3d::From(
        200.0, 200.0 * r.vp->height() / r.vp->width(), 200.0));
    r.vp->setupViewFromFrustum(r.vp->getFrustum(true));
    return r;
}

}  // namespace

// ---------------------------------------------------------------------------
// NavigateMotion 数学锁（无状态面）。
// ---------------------------------------------------------------------------
TEST(NavigateMotionMath, InitResetsToIdentity)
{
    dqApp::NavigateMotion motion(nullptr);
    motion.init(0.5);
    dqGeom::Transform const& t = motion.transform();
    // 恒等：对角 1、平移 0。
    EXPECT_NEAR(t.matrix.coffs[0], 1.0, 1e-12);
    EXPECT_NEAR(t.matrix.coffs[4], 1.0, 1e-12);
    EXPECT_NEAR(t.matrix.coffs[8], 1.0, 1e-12);
    EXPECT_NEAR(t.origin.x, 0.0, 1e-12);
    EXPECT_NEAR(t.origin.y, 0.0, 1e-12);
    EXPECT_NEAR(t.origin.z, 0.0, 1e-12);
}

// takeElevator（:1765-1768 — height × seconds 的 z 平移）。
TEST(NavigateMotionMath, TakeElevatorTranslatesZByHeightTimesSeconds)
{
    dqApp::NavigateMotion motion(nullptr);
    motion.init(2.0);  // seconds
    motion.takeElevator(1.5);
    dqGeom::Transform const& t = motion.transform();
    EXPECT_NEAR(t.origin.x, 0.0, 1e-12);
    EXPECT_NEAR(t.origin.y, 0.0, 1e-12);
    EXPECT_NEAR(t.origin.z, 3.0, 1e-12);  // 1.5 * 2.0
}

// modifyPitchAngleToPreventInversion（:1770-1796 — 0 直通；限位数学需要视图
// 姿态[getViewUp/getViewDirection]，空 viewport 走默认 up(0,1,0)/dir(0,0,-1)
// 分支——85° 限位 + 反向 0 归位）。
TEST(NavigateMotionMath, ModifyPitchAngleZeroPassthrough)
{
    dqApp::NavigateMotion motion(nullptr);
    EXPECT_EQ(0.0, motion.modifyPitchAngleToPreventInversion(0.0));
}

// ---------------------------------------------------------------------------
// ViewWalk/ViewFly/ViewLookAndMove 模态合成 + 键盘累积（真视口夹具）。
// ---------------------------------------------------------------------------
TEST(NavigateWalkFly, ModesAndToolRegistration)
{
    auto r = navigateBuildViewport();
    ASSERT_NE(r.vp, nullptr);

    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();

    // 注册面（ToolAdmin OnInitialized 的 RegisterView）。
    EXPECT_NE(admin.GetRegistry().FindView("View.Walk"), nullptr);
    EXPECT_NE(admin.GetRegistry().FindView("View.Fly"), nullptr);
    EXPECT_NE(admin.GetRegistry().FindView("View.LookAndMove"), nullptr);

    // 工具 ctor + toolId + handleMask（Walk|Pan / Fly|Pan /
    // LookAndMove|Pan——:3112/:3166/:3187）。
    dqApp::WalkViewTool walk(r.vp);
    EXPECT_STREQ("View.Walk", walk.getToolId());
    dqApp::FlyViewTool fly(r.vp);
    EXPECT_STREQ("View.Fly", fly.getToolId());
    dqApp::LookAndMoveTool lam(r.vp);
    EXPECT_STREQ("View.LookAndMove", lam.getToolId());

    // changeViewport 装载 handle（Walk 位 → ViewWalk handle 在表）。
    walk.changeViewport(r.vp);
    EXPECT_TRUE(walk.viewHandles.hasHandle(dqApp::ViewHandleType::Walk));
    EXPECT_TRUE(walk.viewHandles.hasHandle(dqApp::ViewHandleType::Pan));
    fly.changeViewport(r.vp);
    EXPECT_TRUE(fly.viewHandles.hasHandle(dqApp::ViewHandleType::Fly));
    lam.changeViewport(r.vp);
    EXPECT_TRUE(lam.viewHandles.hasHandle(dqApp::ViewHandleType::LookAndMove));

    delete r.vp;
}

// ViewLookAndMove 键盘累积（:2587-2645 —— W/S → z ±1、A/D → x ±1、Q/E →
// y ±1，非导航键 false；越界 clamp ±1）。
TEST(NavigateWalkFly, LookAndMoveKeyAccumulation)
{
    auto r = navigateBuildViewport();
    ASSERT_NE(r.vp, nullptr);
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();

    dqApp::LookAndMoveTool lam(r.vp);
    lam.changeViewport(r.vp);
    // RTTI 禁用（§9）——经 handleType 判别取 LookAndMove handle。
    dqApp::ViewLookAndMove* handle = nullptr;
    for (int i = 0; i < lam.viewHandles.count() && handle == nullptr; ++i) {
        dqApp::ViewingToolHandle* h = lam.viewHandles.getByIndex(i);
        if (h->handleType() == dqApp::ViewHandleType::LookAndMove)
            handle = static_cast<dqApp::ViewLookAndMove*>(h);
    }
    ASSERT_NE(handle, nullptr);

    // 动态更新外的导航键会经 enableKeyStart（视线中心起拖）——参考 onKeyTransition
    // :2588-2591。真实路径成立时 W 按下 → z=+1；松开 → z=0。
    // （enableKeyStart 依赖 processFirstPoint——真视口面；此处经 onKeyTransition
    // 直接驱动。）
    bool const handledDown = handle->onKeyTransition(true, 0x57 /*W*/);
    EXPECT_TRUE(handledDown);
    EXPECT_NEAR(handle->positionInput().z, 1.0, 1e-12);
    bool const handledUp = handle->onKeyTransition(false, 0x57 /*W*/);
    EXPECT_TRUE(handledUp);
    EXPECT_NEAR(handle->positionInput().z, 0.0, 1e-12);

    // 非导航/非功能键（X）→ false。
    EXPECT_FALSE(handle->onKeyTransition(true, 0x58 /*X*/));

    delete r.vp;
}

// changeWalkVelocity 倍率面（:2156-2165 —— walkVelocityChange ±挡 → 倍率；
// 0 挡=1×）。
TEST(NavigateWalkFly, LookAndMoveVelocityChangeMultiplier)
{
    auto r = navigateBuildViewport();
    dqApp::LookAndMoveTool lam(r.vp);
    lam.changeViewport(r.vp);
    // RTTI 禁用（§9）——经 handleType 判别。
    dqApp::ViewLookAndMove* handle = nullptr;
    for (int i = 0; i < lam.viewHandles.count() && handle == nullptr; ++i) {
        dqApp::ViewingToolHandle* h = lam.viewHandles.getByIndex(i);
        if (h->handleType() == dqApp::ViewHandleType::LookAndMove)
            handle = static_cast<dqApp::ViewLookAndMove*>(h);
    }
    ASSERT_NE(handle, nullptr);

    // 缺省 0 挡 → walkVelocity 原值（ToolSettings.walkVelocity=3.5）。
    EXPECT_NEAR(handle->getMaxLinearVelocity(), dqApp::ToolSettings::walkVelocity,
                1e-12);

    // 非动态更新下 "+"/"=" 在导航键门早退（:2588-2591——isNavigationKey 门），
    // 变速键只在动态更新中生效（W 起拖进入动态 → "+"×2 → change=2 →
    // speedFactor=3 → 3×；"=" 重置 → 1×）。
    EXPECT_FALSE(handle->onKeyTransition(true, 0x2B /*+*/));
    EXPECT_TRUE(handle->onKeyTransition(true, 0x57 /*W——进入动态*/));
    EXPECT_TRUE(handle->onKeyTransition(true, 0x2B /*+*/));
    EXPECT_TRUE(handle->onKeyTransition(true, 0x2B /*+*/));
    EXPECT_NEAR(handle->getMaxLinearVelocity(),
                dqApp::ToolSettings::walkVelocity * 3.0, 1e-12);
    EXPECT_TRUE(handle->onKeyTransition(true, 0x3D /*=*/));
    EXPECT_NEAR(handle->getMaxLinearVelocity(),
                dqApp::ToolSettings::walkVelocity, 1e-12);

    dqApp::ToolSettings::walkVelocityChange = 0;  // 复位全局静态
    delete r.vp;
}

// TurnCameraOn 相机开启（:1988-2029 —— cameraOff→相机开 + 镜头角=入参）。
TEST(NavigateWalkFly, TurnCameraOnFromOrtho)
{
    auto r = navigateBuildViewport();
    ASSERT_NE(r.vp, nullptr);
    EXPECT_FALSE(r.vp->isCameraOn());

    dqGeom::Angle const walkLens = dqApp::ToolSettings::walkCameraAngle;
    dqApp::ViewStatus const status = r.vp->TurnCameraOn(walkLens);
    EXPECT_EQ(dqApp::ViewStatus::Success, status);
    EXPECT_TRUE(r.vp->isCameraOn());
    // 镜头角 = walkCameraAngle（75.6°）。
    EXPECT_NEAR(r.view->GetLensAngle(), walkLens.Radians(), 1e-9);

    delete r.vp;
}

// setCameraLensAngle（:860-878 —— retainEyePoint && cameraOn → lookAt 保眼）。
TEST(NavigateWalkFly, SetCameraLensAngleRetainsEyeWhenCameraOn)
{
    auto r = navigateBuildViewport();
    ASSERT_NE(r.vp, nullptr);
    ASSERT_EQ(dqApp::ViewStatus::Success, r.vp->TurnCameraOn(
                  dqApp::ToolSettings::walkCameraAngle));
    dqGeom::Point3d const eyeBefore =
        r.view->AsViewState3d()->getEyePoint();

    dqApp::RotateViewTool rot(r.vp);  // 任一 ViewManip 子类承载静态面。
    dqGeom::Angle const newLens = dqGeom::Angle::FromDegrees(60.0);
    EXPECT_EQ(dqApp::ViewStatus::Success,
              rot.setCameraLensAngle(newLens, true));
    EXPECT_NEAR(r.view->AsViewState3d()->getEyePoint().x, eyeBefore.x, 1e-6);
    EXPECT_NEAR(r.view->AsViewState3d()->getEyePoint().y, eyeBefore.y, 1e-6);
    EXPECT_NEAR(r.view->AsViewState3d()->getEyePoint().z, eyeBefore.z, 1e-6);
    EXPECT_NEAR(r.view->GetLensAngle(), newLens.Radians(), 1e-9);

    delete r.vp;
}

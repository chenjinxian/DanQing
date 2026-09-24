// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — LookToolTest — ViewLook handle + LookViewTool + IdleTool Ctrl+Middle 映射
// Authored: no reference test exists in itwinjs-core for ViewLook/LookViewTool;
//           scenarios from ViewTool.ts:1323-1408 (ViewLook) + :3063-3071 (LookViewTool)
//           + IdleTool.ts:44-56 (IdleTool Ctrl+Middle branch).
#include <gtest/gtest.h>

#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqApp/ViewTool.h>
#include <dqApp/ViewingSpace.h>

#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>

#include <QApplication>

using namespace dqApp;
using namespace dqGeom;

namespace { struct QtEnv { QtEnv() { if (!qApp) { static int argc=1; static char n[]="t"; static char* av[]={n,nullptr}; new QApplication(argc,av);} } }; }
static QtEnv s_qt;

namespace {

// Build a SpatialViewState + Viewport with an initialized ViewingSpace, mirroring
// ViewToolTest.cpp's buildViewWithValidViewingSpace / EventDispatchTest.cpp's
// idleToolBuildViewport (kept local to this TU to avoid reaching into those TUs'
// harnesses).
// Authored: harness helper — no reference test exists for the Viewport wrapper
//           init pattern in isolation.
struct LookViewAndViewport {
    dqBase::RefPtr<BlankConnection> imodel;
    dqBase::RefPtr<SpatialViewState> view;
    Viewport* vp = nullptr;
    float w = 0.0f;
    float h = 0.0f;
};
LookViewAndViewport lookBuildViewport(dqGeom::Point3d const& origin,
                                      double xExtents = 200.0,
                                      double zExtents = 200.0)
{
    LookViewAndViewport r;
    // A real BlankConnection provides projectExtents so SpatialViewState.computeBaseExtents
    // — which reads iModel.projectExtents unconditionally like the reference — has a valid
    // iModel (the reference's createBlankConnection always supplies one).
    BlankConnectionProps props;
    props.extents = dqGeom::Range3d(
        dqGeom::Point3d::From(origin.x - xExtents * 0.5, origin.y - xExtents * 0.5, origin.z - zExtents * 0.5),
        dqGeom::Point3d::From(origin.x + xExtents * 0.5, origin.y + xExtents * 0.5, origin.z + zExtents * 0.5));
    r.imodel = BlankConnection::create(props);
    r.view = SpatialViewState::CreateBlank(r.imodel.Get(), origin,
        dqGeom::Vector3d::From(xExtents, xExtents, zExtents));
    r.vp = Viewport::Create(nullptr, r.view);
    r.w = static_cast<float>(r.vp->width());
    r.h = static_cast<float>(r.vp->height());
    // Match extents.y to the widget aspect ratio so FixAspectRatio is a no-op.
    r.view->SetExtents(dqGeom::Vector3d::From(xExtents, xExtents * r.h / r.w, zExtents));
    // Initialize ViewingSpace (rebuilds m_worldToNpc / m_npcToWorld matrices).
    r.vp->setupViewFromFrustum(r.vp->getFrustum(true));
    return r;
}

}  // namespace

// GoogleTest fixture: drives the singleton ToolAdmin through IdleTool (same
// harness pattern as EventDispatchTest.cpp's IdleToolDispatch — IdleTool routes
// through IModelApp.toolAdmin, so the singleton registry must be populated and
// the viewTool slot cleaned around each test).
class LookTool : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto& admin = Application::Get().GetToolAdmin();
        admin.OnInitialized();
        admin.installViewTool(nullptr);
        m_v = lookBuildViewport(dqGeom::Point3d::From(0.0, 0.0, 0.0));
        ASSERT_NE(m_v.vp, nullptr);
    }
    void TearDown() override
    {
        auto& admin = Application::Get().GetToolAdmin();
        admin.installViewTool(nullptr);
        delete m_v.vp;
    }

    LookViewAndViewport m_v;
};

// Authored: IdleTool Ctrl+中键在 3d 视图上安装 View.Look（IdleTool.ts:47-48 —
//           allow3dManipulations()=true（3d 视图）→ "View.Look"）。驱动方式与
//           EventDispatchTest 的 CtrlMiddle 用例一致：构造真实 viewport +
//           BeButtonEvent（Middle + Control），调 IdleTool::onMouseStartDrag。
TEST_F(LookTool, CtrlMiddleInstallsLookOn3dView)
{
    auto& admin = Application::Get().GetToolAdmin();
    InteractiveTool* idle = admin.GetIdleTool();
    ASSERT_NE(idle, nullptr);

    BeButtonEvent ev;
    ev.button = BeButton::Middle;
    ev.keyModifiers = BeModifierKeys::Control;
    ev.viewport = m_v.vp;

    EXPECT_EQ(idle->onMouseStartDrag(ev), EventHandled::Yes);

    ViewTool* vt = admin.GetViewTool();
    ASSERT_NE(vt, nullptr);
    // IdleTool.ts:47: allow3dManipulations() ? "View.Look" : "View.Scroll" — the
    // blank SpatialViewState is 3d, so Look must be installed.
    EXPECT_STREQ(vt->getToolId(), "View.Look");
}

// Authored: 注册表可建 "View.Look"（ToolAdmin.h 注册表经 OnInitialized 填充；
//           LookViewTool 注册对齐 IModelApp.startup viewTool 注册净效果
//           IModelApp.ts:438-446 registerModule(viewTool)）。
TEST(LookToolRegistry, RegisteredInToolAdmin)
{
    auto& ta = Application::Get().GetToolAdmin();
    ta.OnInitialized();

    auto v = lookBuildViewport(dqGeom::Point3d::From(0.0, 0.0, 0.0));
    ASSERT_NE(v.vp, nullptr);

    InteractiveTool* tool = ta.GetRegistry().CreateVP(
        "View.Look", v.vp, /*oneShot=*/false, /*isDraggingRequired=*/false);
    EXPECT_NE(tool, nullptr);
    if (tool)
        EXPECT_STREQ(tool->getToolId(), "View.Look");

    delete tool;
    delete v.vp;
}

// Authored: no reference test exists in itwinjs-core for the Look/Pan hit-priority
//           competition; scenario from ViewTool.ts:1140（ViewPan.testHandleForHit
//           priority = Low）+ :1334（ViewLook.testHandleForHit priority = Medium —
//           "Always prefer over pan handle which is only force enabled by IdleTool
//           middle button action..."）+ :211-245（ViewHandleArray.testHit 的
//           priority-distance 裁决）+ IdleTool.ts:83（Ctrl+Middle 经
//           startHandleDrag(ev) 非强制命中——forced = ViewHandleType.None）。
TEST(LookToolPriority, UnforcedHitTestPrefersLookOverPan)
{
    auto v = lookBuildViewport(dqGeom::Point3d::From(0.0, 0.0, 0.0));
    ASSERT_NE(v.vp, nullptr);

    // LookViewTool ctor (ViewTool.ts:3067): handleMask = Look | Pan →
    // changeViewport instantiates ViewLook + ViewPan（两 handle 共存于同一数组）。
    LookViewTool tool(v.vp, /*oneShot=*/false, /*isDraggingRequired=*/false);
    ASSERT_EQ(tool.viewHandles.count(), 2);

    // 非强制 testHit（IdleTool.ts:83 startHandleDrag(ev) 无 handleId 实参 →
    // forced=None，ViewTool.ts:744 调用/:213 默认形参）：ViewLook/ViewPan 的
    // testHandleForHit 均恒 true 且 distance=0，裁决纯靠优先级比较
    // （ViewTool.ts:230-244：data.priority >= highestPriority 才入选）——
    // Look(Medium) 必须胜过 Pan(Low)，与 handle 数组顺序无关。
    EXPECT_TRUE(tool.viewHandles.testHit(
        Point3d::From(0.5 * v.w, 0.5 * v.h, 0.0)));   // 视口中心（屏幕坐标）
    ViewingToolHandle* hit = tool.viewHandles.hitHandle();
    ASSERT_NE(hit, nullptr);
    EXPECT_EQ(hit->handleType(), ViewHandleType::Look);

    delete v.vp;
}

// Authored: Look 拖拽改变取景（ViewLook.doManipulation → getLookTransform →
//           setupViewFromFrustum，ViewTool.ts:1369-1407）——经 handle 直接驱动：
//           firstPoint（视口中心）后 doManipulation（右移半屏 → xAngle=-0.5π），
//           断言取景 rotation 绕眼点旋转变化。
TEST(LookToolHandle, DragRotatesViewAboutEye)
{
    auto v = lookBuildViewport(dqGeom::Point3d::From(0.0, 0.0, 0.0));
    ASSERT_NE(v.vp, nullptr);
    ASSERT_GT(v.w, 0.0f);
    ASSERT_GT(v.h, 0.0f);

    // LookViewTool ctor (ViewTool.ts:3067): handleMask = Look | Pan →
    // changeViewport instantiates ViewLook + ViewPan.
    LookViewTool tool(v.vp, /*oneShot=*/false, /*isDraggingRequired=*/false);
    ViewingToolHandle* look = nullptr;
    for (int i = 0; i < tool.viewHandles.count(); ++i) {
        ViewingToolHandle* h = tool.viewHandles.getByIndex(i);
        if (h && h->handleType() == ViewHandleType::Look)
            look = h;
    }
    ASSERT_NE(look, nullptr);

    // First point at the viewport center (view px). ViewLook.firstPoint uses
    // ev.viewPoint only (ViewTool.ts:1348).
    BeButtonEvent ev;
    ev.viewport = v.vp;
    ev.viewPoint = Point3d::From(0.5 * v.w, 0.5 * v.h, 0.0);
    ev.point = v.vp->NpcToWorld(Point3d::From(0.5, 0.5, 0.5));
    ev.rawPoint = ev.point;
    EXPECT_TRUE(look->firstPoint(ev));

    Matrix3d const rotBefore = v.view->getRotation();
    Point3d const eyeBefore = v.view->getEyePoint();

    // Drag right by half the screen width → xAngle = -(0.5w / w)·π = -0.5π
    // (ViewTool.ts:1389-1391).
    BeButtonEvent move = ev;
    move.viewPoint = Point3d::From(1.0 * v.w, 0.5 * v.h, 0.0);
    EXPECT_TRUE(look->doManipulation(move, /*inDynamics=*/true));

    Matrix3d const rotAfter = v.view->getRotation();
    Point3d const eyeAfter = v.view->getEyePoint();

    // Rotation changed (drag rotated the view)…
    EXPECT_FALSE(rotAfter.IsAlmostEqual(rotBefore, 1.0e-10));
    // …about the eye point: getLookTransform's fixed point is the eye
    // (ViewTool.ts:1405 Transform.createFixedPointAndMatrix(this._eyePoint, …)),
    // so the eye is invariant under the drag.
    EXPECT_NEAR(eyeAfter.x, eyeBefore.x, 1.0e-6);
    EXPECT_NEAR(eyeAfter.y, eyeBefore.y, 1.0e-6);
    EXPECT_NEAR(eyeAfter.z, eyeBefore.z, 1.0e-6);

    delete v.vp;
}

// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — WindowAreaTest — ViewState.adjustViewDelta / hasSameCoordinates / extentLimits
//             + ToolAdmin 装饰器接线（W3：活动 ViewTool 的 decorate 转发）
//             + WindowAreaTool（W4：computeWindowCorners / 正交框选缩放 / CanvasDecoration 通路）
// Authored: no reference test exists in itwinjs-core for adjustViewDelta in isolation
//           (reference exercises it indirectly via lookAt/WindowAreaTool); scenarios from
//           ViewState.ts:889-920 (adjustViewDelta) + :1221-1240 (hasSameCoordinates) +
//           SpatialViewState.ts:117 (defaultExtentLimits).
#include <gtest/gtest.h>
#include <QApplication>
#include <dqApp/Application.h>
#include <dqApp/DecorateContext.h>
#include <dqApp/Decorator.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqApp/ViewStatus.h>
#include <dqApp/ViewTool.h>

#include <dqRender/CanvasDecoration.h>
#include <dqRender/Decorations.h>

#include <dqGeom/Point2d.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

using namespace dqApp;

namespace { struct QtEnv { QtEnv() { if (!qApp) { static int argc=1; static char n[]="t"; static char* av[]={n,nullptr}; new QApplication(argc,av);} } }; }
static QtEnv s_qt;

namespace {
// 记录型假 ViewTool（W3 测试夹具）：覆写 decorate 计数，验证 ToolAdmin::Decorate 转发。
// Authored: test fixture — no reference equivalent; same shape as ToolAdminTest.cpp's
//           RecordingTool（记录调用次数的最小 InteractiveTool 子类），此处子类化 ViewTool
//           因为 decorate 虚函数在 ViewTool（ViewTool.h:416）。
class RecordingViewTool : public ViewTool {
public:
    RecordingViewTool() : ViewTool(nullptr) {}
    void decorate(DecorateContext& /*context*/) override { ++decorateCallCount; }
    int decorateCallCount = 0;
};
}  // namespace

// Authored: clamp 到 extentLimits（SpatialViewState 默认 {0.001, 38226000}，
//           = Constant.oneMillimeter / 3*Constant.diameterOfEarth，
//           SpatialViewState.ts:117 + core-geometry Constant.ts:16/24）。
TEST(AdjustViewDelta, ClampsToExtentLimits)
{
    auto view = SpatialViewState::CreateBlank(nullptr, {0,0,0}, {100,100,100});
    dqGeom::Vector3d delta = dqGeom::Vector3d::From(0.0001, 0.0001, 50.0);  // 小于 min=0.001
    dqGeom::Point3d origin = dqGeom::Point3d::From(0, 0, 0);
    auto const rot = dqGeom::Matrix3d::CreateIdentity();

    // aspect 不传 → 只 clamp
    auto status = view->adjustViewDelta(delta, origin, rot, std::nullopt, nullptr);
    EXPECT_EQ(status, ViewStatus::MinWindow);   // clamp 发生 → MinWindow
    EXPECT_DOUBLE_EQ(delta.x, 0.001);
    EXPECT_DOUBLE_EQ(delta.y, 0.001);
    // origin 半移保持中心（ViewState.ts:916-917）：origin += 0.5 * rot^T·(origDelta - delta)
    // —— 参考 delta.vectorTo(origDelta) = origDelta - delta（Point3dVector3d.ts:360-362
    // "vector from this to other"）；rot=identity。delta 被 clamp 变大 → origin 负向回移。
    EXPECT_NEAR(origin.x, 0.5 * (0.0001 - 0.001), 1e-12);
    EXPECT_NEAR(origin.y, 0.5 * (0.0001 - 0.001), 1e-12);
}

// Authored: 纵横比修正（aspect=2.0：宽:高=2:1 时 delta.x 超宽则 delta.y 扩展，
//           ViewState.ts:908-914）。
TEST(AdjustViewDelta, AspectAdjustsToMatchWindow)
{
    auto view = SpatialViewState::CreateBlank(nullptr, {0,0,0}, {100,100,100});
    dqGeom::Vector3d delta = dqGeom::Vector3d::From(200.0, 50.0, 50.0);  // 200:50 = 4:1 > 2:1
    dqGeom::Point3d origin = dqGeom::Point3d::From(0, 0, 0);
    auto const rot = dqGeom::Matrix3d::CreateIdentity();

    auto status = view->adjustViewDelta(delta, origin, rot, 2.0, nullptr);
    EXPECT_EQ(status, ViewStatus::Success);     // 在限内，只纵横比修正
    EXPECT_NEAR(delta.x, 200.0, 1e-9);          // x 不动（200 > 2*50 → 扩 y）
    EXPECT_NEAR(delta.y, 100.0, 1e-9);          // y = 200/2
    // origin 半移补偿（ViewState.ts:916-917）：0.5 * (origDelta.y - delta.y) = 0.5*(50-100)。
    // y 向上扩 → origin 负向回移保持中心不变。
    EXPECT_NEAR(origin.y, 0.5 * (50.0 - 100.0), 1e-9);
}

// Authored: hasSameCoordinates——同连接（同 iModel）两 spatial 视图为 true
// （ViewState.ts:1221-1240：iModel 不同 → false；双 spatial → true）。
TEST(ViewStateCoordinates, HasSameCoordinates)
{
    auto view1 = SpatialViewState::CreateBlank(nullptr, {0,0,0}, {1,1,1});
    auto view2 = SpatialViewState::CreateBlank(nullptr, {5,5,5}, {2,2,2});
    EXPECT_TRUE(view1->hasSameCoordinates(*view2));   // 同 iModel（均 nullptr）+ 双 spatial
}

// Authored: no reference test exists in itwinjs-core for ToolAdmin.decorate forwarding
//           (ToolAdmin.ts:2049-2059); scenario: 活动 ViewTool 的 decorate 经
//           ToolAdmin（ViewManager 装饰器）被调用。Startup 注册断言对应
//           ViewManager.ts:126-130（onInitialized addDecorator(toolAdmin)）。
TEST(ToolAdminDecorator, ForwardsToActiveTool)
{
    // --- Startup 接线：ToolAdmin 注册为 ViewManager 装饰器 ---
    auto& app = Application::Get();
    if (!app.isInitialized()) {
        Application::Options opts;
        opts.applicationId = "WindowAreaTest";
        opts.applicationVersion = "1.0.0";
        opts.noRender = true;
        ASSERT_TRUE(app.Startup(opts));
    }
    auto const& decorators = app.GetViewManager().GetDecorators();
    EXPECT_NE(std::find(decorators.begin(), decorators.end(),
                        static_cast<IDecorator*>(&app.GetToolAdmin())),
              decorators.end())
        << "Startup must register ToolAdmin as a ViewManager decorator (ViewManager.ts:130)";

    // --- decorate 转发：活动 ViewTool 的 decorate 被恰好调用一次 ---
    auto view = SpatialViewState::CreateBlank(nullptr, {0,0,0}, {100,100,100});
    Viewport* vp = Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);
    dqRender::Decorations decorations;
    dqApp::DecorationsCache decCache;
    DecorateContext context(*vp, decorations, decCache);

    RecordingViewTool tool;
    auto& admin = app.GetToolAdmin();
    ViewTool* const priorViewTool = admin.GetViewTool();
    admin.installViewTool(&tool);
    admin.Decorate(context);
    EXPECT_EQ(tool.decorateCallCount, 1);
    admin.installViewTool(priorViewTool);  // 还原插槽，避免污染后续测试
    delete vp;
}

// ===========================================================================
// W4 — WindowAreaTool（ViewTool.ts:3531-3816）+ CanvasDecoration 机制
// （CanvasDecoration.ts:18-38 + ViewContext.ts:312-324 + Target.ts:1413-1424）。
// ===========================================================================

namespace {

// 视口夹具：1000×500（纵横比 2:1）+ blank spatial 视图（origin (0,0,0)、extents
// (100,100,100)）。SetupFromView 重建 ViewingSpace（内部 FixAspectRatio(2.0) 把
// extents 调整为 (100,50,100)、origin → (0,25,0)，中心保持 (50,50,50)——
// ViewState.ts:811-820）。
// Authored: harness helper — same pattern as LookToolTest.cpp's lookBuildViewport /
//           ViewToolTest.cpp's buildViewWithValidViewingSpace (no reference test exists
//           for the Viewport wrapper init pattern in isolation).
struct WaViewport {
    dqBase::RefPtr<SpatialViewState> view;
    Viewport* vp = nullptr;
    WaViewport() {
        view = SpatialViewState::CreateBlank(nullptr, dqGeom::Point3d::From(0, 0, 0),
                                             dqGeom::Vector3d::From(100.0, 100.0, 100.0));
        vp = Viewport::Create(nullptr, view);
        vp->resize(1000, 500);          // 视口 1000×500（2:1）
        view->SetOrigin(dqGeom::Point3d::From(0.0, 0.0, 0.0));      // 重置 Create 期
        view->SetExtents(dqGeom::Vector3d::From(100.0, 100.0, 100.0));  // FixAspectRatio 的改动
        vp->SetupFromView();            // 以新几何重建 ViewingSpace（FixAspectRatio →
                                        // extents (100,50,100)，origin (0,25,0)）
    }
    ~WaViewport() { delete vp; }
};

void ensureAppStartup()
{
    auto& app = Application::Get();
    if (!app.isInitialized()) {
        Application::Options opts;
        opts.applicationId = "WindowAreaTest";
        opts.applicationVersion = "1.0.0";
        opts.noRender = true;
        ASSERT_TRUE(app.Startup(opts));
    }
    app.GetToolAdmin().OnInitialized();  // 幂等注册（含 "View.WindowArea"）
}

// 记录型 CanvasContext（2D 层绘制断言用——记录 drawDecoration 体发出的全部调用）。
// Authored: test fixture — no reference equivalent; the reference tests canvas
//           decorations via a live CanvasRenderingContext2D (DOM), unavailable here.
class RecordingCanvasContext : public dqRender::CanvasContext {
public:
    void save() override { ++saveCount; }
    void restore() override { ++restoreCount; }
    void translate(double x, double y) override { translates.emplace_back(x, y); }
    void beginPath() override { ++beginPathCount; }
    void moveTo(double x, double y) override { moveTos.emplace_back(x, y); }
    void lineTo(double x, double y) override { lineTos.emplace_back(x, y); }
    void stroke() override { ++strokeCount; }
    void setStrokeStyle(dqCommon::ColorDef color) override { strokeStyle = color; }
    void setLineWidth(double w) override { lineWidth = w; }
    // 90c6d3b GLCanvasContext 重写后 CanvasContext 新纯虚（fillStyle/globalAlpha/
    // arc/fill/drawImage）——本夹具补空实现解除 C2259（WindowArea 断言不涉及填充/sprite）。
    void setFillStyle(dqCommon::ColorDef) override {}
    void setGlobalAlpha(double) override {}
    void arc(double, double, double, double, double) override {}
    void fill() override {}
    void drawImage(dqRender::rhi::TextureHandle, uint32_t, uint32_t, double, double) override {}

    int saveCount = 0;
    int restoreCount = 0;
    int beginPathCount = 0;
    int strokeCount = 0;
    std::vector<std::pair<double, double>> translates;
    std::vector<std::pair<double, double>> moveTos;
    std::vector<std::pair<double, double>> lineTos;
    dqCommon::ColorDef strokeStyle = dqCommon::ColorDef::black;
    double lineWidth = 0.0;
};

}  // namespace

// Authored: no reference test exists in itwinjs-core for WindowAreaTool's
//           computeWindowCorners; scenario from ViewTool.ts:3639-3677（纵横比适配 +
//           最小拖拽距离拒绝——startDragDistanceInches=0.15in→14.4px，ToolSettings.ts:34
//           × pixelsFromInches 96，Viewport.ts:2099/1441-1444）。
TEST(WindowArea, ComputeWindowCornersAspectFit)
{
    WaViewport r;
    ASSERT_NE(r.vp, nullptr);
    WindowAreaTool tool(r.vp);

    // 拖 100×400 视口矩形（1:4）：第一点 view(450,50) → 世界；动态到 view(550,450)。
    BeButtonEvent down;
    down.viewport = r.vp;
    down.viewPoint = dqGeom::Point3d::From(450.0, 50.0, 0.0);
    down.point = r.vp->ViewToWorld(down.viewPoint);
    down.rawPoint = down.point;
    EXPECT_EQ(tool.onDataButtonDown(down), EventHandled::Yes);   // ViewTool.ts:3584-3613

    BeButtonEvent move = down;
    move.viewPoint = dqGeom::Point3d::From(550.0, 450.0, 0.0);
    move.point = r.vp->ViewToWorld(move.viewPoint);
    move.rawPoint = move.point;
    tool.onMouseMotion(move);   // :3615 → doManipulation(ev, true) 记录 secondPt

    // computeWindowCorners（:3639-3677）：viewAspect = skew(1.0)*delta.y/delta.x = 50/100 = 0.5；
    // 拖拽纵横比 |400/100| = 4 ≥ 0.5 → else 分支：halfDeltaY = 400/2 = 200，
    // halfDeltaX = 200/0.5 = 400；center = (450,50)+0.5*(100,400) = (500,250)。
    // → 角点 (100,50)/(900,450)（800×400 = 视口纵横比 2:1）。
    auto* corners = tool.computeWindowCorners();
    ASSERT_NE(corners, nullptr);
    ASSERT_EQ(corners->size(), 2u);
    EXPECT_NEAR((*corners)[0].x, 100.0, 1e-6);
    EXPECT_NEAR((*corners)[0].y, 50.0, 1e-6);
    EXPECT_NEAR((*corners)[1].x, 900.0, 1e-6);
    EXPECT_NEAR((*corners)[1].y, 450.0, 1e-6);

    // 最小拖拽距离拒绝（:3649-3651）：magnitudeXY < 14.4px → nullptr。
    BeButtonEvent tiny = down;
    tiny.viewPoint = dqGeom::Point3d::From(452.0, 55.0, 0.0);   // delta (2,5) ≈ 5.4px < 14.4
    tiny.point = r.vp->ViewToWorld(tiny.viewPoint);
    tiny.rawPoint = tiny.point;
    tool.onMouseMotion(tiny);
    EXPECT_EQ(tool.computeWindowCorners(), nullptr);
}

// Authored: no reference test exists in itwinjs-core for WindowAreaTool's ortho box
//           zoom application; scenario from ViewTool.ts:3733-3815（doManipulation
//           正交路径 :3792-3812 + synchWithView :3814 → saveViewUndo Viewport.ts:3648-3676）。
TEST(WindowArea, OrthoBoxSelectionZoomsAndSavesUndo)
{
    WaViewport r;
    ASSERT_NE(r.vp, nullptr);
    ASSERT_FALSE(r.vp->isCameraOn());      // blank 初始正交 → 正交路径（:3792-3812）
    EXPECT_FALSE(r.vp->isUndoPossible());  // Create 仅建基线（Viewport.cpp:163-169）

    WindowAreaTool tool(r.vp);

    // 两点世界矩形：npc (0.1,0.4,0.5) → (0.5,0.8,0.5)。
    // 视口 1000×500、视图 extents (100,50,100) @ origin (0,25,0)（identity 旋转）：
    //   p1 world = (10, 45, 50)（view px (100,300)），p2 world = (50, 65, 50)（view px (500,100)）。
    // 拖拽 view delta = (400,-200)：纵横比 0.5 == viewAspect 0.5 → computeWindowCorners
    // 不扩张；viewToWorldArray → range low (10,45,50) high (50,65,50)；
    // delta = (40,20,0) → delta.z = 当前 extents.z = 100（:3799）；originVec = (10,45,50)；
    // adjustViewDelta(aspect=2.0)：40 == 2.0*20 不动（ViewState.ts:908-914）→
    // setExtents(40,20,100) + setOrigin(10,45,50)（:3808-3809）。
    BeButtonEvent down;
    down.viewport = r.vp;
    down.point = r.vp->NpcToWorld(dqGeom::Point3d::From(0.1, 0.4, 0.5));
    down.rawPoint = down.point;
    down.viewPoint = r.vp->WorldToView(down.point);
    EXPECT_EQ(tool.onDataButtonDown(down), EventHandled::Yes);

    BeButtonEvent up = down;
    up.point = r.vp->NpcToWorld(dqGeom::Point3d::From(0.5, 0.8, 0.5));
    up.rawPoint = up.point;
    up.viewPoint = r.vp->WorldToView(up.point);
    EXPECT_EQ(tool.onDataButtonDown(up), EventHandled::Yes);   // 次点 → doManipulation(false)

    auto const extents = r.view->GetExtents();
    EXPECT_NEAR(extents.x, 40.0, 1e-6);    // 窗口宽度（世界单位）
    EXPECT_NEAR(extents.y, 20.0, 1e-6);    // 窗口高度
    EXPECT_NEAR(extents.z, 100.0, 1e-6);   // 深度保留（:3799 delta.z = extents.z）
    auto const origin = r.view->GetOrigin();
    EXPECT_NEAR(origin.x, 10.0, 1e-6);     // 窗口左下角（世界）
    EXPECT_NEAR(origin.y, 45.0, 1e-6);
    EXPECT_NEAR(origin.z, 50.0, 1e-6);

    // :3814 synchWithView → saveViewUndo：姿态相对基线已变 → 撤销条目产生。
    EXPECT_TRUE(r.vp->isUndoPossible());
}

// Authored: CanvasDecoration 通路（CanvasDecoration.ts:18-38 + ViewContext.ts:312-324
//           的顺序语义：atFront 或空表 → push 尾部；否则 unshift 头部）。
//           DecorateContext 直接绑定 Decorations 容器（ToolAdminDecorator 既有模式）——
//           vp->GetDecorations() 仅在 RenderFrame 内经 CollectDecorations 填充（headless
//           不可达）；同一 AddCanvasDecoration 路径即 2D 层绘制路径的收集端。
TEST(CanvasDecoration, AddCanvasDecorationReachesDecorationsList)
{
    auto view = SpatialViewState::CreateBlank(nullptr, {0,0,0}, {100,100,100});
    Viewport* vp = Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);
    dqRender::Decorations decorations;
    dqApp::DecorationsCache decCache;
    DecorateContext context(*vp, decorations, decCache);

    bool drawnA = false;
    dqRender::CanvasDecoration decA;
    decA.position = dqGeom::Point2d{10.0, 20.0};   // position?: XAndY（CanvasDecoration.ts:31）
    decA.drawDecoration = [&drawnA](dqRender::CanvasContext&) { drawnA = true; };
    context.AddCanvasDecoration(std::move(decA));

    // 空表 → push（:321-322）。
    ASSERT_EQ(decorations.canvasDecorations.size(), 1u);
    ASSERT_TRUE(decorations.canvasDecorations[0].position.has_value());
    EXPECT_DOUBLE_EQ(decorations.canvasDecorations[0].position->x, 10.0);
    EXPECT_DOUBLE_EQ(decorations.canvasDecorations[0].position->y, 20.0);
    // drawDecoration 体随条目保存（2D 层每帧调用——Target.ts:1420）。
    RecordingCanvasContext rec;
    decorations.canvasDecorations[0].drawDecoration(rec);
    EXPECT_TRUE(drawnA);

    // 顺序语义（:321-324）：非空且非 atFront → unshift 头部；atFront → push 尾部。
    dqRender::CanvasDecoration decB;
    decB.drawDecoration = [](dqRender::CanvasContext&) {};
    context.AddCanvasDecoration(std::move(decB));                 // → [B, A]
    dqRender::CanvasDecoration decC;
    decC.drawDecoration = [](dqRender::CanvasContext&) {};
    context.AddCanvasDecoration(std::move(decC), /*atFront=*/true);  // → [B, A, C]
    ASSERT_EQ(decorations.canvasDecorations.size(), 3u);
    EXPECT_FALSE(decorations.canvasDecorations[0].position.has_value());  // B 无 position
    EXPECT_TRUE(decorations.canvasDecorations[1].position.has_value());   // A 在中间

    delete vp;
}

// Authored: no reference test exists in itwinjs-core for WindowAreaTool's crosshair
//           decoration; scenario from ViewTool.ts:3712-3730（无首点 + 光标视口 →
//           全屏十字线经 AddCanvasDecoration，经 W3 ToolAdmin::Decorate 通路）。
TEST(CanvasDecoration, CrossHairDecorationDrawsFullScreenLines)
{
    ensureAppStartup();
    WaViewport r;   // 1000×500
    ASSERT_NE(r.vp, nullptr);

    WindowAreaTool tool(r.vp);
    auto& admin = Application::Get().GetToolAdmin();
    ViewTool* const priorViewTool = admin.GetViewTool();
    admin.installViewTool(&tool);

    // 动态移动（无首点）→ doManipulation(inDynamics=true) 记录 _lastPtView（:3740）。
    BeButtonEvent move;
    move.viewport = r.vp;
    move.viewPoint = dqGeom::Point3d::From(320.3, 240.7, 0.0);
    move.point = r.vp->ViewToWorld(move.viewPoint);
    move.rawPoint = move.point;
    tool.onMouseMotion(move);

    // cursorView = currentInputState.viewport（ToolAdmin.ts:537）——置为本视口，
    // 模拟光标在其上（:3712 的门）。
    admin.currentInputState().viewport = r.vp;
    // 测试断言隔离：AddViewport 首注册触发 SetSelectedViewport → StartDefaultTool
    // → Selection 工具安装（ViewManager.ts:246-247）→ locate 光圈默认开
    // （SelectTool.ts:239）。本用例只量十字线条目——先把光圈显式关掉
    // （真实 startViewTool 流也会关，ToolAdmin.ts:1833），否则装饰列表含
    // crosshair + locate 圈 = 2（参考行为一致，非回归）。
    admin.setLocateCircleOn(false);

    dqRender::Decorations decorations;
    dqApp::DecorationsCache decCache;
    DecorateContext context(*r.vp, decorations, decCache);
    admin.Decorate(context);   // W3 通路：转发活动 ViewTool 的 decorate

    // :3730 — 十字线条目经 AddCanvasDecoration 到达列表。
    ASSERT_EQ(decorations.canvasDecorations.size(), 1u);
    auto const& dec = decorations.canvasDecorations[0];
    EXPECT_FALSE(dec.position.has_value());   // { drawDecoration } —— 无 position（:3730）
    ASSERT_TRUE(static_cast<bool>(dec.drawDecoration));

    // lambda 体逐字断言（:3720-3729）：像素中心对齐 floor+0.5 → (320.5, 240.5)；
    // 对比色（black→"black"，否则 "white"——:3722）；lineWidth 1；全屏横竖线。
    RecordingCanvasContext rec;
    dec.drawDecoration(rec);
    EXPECT_EQ(rec.beginPathCount, 1);
    EXPECT_EQ(rec.strokeCount, 1);
    EXPECT_DOUBLE_EQ(rec.lineWidth, 1.0);
    auto const contrast = r.vp->getContrastToBackgroundColor();
    auto const expectedColor = contrast.equals(dqCommon::ColorDef::black)
        ? dqCommon::ColorDef::black : dqCommon::ColorDef::white;
    EXPECT_TRUE(rec.strokeStyle.equals(expectedColor));
    ASSERT_EQ(rec.moveTos.size(), 2u);
    ASSERT_EQ(rec.lineTos.size(), 2u);
    EXPECT_DOUBLE_EQ(rec.moveTos[0].first, 0.0);      // moveTo(viewRect.left, y)
    EXPECT_DOUBLE_EQ(rec.moveTos[0].second, 240.5);
    EXPECT_DOUBLE_EQ(rec.lineTos[0].first, 1000.0);   // lineTo(viewRect.right, y)
    EXPECT_DOUBLE_EQ(rec.lineTos[0].second, 240.5);
    EXPECT_DOUBLE_EQ(rec.moveTos[1].first, 320.5);    // moveTo(x, viewRect.top)
    EXPECT_DOUBLE_EQ(rec.moveTos[1].second, 0.0);
    EXPECT_DOUBLE_EQ(rec.lineTos[1].first, 320.5);    // lineTo(x, viewRect.bottom)
    EXPECT_DOUBLE_EQ(rec.lineTos[1].second, 500.0);

    admin.currentInputState().viewport = nullptr;
    admin.installViewTool(priorViewTool);  // 还原插槽
}

// Authored: no reference test exists in itwinjs-core for exitViewTool's decoration
//           invalidation; scenario from ToolAdmin.ts:1805（exitViewTool 无条件
//           IModelApp.viewManager.invalidateDecorationsAllViews()）→ ViewManager.ts:361-364
//           （for (const vp of this) vp.invalidateDecorations()）→ Viewport.ts:414
//           的 _decorationsValid 清零。动态期驱动对齐 ViewTool.ts:3740-3741
//           （doManipulation inDynamics 分支同样经 invalidateDecorationsAllViews）。
TEST(WindowArea, ExitViewToolInvalidatesDecorationsAllViews)
{
    ensureAppStartup();
    WaViewport r;
    ASSERT_NE(r.vp, nullptr);

    auto& app = Application::Get();
    auto& admin = app.GetToolAdmin();

    // invalidateDecorationsAllViews 只遍历已注册视口（ViewManager.ts:361-364）。
    app.GetViewManager().AddViewport(r.vp);

    WindowAreaTool tool(r.vp);
    ViewTool* const priorViewTool = admin.GetViewTool();
    admin.installViewTool(&tool);

    // 动态移动（无首点）→ doManipulation(inDynamics=true)：:3740 记录 _lastPtView，
    // :3741 invalidateDecorationsAllViews → 本视口装饰失效。
    BeButtonEvent move;
    move.viewport = r.vp;
    move.viewPoint = dqGeom::Point3d::From(320.0, 240.0, 0.0);
    move.point = r.vp->ViewToWorld(move.viewPoint);
    move.rawPoint = move.point;
    tool.onMouseMotion(move);

    // 模拟一帧回收装饰（headless 不可达 RenderFrame Step 12 —— 测试接缝置位），
    // 并确认十字线经 W3 ToolAdmin::Decorate 通路进入装饰列表（:3730）。
    r.vp->SetDecorationsValidForTest(true);
    admin.currentInputState().viewport = r.vp;   // cursorView 门（:3712）
    admin.setLocateCircleOn(false);   // 同上——隔离 Selection 默认安装的光圈（真实 startViewTool 流亦关，ToolAdmin.ts:1833）
    dqRender::Decorations decorations;
    dqApp::DecorationsCache decCache;
    DecorateContext context(*r.vp, decorations, decCache);
    admin.Decorate(context);
    EXPECT_EQ(decorations.canvasDecorations.size(), 1u);   // 动态十字线已进入装饰

    // exitViewTool → ToolAdmin.ts:1805 → 全部已注册视口装饰失效。
    admin.exitViewTool();
    EXPECT_EQ(admin.GetViewTool(), nullptr);
    EXPECT_FALSE(r.vp->GetDecorationsValidForTest());

    // 还原单例状态，避免污染后续测试。
    admin.currentInputState().viewport = nullptr;
    admin.installViewTool(priorViewTool);
    app.GetViewManager().DropViewport(r.vp);
}

// Authored: 注册表可建 "View.WindowArea"（ToolAdmin::OnInitialized 注册对齐参考
//           IModelApp.startup 的 registerModule(viewTool) 净效果，IModelApp.ts:441/446
//           ——viewTool 模块含 WindowAreaTool，ViewTool.ts:3531-3532 toolId）。
TEST(WindowArea, RegisteredInToolAdmin)
{
    ensureAppStartup();
    WaViewport r;
    ASSERT_NE(r.vp, nullptr);

    auto& ta = Application::Get().GetToolAdmin();
    std::unique_ptr<InteractiveTool> tool(
        ta.GetRegistry().CreateVP("View.WindowArea", r.vp, /*oneShot=*/false, /*isDraggingRequired=*/false));
    ASSERT_NE(tool, nullptr);
    EXPECT_STREQ(tool->getToolId(), "View.WindowArea");
}

// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Marker/Cluster/MarkerSet + ToolAdmin 画布装饰路由测试
//
// Authored: no reference test exists in itwinjs-core for Marker/MarkerSet
//           clustering（core/frontend 唯一的 Marker 测试 = DeferredMarkerHtml
//           Removal.test.ts——htmlElement DOM 覆盖面，XE6 不移植域）。本文件
//           测试场景/断言值从参考实现的行为语义推导（Marker.ts/Marker.ts 注释
//           + ToolAdmin.ts:1036-1057 路由），钉值由夹具几何自校准（ViewToWorld
//           回代），不真随机。
//
// 覆盖：
//   - Marker.setPosition 投影/剔除/缩放门（Marker.ts:250-283）
//   - Marker.pick / drawDecoration 序（:148/:195-223）
//   - Cluster.getClusterLocation（:342-349）
//   - MarkerSet 聚类/最小成组数/半径缺省/视口门/缓存重建（:427-493）
//   - ToolAdmin 悬停/点击路由（ToolAdmin.ts:1036-1057 + 三调用点 + :2073
//     光圈门）+ XE14 析构缝
//   - DecorateContext 画布装饰缓存（ViewContext.ts:314-315/restoreCache
//     :248-250——M-X X-b1 接通）

#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/DecorateContext.h>
#include <dqApp/Marker.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>

#include <dqBase/DqEvent.h>

#include <dqRender/CanvasDecoration.h>

#include <QApplication>
#include <QCursor>

#include <cmath>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace dqApp;

namespace {

// ---------------------------------------------------------------------------
// 夹具：BlankConnection + SpatialViewState + Viewport（EventDispatchTest 的
// idleToolBuildViewport 同型——无 GL 的 headless 视口，setupViewFromFrustum
// 初始化 ViewingSpace 使 worldToView 投影可用）。
// QApplication 环境由同二进制的 FrustumApiTest/ViewToolTest 注册（沿用
// EventDispatchTest 的既有约定，本 TU 不重复注册）。
// ---------------------------------------------------------------------------
struct MarkerViewportFixture {
    dqBase::RefPtr<BlankConnection> imodel;
    dqBase::RefPtr<SpatialViewState> view;
    Viewport* vp = nullptr;   // 泄漏至进程终（EventDispatchTest 同约定——QObject 树无父）
};
MarkerViewportFixture makeMarkerViewport()
{
    MarkerViewportFixture f;
    BlankConnectionProps props;
    props.extents = dqGeom::Range3d(
        dqGeom::Point3d::From(-100.0, -100.0, -100.0),
        dqGeom::Point3d::From(100.0, 100.0, 100.0));
    f.imodel = BlankConnection::create(props);
    f.view = SpatialViewState::CreateBlank(f.imodel.Get(),
                                           dqGeom::Point3d::From(0, 0, 0),
                                           dqGeom::Vector3d::From(200, 200, 200));
    f.vp = Viewport::Create(nullptr, f.view);
    float const w = static_cast<float>(f.vp->width());
    float const h = static_cast<float>(f.vp->height());
    f.view->SetExtents(dqGeom::Vector3d::From(200.0, 200.0 * h / w, 200.0));
    f.vp->setupViewFromFrustum(f.vp->getFrustum(true));
    return f;
}

// 测试 Marker——暴露参考 protected 成员（Marker.ts:72-80 的子类消费形，
// IncidentMarkerDemo.ts:25 注释行即先例）+ onMouseButton 覆写记录。
struct TestMarker : public Marker {
    using Marker::Marker;
    using Marker::m_hiliteColor;
    using Marker::m_isHilited;
    using Marker::m_scaleFactor;
    using Marker::m_lastDecorateViewport;   // 终审 I-1 锁的观测面
    using Marker::m_scaleFactorRange;

    int mouseButtonCalls = 0;
    bool buttonResult = true;   // IncidentMarker.onMouseButton 恒 true（:40）
    bool onMouseButton(BeButtonEvent const& /*ev*/) override
    {
        ++mouseButtonCalls;
        return buttonResult;
    }
};

// 测试 MarkerSet——聚类创建计数 + entries 检视 + m_minScaleViewW 暴露
//（Marker.ts:365 的 protected 成员——修复轮 2 锁的观测面）。
struct TestMarkerSet : public MarkerSet<TestMarker> {
    using MarkerSet<TestMarker>::MarkerSet;
    using MarkerSet<TestMarker>::entriesCountForTest;
    using MarkerSet<TestMarker>::m_minScaleViewW;
    int clusterMarkerCalls = 0;
    Marker* getClusterMarker(Cluster<TestMarker>& cluster) override
    {
        ++clusterMarkerCalls;
        return new TestMarker(cluster.getClusterLocation(), cluster.markers[0]->size);
    }
};

// 记录型 CanvasContext（CanvasContext 纯虚全覆写）——drawDecoration 序钉桩。
struct RecordingCanvasContext : public dqRender::CanvasContext {
    std::vector<std::string> ops;
    struct ImageCall { uint32_t w; uint32_t h; double dx; double dy; };
    std::vector<ImageCall> images;

    void save() override { ops.emplace_back("save"); }
    void restore() override { ops.emplace_back("restore"); }
    void translate(double, double) override { ops.emplace_back("translate"); }
    void beginPath() override { ops.emplace_back("beginPath"); }
    void moveTo(double, double) override { ops.emplace_back("moveTo"); }
    void lineTo(double, double) override { ops.emplace_back("lineTo"); }
    void stroke() override { ops.emplace_back("stroke"); }
    void setStrokeStyle(dqCommon::ColorDef) override { ops.emplace_back("strokeStyle"); }
    void setLineWidth(double) override { ops.emplace_back("lineWidth"); }
    void setFillStyle(dqCommon::ColorDef) override { ops.emplace_back("fillStyle"); }
    void setGlobalAlpha(double) override { ops.emplace_back("alpha"); }
    void arc(double, double, double, double, double) override { ops.emplace_back("arc"); }
    void fill() override { ops.emplace_back("fill"); }
    void drawImage(dqRender::rhi::TextureHandle, uint32_t w, uint32_t h,
                   double dx, double dy) override
    {
        ops.emplace_back("drawImage");
        images.push_back({w, h, dx, dy});
    }
};

// 视口像素 → 世界点的自校准换算（钉值不依赖夹具取景的假设——CreateBlank 的
// 视图取向/取景由 ViewingSpace 决定，测试一律经 ViewToWorld 回代定位）。
dqGeom::Point3d atViewPixel(Viewport* vp, double x, double y)
{
    return vp->ViewToWorld(dqGeom::Point3d::From(x, y, 0));
}

// 直构一个 DecorateContext（收集到自建 Decorations——MarkerSet.addDecoration
// 的消费面）。
struct CollectFixture {
    MarkerViewportFixture holder;
    dqRender::Decorations decorations;
    DecorationsCache cache;
    CollectFixture() : holder(makeMarkerViewport()) {}
    DecorateContext makeContext() { return DecorateContext(*holder.vp, decorations, cache); }
};

// 记录型 idle tool（sendButtonEvent 吞没断言——idle 不应收到）。
struct IdleRecordingTool : public InteractiveTool {
    int dataDownCalls = 0;
    const char* getToolId() const override { return "IdleRecording"; }
    EventHandled onDataButtonDown(BeButtonEvent const&) override
    {
        ++dataDownCalls;
        return EventHandled::Yes;
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// Marker.setPosition（Marker.ts:250-283）
// ---------------------------------------------------------------------------

// Authored: 投影到视口中心 + rect 半尺寸布局（:262-265）。
TEST(MarkerTest, SetPositionProjectsToViewCenter)
{
    CollectFixture f;
    // 视口中心像素（320,240）的世界点 → setPosition 后落在该像素（自校准）。
    TestMarker marker(atViewPixel(f.holder.vp, 320, 240), dqGeom::Point2d::From(30, 30));
    ASSERT_TRUE(marker.setPosition(*f.holder.vp));

    double const cx = f.holder.vp->width() / 2.0;
    double const cy = f.holder.vp->height() / 2.0;
    EXPECT_NEAR(marker.position.x, cx, 1.0);
    EXPECT_NEAR(marker.position.y, cy, 1.0);
    // rect = position ± size/2（ViewRect init 的 floor 语义内——整值精确）。
    EXPECT_EQ(marker.rect.left, static_cast<int32_t>(std::floor(cx - 15.0)));
    EXPECT_EQ(marker.rect.right, static_cast<int32_t>(std::floor(cx + 15.0)));
    EXPECT_EQ(marker.rect.top, static_cast<int32_t>(std::floor(cy - 15.0)));
    EXPECT_EQ(marker.rect.bottom, static_cast<int32_t>(std::floor(cy + 15.0)));
}

// Authored: 剔除臂（:251-260）——不可见/视口矩形外。
TEST(MarkerTest, SetPositionCullsInvisibleAndOffscreen)
{
    CollectFixture f;
    TestMarker hidden(dqGeom::Point3d::From(0, 0, 0), dqGeom::Point2d::From(30, 30));
    hidden.visible = false;
    EXPECT_FALSE(hidden.setPosition(*f.holder.vp));   // :251——visible=false 早退

    // 视口矩形外（世界点投影远超 640x480——:259 的 viewRect.containsPoint 门）。
    TestMarker offscreen(dqGeom::Point3d::From(1.0e6, 1.0e6, 0.0), dqGeom::Point2d::From(30, 30));
    EXPECT_FALSE(offscreen.setPosition(*f.holder.vp));
}

// Authored: scaleFactor 门（:267-280）——正交视图（isCameraOn=false）不缩放
// rect、scaleFactor 置 1。
TEST(MarkerTest, SetPositionScaleFactorInertOnOrthoView)
{
    CollectFixture f;
    ASSERT_FALSE(f.holder.vp->isCameraOn());   // 夹具 = 正交（BlankConnection 视图）
    TestMarker marker(atViewPixel(f.holder.vp, 320, 240), dqGeom::Point2d::From(30, 30));
    marker.setScaleFactor(dqGeom::Range1d(0.2, 1.4));   // IncidentMarkerDemo.ts:51
    ASSERT_TRUE(marker.setPosition(*f.holder.vp));
    ASSERT_TRUE(marker.m_scaleFactor.has_value());
    EXPECT_DOUBLE_EQ(marker.m_scaleFactor->x, 1.0);   // :279——正交下 scale 恒 1
    EXPECT_DOUBLE_EQ(marker.m_scaleFactor->y, 1.0);
    // rect 未缩放（:277 的 scaleAboutCenter 在 isCameraOn 门内）。
    EXPECT_EQ(marker.rect.width(), 30);
    EXPECT_EQ(marker.rect.height(), 30);
}

// Authored: 修复轮 2——MarkerSet<T>::getMinScaleViewW 成员体内的同名自由
// 函数调用（Marker.ts:417-421 → :46-63）命名空间限定化的行为锁：camera-on
// 视口 + setScaleFactor marker + MarkerSet 驱动 setPosition——C++ 类作用域
// 查找命中成员即终止且抑制 ADL，未限定时成员无限自递归（修复前本用例栈
// 溢出崩）；GREEN = setPosition 完成 + m_minScaleViewW 恰等于自由函数值
// （钉值自校准——同一计算）+ scaleFactor 在参考 clamp 域 [.4, 2.0]。
TEST(MarkerTest, MarkerSetMinScaleViewWDelegatesToFreeFunction)
{
    CollectFixture f;
    // 相机开启（M-O(3) P1 的 TurnCameraOn 面——纯数学，headless 安全）。
    ASSERT_EQ(f.holder.vp->TurnCameraOn(), ViewStatus::Success);
    ASSERT_TRUE(f.holder.vp->isCameraOn());

    TestMarkerSet set(f.holder.vp);
    auto const center = atViewPixel(f.holder.vp, 320, 240);
    TestMarker* raw = set.markers().add(
        std::make_unique<TestMarker>(center, dqGeom::Point2d::From(30, 30)));
    raw->setScaleFactor(dqGeom::Range1d(0.2, 1.4));   // IncidentMarkerDemo.ts:51 同值

    auto context = f.makeContext();
    set.addDecoration(context);   // 驱动面：setPosition(vp, this) → 成员 getMinScaleViewW
    EXPECT_EQ(f.decorations.canvasDecorations.size(), 1u);   // setPosition 完成并入列

    // m_minScaleViewW 恰等于自由函数（同一计算——缓存值对账）。
    ASSERT_TRUE(set.m_minScaleViewW.has_value());
    EXPECT_DOUBLE_EQ(*set.m_minScaleViewW, dqApp::getMinScaleViewW(*f.holder.vp));
    EXPECT_GT(*set.m_minScaleViewW, 0.0);

    // 缩放因子落在参考 clamp 域（Marker.ts:274/:276——Geometry.clamp(.,.4,2.0)）。
    ASSERT_TRUE(raw->m_scaleFactor.has_value());
    EXPECT_GE(raw->m_scaleFactor->x, 0.4);
    EXPECT_LE(raw->m_scaleFactor->x, 2.0);
    EXPECT_DOUBLE_EQ(raw->m_scaleFactor->x, raw->m_scaleFactor->y);
}

// Authored: pick（:148——rect.containsPoint；ViewRect.ts:145 的左/上含、
// 右/下不含边界语义）。
TEST(MarkerTest, PickMatchesRectEdgeSemantics)
{
    CollectFixture f;
    TestMarker marker(atViewPixel(f.holder.vp, 320, 240), dqGeom::Point2d::From(30, 30));
    ASSERT_TRUE(marker.setPosition(*f.holder.vp));
    // 内含点
    EXPECT_TRUE(marker.pick(dqGeom::Point2d::From(marker.position.x, marker.position.y)));
    EXPECT_TRUE(marker.pick(dqGeom::Point2d::From(marker.rect.left, marker.rect.top)));   // 左/上含
    // 右/下不含（ViewRect.ts:145 注释语义）
    EXPECT_FALSE(marker.pick(dqGeom::Point2d::From(marker.rect.right, marker.rect.bottom)));
    // 外点
    EXPECT_FALSE(marker.pick(dqGeom::Point2d::From(1.0, 1.0)));
}

// ---------------------------------------------------------------------------
// Marker.drawDecoration（:195-223）+ drawHilited（:187-192）
// ---------------------------------------------------------------------------

// Authored: 绘制序 = drawFunc → image（:203-213）；image 尺寸/偏移（:208-212
// ——imageSize 40x40 + imageOffset (0,30) → drawImage(40,40,-20,-50)）；
// label 腿 = XE8 登记缺口（无 fillText 调用面——绘制完成即证）。
TEST(MarkerTest, DrawDecorationDrawsFuncThenImageWithOffset)
{
    TestMarker marker(dqGeom::Point3d::From(0, 0, 0), dqGeom::Point2d::From(30, 30));
    marker.drawFunc = [](dqRender::CanvasContext& ctx) {
        ctx.beginPath();
        ctx.arc(0, 0, 13, 0, 6.283185307179586);
        ctx.fill();
    };
    MarkerImage img;   // 无效纹理句柄——记录桩不触 GL（GLCanvasContext 的无效句柄
                       // 早退由 GLCanvasContext::drawImage 承载，不在本锁面）。
    img.width = 256;
    img.height = 256;
    marker.setImage(img);
    marker.imageSize = dqGeom::Point2d::From(40, 40);     // IncidentMarkerDemo.ts:49
    marker.imageOffset = dqGeom::Point2d::From(0, 30);    // :48
    marker.label = std::string("7");                      // :52

    RecordingCanvasContext ctx;
    marker.drawDecoration(ctx);

    // drawFunc 内容先于 drawImage（:203→:207 序）。
    auto const drawImageIt = std::find(ctx.ops.begin(), ctx.ops.end(), "drawImage");
    auto const fillIt = std::find(ctx.ops.begin(), ctx.ops.end(), "fill");
    ASSERT_NE(drawImageIt, ctx.ops.end());
    ASSERT_NE(fillIt, ctx.ops.end());
    EXPECT_LT(fillIt, drawImageIt);   // drawFunc（fill）在 image 之前
    // :208-212——offset = (40/2+0, 40/2+30) = (20,50) → drawImage(-20,-50,40,40)。
    ASSERT_EQ(ctx.images.size(), 1u);
    EXPECT_EQ(ctx.images[0].w, 40u);
    EXPECT_EQ(ctx.images[0].h, 40u);
    EXPECT_DOUBLE_EQ(ctx.images[0].dx, -20.0);
    EXPECT_DOUBLE_EQ(ctx.images[0].dy, -50.0);
}

// Authored: wantImage=false 门（:207）+ 无图仅 drawFunc。
TEST(MarkerTest, DrawDecorationWantImageGateSuppressesImage)
{
    struct NoImageMarker : public TestMarker {
        using TestMarker::TestMarker;
        bool wantImage() const override { return false; }   // :124 覆写形
    };
    NoImageMarker marker(dqGeom::Point3d::From(0, 0, 0), dqGeom::Point2d::From(30, 30));
    MarkerImage img;
    img.width = 8;
    img.height = 8;
    marker.setImage(img);

    RecordingCanvasContext ctx;
    marker.drawDecoration(ctx);
    EXPECT_TRUE(ctx.images.empty());
}

// Authored: makeFrom（:172-180——位置/尺寸/rect/scaleFactor 随拷）。
TEST(MarkerTest, MakeFromCopiesPositionRectAndScaleFactor)
{
    CollectFixture f;
    TestMarker marker(atViewPixel(f.holder.vp, 320, 240), dqGeom::Point2d::From(30, 30));
    marker.setScaleFactor(dqGeom::Range1d(0.2, 1.4));
    ASSERT_TRUE(marker.setPosition(*f.holder.vp));

    auto copy = Marker::makeFrom<TestMarker>(marker);
    EXPECT_EQ(copy->worldLocation.x, marker.worldLocation.x);
    EXPECT_EQ(copy->size.x, marker.size.x);
    EXPECT_EQ(copy->rect.left, marker.rect.left);
    EXPECT_EQ(copy->position.x, marker.position.x);
    ASSERT_TRUE(copy->m_scaleFactorRange.has_value());
    EXPECT_DOUBLE_EQ(copy->m_scaleFactorRange->low, 0.2);
    EXPECT_DOUBLE_EQ(copy->m_scaleFactorRange->high, 1.4);
}

// Authored: 悬停进出（:130-137）——enter 置 hilite + 采视口 hilite 色，
// leave 清除。
TEST(MarkerTest, MouseEnterHilitesWithViewportHiliteColor)
{
    CollectFixture f;
    TestMarker marker(dqGeom::Point3d::From(0, 0, 0), dqGeom::Point2d::From(30, 30));
    BeButtonEvent ev;
    ev.viewport = f.holder.vp;

    EXPECT_FALSE(marker.m_isHilited);
    marker.onMouseEnter(ev);
    EXPECT_TRUE(marker.m_isHilited);
    ASSERT_TRUE(marker.m_hiliteColor.has_value());
    // :132——ev.viewport.hilite.color（DanQing GetHiliteColorDef 缺省
    // (0x23,0xbb,0xfc)，Hilite.ts:53）。
    auto const c = marker.m_hiliteColor->getColors();
    EXPECT_EQ(c.r, 0x23);
    EXPECT_EQ(c.g, 0xbb);
    EXPECT_EQ(c.b, 0xfc);
    marker.onMouseLeave();
    EXPECT_FALSE(marker.m_isHilited);
}

// Authored: onMouseMove 开 tooltip（:140-143）——OnToolTip 事件携带 title
// 纯文本（XE3）+ 视口内坐标透传。
TEST(MarkerTest, MouseMoveOpensToolTipWithTitle)
{
    CollectFixture f;
    TestMarker marker(dqGeom::Point3d::From(0, 0, 0), dqGeom::Point2d::From(30, 30));
    marker.title = std::string("Severity: 5\nId: 3");   // XE3 纯文本形

    std::vector<std::string> tips;
    dqBase::DqEventScope scope;
    scope.add(Application::Get().GetNotificationManager().OnToolTip.AddListener(
        [&tips](std::string const& message, double, double) { tips.push_back(message); }));

    BeButtonEvent ev;
    ev.viewport = f.holder.vp;
    ev.viewPoint = dqGeom::Point3d::From(320, 240, 0);
    marker.onMouseMove(ev);
    ASSERT_EQ(tips.size(), 1u);
    EXPECT_EQ(tips[0], "Severity: 5\nId: 3");

    // 无 title → 不开（:141 门）。
    TestMarker untitled(dqGeom::Point3d::From(0, 0, 0), dqGeom::Point2d::From(30, 30));
    untitled.onMouseMove(ev);
    EXPECT_EQ(tips.size(), 1u);
}

// ---------------------------------------------------------------------------
// Cluster（:325-350）
// ---------------------------------------------------------------------------

// Authored: getClusterLocation = 成员 worldLocation 平均（:342-349）。
TEST(MarkerTest, ClusterLocationIsAverageOfWorldLocations)
{
    auto a = std::make_unique<TestMarker>(dqGeom::Point3d::From(0, 0, 0), dqGeom::Point2d::From(30, 30));
    auto b = std::make_unique<TestMarker>(dqGeom::Point3d::From(10, 20, 30), dqGeom::Point2d::From(30, 30));
    auto c = std::make_unique<TestMarker>(dqGeom::Point3d::From(20, 40, 60), dqGeom::Point2d::From(30, 30));
    Cluster<TestMarker> cluster({a.get(), b.get(), c.get()});
    dqGeom::Point3d const loc = cluster.getClusterLocation();
    EXPECT_DOUBLE_EQ(loc.x, 10.0);
    EXPECT_DOUBLE_EQ(loc.y, 20.0);
    EXPECT_DOUBLE_EQ(loc.z, 30.0);
    // :334-336——组位置 = 首成员的 position。
    a->position = dqGeom::Point3d::From(7, 8, 9);
    EXPECT_DOUBLE_EQ(cluster.position().x, 7.0);
}

// ---------------------------------------------------------------------------
// MarkerSet.addDecoration（:427-493）
// ---------------------------------------------------------------------------

// Authored: 重叠聚类——同世界点的两 marker 聚为一组（绘制 = 组 marker 一枚
// 画布装饰），getClusterMarker 恰调一次；默认半径 = 首可见 marker 的
// max(size.x,size.y)×1.5（:449-453——30px → 45px，世界差经 ViewToWorld
// 自校准：44px 内聚、46px 分）。
TEST(MarkerTest, MarkerSetClustersOverlappingMarkers)
{
    CollectFixture f;
    TestMarkerSet set(f.holder.vp);

    auto const center = f.holder.vp->ViewToWorld(dqGeom::Point3d::From(320, 240, 0));
    auto const near44 = f.holder.vp->ViewToWorld(dqGeom::Point3d::From(320 + 44, 240, 0));
    set.markers().add(std::make_unique<TestMarker>(center, dqGeom::Point2d::From(30, 30)));
    set.markers().add(std::make_unique<TestMarker>(near44, dqGeom::Point2d::From(30, 30)));

    {
        auto context = f.makeContext();
        set.addDecoration(context);
        EXPECT_EQ(f.decorations.canvasDecorations.size(), 1u);   // 组 marker 一枚
        EXPECT_EQ(set.clusterMarkerCalls, 1);
        EXPECT_EQ(set.entriesCountForTest(), 1u);
    }

    // 46px 分离（> 45px 半径）→ 两枚独立绘制。
    f.decorations.clear();   // 前段产出清零——分段断言互不累加
    TestMarkerSet setFar(f.holder.vp);
    auto const near46 = atViewPixel(f.holder.vp, 320 + 46, 240);
    setFar.markers().add(std::make_unique<TestMarker>(center, dqGeom::Point2d::From(30, 30)));
    setFar.markers().add(std::make_unique<TestMarker>(near46, dqGeom::Point2d::From(30, 30)));
    {
        auto context = f.makeContext();
        setFar.addDecoration(context);
        EXPECT_EQ(f.decorations.canvasDecorations.size(), 2u);
        EXPECT_EQ(setFar.clusterMarkerCalls, 0);
        EXPECT_EQ(setFar.entriesCountForTest(), 2u);
    }
}

// Authored: minimumClusterSize 门（:476——组成员数 <= 门则逐成员绘制）。
TEST(MarkerTest, MarkerSetMinimumClusterSizeDrawsMembersIndividually)
{
    CollectFixture f;
    TestMarkerSet set(f.holder.vp);
    set.minimumClusterSize = 2;   // 2 成员的组不画组 marker

    auto const center = f.holder.vp->ViewToWorld(dqGeom::Point3d::From(320, 240, 0));
    set.markers().add(std::make_unique<TestMarker>(center, dqGeom::Point2d::From(30, 30)));
    set.markers().add(std::make_unique<TestMarker>(center, dqGeom::Point2d::From(30, 30)));

    auto context = f.makeContext();
    set.addDecoration(context);
    EXPECT_EQ(f.decorations.canvasDecorations.size(), 2u);   // 逐成员
    EXPECT_EQ(set.clusterMarkerCalls, 0);
    EXPECT_EQ(set.entriesCountForTest(), 1u);   // 条目仍是一组
}

// Authored: 视口门（:429-430——非本 MarkerSet 视口的 context 早退）。
TEST(MarkerTest, MarkerSetIgnoresForeignViewportContext)
{
    CollectFixture f;
    MarkerViewportFixture other = makeMarkerViewport();
    TestMarkerSet set(other.vp);   // 绑另一个视口

    auto const center = f.holder.vp->ViewToWorld(dqGeom::Point3d::From(320, 240, 0));
    set.markers().add(std::make_unique<TestMarker>(center, dqGeom::Point2d::From(30, 30)));

    auto context = f.makeContext();   // f.holder.vp 的 context
    set.addDecoration(context);
    EXPECT_TRUE(f.decorations.canvasDecorations.empty());
}

// Authored: entries 缓存（:434-439——worldToViewMap 不变不重建；组 marker
// 不重建 = 悬停身份存续的参考注释语义）+ markDirty（:405-407）强制重建。
TEST(MarkerTest, MarkerSetEntriesCachedUntilMarkDirty)
{
    CollectFixture f;
    TestMarkerSet set(f.holder.vp);
    auto const center = f.holder.vp->ViewToWorld(dqGeom::Point3d::From(320, 240, 0));
    set.markers().add(std::make_unique<TestMarker>(center, dqGeom::Point2d::From(30, 30)));
    set.markers().add(std::make_unique<TestMarker>(center, dqGeom::Point2d::From(30, 30)));

    {
        auto context = f.makeContext();
        set.addDecoration(context);
        EXPECT_EQ(set.clusterMarkerCalls, 1);
    }
    {
        auto context = f.makeContext();
        set.addDecoration(context);
        EXPECT_EQ(set.clusterMarkerCalls, 1);   // 缓存——未重建
    }
    set.markDirty();
    {
        auto context = f.makeContext();
        set.addDecoration(context);
        EXPECT_EQ(set.clusterMarkerCalls, 2);   // 脏标记 → 重建
    }
}

// ---------------------------------------------------------------------------
// ToolAdmin 画布装饰路由（ToolAdmin.ts:1036-1057 + 三调用点 + :2073 光圈门）
// ---------------------------------------------------------------------------
namespace {

// 装饰器：把 MarkerSet 挂上视口的装饰收集（EventDispatchTest 的 dispatch 链路
// 共用 ToolAdmin 实例 + 真视口装饰列表）。
struct MarkerDecorator : public IDecorator {
    TestMarkerSet* set = nullptr;
    explicit MarkerDecorator(TestMarkerSet* s) : set(s) {}
    void Decorate(DecorateContext& context) override { set->addDecoration(context); }
    bool TestDecorationHit(uint32_t) const override { return false; }
    QString GetDecorationToolTip(uint32_t) const override { return {}; }
};

// 路由夹具：视口 + 居中 marker + 装饰器注册 + 一次收集（装饰列表落位）。
// 全局 ToolAdmin 的状态（悬停/idle 工具）在本夹具析构恢复——dqAppTest 的
// Application 单例不经 Startup（无全局装饰器/idle 工具），保持零污染。
struct RouteFixture {
    MarkerViewportFixture holder;
    TestMarkerSet set;
    MarkerDecorator decorator;
    TestMarker* markerRaw = nullptr;
    InteractiveTool* savedIdle = nullptr;
    Viewport* savedSelection = nullptr;
    RouteFixture() : holder(makeMarkerViewport()), set(holder.vp), decorator(&set)
    {
        // addEvent 在事件循环未启动时丢事件（ToolAdmin.ts:794-795）——
        // EventDispatchTest 同款启动。
        Application::Get().StartEventLoop();
        ToolAdmin::clearQueue();
        // 视口选中前置（ToolAdmin.ts:1389-1390——首个 Data down 会
        // SetSelectedViewport(vp)，参考语义；ViewManager.ts:246-247 的首选中
        // 触发 StartDefaultTool 装默认工具）——先选中再清工具位，使测试态稳定
        //（免"首击装工具"污染断言）。
        savedSelection = Application::Get().GetViewManager().GetSelectedViewport();
        Application::Get().GetViewManager().SetSelectedViewport(holder.vp);
        auto const center = holder.vp->ViewToWorld(dqGeom::Point3d::From(320, 240, 0));
        markerRaw = set.markers().add(
            std::make_unique<TestMarker>(center, dqGeom::Point2d::From(30, 30)));
        Application::Get().GetViewManager().AddDecorator(&decorator);
        holder.vp->CollectDecorationsForTest();
        savedIdle = Application::Get().GetToolAdmin().GetIdleTool();
    }
    ~RouteFixture()
    {
        Application::Get().GetViewManager().DropDecorator(&decorator);
        // 清悬停（onMouseLeave = setCanvasDecoration(vp, undefined) 的公共入口）
        // + 恢复 idle 工具与视口选中 + 清空共享事件队列——跨测试零残留。
        Application::Get().GetToolAdmin().onMouseLeave(holder.vp);
        Application::Get().GetToolAdmin().setIdleTool(savedIdle);
        Application::Get().GetViewManager().SetSelectedViewport(savedSelection);
        ToolAdmin::clearQueue();
    }
    TestMarker* marker() const { return markerRaw; }
};

void driveMotion(Viewport* vp, float x, float y)
{
    ToolAdmin::addEvent({ToolEventType::MouseMove, vp, x, y});
    Application::Get().GetToolAdmin().processEvent();
}

}  // namespace

// Authored: 悬停路由（:1134-1141 → :1036-1048）——motion 到 marker 上：
// enter（hilite）+ 悬停身份 + 光标 pointer + 光圈门熄灭；移出：leave + 光标
// 复位 viewManager.cursor。
TEST(MarkerTest, ToolAdminHoverRoutesEnterLeaveCursorAndLocateGate)
{
    RouteFixture f;
    ToolAdmin& ta = Application::Get().GetToolAdmin();
    ASSERT_EQ(ta.GetCanvasDecorationMarkerForTest(), nullptr);

    ta.toolState.locateCircleOn = true;
    EXPECT_TRUE(ta.isLocateCircleOn());   // 无悬停——光圈面正常

    driveMotion(f.holder.vp, 320, 240);
    EXPECT_EQ(ta.GetCanvasDecorationMarkerForTest(), f.marker());
    EXPECT_TRUE(f.marker()->m_isHilited);                       // :1040 onMouseEnter
    EXPECT_EQ(f.holder.vp->cursor().shape(), Qt::PointingHandCursor);  // :1046 pointer
    EXPECT_FALSE(ta.isLocateCircleOn());   // :2073——悬停 canvas 装饰熄光圈

    // 同装饰内的重复 motion 不重发 enter（:1037 恒等短路）。
    f.marker()->m_isHilited = false;
    driveMotion(f.holder.vp, 321, 241);
    EXPECT_FALSE(f.marker()->m_isHilited);   // 未重发 enter（hilite 保持我们清的状态）
    EXPECT_EQ(ta.GetCanvasDecorationMarkerForTest(), f.marker());

    // 移出（空点）→ leave + 光标复位 + 光圈门恢复。
    driveMotion(f.holder.vp, 10, 10);
    EXPECT_EQ(ta.GetCanvasDecorationMarkerForTest(), nullptr);
    EXPECT_NE(f.holder.vp->cursor().shape(), Qt::PointingHandCursor);   // viewManager.cursor
    EXPECT_TRUE(ta.isLocateCircleOn());
    ta.toolState.locateCircleOn = false;
}

// Authored: 点击吞没（:1325-1327）——marker 上的 Data 键 down 由
// onMouseButton 消费，不进工具路由；空点则进 idle 工具。
TEST(MarkerTest, ToolAdminDataButtonOnMarkerSwallowedByOnMouseButton)
{
    RouteFixture f;
    ToolAdmin& ta = Application::Get().GetToolAdmin();
    IdleRecordingTool idle;
    ta.setIdleTool(&idle);
    InteractiveTool* const savedPrimitive = ta.GetPrimitiveTool();
    ta.setPrimitiveTool(nullptr);   // 隔离——空点击落点恒进 idle

    // 完整点击手势（down+up——参考 sendButtonEvent 两臂；marker 的
    // onMouseButton 恒 true（IncidentMarker.onMouseButton:40）→ 两臂皆吞）。
    ToolAdmin::addEvent({ToolEventType::MouseDown, f.holder.vp, 320, 240, BeButton::Data});
    ToolAdmin::addEvent({ToolEventType::MouseUp, f.holder.vp, 320, 240, BeButton::Data});
    ta.processEvent();
    ta.processEvent();
    EXPECT_EQ(f.marker()->mouseButtonCalls, 2);
    EXPECT_EQ(idle.dataDownCalls, 0);   // 吞没——工具路由未达

    ToolAdmin::addEvent({ToolEventType::MouseDown, f.holder.vp, 10, 10, BeButton::Data});
    ToolAdmin::addEvent({ToolEventType::MouseUp, f.holder.vp, 10, 10, BeButton::Data});
    ta.processEvent();
    ta.processEvent();
    EXPECT_EQ(f.marker()->mouseButtonCalls, 2);   // 空点不进 marker
    EXPECT_EQ(idle.dataDownCalls, 1);             // 进 idle 路由
    ta.setPrimitiveTool(savedPrimitive);
}

// Authored: XE14 析构缝——悬停中的 marker 销毁 → ToolAdmin 悬停指针清除
// （光标复位 + 装饰失效），后续 motion 不触死对象。
TEST(MarkerTest, MarkerDestroyedClearsToolAdminHover)
{
    RouteFixture f;
    ToolAdmin& ta = Application::Get().GetToolAdmin();
    driveMotion(f.holder.vp, 320, 240);
    ASSERT_EQ(ta.GetCanvasDecorationMarkerForTest(), f.marker());

    TestMarker* raw = f.marker();
    f.set.markers().erase(raw);   // unique_ptr 删除 → ~Marker → 析构缝
    EXPECT_EQ(ta.GetCanvasDecorationMarkerForTest(), nullptr);

    // 后续 motion 安全（悬垂拾取已被视口清扫 + 悬停指针已清）。
    driveMotion(f.holder.vp, 320, 240);
    driveMotion(f.holder.vp, 10, 10);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// DecorateContext 画布装饰缓存（ViewContext.ts:314-315 + restoreCache
// :248-250——M-X X-b1 接通。RED 实测：缓存腿缺席时二次收集丢画布条目）。
// ---------------------------------------------------------------------------
namespace {

struct CacheableMarkerDecorator : public IDecorator {
    TestMarkerSet* set = nullptr;
    int decorateCalls = 0;   // 缓存命中时 Decorate 不再被调（ViewContext.ts:222-226）
    explicit CacheableMarkerDecorator(TestMarkerSet* s) : set(s) {}
    bool UseCachedDecorations() const override { return true; }   // IncidentMarkerDemo.ts:206
    void Decorate(DecorateContext& context) override
    {
        ++decorateCalls;
        set->addDecoration(context);
    }
    bool TestDecorationHit(uint32_t) const override { return false; }
    QString GetDecorationToolTip(uint32_t) const override { return {}; }
};

}  // namespace

// Authored: cacheable decorator 的画布条目随缓存重放（二次收集 = 缓存命中
// 重放——Decorate 不再跑，画布条目经 RestoreCache :248-250 复原）。
TEST(MarkerTest, CanvasDecorationsReplayFromCacheOnSecondCollect)
{
    MarkerViewportFixture holder = makeMarkerViewport();
    TestMarkerSet set(holder.vp);
    auto const center = atViewPixel(holder.vp, 320, 240);
    set.markers().add(std::make_unique<TestMarker>(center, dqGeom::Point2d::From(30, 30)));
    CacheableMarkerDecorator decorator(&set);
    Application::Get().GetViewManager().AddDecorator(&decorator);

    holder.vp->CollectDecorationsForTest();
    EXPECT_EQ(holder.vp->GetDecorations().canvasDecorations.size(), 1u);
    EXPECT_EQ(decorator.decorateCalls, 1);
    // 二次收集 = 缓存命中重放（未经 InvalidateScene 清缓存）——Decorate 不重跑
    // 且画布条目仍在列（缓存腿缺席时本断言为 0 条[重放丢画布]）。
    holder.vp->CollectDecorationsForTest();
    EXPECT_EQ(decorator.decorateCalls, 1);
    EXPECT_EQ(holder.vp->GetDecorations().canvasDecorations.size(), 1u);

    Application::Get().GetViewManager().DropDecorator(&decorator);
}

// Authored: 终审 I-1（视口先亡向）机制锁——MarkerSet+marker 存活期直接析构
// 承载视口（不触发 OnChangeView；~Viewport 幂等 Shutdown 面 = 直接 delete 安全）：
// m_lastDecorateViewport/MarkerSet::m_viewport 换 QPointer 后随 QObject 析构
// 置空（GREEN 形态）；修复前裸指针悬垂非空——~Marker 反引用死 QWidget =
// teardown UAF（Debug 堆读多静默、崩溃非确定，故机制锁钉 QPointer 空转 +
// 析构路径跑通无崩，UB 面随承载类型结构性消除）。
TEST(MarkerTest, ViewportDeathBeforeMarkerShortCircuitsCleanup)
{
    MarkerViewportFixture f = makeMarkerViewport();
    {
        TestMarkerSet set(f.vp);
        auto const center = atViewPixel(f.vp, 320, 240);
        TestMarker* raw = set.markers().add(
            std::make_unique<TestMarker>(center, dqGeom::Point2d::From(30, 30)));
        {
            dqRender::Decorations decorations;
            dqApp::DecorationsCache cache;
            DecorateContext context(*f.vp, decorations, cache);
            set.addDecoration(context);   // addMarker → m_lastDecorateViewport = f.vp
        }
        ASSERT_NE(raw->m_lastDecorateViewport, nullptr);
        ASSERT_EQ(set.viewport(), f.vp);

        // 视口先亡：直接 delete（QObject 析构 → 全部 QPointer 置空）。
        delete f.vp;
        f.vp = nullptr;

        // 机制断言（GREEN 形态——修复前两指针悬垂非空，断言红）。
        EXPECT_EQ(raw->m_lastDecorateViewport, nullptr);
        EXPECT_EQ(set.viewport(), nullptr);

        // marker 析构（~Marker → 空指针门短路；集合析构同向）——无 UAF。
        set.markers().erase(raw);
    }
    ToolAdmin::clearQueue();
    SUCCEED();
}

// Authored: XE14 扩展清扫（修复轮 1 Important-1）——cacheable decorator 存活
// 期 markers().erase() 删 marker：~Marker 同点清扫视口活列表 +
// DecorationsCache 缓存条目；二次收集（缓存命中重放，Decorate 不重跑）不
// 复活死 marker 的函子（无悬垂 draw/pick 回调）。测试只读指针值做同一性
// 对账（死指针不解引用——RED 态安全）。
TEST(MarkerTest, MarkerDestroyedScrubsCachedCanvasEntries)
{
    MarkerViewportFixture holder = makeMarkerViewport();
    TestMarkerSet set(holder.vp);
    auto const center = atViewPixel(holder.vp, 320, 240);
    TestMarker* raw = set.markers().add(
        std::make_unique<TestMarker>(center, dqGeom::Point2d::From(30, 30)));
    CacheableMarkerDecorator decorator(&set);
    Application::Get().GetViewManager().AddDecorator(&decorator);

    // 首次收集：cacheable → 活列表一枚 + 缓存一枚（两份拷贝捕获同一 raw）。
    holder.vp->CollectDecorationsForTest();
    ASSERT_EQ(holder.vp->GetDecorations().canvasDecorations.size(), 1u);
    ASSERT_EQ(Marker::fromDecoration(holder.vp->GetDecorations().canvasDecorations[0]),
              raw);
    ASSERT_EQ(decorator.decorateCalls, 1);

    // erase → unique_ptr 删除 → ~Marker（活列表 + 缓存双清扫）。
    set.markers().erase(raw);
    EXPECT_TRUE(holder.vp->GetDecorations().canvasDecorations.empty());

    // 二次收集：缓存命中重放（Decorate 不重跑）——清扫后的缓存 bucket 不再
    // 携死 marker 条目（修复前：重放复活死条目 → size==1 + 死指针）。
    holder.vp->CollectDecorationsForTest();
    EXPECT_EQ(decorator.decorateCalls, 1);   // 缓存命中面（两态同）——判据在下行
    EXPECT_TRUE(holder.vp->GetDecorations().canvasDecorations.empty());

    Application::Get().GetViewManager().DropDecorator(&decorator);
    ToolAdmin::clearQueue();
}

// ---------------------------------------------------------------------------
// Marker::setImageUrl（Marker.ts:244——imageElementFromUrl 的 DanQing 承载；
// PNG 路径[qsvg 插件面的 SVG 装载锁在 X-b2 演示侧]）
// ---------------------------------------------------------------------------

// Authored: 装载既有 PNG 资产 → image 就位（纹理句柄 headless 为空[无 GL
// 管道——登记]，自然尺寸随图）。
TEST(MarkerTest, SetImageUrlLoadsPngAsset)
{
#ifndef DANQING_TEST_ASSET_ROOT
#define DANQING_TEST_ASSET_ROOT "."
#endif
    CollectFixture f;
    TestMarker marker(dqGeom::Point3d::From(0, 0, 0), dqGeom::Point2d::From(30, 30));
    marker.setImageUrl(*f.holder.vp,
                       std::string(DANQING_TEST_ASSET_ROOT) + "/dqCommon/tests/assets/red1x1.png");
    ASSERT_TRUE(marker.image.has_value());
    EXPECT_EQ(marker.image->width, 1u);
    EXPECT_EQ(marker.image->height, 1u);

    // 装载失败面（IncidentMarkerDemo.ts:164-169 的 catch——记日志不抛、image 缺席）。
    TestMarker missing(dqGeom::Point3d::From(0, 0, 0), dqGeom::Point2d::From(30, 30));
    missing.setImageUrl(*f.holder.vp, std::string(DANQING_TEST_ASSET_ROOT) + "/no-such-file.png");
    EXPECT_FALSE(missing.image.has_value());
}

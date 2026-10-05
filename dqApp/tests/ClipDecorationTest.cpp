// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp tests — ViewClipDecoration + EditManipulator（M-P P-F）
//
// Authored: no reference test exists in itwinjs-core for ViewClipDecoration /
//           EditManipulator（无对应 .test.ts；EditManipulator.test.ts 不存在，
//           ClipViewTool.test.ts 不存在——行为锚 = ClipViewTool.ts / EditManipulator.ts
//           各方法行号，断言场景取自参考实现的数据面语义）。
//
// 覆盖面（参考锚）：
//  - create/get/clear/toggle（:1966-1996）：hasClip 门 + 视口同一性 + 单例。
//  - ctor（:1335-1344）：getClipData → clipId → selectOnCreate 选集替换。
//  - createControls 选集门（:1535-1563）：clipId 选中（且无未知元素）才建
//    手柄；clearOnDeselect → 整装饰 clear。
//  - createClipShapeControls（:1438-1465）：numControls = 顶点数+1，前
//    numControls-2 为边中点面箭头（size ≤5 点 0.75），zLow/-Z、zHigh/+Z 蓝
//    填充（150,150,250）命名手柄。
//  - createClipPlanesControls（:1477-1533）：单面 → loop 质心单箭头 0.75。
//  - doClipPlaneNegate（:1587-1605）：cloneNegated 重建 planeSet → 视图
//    clip 内法向翻转。
//  - doClipPlaneClear（:1607-1635）：单面 → 整 clip 清 + onClearClip +
//    装饰 clear；多面 → 去该面。
//  - isClipShapeAlignedWithWorldUp（:1767-1798）：对齐判别 + extents 世界化。
//  - doClipShapeSetZExtents（:1800-1828）：世界 z 段 → 新 shape zLow/zHigh。
//  - ViewClipDecorationProvider（:2008-2073）：onNewClip/onNewClipPlane/
//    onModifyClip/onClearClip 四事件恰一次 + 装饰生命周期；onRightClick
//    无监听默认 negate（:2046-2047）。
//  - HandleUtils.adjustForBackgroundColor EQUIVALENCE 验证（§11.10——方向
//    断言：暗背景保持 / 亮背景压暗）。
#include <dqApp/BlankConnection.h>
#include <dqApp/ClipViewTool.h>
#include <dqApp/EditManipulator.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/SelectionSet.h>
#include <dqApp/ViewState.h>
#include <dqApp/Viewport.h>

#include <dqCommon/ColorDef.h>
#include <dqGeom/ClipPlane.h>
#include <dqGeom/ClipPrimitive.h>
#include <dqGeom/ClipVector.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <gtest/gtest.h>

#include <QString>
#include <vector>

namespace {

using dqGeom::ClipVector;
using namespace dqApp;

struct ClipDecoFixture {
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    std::unique_ptr<dqApp::Viewport> vp;

    ClipDecoFixture()
    {
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(dqGeom::Point3d::From(-100, -100, -100),
                                        dqGeom::Point3d::From(100, 100, 100));
        imodel = dqApp::BlankConnection::create(props);
        view = dqApp::SpatialViewState::CreateBlank(
            imodel.Get(), dqGeom::Point3d::From(0, 0, 0),
            dqGeom::Vector3d::From(200, 200, 200));
        vp.reset(dqApp::Viewport::Create(nullptr, view));
    }

    ~ClipDecoFixture()
    {
        ViewClipDecoration::clear();
        ViewClipDecorationProvider::clearProvider();
    }
};

}  // namespace

// Authored（create/get/clear/toggle :1966-1996 + ctor :1335-1344）
TEST(ClipDecorationTest, CreateRequiresClipAndSingletonPerViewport)
{
    ClipDecoFixture f;

    // 无 clip → nullopt（:1974-1975 hasClip 门）
    EXPECT_FALSE(ViewClipDecoration::create(*f.vp).has_value());

    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f.vp, dqGeom::Point3d::From(0, 0, 0),
                                            dqGeom::Vector3d::From(0, 0, 1), true));
    std::optional<uint32_t> const id = ViewClipDecoration::create(*f.vp);
    ASSERT_TRUE(id.has_value());
    EXPECT_NE(*id, 0u);

    // get：同视口命中；异视口 null（:1966-1970）
    ASSERT_NE(ViewClipDecoration::get(*f.vp), nullptr);
    dqBase::RefPtr<dqApp::SpatialViewState> view2 = dqApp::SpatialViewState::CreateBlank(
        f.imodel.Get(), dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(200, 200, 200));
    std::unique_ptr<dqApp::Viewport> vp2(dqApp::Viewport::Create(nullptr, view2));
    EXPECT_EQ(ViewClipDecoration::get(*vp2), nullptr);

    // 无 handler → 选集不含 clipId（selectOnCreate 默认 false）
    EXPECT_FALSE(f.imodel->GetSelectionSet().Contains(*id));

    // tooltip（:1850-1853——CoreTools.json tools.ViewClip.Message 解析值）
    EXPECT_EQ(ViewClipDecoration::get(*f.vp)->GetDecorationToolTip(*id),
              QString("View Clip"));

    // clear → get null（:1981-1986）
    ViewClipDecoration::clear();
    EXPECT_EQ(ViewClipDecoration::get(*f.vp), nullptr);
}

// Authored（createControls 选集门 :1535-1563 —— Provider selectOnCreate 路径）
TEST(ClipDecorationTest, ShapeControlsRequireSelectionAndClearOnDeselect)
{
    ClipDecoFixture f;
    ViewClipDecorationProvider& provider = ViewClipDecorationProvider::create();

    // 矩形 shape clip（Top 朝向——世界 up 对齐）
    std::vector<dqGeom::Point3d> const rect{
        dqGeom::Point3d::From(-50, -30, 0), dqGeom::Point3d::From(50, -30, 0),
        dqGeom::Point3d::From(50, 30, 0), dqGeom::Point3d::From(-50, 30, 0)};
    ASSERT_TRUE(ViewClipTool::doClipToShape(*f.vp, rect));

    // Provider.onNewClip → create + selectOnCreate 选集替换 → Synch → controls
    provider.onNewClip(*f.vp);
    ViewClipDecoration* deco = ViewClipDecoration::get(*f.vp);
    ASSERT_NE(deco, nullptr);
    EXPECT_TRUE(f.imodel->GetSelectionSet().Contains(deco->clipId()));

    // numControls = shapePtsLo.length + 1：多边形含闭合点（5 点）→ 4 边中点
    // （i < numControls-2，含闭合边）+ zLow + zHigh = 6。
    ASSERT_EQ(deco->controlIds().size(), 6u);
    ASSERT_EQ(deco->controls().size(), 6u);

    // 面 0：边 (p0→p1) 中点，法向 = edgeTangent × areaNormal（外向）。
    // CCW 矩形（+Z 面法向）：edge0 切向 +X → cross(+X,+Z) = -Y（外向）。
    ViewClipControlArrow const& face0 = deco->controls()[0];
    EXPECT_NEAR(face0.origin.x, 0.0, 1e-9);
    EXPECT_NEAR(face0.origin.y, -30.0, 1e-9);
    EXPECT_NEAR(face0.direction.x, 0.0, 1e-9);
    EXPECT_NEAR(face0.direction.y, -1.0, 1e-9);
    EXPECT_DOUBLE_EQ(face0.sizeInches, 0.75);  // 顶点数 ≤ 5

    // zLow（:1459-1460）：area 质心 + (0,0,-1)，蓝填充 + 命名。
    ViewClipControlArrow const& zLow = deco->controls()[4];
    EXPECT_EQ(zLow.name, "zLow");
    EXPECT_NEAR(zLow.origin.x, 0.0, 1e-9);
    EXPECT_NEAR(zLow.origin.y, 0.0, 1e-9);
    EXPECT_NEAR(zLow.direction.z, -1.0, 1e-9);
    ASSERT_TRUE(zLow.fill.has_value());
    EXPECT_EQ(zLow.fill->getTbgr(), dqCommon::ColorDef::from(150, 150, 250).getTbgr());

    // zHigh（:1461-1461）：质心 + unitZ×zSpan，+Z。
    ViewClipControlArrow const& zHigh = deco->controls()[5];
    EXPECT_EQ(zHigh.name, "zHigh");
    EXPECT_NEAR(zHigh.direction.z, 1.0, 1e-9);
    EXPECT_GT(zHigh.origin.z, zLow.origin.z);
    // zSpan = shapePtsLo[0].distance(shapePtsHi[0])（非 extents 差的等价面——
    // 无 transform 时二者同值）
    EXPECT_NEAR(zHigh.origin.z - zLow.origin.z,
                zHigh.origin.z - zLow.origin.z, 1e-12);

    // deselect（选集清空）→ createControls 门失败 + clearOnDeselect → 装饰 clear
    f.imodel->GetSelectionSet().Replace(QSet<uint32_t>{});
    EXPECT_EQ(ViewClipDecoration::get(*f.vp), nullptr);
}

// Authored（createClipPlanesControls :1477-1533 —— loop 质心单箭头）
TEST(ClipDecorationTest, PlaneControlsAtLoopCentroid)
{
    ClipDecoFixture f;
    ViewClipDecorationProvider& provider = ViewClipDecorationProvider::create();

    // 单平面 z=0、内法向 +Z：与视域盒 ±100 相交 → z=0 矩形 loop，质心 (0,0,0)。
    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f.vp, dqGeom::Point3d::From(0, 0, 0),
                                            dqGeom::Vector3d::From(0, 0, 1), true));
    provider.onNewClipPlane(*f.vp);
    ViewClipDecoration* deco = ViewClipDecoration::get(*f.vp);
    ASSERT_NE(deco, nullptr);

    ASSERT_EQ(deco->controls().size(), 1u);
    ViewClipControlArrow const& arrow = deco->controls()[0];
    EXPECT_NEAR(arrow.origin.x, 0.0, 1e-6);
    EXPECT_NEAR(arrow.origin.y, 0.0, 1e-6);
    EXPECT_NEAR(arrow.origin.z, 0.0, 1e-6);
    // 外向 = loop 面法向取负——z 轴单位向（符号随 loop 定向，x/y 分量为 0）。
    EXPECT_NEAR(arrow.direction.x, 0.0, 1e-9);
    EXPECT_NEAR(arrow.direction.y, 0.0, 1e-9);
    EXPECT_NEAR(std::abs(arrow.direction.z), 1.0, 1e-9);
    EXPECT_DOUBLE_EQ(arrow.sizeInches, 0.75);
    EXPECT_FALSE(arrow.fill.has_value());  // 贡献面无覆写色
}

// Authored（doClipPlaneNegate :1587-1605 —— cloneNegated 重建后视图 clip 翻转）
TEST(ClipDecorationTest, PlaneNegateFlipsInViewClip)
{
    ClipDecoFixture f;
    ViewClipDecorationProvider& provider = ViewClipDecorationProvider::create();

    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f.vp, dqGeom::Point3d::From(0, 0, 0),
                                            dqGeom::Vector3d::From(1, 0, 0), true));
    provider.onNewClipPlane(*f.vp);
    ViewClipDecoration* deco = ViewClipDecoration::get(*f.vp);
    ASSERT_NE(deco, nullptr);

    dqGeom::ClipPlane const before = *ViewClipTool::isSingleClipPlane(
        *f.view->getViewClip());
    ASSERT_TRUE(deco->doClipPlaneNegate(0));

    dqGeom::ClipPlane const* after = ViewClipTool::isSingleClipPlane(
        *f.view->getViewClip());
    ASSERT_NE(after, nullptr);
    // 内法向翻转（cloneNegated :103-107——法向取负 + 距离取负）
    EXPECT_NEAR(after->getPlane3d().getNormalRef().x, -before.getPlane3d().getNormalRef().x,
                1e-12);
    EXPECT_NEAR(after->getPlane3d().getOriginRef().x, before.getPlane3d().getOriginRef().x,
                1e-9);

    // 越界 index 拒绝（:1591-1593）
    EXPECT_FALSE(deco->doClipPlaneNegate(1));
    EXPECT_FALSE(deco->doClipPlaneNegate(-1));
}

// Authored（doClipPlaneClear :1607-1635 —— 单面整体清 / 多面去一面）
TEST(ClipDecorationTest, PlaneClearSingleClearsClipAndDecoration)
{
    ClipDecoFixture f;
    ViewClipDecorationProvider& provider = ViewClipDecorationProvider::create();
    int clearEvents = 0;
    dqBase::DqEventScope scope;
    scope.add(provider.onActiveClipChanged.AddListener(
        [&](Viewport&, ClipEventType type, ViewClipDecorationProvider*) {
            if (type == ClipEventType::Clear)
                clearEvents++;
        }));

    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f.vp, dqGeom::Point3d::From(0, 0, 0),
                                            dqGeom::Vector3d::From(0, 0, 1), true));
    provider.onNewClipPlane(*f.vp);
    ViewClipDecoration* deco = ViewClipDecoration::get(*f.vp);
    ASSERT_NE(deco, nullptr);

    // 单面 → doClipClear + handler onClearClip + 装饰 clear（:1612-1621）
    ASSERT_TRUE(deco->doClipPlaneClear(0));
    EXPECT_TRUE(f.view->getViewClip().IsNull());
    EXPECT_EQ(ViewClipDecoration::get(*f.vp), nullptr);
    EXPECT_EQ(clearEvents, 1);  // provider.onClearClip → Clear 事件
}

TEST(ClipDecorationTest, PlaneClearMultiRemovesOnePlane)
{
    ClipDecoFixture f;
    ViewClipDecorationProvider& provider = ViewClipDecorationProvider::create();

    // 两面剖切：z ≥ -10（追加重命名面）。
    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f.vp, dqGeom::Point3d::From(0, 0, -10),
                                            dqGeom::Vector3d::From(0, 0, 1), true));
    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f.vp, dqGeom::Point3d::From(0, 0, 10),
                                            dqGeom::Vector3d::From(0, 0, -1), false));
    provider.onNewClip(*f.vp);
    ViewClipDecoration* deco = ViewClipDecoration::get(*f.vp);
    ASSERT_NE(deco, nullptr);
    ASSERT_EQ(deco->clipPlaneSet()->planes.size(), 2u);

    ASSERT_TRUE(deco->doClipPlaneClear(1));
    dqGeom::ConvexClipPlaneSet const* set =
        ViewClipTool::isSingleConvexClipPlaneSet(*f.view->getViewClip());
    ASSERT_NE(set, nullptr);
    ASSERT_EQ(set->planes.size(), 1u);
    // 剩余面 = 面 0（z ≥ -10，内法向 +Z）
    EXPECT_NEAR(set->planes[0].getPlane3d().getNormalRef().z, 1.0, 1e-12);
}

// Authored（isClipShapeAlignedWithWorldUp :1767-1798）
TEST(ClipDecorationTest, ShapeAlignedWithWorldUpAndExtents)
{
    ClipDecoFixture f;
    ViewClipDecorationProvider& provider = ViewClipDecorationProvider::create();

    std::vector<dqGeom::Point3d> const rect{
        dqGeom::Point3d::From(-50, -30, 0), dqGeom::Point3d::From(50, -30, 0),
        dqGeom::Point3d::From(50, 30, 0), dqGeom::Point3d::From(-50, 30, 0)};
    ASSERT_TRUE(ViewClipTool::doClipToShape(*f.vp, rect, nullptr, -5.0, 10.0));
    provider.onNewClip(*f.vp);
    ViewClipDecoration* deco = ViewClipDecoration::get(*f.vp);
    ASSERT_NE(deco, nullptr);

    // 无 transform：局部 z 即世界 z（世界上面过原点法向 Z）——对齐成立。
    dqGeom::Range1d extents;
    EXPECT_TRUE(deco->isClipShapeAlignedWithWorldUp(&extents));
    EXPECT_NEAR(extents.low, -5.0, 1e-9);
    EXPECT_NEAR(extents.high, 10.0, 1e-9);

    // 非对齐：绕 X 转 90°（局部 +Z → 世界 +Y）→ false。
    dqGeom::Matrix3d const rot = dqGeom::Matrix3d::CreateRotationAroundAxis(
        dqGeom::Vector3d::From(1, 0, 0), dqGeom::Angle::kPiOver2);
    dqGeom::Transform const transform(dqGeom::Point3d::From(0, 0, 0), rot);
    ClipDecoFixture f2;
    ASSERT_TRUE(ViewClipTool::doClipToShape(*f2.vp, rect, &transform, -5.0, 10.0));
    provider.onNewClip(*f2.vp);
    ViewClipDecoration* deco2 = ViewClipDecoration::get(*f2.vp);
    ASSERT_NE(deco2, nullptr);
    EXPECT_FALSE(deco2->isClipShapeAlignedWithWorldUp(nullptr));
}

// Authored（doClipShapeSetZExtents :1800-1828 —— 世界 z 段写回 shape）
TEST(ClipDecorationTest, ShapeSetZExtentsRoundTrip)
{
    ClipDecoFixture f;
    ViewClipDecorationProvider& provider = ViewClipDecorationProvider::create();

    std::vector<dqGeom::Point3d> const rect{
        dqGeom::Point3d::From(-50, -30, 0), dqGeom::Point3d::From(50, -30, 0),
        dqGeom::Point3d::From(50, 30, 0), dqGeom::Point3d::From(-50, 30, 0)};
    ASSERT_TRUE(ViewClipTool::doClipToShape(*f.vp, rect, nullptr, -5.0, 10.0));
    provider.onNewClip(*f.vp);
    ViewClipDecoration* deco = ViewClipDecoration::get(*f.vp);
    ASSERT_NE(deco, nullptr);

    // 反序 extents 拒绝（:1801-1802——宿主输入域可达；createXX 恒归一[1:1
    // Range.ts:1195-1199 min/max]，故直构反序）
    dqGeom::Range1d reversed;
    reversed.low = 10.0;
    reversed.high = -5.0;
    EXPECT_FALSE(deco->doClipShapeSetZExtents(reversed));

    ASSERT_TRUE(deco->doClipShapeSetZExtents(dqGeom::Range1d::CreateXX(-20.0, 30.0)));
    dqGeom::ClipShape const* shape =
        ViewClipTool::isSingleClipShape(*f.view->getViewClip());
    ASSERT_NE(shape, nullptr);
    // 无 transform：世界 z 段直写（transformToClip 恒等）
    ASSERT_TRUE(shape->zLow().has_value());
    ASSERT_TRUE(shape->zHigh().has_value());
    EXPECT_NEAR(*shape->zLow(), -20.0, 1e-9);
    EXPECT_NEAR(*shape->zHigh(), 30.0, 1e-9);
}

// Authored（Provider 事件序 :2031-2048 —— 四态恰一次 + 装饰生命周期）
TEST(ClipDecorationTest, ProviderEventSequenceEachExactlyOnce)
{
    ClipDecoFixture f;
    ViewClipDecorationProvider& provider = ViewClipDecorationProvider::create();

    std::vector<ClipEventType> events;
    dqBase::DqEventScope scope;
    scope.add(provider.onActiveClipChanged.AddListener(
        [&events](Viewport&, ClipEventType type, ViewClipDecorationProvider*) {
            events.push_back(type);
        }));

    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f.vp, dqGeom::Point3d::From(0, 0, 0),
                                            dqGeom::Vector3d::From(0, 0, 1), true));
    provider.onNewClipPlane(*f.vp);
    EXPECT_TRUE(provider.isDecorationActive(*f.vp));

    provider.onModifyClip(*f.vp);
    provider.onClearClip(*f.vp);
    EXPECT_FALSE(provider.isDecorationActive(*f.vp));

    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0], ClipEventType::NewPlane);
    EXPECT_EQ(events[1], ClipEventType::Modify);
    EXPECT_EQ(events[2], ClipEventType::Clear);

    // onNewClip（非 plane 工具路径）→ New
    ASSERT_TRUE(ViewClipTool::doClipToShape(
        *f.vp, std::vector<dqGeom::Point3d>{dqGeom::Point3d::From(-10, -10, 0),
                                            dqGeom::Point3d::From(10, -10, 0),
                                            dqGeom::Point3d::From(10, 10, 0)}));
    provider.onNewClip(*f.vp);
    ASSERT_EQ(events.size(), 4u);
    EXPECT_EQ(events[3], ClipEventType::New);
    EXPECT_TRUE(provider.isDecorationActive(*f.vp));
}

// Authored（Provider.onRightClick :2044-2048 —— 无监听默认 negate）
TEST(ClipDecorationTest, ProviderRightClickDefaultsToNegate)
{
    ClipDecoFixture f;
    ViewClipDecorationProvider& provider = ViewClipDecorationProvider::create();

    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f.vp, dqGeom::Point3d::From(0, 0, 0),
                                            dqGeom::Vector3d::From(1, 0, 0), true));
    provider.onNewClipPlane(*f.vp);
    ViewClipDecoration* deco = ViewClipDecoration::get(*f.vp);
    ASSERT_NE(deco, nullptr);
    dqGeom::ClipPlane const before = *ViewClipTool::isSingleClipPlane(
        *f.view->getViewClip());

    // ev.viewport 指向装饰视口 + sourceId = 控制手柄 id → 默认 negate
    BeButtonEvent ev;
    ev.viewport = f.vp.get();
    uint32_t const controlId = deco->controlIds()[0];
    EXPECT_TRUE(provider.onRightClick(controlId, ev));
    dqGeom::ClipPlane const* after =
        ViewClipTool::isSingleClipPlane(*f.view->getViewClip());
    ASSERT_NE(after, nullptr);
    EXPECT_NEAR(after->getPlane3d().getNormalRef().x,
                -before.getPlane3d().getNormalRef().x, 1e-12);

    // 未知 sourceId → getControlIndex = -1 → negate 越界拒绝 → false
    EXPECT_FALSE(provider.onRightClick(0x1234567u, ev));

    // 有监听 → 交监听（不再 negate）
    double const flippedX = after->getPlane3d().getNormalRef().x;
    int heard = 0;
    dqBase::DqEventScope scope;
    scope.add(provider.onActiveClipRightClick.AddListener(
        [&heard](uint32_t, BeButtonEvent const&, ViewClipDecorationProvider*) {
            heard++;
        }));
    EXPECT_TRUE(provider.onRightClick(controlId, ev));
    EXPECT_EQ(heard, 1);
    dqGeom::ClipPlane const* after2 =
        ViewClipTool::isSingleClipPlane(*f.view->getViewClip());
    ASSERT_NE(after2, nullptr);
    EXPECT_NEAR(after2->getPlane3d().getNormalRef().x, flippedX, 1e-12);  // 未再翻转
}

// Authored（HandleUtils.adjustForBackgroundColor EQUIVALENCE 验证——
// §11.10 登记的验证法：方向断言。参考 :285-290 adjustedForContrast 的
// 亮度判别翻转承载。）
TEST(ClipDecorationTest, AdjustForBackgroundColorDirection)
{
    ClipDecoFixture f;
    dqCommon::ColorDef const white = dqCommon::ColorDef::from(255, 255, 255);

    // 暗背景（黑）→ 对比度足（luma 差 1.0 ≥ 0.5）→ 原色保持
    f.view->GetDisplayStyle().getSettings().setBackgroundColor(
        dqCommon::ColorDef::from(0, 0, 0));
    dqCommon::ColorDef const onDark =
        HandleUtils::adjustForBackgroundColor(white, *f.vp);
    EXPECT_EQ(onDark.getTbgr(), white.getTbgr());

    // 亮背景（白）→ 对比度不足（差 0）→ 压暗
    f.view->GetDisplayStyle().getSettings().setBackgroundColor(
        dqCommon::ColorDef::from(255, 255, 255));
    dqCommon::ColorDef const onLight =
        HandleUtils::adjustForBackgroundColor(white, *f.vp);
    EXPECT_EQ(onLight.getTbgr(),
              dqCommon::ColorDef::from(64, 64, 64).getTbgr());
}

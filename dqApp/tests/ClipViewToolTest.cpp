// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp tests — ViewClip 工具族
//
// Authored: no reference test exists in itwinjs-core for ViewClipTool
//           （无 ClipViewTool.test.ts；行为锚 = ClipViewTool.ts 各方法行号，
//           断言场景取自参考实现的数据面语义）。
//
// 覆盖面（参考锚）：
//  - doClipToPlane（:159-179）：首建 = 单 ClipPrimitive 包 ConvexClipPlaneSet 单面；
//    追加（clearExistingPlanes=false）= 同 planeSet 增至 2 面；clear=true = 重置 1 面。
//  - doClipToShape（:181-185）：ClipShape + zLow/zHigh + transform 保留。
//  - doClipToRange（:187-195）：块 mask=All；薄 Z（isAlmostZeroZ）→ XAndY；
//    null/薄 X/Y 拒绝。
//  - doClipClear（:196-200）：无 clip → false；有 → 清空 + false on repeat。
//  - isSingleClipShape/isSingleConvexClipPlaneSet/isSingleClipPlane（:345/:385/:396）。
//  - areClipsEqual（:404-440）。
//  - getPlaneInwardNormal 六世界朝向（:131-136——EQUIVALENCE 世界轴承载）。
//  - getClipRayTransformed/getOffsetValueTransformed（:203/:217）。
//  - getClipShapePoints/getClipShapeExtents（:312/:320——未设 z 段时从 viewRange 角点投影）。
//  - keyin 注册（ViewClip.Clear/ByPlane/ByShape/ByRange/ByElement）。
#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ClipViewTool.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>

#include <dqGeom/ClipPrimitive.h>
#include <dqGeom/ClipVector.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <gtest/gtest.h>

#include <vector>

namespace {

using dqGeom::ClipVector;
using namespace dqApp;

struct ClipToolFixture {
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    std::unique_ptr<dqApp::Viewport> vp;

    ClipToolFixture()
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
};

}  // namespace

// Authored（doClipToPlane :159-179）
TEST(ClipViewToolTest, DoClipToPlaneBuildsSinglePlaneSet)
{
    ClipToolFixture f;
    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f.vp, dqGeom::Point3d::From(0, 0, 0),
                                            dqGeom::Vector3d::From(0, 0, 1), true));
    ClipVector::Ptr const clip = f.view->getViewClip();
    ASSERT_FALSE(clip.IsNull());
    ASSERT_EQ(clip->clips().size(), 1u);
    // 非 ClipShape primitive + 单 planeSet + 单面
    ASSERT_EQ(clip->clips()[0]->asClipShape(), nullptr);
    dqGeom::ConvexClipPlaneSet const* set = ViewClipTool::isSingleConvexClipPlaneSet(*clip);
    ASSERT_NE(set, nullptr);
    ASSERT_EQ(set->planes.size(), 1u);
    // 面语义：过原点、法向 +Z（keep z ≥ 0）
    dqGeom::ClipPlane const* plane = ViewClipTool::isSingleClipPlane(*clip);
    ASSERT_NE(plane, nullptr);
    EXPECT_NEAR(plane->inwardNormal.DotProduct(dqGeom::Vector3d::From(0, 0, 1)), 1.0, 1e-12);
    EXPECT_NEAR(plane->altitude(dqGeom::Point3d::From(0, 0, 5)), 5.0, 1e-9);
}

// Authored（doClipToPlane :166-176——clearExistingPlanes=false 追加现存唯一 planeSet）
TEST(ClipViewToolTest, DoClipToPlaneAppendsToExistingPlaneSet)
{
    ClipToolFixture f;
    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f.vp, dqGeom::Point3d::From(0, 0, 0),
                                            dqGeom::Vector3d::From(0, 0, 1), true));
    // 追加第二面（clearExistingPlanes=false）
    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f.vp, dqGeom::Point3d::From(10, 0, 0),
                                            dqGeom::Vector3d::From(1, 0, 0), false));
    ClipVector::Ptr const clip = f.view->getViewClip();
    ASSERT_EQ(clip->clips().size(), 1u);  // 仍单 primitive（追加到现存 planeSet :168-176）
    dqGeom::ConvexClipPlaneSet const* set = ViewClipTool::isSingleConvexClipPlaneSet(*clip);
    ASSERT_NE(set, nullptr);
    ASSERT_EQ(set->planes.size(), 2u);  // 两面（半空间交）
    // clear=true 重置
    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f.vp, dqGeom::Point3d::From(0, 0, 0),
                                            dqGeom::Vector3d::From(0, 1, 0), true));
    set = ViewClipTool::isSingleConvexClipPlaneSet(*f.view->getViewClip());
    ASSERT_NE(set, nullptr);
    ASSERT_EQ(set->planes.size(), 1u);
}

// Authored（doClipToShape :181-185）
TEST(ClipViewToolTest, DoClipToShapeStoresZAndTransform)
{
    ClipToolFixture f;
    std::vector<dqGeom::Point3d> const shape = {
        dqGeom::Point3d::From(0, 0, 0), dqGeom::Point3d::From(10, 0, 0),
        dqGeom::Point3d::From(10, 10, 0), dqGeom::Point3d::From(0, 10, 0)};
    dqGeom::Transform const transform = dqGeom::Transform::CreateTranslation(5.0, 0.0, 0.0);
    ASSERT_TRUE(ViewClipTool::doClipToShape(*f.vp, shape, &transform, 1.0, 2.0));
    ClipVector::Ptr const clip = f.view->getViewClip();
    dqGeom::ClipShape const* clipShape = ViewClipTool::isSingleClipShape(*clip);
    ASSERT_NE(clipShape, nullptr);
    ASSERT_TRUE(clipShape->hasZLow());
    ASSERT_TRUE(clipShape->hasZHigh());
    EXPECT_DOUBLE_EQ(*clipShape->zLow(), 1.0);
    EXPECT_DOUBLE_EQ(*clipShape->zHigh(), 2.0);
    ASSERT_NE(clipShape->transformFromClip(), nullptr);
    EXPECT_NEAR(clipShape->transformFromClip()->origin.x, 5.0, 1e-12);
    // 参考怪癖 1:1：appendShape 对 <3 点返回 false 但 doClipToShape 不检查返回值
    // （:183 直接 setViewClip）→ 恒 true + 空 clip（isSingleClipShape null）。
    EXPECT_TRUE(ViewClipTool::doClipToShape(*f.vp,
                                            {dqGeom::Point3d::From(0, 0, 0),
                                             dqGeom::Point3d::From(1, 0, 0)},
                                            nullptr));
    // 空 clip：getViewClip() 对 invalid 返空（ViewDetails getter 语义）——
    // 判定面随之空。
    ClipVector::Ptr const after = f.view->getViewClip();
    EXPECT_TRUE(after.IsNull() || ViewClipTool::isSingleClipShape(*after) == nullptr);
}

// Authored（doClipToRange :187-195）
TEST(ClipViewToolTest, DoClipToRangeMaskBranches)
{
    ClipToolFixture f;
    dqGeom::Range3d const full = dqGeom::Range3d::CreateXYZXYZ(0, 0, 0, 10, 10, 10);
    ASSERT_TRUE(ViewClipTool::doClipToRange(*f.vp, full));
    dqGeom::ClipShape const* block = ViewClipTool::isSingleClipShape(*f.view->getViewClip());
    ASSERT_NE(block, nullptr);
    ASSERT_TRUE(block->hasZLow());  // mask=All → z 段携带
    ASSERT_TRUE(block->hasZHigh());

    // 薄 Z（isAlmostZeroZ）→ XAndY（无 z 面）
    dqGeom::Range3d const flat = dqGeom::Range3d::CreateXYZXYZ(0, 0, 5, 10, 10, 5);
    ASSERT_TRUE(ViewClipTool::doClipToRange(*f.vp, flat));
    dqGeom::ClipShape const* flatBlock = ViewClipTool::isSingleClipShape(*f.view->getViewClip());
    ASSERT_NE(flatBlock, nullptr);
    EXPECT_FALSE(flatBlock->hasZLow());
    EXPECT_FALSE(flatBlock->hasZHigh());

    // null / 薄 X / 薄 Y 拒绝（:188）
    EXPECT_FALSE(ViewClipTool::doClipToRange(*f.vp, dqGeom::Range3d::CreateNull()));
    // isAlmostZeroX/Y = XLength <= Geometry.smallMetricDistance(1e-6)。
    EXPECT_FALSE(ViewClipTool::doClipToRange(
        *f.vp, dqGeom::Range3d::CreateXYZXYZ(0, 0, 0, 0.0000005, 10, 10)));
    EXPECT_FALSE(ViewClipTool::doClipToRange(
        *f.vp, dqGeom::Range3d::CreateXYZXYZ(0, 0, 0, 10, 0.0000005, 10)));
}

// Authored（doClipClear :196-200）
TEST(ClipViewToolTest, DoClipClearSemantics)
{
    ClipToolFixture f;
    EXPECT_FALSE(ViewClipTool::doClipClear(*f.vp));  // 无 clip → false（:198）
    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f.vp, dqGeom::Point3d::From(0, 0, 0),
                                            dqGeom::Vector3d::From(0, 0, 1), true));
    ASSERT_TRUE(ViewClipTool::doClipClear(*f.vp));
    EXPECT_FALSE(f.view->getViewClip().IsValid());  // 清空
    EXPECT_FALSE(ViewClipTool::doClipClear(*f.vp));  // 重复清 → false
}

// Authored（areClipsEqual :404-440 + 判定面 :345/:385/:396）
TEST(ClipViewToolTest, AreClipsEqualAndPredicates)
{
    ClipToolFixture f;
    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f.vp, dqGeom::Point3d::From(0, 0, 0),
                                            dqGeom::Vector3d::From(0, 0, 1), true));
    ClipVector::Ptr const a = f.view->getViewClip();
    // 同对象
    EXPECT_TRUE(ViewClipTool::areClipsEqual(*a, *a));
    // clone 等价
    ClipVector::Ptr const aClone = a->clone();
    EXPECT_TRUE(ViewClipTool::areClipsEqual(*a, *aClone));
    // 不同 clip
    ClipToolFixture f2;
    ASSERT_TRUE(ViewClipTool::doClipToPlane(*f2.vp, dqGeom::Point3d::From(0, 0, 0),
                                            dqGeom::Vector3d::From(0, 1, 0), true));
    EXPECT_FALSE(ViewClipTool::areClipsEqual(*a, *f2.view->getViewClip()));
    // shape ≠ planeSet（isSingleClipShape null / isSingleConvexClipPlaneSet null）
    ASSERT_TRUE(ViewClipTool::doClipToRange(
        *f.vp, dqGeom::Range3d::CreateXYZXYZ(0, 0, 0, 10, 10, 10)));
    ClipVector::Ptr const shapeClip = f.view->getViewClip();
    EXPECT_NE(ViewClipTool::isSingleClipShape(*shapeClip), nullptr);
    EXPECT_EQ(ViewClipTool::isSingleConvexClipPlaneSet(*shapeClip), nullptr);
}

// Authored（getPlaneInwardNormal :131-136——EQUIVALENCE 世界轴承载）
TEST(ClipViewToolTest, PlaneInwardNormalSixOrientations)
{
    ClipToolFixture f;
    auto expectNormal = [&](ContextRotationId o, double x, double y, double z) {
        auto const n = ViewClipTool::getPlaneInwardNormal(o, *f.vp);
        ASSERT_TRUE(n.has_value());
        EXPECT_NEAR(n->x, x, 1e-12);
        EXPECT_NEAR(n->y, y, 1e-12);
        EXPECT_NEAR(n->z, z, 1e-12);
    };
    expectNormal(ContextRotationId::Top, 0, 0, -1);
    expectNormal(ContextRotationId::Bottom, 0, 0, 1);
    expectNormal(ContextRotationId::Front, 0, -1, 0);
    expectNormal(ContextRotationId::Back, 0, 1, 0);
    expectNormal(ContextRotationId::Left, -1, 0, 0);
    expectNormal(ContextRotationId::Right, 1, 0, 0);
    // View/Face：视图 Z 列负（Top 视图 = -Z 朝下 → 内法向 +Z）
    auto const viewN = ViewClipTool::getPlaneInwardNormal(ContextRotationId::View, *f.vp);
    ASSERT_TRUE(viewN.has_value());
    EXPECT_NEAR(std::abs(viewN->z), 1.0, 1e-9);
}

// Authored（getClipRayTransformed :203-215 / getOffsetValueTransformed :217-227）
TEST(ClipViewToolTest, RayAndOffsetTransform)
{
    dqGeom::Transform const t = dqGeom::Transform::CreateTranslation(10, 0, 0);
    dqGeom::Ray3d const ray =
        ViewClipTool::getClipRayTransformed(dqGeom::Point3d::From(1, 0, 0),
                                            dqGeom::Vector3d::From(0, 2, 0), &t);
    EXPECT_NEAR(ray.origin.x, 11.0, 1e-12);
    EXPECT_NEAR(ray.direction.y, 1.0, 1e-12);  // normalizeInPlace

    EXPECT_DOUBLE_EQ(ViewClipTool::getOffsetValueTransformed(3.0), 3.0);
    dqGeom::Transform const constScale =
        dqGeom::Transform(dqGeom::Point3d::From(0, 0, 0),
                          dqGeom::Matrix3d::CreateRowValues(2, 0, 0, 0, 2, 0, 0, 0, 2));
    EXPECT_DOUBLE_EQ(ViewClipTool::getOffsetValueTransformed(1.5, &constScale), 3.0);
    EXPECT_DOUBLE_EQ(ViewClipTool::getOffsetValueTransformed(-1.5, &constScale), -3.0);
}

// Authored（getClipShapePoints :312-318 / getClipShapeExtents :320-343）
TEST(ClipViewToolTest, ShapePointsAndExtents)
{
    ClipToolFixture f;
    ASSERT_TRUE(ViewClipTool::doClipToShape(
        *f.vp, {dqGeom::Point3d::From(0, 0, 0), dqGeom::Point3d::From(10, 0, 0),
                dqGeom::Point3d::From(10, 10, 0), dqGeom::Point3d::From(0, 10, 0)},
        nullptr, 1.0, 2.0));
    dqGeom::ClipShape const* shape = ViewClipTool::isSingleClipShape(*f.view->getViewClip());
    ASSERT_NE(shape, nullptr);

    std::vector<dqGeom::Point3d> const pts = ViewClipTool::getClipShapePoints(*shape, 7.0);
    ASSERT_EQ(pts.size(), shape->polygon().size());
    for (dqGeom::Point3d const& p : pts)
        EXPECT_DOUBLE_EQ(p.z, 7.0);

    // z 段已设 → extents 直取
    dqGeom::Range1d const ext = ViewClipTool::getClipShapeExtents(
        *shape, dqGeom::Range3d::CreateXYZXYZ(-100, -100, -100, 100, 100, 100));
    EXPECT_DOUBLE_EQ(ext.low, 1.0);
    EXPECT_DOUBLE_EQ(ext.high, 2.0);

    // z 段未设 → 从 viewRange 角点沿 z 投影推导（:329-341）
    ASSERT_TRUE(ViewClipTool::doClipToShape(
        *f.vp, {dqGeom::Point3d::From(0, 0, 0), dqGeom::Point3d::From(10, 0, 0),
                dqGeom::Point3d::From(10, 10, 0), dqGeom::Point3d::From(0, 10, 0)},
        nullptr));
    dqGeom::ClipShape const* open = ViewClipTool::isSingleClipShape(*f.view->getViewClip());
    ASSERT_NE(open, nullptr);
    dqGeom::Range1d const derived = ViewClipTool::getClipShapeExtents(
        *open, dqGeom::Range3d::CreateXYZXYZ(-10, -10, -10, 10, 10, 30));
    EXPECT_DOUBLE_EQ(derived.low, -10.0);
    EXPECT_DOUBLE_EQ(derived.high, 30.0);
}

// Authored（keyin 注册面——IModelApp.ts:438-446 registerModule 语义；
// 注册在 ToolAdmin::OnInitialized——MeasureToolTest 同款启动）
TEST(ClipViewToolTest, KeyinRegistration)
{
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();
    auto const& registry = admin.GetRegistry();
    for (char const* keyin :
         {"ViewClip.Clear", "ViewClip.ByPlane", "ViewClip.ByShape", "ViewClip.ByRange",
          "ViewClip.ByElement"}) {
        EXPECT_NE(registry.Find(keyin), nullptr) << keyin << " not registered";
    }
}

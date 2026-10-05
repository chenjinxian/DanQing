// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp tests — ViewState clipVector（ViewDetails.clip 承载）
//
// Ported from: itwinjs-core core/common/src/test/ViewDetails.test.ts
//              describe("ViewDetails").describe("clipVector")
//
// §5(e) 映射：参考的 TestDetails 子类（暴露 _clipVector/_json 内部态）→
// DanQing ViewState 就地承载（grid settings 同先例）：
//  - storedClipProps → ToProps().viewDetailsProps.clip（序列化对偶）；
//  - storedClip 的"已分配但无效"内部态 → 经 getter 返 null + props 无 clip 段联合断言；
//  - "getter allocates on first call" 的内部分配态无公开观测面 → 以
//    props 载入 + getter 物化 + round-trip 等价断言承载（EQUIVALENCE 登记）。
// 场景与断言值 1:1 取自参考。
#include <dqApp/BlankConnection.h>
#include <dqApp/ViewState.h>
#include <dqApp/ViewStateProps.h>
#include <dqApp/Viewport.h>

#include <dqGeom/ClipPrimitive.h>
#include <dqGeom/ClipVector.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Vector3d.h>

#include <gtest/gtest.h>

#include <optional>
#include <vector>

namespace {

using dqGeom::ClipPrimitiveProps;
using dqGeom::ClipShapeTransformProps;
using dqGeom::ClipVector;
using dqGeom::ClipVectorProps;

// 参考夹具的 clipProps（ViewDetails.test.ts:34-40）。
ClipVectorProps makeClipProps()
{
    ClipVectorProps props;
    ClipPrimitiveProps& prim = props.emplace_back();
    dqGeom::ClipPrimitiveShapePart& shape = prim.shape.emplace();
    shape.points = {dqGeom::Point3d::From(0, 0, 0), dqGeom::Point3d::From(1, 0, 0),
                    dqGeom::Point3d::From(1, 1, 0), dqGeom::Point3d::From(0, 0, 0)};
    return props;
}

bool clipPropsEqual(ClipVectorProps const& a, ClipVectorProps const& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].shape.has_value() != b[i].shape.has_value())
            return false;
        if (!a[i].shape.has_value())
            continue;
        auto const& sa = *a[i].shape;
        auto const& sb = *b[i].shape;
        if (sa.points.size() != sb.points.size())
            return false;
        for (size_t k = 0; k < sa.points.size(); ++k)
            if (!sa.points[k].AlmostEqual(sb.points[k], 1e-12))
                return false;
        if (sa.mask.has_value() != sb.mask.has_value() || sa.invisible.has_value() != sb.invisible.has_value())
            return false;
        if (sa.zlow.has_value() != sb.zlow.has_value() || sa.zhigh.has_value() != sb.zhigh.has_value())
            return false;
    }
    return true;
}

struct ClipFixture {
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    dqBase::RefPtr<dqApp::SpatialViewState> view;

    ClipFixture()
    {
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(dqGeom::Point3d::From(-100, -100, -100),
                                        dqGeom::Point3d::From(100, 100, 100));
        imodel = dqApp::BlankConnection::create(props);
        view = dqApp::SpatialViewState::CreateBlank(
            imodel.Get(), dqGeom::Point3d::From(0, 0, 0),
            dqGeom::Vector3d::From(200, 200, 200));
    }
};

}  // namespace

// Ported from: ViewDetails.test.ts:42-59 ("should keep in sync with JSON")
TEST(ViewClipStateTest, KeepsInSyncWithProps)
{
    ClipVectorProps const clipProps = makeClipProps();

    {
        ClipFixture f;
        EXPECT_FALSE(f.view->getViewClip());                       // undefined
        EXPECT_FALSE(f.view->ToProps().viewDetailsProps.clip.has_value());  // storedClipProps undefined

        ClipVector::Ptr const clip = ClipVector::fromJSON(&clipProps);
        f.view->setViewClip(clip);
        auto const& stored = f.view->ToProps().viewDetailsProps.clip;
        ASSERT_TRUE(stored.has_value());
        EXPECT_TRUE(clipPropsEqual(*stored, clipProps));           // deep equal
        EXPECT_EQ(f.view->getViewClip().Get(), clip.Get());        // same object
    }

    // new TestDetails(clipProps)：props 载入 → 存储保持
    {
        dqApp::ViewStateProps props;
        props.viewDetailsProps.clip = clipProps;
        dqBase::RefPtr<dqApp::SpatialViewState> const loaded =
            dqApp::SpatialViewState::CreateFromProps(props, nullptr);
        ASSERT_TRUE(loaded);
        auto const& stored = loaded->ToProps().viewDetailsProps.clip;
        ASSERT_TRUE(stored.has_value());
        EXPECT_TRUE(clipPropsEqual(*stored, clipProps));

        loaded->setViewClip(nullptr);                              // clipVector = undefined
        EXPECT_FALSE(loaded->ToProps().viewDetailsProps.clip.has_value());
        EXPECT_FALSE(loaded->getViewClip());
    }
}

// Ported from: ViewDetails.test.ts:61-77 ("should raise event when changed")
TEST(ViewClipStateTest, RaisesEventWhenChanged)
{
    ClipVectorProps const clipProps = makeClipProps();
    ClipFixture f;
    int raised = 0;
    auto scope = f.view->OnClipVectorChanged.AddListener([&raised]() { ++raised; });

    f.view->setViewClip(ClipVector::fromJSON(&clipProps));
    EXPECT_EQ(raised, 1);

    f.view->setViewClip(nullptr);
    EXPECT_EQ(raised, 2);

    f.view->setViewClip(ClipVector::fromJSON(&clipProps));
    EXPECT_EQ(raised, 3);

    f.view->setViewClip(ClipVector::createEmpty());
    EXPECT_EQ(raised, 4);
}

// Ported from: ViewDetails.test.ts:79-89 ("treats empty and undefined as equivalent")
TEST(ViewClipStateTest, TreatsEmptyAndUndefinedAsEquivalent)
{
    ClipFixture f;
    EXPECT_FALSE(f.view->getViewClip());

    f.view->setViewClip(ClipVector::createEmpty());
    EXPECT_FALSE(f.view->getViewClip());              // getter → undefined for invalid
    // storedClip 已分配但无效（无公开观测面——由 props 无 clip 段 + getter null 联合承载）
    EXPECT_FALSE(f.view->ToProps().viewDetailsProps.clip.has_value());
}

// Ported from: ViewDetails.test.ts:91-116 ("should do nothing if equivalent clip is assigned")
TEST(ViewClipStateTest, DoesNothingIfEquivalentClipAssigned)
{
    ClipVectorProps const clipProps = makeClipProps();
    ClipFixture f;
    int raised = 0;
    auto scope = f.view->OnClipVectorChanged.AddListener([&raised]() { ++raised; });

    f.view->setViewClip(ClipVector::createEmpty());
    EXPECT_EQ(raised, 0);                             // empty-on-empty → no event

    f.view->setViewClip(nullptr);
    EXPECT_EQ(raised, 0);                             // undefined-on-empty → no event

    ClipVector::Ptr const clip = ClipVector::fromJSON(&clipProps);
    f.view->setViewClip(clip);
    EXPECT_EQ(raised, 1);

    f.view->setViewClip(clip);                        // same object → no event
    EXPECT_EQ(raised, 1);

    f.view->setViewClip(clip->clone());               // different object (clone) → event
    EXPECT_EQ(raised, 2);

    f.view->setViewClip(nullptr);
    EXPECT_EQ(raised, 3);

    f.view->setViewClip(nullptr);                     // undefined again → no event
    EXPECT_EQ(raised, 3);
}

// Ported from: ViewDetails.test.ts:118-123 ("does not save empty clip vector in JSON")
TEST(ViewClipStateTest, DoesNotSaveEmptyClipVectorInProps)
{
    ClipFixture f;
    f.view->setViewClip(ClipVector::createEmpty());
    EXPECT_FALSE(f.view->getViewClip());
    EXPECT_FALSE(f.view->ToProps().viewDetailsProps.clip.has_value());
}

// Ported from: ViewDetails.test.ts:125-132 ("getter allocates on first call")
// EQUIVALENCE：内部分配态无公开观测面——props 载入后 getter 物化 + 内容断言承载。
TEST(ViewClipStateTest, GetterMaterializesFromProps)
{
    ClipVectorProps const clipProps = makeClipProps();
    dqApp::ViewStateProps props;
    props.viewDetailsProps.clip = clipProps;
    dqBase::RefPtr<dqApp::SpatialViewState> const loaded =
        dqApp::SpatialViewState::CreateFromProps(props, nullptr);
    ASSERT_TRUE(loaded);

    ClipVector::Ptr const clip = loaded->getViewClip();
    ASSERT_TRUE(clip);
    ASSERT_EQ(clip->clips().size(), 1u);
    ASSERT_NE(clip->clips()[0]->asClipShape(), nullptr);
    EXPECT_EQ(clip->clips()[0]->asClipShape()->polygon().size(), 4u);
}

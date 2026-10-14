// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp tests — ViewState aspectRatioSkew（ViewDetails.aspectSkew 就地承载）
//
// 行为锚定：itwinjs-core ViewDetails.ts:95-104（maxSkew=25 静态 + getter
// JsonUtils.asDouble(_json.aspectSkew, 1.0) + Geometry.clamp(1/maxSkew, maxSkew)
// + setter 1.0 置 undefined）× ViewState.ts:971-977（getAspectRatioSkew/
// setAspectRatioSkew 直通 details）× ViewState.ts:871（fixAspectRatio 消费）。
//
// Authored: no reference test exists in itwinjs-core for aspectRatioSkew
//           （core/common/src/test/ViewDetails.test.ts 无 aspectSkew 用例——
//           M-P P-B 移植面确认；场景按参考实现语义构造，§5(f) 标注）。
//
// RED（W-p1 落地前）：getAspectRatioSkew 恒 1.0（ViewState.h 硬编码）且
// setAspectRatioSkew 不存在——编译期缺 API 红 + ClampBounds 行为红。
#include <gtest/gtest.h>

#include <dqApp/BlankConnection.h>
#include <dqApp/ViewState.h>
#include <dqApp/ViewStateProps.h>

#include <dqCommon/ViewDetails.h>

namespace {

struct SkewFixture {
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    dqBase::RefPtr<dqApp::SpatialViewState> view;

    SkewFixture()
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

// maxSkew 可变静态的测试后恢复守卫（ViewDetails.ts:95 语义——app 可覆写）。
struct MaxSkewGuard {
    double saved = dqCommon::ViewDetails::maxSkew;
    ~MaxSkewGuard() { dqCommon::ViewDetails::maxSkew = saved; }
};

}  // namespace

// 默认 1.0；clamp 上界=maxSkew(25)、下界=1/maxSkew；负值同 clamp（参考无特判）。
// 锚：ViewDetails.ts:97-101。
TEST(ViewAspectRatioSkewTest, DefaultOneAndClampBounds)
{
    SkewFixture f;
    EXPECT_DOUBLE_EQ(1.0, f.view->getAspectRatioSkew());

    f.view->setAspectRatioSkew(100.0);
    EXPECT_DOUBLE_EQ(25.0, f.view->getAspectRatioSkew());   // clamp 上界 = maxSkew

    f.view->setAspectRatioSkew(0.01);
    EXPECT_DOUBLE_EQ(1.0 / 25.0, f.view->getAspectRatioSkew());  // clamp 下界

    f.view->setAspectRatioSkew(-5.0);
    EXPECT_DOUBLE_EQ(1.0 / 25.0, f.view->getAspectRatioSkew());  // 负值同 clamp

    f.view->setAspectRatioSkew(2.5);
    EXPECT_DOUBLE_EQ(2.5, f.view->getAspectRatioSkew());    // 域内原样
}

// setter 语义：1.0 → props 不写出（undefined）；非 1.0 → 写出。
// 锚：ViewDetails.ts:102-104。
TEST(ViewAspectRatioSkewTest, SetterOmitsOneFromProps)
{
    SkewFixture f;
    EXPECT_FALSE(f.view->ToProps().viewDetailsProps.aspectSkew.has_value());

    f.view->setAspectRatioSkew(2.5);
    auto props = f.view->ToProps();
    ASSERT_TRUE(props.viewDetailsProps.aspectSkew.has_value());
    EXPECT_DOUBLE_EQ(2.5, *props.viewDetailsProps.aspectSkew);

    f.view->setAspectRatioSkew(1.0);
    EXPECT_FALSE(f.view->ToProps().viewDetailsProps.aspectSkew.has_value());
}

// props 往返 + clone 携带（参考 _json 随 toProps/clone 原样保留的等价面）。
TEST(ViewAspectRatioSkewTest, PropsRoundTripAndClone)
{
    SkewFixture f;
    f.view->setAspectRatioSkew(2.5);

    auto props = f.view->ToProps();
    auto revived = dqApp::SpatialViewState::CreateFromProps(props, f.imodel.Get());
    ASSERT_TRUE(revived);
    EXPECT_DOUBLE_EQ(2.5, revived->getAspectRatioSkew());

    auto clone = f.view->Clone();
    ASSERT_TRUE(clone);
    EXPECT_DOUBLE_EQ(2.5, clone->getAspectRatioSkew());
}

// fixAspectRatio 消费：extents.y = extents.x / (windowAspect × skew)
// （ViewState.ts:871——清偿 ViewState.cpp 的 1.0 硬编码移植缺口）。
TEST(ViewAspectRatioSkewTest, FixAspectRatioConsumesSkew)
{
    SkewFixture f;  // extents (200,200,200)
    f.view->FixAspectRatio(1.0f);
    EXPECT_NEAR(200.0, f.view->GetExtents().y, 1e-9);   // skew 缺席=1.0

    f.view->setAspectRatioSkew(4.0);
    f.view->FixAspectRatio(1.0f);
    EXPECT_NEAR(50.0, f.view->GetExtents().y, 1e-9);    // 200 / (1×4)
}

// maxSkew 覆写放宽 clamp（ViewDetails.ts:95 "Apps can override"——
// AspectRatioSkewDecorator.ts:43 的 maxSkew=1000 置位路径）。
TEST(ViewAspectRatioSkewTest, MaxSkewOverrideWidensClamp)
{
    MaxSkewGuard guard;
    dqCommon::ViewDetails::maxSkew = 1000.0;

    SkewFixture f;
    f.view->setAspectRatioSkew(100.0);
    EXPECT_DOUBLE_EQ(100.0, f.view->getAspectRatioSkew());

    f.view->setAspectRatioSkew(2000.0);
    EXPECT_DOUBLE_EQ(1000.0, f.view->getAspectRatioSkew());
}

// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — LookAt tests
// Ported from: itwinjs-core core/frontend/src/test/LookAt.test.ts
//              describe("Look At")
//
// 覆盖两组：lookAtViewAlignedVolume（dilation/marginPercent/paddingPercent/
// 优先级 + aspect 调整）与 lookAtGlobalLocation（should change camera）。
// 场景与断言值全部 1:1 来自参考（含 MarginPercent "百分比并不精确"的怪癖）。
//
// 本文件取代 D3.1 时期的部分移植（同源参考的 aspect=1.0 方形子集；当时
// adjustViewDelta 的 aspect 分支未接线，非方形 delta 用例被显式延后）——
// 现全量移植，含 aspect=0.5/2.0 与 {right:0.5}/{left:1} 等非方形用例。
#include <gtest/gtest.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/MarginOptions.h>
#include <dqApp/StandardView.h>
#include <dqApp/ViewState.h>
#include <dqApp/ViewStatus.h>
#include <dqCommon/Cartographic.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Vector3d.h>

#include <array>
#include <cmath>
#include <optional>

// Ported from: itwinjs-core LookAt.test.ts beforeAll —— blank connection + Exton
// 以南的 ECEF 定位（latitude 39.144703, longitude -75.703054）。
namespace {
dqBase::RefPtr<dqApp::IModelConnection> makeLookAtIModel()
{
    dqApp::BlankConnectionProps props;
    props.locationIsCartographic = true;
    props.cartographicLocation =
        dqCommon::Cartographic::fromDegrees(-75.703054, 39.144703, 0.0);
    return dqApp::BlankConnection::create(props);
}
}  // namespace

// Ported from: itwinjs-core LookAt.test.ts
//              describe("lookAtViewAlignedVolume") / createTopView
static dqBase::RefPtr<dqApp::SpatialViewState> createTopView(dqApp::IModelConnection* imodel)
{
    // SpatialViewState.createBlank(imodel, new Point3d(), new Point3d())
    auto view = dqApp::SpatialViewState::CreateBlank(
        imodel, dqGeom::Point3d(0, 0, 0), dqGeom::Vector3d(0, 0, 0));
    view->SetStandardView(static_cast<int>(dqApp::StandardViewId::Top));
    EXPECT_FALSE(view->IsCameraOn());
    return view;
}

// Ported from: itwinjs-core LookAt.test.ts
//              describe("lookAtViewAlignedVolume") / expectExtents(volume, expected, options, aspect)
static void expectExtentsImpl(dqApp::IModelConnection* imodel, double const (&volume)[4],
                              std::array<int, 4> const& expected,
                              dqApp::MarginOptions const* options, double aspect)
{
    auto view = createTopView(imodel);
    dqGeom::Range3d range(volume[0], volume[1], -1, volume[2], volume[3], 1);
    view->LookAtVolume(range, &aspect, options);

    auto const delta = view->GetExtents();
    auto const origin = view->GetOrigin();
    std::array<int, 4> const actual = {
        static_cast<int>(std::lround(origin.x)),
        static_cast<int>(std::lround(origin.y)),
        static_cast<int>(std::lround(delta.x)),
        static_cast<int>(std::lround(delta.y)),
    };
    EXPECT_EQ(actual, expected);
}

static void expectExtents(dqApp::IModelConnection* imodel, double const (&volume)[4],
                          std::array<int, 4> const& expected, double aspect = 1.0)
{
    expectExtentsImpl(imodel, volume, expected, nullptr, aspect);
}

static void expectExtents(dqApp::IModelConnection* imodel, double const (&volume)[4],
                          std::array<int, 4> const& expected,
                          dqApp::MarginOptions const& options, double aspect = 1.0)
{
    expectExtentsImpl(imodel, volume, expected, &options, aspect);
}

// ============================================================================
// lookAtViewAlignedVolume — applies default dilation of 1.04
// Ported from: itwinjs-core LookAt.test.ts
//              it("applies default dilation of 1.04")
// ============================================================================
TEST(LookAtViewAlignedVolumeTest, AppliesDefaultDilation)
{
    auto imodel = makeLookAtIModel();
    ASSERT_NE(imodel.Get(), nullptr);
    expectExtents(imodel.Get(), {0, 0, 100, 100}, {-2, -2, 104, 104});
    // aspect 0.5/2.0 走 adjustViewDelta 的纵横比分支（ViewState.ts:908-913）。
    expectExtents(imodel.Get(), {0, 0, 100, 100}, {-2, -54, 104, 208}, 0.5);
    expectExtents(imodel.Get(), {0, 0, 100, 100}, {-54, -2, 208, 104}, 2.0);
}

// ============================================================================
// lookAtViewAlignedVolume — applies MarginPercent
// Ported from: itwinjs-core LookAt.test.ts it("applies MarginPercent")
// 注：参考注释——MarginPercent 的"百分比"并不精确（每侧 0.25 实际每侧加 50%）。
// ============================================================================
TEST(LookAtViewAlignedVolumeTest, AppliesMarginPercent)
{
    auto imodel = makeLookAtIModel();
    ASSERT_NE(imodel.Get(), nullptr);
    auto marginUniform = [](double p) {
        dqApp::MarginOptions o;
        o.marginPercent = dqApp::MarginPercent(p, p, p, p);
        return o;
    };

    expectExtents(imodel.Get(), {0, 0, 100, 100}, {-50, -50, 200, 200},
                  marginUniform(0.25));
    // { left: 0.25, top: 0.25 }
    {
        dqApp::MarginOptions o;
        o.marginPercent = dqApp::MarginPercent(0.25, 0.25, 0.0, 0.0);
        expectExtents(imodel.Get(), {0, 0, 100, 100}, {-33, 0, 133, 133}, o);
    }
    // { left: 0.25, bottom: 0.25 }
    {
        dqApp::MarginOptions o;
        o.marginPercent = dqApp::MarginPercent(0.25, 0.0, 0.0, 0.25);
        expectExtents(imodel.Get(), {0, 0, 100, 100}, {-33, -33, 133, 133}, o);
    }
    // { right: 0.25, top: 0.25 }
    {
        dqApp::MarginOptions o;
        o.marginPercent = dqApp::MarginPercent(0.0, 0.25, 0.25, 0.0);
        expectExtents(imodel.Get(), {0, 0, 100, 100}, {0, 0, 133, 133}, o);
    }
}

// ============================================================================
// lookAtViewAlignedVolume — applies PaddingPercent
// Ported from: itwinjs-core LookAt.test.ts it("applies PaddingPercent")
// ============================================================================
TEST(LookAtViewAlignedVolumeTest, AppliesPaddingPercent)
{
    auto imodel = makeLookAtIModel();
    ASSERT_NE(imodel.Get(), nullptr);
    auto padUniform = [](double p) {
        dqApp::MarginOptions o;
        o.paddingPercentUniform = p;
        return o;
    };
    auto padSides = [](double l, double r, double t, double b) {
        dqApp::MarginOptions o;
        dqApp::PaddingPercent pp;
        pp.left = l; pp.right = r; pp.top = t; pp.bottom = b;
        o.paddingPercent = pp;
        return o;
    };

    expectExtents(imodel.Get(), {0, 0, 100, 100}, {-50, -50, 200, 200}, padUniform(0.5));
    expectExtents(imodel.Get(), {0, 0, 100, 100}, {-25, -25, 150, 150}, padUniform(0.25));
    expectExtents(imodel.Get(), {0, 0, 100, 100}, {-25, -25, 125, 125}, padSides(0.25, 0, 0, 0.25));
    expectExtents(imodel.Get(), {0, 0, 100, 100}, {0, -25, 150, 150}, padSides(0, 0.5, 0, 0));
    expectExtents(imodel.Get(), {0, 0, 100, 100}, {-100, -50, 200, 200}, padSides(1, 0, 0, 0));
    expectExtents(imodel.Get(), {0, 0, 100, 100}, {-100, 0, 200, 100}, padSides(1, 0, 0, 0), 2);
    // 负 padding（收窄视图）。
    expectExtents(imodel.Get(), {0, 0, 100, 100}, {25, 25, 50, 50}, padUniform(-0.25));
    expectExtents(imodel.Get(), {0, 0, 100, 100}, {25, 25, 75, 75}, padSides(-0.25, 0, 0, -0.25));
    expectExtents(imodel.Get(), {0, 0, 100, 100}, {0, 0, 25, 25}, padSides(0, -0.75, -0.75, 0));
    expectExtents(imodel.Get(), {0, 0, 100, 100}, {100, 0, 100, 100}, padSides(-1, 1, 0, 0));
}

// ============================================================================
// lookAtViewAlignedVolume — prioritizes PaddingPercent over MarginPercent
// Ported from: itwinjs-core LookAt.test.ts
//              it("prioritizes PaddingPercent over MarginPercent")
// ============================================================================
TEST(LookAtViewAlignedVolumeTest, PrioritizesPaddingOverMargin)
{
    auto imodel = makeLookAtIModel();
    ASSERT_NE(imodel.Get(), nullptr);
    {
        dqApp::MarginOptions o;
        o.paddingPercentUniform = 0.5;
        o.marginPercent = dqApp::MarginPercent(0, 0.25, 0.125, 0.2);
        expectExtents(imodel.Get(), {0, 0, 100, 100}, {-50, -50, 200, 200}, o);
    }
    {
        dqApp::MarginOptions o;  // paddingPercent: undefined
        o.marginPercent = dqApp::MarginPercent(0.25, 0.25, 0.25, 0.25);
        expectExtents(imodel.Get(), {0, 0, 100, 100}, {-50, -50, 200, 200}, o);
    }
}

// ============================================================================
// lookAtGlobalLocation — should change camera
// Ported from: itwinjs-core LookAt.test.ts
//              describe("lookAtGlobalLocation") it("should change camera")
// ============================================================================
TEST(LookAtGlobalLocationTest, ShouldChangeCamera)
{
    auto imodel = makeLookAtIModel();
    ASSERT_NE(imodel.Get(), nullptr);
    auto view = dqApp::SpatialViewState::CreateBlank(
        imodel.Get(), dqGeom::Point3d(0, 0, 0), dqGeom::Vector3d(0, 0, 0));
    auto* v3 = view->AsViewState3d();
    ASSERT_NE(v3, nullptr);

    // view3d.camera.focusDist = 0; // ortho views should not use camera values
    v3->setFocusDistance(0.0);
    dqApp::LookAtArgs args;  // lookAt({viewDirection (0,1,0), eye 0, newExtents {1,1}, up z})
    args.viewDirection = dqGeom::Vector3d::From(0.0, 1.0, 0.0);
    args.eyePoint = dqGeom::Point3d::From(0.0, 0.0, 0.0);
    args.newExtents = dqGeom::Point2d{1.0, 1.0};
    args.upVector = dqGeom::Vector3d::From(0.0, 0.0, 1.0);
    EXPECT_EQ(v3->lookAt(args), dqApp::ViewStatus::Success);

    auto const oldCam = v3->GetCamera().clone();
    dqApp::GlobalLocation location;
    location.center = dqCommon::Cartographic::fromDegrees(-75.703054, 39.144703, 0.0);
    v3->lookAtGlobalLocation(1000.0, dqCommon::kPi / 4.0, location);
    EXPECT_GT(v3->getFocusDistance(), 0.0);
    EXPECT_FALSE(oldCam.equals(v3->GetCamera()));
}

// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — FeatureOverrideProvider tests
//
// Ported from: itwinjs-core core/frontend/src/test/render/FeatureSymbology.test.ts
#include <gtest/gtest.h>

#include <QApplication>

#include <dqApp/BlankConnection.h>
#include <dqApp/FeatureOverrideProvider.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqCommon/FeatureOverrides.h>
#include <dqCommon/FeatureSymbology.h>

using namespace dqApp;
using namespace dqCommon;
using namespace dqBase;

// Concrete test provider
//（M-O(2) I10 注：dqCommon::FeatureOverrideProvider 引入后 unqualified 名
//  在双 using 下歧义——dqApp 旧接口限定之。）
class TestProvider : public dqApp::FeatureOverrideProvider {
public:
    int callCount = 0;

    void addFeatureOverrides(const Viewport& /*viewport*/) override
    {
        callCount++;
    }
};

// FeatureOverrideProvider tests
// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(FeatureOverrideProvider, BasicInterface)
{
    // Test that the interface can be implemented
    TestProvider provider;
    EXPECT_EQ(provider.callCount, 0);
}

// ViewportFeatureOverrides tests
// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(ViewportFeatureOverrides, DefaultState)
{
    ViewportFeatureOverrides overrides;
    EXPECT_TRUE(overrides.GetProviders().empty());
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(ViewportFeatureOverrides, AddRemoveProvider)
{
    ViewportFeatureOverrides overrides;
    TestProvider provider1;
    TestProvider provider2;

    overrides.AddProvider(&provider1);
    EXPECT_EQ(overrides.GetProviders().size(), 1u);

    overrides.AddProvider(&provider2);
    EXPECT_EQ(overrides.GetProviders().size(), 2u);

    overrides.RemoveProvider(&provider1);
    EXPECT_EQ(overrides.GetProviders().size(), 1u);
    EXPECT_EQ(overrides.GetProviders()[0], &provider2);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(ViewportFeatureOverrides, RemoveNonexistentProvider)
{
    ViewportFeatureOverrides overrides;
    TestProvider provider1;
    TestProvider provider2;

    overrides.AddProvider(&provider1);
    overrides.RemoveProvider(&provider2);  // Not added, should be no-op
    EXPECT_EQ(overrides.GetProviders().size(), 1u);
}

// FeatureAppearance integration tests
// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(FeatureAppearanceIntegration, fromRgb)
{
    const auto app = FeatureAppearance::fromRgb(ColorDef::from(255, 0, 0));
    EXPECT_TRUE(app.overridesRgb());
    ASSERT_TRUE(app.getRgb().has_value());
    EXPECT_EQ(app.getRgb()->r, 255);
    EXPECT_EQ(app.getRgb()->g, 0);
    EXPECT_EQ(app.getRgb()->b, 0);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(FeatureAppearanceIntegration, fromRgba)
{
    const auto app = FeatureAppearance::fromRgba(ColorDef::from(0, 255, 0, 128));
    EXPECT_TRUE(app.overridesRgb());
    EXPECT_TRUE(app.overridesTransparency());
    ASSERT_TRUE(app.getTransparency().has_value());
    EXPECT_NEAR(app.getTransparency().value(), 128.0 / 255.0, 0.01);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(FeatureAppearanceIntegration, extendAppearance)
{
    const auto base = FeatureAppearance::fromRgb(ColorDef::red);
    const auto ext = FeatureAppearance::fromTransparency(0.5);

    const auto merged = ext.extendAppearance(base);
    EXPECT_TRUE(merged.overridesRgb());
    EXPECT_TRUE(merged.overridesTransparency());
}

// ViewportFeatureOverrides integration tests
// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(ViewportFeatureOverridesIntegration, ElementAppearance)
{
    ViewportFeatureOverrides overrides;

    // No appearance by default
    EXPECT_FALSE(overrides.getAppearance(DqId(1)).has_value());

    // Set appearance
    auto app = FeatureAppearance::fromRgb(ColorDef::red);
    overrides.SetElementAppearance(DqId(1), app);

    const auto result = overrides.getAppearance(DqId(1));
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->overridesRgb());

    // clear appearance
    overrides.ClearElementAppearance(DqId(1));
    EXPECT_FALSE(overrides.getAppearance(DqId(1)).has_value());
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(ViewportFeatureOverridesIntegration, DefaultOverrides)
{
    ViewportFeatureOverrides overrides;

    // Default should be FeatureAppearance::defaults
    EXPECT_TRUE(overrides.getDefaultOverrides().matchesDefaults());

    // Set custom default
    auto custom = FeatureAppearance::fromTransparency(0.5);
    overrides.setDefaultOverrides(custom);
    EXPECT_FALSE(overrides.getDefaultOverrides().matchesDefaults());
    EXPECT_TRUE(overrides.getDefaultOverrides().overridesTransparency());
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(ViewportFeatureOverridesIntegration, clear)
{
    ViewportFeatureOverrides overrides;
    overrides.SetElementAppearance(DqId(1), FeatureAppearance::fromRgb(ColorDef::red));
    overrides.setDefaultOverrides(FeatureAppearance::fromTransparency(0.5));

    overrides.clear();
    EXPECT_FALSE(overrides.getAppearance(DqId(1)).has_value());
    EXPECT_TRUE(overrides.getDefaultOverrides().matchesDefaults());
}

// ===========================================================================
// M-O(2) I10 — Viewport 级 FeatureOverrideProvider 注册面
// Ported from: itwinjs-core Viewport.addFeatureOverrideProvider /
//              dropFeatureOverrideProvider / findFeatureOverrideProvider
//              (Viewport.ts:1570-1615——重复注册 false / 未注册 drop false /
//              谓词查找)。参考无直接单测（Viewport 集成面经 browser tests）；
// Authored: 断言值 = Viewport.ts:1571-1576/:1584-1591/:1600-1606 的返回值
//           语义逐行。
// ===========================================================================
namespace {

class TestVpOverrideProvider : public dqCommon::FeatureOverrideProvider {
public:
    int callCount = 0;
    void addFeatureOverrides(dqCommon::FeatureOverrides& ovrs, void* /*context*/) override
    {
        ++callCount;
        ovrs.overrideElement(dqBase::DqId(0x1234),
                             dqCommon::FeatureAppearance::fromRgb(
                                 dqCommon::ColorDef::from(255, 0, 0)));
    }
};

// Viewport 构造需要 QApplication（ViewToolTest 的 Task10QApplicationEnv
// 先行创建；qApp 全局守卫下补一份本 TU 的惰性初始化）。
struct QtEnvFO {
    QtEnvFO()
    {
        if (qApp == nullptr) {
            static int argc = 1;
            static char name[] = "t";
            static char* argv[] = {name, nullptr};
            static QApplication app(argc, argv);
        }
    }
};
QtEnvFO s_qtEnvFO;

// 空白视图 + Viewport（ViewToolTest::buildViewWithValidViewingSpace 的最小变体）。
dqApp::Viewport* makeBlankViewport()
{
    auto props = dqApp::BlankConnectionProps{};
    props.extents = dqGeom::Range3d(dqGeom::Point3d::From(-100, -100, -100),
                                    dqGeom::Point3d::From(100, 100, 100));
    auto imodel = dqApp::BlankConnection::create(props);
    auto view = dqApp::SpatialViewState::CreateBlank(
        imodel.Get(), dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(200, 200, 200));
    return dqApp::Viewport::Create(nullptr, view);
}

}  // namespace

TEST(ViewportFeatureOverrideProvider, RegistrationSemanticsMatchReference)
{
    auto* vp = makeBlankViewport();
    ASSERT_NE(vp, nullptr);
    TestVpOverrideProvider provider;

    // addFeatureOverrideProvider (:1570-1577)——首次 true、重复 false。
    EXPECT_TRUE(vp->AddFeatureOverrideProvider(&provider));
    EXPECT_FALSE(vp->AddFeatureOverrideProvider(&provider));
    EXPECT_EQ(1u, vp->getFeatureOverrideProviders().size());

    // findFeatureOverrideProvider (:1600-1606)——谓词命中/未命中。
    EXPECT_EQ(&provider,
              vp->FindFeatureOverrideProvider([](dqCommon::FeatureOverrideProvider* x) {
                  return x != nullptr;
              }));
    EXPECT_EQ(nullptr,
              vp->FindFeatureOverrideProvider([](dqCommon::FeatureOverrideProvider*) {
                  return false;
              }));

    // dropFeatureOverrideProvider (:1584-1592)——已注册 true、未注册 false。
    EXPECT_TRUE(vp->DropFeatureOverrideProvider(&provider));
    EXPECT_FALSE(vp->DropFeatureOverrideProvider(&provider));
    EXPECT_EQ(0u, vp->getFeatureOverrideProviders().size());

    delete vp;
}


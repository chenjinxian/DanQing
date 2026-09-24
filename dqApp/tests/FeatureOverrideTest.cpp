// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — FeatureOverrideProvider tests
//
// Ported from: itwinjs-core core/frontend/src/test/render/FeatureSymbology.test.ts
#include <gtest/gtest.h>

#include <dqApp/FeatureOverrideProvider.h>
#include <dqCommon/FeatureSymbology.h>

using namespace dqApp;
using namespace dqCommon;
using namespace dqBase;

// Concrete test provider
class TestProvider : public FeatureOverrideProvider {
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

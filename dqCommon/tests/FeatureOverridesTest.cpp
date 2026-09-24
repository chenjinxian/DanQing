// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — FeatureOverrides unit tests (subsume default + ignoreAnimationOverrides)
//
// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              describe("FeatureOverrides")
//                it("applies conflict strategy")
//                it("subsumes by default")
//                it("ignores animation color/transparency overrides if specified")
#include <dqCommon/ColorDef.h>
#include <dqCommon/FeatureOverrides.h>
#include <dqCommon/FeatureSymbology.h>
#include <dqCommon/FeatureTable.h>

#include <functional>
#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqBase;

namespace {
// Helper: build a Feature from hex-string element id, default subcategory.
Feature MakeFeature(const char* elementId)
{
    return Feature(DqId::FromString(elementId));
}
}  // namespace

// Ported from: itwinjs-core FeatureSymbology.test.ts
//              it("subsumes by default")  (lines 456-464)
TEST(FeatureOverridesConflictTest, SubsumesByDefault)
{
    const DqId elementId = DqId::FromString("0x1");
    FeatureOverrides ovrs;

    const auto blueHalf = FeatureAppearance::fromRgba(ColorDef::blue.withTransparency(0x7f));
    const auto red = FeatureAppearance::fromRgb(ColorDef::red);
    ovrs.overrideElement(elementId, blueHalf);  // default onConflict = Subsume
    ovrs.overrideElement(elementId, red);        // default onConflict = Subsume

    const auto* app = ovrs.getElementOverridesById(elementId);
    ASSERT_NE(app, nullptr);
    // existing (blue + 0x7f) subsumed by new (red, no transp): red wins RGB, transparency preserved.
    const auto expected = FeatureAppearance::fromRgba(ColorDef::red.withTransparency(0x7f));
    EXPECT_TRUE(app->equals(expected));
}

// Ported from: itwinjs-core FeatureSymbology.test.ts
//              it("applies conflict strategy")  (lines 422-454) — reduced to the subsume cases.
TEST(FeatureOverridesConflictTest, SubsumeConflictStrategy)
{
    const DqId elementId = DqId::FromString("0x1");
    FeatureOverrides ovrs;

    // Establish green as the existing override.
    const auto green = FeatureAppearance::fromRgb(ColorDef::green);
    ovrs.overrideElement(elementId, green);

    // subsume: existing.extendAppearance(new). New transparency (0.25 = 0x3f) applies; RGB stays green.
    const auto transp025 = FeatureAppearance::fromTransparency(0.25);
    ovrs.overrideElement(elementId, transp025, OverrideConflict::Subsume);
    const auto* app = ovrs.getElementOverridesById(elementId);
    ASSERT_NE(app, nullptr);
    // 0.25 transparency → 0x3f byte (floor(0.25 * 0xff) = 63 = 0x3f)
    const auto expected = FeatureAppearance::fromRgba(ColorDef::blue.withTransparency(0x3f))
                              .extendAppearance(green);  // not exact; use direct expected below
    // Direct expected: green RGB + 0x3f transparency.
    const auto directExpected = FeatureAppearance::fromRgb(ColorDef::green)
                                    .extendAppearance(FeatureAppearance::fromTransparency(0.25));
    EXPECT_TRUE(app->equals(directExpected));
    (void)expected;

    // subsume again with a fresh RGB: red overrides green RGB; transparency preserved from existing.
    const auto red = FeatureAppearance::fromRgb(ColorDef::red);
    ovrs.overrideElement(elementId, red, OverrideConflict::Subsume);
    const auto* app2 = ovrs.getElementOverridesById(elementId);
    ASSERT_NE(app2, nullptr);
    const auto expected2 = FeatureAppearance::fromRgb(ColorDef::green)  // base w/ transp 0.25
                               .extendAppearance(FeatureAppearance::fromTransparency(0.25))
                               .extendAppearance(red);
    EXPECT_TRUE(app2->equals(expected2));
}

// Ported from: itwinjs-core FeatureSymbology.test.ts
//              it("ignores animation color/transparency overrides if specified") (lines 389-420)
TEST(FeatureOverridesAnimationTest, ignoreAnimationOverrides)
{
    FeatureOverrides ovrs;

    const auto red = FeatureAppearance::fromRgb(ColorDef::red);
    const auto green = FeatureAppearance::fromRgb(ColorDef::green);
    const auto blue = FeatureAppearance::fromRgb(ColorDef::blue);
    ovrs.overrideAnimationNode(1, red);
    ovrs.overrideAnimationNode(5, green);
    ovrs.overrideAnimationNode(10, blue);

    // Register two ignore-callbacks: ignore if elementId.lower > 5 OR animationNodeId < 5.
    ovrs.ignoreAnimationOverrides([](const IgnoreAnimationOverridesArgs& args) {
        return args.elementId.lower > 5;
    });
    ovrs.ignoreAnimationOverrides([](const IgnoreAnimationOverridesArgs& args) {
        return args.animationNodeId < 5;
    });

    const DqId modelId = DqId::FromString("0x1");

    // elemId "0x1", nodeId 10 → neither condition fires → blue applied.
    {
        auto app = ovrs.getFeatureAppearance(MakeFeature("0x1"), modelId, BatchType::Primary, 10);
        ASSERT_TRUE(app.has_value());
        EXPECT_TRUE(app->equals(blue)) << "elem 0x1 / node 10 should keep blue override";
    }
    // elemId "0x1", nodeId 5 → neither fires (lower=1 not >5; nodeId 5 not <5) → green applied.
    {
        auto app = ovrs.getFeatureAppearance(MakeFeature("0x1"), modelId, BatchType::Primary, 5);
        ASSERT_TRUE(app.has_value());
        EXPECT_TRUE(app->equals(green)) << "elem 0x1 / node 5 should keep green override";
    }
    // elemId "0x10", nodeId 10 → elementId.lower=16 > 5 → ignored → defaults.
    {
        auto app = ovrs.getFeatureAppearance(MakeFeature("0x10"), modelId, BatchType::Primary, 10);
        ASSERT_TRUE(app.has_value());
        EXPECT_TRUE(app->equals(FeatureAppearance::defaults()));
    }
    // elemId "0x1", nodeId 1 → nodeId 1 < 5 → ignored → defaults.
    {
        auto app = ovrs.getFeatureAppearance(MakeFeature("0x1"), modelId, BatchType::Primary, 1);
        ASSERT_TRUE(app.has_value());
        EXPECT_TRUE(app->equals(FeatureAppearance::defaults()));
    }
    // elemId "0x6", nodeId 5 → elementId.lower=6 > 5 → ignored → defaults.
    {
        auto app = ovrs.getFeatureAppearance(MakeFeature("0x6"), modelId, BatchType::Primary, 5);
        ASSERT_TRUE(app.has_value());
        EXPECT_TRUE(app->equals(FeatureAppearance::defaults()));
    }
}

// Authored: neverDrawn/alwaysDrawn getters return the registered sets.
TEST(FeatureOverridesDrawnSetsTest, NeverDrawnAndAlwaysDrawnGetters)
{
    FeatureOverrides ovrs;
    const DqId a = DqId::FromString("0x1");
    const DqId b = DqId::FromString("0x2");
    ovrs.setNeverDrawn(a);
    ovrs.setAlwaysDrawn(b);

    EXPECT_TRUE(ovrs.neverDrawn().count(a.GetValue()) > 0);
    EXPECT_TRUE(ovrs.alwaysDrawn().count(b.GetValue()) > 0);
    EXPECT_FALSE(ovrs.neverDrawn().count(b.GetValue()) > 0);
    EXPECT_FALSE(ovrs.alwaysDrawn().count(a.GetValue()) > 0);
}

// Authored: isSubCategoryIdVisible(Id) wraps the uint32 pair query.
TEST(FeatureOverridesVisibilityTest, IsSubCategoryIdVisibleById)
{
    FeatureOverrides ovrs;
    const DqId sub = DqId::FromString("0x3");
    EXPECT_FALSE(ovrs.isSubCategoryIdVisible(sub));
    ovrs.setVisibleSubCategory(sub);
    EXPECT_TRUE(ovrs.isSubCategoryIdVisible(sub));
}

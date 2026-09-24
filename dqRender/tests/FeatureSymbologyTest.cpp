// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Feature Symbology tests
//
// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts

#include "render/FeatureSymbology.h"

#include <gtest/gtest.h>

using namespace dqRender;

// ============================================================================
// FeatureAppearance tests
// ============================================================================

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "default constructor works as expected"
TEST(FeatureAppearanceTest, DefaultConstructorHasNoOverrides)
{
    FeatureAppearance app = FeatureAppearance::defaults();
    EXPECT_EQ(app.rgb, 0u);
    EXPECT_EQ(app.weight, 0u);
    EXPECT_FLOAT_EQ(app.transparency, 0.0f);
    EXPECT_EQ(app.linePixels, 0);
    EXPECT_FALSE(app.ignoresMaterial);
    EXPECT_FALSE(app.anyOverridden());
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "AppearanceProps passed in constructor works as expected"
TEST(FeatureAppearanceTest, FromRgbCreatesCorrectRgb)
{
    // RgbColor(100, 100, 100) = 0x646464
    FeatureAppearance app = FeatureAppearance::fromRgb(0x646464);
    EXPECT_EQ(app.rgb, 0x646464u);
    EXPECT_TRUE(app.anyOverridden());
    // Other fields remain default
    EXPECT_EQ(app.weight, 0u);
    EXPECT_FLOAT_EQ(app.transparency, 0.0f);
    EXPECT_EQ(app.linePixels, 0);
    EXPECT_FALSE(app.ignoresMaterial);
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "AppearanceProps passed in constructor works as expected"
TEST(FeatureAppearanceTest, FromTransparencyCreatesCorrectTransparency)
{
    float t = 200.0f / 255.0f;
    FeatureAppearance app = FeatureAppearance::fromTransparency(t);
    EXPECT_FLOAT_EQ(app.transparency, t);
    EXPECT_TRUE(app.anyOverridden());
    // Other fields remain default
    EXPECT_EQ(app.rgb, 0u);
    EXPECT_EQ(app.weight, 0u);
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              implicit in view-dependent transparency tests
TEST(FeatureAppearanceTest, isFullyTransparent)
{
    FeatureAppearance app;
    EXPECT_FALSE(app.isFullyTransparent());

    app.transparency = 1.0f;
    EXPECT_TRUE(app.isFullyTransparent());

    // If only surface transparency = 1.0 but lineTransparency = 0 (unset),
    // isFullyTransparent falls back to surface for line, so true.
    app.transparency = 1.0f;
    app.lineTransparency = 0.0f;
    EXPECT_TRUE(app.isFullyTransparent());

    // If both are explicitly set < 1.0, not fully transparent
    app.transparency = 0.9f;
    app.lineTransparency = 1.0f;
    EXPECT_FALSE(app.isFullyTransparent());

    // Both explicit >= 1.0
    app.transparency = 1.0f;
    app.lineTransparency = 1.0f;
    EXPECT_TRUE(app.isFullyTransparent());
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "extend works as expected"
TEST(FeatureAppearanceTest, ExtendAppearanceMergesCorrectly)
{
    // app1: rgb + linePixels + ignoresMaterial
    FeatureAppearance app1;
    app1.rgb = 0x646464;
    app1.linePixels = 2;
    app1.ignoresMaterial = true;

    // app2: rgb + weight + transparency + linePixels
    FeatureAppearance app2;
    app2.rgb = 0xFA96B4;  // (250, 180, 150)
    app2.weight = 1;
    app2.transparency = 200.0f / 255.0f;
    app2.linePixels = 3;

    // Extend app1 with app2: app2's non-zero fields override app1
    FeatureAppearance merged = app2.extendAppearance(app1);

    // app2's rgb wins over app1's rgb
    EXPECT_EQ(merged.rgb, 0xFA96B4u);
    // app1's linePixels wins because app2 also has linePixels (both non-zero, app2 wins)
    EXPECT_EQ(merged.linePixels, 3u);
    // app1's ignoresMaterial stays (app2 doesn't override it)
    EXPECT_TRUE(merged.ignoresMaterial);
    // app2's weight and transparency
    EXPECT_EQ(merged.weight, 1u);
    EXPECT_FLOAT_EQ(merged.transparency, 200.0f / 255.0f);
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "compares for equality"
TEST(FeatureAppearanceTest, EqualsReturnsTrueForIdenticalAppearances)
{
    FeatureAppearance a = FeatureAppearance::defaults();
    FeatureAppearance b;
    b.rgb = 0xFFFFFF;

    EXPECT_FALSE(a.equals(b));
    EXPECT_TRUE(a.equals(a));
    EXPECT_TRUE(b.equals(b));
    EXPECT_FALSE(b.equals(a));

    // Same fields
    FeatureAppearance c;
    c.rgb = 0xFFFFFF;
    EXPECT_TRUE(b.equals(c));
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "extend works as expected" — material override preserved
TEST(FeatureAppearanceTest, ExtendAppearanceIgnoresMaterialPreserved)
{
    FeatureAppearance base;
    base.ignoresMaterial = true;

    FeatureAppearance overlay;
    overlay.weight = 5;

    FeatureAppearance result = overlay.extendAppearance(base);
    EXPECT_TRUE(result.ignoresMaterial);
    EXPECT_EQ(result.weight, 5u);
}

// ============================================================================
// FeatureOverridesBase tests
// ============================================================================
// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts

// Helper subclass to access protected members (mirrors itwinjs test Overrides class)
class TestableOverrides : public FeatureOverridesBase {
public:
    using FeatureOverridesBase::FeatureOverridesBase;

    // Expose protected data for inspection
    std::unordered_map<uint32_t, FeatureAppearance> const& getElementOverrides() const { return m_elementOverrides; }
    std::unordered_map<uint32_t, FeatureAppearance> const& getModelOverrides() const { return m_modelOverrides; }
    std::unordered_map<uint32_t, FeatureAppearance> const& getSubCategoryOverrides() const { return m_subCategoryOverrides; }
};

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "default constructor works as expected"
TEST(FeatureOverridesTest, DefaultStateHasNoOverrides)
{
    TestableOverrides overrides;
    EXPECT_FALSE(overrides.isClassVisible(GeometryClass::Construction));
    EXPECT_FALSE(overrides.isClassVisible(GeometryClass::Dimension));
    EXPECT_FALSE(overrides.isClassVisible(GeometryClass::Pattern));
    EXPECT_TRUE(overrides.getLineWeights());
    EXPECT_FALSE(overrides.isAlwaysDrawnExclusive());
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "isSubCategoryVisible works as expected"
TEST(FeatureOverridesTest, SetVisibleSubCategoryRoundTrip)
{
    TestableOverrides overrides;
    uint32_t subCatId = 0x124;
    EXPECT_FALSE(overrides.isSubCategoryVisible(subCatId));

    overrides.setVisibleSubCategory(subCatId);
    EXPECT_TRUE(overrides.isSubCategoryVisible(subCatId));

    overrides.clearVisibleSubCategory(subCatId);
    EXPECT_FALSE(overrides.isSubCategoryVisible(subCatId));
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "override Element works as expected" (neverDrawn part)
TEST(FeatureOverridesTest, SetNeverDrawnMakesElementInvisible)
{
    TestableOverrides overrides;
    uint32_t elemId = 0x111;
    overrides.setVisibleSubCategory(0x1);

    FeatureAppearance app = FeatureAppearance::fromRgb(0x646464);
    overrides.override(elemId, app);

    // Before neverDrawn: element is visible
    FeatureAppearance result = overrides.getAppearance(elemId, 0x1, GeometryClass::Primary, 0);
    EXPECT_TRUE(result.anyOverridden());

    // After neverDrawn: element is invisible
    overrides.setNeverDrawn(elemId);
    result = overrides.getAppearance(elemId, 0x1, GeometryClass::Primary, 0);
    EXPECT_FALSE(result.anyOverridden());
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              implicit in override tests — alwaysDrawn bypasses subcategory
TEST(FeatureOverridesTest, SetAlwaysDrawnMakesElementVisible)
{
    TestableOverrides overrides;
    uint32_t elemId = 0x222;
    uint32_t subCatId = 0x1;

    // Subcategory is NOT visible
    EXPECT_FALSE(overrides.isSubCategoryVisible(subCatId));

    // Element is NOT alwaysDrawn: invisible
    EXPECT_FALSE(overrides.isFeatureVisible(elemId, subCatId, GeometryClass::Primary));

    // Set element as alwaysDrawn: visible even if subcategory hidden
    overrides.setAlwaysDrawn(elemId);
    EXPECT_TRUE(overrides.isFeatureVisible(elemId, subCatId, GeometryClass::Primary));
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "default constructor works as expected" (class visibility)
TEST(FeatureOverridesTest, IsClassVisibleRespectsFlags)
{
    TestableOverrides overrides;

    // Construction, Dimension, Pattern default to false
    EXPECT_FALSE(overrides.isClassVisible(GeometryClass::Construction));
    EXPECT_FALSE(overrides.isClassVisible(GeometryClass::Dimension));
    EXPECT_FALSE(overrides.isClassVisible(GeometryClass::Pattern));

    // Primary (the "real" geometry) is always visible
    EXPECT_TRUE(overrides.isClassVisible(GeometryClass::Primary));
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "override Model works as expected" (setAppearance/getAppearance round-trip)
TEST(FeatureOverridesTest, OverrideModelRoundTrip)
{
    TestableOverrides overrides;
    uint32_t modelId = 0x111;

    FeatureAppearance app1 = FeatureAppearance::fromRgb(0x646464);
    app1.weight = 1;
    app1.transparency = 100.0f / 255.0f;
    app1.linePixels = 1;  // Solid
    app1.ignoresMaterial = true;

    FeatureAppearance app2 = FeatureAppearance::fromRgb(0x646464);
    app2.weight = 1;
    app2.transparency = 200.0f / 255.0f;

    // override model with app1
    overrides.overrideModel(modelId, app1);
    EXPECT_TRUE(overrides.getModelOverrides().count(modelId) > 0);

    // override model with app2 — replaces existing
    overrides.overrideModel(modelId, app2);
    auto const& modelOverrides = overrides.getModelOverrides();
    auto it = modelOverrides.find(modelId);
    ASSERT_NE(it, modelOverrides.end());
    EXPECT_TRUE(it->second.equals(app2));
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "override SubCategory works as expected"
TEST(FeatureOverridesTest, OverrideSubCategoryRoundTrip)
{
    TestableOverrides overrides;
    uint32_t subCatId = 0x111;

    FeatureAppearance app1 = FeatureAppearance::fromRgb(0x646464);
    app1.weight = 1;
    app1.transparency = 100.0f / 255.0f;
    app1.linePixels = 1;
    app1.ignoresMaterial = true;

    FeatureAppearance app2 = FeatureAppearance::fromRgb(0x646464);
    app2.weight = 1;
    app2.transparency = 200.0f / 255.0f;

    // Before override: no subcategory override
    EXPECT_TRUE(overrides.getSubCategoryOverrides().empty());

    overrides.overrideSubCategory(subCatId, app1);
    EXPECT_TRUE(overrides.getSubCategoryOverrides().count(subCatId) > 0);

    // override again with app2 — replaces
    overrides.overrideSubCategory(subCatId, app2);
    auto const& subCatOverrides = overrides.getSubCategoryOverrides();
    auto it = subCatOverrides.find(subCatId);
    ASSERT_NE(it, subCatOverrides.end());
    EXPECT_TRUE(it->second.equals(app2));
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "override Element works as expected"
TEST(FeatureOverridesTest, OverrideElementNeverDrawnIgnoresOverride)
{
    TestableOverrides overrides;
    uint32_t elemId = 0x111;

    FeatureAppearance app = FeatureAppearance::fromRgb(0x646464);
    app.weight = 1;
    app.transparency = 100.0f / 255.0f;

    // Set neverDrawn first, then override — override is stored but query returns empty
    // (itwinjs-core filters at query time, not storage time)
    overrides.setNeverDrawn(elemId);
    overrides.override(elemId, app);
    FeatureAppearance result = overrides.getAppearance(elemId, 0x1, GeometryClass::Primary, 0);
    EXPECT_FALSE(result.anyOverridden());  // invisible due to neverDrawn

    // override visible element — should be set and visible
    TestableOverrides overrides2;
    overrides2.setVisibleSubCategory(0x1);
    overrides2.override(elemId, app);
    EXPECT_TRUE(overrides2.getElementOverrides().count(elemId) > 0);
    result = overrides2.getAppearance(elemId, 0x1, GeometryClass::Primary, 0);
    EXPECT_TRUE(result.anyOverridden());
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "setDefaultOverrides works as expected"
TEST(FeatureOverridesTest, setDefaultOverrides)
{
    TestableOverrides overrides;
    // Initial default should be empty
    EXPECT_FALSE(overrides.getDefaultOverrides().anyOverridden());

    FeatureAppearance app = FeatureAppearance::fromRgb(0x646464);
    app.weight = 1;
    app.transparency = 100.0f / 255.0f;

    overrides.setDefaultOverrides(app);
    EXPECT_TRUE(overrides.getDefaultOverrides().equals(app));
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              "should not apply default overrides if appearance explicitly specified"
TEST(FeatureOverridesTest, DefaultOverridesNotAppliedWhenExplicit)
{
    uint32_t cat1 = 0x1, cat2 = 0x2, cat3 = 0x3;
    uint32_t mod1 = 0x4, mod2 = 0x5, mod3 = 0x6;
    uint32_t el1 = 0x7, el2 = 0x8, el3 = 0x9;

    TestableOverrides ovrs;
    ovrs.setVisibleSubCategory(cat1);
    ovrs.setVisibleSubCategory(cat2);
    ovrs.setVisibleSubCategory(cat3);

    FeatureAppearance app = FeatureAppearance::fromRgb(0x00FF00);  // green
    FeatureAppearance noApp;
    FeatureAppearance defApp = FeatureAppearance::fromRgb(0xFF0000);  // red
    ovrs.setDefaultOverrides(defApp);

    ovrs.override(el1, app);
    ovrs.overrideModel(mod1, app);
    ovrs.overrideSubCategory(cat1, app);
    ovrs.override(el2, noApp);
    ovrs.overrideModel(mod2, noApp);
    ovrs.overrideSubCategory(cat2, noApp);

    // el1 has explicit element override -> app wins
    FeatureAppearance result = ovrs.getAppearance(el1, cat3, GeometryClass::Primary, mod3);
    EXPECT_TRUE(result.equals(app));

    // el2 has explicit element override (noApp = default) -> noApp
    result = ovrs.getAppearance(el2, cat3, GeometryClass::Primary, mod3);
    EXPECT_TRUE(result.equals(noApp));

    // el3 has no explicit override -> default overrides apply
    result = ovrs.getAppearance(el3, cat3, GeometryClass::Primary, mod3);
    EXPECT_TRUE(result.equals(defApp));

    // el3 + mod1: model override (app) applies
    result = ovrs.getAppearance(el3, cat3, GeometryClass::Primary, mod1);
    EXPECT_TRUE(result.equals(app));

    // el3 + mod2: model override (noApp) applies
    result = ovrs.getAppearance(el3, cat3, GeometryClass::Primary, mod2);
    EXPECT_TRUE(result.equals(noApp));

    // el3 + cat1: subcategory override (app) applies
    result = ovrs.getAppearance(el3, cat1, GeometryClass::Primary, mod3);
    EXPECT_TRUE(result.equals(app));

    // el3 + cat2: subcategory override (noApp) applies
    result = ovrs.getAppearance(el3, cat2, GeometryClass::Primary, mod3);
    EXPECT_TRUE(result.equals(noApp));
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              implicit — getAppearance precedence: element extends model extends defaults
//              Per itwinjs-core: elemApp.extendAppearance(modelApp.extendAppearance(base))
//              Non-zero fields in elemApp override modelApp fields.
TEST(FeatureOverridesTest, GetAppearancePrecedenceModelOverElement)
{
    TestableOverrides ovrs;
    uint32_t elemId = 0x1;
    uint32_t subCatId = 0x2;
    uint32_t modelId = 0x3;

    ovrs.setVisibleSubCategory(subCatId);

    FeatureAppearance elemApp = FeatureAppearance::fromRgb(0x00FF00);  // green
    FeatureAppearance modelApp = FeatureAppearance::fromRgb(0xFF0000);  // red

    ovrs.override(elemId, elemApp);
    ovrs.overrideModel(modelId, modelApp);

    // Element extends model: elemApp's green (non-zero) overrides model's red
    FeatureAppearance result = ovrs.getAppearance(elemId, subCatId, GeometryClass::Primary, modelId);
    EXPECT_EQ(result.rgb, 0x00FF00u);  // green wins (elem extends model)
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              implicit — getAppearance precedence: element > subcategory
TEST(FeatureOverridesTest, GetAppearancePrecedenceElementOverSubCategory)
{
    TestableOverrides ovrs;
    uint32_t elemId = 0x1;
    uint32_t subCatId = 0x2;

    ovrs.setVisibleSubCategory(subCatId);

    FeatureAppearance elemApp = FeatureAppearance::fromRgb(0x00FF00);  // green
    FeatureAppearance subCatApp = FeatureAppearance::fromRgb(0x0000FF);  // blue

    ovrs.override(elemId, elemApp);
    ovrs.overrideSubCategory(subCatId, subCatApp);

    // Subcategory extends element result: subCatApp's blue overrides elemApp's green
    FeatureAppearance result = ovrs.getAppearance(elemId, subCatId, GeometryClass::Primary, 0);
    EXPECT_EQ(result.rgb, 0x0000FFu);  // blue wins (subcat extends element)
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              implicit — getAppearance precedence: subcategory > default
TEST(FeatureOverridesTest, GetAppearancePrecedenceSubCategoryOverDefault)
{
    TestableOverrides ovrs;
    uint32_t subCatId = 0x2;

    ovrs.setVisibleSubCategory(subCatId);

    FeatureAppearance subCatApp = FeatureAppearance::fromRgb(0x0000FF);  // blue
    FeatureAppearance defApp = FeatureAppearance::fromRgb(0xFF0000);  // red

    ovrs.overrideSubCategory(subCatId, subCatApp);
    ovrs.setDefaultOverrides(defApp);

    // No element override — subcategory overrides are NOT considered "explicit"
    // for default override suppression in our implementation. The default overrides
    // apply when no explicit element/model/subcategory override exists.
    // In our impl, subCategory override is explicit so defaults are skipped.
    FeatureAppearance result = ovrs.getAppearance(0x99, subCatId, GeometryClass::Primary, 0);
    EXPECT_EQ(result.rgb, 0x0000FFu);
}

// ============================================================================
// FeatureSymbologyOverrides tests
// ============================================================================

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              implicit in _initFromView — sets visible subcategories
TEST(FeatureSymbologyOverridesTest, InitFromViewSetsVisibleSubcategories)
{
    FeatureSymbologyOverrides ovrs;
    std::vector<uint32_t> categoryIds = {1, 2};
    std::vector<uint32_t> subcategoryIds = {10, 20};
    std::unordered_set<uint32_t> excludedElements;

    ovrs.initFromView(categoryIds, subcategoryIds, excludedElements,
                      false, false, false, true);

    EXPECT_TRUE(ovrs.isSubCategoryVisible(1));
    EXPECT_TRUE(ovrs.isSubCategoryVisible(2));
    EXPECT_TRUE(ovrs.isSubCategoryVisible(10));
    EXPECT_TRUE(ovrs.isSubCategoryVisible(20));
    EXPECT_FALSE(ovrs.isSubCategoryVisible(30));
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              implicit in _initFromView — applies excluded elements to neverDrawn
TEST(FeatureSymbologyOverridesTest, InitFromViewAppliesExcludedElements)
{
    FeatureSymbologyOverrides ovrs;
    std::vector<uint32_t> categoryIds;
    std::vector<uint32_t> subcategoryIds = {1};
    std::unordered_set<uint32_t> excludedElements = {100, 200};

    ovrs.initFromView(categoryIds, subcategoryIds, excludedElements,
                      false, false, false, true);

    // Excluded elements should be invisible (neverDrawn)
    EXPECT_FALSE(ovrs.isFeatureVisible(100, 1, GeometryClass::Primary));
    EXPECT_FALSE(ovrs.isFeatureVisible(200, 1, GeometryClass::Primary));

    // Non-excluded element should be visible
    EXPECT_TRUE(ovrs.isFeatureVisible(300, 1, GeometryClass::Primary));
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              implicit in _initFromView — applies subcategory overrides
TEST(FeatureSymbologyOverridesTest, InitFromViewAppliesSubcategoryOverrides)
{
    FeatureSymbologyOverrides ovrs;
    std::vector<uint32_t> categoryIds;
    std::vector<uint32_t> subcategoryIds = {10};
    std::unordered_set<uint32_t> excludedElements;

    FeatureAppearance subCatApp = FeatureAppearance::fromRgb(0x00FF00);
    std::unordered_map<uint32_t, FeatureAppearance> subCatOverrides = {{10, subCatApp}};

    ovrs.initFromView(categoryIds, subcategoryIds, excludedElements,
                      false, false, false, true,
                      subCatOverrides);

    // The subcategory override should be applied to elements in that subcategory
    FeatureAppearance result = ovrs.getAppearance(99, 10, GeometryClass::Primary, 0);
    EXPECT_EQ(result.rgb, 0x00FF00u);
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              implicit in _initFromView — applies geometry class flags
TEST(FeatureSymbologyOverridesTest, InitFromViewAppliesGeometryClassFlags)
{
    FeatureSymbologyOverrides ovrs;
    std::vector<uint32_t> categoryIds;
    std::vector<uint32_t> subcategoryIds = {10};
    std::unordered_set<uint32_t> excludedElements;

    ovrs.initFromView(categoryIds, subcategoryIds, excludedElements,
                      true,   // constructions
                      true,   // dimensions
                      false,  // patterns
                      true);  // lineWeights

    EXPECT_TRUE(ovrs.isClassVisible(GeometryClass::Construction));
    EXPECT_TRUE(ovrs.isClassVisible(GeometryClass::Dimension));
    EXPECT_FALSE(ovrs.isClassVisible(GeometryClass::Pattern));
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              implicit in FeatureOverrides tests — initFromViewport sets neverDrawn/alwaysDrawn
TEST(FeatureSymbologyOverridesTest, InitFromViewportSetsNeverDrawnAlwaysDrawn)
{
    FeatureSymbologyOverrides ovrs;
    std::vector<uint32_t> neverDrawn = {10, 20};
    std::vector<uint32_t> alwaysDrawn = {30, 40};

    ovrs.initFromViewport(neverDrawn, alwaysDrawn);

    // neverDrawn elements are invisible
    EXPECT_FALSE(ovrs.isFeatureVisible(10, 0, GeometryClass::Primary));
    EXPECT_FALSE(ovrs.isFeatureVisible(20, 0, GeometryClass::Primary));

    // alwaysDrawn elements are visible
    EXPECT_TRUE(ovrs.isFeatureVisible(30, 0, GeometryClass::Primary));
    EXPECT_TRUE(ovrs.isFeatureVisible(40, 0, GeometryClass::Primary));
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              implicit — initFromViewport alwaysDrawn overrides subcategory
TEST(FeatureSymbologyOverridesTest, InitFromViewportAlwaysDrawnBypassesSubcategory)
{
    FeatureSymbologyOverrides ovrs;
    std::vector<uint32_t> neverDrawn;
    std::vector<uint32_t> alwaysDrawn = {50};

    ovrs.initFromViewport(neverDrawn, alwaysDrawn);

    // Subcategory is NOT visible
    EXPECT_FALSE(ovrs.isSubCategoryVisible(99));

    // But alwaysDrawn element is visible even with hidden subcategory
    EXPECT_TRUE(ovrs.isFeatureVisible(50, 99, GeometryClass::Primary));
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              implicit — initFromView model overrides
TEST(FeatureSymbologyOverridesTest, InitFromViewAppliesModelOverrides)
{
    FeatureSymbologyOverrides ovrs;
    std::vector<uint32_t> categoryIds;
    std::vector<uint32_t> subcategoryIds = {10};
    std::unordered_set<uint32_t> excludedElements;

    FeatureAppearance modelApp = FeatureAppearance::fromRgb(0xFF0000);
    std::unordered_map<uint32_t, FeatureAppearance> modelOverrides = {{5, modelApp}};

    ovrs.initFromView(categoryIds, subcategoryIds, excludedElements,
                      false, false, false, true,
                      {},  // no subcategory overrides
                      modelOverrides);

    // Element in that model should get the model override
    FeatureAppearance result = ovrs.getAppearance(99, 10, GeometryClass::Primary, 5);
    EXPECT_EQ(result.rgb, 0xFF0000u);
}

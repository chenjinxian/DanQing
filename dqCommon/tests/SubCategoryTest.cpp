// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — SubCategoryAppearance, SubCategoryOverride, GraphicParams, GeometryParams tests
//
// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
#include "dqCommon/ColorDef.h"
#include "dqCommon/GraphicParams.h"
#include "dqCommon/SubCategoryAppearance.h"

#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqBase;

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(SubCategoryAppearance, defaults)
TEST(SubCategoryAppearance, defaults)
{
    const auto& def = SubCategoryAppearance::defaults();
    EXPECT_TRUE(def.color.equals(ColorDef::black));
    EXPECT_EQ(def.weight, 0);
    EXPECT_EQ(def.priority, 0);
    EXPECT_DOUBLE_EQ(def.transparency, 0.0);
    EXPECT_FALSE(def.invisible);
    EXPECT_FALSE(def.dontPlot);
    EXPECT_FALSE(def.dontSnap);
    EXPECT_FALSE(def.dontLocate);
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(SubCategoryAppearance, FillColor)
TEST(SubCategoryAppearance, FillColor)
{
    SubCategoryAppearance app;
    // Fill color defaults to line color
    EXPECT_TRUE(app.getFillColor().equals(app.color));

    app.setFillColor(ColorDef::red);
    EXPECT_TRUE(app.getFillColor().equals(ColorDef::red));
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(SubCategoryAppearance, FillTransparency)
TEST(SubCategoryAppearance, FillTransparency)
{
    SubCategoryAppearance app;
    // Fill transparency defaults to line transparency
    EXPECT_DOUBLE_EQ(app.getFillTransparency(), app.transparency);

    app.setFillTransparency(0.5);
    EXPECT_DOUBLE_EQ(app.getFillTransparency(), 0.5);
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(SubCategoryAppearance, Equality)
TEST(SubCategoryAppearance, Equality)
{
    const auto& a = SubCategoryAppearance::defaults();
    const auto& b = SubCategoryAppearance::defaults();
    EXPECT_TRUE(a.equals(b));

    SubCategoryAppearance c;
    c.invisible = true;
    EXPECT_FALSE(a.equals(c));
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(SubCategoryAppearance, clone)
TEST(SubCategoryAppearance, clone)
{
    SubCategoryAppearance app;
    app.weight = 5;
    app.transparency = 0.3;
    const auto clone = app.clone();
    EXPECT_TRUE(app.equals(clone));
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(SubCategoryOverride, defaults)
TEST(SubCategoryOverride, defaults)
{
    const auto& def = SubCategoryOverride::defaults();
    EXPECT_FALSE(def.anyOverridden());
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(SubCategoryOverride, fromJSON)
TEST(SubCategoryOverride, fromJSON)
{
    auto ovr = SubCategoryOverride::fromJSON(ColorDef::red, false, 5, 0, 0.5);
    EXPECT_TRUE(ovr.anyOverridden());
    ASSERT_TRUE(ovr.color.has_value());
    EXPECT_TRUE(ovr.color->equals(ColorDef::red));
    ASSERT_TRUE(ovr.weight.has_value());
    EXPECT_EQ(ovr.weight.value(), 5);
    ASSERT_TRUE(ovr.transparency.has_value());
    EXPECT_DOUBLE_EQ(ovr.transparency.value(), 0.5);
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(SubCategoryOverride, override)
TEST(SubCategoryOverride, override)
{
    SubCategoryAppearance app;
    app.weight = 1;
    app.transparency = 0.0;

    auto ovr = SubCategoryOverride::fromJSON(ColorDef::red, std::nullopt, 5, std::nullopt, 0.5);
    const auto result = ovr.override(app);

    EXPECT_TRUE(result.color.equals(ColorDef::red));
    EXPECT_EQ(result.weight, 5);
    EXPECT_DOUBLE_EQ(result.transparency, 0.5);
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(SubCategoryOverride, NoOverride)
TEST(SubCategoryOverride, NoOverride)
{
    SubCategoryAppearance app;
    app.weight = 1;

    const auto& ovr = SubCategoryOverride::defaults();
    const auto result = ovr.override(app);
    EXPECT_TRUE(result.equals(app));
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(SubCategoryOverride, Equality)
TEST(SubCategoryOverride, Equality)
{
    const auto a = SubCategoryOverride::fromJSON(ColorDef::red, std::nullopt, 5);
    const auto b = SubCategoryOverride::fromJSON(ColorDef::red, std::nullopt, 5);
    const auto c = SubCategoryOverride::fromJSON(ColorDef::blue, std::nullopt, 5);
    EXPECT_TRUE(a.equals(b));
    EXPECT_FALSE(a.equals(c));
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(GraphicParams, defaults)
TEST(GraphicParams, defaults)
{
    const GraphicParams gp;
    EXPECT_EQ(gp.fillFlags, FillFlags::None);
    EXPECT_EQ(gp.linePixels, LinePixels::Solid);
    EXPECT_EQ(gp.rasterWidth, 1);
    EXPECT_TRUE(gp.lineColor.equals(ColorDef::black));
    EXPECT_TRUE(gp.fillColor.equals(ColorDef::black));
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(GraphicParams, fromSymbology)
TEST(GraphicParams, fromSymbology)
{
    const auto gp = GraphicParams::fromSymbology(ColorDef::red, ColorDef::blue, 3, LinePixels::Code2);
    EXPECT_TRUE(gp.lineColor.equals(ColorDef::red));
    EXPECT_TRUE(gp.fillColor.equals(ColorDef::blue));
    EXPECT_EQ(gp.rasterWidth, 3);
    EXPECT_EQ(gp.linePixels, LinePixels::Code2);
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(GraphicParams, fromBlankingFill)
TEST(GraphicParams, fromBlankingFill)
{
    const auto gp = GraphicParams::fromBlankingFill(ColorDef::green);
    EXPECT_TRUE(gp.fillColor.equals(ColorDef::green));
    EXPECT_EQ(gp.fillFlags, FillFlags::Blanking);
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(GraphicParams, SetTransparency)
TEST(GraphicParams, SetTransparency)
{
    GraphicParams gp;
    gp.setLineTransparency(128);
    EXPECT_EQ(gp.lineColor.getTransparency(), 128);
    gp.setFillTransparency(64);
    EXPECT_EQ(gp.fillColor.getTransparency(), 64);
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(GraphicParams, clone)
TEST(GraphicParams, clone)
{
    const auto gp = GraphicParams::fromSymbology(ColorDef::red, ColorDef::blue, 3);
    const auto clone = gp.clone();
    EXPECT_TRUE(clone.lineColor.equals(gp.lineColor));
    EXPECT_TRUE(clone.fillColor.equals(gp.fillColor));
    EXPECT_EQ(clone.rasterWidth, gp.rasterWidth);
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(FillDisplayEnum, Values)
TEST(FillDisplayEnum, Values)
{
    EXPECT_EQ(static_cast<int>(FillDisplay::Never), 0);
    EXPECT_EQ(static_cast<int>(FillDisplay::ByView), 1);
    EXPECT_EQ(static_cast<int>(FillDisplay::Always), 2);
    EXPECT_EQ(static_cast<int>(FillDisplay::Blanking), 3);
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(BackgroundFillEnum, Values)
TEST(BackgroundFillEnum, Values)
{
    EXPECT_EQ(static_cast<int>(BackgroundFill::None), 0);
    EXPECT_EQ(static_cast<int>(BackgroundFill::Solid), 1);
    EXPECT_EQ(static_cast<int>(BackgroundFill::Outline), 2);
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(GeometryParams, Construction)
TEST(GeometryParams, Construction)
{
    const GeometryParams params(DqId(100), DqId(200));
    EXPECT_EQ(params.categoryId, DqId(100));
    EXPECT_EQ(params.subCategoryId, DqId(200));
    EXPECT_FALSE(params.materialId.has_value());
    EXPECT_FALSE(params.lineColor.has_value());
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(GeometryParams, resetAppearance)
TEST(GeometryParams, resetAppearance)
{
    GeometryParams params(DqId(1), DqId(2));
    params.weight = 5;
    params.lineColor = ColorDef::red;
    params.elmTransparency = 0.5;

    params.resetAppearance();
    EXPECT_FALSE(params.weight.has_value());
    EXPECT_FALSE(params.lineColor.has_value());
    EXPECT_FALSE(params.elmTransparency.has_value());
    EXPECT_EQ(params.categoryId, DqId(1));
    EXPECT_EQ(params.subCategoryId, DqId(2));
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(GeometryParams, isEquivalent)
TEST(GeometryParams, isEquivalent)
{
    GeometryParams a(DqId(1), DqId(2));
    GeometryParams b(DqId(1), DqId(2));
    EXPECT_TRUE(a.isEquivalent(b));

    a.weight = 5;
    EXPECT_FALSE(a.isEquivalent(b));

    b.weight = 5;
    EXPECT_TRUE(a.isEquivalent(b));

    a.lineColor = ColorDef::red;
    EXPECT_FALSE(a.isEquivalent(b));
}

// Ported from: itwinjs-core core/common/src/test/SubCategory.test.ts
//              TEST(GeometryParams, clone)
TEST(GeometryParams, clone)
{
    GeometryParams params(DqId(1), DqId(2));
    params.weight = 5;
    params.lineColor = ColorDef::red;

    const auto clone = params.clone();
    EXPECT_EQ(clone.categoryId, DqId(1));
    EXPECT_EQ(clone.weight.value(), 5);
    EXPECT_TRUE(clone.lineColor->equals(ColorDef::red));
}

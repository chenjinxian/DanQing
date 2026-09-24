// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — GraphicParams and GeometryParams additional behavior tests
//
// Authored: no reference test exists in itwinjs-core for GraphicParams/GeometryParams.
//           Behavior verified against GraphicParams.ts and GeometryParams.ts directly.
#include <dqCommon/GraphicParams.h>
#include <dqCommon/RenderMaterial.h>

#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqBase;

namespace {
class TestMaterial : public RenderMaterial {
public:
    TestMaterial() = default;
    TestMaterial(std::string k) { key = std::move(k); }
};
}  // namespace

// Authored: GraphicParams.material + gradient fields (ref GraphicParams.ts lines 52, 54)
TEST(GraphicParamsFieldsTest, MaterialAndGradientFields)
{
    GraphicParams gp;
    EXPECT_FALSE(gp.material);
    EXPECT_FALSE(gp.gradient.has_value());

    gp.material = RefPtr<RenderMaterial>(new TestMaterial("m1"));
    EXPECT_TRUE(gp.material);
    EXPECT_EQ(gp.material->key, "m1");

    GradientSymb symb;
    symb.mode = GradientMode::Linear;
    gp.gradient = symb;
    EXPECT_TRUE(gp.gradient.has_value());
    EXPECT_EQ(gp.gradient->mode, GradientMode::Linear);
}

// Authored: clone copies material + gradient (ref GraphicParams.clone lines 62-74)
TEST(GraphicParamsFieldsTest, CloneCopiesMaterialAndGradient)
{
    GraphicParams gp;
    gp.material = RefPtr<RenderMaterial>(new TestMaterial("m1"));
    GradientSymb symb;
    symb.shift = 0.5;
    gp.gradient = symb;

    const auto clone = gp.clone();
    EXPECT_TRUE(clone.material);
    EXPECT_EQ(clone.material->key, "m1");
    EXPECT_TRUE(clone.gradient.has_value());
    EXPECT_DOUBLE_EQ(clone.gradient->shift, 0.5);
}

// Authored: GeometryParams.geometryClass field (ref GeometryParams.ts line 112)
TEST(GeometryParamsFieldsTest, GeometryClassField)
{
    GeometryParams gp(DqId::FromString("0x1"));
    EXPECT_FALSE(gp.geometryClass.has_value());
    gp.geometryClass = GeometryClass::Construction;
    EXPECT_EQ(*gp.geometryClass, GeometryClass::Construction);
}

// Authored: GeometryParams.resetAppearance clears geometryClass (ref line 165)
TEST(GeometryParamsFieldsTest, ResetAppearanceClearsGeometryClass)
{
    GeometryParams gp(DqId::FromString("0x1"));
    gp.geometryClass = GeometryClass::Pattern;
    gp.weight = 5;
    gp.resetAppearance();
    EXPECT_FALSE(gp.geometryClass.has_value());
    EXPECT_FALSE(gp.weight.has_value());
}

// Authored: setCategoryId auto-fills default subcategory = localId + 1
// (ref GeometryParams.setCategoryId + IModel.getDefaultSubCategoryId, lines 241-246, 636-638)
TEST(GeometryParamsFieldsTest, SetCategoryIdAutoFillsDefaultSubCategory)
{
    GeometryParams gp(DqId::FromString("0x1"));
    gp.weight = 5;
    // categoryId 0x1 (localId 1, briefcase 0) → default subcategory localId 2 → 0x2
    gp.setCategoryId(DqId::FromString("0x1"));
    EXPECT_EQ(gp.categoryId, DqId::FromString("0x1"));
    EXPECT_EQ(gp.subCategoryId, DqId::FromString("0x2"));
    EXPECT_FALSE(gp.weight.has_value());  // appearance overrides cleared by default
}

// Authored: setCategoryId with clearAppearanceOverrides=false preserves overrides
TEST(GeometryParamsFieldsTest, SetCategoryIdPreservesOverridesWhenRequested)
{
    GeometryParams gp(DqId::FromString("0x1"));
    gp.weight = 5;
    gp.setCategoryId(DqId::FromString("0x1"), false);
    EXPECT_EQ(gp.categoryId, DqId::FromString("0x1"));
    EXPECT_EQ(gp.subCategoryId, DqId::FromString("0x2"));
    EXPECT_TRUE(gp.weight.has_value());
}

// Authored: setSubCategoryId clears overrides by default (ref line 249-253)
TEST(GeometryParamsFieldsTest, SetSubCategoryIdClearsOverrides)
{
    GeometryParams gp(DqId::FromString("0x1"));
    gp.weight = 5;
    gp.setSubCategoryId(DqId::FromString("0xa"));
    EXPECT_EQ(gp.subCategoryId, DqId::FromString("0xa"));
    EXPECT_FALSE(gp.weight.has_value());
}

// Authored: isEquivalent includes geometryClass + new fields (ref lines 172-238)
TEST(GeometryParamsFieldsTest, IsEquivalentComparesGeometryClass)
{
    GeometryParams a(DqId::FromString("0x1"));
    GeometryParams b(DqId::FromString("0x1"));
    EXPECT_TRUE(a.isEquivalent(b));

    a.geometryClass = GeometryClass::Construction;
    EXPECT_FALSE(a.isEquivalent(b));
    b.geometryClass = GeometryClass::Construction;
    EXPECT_TRUE(a.isEquivalent(b));
}

// Authored: GeometryParams.clone is a deep copy of optional fields (ref lines 136-152)
TEST(GeometryParamsFieldsTest, CloneIsDeepCopy)
{
    GeometryParams a(DqId::FromString("0x1"));
    a.weight = 3;
    a.geometryClass = GeometryClass::Dimension;
    GradientSymb symb;
    symb.shift = 0.25;
    a.gradient = symb;

    auto b = a.clone();
    EXPECT_EQ(b.categoryId, a.categoryId);
    EXPECT_EQ(b.weight, a.weight);
    EXPECT_EQ(b.geometryClass, a.geometryClass);
    ASSERT_TRUE(b.gradient.has_value());
    EXPECT_DOUBLE_EQ(b.gradient->shift, 0.25);

    // Mutating the clone must not affect the original.
    b.gradient->shift = 0.99;
    EXPECT_DOUBLE_EQ(a.gradient->shift, 0.25);
}

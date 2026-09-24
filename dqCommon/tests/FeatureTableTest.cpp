// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — FeatureTable, FeatureIndex, ColorIndex unit tests
//
// Reference test file: itwinjs-core core/common/src/test/FeatureIndex.test.ts has exactly ONE
//   describe-block: describe("ColorIndex") { it("should create, store and retrieve from ColorIndex") }.
//   Note: despite the filename "FeatureIndex", the reference test covers ColorIndex only — there is
//   NO reference test for the FeatureIndex struct, the Feature struct, or the FeatureTable class.
//
// Citation map:
//   - ColorIndex.CreateAndRetrieve → ported from the single real ref case (verbatim assertions).
//   - Feature / FeatureTable / GeometryClass / BatchType → Authored (no ref case covers these).
//   - FeatureIndex (the struct) → Authored (ref test file does not exercise it).
#include "dqCommon/ColorDef.h"
#include "dqCommon/FeatureIndex.h"
#include "dqCommon/FeatureTable.h"
#include "dqCommon/GeometryClass.h"

#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqBase;

// Authored: no equivalent reference test in itwinjs-core for the makeFeatureTable helper
//   (FeatureIndex.test.ts has no FeatureTable helper; it constructs ColorIndex directly).
static FeatureTable MakeFeatureTable(int numFeatures)
{
    FeatureTable table(numFeatures);
    for (int i = 1; i <= numFeatures; i++)
        table.insertWithIndex(Feature(DqId(static_cast<uint64_t>(i))), i - 1);
    return table;
}

// Authored: no equivalent reference test in itwinjs-core for Feature default construction
//   (FeatureIndex.test.ts covers ColorIndex only, never the Feature struct);
//   behavior verified against itwinjs-core Feature.ts contract.
TEST(Feature, DefaultConstruction)
{
    const Feature f;
    EXPECT_TRUE(f.elementId.isNull());
    EXPECT_TRUE(f.subCategoryId.isNull());
    EXPECT_EQ(f.geometryClass, GeometryClass::Primary);
    EXPECT_FALSE(f.isDefined());
}

// Authored: no equivalent reference test in itwinjs-core for Feature.WithElementId
TEST(Feature, WithElementId)
{
    const Feature f(DqId(42));
    EXPECT_EQ(f.elementId, DqId(42));
    EXPECT_TRUE(f.isDefined());
}

// Authored: no equivalent reference test in itwinjs-core for Feature.equals
TEST(Feature, equals)
{
    const Feature f1(DqId(1), DqId(2), GeometryClass::Primary);
    const Feature f2(DqId(1), DqId(2), GeometryClass::Primary);
    const Feature f3(DqId(1), DqId(3), GeometryClass::Primary);
    EXPECT_TRUE(f1.equals(f2));
    EXPECT_FALSE(f1.equals(f3));
}

// Authored: no equivalent reference test in itwinjs-core for Feature.Compare
TEST(Feature, Compare)
{
    const Feature f1(DqId(1), DqId(2), GeometryClass::Primary);
    const Feature f2(DqId(1), DqId(2), GeometryClass::Primary);
    EXPECT_EQ(f1.compare(f2), 0);

    const Feature f3(DqId(2), DqId(2), GeometryClass::Primary);
    EXPECT_LT(f1.compare(f3), 0);
    EXPECT_GT(f3.compare(f1), 0);

    // Different geometry class: Construction (1) > Primary (0)
    const Feature f4(DqId(1), DqId(2), GeometryClass::Construction);
    EXPECT_GT(f4.compare(f1), 0);  // Construction > Primary
}

// Authored: no equivalent reference test in itwinjs-core for FeatureTable construction
TEST(FeatureTable, Construction)
{
    const FeatureTable table(100);
    EXPECT_EQ(table.getSize(), 0);
    EXPECT_EQ(table.getMaxFeatures(), 100);
    EXPECT_EQ(table.getType(), BatchType::Primary);
    EXPECT_FALSE(table.isUniform());
}

// Authored: no equivalent reference test in itwinjs-core for FeatureTable.insert
TEST(FeatureTable, insert)
{
    FeatureTable table(100);
    const int idx1 = table.insert(Feature(DqId(1)));
    const int idx2 = table.insert(Feature(DqId(2)));
    const int idx3 = table.insert(Feature(DqId(1)));  // duplicate
    EXPECT_EQ(idx1, 0);
    EXPECT_EQ(idx2, 1);
    EXPECT_EQ(idx3, 0);  // returns existing index
    EXPECT_EQ(table.getSize(), 2);
}

// Authored: no equivalent reference test in itwinjs-core for FeatureTable.insertWithIndex
TEST(FeatureTable, insertWithIndex)
{
    FeatureTable table(100);
    table.insertWithIndex(Feature(DqId(1)), 5);
    table.insertWithIndex(Feature(DqId(2)), 10);
    EXPECT_EQ(table.getSize(), 2);

    const auto f1 = table.findFeature(5);
    ASSERT_TRUE(f1.has_value());
    EXPECT_EQ(f1->elementId, DqId(1));

    const auto f2 = table.findFeature(10);
    ASSERT_TRUE(f2.has_value());
    EXPECT_EQ(f2->elementId, DqId(2));

    // Non-existent index
    EXPECT_FALSE(table.findFeature(3).has_value());
}

// Authored: no equivalent reference test in itwinjs-core for FeatureTable.isAnyDefined
TEST(FeatureTable, AnyDefined)
{
    FeatureTable table(100);
    EXPECT_FALSE(table.isAnyDefined());

    table.insert(Feature(DqId(1)));
    EXPECT_TRUE(table.isAnyDefined());

    // insert undefined feature
    FeatureTable table2(100);
    table2.insert(Feature());
    EXPECT_FALSE(table2.isAnyDefined());
}

// Authored: no equivalent reference test in itwinjs-core for FeatureTable uniform-feature query
TEST(FeatureTable, Uniform)
{
    FeatureTable table(100);
    EXPECT_FALSE(table.isUniform());

    table.insert(Feature(DqId(1)));
    EXPECT_TRUE(table.isUniform());
    const auto uniform = table.getUniform();
    ASSERT_TRUE(uniform.has_value());
    EXPECT_EQ(uniform->elementId, DqId(1));

    table.insert(Feature(DqId(2)));
    EXPECT_FALSE(table.isUniform());
    EXPECT_FALSE(table.getUniform().has_value());
}

// Authored: no equivalent reference test in itwinjs-core for the MakeFeatureTable helper
TEST(FeatureTable, MakeFeatureTable)
{
    auto table = MakeFeatureTable(5);
    EXPECT_EQ(table.getSize(), 5);
    for (int i = 1; i <= 5; i++) {
        const auto f = table.findFeature(i - 1);
        ASSERT_TRUE(f.has_value()) << "Feature index " << (i - 1);
        EXPECT_EQ(f->elementId, DqId(static_cast<uint64_t>(i)));
    }
}

// Authored: no equivalent reference test in itwinjs-core for GeometryClass enum values
//   (FeatureIndex.test.ts does not assert enum integer values);
//   values verified against itwinjs-core Feature.ts GeometryClass enum.
TEST(GeometryClass, Values)
{
    EXPECT_EQ(static_cast<int>(GeometryClass::Primary), 0);
    EXPECT_EQ(static_cast<int>(GeometryClass::Construction), 1);
    EXPECT_EQ(static_cast<int>(GeometryClass::Dimension), 2);
    EXPECT_EQ(static_cast<int>(GeometryClass::Pattern), 3);
}

// Authored: no equivalent reference test in itwinjs-core for BatchType enum values
//   values verified against itwinjs-core Feature.ts BatchType enum.
TEST(BatchType, Values)
{
    EXPECT_EQ(static_cast<int>(BatchType::Primary), 0);
    EXPECT_EQ(static_cast<int>(BatchType::VolumeClassifier), 1);
    EXPECT_EQ(static_cast<int>(BatchType::PlanarClassifier), 2);
}

// Authored: no equivalent reference test in itwinjs-core for the FeatureIndex struct
//   (FeatureIndex.test.ts, despite its name, covers ColorIndex ONLY — it never constructs
//    or asserts the FeatureIndex struct); behavior verified against FeatureIndex.ts contract.
TEST(FeatureIndex, DefaultConstruction)
{
    FeatureIndex fi;
    EXPECT_EQ(fi.type, FeatureIndexType::Empty);
    EXPECT_EQ(fi.featureID, 0u);
    EXPECT_TRUE(fi.isEmpty());
    EXPECT_FALSE(fi.isUniform());
}

// Authored: no equivalent reference test in itwinjs-core for the FeatureIndex struct (uniform state)
TEST(FeatureIndex, Uniform)
{
    FeatureIndex fi;
    fi.type = FeatureIndexType::Uniform;
    fi.featureID = 42;
    EXPECT_TRUE(fi.isUniform());
    EXPECT_FALSE(fi.isEmpty());
    EXPECT_EQ(fi.featureID, 42u);
}

// Authored: no equivalent reference test in itwinjs-core for the FeatureIndex struct (Reset)
TEST(FeatureIndex, Reset)
{
    FeatureIndex fi;
    fi.type = FeatureIndexType::NonUniform;
    fi.featureID = 99;
    fi.featureIDs = {1, 2, 3};
    fi.reset();
    EXPECT_EQ(fi.type, FeatureIndexType::Empty);
    EXPECT_EQ(fi.featureID, 0u);
    EXPECT_TRUE(fi.featureIDs.empty());
}

// ColorIndex tests
// Ported from: itwinjs-core core/common/src/test/FeatureIndex.test.ts
//              describe("ColorIndex") it("should create, store and retrieve from ColorIndex") (L9-68)
//              Verbatim ref assertions: 0x00ffffff default uniform, 0xff000000 initUniform w/ alpha,
//               ColorDef.from(255,127,63,31) → 0x1f3f7fff, 10-color nonUniform with alpha, reset round-trip.
TEST(ColorIndex, CreateAndRetrieve)
{
    ColorIndex ci;

    // Newly created should be uniform white
    EXPECT_TRUE(ci.isUniform());
    EXPECT_EQ(ci.numColors(), 1);
    const auto white = ci.getUniform();
    ASSERT_TRUE(white.has_value());
    EXPECT_EQ(white->getTbgr(), ColorDef::white.getTbgr());
    EXPECT_EQ(ci.getNonUniform(), nullptr);
    EXPECT_FALSE(ci.hasAlpha());

    // Set uniform color with transparency
    ci.initUniform(0xff000000u);
    EXPECT_TRUE(ci.isUniform());
    EXPECT_EQ(ci.numColors(), 1);
    EXPECT_EQ(ci.getUniform()->getTbgr(), 0xff000000u);
    EXPECT_EQ(ci.getNonUniform(), nullptr);
    EXPECT_TRUE(ci.hasAlpha());

    // Set uniform color via ColorDef
    ci.initUniform(ColorDef::from(255, 127, 63, 31));
    EXPECT_TRUE(ci.isUniform());
    EXPECT_EQ(ci.numColors(), 1);
    EXPECT_EQ(ci.getUniform()->getTbgr(), 0x1f3f7fffu);
    EXPECT_EQ(ci.getNonUniform(), nullptr);
    EXPECT_TRUE(ci.hasAlpha());

    // Reset
    ci.reset();
    EXPECT_TRUE(ci.isUniform());
    EXPECT_EQ(ci.numColors(), 1);
    EXPECT_EQ(ci.getUniform()->getTbgr(), ColorDef::white.getTbgr());
    EXPECT_EQ(ci.getNonUniform(), nullptr);
    EXPECT_FALSE(ci.hasAlpha());

    // Non-uniform colors
    const int numColors = 10;
    std::vector<uint32_t> colors(numColors);
    std::vector<uint16_t> indices(numColors);
    for (int i = 0; i < numColors; ++i) {
        colors[i] = ColorDef::from(i, i * 2, i * 4, 63).getTbgr();
        indices[i] = static_cast<uint16_t>(i);
    }
    ci.initNonUniform(colors, indices, true);
    EXPECT_FALSE(ci.isUniform());
    EXPECT_EQ(ci.numColors(), numColors);
    EXPECT_TRUE(ci.hasAlpha());
    const auto* nu = ci.getNonUniform();
    ASSERT_NE(nu, nullptr);
    for (int i = 0; i < numColors; ++i) {
        EXPECT_EQ(nu->colors[i], colors[i]);
        EXPECT_EQ(nu->indices[i], indices[i]);
    }

    // Reset again
    ci.reset();
    EXPECT_TRUE(ci.isUniform());
    EXPECT_EQ(ci.numColors(), 1);
    EXPECT_EQ(ci.getUniform()->getTbgr(), ColorDef::white.getTbgr());
    EXPECT_EQ(ci.getNonUniform(), nullptr);
    EXPECT_FALSE(ci.hasAlpha());
}

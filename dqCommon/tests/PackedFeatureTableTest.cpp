// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core for PackedFeatureTable C++ port
// DanQing dqCommon — PackedFeatureTable tests
//
// Tests for binary packing of FeatureTable for GPU consumption.

#include "dqCommon/PackedFeatureTable.h"
#include "dqCommon/FeatureTable.h"

#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqBase;

// ============================================================================
// PackedFeatureTable tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for PackedFeatureTable C++ port
TEST(PackedFeatureTableTest, PackEmpty)
{
    FeatureTable table(10);
    auto packed = PackedFeatureTable::pack(table);
    EXPECT_EQ(packed.getNumFeatures(), 0u);
    EXPECT_EQ(packed.getDataSize(), 0u);
}

// Authored: no reference test exists in itwinjs-core for PackedFeatureTable C++ port
TEST(PackedFeatureTableTest, PackSingleFeature)
{
    FeatureTable table(10);
    DqId elemId(42);
    DqId subCatId(7);
    table.insert(Feature(elemId, subCatId, GeometryClass::Primary));

    auto packed = PackedFeatureTable::pack(table);
    EXPECT_EQ(packed.getNumFeatures(), 1u);
    EXPECT_EQ(packed.getNumSubCategories(), 1u);

    // Verify data size: 3 (features) + 2 (subcategories) = 5 uint32s
    EXPECT_EQ(packed.getDataSize(), 5u);

    // Verify feature data.
    auto feature = packed.getFeature(0);
    EXPECT_EQ(feature.elementId, 42u);
    EXPECT_EQ(feature.subCategoryId, 7u);
    EXPECT_EQ(feature.geometryClass, GeometryClass::Primary);
}

// Authored: no reference test exists in itwinjs-core for PackedFeatureTable C++ port
TEST(PackedFeatureTableTest, PackMultipleFeatures)
{
    FeatureTable table(10);
    table.insert(Feature(DqId(1), DqId(10), GeometryClass::Primary));
    table.insert(Feature(DqId(2), DqId(20), GeometryClass::Primary));
    table.insert(Feature(DqId(3), DqId(10), GeometryClass::Construction));

    auto packed = PackedFeatureTable::pack(table);
    EXPECT_EQ(packed.getNumFeatures(), 3u);
    // Two unique subcategories: 10 and 20
    EXPECT_EQ(packed.getNumSubCategories(), 2u);

    // Verify each feature.
    auto f0 = packed.getFeature(0);
    EXPECT_EQ(f0.elementId, 1u);
    EXPECT_EQ(f0.subCategoryId, 10u);

    auto f1 = packed.getFeature(1);
    EXPECT_EQ(f1.elementId, 2u);
    EXPECT_EQ(f1.subCategoryId, 20u);

    auto f2 = packed.getFeature(2);
    EXPECT_EQ(f2.elementId, 3u);
    EXPECT_EQ(f2.subCategoryId, 10u);
    EXPECT_EQ(f2.geometryClass, GeometryClass::Construction);
}

// Authored: no reference test exists in itwinjs-core for PackedFeatureTable C++ port
TEST(PackedFeatureTableTest, PackLargeId)
{
    FeatureTable table(10);
    uint64_t largeId = 0x123456789ABCDEF0ULL;
    table.insert(Feature(DqId(largeId), DqId(0), GeometryClass::Primary));

    auto packed = PackedFeatureTable::pack(table);
    auto feature = packed.getFeature(0);
    EXPECT_EQ(feature.elementId, largeId);
}

// Authored: no reference test exists in itwinjs-core for PackedFeatureTable C++ port
TEST(PackedFeatureTableTest, PackSubcategoryDedup)
{
    FeatureTable table(10);
    // Three features sharing two subcategories.
    table.insert(Feature(DqId(1), DqId(100), GeometryClass::Primary));
    table.insert(Feature(DqId(2), DqId(200), GeometryClass::Primary));
    table.insert(Feature(DqId(3), DqId(100), GeometryClass::Primary));

    auto packed = PackedFeatureTable::pack(table);
    EXPECT_EQ(packed.getNumSubCategories(), 2u);

    // Verify subcategory lookup.
    EXPECT_EQ(packed.getFeature(0).subCategoryId, 100u);
    EXPECT_EQ(packed.getFeature(1).subCategoryId, 200u);
    EXPECT_EQ(packed.getFeature(2).subCategoryId, 100u);
}

// Authored: no reference test exists in itwinjs-core for PackedFeatureTable C++ port
TEST(PackedFeatureTableTest, findFeature)
{
    FeatureTable table(10);
    table.insert(Feature(DqId(42), DqId(7), GeometryClass::Primary));

    auto packed = PackedFeatureTable::pack(table);

    auto found = packed.findFeature(0);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->elementId, 42u);
    EXPECT_EQ(found->subCategoryId, 7u);

    auto notFound = packed.findFeature(1);
    EXPECT_FALSE(notFound.has_value());
}

// Authored: no reference test exists in itwinjs-core for PackedFeatureTable C++ port
TEST(PackedFeatureTableTest, findElementId)
{
    FeatureTable table(10);
    table.insert(Feature(DqId(100), DqId(1), GeometryClass::Primary));
    table.insert(Feature(DqId(200), DqId(2), GeometryClass::Primary));

    auto packed = PackedFeatureTable::pack(table);

    auto id0 = packed.findElementId(0);
    ASSERT_TRUE(id0.has_value());
    EXPECT_EQ(*id0, 100u);

    auto id1 = packed.findElementId(1);
    ASSERT_TRUE(id1.has_value());
    EXPECT_EQ(*id1, 200u);

    EXPECT_FALSE(packed.findElementId(2).has_value());
}

// Authored: no reference test exists in itwinjs-core for PackedFeatureTable C++ port
TEST(PackedFeatureTableTest, getElementIdPair)
{
    FeatureTable table(10);
    uint64_t largeId = 0x123456789ABCDEF0ULL;
    table.insert(Feature(DqId(largeId), DqId(0), GeometryClass::Primary));

    auto packed = PackedFeatureTable::pack(table);

    uint32_t lo = 0, hi = 0;
    packed.getElementIdPair(0, lo, hi);
    EXPECT_EQ(lo, static_cast<uint32_t>(largeId & 0xFFFFFFFF));
    EXPECT_EQ(hi, static_cast<uint32_t>(largeId >> 32));
}

// Authored: no reference test exists in itwinjs-core for PackedFeatureTable C++ port
TEST(PackedFeatureTableTest, isUniform)
{
    FeatureTable table1(10);
    table1.insert(Feature(DqId(1), DqId(10), GeometryClass::Primary));
    auto packed1 = PackedFeatureTable::pack(table1);
    EXPECT_TRUE(packed1.isUniform());
    auto uniform = packed1.getUniform();
    ASSERT_TRUE(uniform.has_value());
    EXPECT_EQ(uniform->elementId, 1u);

    FeatureTable table2(10);
    table2.insert(Feature(DqId(1), DqId(10), GeometryClass::Primary));
    table2.insert(Feature(DqId(2), DqId(20), GeometryClass::Primary));
    auto packed2 = PackedFeatureTable::pack(table2);
    EXPECT_FALSE(packed2.isUniform());
    EXPECT_FALSE(packed2.getUniform().has_value());
}

// Authored: no reference test exists in itwinjs-core for PackedFeatureTable C++ port
TEST(PackedFeatureTableTest, BatchType)
{
    FeatureTable table(10, DqId(0), BatchType::VolumeClassifier);
    table.insert(Feature(DqId(1), DqId(10), GeometryClass::Primary));

    auto packed = PackedFeatureTable::pack(table);
    EXPECT_TRUE(packed.isVolumeClassifier());
    EXPECT_FALSE(packed.isPlanarClassifier());
    EXPECT_EQ(packed.getType(), BatchType::VolumeClassifier);
}

// Authored: no reference test exists in itwinjs-core for PackedFeatureTable C++ port
TEST(PackedFeatureTableTest, unpack)
{
    FeatureTable table(10);
    table.insert(Feature(DqId(1), DqId(10), GeometryClass::Primary));
    table.insert(Feature(DqId(2), DqId(20), GeometryClass::Construction));

    auto packed = PackedFeatureTable::pack(table);
    auto unpacked = packed.unpack();

    EXPECT_EQ(unpacked.getSize(), 2);
    auto f0 = unpacked.findFeature(0);
    ASSERT_TRUE(f0.has_value());
    EXPECT_EQ(f0->elementId, DqId(1));
    EXPECT_EQ(f0->subCategoryId, DqId(10));
    EXPECT_EQ(f0->geometryClass, GeometryClass::Primary);

    auto f1 = unpacked.findFeature(1);
    ASSERT_TRUE(f1.has_value());
    EXPECT_EQ(f1->elementId, DqId(2));
    EXPECT_EQ(f1->geometryClass, GeometryClass::Construction);
}

// Authored: no reference test exists in itwinjs-core for PackedFeatureTable C++ port
TEST(PackedFeatureTableTest, AnimationNodeId)
{
    FeatureTable table(10);
    table.insert(Feature(DqId(1), DqId(10), GeometryClass::Primary));

    auto packed = PackedFeatureTable::pack(table);
    // No animation nodes populated → returns 0
    EXPECT_EQ(packed.getAnimationNodeId(0), 0u);
    EXPECT_EQ(packed.getAnimationNodeId(999), 0u);
}

// Ported from: itwinjs-core core/common/src/internal/PackedFeatureTable.ts
//              constructor (:35-58) + getFeature (:100+) —— 布局契约测试；
//              参考无独立 PackedFeatureTable.test.ts（用例在 FeatureTable.test.ts
//              的 pack 往返中），本用例按线上 packed words 布局自写。
// Authored: no reference test exists in itwinjs-core for constructing
//           PackedFeatureTable directly from imdl wire words.
TEST(PackedFeatureTableTest, ConstructFromWireWords)
{
    // 2 features + 1 subcategory tail:
    //   feature0: elementId 0x0000000100000002, subCatIdx 0 | class Primary(0)<<24
    //   feature1: elementId 0x0000000000000005, subCatIdx 0 | class Construction(1)<<24
    //   subcat[0]: 0x0000000200000007
    std::vector<uint32_t> words = {
        0x00000002u, 0x00000001u, 0x00000000u,  // f0: lo, hi, idx|class
        0x00000005u, 0x00000000u, 0x01000000u,  // f1
        0x00000007u, 0x00000002u,               // subcat[0] lo, hi
    };
    dqCommon::PackedFeatureTable table(words, /*modelId*/0, /*numFeatures*/2,
                                       dqCommon::BatchType::Primary);
    EXPECT_EQ(table.getNumFeatures(), 2u);
    EXPECT_EQ(table.getNumSubCategories(), 1u);

    auto f0 = table.findFeature(0);
    ASSERT_TRUE(f0.has_value());
    EXPECT_EQ(f0->elementId, 0x0000000100000002ull);
    EXPECT_EQ(f0->subCategoryId, 0x0000000200000007ull);
    EXPECT_EQ(f0->geometryClass, dqCommon::GeometryClass::Primary);

    auto f1 = table.findFeature(1);
    ASSERT_TRUE(f1.has_value());
    EXPECT_EQ(f1->elementId, 0x5ull);
    EXPECT_EQ(f1->geometryClass, dqCommon::GeometryClass::Construction);

    // unpack() 往返：FeatureTable 侧逐 feature 可达（Task 2 的 createBatch 入口）。
    dqCommon::FeatureTable unpacked = table.unpack();
    EXPECT_EQ(unpacked.getSize(), 2);
}

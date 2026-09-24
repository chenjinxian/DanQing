// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — GeometryStreamIterator unit tests
//
// Authored: no reference tests exist in itwinjs-core for GeometryStreamIterator C++ port
#include <dqCommon/GeometryStreamBuilder.h>
#include <dqCommon/GeometryStreamIterator.h>

#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqGeom;
using namespace dqBase;

// Authored: no reference tests exist in itwinjs-core for GeometryStreamIterator C++ port
TEST(GeometryStreamIteratorTest, EmptyStream)
{
    std::vector<GeometryStreamEntry> entries;
    GeometryStreamIterator iter(entries);

    EXPECT_EQ(iter.getSize(), 0u);
    EXPECT_FALSE(iter.hasNext());
    EXPECT_FALSE(iter.next().has_value());
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamIterator C++ port
TEST(GeometryStreamIteratorTest, SingleEntry)
{
    GeometryStreamBuilder builder;
    GeometryAppearanceProps app;
    app.subCategory = DqId(1);
    builder.appendSubCategoryChange(app);

    GeometryStreamIterator iter(builder.getEntries());
    EXPECT_EQ(iter.getSize(), 1u);
    EXPECT_TRUE(iter.hasNext());

    auto entry = iter.next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->opcode, ElementGeometryOpcode::BasicSymbology);
    ASSERT_NE(entry->appearance, nullptr);
    EXPECT_EQ(entry->appearance->subCategory, DqId(1));
    EXPECT_FALSE(iter.hasNext());
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamIterator C++ port
TEST(GeometryStreamIteratorTest, MultipleEntries)
{
    GeometryStreamBuilder builder;

    // SubGraphicRange
    Placement3d placement;
    placement.bbox = Range3d::CreateXYZXYZ(0, 0, 0, 10, 10, 10);
    builder.setLocalToWorld3d(placement);

    // Appearance
    GeometryAppearanceProps app;
    app.color = 0xFF0000u;
    builder.appendSubCategoryChange(app);

    // BRep
    BRepDataProps brep;
    brep.data = "solid_data";
    brep.type = BRepType::Solid;
    builder.appendBRepData(brep);

    GeometryStreamIterator iter(builder.getEntries());
    EXPECT_EQ(iter.getSize(), 3u);

    // Entry 0: SubGraphicRange
    {
        auto e = iter.next();
        ASSERT_TRUE(e.has_value());
        EXPECT_EQ(e->opcode, ElementGeometryOpcode::SubGraphicRange);
        EXPECT_NE(e->subRange, nullptr);
    }

    // Entry 1: BasicSymbology
    {
        auto e = iter.next();
        ASSERT_TRUE(e.has_value());
        EXPECT_EQ(e->opcode, ElementGeometryOpcode::BasicSymbology);
        ASSERT_NE(e->appearance, nullptr);
        EXPECT_EQ(e->appearance->color, 0xFF0000u);
    }

    // Entry 2: BRep
    {
        auto e = iter.next();
        ASSERT_TRUE(e.has_value());
        EXPECT_EQ(e->opcode, ElementGeometryOpcode::BRep);
        ASSERT_NE(e->brep, nullptr);
        EXPECT_EQ(e->brep->data, "solid_data");
    }

    EXPECT_FALSE(iter.hasNext());
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamIterator C++ port
TEST(GeometryStreamIteratorTest, Reset)
{
    GeometryStreamBuilder builder;
    builder.appendSubCategoryChange(GeometryAppearanceProps{});
    builder.appendFill(AreaFillProps{});

    GeometryStreamIterator iter(builder.getEntries());
    iter.next();
    iter.next();
    EXPECT_FALSE(iter.hasNext());

    iter.reset();
    EXPECT_TRUE(iter.hasNext());
    EXPECT_EQ(iter.getIndex(), 0u);
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamIterator C++ port
TEST(GeometryStreamIteratorTest, GetCurrent)
{
    GeometryStreamBuilder builder;
    GeometryMaterialProps mat;
    mat.materialId = DqId(42);
    builder.appendMaterial(mat);

    GeometryStreamIterator iter(builder.getEntries());

    // GetCurrent doesn't advance
    auto e1 = iter.getCurrent();
    EXPECT_EQ(e1.opcode, ElementGeometryOpcode::Material);
    EXPECT_EQ(iter.getIndex(), 0u);

    auto e2 = iter.getCurrent();
    EXPECT_EQ(e2.opcode, ElementGeometryOpcode::Material);
    EXPECT_EQ(iter.getIndex(), 0u);

    // next advances
    iter.next();
    EXPECT_EQ(iter.getIndex(), 1u);
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamIterator C++ port
TEST(GeometryStreamIteratorTest, findNext)
{
    GeometryStreamBuilder builder;
    builder.appendSubCategoryChange(GeometryAppearanceProps{});
    builder.appendFill(AreaFillProps{});
    builder.appendMaterial(GeometryMaterialProps{});
    builder.appendBRepData(BRepDataProps{});

    GeometryStreamIterator iter(builder.getEntries());

    // findNext skips non-matching entries
    auto found = iter.findNext(ElementGeometryOpcode::Material);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->opcode, ElementGeometryOpcode::Material);

    // Iterator is now past the material entry
    EXPECT_EQ(iter.getIndex(), 3u);  // after BasicSymbology(0), Fill(1), Material(2)

    // Find BRep (index 3)
    auto brep = iter.findNext(ElementGeometryOpcode::BRep);
    ASSERT_TRUE(brep.has_value());
    EXPECT_EQ(brep->opcode, ElementGeometryOpcode::BRep);
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamIterator C++ port
TEST(GeometryStreamIteratorTest, isViewIndependent)
{
    GeometryStreamBuilder builder;
    builder.setViewIndependent(true);
    builder.appendSubCategoryChange(GeometryAppearanceProps{});

    GeometryStreamIterator iter(builder.getEntries());
    EXPECT_TRUE(iter.isViewIndependent());
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamIterator C++ port
TEST(GeometryStreamIteratorTest, getLocalRange)
{
    GeometryStreamBuilder builder;
    Placement3d placement;
    placement.bbox = Range3d::CreateXYZXYZ(0, 0, 0, 100, 200, 300);
    builder.setLocalToWorld3d(placement);

    GeometryStreamIterator iter(builder.getEntries());
    auto range = iter.getLocalRange();
    ASSERT_TRUE(range.has_value());
    EXPECT_TRUE(range->low.IsEqual(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(range->high.IsEqual(Point3d::From(100, 200, 300)));
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamIterator C++ port
TEST(GeometryStreamIteratorTest, PartReference)
{
    GeometryStreamBuilder builder;
    GeometryPartInstanceProps part;
    part.partId = DqId(99);
    part.originX = 5.0;
    part.originY = 10.0;
    part.originZ = 15.0;
    builder.appendGeometryPart3d(part);

    GeometryStreamIterator iter(builder.getEntries());
    auto e = iter.next();
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->opcode, ElementGeometryOpcode::PartReference);
    ASSERT_NE(e->partRef, nullptr);
    EXPECT_EQ(e->partRef->partId, DqId(99));
    EXPECT_NEAR(e->partRef->originX, 5.0, 1e-10);
}

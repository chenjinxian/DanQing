// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — EntityState and ModelState tests
//
// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
#include <gtest/gtest.h>

#include <dqApp/EntityState.h>

using namespace dqApp;
using namespace dqBase;
using namespace dqGeom;

// EntityState tests
// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
TEST(EntityState, BasicProperties)
{
    const ElementState elem(DqId(42), DqId(1), "BisCore:Element");
    EXPECT_EQ(elem.GetId(), DqId(42));
    EXPECT_EQ(elem.getModelId(), DqId(1));
    EXPECT_EQ(elem.GetClassFullName(), "BisCore:Element");
}

// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
TEST(EntityState, Equality)
{
    const ElementState a(DqId(1), DqId(10));
    const ElementState b(DqId(1), DqId(10));
    const ElementState c(DqId(2), DqId(10));
    EXPECT_TRUE(a.equals(b));
    EXPECT_FALSE(a.equals(c));
}

// ElementState tests
// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
TEST(ElementState, UserLabel)
{
    ElementState elem(DqId(1), DqId(10));
    EXPECT_TRUE(elem.GetUserLabel().empty());

    elem.SetUserLabel("Test Element");
    EXPECT_EQ(elem.GetUserLabel(), "Test Element");
}

// ModelState tests
// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
TEST(ModelState, BasicProperties)
{
    ModelState model(DqId(100), "TestModel");
    EXPECT_EQ(model.GetId(), DqId(100));
    EXPECT_EQ(model.GetName(), "TestModel");
    EXPECT_FALSE(model.IsGeometricModel());
    EXPECT_FALSE(model.is3d());
    EXPECT_TRUE(model.Is2d());
}

// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
TEST(ModelState, ParentModel)
{
    ModelState model(DqId(100), "TestModel");
    EXPECT_EQ(model.GetParentModelId(), DqId());  // default invalid

    model.SetParentModelId(DqId(50));
    EXPECT_EQ(model.GetParentModelId(), DqId(50));
}

// GeometricModelState tests
// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
TEST(GeometricModelState, IsGeometric)
{
    GeometricModel3dState model(DqId(100), "SpatialModel");
    EXPECT_TRUE(model.IsGeometricModel());
    EXPECT_TRUE(model.is3d());
    EXPECT_FALSE(model.Is2d());
}

// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
TEST(GeometricModelState, ModelRange)
{
    GeometricModel3dState model(DqId(100), "TestModel");
    const auto range = Range3d::CreateXYZXYZ(0, 0, 0, 1000, 1000, 1000);
    model.SetModelRange(range);

    const auto& r = model.GetModelRange();
    EXPECT_TRUE(r.low.AlmostEqual(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(r.high.AlmostEqual(Point3d::From(1000, 1000, 1000)));
}

// GeometricModel2dState tests
// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
TEST(GeometricModel2dState, Is2d)
{
    GeometricModel2dState model(DqId(100), "DrawingModel");
    EXPECT_TRUE(model.IsGeometricModel());
    EXPECT_FALSE(model.is3d());
    EXPECT_TRUE(model.Is2d());
}

// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
TEST(GeometricModel2dState, OriginDelta)
{
    GeometricModel2dState model(DqId(100), "DrawingModel");
    model.SetOrigin(10.0, 20.0);
    model.SetDelta(500.0, 300.0);

    EXPECT_DOUBLE_EQ(model.GetOriginX(), 10.0);
    EXPECT_DOUBLE_EQ(model.GetOriginY(), 20.0);
    EXPECT_DOUBLE_EQ(model.GetDeltaX(), 500.0);
    EXPECT_DOUBLE_EQ(model.GetDeltaY(), 300.0);
}

// SpatialModelState tests
// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
TEST(SpatialModelState, IsSpatial)
{
    SpatialModelState model(DqId(100), "SpatialModel");
    EXPECT_TRUE(model.IsGeometricModel());
    EXPECT_TRUE(model.is3d());
    EXPECT_TRUE(model.IsSpatialModel());
}

// CategorySelectorState tests
// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
TEST(CategorySelectorState, BasicProperties)
{
    CategorySelectorState selector(DqId(1), "Default");
    EXPECT_EQ(selector.GetId(), DqId(1));
    EXPECT_EQ(selector.GetName(), "Default");
    EXPECT_TRUE(selector.GetCategories().empty());
}

// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
TEST(CategorySelectorState, AddCategory)
{
    CategorySelectorState selector(DqId(1));
    selector.AddCategory(DqId(10));
    selector.AddCategory(DqId(20));
    selector.AddCategory(DqId(30));

    EXPECT_EQ(selector.GetCategories().size(), 3u);
    EXPECT_TRUE(selector.Contains(DqId(10)));
    EXPECT_TRUE(selector.Contains(DqId(20)));
    EXPECT_TRUE(selector.Contains(DqId(30)));
    EXPECT_FALSE(selector.Contains(DqId(40)));
}

// ModelSelectorState tests
// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
TEST(ModelSelectorState, BasicProperties)
{
    ModelSelectorState selector(DqId(1), "Default");
    EXPECT_EQ(selector.GetId(), DqId(1));
    EXPECT_EQ(selector.GetName(), "Default");
    EXPECT_TRUE(selector.getModels().empty());
}

// Ported from: itwinjs-core core/frontend/src/test/EntityState.test.ts
TEST(ModelSelectorState, addModel)
{
    ModelSelectorState selector(DqId(1));
    selector.addModel(DqId(100));
    selector.addModel(DqId(200));

    EXPECT_EQ(selector.getModels().size(), 2u);
    EXPECT_TRUE(selector.Contains(DqId(100)));
    EXPECT_TRUE(selector.Contains(DqId(200)));
    EXPECT_FALSE(selector.Contains(DqId(300)));
}

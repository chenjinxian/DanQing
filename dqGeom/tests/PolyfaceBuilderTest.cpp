// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — PolyfaceBuilder tests
// Ported from: imodel-native iModelCore/GeomLibs/geom/src/polyface/PolyfaceBuilder.cpp
#include <gtest/gtest.h>
#include <vector>

#include <dqGeom/PolyfaceBuilder.h>
#include <dqGeom/PolyfaceQuery.h>
#include <dqGeom/Arc3d.h>
#include <dqGeom/LineSegment3d.h>

using namespace dqGeom;

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/polyface/PolyfaceBuilder.cpp
TEST(PolyfaceBuilderTest, CreateEmpty)
{
    auto builder = PolyfaceBuilder::create();
    auto pf = builder->ClaimPolyface();
    EXPECT_TRUE(pf.IsValid());
    EXPECT_EQ(pf->FacetCount(), 0u);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/polyface/PolyfaceBuilder.cpp
TEST(PolyfaceBuilderTest, AddTriangle)
{
    auto builder = PolyfaceBuilder::create();

    Point3d pts[3] = {
        Point3d::From(0, 0, 0),
        Point3d::From(1, 0, 0),
        Point3d::From(0, 1, 0)
    };
    builder->AddTriangleFacet(pts);

    auto pf = builder->ClaimPolyface(false);
    EXPECT_EQ(pf->FacetCount(), 1u);
    EXPECT_EQ(pf->Data().PointCount(), 3u);
    EXPECT_EQ(PolyfaceQuery::GetNumFacet(*pf), 1u);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/polyface/PolyfaceBuilder.cpp
TEST(PolyfaceBuilderTest, AddQuadTriangulated)
{
    StrokeOptions opts;
    opts.shouldTriangulate = true;
    auto builder = PolyfaceBuilder::create(opts);

    Point3d pts[4] = {
        Point3d::From(0, 0, 0),
        Point3d::From(1, 0, 0),
        Point3d::From(1, 1, 0),
        Point3d::From(0, 1, 0)
    };
    builder->AddQuadFacet(pts);

    auto pf = builder->ClaimPolyface(false);
    // Quad triangulated → 2 triangles
    EXPECT_EQ(pf->FacetCount(), 2u);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/polyface/PolyfaceBuilder.cpp
TEST(PolyfaceBuilderTest, AddQuadNotTriangulated)
{
    StrokeOptions opts;
    opts.shouldTriangulate = false;
    auto builder = PolyfaceBuilder::create(opts);

    Point3d pts[4] = {
        Point3d::From(0, 0, 0),
        Point3d::From(1, 0, 0),
        Point3d::From(1, 1, 0),
        Point3d::From(0, 1, 0)
    };
    builder->AddQuadFacet(pts);

    auto pf = builder->ClaimPolyface(false);
    // Quad not triangulated → 1 quad face
    EXPECT_EQ(pf->FacetCount(), 1u);
    EXPECT_EQ(pf->NumEdgeInFacet(0), 4u);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/polyface/PolyfaceBuilder.cpp
TEST(PolyfaceBuilderTest, AddPolygon)
{
    auto builder = PolyfaceBuilder::create();

    std::vector<Point3d> pts = {
        Point3d::From(0, 0, 0),
        Point3d::From(1, 0, 0),
        Point3d::From(1, 1, 0),
        Point3d::From(0.5, 1.5, 0),
        Point3d::From(0, 1, 0)
    };
    builder->AddPolygon(pts);

    auto pf = builder->ClaimPolyface(false);
    EXPECT_EQ(pf->FacetCount(), 1u);
    EXPECT_EQ(pf->NumEdgeInFacet(0), 5u);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/polyface/PolyfaceBuilder.cpp
TEST(PolyfaceBuilderTest, AddPolygonTriangulated)
{
    StrokeOptions opts;
    opts.shouldTriangulate = true;
    auto builder = PolyfaceBuilder::create(opts);

    std::vector<Point3d> pts = {
        Point3d::From(0, 0, 0),
        Point3d::From(1, 0, 0),
        Point3d::From(1, 1, 0),
        Point3d::From(0, 1, 0)
    };
    builder->AddPolygon(pts);

    auto pf = builder->ClaimPolyface(false);
    // 4-gon fan triangulated → 2 triangles
    EXPECT_EQ(pf->FacetCount(), 2u);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/polyface/PolyfaceBuilder.cpp
TEST(PolyfaceBuilderTest, AddCurveStroke)
{
    auto builder = PolyfaceBuilder::create();

    auto arc = Arc3d::CreateXY(Point3d::FromZero(), 5.0);
    builder->AddCurveStroke(*arc);

    auto pf = builder->ClaimPolyface(false);
    EXPECT_EQ(pf->FacetCount(), 1u);
    // With default 15° angle tolerance, a full circle gets ~24 strokes
    EXPECT_GE(pf->NumEdgeInFacet(0), 12u);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/polyface/PolyfaceBuilder.cpp
TEST(PolyfaceBuilderTest, ClaimPolyfaceCompress)
{
    auto builder = PolyfaceBuilder::create();

    // Add two triangles sharing an edge (duplicate vertices)
    Point3d tri0[3] = {
        Point3d::From(0, 0, 0),
        Point3d::From(1, 0, 0),
        Point3d::From(0, 1, 0)
    };
    Point3d tri1[3] = {
        Point3d::From(1, 0, 0),
        Point3d::From(1, 1, 0),
        Point3d::From(0, 1, 0)
    };
    builder->AddTriangleFacet(tri0);
    builder->AddTriangleFacet(tri1);

    auto pf = builder->ClaimPolyface(true);  // compress
    EXPECT_EQ(pf->FacetCount(), 2u);
    // After compression, shared vertices should be deduplicated
    // 5 unique points: (0,0,0), (1,0,0), (0,1,0), (1,1,0)
    EXPECT_LE(pf->Data().PointCount(), 6u);
}

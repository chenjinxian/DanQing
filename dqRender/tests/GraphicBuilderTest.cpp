// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core for GraphicAssembler.addLineString2d /
//           addPointString2d / addShape2d (TS covers 2d variants via integration, not unit).
//           Behaviors under test (2d→3d lift via copy2dTo3d, z=zDepth applied, x/y preserved)
//           are defined by core/frontend/src/common/render/GraphicAssembler.ts.
// DanQing dqRender — GraphicBuilder 2d-variant delegation tests.

#include "dqRender/GraphicBuilder.h"
#include "dqRender/RenderGraphic.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/Frustum.h>
#include <dqCommon/LinePixels.h>
#include <dqGeom/AngleSweep.h>
#include <dqGeom/Arc3d.h>
#include <dqGeom/Box.h>
#include <dqGeom/CurvePrimitive.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/LineString3d.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Path.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Polyface.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/SolidPrimitive.h>

#include <gtest/gtest.h>
#include <vector>

using namespace dqRender;
using namespace dqGeom;

namespace {
constexpr double kTol = 1.0e-9;

// Test-only GraphicBuilder that records the 3d point arrays passed to each add* virtual.
// Used to verify the 2d variants delegate correctly (lift 2d → 3d at z=zDepth).
class CapturingGraphicBuilder : public GraphicBuilder {
public:
    // Explicit public ctor delegates to the protected base ctor (ref GraphicBuilder is abstract).
    explicit CapturingGraphicBuilder(const GraphicBuilderOptions& opts) : GraphicBuilder(opts) {}

    std::vector<Point3d> lastLineString;
    std::vector<Point3d> lastPointString;
    std::vector<Point3d> lastShape;
    int lineStringCalls = 0;
    int shapeCalls = 0;
    int pathCalls = 0;
    int loopCalls = 0;
    int polyfaceCalls = 0;
    int solidCalls = 0;
    size_t lastPathCurveCount = 0;
    size_t lastLoopCurveCount = 0;
    size_t lastPolyfaceFacetCount = 0;
    dqGeom::SolidPrimitiveType lastSolidType{};

    // setSymbology/activateGraphicParams/setBlankingFill inherited (concrete on the GraphicBuilder base).

    void addPointString(const Point3d* points, size_t count) override {
        lastPointString.assign(points, points + count);
    }
    void addLineString(const Point3d* points, size_t count) override {
        lastLineString.assign(points, points + count);
        ++lineStringCalls;
    }
    void addShape(const Point3d* points, size_t count) override {
        lastShape.assign(points, points + count);
        ++shapeCalls;
    }
    // Capture chain-routing (addArc/addCurvePrimitive build a 1-curve Path/Loop).
    void addPath(const Path& path) override { ++pathCalls; lastPathCurveCount = path.Curves().size(); }
    void addLoop(const Loop& loop) override { ++loopCalls; lastLoopCurveCount = loop.Curves().size(); }
    // Capture polyface routing.
    void addPolyface(const Polyface& meshData, bool /*filled*/) override {
        ++polyfaceCalls;
        lastPolyfaceFacetCount = meshData.FacetCount();
    }
    // Capture solid routing.
    void addSolidPrimitive(const SolidPrimitive& primitive) override {
        ++solidCalls;
        lastSolidType = primitive.GetSolidPrimitiveType();
    }

    RenderGraphic* finish() override { return nullptr; }
};

// Assert each lifted point has original x,y and the expected zDepth.
void ExpectLiftedTo(const std::vector<Point3d>& got, const Point2d* src2d, size_t count, double zDepth) {
    ASSERT_EQ(count, got.size());
    for (size_t i = 0; i < count; ++i) {
        EXPECT_NEAR(src2d[i].x, got[i].x, kTol);
        EXPECT_NEAR(src2d[i].y, got[i].y, kTol);
        EXPECT_NEAR(zDepth, got[i].z, kTol);
    }
}
} // namespace

// Authored: addLineString2d lifts each Point2d to z=zDepth and delegates to addLineString.
TEST(GraphicBuilderTest, AddLineString2dLiftsToZDepth) {
    GraphicBuilderOptions opts;
    CapturingGraphicBuilder b(opts);
    const Point2d pts[] = {Point2d::From(1.0, 2.0), Point2d::From(3.0, 4.0), Point2d::From(5.0, 6.0)};

    b.addLineString2d(pts, 3, 7.5);
    ExpectLiftedTo(b.lastLineString, pts, 3, 7.5);
}

// Authored: addPointString2d delegates to addPointString with z=zDepth.
TEST(GraphicBuilderTest, AddPointString2dLiftsToZDepth) {
    GraphicBuilderOptions opts;
    CapturingGraphicBuilder b(opts);
    const Point2d pts[] = {Point2d::From(-1.0, 0.0), Point2d::From(2.5, -3.5)};

    b.addPointString2d(pts, 2, 0.0);
    ExpectLiftedTo(b.lastPointString, pts, 2, 0.0);
}

// Authored: addShape2d delegates to addShape with z=zDepth.
TEST(GraphicBuilderTest, AddShape2dLiftsToZDepth) {
    GraphicBuilderOptions opts;
    CapturingGraphicBuilder b(opts);
    const Point2d pts[] = {Point2d::From(0.0, 0.0), Point2d::From(10.0, 0.0),
                           Point2d::From(10.0, 10.0), Point2d::From(0.0, 10.0)};

    b.addShape2d(pts, 4, -2.0);
    ExpectLiftedTo(b.lastShape, pts, 4, -2.0);
}

// Authored: 2d variants are independent — each routes to its own 3d virtual.
TEST(GraphicBuilderTest, TwoDeeVariantsRouteToDistinctVirtuals) {
    GraphicBuilderOptions opts;
    CapturingGraphicBuilder b(opts);
    const Point2d pts[] = {Point2d::From(1.0, 1.0), Point2d::From(2.0, 2.0)};

    b.addLineString2d(pts, 2, 1.0);
    b.addPointString2d(pts, 2, 2.0);
    b.addShape2d(pts, 2, 3.0);

    EXPECT_EQ(2u, b.lastLineString.size());
    EXPECT_EQ(2u, b.lastPointString.size());
    EXPECT_EQ(2u, b.lastShape.size());
    EXPECT_NEAR(1.0, b.lastLineString[0].z, kTol);
    EXPECT_NEAR(2.0, b.lastPointString[0].z, kTol);
    EXPECT_NEAR(3.0, b.lastShape[0].z, kTol);
}

// Authored: addCurvePrimitive(LineString) dispatches to addLineString with the linestring's points.
// Behavior defined by GraphicAssembler.ts:265-284.
TEST(GraphicBuilderTest, AddCurvePrimitiveLineStringRoutesToLineString) {
    CapturingGraphicBuilder b(GraphicBuilderOptions{});
    auto ls = LineString3d::create({Point3d::From(0, 0, 0), Point3d::From(1, 2, 3), Point3d::From(4, 5, 6)});
    b.addCurvePrimitive(*ls);
    EXPECT_EQ(1, b.lineStringCalls);
    ASSERT_EQ(3u, b.lastLineString.size());
    EXPECT_NEAR(1.0, b.lastLineString[1].x, kTol);
}

// Authored: addCurvePrimitive(Arc) dispatches to addArc → addPath (open arc = 1-curve Path).
TEST(GraphicBuilderTest, AddCurvePrimitiveArcRoutesToPath) {
    CapturingGraphicBuilder b(GraphicBuilderOptions{});
    auto arc = Arc3d::CreateXY(Point3d::FromZero(), 1.0);
    b.addCurvePrimitive(*arc);
    EXPECT_EQ(1, b.pathCalls);
    EXPECT_EQ(1u, b.lastPathCurveCount);
}

// Authored: addArc(open, not filled) builds a 1-curve Path (GraphicAssembler.ts:219-236).
TEST(GraphicBuilderTest, AddArcOpenBuildsPath) {
    CapturingGraphicBuilder b(GraphicBuilderOptions{});
    auto arc = Arc3d::CreateXY(Point3d::FromZero(), 1.0);
    b.addArc(*arc, /*isEllipse=*/false, /*filled=*/false);
    EXPECT_EQ(1, b.pathCalls);
    EXPECT_EQ(0, b.loopCalls);
    EXPECT_EQ(1u, b.lastPathCurveCount);
}

// Authored: addArc(isEllipse=true) builds a Loop (filled region semantics).
TEST(GraphicBuilderTest, AddArcEllipseBuildsLoop) {
    CapturingGraphicBuilder b(GraphicBuilderOptions{});
    auto arc = Arc3d::CreateXY(Point3d::FromZero(), 1.0);
    b.addArc(*arc, /*isEllipse=*/true, /*filled=*/false);
    EXPECT_EQ(1, b.loopCalls);
    EXPECT_EQ(0, b.pathCalls);
    EXPECT_EQ(1u, b.lastLoopCurveCount);
}

// Authored: addRangeBox emits the 12-edge wireframe as 4 line strings (GraphicAssembler.ts:347-386).
TEST(GraphicBuilderTest, AddRangeBoxEmitsWireframe) {
    CapturingGraphicBuilder b(GraphicBuilderOptions{});
    auto range = Range3d::create({Point3d::From(0, 0, 0), Point3d::From(1, 1, 1)});
    b.addRangeBox(range, /*solid=*/false);
    EXPECT_EQ(4, b.lineStringCalls);
    EXPECT_EQ(0, b.shapeCalls);
}

// Authored: addFrustum routes through addRangeBoxFromCorners (same 4-line wireframe).
TEST(GraphicBuilderTest, AddFrustumEmitsWireframe) {
    CapturingGraphicBuilder b(GraphicBuilderOptions{});
    auto range = Range3d::create({Point3d::From(0, 0, 0), Point3d::From(2, 2, 2)});
    auto frustum = dqCommon::Frustum::fromRange(range);
    b.addFrustum(frustum);
    EXPECT_EQ(4, b.lineStringCalls);
}

// Authored: addRangeBoxSidesFromCorners emits the 6 box faces as shapes (GraphicAssembler.ts:389-426).
TEST(GraphicBuilderTest, AddRangeBoxSidesEmitsSixShapes) {
    CapturingGraphicBuilder b(GraphicBuilderOptions{});
    auto range = Range3d::create({Point3d::From(0, 0, 0), Point3d::From(1, 1, 1)});
    auto frustum = dqCommon::Frustum::fromRange(range);
    b.addFrustumSides(frustum);
    EXPECT_EQ(6, b.shapeCalls);
    EXPECT_EQ(0, b.lineStringCalls);
}

// Authored: addPolyface routes to the virtual with the mesh's facet count visible.
// (GraphicAssembler.ts:290 — triangulation reuses addShape, exercised via PrimitiveBuilder/addShape coverage.)
TEST(GraphicBuilderTest, AddPolyfaceRoutesToVirtual) {
    CapturingGraphicBuilder b(GraphicBuilderOptions{});
    auto pf = IndexedPolyface::create();
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(1, 0, 0));
    pf->AddPoint(Point3d::From(0, 1, 0));
    pf->AddPointIndex(1); pf->AddPointIndex(2); pf->AddPointIndex(3);
    pf->TerminateFacet();
    b.addPolyface(*pf, false);
    EXPECT_EQ(1, b.polyfaceCalls);
    EXPECT_EQ(1u, b.lastPolyfaceFacetCount);
}

// Authored: addSolidPrimitive routes to the virtual with the solid type visible (GraphicAssembler.ts:295).
// (Box tessellation → facets is exercised at the dqGeom level in SolidPrimitiveTest.)
TEST(GraphicBuilderTest, AddSolidPrimitiveRoutesToVirtual) {
    CapturingGraphicBuilder b(GraphicBuilderOptions{});
    auto box = Box::CreateRange(Range3d::create({Point3d::From(0, 0, 0), Point3d::From(1, 1, 1)}), true);
    b.addSolidPrimitive(*box);
    EXPECT_EQ(1, b.solidCalls);
    EXPECT_EQ(SolidPrimitiveType::Box, b.lastSolidType);
}

// Ported from: itwinjs-core core/frontend/src/test/render/GraphicBuilder.test.ts
//              describe("GraphicBuilder") / describe("generates normals") + describe("generates edges").
// DanQing adaptation: GraphicBuilderOptions has no viewport, so the `viewFlags.edgesRequired()` branch
// collapses to its default (true) → wantEdges default = isSceneGraphic(). The "never, if view flags do
// not require them" case (needs a viewport with edgesRequired=false) has no faithful DanQing equivalent and
// is omitted; all explicit-option cases are ported verbatim (GraphicBuilder.ts:150-152 resolution).
namespace {
const GraphicType kAllGraphicTypes[] = {
    GraphicType::ViewBackground, GraphicType::Scene, GraphicType::WorldDecoration,
    GraphicType::WorldOverlay, GraphicType::ViewOverlay,
};
}  // namespace

// Ported from: GraphicBuilder.test.ts "generates normals" / "for scene graphics only by default" (62-65).
TEST(GraphicBuilder, WantNormalsForSceneGraphicsOnlyByDefault) {
    for (auto type : kAllGraphicTypes) {
        CapturingGraphicBuilder b(GraphicBuilderOptions{.type = type});
        EXPECT_EQ(b.wantNormals(), type == GraphicType::Scene);
    }
}

// Ported from: GraphicBuilder.test.ts "generates normals" / "always if generating edges" (67-74).
TEST(GraphicBuilder, WantNormalsAlwaysIfGeneratingEdges) {
    for (auto type : kAllGraphicTypes) {
        {
            CapturingGraphicBuilder b(GraphicBuilderOptions{.type = type, .generateEdges = true});
            EXPECT_TRUE(b.wantNormals());
        }
        {
            CapturingGraphicBuilder b(GraphicBuilderOptions{.type = type, .generateEdges = false});
            EXPECT_EQ(b.wantNormals(), type == GraphicType::Scene);
        }
        {
            CapturingGraphicBuilder b(GraphicBuilderOptions{.type = type, .wantNormals = false, .generateEdges = true});
            EXPECT_FALSE(b.wantNormals());
        }
    }
}

// Ported from: GraphicBuilder.test.ts "generates normals" / "always if explicitly requested" (76-79).
TEST(GraphicBuilder, WantNormalsAlwaysIfExplicitlyRequested) {
    for (auto type : kAllGraphicTypes) {
        CapturingGraphicBuilder b(GraphicBuilderOptions{.type = type, .wantNormals = true});
        EXPECT_TRUE(b.wantNormals());
    }
}

// Ported from: GraphicBuilder.test.ts "generates normals" / "never if explicitly specified" (81-84).
TEST(GraphicBuilder, WantNormalsNeverIfExplicitlySpecified) {
    for (auto type : kAllGraphicTypes) {
        CapturingGraphicBuilder b(GraphicBuilderOptions{.type = type, .wantNormals = false});
        EXPECT_FALSE(b.wantNormals());
    }
}

// Ported from: GraphicBuilder.test.ts "generates edges" / "by default only for scene graphics, if view flags require them" (93-96).
TEST(GraphicBuilder, WantEdgesByDefaultOnlyForSceneGraphics) {
    for (auto type : kAllGraphicTypes) {
        CapturingGraphicBuilder b(GraphicBuilderOptions{.type = type});
        EXPECT_EQ(b.wantEdges(), type == GraphicType::Scene);
    }
}

// Ported from: GraphicBuilder.test.ts "generates edges" / "always if explicitly requested" (108-...) +
// "never if explicitly specified" (113-...).
TEST(GraphicBuilder, WantEdgesRespectsExplicitGenerateEdges) {
    for (auto type : kAllGraphicTypes) {
        {
            CapturingGraphicBuilder b(GraphicBuilderOptions{.type = type, .generateEdges = true});
            EXPECT_TRUE(b.wantEdges());
        }
        {
            CapturingGraphicBuilder b(GraphicBuilderOptions{.type = type, .generateEdges = false});
            EXPECT_FALSE(b.wantEdges());
        }
    }
}

// Authored: no reference test exists in itwinjs-core for the GraphicType predicates
// (isViewCoordinates/isWorldCoordinates/isSceneGraphic/isViewBackground/isOverlay); behaviors under test
// are defined by core/frontend/src/common/render/GraphicAssembler.ts:102-124.
TEST(GraphicBuilder, GraphicTypePredicates) {
    for (auto type : kAllGraphicTypes) {
        CapturingGraphicBuilder b(GraphicBuilderOptions{.type = type});
        EXPECT_EQ(b.isSceneGraphic(), type == GraphicType::Scene);
        EXPECT_EQ(b.isViewBackground(), type == GraphicType::ViewBackground);
        EXPECT_EQ(b.isOverlay(), type == GraphicType::ViewOverlay || type == GraphicType::WorldOverlay);
        const bool viewCoords = type == GraphicType::ViewBackground || type == GraphicType::ViewOverlay;
        EXPECT_EQ(b.isViewCoordinates(), viewCoords);
        EXPECT_EQ(b.isWorldCoordinates(), !viewCoords);
    }
}

// Authored: no reference unit test exists for GraphicAssembler.activateGraphicParams / setSymbology /
// setBlankingFill — itwinjs tests these via integration-level rendering-color checks (GraphicBuilder.test.ts
// "colors" describe) which require the full tessellation/mesh pipeline. These tests verify the storage
// contract directly via a probe subclass. Behaviors defined by GraphicAssembler.ts:130-132 / 435-437 / 445.
class SymbologyProbeBuilder : public GraphicBuilder {
public:
    SymbologyProbeBuilder() : GraphicBuilder(GraphicBuilderOptions{.type = GraphicType::Scene}) {}
    const dqCommon::GraphicParams& currentParams() const noexcept { return graphicParams(); }

    void addPointString(const Point3d*, size_t) override {}
    void addLineString(const Point3d*, size_t) override {}
    void addShape(const Point3d*, size_t) override {}
    void addPath(const Path&) override {}
    void addLoop(const Loop&) override {}
    void addPolyface(const Polyface&, bool) override {}
    void addSolidPrimitive(const SolidPrimitive&) override {}
    RenderGraphic* finish() override { return nullptr; }
};

TEST(GraphicBuilder, SetSymbologyStoresIntoGraphicParams) {
    SymbologyProbeBuilder b;
    const dqCommon::ColorDef line(dqCommon::ColorDef::from(10, 20, 30));
    b.setSymbology(line, dqCommon::ColorDef::black, 5, dqCommon::LinePixels::HiddenLine);
    EXPECT_EQ(b.currentParams().lineColor.getAbgr(), line.getAbgr());
    EXPECT_EQ(b.currentParams().rasterWidth, 5);
    EXPECT_EQ(b.currentParams().linePixels, dqCommon::LinePixels::HiddenLine);
}

TEST(GraphicBuilder, ActivateGraphicParamsStoresClone) {
    SymbologyProbeBuilder b;
    dqCommon::GraphicParams gp;
    gp.rasterWidth = 7;
    gp.linePixels = dqCommon::LinePixels::HiddenLine;
    b.activateGraphicParams(gp);
    EXPECT_EQ(b.currentParams().rasterWidth, 7);
    EXPECT_EQ(b.currentParams().linePixels, dqCommon::LinePixels::HiddenLine);
    // activateGraphicParams stores a clone; mutating the source afterward must not affect it.
    gp.rasterWidth = 99;
    EXPECT_EQ(b.currentParams().rasterWidth, 7);
}

TEST(GraphicBuilder, SetBlankingFillSetsBlankingFlag) {
    SymbologyProbeBuilder b;
    const dqCommon::ColorDef fill(dqCommon::ColorDef::from(40, 50, 60));
    b.setBlankingFill(fill);
    EXPECT_EQ(b.currentParams().fillColor.getAbgr(), fill.getAbgr());
    EXPECT_EQ(b.currentParams().fillFlags, dqCommon::FillFlags::Blanking);
}

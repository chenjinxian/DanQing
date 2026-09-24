// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Polyface tests
// Ported from: itwinjs-core core/geometry/src/test/polyface/Polyface.test.ts
#include <gtest/gtest.h>

#include <dqGeom/GeometryHandler.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/Polyface.h>
#include <dqGeom/PolyfaceQuery.h>

#include <vector>

using namespace dqGeom;

// ---------------------------------------------------------------------------
// IndexedPolyface tests
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core core/geometry/src/test/polyface/Polyface.test.ts
//              TEST(IndexedPolyfaceTest, CreateEmpty)
TEST(IndexedPolyfaceTest, CreateEmpty)
{
    auto pf = IndexedPolyface::create();
    EXPECT_EQ(pf->FacetCount(), 0u);
    EXPECT_EQ(pf->Data().PointCount(), 0u);
}

// Ported from: itwinjs-core core/geometry/src/test/polyface/Polyface.test.ts
//              TEST(IndexedPolyfaceTest, AddTriangle)
TEST(IndexedPolyfaceTest, AddTriangle)
{
    auto pf = IndexedPolyface::create(true, false);  // needNormals=true

    // Add 3 points (1-based indices: 1, 2, 3)
    EXPECT_EQ(pf->AddPoint(Point3d::From(0, 0, 0)), 1);
    EXPECT_EQ(pf->AddPoint(Point3d::From(1, 0, 0)), 2);
    EXPECT_EQ(pf->AddPoint(Point3d::From(0, 1, 0)), 3);

    // Add normal
    EXPECT_EQ(pf->AddNormal(Vector3d::From(0, 0, 1)), 1);

    // Add face loop
    pf->AddPointIndex(1, true);
    pf->AddPointIndex(2, true);
    pf->AddPointIndex(3, true);
    pf->TerminateFacet();

    EXPECT_EQ(pf->FacetCount(), 1u);
    EXPECT_EQ(pf->Data().PointCount(), 3u);
    EXPECT_EQ(pf->Data().IndexCount(), 3u);
    EXPECT_EQ(pf->NumEdgeInFacet(0), 3u);
}

// Ported from: itwinjs-core core/geometry/src/test/polyface/Polyface.test.ts
//              TEST(IndexedPolyfaceTest, AddQuadAndTriangle)
TEST(IndexedPolyfaceTest, AddQuadAndTriangle)
{
    auto pf = IndexedPolyface::create();

    // 4 points for quad + 1 extra for triangle
    pf->AddPoint(Point3d::From(0, 0, 0));  // 1
    pf->AddPoint(Point3d::From(1, 0, 0));  // 2
    pf->AddPoint(Point3d::From(1, 1, 0));  // 3
    pf->AddPoint(Point3d::From(0, 1, 0));  // 4

    // Quad face
    pf->AddPointIndex(1, true);
    pf->AddPointIndex(2, true);
    pf->AddPointIndex(3, true);
    pf->AddPointIndex(4, true);
    pf->TerminateFacet();

    EXPECT_EQ(pf->FacetCount(), 1u);
    EXPECT_EQ(pf->NumEdgeInFacet(0), 4u);

    // Triangle face (reuse points 1, 2, 3)
    pf->AddPointIndex(1, true);
    pf->AddPointIndex(2, true);
    pf->AddPointIndex(3, true);
    pf->TerminateFacet();

    EXPECT_EQ(pf->FacetCount(), 2u);
    EXPECT_EQ(pf->NumEdgeInFacet(1), 3u);
}

// Ported from: itwinjs-core core/geometry/src/test/polyface/Polyface.test.ts
//              TEST(IndexedPolyfaceTest, FacetNavigation)
TEST(IndexedPolyfaceTest, FacetNavigation)
{
    auto pf = IndexedPolyface::create();
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(1, 0, 0));
    pf->AddPoint(Point3d::From(0, 1, 0));

    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->AddPointIndex(3);
    pf->TerminateFacet();

    EXPECT_EQ(pf->FacetIndex0(0), 0u);
    EXPECT_EQ(pf->FacetIndex1(0), 3u);
}

// Ported from: itwinjs-core core/geometry/src/test/polyface/Polyface.test.ts
//              TEST(IndexedPolyfaceTest, Range)
TEST(IndexedPolyfaceTest, Range)
{
    auto pf = IndexedPolyface::create();
    pf->AddPoint(Point3d::From(-1, -2, -3));
    pf->AddPoint(Point3d::From(4, 5, 6));

    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->TerminateFacet();

    Range3d range = pf->Range();
    EXPECT_TRUE(range.low.AlmostEqual(Point3d::From(-1, -2, -3)));
    EXPECT_TRUE(range.high.AlmostEqual(Point3d::From(4, 5, 6)));
}

// Ported from: itwinjs-core core/geometry/src/test/polyface/Polyface.test.ts
//              TEST(IndexedPolyfaceTest, clone)
TEST(IndexedPolyfaceTest, clone)
{
    auto pf = IndexedPolyface::create();
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(1, 0, 0));
    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->TerminateFacet();

    auto clone = pf->clone();
    EXPECT_TRUE(clone.IsValid());
    EXPECT_TRUE(pf->IsAlmostEqual(*clone, 1.0e-10));
}

// Ported from: itwinjs-core core/geometry/src/test/polyface/Polyface.test.ts
//              TEST(IndexedPolyfaceTest, TwoSided)
TEST(IndexedPolyfaceTest, TwoSided)
{
    auto pf = IndexedPolyface::create(false, false, true);
    EXPECT_TRUE(pf->TwoSided());
    pf->SetTwoSided(false);
    EXPECT_FALSE(pf->TwoSided());
}

// Authored: no reference test exists in itwinjs-core/imodel-native for
// IndexedPolyface.IsAlmostEqual full-array coverage（PolyfaceData.ts:184-214
// 的 isAlmostEqual 是 PolyfaceData 级方法，GeometryQuery::IsAlmostEqual 在
// 两处参考均无对应物；TD-13 登记的是 DanQing 侧 GeometryQuery 接口的比较面
// 缺口）。断言面与参考 isAlmostEqual 对齐：normals/colors/params/twoSided/
// expectedClosure/edgeVisible 任一差异必须检出（当前实现只比较
// points/pointIndex → 全测不过 = RED）。
TEST(IndexedPolyfaceTest, IsAlmostEqualCoversAllDataChannels)
{
    auto makeTri = [] {
        auto pf = IndexedPolyface::create(/*needNormals=*/true, /*needColors=*/true,
                                          /*twoSided=*/false, /*needParams=*/true);
        pf->AddPoint(Point3d::From(0, 0, 0));
        pf->AddPoint(Point3d::From(1, 0, 0));
        pf->AddPoint(Point3d::From(0, 1, 0));
        pf->AddNormal(Vector3d::From(0, 0, 1));
        pf->AddColor(0xFF0000FF);
        pf->AddParam(Point2d::From(0.0, 0.0));
        pf->AddParam(Point2d::From(1.0, 0.0));
        pf->AddParam(Point2d::From(0.0, 1.0));
        int32_t const idx[3] = {1, 2, 3};
        for (int32_t i : idx) {
            pf->AddPointIndex(i);
            pf->AddNormalIndex(1);
            pf->AddColorIndex(1);
            pf->AddParamIndex(i);
        }
        pf->TerminateFacet();
        return pf;
    };

    auto const base = makeTri();
    EXPECT_TRUE(base->IsAlmostEqual(*base, 1.0e-10));
    auto const cloneRef = makeTri();
    EXPECT_TRUE(base->IsAlmostEqual(*cloneRef, 1.0e-10));

    // 各通道差异逐一致假（缺陷检出点——当前实现全部漏检）。
    {
        auto diff = makeTri();
        diff->Data().normals[0] = Vector3d::From(1, 0, 0);
        EXPECT_FALSE(base->IsAlmostEqual(*diff, 1.0e-10)) << "normal value change";
    }
    {
        auto diff = makeTri();
        diff->Data().normals.push_back(Vector3d::From(0, 1, 0));
        EXPECT_FALSE(base->IsAlmostEqual(*diff, 1.0e-10)) << "normal count change";
    }
    {
        auto diff = makeTri();
        diff->Data().normalIndex[0] = 2;  // 越界无所谓——数组相等性不解读
        EXPECT_FALSE(base->IsAlmostEqual(*diff, 1.0e-10)) << "normalIndex change";
    }
    {
        auto diff = makeTri();
        diff->Data().colors[0] = 0x00FF00FF;
        EXPECT_FALSE(base->IsAlmostEqual(*diff, 1.0e-10)) << "color value change";
    }
    {
        auto diff = makeTri();
        diff->Data().colorIndex[1] = 3;
        EXPECT_FALSE(base->IsAlmostEqual(*diff, 1.0e-10)) << "colorIndex change";
    }
    {
        auto diff = makeTri();
        diff->Data().params[0].x = 0.5;
        EXPECT_FALSE(base->IsAlmostEqual(*diff, 1.0e-10)) << "param value change";
    }
    {
        auto diff = makeTri();
        diff->Data().paramIndex[2] = 1;
        EXPECT_FALSE(base->IsAlmostEqual(*diff, 1.0e-10)) << "paramIndex change";
    }
    {
        auto diff = makeTri();
        diff->SetTwoSided(true);
        EXPECT_FALSE(base->IsAlmostEqual(*diff, 1.0e-10)) << "twoSided flip";
    }
    {
        auto diff = makeTri();
        diff->SetExpectedClosure(2);
        EXPECT_FALSE(base->IsAlmostEqual(*diff, 1.0e-10)) << "expectedClosure change";
    }
    {
        auto diff = makeTri();
        if (diff->Data().edgeVisible.empty())
            diff->Data().edgeVisible.assign(diff->Data().pointIndex.size(), true);
        diff->Data().edgeVisible[1] = false;
        EXPECT_FALSE(base->IsAlmostEqual(*diff, 1.0e-10)) << "edgeVisible change";
    }
}

// ---------------------------------------------------------------------------
// PolyfaceQuery tests
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core core/geometry/src/test/polyface/Polyface.test.ts
//              TEST(PolyfaceQueryTest, PointRange)
TEST(PolyfaceQueryTest, PointRange)
{
    auto pf = IndexedPolyface::create();
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(10, 20, 30));
    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->TerminateFacet();

    Range3d range = PolyfaceQuery::PointRange(*pf);
    EXPECT_TRUE(range.low.AlmostEqual(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(range.high.AlmostEqual(Point3d::From(10, 20, 30)));
}

// Ported from: itwinjs-core core/geometry/src/test/polyface/Polyface.test.ts
//              TEST(PolyfaceQueryTest, GetNumFacet)
TEST(PolyfaceQueryTest, GetNumFacet)
{
    auto pf = IndexedPolyface::create();
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(1, 0, 0));
    pf->AddPoint(Point3d::From(0, 1, 0));
    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->AddPointIndex(3);
    pf->TerminateFacet();

    EXPECT_EQ(PolyfaceQuery::GetNumFacet(*pf), 1u);
    EXPECT_EQ(PolyfaceQuery::GetNumVertex(*pf), 3u);
    EXPECT_TRUE(PolyfaceQuery::HasFacets(*pf));
}

// Ported from: itwinjs-core core/geometry/src/test/polyface/Polyface.test.ts
//              TEST(PolyfaceQueryTest, SumFacetAreas)
TEST(PolyfaceQueryTest, SumFacetAreas)
{
    // Right triangle with legs 3 and 4 → area = 6
    auto pf = IndexedPolyface::create();
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(3, 0, 0));
    pf->AddPoint(Point3d::From(0, 4, 0));
    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->AddPointIndex(3);
    pf->TerminateFacet();

    double area = PolyfaceQuery::SumFacetAreas(*pf);
    EXPECT_NEAR(area, 6.0, 1.0e-10);
}

// Ported from: itwinjs-core core/geometry/src/test/polyface/Polyface.test.ts
//              TEST(PolyfaceQueryTest, ComputeFacetUnitNormal)
TEST(PolyfaceQueryTest, ComputeFacetUnitNormal)
{
    auto pf = IndexedPolyface::create();
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(1, 0, 0));
    pf->AddPoint(Point3d::From(0, 1, 0));
    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->AddPointIndex(3);
    pf->TerminateFacet();

    Vector3d normal;
    EXPECT_TRUE(PolyfaceQuery::ComputeFacetUnitNormal(*pf, 0, normal));
    EXPECT_NEAR(normal.z, 1.0, 1.0e-10);
}

// Ported from: itwinjs-core core/geometry/src/test/polyface/Polyface.test.ts
//              TEST(PolyfaceQueryTest, IsClosedByEdgePairing)
TEST(PolyfaceQueryTest, IsClosedByEdgePairing)
{
    // Open triangle — not closed
    auto pf = IndexedPolyface::create();
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(1, 0, 0));
    pf->AddPoint(Point3d::From(0, 1, 0));
    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->AddPointIndex(3);
    pf->TerminateFacet();

    EXPECT_FALSE(PolyfaceQuery::IsClosedByEdgePairing(*pf));
}

// ---------------------------------------------------------------------------
// PR 2 coverage: Polyface abstract base + IndexedPolyface derivation + handler dispatch.
// Authored: Polyface.test.ts has no self-contained unit for category-derivation /
//           handleIndexedPolyface dispatch / areIndicesValid (covered here per PR 2 contract).
// ---------------------------------------------------------------------------

// Authored: IndexedPolyface derives Polyface; category + cross-type IsSameGeometryClass.
TEST(IndexedPolyfaceTest, DerivesPolyface)
{
    auto pf = IndexedPolyface::create();
    EXPECT_EQ(GeometryCategory::Polyface, pf->Category());
    auto pf2 = IndexedPolyface::create();
    EXPECT_TRUE(pf->IsSameGeometryClass(*pf2));
}

// Authored: DispatchToHandler reaches GeometryHandler::HandleIndexedPolyface (1:1 dispatchToGeometryHandler).
TEST(IndexedPolyfaceTest, DispatchReachesHandleIndexedPolyface)
{
    struct Recorder : GeometryHandler {
        int visits = 0;
        void HandleIndexedPolyface(IndexedPolyface&) override { ++visits; }
    } recorder;
    auto pf = IndexedPolyface::create();
    pf->DispatchToHandler(recorder);
    EXPECT_EQ(1, recorder.visits);
}

// Authored: Polyface::AreIndicesValid range check (1:1 Polyface.areIndicesValid).
// Indices must be in [0, dataLength); out-of-range or bad posA/posB → invalid.
TEST(PolyfaceStaticTest, AreIndicesValid)
{
    std::vector<int32_t> good{0, 1, 2};  // all < dataLength=3
    std::vector<int32_t> bad{0, 1, 5};   // 5 >= 3
    EXPECT_TRUE(Polyface::AreIndicesValid(good, 0, 3, 3));
    EXPECT_FALSE(Polyface::AreIndicesValid(bad, 0, 3, 3));
    EXPECT_FALSE(Polyface::AreIndicesValid(good, 0, 4, 3)); // posB(4) > size(3)
}

// Authored: IsEmpty true on fresh polyface, false after a facet.
TEST(IndexedPolyfaceTest, IsEmpty)
{
    auto pf = IndexedPolyface::create();
    EXPECT_TRUE(pf->IsEmpty());
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(1, 0, 0));
    pf->AddPoint(Point3d::From(0, 1, 0));
    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->AddPointIndex(3);
    pf->TerminateFacet();
    EXPECT_FALSE(pf->IsEmpty());
}

// Ported from: itwinjs-core core/geometry/src/test/polyface/PolyfaceQuery.test.ts
//              ("buildAverageNormals adds normals" + "does not crash on 0-area facet").
// 1:1 PolyfaceQuery.buildAverageNormals → BuildAverageNormalsContext.buildFastAverageNormals.
TEST(PolyfaceQueryTest, BuildAverageNormalsAddsNormals)
{
    auto pf = IndexedPolyface::create();
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(1, 0, 0));
    pf->AddPoint(Point3d::From(0, 1, 0));
    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->AddPointIndex(3);
    pf->TerminateFacet();
    EXPECT_EQ(pf->Data().NormalCount(), 0u);  // normal-less before

    PolyfaceQuery::BuildAverageNormals(*pf);

    // 1:1 reference assertion: "normals were added".
    EXPECT_GT(pf->Data().NormalCount(), 0u);
    // One normal index per sector (facet vertex) — 3 for a single triangle.
    EXPECT_EQ(pf->Data().normalIndex.size(), 3u);
    EXPECT_FALSE(pf->Data().normals.empty());
}

// Ported from: itwinjs-core PolyfaceQuery.test.ts — buildAverageNormals over a 2-facet planar quad
// clusters coplanar facet normals per vertex (4 vertices → 4 averaged normals, 6 sector indices).
TEST(PolyfaceQueryTest, BuildAverageNormalsPlanarQuad)
{
    auto pf = IndexedPolyface::create();
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(1, 0, 0));
    pf->AddPoint(Point3d::From(1, 1, 0));
    pf->AddPoint(Point3d::From(0, 1, 0));
    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->AddPointIndex(3);
    pf->TerminateFacet();
    pf->AddPointIndex(1);
    pf->AddPointIndex(3);
    pf->AddPointIndex(4);
    pf->TerminateFacet();

    PolyfaceQuery::BuildAverageNormals(*pf);

    // 4 distinct vertices → 4 per-vertex clusters; 6 sectors (2 facets × 3 verts).
    EXPECT_EQ(pf->Data().NormalCount(), 4u);
    EXPECT_EQ(pf->Data().normalIndex.size(), 6u);
}

// Ported from: itwinjs-core PolyfaceQuery.test.ts ("should not crash on the 0-area facet").
// A degenerate (collinear) facet yields a zero area normal → falls back to the default normal.
TEST(PolyfaceQueryTest, BuildAverageNormalsHandlesZeroAreaFacet)
{
    auto pf = IndexedPolyface::create();
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(1, 0, 0));
    pf->AddPoint(Point3d::From(2, 0, 0));  // collinear → 0-area facet
    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->AddPointIndex(3);
    pf->TerminateFacet();

    PolyfaceQuery::BuildAverageNormals(*pf);  // must not crash

    EXPECT_GT(pf->Data().NormalCount(), 0u);  // default normal still emitted
    EXPECT_EQ(pf->Data().normalIndex.size(), 3u);
}

// Authored: no reference test exists in itwinjs-core for polyface param/paramIndex
//              (UV texture coordinates, 1-based index arrays); 行为锚定实现
//              core/geometry/src/polyface/PolyfaceData.ts.
TEST(PolyfaceTest, CarriesTexCoordParams)
{
    auto pf = IndexedPolyface::create(false, false, false, /*needParams=*/true);
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(1, 0, 0));
    pf->AddPoint(Point3d::From(0, 1, 0));
    pf->AddParam(Point2d::From(0.0, 0.0));
    pf->AddParam(Point2d::From(1.0, 0.0));
    pf->AddParam(Point2d::From(0.0, 1.0));
    for (int32_t i = 1; i <= 3; ++i) {
        pf->AddPointIndex(i);
        pf->AddParamIndex(i);
    }
    pf->TerminateFacet();

    EXPECT_EQ(pf->Data().ParamCount(), 3u);
    EXPECT_EQ(pf->Data().paramIndex.size(), 3u);
    auto uv0 = pf->Data().GetParam(1);  // 1-based
    EXPECT_DOUBLE_EQ(uv0.x, 0.0);
    auto uv1 = pf->Data().GetParam(2);
    EXPECT_DOUBLE_EQ(uv1.x, 1.0);
}

// Authored: no reference test exists in itwinjs-core for IndexedPolyface data
//              arrays present only when requested; 行为锚定实现
//              IndexedPolyface.create(needParams)（Polyface.ts IndexedPolyface
//              constructor）.
TEST(PolyfaceTest, DefaultCreateHasNoParams)
{
    auto pf = IndexedPolyface::create();  // 0-arg existing call sites unchanged
    EXPECT_EQ(pf->Data().ParamCount(), 0u);
}

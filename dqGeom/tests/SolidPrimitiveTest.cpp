// SPDX-License-Identifier: Apache-2.0
// dqGeom SolidPrimitive / Box tests
// Ported from: itwinjs-core core/geometry/src/solid/SolidPrimitive.ts + Box.ts
//
// Authored for the ported surface（SolidPrimitive 抽象基 + Box 子类型 + PolyfaceBuilder.AddBox
// tessellation + GeometryHandler dispatch）。参考 solid/*.test.ts 多为 UV/查询集成场景；
// 本测试覆盖 PR 3 commit 1 移植的行为契约。
#include <gtest/gtest.h>

#include <dqGeom/AngleSweep.h>
#include <dqGeom/Box.h>
#include <dqGeom/Cone.h>
#include <dqGeom/GeometryHandler.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/LinearSweep.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/PolyfaceBuilder.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/RotationalSweep.h>
#include <dqGeom/RuledSweep.h>
#include <dqGeom/SolidPrimitive.h>
#include <dqGeom/Sphere.h>
#include <dqGeom/TorusPipe.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <vector>

using namespace dqGeom;

// Authored: Box::CreateRange builds an axis-aligned box; corners at the 8 range extremes.
TEST(BoxTest, CreateRangeCorners)
{
    auto box = Box::CreateRange(Range3d::create({Point3d::From(0, 0, 0), Point3d::From(1, 1, 1)}), /*capped=*/true);
    ASSERT_TRUE(box);
    EXPECT_EQ(SolidPrimitiveType::Box, box->GetSolidPrimitiveType());
    EXPECT_EQ(GeometryCategory::Solid, box->Category());
    EXPECT_TRUE(box->IsClosedVolume()); // capped

    auto corners = box->GetCorners();
    ASSERT_EQ(8u, corners.size());
    // base (z=0): (0,0,0),(1,0,0),(0,1,0),(1,1,0); top (z=1): same +z.
    EXPECT_TRUE(corners[0].AlmostEqual(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(corners[3].AlmostEqual(Point3d::From(1, 1, 0)));
    EXPECT_TRUE(corners[7].AlmostEqual(Point3d::From(1, 1, 1)));
}

// Authored: clone produces an independent deep copy; IsAlmostEqual holds (1:1 Box.clone/isAlmostEqual).
TEST(BoxTest, CloneAndAlmostEqual)
{
    auto box = Box::CreateRange(Range3d::create({Point3d::From(0, 0, 0), Point3d::From(2, 3, 4)}), true);
    auto cloned = box->clone().StaticCast<Box>();
    ASSERT_TRUE(cloned);
    EXPECT_TRUE(box->IsAlmostEqual(*cloned, 1.0e-9));
    // Mutate original's capped state; clone independent.
    box->SetCapped(false);
    EXPECT_FALSE(box->IsAlmostEqual(*cloned, 1.0e-9));
}

// Authored: DispatchToHandler reaches HandleBox (1:1 Box.dispatchToGeometryHandler).
TEST(BoxTest, DispatchReachesHandleBox)
{
    struct Recorder : GeometryHandler {
        int boxVisits = 0;
        int solidVisits = 0;
        void HandleBox(Box&) override { ++boxVisits; }
        void HandleSolidPrimitive(SolidPrimitive&) override { ++solidVisits; }
    } recorder;
    auto box = Box::CreateRange(Range3d::create({Point3d::From(0, 0, 0), Point3d::From(1, 1, 1)}), true);
    box->DispatchToHandler(recorder);
    EXPECT_EQ(1, recorder.boxVisits);
}

// Authored: PolyfaceBuilder.AddBox tessellates a box into facets (1:1 PolyfaceBuilder.addBox).
// Capped box has more facets than uncapped (sides + 2 caps vs sides only).
TEST(BoxTest, PolyfaceBuilderAddBox)
{
    auto capped = Box::CreateRange(Range3d::create({Point3d::From(0, 0, 0), Point3d::From(1, 1, 1)}), true);
    auto uncapped = Box::CreateRange(Range3d::create({Point3d::From(0, 0, 0), Point3d::From(1, 1, 1)}), false);

    auto builderC = PolyfaceBuilder::create(StrokeOptions{});
    builderC->AddBox(*capped);
    auto pfC = builderC->ClaimPolyface(false);

    auto builderU = PolyfaceBuilder::create(StrokeOptions{});
    builderU->AddBox(*uncapped);
    auto pfU = builderU->ClaimPolyface(false);

    ASSERT_TRUE(pfC);
    ASSERT_TRUE(pfU);
    EXPECT_GE(pfC->FacetCount(), 6u); // 4 sides + 2 caps (>=6 whether triangulated or not)
    EXPECT_GE(pfU->FacetCount(), 4u); // 4 sides only
    EXPECT_GT(pfC->FacetCount(), pfU->FacetCount()); // caps add facets
}

// Authored: Cone construction + capped/radii + dispatch (1:1 Cone.ts).
TEST(ConeTest, ConstructAndDispatch)
{
    auto cone = Cone::CreateBaseAndTarget(Point3d::From(0, 0, 0), Point3d::From(0, 0, 2),
                                          Vector3d::UnitX(), Vector3d::UnitY(),
                                          /*radiusA=*/1.0, /*radiusB=*/0.5, /*capped=*/true);
    ASSERT_TRUE(cone);
    EXPECT_EQ(SolidPrimitiveType::Cone, cone->GetSolidPrimitiveType());
    EXPECT_TRUE(cone->IsClosedVolume()); // capped && rA && rB
    EXPECT_NEAR(1.0, cone->GetRadiusA(), 1.0e-12);
    EXPECT_NEAR(0.5, cone->GetRadiusB(), 1.0e-12);
    EXPECT_NEAR(1.0, cone->GetMaxRadius(), 1.0e-12);
    EXPECT_TRUE(cone->GetCenterA().AlmostEqual(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(cone->GetCenterB().AlmostEqual(Point3d::From(0, 0, 2)));

    struct Recorder : GeometryHandler {
        int visits = 0;
        void HandleCone(Cone&) override { ++visits; }
    } rec;
    cone->DispatchToHandler(rec);
    EXPECT_EQ(1, rec.visits);
}

// Authored: PolyfaceBuilder.AddCone emits lateral quads + cap fans (1:1 PolyfaceBuilder.addCone).
TEST(ConeTest, PolyfaceBuilderAddCone)
{
    auto cone = Cone::CreateBaseAndTarget(Point3d::From(0, 0, 0), Point3d::From(0, 0, 1),
                                          Vector3d::UnitX(), Vector3d::UnitY(), 1.0, 1.0, true);
    auto builder = PolyfaceBuilder::create(StrokeOptions{});
    builder->AddCone(*cone, /*strokeCount=*/8);
    auto pf = builder->ClaimPolyface(false);
    ASSERT_TRUE(pf);
    // 8 lateral quads + 2 cap fans (≥8 facets; capped cone has more than uncapped).
    EXPECT_GE(pf->FacetCount(), 8u);
    auto builderU = PolyfaceBuilder::create(StrokeOptions{});
    auto uncapped = Cone::CreateBaseAndTarget(Point3d::From(0, 0, 0), Point3d::From(0, 0, 1),
                                              Vector3d::UnitX(), Vector3d::UnitY(), 1.0, 1.0, false);
    builderU->AddCone(*uncapped, 8);
    auto pfU = builderU->ClaimPolyface(false);
    EXPECT_GT(pf->FacetCount(), pfU->FacetCount()); // caps add facets
}

// Authored: Sphere construction + IsClosedVolume + dispatch (1:1 Sphere.ts).
TEST(SphereTest, ConstructAndDispatch)
{
    auto sphere = Sphere::CreateCenterRadius(Point3d::From(1, 2, 3), 2.0, Sphere::FullLatitudeSweep(), true);
    ASSERT_TRUE(sphere);
    EXPECT_EQ(SolidPrimitiveType::Sphere, sphere->GetSolidPrimitiveType());
    EXPECT_TRUE(sphere->IsClosedVolume()); // capped
    EXPECT_NEAR(2.0, sphere->MaxAxisRadius(), 1.0e-12);
    EXPECT_TRUE(sphere->CloneCenter().AlmostEqual(Point3d::From(1, 2, 3)));

    struct Recorder : GeometryHandler {
        int visits = 0;
        void HandleSphere(Sphere&) override { ++visits; }
    } rec;
    sphere->DispatchToHandler(rec);
    EXPECT_EQ(1, rec.visits);
}

// Authored: PolyfaceBuilder.AddSphere emits a lat/long quad grid (1:1 PolyfaceBuilder.addSphere).
TEST(SphereTest, PolyfaceBuilderAddSphere)
{
    auto sphere = Sphere::CreateCenterRadius(Point3d::FromZero(), 1.0, Sphere::FullLatitudeSweep(), false);
    auto builder = PolyfaceBuilder::create(StrokeOptions{});
    builder->AddSphere(*sphere, /*strokeCount=*/8);
    auto pf = builder->ClaimPolyface(false);
    ASSERT_TRUE(pf);
    // nTheta=8, nPhi=4 → 32 quad cells → ≥32 facets when triangulated.
    EXPECT_GE(pf->FacetCount(), 32u);
}

// Authored: TorusPipe construction + IsClosedVolume + dispatch (1:1 TorusPipe.ts).
TEST(TorusPipeTest, ConstructAndDispatch)
{
    auto tp = TorusPipe::CreateInFrame(Transform::CreateIdentity(),
                                       /*majorRadius=*/2.0, /*minorRadius=*/0.5,
                                       Angle::FullCircle(), /*capped=*/false);
    ASSERT_TRUE(tp);
    EXPECT_EQ(SolidPrimitiveType::TorusPipe, tp->GetSolidPrimitiveType());
    EXPECT_TRUE(tp->IsClosedVolume());     // full sweep → closed surface
    EXPECT_NEAR(1.0, tp->GetThetaFraction(), 1.0e-12);
    EXPECT_NEAR(2.0, tp->GetRadiusA(), 1.0e-12);

    struct Recorder : GeometryHandler {
        int visits = 0;
        void HandleTorusPipe(TorusPipe&) override { ++visits; }
    } rec;
    tp->DispatchToHandler(rec);
    EXPECT_EQ(1, rec.visits);
}

// Authored: PolyfaceBuilder.AddTorusPipe emits a torus grid (1:1 PolyfaceBuilder.addTorusPipe).
TEST(TorusPipeTest, PolyfaceBuilderAddTorusPipe)
{
    auto tp = TorusPipe::CreateInFrame(Transform::CreateIdentity(), 2.0, 0.5, Angle::FullCircle(), false);
    auto builder = PolyfaceBuilder::create(StrokeOptions{});
    builder->AddTorusPipe(*tp, /*numPhi=*/8, /*numTheta=*/16);
    auto pf = builder->ClaimPolyface(false);
    ASSERT_TRUE(pf);
    // 8 × 16 = 128 quad cells → ≥128 facets when triangulated.
    EXPECT_GE(pf->FacetCount(), 128u);
}

// Authored: sweep type classes exist + dispatch (1:1 LinearSweep/RotationalSweep/RuledSweep.ts).
// Sweep tessellation is phased (needs SweepContour); these cover construction/type/closed-volume/dispatch.
TEST(SweepTest, ConstructAndDispatch)
{
    auto loop = Loop::CreatePolygon({Point3d::From(0, 0, 0), Point3d::From(1, 0, 0), Point3d::From(1, 1, 0)});

    auto lin = LinearSweep::Create(loop, Vector3d::From(0, 0, 1), /*capped=*/true);
    ASSERT_TRUE(lin);
    EXPECT_EQ(SolidPrimitiveType::LinearSweep, lin->GetSolidPrimitiveType());
    EXPECT_TRUE(lin->IsClosedVolume()); // capped && Loop (region)

    auto rot = RotationalSweep::Create(loop, Point3d::From(0, 0, 0), Vector3d::UnitZ(),
                                      Angle::FullCircle(), /*capped=*/false);
    ASSERT_TRUE(rot);
    EXPECT_EQ(SolidPrimitiveType::RotationalSweep, rot->GetSolidPrimitiveType());
    EXPECT_TRUE(rot->IsClosedVolume()); // full-circle sweep

    auto ruled = RuledSweep::Create({loop, loop}, /*capped=*/true);
    ASSERT_TRUE(ruled);
    EXPECT_EQ(SolidPrimitiveType::RuledSweep, ruled->GetSolidPrimitiveType());
    EXPECT_TRUE(ruled->IsClosedVolume()); // 2 contours && capped

    struct Recorder : GeometryHandler {
        int lin = 0, rot = 0, ruled = 0;
        void HandleLinearSweep(LinearSweep&) override { ++lin; }
        void HandleRotationalSweep(RotationalSweep&) override { ++rot; }
        void HandleRuledSweep(RuledSweep&) override { ++ruled; }
    } rec;
    lin->DispatchToHandler(rec);
    rot->DispatchToHandler(rec);
    ruled->DispatchToHandler(rec);
    EXPECT_EQ(1, rec.lin);
    EXPECT_EQ(1, rec.rot);
    EXPECT_EQ(1, rec.ruled);
}

// Ported from: PolyfaceBuilder.ts addLinearSweep (1363). Stroke cross-section → translate → side
// quads → caps. Capped (region Loop) has more facets than uncapped; range extends along the sweep.
TEST(SweepTest, LinearSweepTessellates)
{
    auto loop = Loop::CreatePolygon({Point3d::From(0, 0, 0), Point3d::From(1, 0, 0), Point3d::From(1, 1, 0)});
    auto capped = LinearSweep::Create(loop, Vector3d::From(0, 0, 2), /*capped=*/true);
    auto uncapped = LinearSweep::Create(loop, Vector3d::From(0, 0, 2), /*capped=*/false);
    ASSERT_TRUE(capped);
    ASSERT_TRUE(uncapped);

    auto build = [](SolidPrimitive const& s) {
        auto b = PolyfaceBuilder::create(StrokeOptions{});
        b->AddGeometryQuery(s);
        return b->ClaimPolyface(false);
    };
    auto pfC = build(*capped);
    auto pfU = build(*uncapped);
    ASSERT_TRUE(pfC);
    ASSERT_TRUE(pfU);

    EXPECT_GT(pfC->FacetCount(), 0u);   // sides emitted
    EXPECT_GT(pfU->FacetCount(), 0u);
    EXPECT_GT(pfC->FacetCount(), pfU->FacetCount());  // caps add facets
    EXPECT_GT(pfC->Data().PointCount(), 0u);

    // Range extends along the sweep vector (z: 0 → 2).
    const Range3d r = pfC->Range();
    EXPECT_LE(r.low.z, 1.0e-9);
    EXPECT_GE(r.high.z, 2.0 - 1.0e-9);
}

// Ported from: PolyfaceBuilder.ts addRotationalSweep (1211). Stroke cross-section → rotate through
// sweepAngle in N steps → side quads. Capped (region Loop) has more facets than uncapped.
TEST(SweepTest, RotationalSweepTessellates)
{
    auto loop = Loop::CreatePolygon({Point3d::From(0, 0, 0), Point3d::From(1, 0, 0), Point3d::From(1, 1, 0)});
    auto capped = RotationalSweep::Create(loop, Point3d::From(0, 0, 0), Vector3d::UnitZ(),
                                          Angle::FromDegrees(90.0), /*capped=*/true);
    auto uncapped = RotationalSweep::Create(loop, Point3d::From(0, 0, 0), Vector3d::UnitZ(),
                                            Angle::FromDegrees(90.0), /*capped=*/false);
    ASSERT_TRUE(capped);
    ASSERT_TRUE(uncapped);

    auto build = [](SolidPrimitive const& s) {
        auto b = PolyfaceBuilder::create(StrokeOptions{});
        b->AddGeometryQuery(s);
        return b->ClaimPolyface(false);
    };
    auto pfC = build(*capped);
    auto pfU = build(*uncapped);
    ASSERT_TRUE(pfC);
    ASSERT_TRUE(pfU);

    EXPECT_GT(pfC->FacetCount(), 0u);   // rotated side quads emitted
    EXPECT_GT(pfU->FacetCount(), 0u);
    EXPECT_GT(pfC->FacetCount(), pfU->FacetCount());  // caps add facets
    EXPECT_GT(pfC->Data().PointCount(), 0u);
}

// Ported from: PolyfaceBuilder.ts addRuledSweep (1381). Loft between matching profiles → side quads.
// Two identical Loops (same point count) → sides emitted; capped adds caps.
TEST(SweepTest, RuledSweepTessellates)
{
    auto loopA = Loop::CreatePolygon({Point3d::From(0, 0, 0), Point3d::From(1, 0, 0), Point3d::From(1, 1, 0)});
    auto loopB = Loop::CreatePolygon({Point3d::From(0, 0, 2), Point3d::From(1, 0, 2), Point3d::From(1, 1, 2)});
    auto capped = RuledSweep::Create({loopA, loopB}, /*capped=*/true);
    auto uncapped = RuledSweep::Create({loopA, loopB}, /*capped=*/false);
    ASSERT_TRUE(capped);
    ASSERT_TRUE(uncapped);

    auto build = [](SolidPrimitive const& s) {
        auto b = PolyfaceBuilder::create(StrokeOptions{});
        b->AddGeometryQuery(s);
        return b->ClaimPolyface(false);
    };
    auto pfC = build(*capped);
    auto pfU = build(*uncapped);
    ASSERT_TRUE(pfC);
    ASSERT_TRUE(pfU);

    EXPECT_GT(pfC->FacetCount(), 0u);   // ruled side quads between the two profiles
    EXPECT_GT(pfU->FacetCount(), 0u);
    EXPECT_GT(pfC->FacetCount(), pfU->FacetCount());  // caps add facets
    EXPECT_GT(pfC->Data().PointCount(), 0u);
}

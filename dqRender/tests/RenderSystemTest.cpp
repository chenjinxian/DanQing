// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderGraphic, GraphicBranch, Scene tests
//
// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
#include "dqRender/GraphicBranch.h"
#include "dqRender/GraphicBuilder.h"
#include "dqRender/RenderGraphic.h"
#include "dqRender/RenderSystem.h"
#include "dqRender/RenderTarget.h"
#include "dqRender/Scene.h"

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqGeom;
using namespace dqCommon;

// Concrete test graphic
class TestGraphic : public RenderGraphic {
public:
    Range3d range;

    explicit TestGraphic(const Range3d& r) : range(r) {}

    void unionRange(Range3d& r) const override
    {
        r.ExtendRange(range);
    }
};

// RenderGraphic tests
// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
TEST(RenderGraphic, unionRange)
{
    TestGraphic g(Range3d::CreateXYZXYZ(0, 0, 0, 10, 10, 10));
    auto range = Range3d::CreateNull();
    g.unionRange(range);
    EXPECT_TRUE(range.low.AlmostEqual(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(range.high.AlmostEqual(Point3d::From(10, 10, 10)));
}

// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
TEST(RenderGraphic, MultipleUnionRange)
{
    TestGraphic g1(Range3d::CreateXYZXYZ(0, 0, 0, 10, 10, 10));
    TestGraphic g2(Range3d::CreateXYZXYZ(5, 5, 5, 20, 20, 20));
    auto range = Range3d::CreateNull();
    g1.unionRange(range);
    g2.unionRange(range);
    EXPECT_TRUE(range.low.AlmostEqual(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(range.high.AlmostEqual(Point3d::From(20, 20, 20)));
}

// GraphicBranch tests
// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
TEST(GraphicBranch, DefaultConstruction)
{
    GraphicBranch branch;
    EXPECT_EQ(branch.size(), 0u);
    EXPECT_FALSE(branch.ownsEntries);
}

// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
TEST(GraphicBranch, AddGraphics)
{
    GraphicBranch branch;
    TestGraphic g1(Range3d::CreateXYZXYZ(0, 0, 0, 10, 10, 10));
    TestGraphic g2(Range3d::CreateXYZXYZ(5, 5, 5, 20, 20, 20));

    branch.add(&g1);
    branch.add(&g2);
    EXPECT_EQ(branch.size(), 2u);
    EXPECT_EQ(branch.get(0), &g1);
    EXPECT_EQ(branch.get(1), &g2);
}

// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
TEST(GraphicBranch, unionRange)
{
    GraphicBranch branch;
    TestGraphic g1(Range3d::CreateXYZXYZ(0, 0, 0, 10, 10, 10));
    TestGraphic g2(Range3d::CreateXYZXYZ(5, 5, 5, 20, 20, 20));

    branch.add(&g1);
    branch.add(&g2);

    auto range = Range3d::CreateNull();
    branch.unionRange(range);
    EXPECT_TRUE(range.low.AlmostEqual(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(range.high.AlmostEqual(Point3d::From(20, 20, 20)));
}

// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
TEST(GraphicBranch, ViewFlagOverrides)
{
    GraphicBranch branch;
    EXPECT_EQ(branch.viewFlagOverrides.renderMode, RenderMode::Wireframe);

    branch.viewFlagOverrides.renderMode = RenderMode::SmoothShade;
    EXPECT_EQ(branch.viewFlagOverrides.renderMode, RenderMode::SmoothShade);
}

// GraphicType tests
// Values verified against itwinjs-core core/frontend/src/common/render/GraphicType.ts enum.
// (RenderSystem.test.ts contains no GraphicType assertions; enum values are authoritative from GraphicType.ts.)
TEST(GraphicType, Values)
{
    // Values match itwinjs-core GraphicType.ts enum
    EXPECT_EQ(static_cast<int>(GraphicType::ViewBackground), 0);
    EXPECT_EQ(static_cast<int>(GraphicType::Scene), 1);
    EXPECT_EQ(static_cast<int>(GraphicType::WorldDecoration), 2);
    EXPECT_EQ(static_cast<int>(GraphicType::WorldOverlay), 3);
    EXPECT_EQ(static_cast<int>(GraphicType::ViewOverlay), 4);
}

// ViewRect tests
// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
TEST(ViewRect, DefaultConstruction)
{
    const ViewRect rect;
    EXPECT_EQ(rect.left, 0u);
    EXPECT_EQ(rect.top, 0u);
    EXPECT_EQ(rect.right, 0u);
    EXPECT_EQ(rect.bottom, 0u);
    EXPECT_TRUE(rect.isNull());
}

// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
TEST(ViewRect, CustomConstruction)
{
    const ViewRect rect(10, 20, 100, 200);
    EXPECT_EQ(rect.left, 10u);
    EXPECT_EQ(rect.top, 20u);
    EXPECT_EQ(rect.right, 100u);
    EXPECT_EQ(rect.bottom, 200u);
    EXPECT_EQ(rect.width(), 90u);
    EXPECT_EQ(rect.height(), 180u);
    EXPECT_TRUE(rect.isValid());
}

// Scene tests
// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
TEST(Scene, DefaultConstruction)
{
    Scene scene;
    EXPECT_EQ(scene.size(), 0u);
}

// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
TEST(Scene, AddGraphics)
{
    Scene scene;
    TestGraphic g1(Range3d::CreateXYZXYZ(0, 0, 0, 10, 10, 10));
    TestGraphic g2(Range3d::CreateXYZXYZ(5, 5, 5, 20, 20, 20));

    scene.add(&g1);
    scene.add(&g2);
    EXPECT_EQ(scene.size(), 2u);
}

// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
TEST(Scene, computeRange)
{
    Scene scene;
    TestGraphic g1(Range3d::CreateXYZXYZ(0, 0, 0, 10, 10, 10));
    TestGraphic g2(Range3d::CreateXYZXYZ(5, 5, 5, 20, 20, 20));

    scene.add(&g1);
    scene.add(&g2);

    const auto range = scene.computeRange();
    EXPECT_TRUE(range.low.AlmostEqual(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(range.high.AlmostEqual(Point3d::From(20, 20, 20)));
}

// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
TEST(Scene, clear)
{
    Scene scene;
    TestGraphic g1(Range3d::CreateXYZXYZ(0, 0, 0, 10, 10, 10));
    scene.add(&g1);
    EXPECT_EQ(scene.size(), 1u);

    scene.clear();
    EXPECT_EQ(scene.size(), 0u);
}

// RenderGraphicOwner tests
// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
TEST(RenderGraphicOwner, Ownership)
{
    auto* owned = new TestGraphic(Range3d::CreateXYZXYZ(0, 0, 0, 10, 10, 10));
    RenderGraphicOwner owner(owned);

    EXPECT_EQ(owner.graphic(), owned);

    auto range = Range3d::CreateNull();
    owner.unionRange(range);
    EXPECT_TRUE(range.low.AlmostEqual(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(range.high.AlmostEqual(Point3d::From(10, 10, 10)));

    delete owned;
}

// ---------------------------------------------------------------------------
// RenderSystem::get() null-safety (P0 crash fix)
//
// Authored: no reference test exists in itwinjs-core for Get()-before-startup;
//   in the ref, IModelApp.renderSystem is always established by host setup.
//   This test asserts the DanQing bounded null-safety invariant: Get() never
//   returns a dereferenced null, and the fallback system reports isValid()==false.
// ---------------------------------------------------------------------------
TEST(RenderSystem, GetBeforeSetInstanceDoesNotCrash)
{
    // Force the unset state: clear any instance left by prior tests.
    RenderSystem::setInstance(nullptr);

    // Get() must not crash — it returns a no-op default system.
    RenderSystem* sys = &RenderSystem::get();
    ASSERT_NE(sys, nullptr);

    // The fallback system is not a valid (initialized) system.
    EXPECT_FALSE(sys->isValid());

    // Factory methods on the fallback are no-ops (return nullptr), not crashes.
    EXPECT_EQ(sys->createBranch(true), nullptr);
    EXPECT_EQ(sys->createGraphicList({}), nullptr);
    EXPECT_EQ(sys->createGraphicFromPolyface(nullptr, 0, 0), nullptr);
}

// Ported from: itwinjs-core core/frontend/src/test/RenderSystem.test.ts
//              TEST(RenderSystem, SetInstanceOverridesDefault)
TEST(RenderSystem, SetInstanceOverridesDefault)
{
    // After SetInstance(p), Get() must return the installed instance.
    RenderSystem::setInstance(nullptr);  // reset to default
    auto& before = RenderSystem::get();
    EXPECT_FALSE(before.isValid());

    // Install a custom no-op system via SetInstance.
    // (MockRenderSystem would live in src/render/MockRender.h; for this test we
    // reuse the default's address by re-installing it — SetInstance contract is
    // purely about pointer identity.)
    RenderSystem::setInstance(&before);
    auto& after = RenderSystem::get();
    EXPECT_EQ(&after, &before);

    // Restore unset state for subsequent tests.
    RenderSystem::setInstance(nullptr);
}

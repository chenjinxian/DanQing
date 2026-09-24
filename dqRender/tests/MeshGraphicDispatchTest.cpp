// SPDX-License-Identifier: Apache-2.0
// Authored: PR F regression guard for the non-renderable-bug fix (no reference test — this asserts the
// DanQing type-hierarchy invariant that makes RenderGraphicAdapter dispatch defined).
//
// The bug: PrimitiveBuilder.finish() produced a `MeshGraphicRender` that inherited the PUBLIC
// RenderGraphic but was NOT an internal Graphic. RenderGraphicAdapter::addCommands does
// `if (isInternalGraphic(g)) static_cast<Graphic*>(g)->addCommands(...)` where isInternalGraphic is
// `!g->isBranch()`. MeshGraphicRender had isBranch()==false → the adapter static_cast a non-Graphic → UB
// (nothing drew). The fix: finish() now produces a MeshGraphic, which IS-A Graphic — so the same
// static_cast is defined behavior.
//
// This guard pins that invariant: MeshGraphic is-a Graphic (compile-time) + isBranch()==false (the
// adapter's discriminator). A full finish()→addCommands execution needs a GL-context RenderCommands
// harness (not present in dqRenderTest); the type identity here is the bug-fix proof.
#include <gtest/gtest.h>

#include "render/Batch.h"
#include "render/BranchStack.h"
#include "render/Graphic.h"
#include "render/MeshGraphic.h"
#include "render/RenderCommands.h"
#include "render/RenderGraphicAdapter.h"
#include "render/SurfaceGeometry.h"
#include "render/TargetImpl.h"

#include "NullDriver.h"

#include "dqRender/RenderGraphic.h"

#include <memory>
#include <type_traits>

using namespace dqRender;

// PR F regression guard: MeshGraphic is an internal Graphic, so RenderGraphicAdapter's
// static_cast<Graphic*> on it is defined behavior (the non-renderable-bug fix).
TEST(MeshGraphicDispatchTest, IsGraphicSoAdapterDispatchIsDefined)
{
    rhi::NullDriver driver;
    auto mg = std::make_unique<MeshGraphic>(driver);
    RenderGraphic* rg = mg.get();

    // Compile-time: MeshGraphic IS-A Graphic (inheriting RenderGraphic via Graphic). A regression that
    // made finish()'s output a non-Graphic RenderGraphic would fail this static_assert.
    static_assert(std::is_base_of_v<Graphic, MeshGraphic>);
    static_assert(std::is_base_of_v<RenderGraphic, MeshGraphic>);

    // The adapter's discriminator (RenderGraphicAdapter::isInternalGraphic): !isBranch() → "internal
    // Graphic" → takes the static_cast<Graphic*> branch. MeshGraphic must report isBranch()==false.
    EXPECT_FALSE(rg->isBranch());

    // Defined dispatch: the same object is usable as both Graphic& and RenderGraphic& (the upcast the
    // adapter's static_cast performs at runtime).
    Graphic& asGraphic = *mg;
    RenderGraphic& asRenderGraphic = *mg;
    (void)asGraphic;
    (void)asRenderGraphic;

    // RenderGraphicAdapter constructs on a MeshGraphic; with isBranch()==false it will dispatch to
    // Graphic::addCommands (MeshGraphic::addCommands) — defined, not UB.
    RenderGraphicAdapter adapter(rg);
    (void)adapter;
}

// Authored: a bare GraphicBranch (public API) IS a branch — the adapter iterates its entries instead of
// static_cast-ing. This is the other dispatch branch; documents the discriminator contract.
TEST(MeshGraphicDispatchTest, GraphicBranchIsBranchSoAdapterIteratesEntries)
{
    auto branch = std::make_unique<GraphicBranch>(true);
    RenderGraphic* rg = branch.get();
    EXPECT_TRUE(rg->isBranch());  // → isInternalGraphic==false → adapter iterates entries (no static_cast)
}

// Authored: PR F follow-up — finish() now returns a Branch(GraphicsArray(MeshGraphic…)) carrying the
// recentering translation. Pin that every node in that tree IS-A Graphic, so RenderGraphicAdapter
// dispatches through defined static_cast<Graphic*> at every level (no UB). (Executing addCommands needs
// a GL-context RenderCommands harness — absent in dqRenderTest; the type identity is the proof.)
TEST(MeshGraphicDispatchTest, FinishTreeIsAllGraphicSoDispatchIsDefined)
{
    static_assert(std::is_base_of_v<Graphic, Branch>);
    static_assert(std::is_base_of_v<Graphic, GraphicsArray>);
    static_assert(std::is_base_of_v<Graphic, MeshGraphic>);

    // Build the tree finish() produces: Branch → GraphicsArray → MeshGraphic leaves.
    rhi::NullDriver driver;
    auto leaf = std::make_unique<MeshGraphic>(driver);
    MeshGraphic* leafPtr = leaf.get();
    auto array = std::make_unique<GraphicsArray>();
    array->add(std::move(leaf));
    auto branch = std::make_unique<Branch>();
    branch->setChild(std::move(array));

    RenderGraphic* rg = branch.get();
    EXPECT_FALSE(rg->isBranch());              // Branch (internal) → adapter treats as internal Graphic
    EXPECT_FALSE(static_cast<RenderGraphic*>(leafPtr)->isBranch());  // leaf likewise

    // Upcasts the adapter performs at runtime — defined because each node IS a Graphic.
    Graphic& branchAsGraphic = *branch;
    Graphic& leafAsGraphic = *leafPtr;
    (void)branchAsGraphic;
    (void)leafAsGraphic;
}

// Authored: weak runtime test of the addCommands dispatch — the finish() tree (Branch → GraphicsArray →
// MeshGraphic) actually produces RenderCommands entries when traversed (not just compile-time type
// checks). Uses the fake-TargetImpl trick from RenderingPipelineTest.cpp:329-334 — RenderCommands::
// addPrimitive(CachedGeometry*) never dereferences the target. The MeshGraphic's SurfaceGeometry has a
// null primitive handle (the weak path never calls draw(), so no GL is touched).
TEST(MeshGraphicDispatchTest, AddCommandsProducesRenderCommands)
{
    BranchStack stack;
    BatchState batchState;
    // Fake, never-dereferenced TargetImpl storage (see RenderingPipelineTest.cpp:331).
    static char buf[sizeof(TargetImpl)];  // NOLINT — uninitialized is intentional
    auto* fakeTarget = reinterpret_cast<TargetImpl*>(buf);
    RenderCommands cmds(*fakeTarget, stack, batchState);

    // Build the exact tree finish() produces, with one surface leaf.
    rhi::NullDriver driver;
    auto leaf = std::make_unique<MeshGraphic>(driver);
    auto surface = std::make_unique<SurfaceGeometry>(driver, rhi::IndexBufferHandle{}, 6u, SurfaceType::Opaque, false, false);
    surface->setPrimitive({});  // null handle — addCommands only routes, never draws
    leaf->addSurface(std::move(surface));
    auto array = std::make_unique<GraphicsArray>();
    array->add(std::move(leaf));
    auto branch = std::make_unique<Branch>();
    branch->setChild(std::move(array));

    // Drive the dispatch through the RenderGraphicAdapter — the exact path that was UB before PR F
    // (adapter does isInternalGraphic → static_cast<Graphic*> → Graphic::addCommands).
    RenderGraphicAdapter adapter(branch.get());
    adapter.addCommands(cmds);

    EXPECT_FALSE(cmds.isEmpty());
    EXPECT_GE(cmds.getCommandCount(), 1u);
}

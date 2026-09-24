// SPDX-License-Identifier: Apache-2.0
// Authored: regression guard for the PrimitiveBuilder.finish() recentering-recovery
// wiring. No reference test exists in itwinjs-core for this DanQing plumbing — the
// itwinjs equivalent (BranchUniforms.bindForGeometry folding mv = view ·
// currentTransform, where currentTransform is the composed branch localToWorld) is
// exercised only end-to-end in the reference test-app. This pins the contract at the
// draw-command layer so the ACS Z-axis PointString tip renders at the disc center
// instead of ~rangeCenter off it.
//
// Background (the bug this catches): PrimitiveBuilder.finish() wraps the recentered
// mesh array in a Branch carrying localToWorld = translate(rangeCenter) to recover
// world placement (1:1 with itwinjs PrimitiveBuilder.ts:122-123). Non-quantized Mesh
// vertices are pre-transformed relative to the shared range center (PrimitiveBuilder
// .ts:81-82; Mesh ctor sets Point3dList.center = range.Center()). itwinjs folds that
// model transform into the draw mv via BranchUniforms. DanQing's RenderCommands
// recording sites pushed getMv()/getMvp() — identity for setLocalToWorld branches —
// so the translate never reached the draw MVP and recentered vertices projected
// offset by the range center.
#include <gtest/gtest.h>

#include "render/BranchStack.h"
#include "render/Batch.h"
#include "render/DrawCommand.h"
#include "render/Graphic.h"
#include "render/MeshGraphic.h"
#include "render/MeshPrimitive.h"
#include "render/MeshPrimitives.h"
#include "render/RenderCommands.h"
#include "render/RenderGraphicAdapter.h"
#include "render/SurfaceGeometry.h"
#include "render/TargetImpl.h"
#include "render/VertexKey.h"

#include "NullDriver.h"

#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>

#include <memory>

using namespace dqRender;

namespace {
// Find the first PushBranchCommand recorded across all render passes.
PushBranchCommand const* findRecordedPushBranch(RenderCommands const& cmds)
{
    for (int p = 0; p < static_cast<int>(RenderPass::COUNT); ++p) {
        for (auto const& c : cmds.getCommands(static_cast<RenderPass>(p))) {
            if (c && c->getType() == DrawCommandType::PushBranch)
                return static_cast<PushBranchCommand const*>(c.get());
        }
    }
    return nullptr;
}
}  // namespace

// A Branch carrying a localToWorld translation (the recovery transform
// PrimitiveBuilder.finish attaches) MUST fold that translation into the draw
// transform it records (PushBranchCommand carries mv/mvp). The compositor uploads
// u_mvp/u_mv from that recorded transform; without the fold, recentered vertices
// (see MeshVerticesRecenteredAboutRangeCenter) project offset by the range center.
// Column-major (GL): translation lives at data[12],[13],[14].
TEST(BranchLocalToWorldTest, LocalToWorldTranslationFoldsIntoRecordedDrawTransform)
{
    BranchStack stack;
    BatchState batchState;
    static char buf[sizeof(TargetImpl)];  // NOLINT — uninitialized is intentional (fake target)
    auto* fakeTarget = reinterpret_cast<TargetImpl*>(buf);
    RenderCommands cmds(*fakeTarget, stack, batchState);

    // Branch(setLocalToWorld=translate) → GraphicsArray → MeshGraphic leaf, the tree
    // PrimitiveBuilder.finish() produces. The leaf carries one surface so pushAndPop
    // keeps the PushBranchCommand (it drops pushes with no following command).
    rhi::NullDriver driver;
    auto leaf = std::make_unique<MeshGraphic>(driver);
    auto surface = std::make_unique<SurfaceGeometry>(
        driver, rhi::IndexBufferHandle{}, 6u, SurfaceType::Opaque, false, false);
    surface->setPrimitive({});  // null handle — addCommands only routes, never draws
    leaf->addSurface(std::move(surface));
    auto array = std::make_unique<GraphicsArray>();
    array->add(std::move(leaf));
    auto branch = std::make_unique<Branch>();
    branch->setLocalToWorld(dqGeom::Transform::CreateTranslation(10.0, 20.0, 30.0));
    branch->setChild(std::move(array));

    RenderGraphicAdapter adapter(branch.get());
    adapter.addCommands(cmds);

    auto const* push = findRecordedPushBranch(cmds);
    ASSERT_NE(nullptr, push);
    EXPECT_FLOAT_EQ(10.0f, push->getMvp()[12]);
    EXPECT_FLOAT_EQ(20.0f, push->getMvp()[13]);
    EXPECT_FLOAT_EQ(30.0f, push->getMvp()[14]);
    EXPECT_FLOAT_EQ(10.0f, push->getMv()[12]);
    EXPECT_FLOAT_EQ(20.0f, push->getMv()[13]);
    EXPECT_FLOAT_EQ(30.0f, push->getMv()[14]);
}

// Non-quantized Mesh vertices are stored relative to the shared range center. This
// is WHY the branch translation above is required: the recentered vertex only lands
// at its world position when the range-center translation is folded back in at draw.
TEST(BranchLocalToWorldTest, MeshVerticesRecenteredAboutRangeCenter)
{
    Mesh::Props props;
    props.type = MeshPrimitiveType::Point;
    props.range = dqGeom::Range3d(0.0, 0.0, 0.0, 100.0, 100.0, 100.0);  // center (50,50,50)
    props.quantizePositions = false;
    auto mesh = Mesh::create(std::move(props));

    VertexKeyProps v;
    v.position = dqGeom::Point3d{50.0, 50.0, 50.0};    // == range center → stored as origin
    mesh->addVertex(v);
    v.position = dqGeom::Point3d{100.0, 100.0, 100.0};  // → stored as (50,50,50)
    mesh->addVertex(v);

    auto const& pts = mesh->points().points;
    ASSERT_EQ(2u, pts.size());
    EXPECT_NEAR(0.0, pts[0].x, 1e-6);
    EXPECT_NEAR(0.0, pts[0].y, 1e-6);
    EXPECT_NEAR(0.0, pts[0].z, 1e-6);
    EXPECT_NEAR(50.0, pts[1].x, 1e-6);
    EXPECT_NEAR(50.0, pts[1].y, 1e-6);
    EXPECT_NEAR(50.0, pts[1].z, 1e-6);
}

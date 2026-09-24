// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — PlanarGrid tests
// Ported from: itwinjs-core core/frontend/src/render/PlanarGrid.ts
#include <gtest/gtest.h>

#include <dqRender/RenderPipeline.h>

// ---------------------------------------------------------------------------
// PlanarGrid tests
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core core/frontend/src/test/PlanarGrid.test.ts
TEST(PlanarGridTest, PipelineInitialize)
{
    // Pipeline requires a GL context, so we test the public API shape
    // without actually initializing (would need a window).
    dqRender::RenderPipeline pipeline;
    EXPECT_FALSE(pipeline.isInitialized());
}

// Ported from: itwinjs-core core/frontend/src/test/PlanarGrid.test.ts
TEST(PlanarGridTest, PipelineShutdownWithoutInit)
{
    // shutdown on non-initialized pipeline should be a no-op
    dqRender::RenderPipeline pipeline;
    pipeline.shutdown();
    EXPECT_FALSE(pipeline.isInitialized());
}

// Ported from: itwinjs-core core/frontend/src/test/PlanarGrid.test.ts
TEST(PlanarGridTest, RenderGridWithoutInit)
{
    // renderGrid on non-initialized pipeline should be a no-op
    dqRender::RenderPipeline pipeline;
    float mvp[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float color[4] = {0.2f, 0.2f, 0.2f, 1.0f};
    // Should not crash
    pipeline.renderGrid(mvp, 800, 600, color);
}

// Ported from: itwinjs-core core/frontend/src/test/PlanarGrid.test.ts
TEST(PlanarGridTest, GetGridCountsWithoutInit)
{
    dqRender::RenderPipeline pipeline;
    EXPECT_EQ(pipeline.getGridVertexCount(), 0u);
    EXPECT_EQ(pipeline.getGridIndexCount(), 0u);
}

// Ported from: itwinjs-core core/frontend/src/test/PlanarGrid.test.ts
TEST(PlanarGridTest, BeginEndFrameWithoutInit)
{
    // beginFrame/endFrame on non-initialized pipeline should be no-ops
    dqRender::RenderPipeline pipeline;
    pipeline.beginFrame();
    pipeline.endFrame();
}

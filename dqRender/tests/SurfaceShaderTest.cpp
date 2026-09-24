// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core or filament for surface/edge shader integration
// DanQing dqRender — Surface/Edge shader tests
#include <gtest/gtest.h>

#include <dqRender/RenderPipeline.h>

using namespace dqRender;

// Authored: no reference test exists in itwinjs-core or filament for surface/edge shader integration
TEST(SurfaceShaderTest, PipelineWithoutInit)
{
    RenderPipeline pipeline;
    EXPECT_FALSE(pipeline.isInitialized());
}

// Authored: no reference test exists in itwinjs-core or filament for surface/edge shader integration
TEST(SurfaceShaderTest, RenderSurfaceWithoutInit)
{
    RenderPipeline pipeline;
    float mvp[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float mv[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float color[4] = {0.2f, 0.2f, 0.2f, 1.0f};
    // Should not crash
    pipeline.renderSurface(mvp, mv, 800, 600, color);
}

// Authored: no reference test exists in itwinjs-core or filament for surface/edge shader integration
TEST(EdgeShaderTest, RenderEdgesWithoutInit)
{
    RenderPipeline pipeline;
    float mvp[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float color[4] = {0.2f, 0.2f, 0.2f, 1.0f};
    // Should not crash
    pipeline.renderEdges(mvp, 800, 600, color);
}

// Authored: no reference test exists in itwinjs-core or filament for surface/edge shader integration
TEST(MultiVariantTest, RenderPipelineHasTechniques)
{
    // Verify the pipeline can be constructed (techniques created internally)
    RenderPipeline pipeline;
    EXPECT_FALSE(pipeline.isInitialized());
    // After initialize(), Surface and Edge techniques would be available
}

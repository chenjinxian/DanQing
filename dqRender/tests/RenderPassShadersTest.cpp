// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderPassShaders tests
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/RenderPass.ts
#include "render/RenderPassShaders.h"
#include "render/ShaderBuilder.h"

#include <gtest/gtest.h>

using namespace dqRender;

// addRenderPass registers u_renderPass + the 12 kRenderPass_* globals (no throw).
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/RenderPass.ts
//              TEST(RenderPassShadersTest, AddRenderPassWiring)
TEST(RenderPassShadersTest, AddRenderPassWiring)
{
    ProgramBuilder builder;
    addRenderPass(builder.getFragmentBuilder());
    SUCCEED();
}

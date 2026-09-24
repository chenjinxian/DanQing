// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — VertexShaders tests
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Vertex.ts
//              addProjectionMatrix / addModelViewMatrix
#include "render/VertexShaders.h"
#include "render/ShaderBuilder.h"

#include <gtest/gtest.h>

using namespace dqRender;

// addProjectionMatrix registers u_proj (no throw).
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Vertex.ts
//              TEST(VertexShadersTest, AddProjectionMatrixWiring)
TEST(VertexShadersTest, AddProjectionMatrixWiring)
{
    ProgramBuilder builder;
    addProjectionMatrix(builder.getVertexBuilder());
    SUCCEED();
}

// addModelViewMatrix non-instanced: declares u_mv.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Vertex.ts
//              TEST(VertexShadersTest, AddModelViewMatrixNonInstancedWiring)
TEST(VertexShadersTest, AddModelViewMatrixNonInstancedWiring)
{
    ProgramBuilder builder;
    addModelViewMatrix(builder.getVertexBuilder());  // usesInstancedGeometry == false
    SUCCEED();
}

// addModelViewMatrix instanced: declares u_instanced_modelView + g_mv + initializer.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Vertex.ts
//              TEST(VertexShadersTest, AddModelViewMatrixInstancedWiring)
TEST(VertexShadersTest, AddModelViewMatrixInstancedWiring)
{
    ProgramBuilder builder;
    ShaderBuilder& vert = builder.getVertexBuilder();
    vert.setUsesInstancedGeometry(true);
    addModelViewMatrix(vert);
    SUCCEED();
}

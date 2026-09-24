// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ViewportShaders tests
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Viewport.ts
#include "render/ViewportShaders.h"
#include "render/ShaderBuilder.h"

#include <gtest/gtest.h>
#include <string>

using namespace dqRender;

// modelToWindowCoordinates: references MAT_MV, u_proj, u_viewportTransformation,
// u_frustum, the overlay/background render-pass constants, and the segment-drop
// front-clip logic.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Viewport.ts
//              TEST(ViewportShadersTest, ModelToWindowCoordinatesGlslFaithful)
TEST(ViewportShadersTest, ModelToWindowCoordinatesGlslFaithful)
{
    std::string const s(kModelToWindowCoordinates);
    EXPECT_NE(s.find("modelToWindowCoordinates"), std::string::npos);
    EXPECT_NE(s.find("MAT_MV"), std::string::npos);
    EXPECT_NE(s.find("u_proj"), std::string::npos);
    EXPECT_NE(s.find("u_viewportTransformation"), std::string::npos);
    EXPECT_NE(s.find("u_frustum"), std::string::npos);
    EXPECT_NE(s.find("kRenderPass_ViewOverlay"), std::string::npos);
    EXPECT_NE(s.find("kRenderPass_Background"), std::string::npos);
    EXPECT_NE(s.find("s_maxZ"), std::string::npos);
}

// addViewport registers u_viewport.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Viewport.ts
//              TEST(ViewportShadersTest, AddViewportWiring)
TEST(ViewportShadersTest, AddViewportWiring)
{
    ProgramBuilder builder;
    addViewport(builder.getVertexBuilder());
    SUCCEED();
}

// addViewportTransformation registers u_viewportTransformation.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Viewport.ts
//              TEST(ViewportShadersTest, AddViewportTransformationWiring)
TEST(ViewportShadersTest, AddViewportTransformationWiring)
{
    ProgramBuilder builder;
    addViewportTransformation(builder.getVertexBuilder());
    SUCCEED();
}

// addModelToWindowCoordinates wires model-view + projection + viewport transform
// + render pass + the function (no throw).
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Viewport.ts
//              TEST(ViewportShadersTest, AddModelToWindowCoordinatesWiring)
TEST(ViewportShadersTest, AddModelToWindowCoordinatesWiring)
{
    ProgramBuilder builder;
    addModelToWindowCoordinates(builder.getVertexBuilder());
    SUCCEED();
}

// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Combine two textures shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/CombineTextures.ts
//
// GLSL functions for combining two vertically stacked textures (planar
// classification). The top half of the quad samples texture 0, the
// bottom half samples texture 1.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// CombineTextures — computeBaseColor
// ---------------------------------------------------------------------------
// Returns white as the base color (texture sampling overrides it).
// Ported from: itwinjs-core CombineTextures.ts computeBaseColor
static char const* kCombineTexturesComputeBaseColor = R"glsl(
return vec4(1.0);
)glsl";

// ---------------------------------------------------------------------------
// CombineTextures — assignFragData
// ---------------------------------------------------------------------------
// Samples from one of two textures based on vertical coordinate.
// Top half (v < 0.5): texture 0 scaled to [0..1].
// Bottom half (v >= 0.5): texture 1 scaled to [0..1].
// Ported from: itwinjs-core CombineTextures.ts assignFragData
static char const* kCombineTexturesAssignFragData = R"glsl(
  if (v_texCoord.y < .5)
    FragColor = TEXTURE(u_texture0, vec2(v_texCoord.x, v_texCoord.y * 2.0));
  else
    FragColor = TEXTURE(u_texture1, vec2(v_texCoord.x, v_texCoord.y * 2.0 - 1.0));
)glsl";

// ---------------------------------------------------------------------------
// CombineTextures — complete fragment shader
// ---------------------------------------------------------------------------
// Standalone fragment shader that combines two vertically stacked textures.
static char const* kCombineTexturesFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
out vec4 FragColor;
void main() {
    if (v_texCoord.y < .5)
        FragColor = texture(u_texture0, vec2(v_texCoord.x, v_texCoord.y * 2.0));
    else
        FragColor = texture(u_texture1, vec2(v_texCoord.x, v_texCoord.y * 2.0 - 1.0));
}
)glsl";

END_DQ_RENDER_NAMESPACE

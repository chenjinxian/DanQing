// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Combine three textures shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Combine3Textures.ts
//
// GLSL functions for combining three vertically stacked textures (planar
// classification draping). The quad is divided into thirds vertically,
// each third sampling from a different texture.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Combine3Textures — computeBaseColor
// ---------------------------------------------------------------------------
// Returns white as the base color (texture sampling overrides it).
// Ported from: itwinjs-core Combine3Textures.ts computeBaseColor
static char const* kCombine3TexturesComputeBaseColor = R"glsl(
return vec4(1.0);
)glsl";

// ---------------------------------------------------------------------------
// Combine3Textures — assignFragData
// ---------------------------------------------------------------------------
// Samples from one of three textures based on vertical coordinate.
// Top third (v < 1/3): texture 0.
// Middle third (1/3 <= v < 2/3): texture 1.
// Bottom third (v >= 2/3): texture 2.
// Ported from: itwinjs-core Combine3Textures.ts assignFragData
static char const* kCombine3TexturesAssignFragData = R"glsl(
  if (v_texCoord.y < (1.0 / 3.0))
    FragColor = TEXTURE(u_texture0, vec2(v_texCoord.x, v_texCoord.y * 3.0));
  else if (v_texCoord.y < (2.0 / 3.0))
    FragColor = TEXTURE(u_texture1, vec2(v_texCoord.x, v_texCoord.y * 3.0 - 1.0));
  else
    FragColor = TEXTURE(u_texture2, vec2(v_texCoord.x, v_texCoord.y * 3.0 - 2.0));
)glsl";

// ---------------------------------------------------------------------------
// Combine3Textures — complete fragment shader
// ---------------------------------------------------------------------------
// Standalone fragment shader that combines three vertically stacked textures.
static char const* kCombine3TexturesFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
out vec4 FragColor;
void main() {
    if (v_texCoord.y < (1.0 / 3.0))
        FragColor = texture(u_texture0, vec2(v_texCoord.x, v_texCoord.y * 3.0));
    else if (v_texCoord.y < (2.0 / 3.0))
        FragColor = texture(u_texture1, vec2(v_texCoord.x, v_texCoord.y * 3.0 - 1.0));
    else
        FragColor = texture(u_texture2, vec2(v_texCoord.x, v_texCoord.y * 3.0 - 2.0));
}
)glsl";

END_DQ_RENDER_NAMESPACE

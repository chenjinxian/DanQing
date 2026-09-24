// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Viewport quad shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/ViewportQuad.ts
//
// Viewport quad base builder. Provides a passthrough vertex shader for
// screen-aligned quads and a texture-sampling fragment shader for
// copy/composite operations.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Viewport quad vertex shader (passthrough)
// ---------------------------------------------------------------------------
// Positions are in NDC [-1..1]. Compute UV params in [0..1].
// Attributes: a_position (vec2)
// Varyings:   v_texCoord (vec2)
// Ported from: itwinjs-core ViewportQuad.ts computePosition / computeTexCoord
static char const* kViewportQuadVert = R"glsl(
#version 410 core

layout(location = 0) in vec2 a_position;

out vec2 v_texCoord;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texCoord = (a_position.xy + 1.0) * 0.5;
}
)glsl";

// ---------------------------------------------------------------------------
// Viewport quad vertex shader (with depth output)
// ---------------------------------------------------------------------------
// Same as above but also writes gl_FragDepth for depth-copy passes.
static char const* kViewportQuadDepthVert = R"glsl(
#version 410 core

layout(location = 0) in vec2 a_position;

out vec2 v_texCoord;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texCoord = (a_position.xy + 1.0) * 0.5;
    gl_FragDepth = 0.0;
}
)glsl";

// ---------------------------------------------------------------------------
// Viewport quad fragment shader (texture sampling)
// ---------------------------------------------------------------------------
// Samples a 2D texture at the computed UV coordinate.
// Uniforms:   u_texture (sampler2D)
// Varyings:   v_texCoord (vec2)
static char const* kViewportQuadFrag = R"glsl(
#version 410 core

in vec2 v_texCoord;
uniform sampler2D u_texture;
out vec4 fragColor;

void main()
{
    fragColor = texture(u_texture, v_texCoord);
}
)glsl";

END_DQ_RENDER_NAMESPACE

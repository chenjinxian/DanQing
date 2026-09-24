// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Screen space effect shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/ScreenSpaceEffect.ts
//
// Screen space effect program builder. The vertex shader calls a
// user-provided effectMain(rawPos) function; the fragment shader calls
// user-provided effectMain() for the output color.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Screen space effect vertex shader
// ---------------------------------------------------------------------------
// Calls user-provided effectMain(rawPos) then returns rawPos as the position.
// The user's effectMain may modify rawPos or perform side-effect computation.
// Attributes: a_position (vec4)
// Varyings:   v_texCoord (vec2)
// Ported from: itwinjs-core ScreenSpaceEffect.ts computePosition
static char const* kScreenSpaceEffectVert = R"glsl(
#version 410 core

layout(location = 0) in vec4 a_position;

out vec2 v_texCoord;

void main()
{
    vec4 rawPos = a_position;
    effectMain(rawPos);
    gl_Position = rawPos;
    v_texCoord = (rawPos.xy + 1.0) * 0.5;
}
)glsl";

// ---------------------------------------------------------------------------
// Screen space effect fragment shader
// ---------------------------------------------------------------------------
// Calls user-provided effectMain() to compute the fragment color.
// The user provides the function body via ShaderBuilder.
// Uniforms:   u_diffuse (sampler2D) — source texture for screen effects
// Varyings:   v_texCoord (vec2)
// Ported from: itwinjs-core ScreenSpaceEffect.ts computeBaseColor
static char const* kScreenSpaceEffectFrag = R"glsl(
#version 410 core

in vec2 v_texCoord;
uniform sampler2D u_diffuse;
out vec4 fragColor;

void main()
{
    fragColor = effectMain();
}
)glsl";

// ---------------------------------------------------------------------------
// Screen space effect fragment shader (with source pixel sampling)
// ---------------------------------------------------------------------------
// Variant that supports reading back source pixels during readPixels.
// When u_readingPixels is true, samples source pixel directly.
// Ported from: itwinjs-core ScreenSpaceEffect.ts computeBaseColorWithShift
static char const* kScreenSpaceEffectFragWithSample = R"glsl(
#version 410 core

in vec2 v_texCoord;
uniform sampler2D u_diffuse;
uniform bool u_readingPixels;
out vec4 fragColor;

vec2 textureCoordFromPosition(vec4 pos) {
    return (pos.xy + 1.0) * 0.5;
}

void main()
{
    fragColor = u_readingPixels ? sampleSourcePixel() : effectMain();
}
)glsl";

END_DQ_RENDER_NAMESPACE

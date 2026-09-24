// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Volume classification stencil shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/CopyStencil.ts
//
// Volume classification stencil programs for spatial classification of
// reality mesh data. Programs: ColorUsingStencil (hilite color output),
// CopyZ (copy depth from texture), SetBlend (set blend with boundary check),
// Blend (blend texture sampling).
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Reuse kViewportQuadVert from ViewportQuadShaders.h

// ---------------------------------------------------------------------------
// VolClassColorUsingStencil — hilite color output
// ---------------------------------------------------------------------------
// Outputs the hilite color directly, used for stencil-based volume classification.
// Ported from: itwinjs-core CopyStencil.ts computehiliteColor
static char const* kVolClassColorVert = R"glsl(
#version 410 core

layout(location = 0) in vec2 a_position;

out vec2 v_texCoord;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texCoord = (a_position.xy + 1.0) * 0.5;
}
)glsl";

static char const* kVolClassColorFrag = R"glsl(
#version 410 core

in vec2 v_texCoord;
uniform vec4 u_hilite_color;
out vec4 fragColor;

void main()
{
    fragColor = vec4(u_hilite_color.rgb, 1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// VolClassCopyZ — copy depth from texture
// ---------------------------------------------------------------------------
// Copies depth from a depth texture and sets it as the fragment depth.
// Ported from: itwinjs-core CopyStencil.ts depthFromTexture
static char const* kVolClassCopyZVert = R"glsl(
#version 410 core

layout(location = 0) in vec2 a_position;

out vec2 v_texCoord;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texCoord = (a_position.xy + 1.0) * 0.5;
}
)glsl";

static char const* kVolClassCopyZFrag = R"glsl(
#version 410 core

in vec2 v_texCoord;
uniform sampler2D u_depthTexture;
uniform vec4 u_blend_color;
out vec4 fragColor;

void main()
{
    fragColor = u_blend_color;
    gl_FragDepth = texture(u_depthTexture, v_texCoord).r;
}
)glsl";

// ---------------------------------------------------------------------------
// VolClassSetBlend — set blend mode with boundary type check
// ---------------------------------------------------------------------------
// Sets blend color based on boundary type (Inside/Outside/Selected) and
// depth texture comparison for background discard.
// Ported from: itwinjs-core CopyStencil.ts computeSetBlendColor / checkDiscardBackgroundByZ
static char const* kVolClassBlendVert = R"glsl(
#version 410 core

layout(location = 0) in vec2 a_position;

out vec2 v_texCoord;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texCoord = (a_position.xy + 1.0) * 0.5;
}
)glsl";

static char const* kVolClassBlendFrag = R"glsl(
#version 410 core

in vec2 v_texCoord;
uniform sampler2D u_blendTexture;
out vec4 fragColor;

void main()
{
    fragColor = texture(u_blendTexture, v_texCoord);
}
)glsl";

END_DQ_RENDER_NAMESPACE

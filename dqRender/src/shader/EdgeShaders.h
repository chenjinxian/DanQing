// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Edge shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Edge.ts
//
// Minimal Edge shader: MVP transform, unlit (no lighting).
// Phase 1: simple GL_LINES. Screen-space quad expansion in Phase 2.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Edge vertex shader
// ---------------------------------------------------------------------------
static char const* kEdgeVert = R"glsl(
#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;

uniform mat4 u_mvp;

out vec4 v_color;

void main()
{
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_color = a_color;
}
)glsl";

// ---------------------------------------------------------------------------
// Edge fragment shader
// ---------------------------------------------------------------------------
// Unlit monochrome — edges are not affected by lighting.
static char const* kEdgeFrag = R"glsl(
#version 410 core

in vec4 v_color;
out vec4 fragColor;

void main()
{
    fragColor = v_color;
}
)glsl";

END_DQ_RENDER_NAMESPACE

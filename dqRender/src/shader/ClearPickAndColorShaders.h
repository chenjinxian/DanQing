// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Clear pick and color buffer shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/ClearPickAndColor.ts
//
// GLSL functions for clearing the pick and color buffers. The clear
// program writes the background color to the color attachment and zeros
// out the pick and feature-id attachments.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// ClearPickAndColor — computeBaseColor
// ---------------------------------------------------------------------------
// Returns the background color as the base color.
// Ported from: itwinjs-core ClearPickAndColor.ts computeBaseColor
static char const* kClearPickAndColorComputeBaseColor = R"glsl(
return u_bgColor;
)glsl";

// ---------------------------------------------------------------------------
// ClearPickAndColor — assignFragData
// ---------------------------------------------------------------------------
// Writes baseColor to color attachment 0, zeros out attachments 1 and 2.
// Requires GL_EXT_draw_buffers / MRT with 3 draw buffers.
// Ported from: itwinjs-core ClearPickAndColor.ts assignFragData
static char const* kClearPickAndColorAssignFragData = R"glsl(
  FragColor0 = baseColor;
  FragColor1 = vec4(0.0);
  FragColor2 = vec4(0.0);
)glsl";

// ---------------------------------------------------------------------------
// ClearPickAndColor — complete fragment shader
// ---------------------------------------------------------------------------
// Standalone fragment shader that clears all three MRT attachments.
static char const* kClearPickAndColorFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform vec4 u_bgColor;
layout(location = 0) out vec4 FragColor0;
layout(location = 1) out vec4 FragColor1;
layout(location = 2) out vec4 FragColor2;
void main() {
    vec4 baseColor = u_bgColor;
    FragColor0 = baseColor;
    FragColor1 = vec4(0.0);
    FragColor2 = vec4(0.0);
}
)glsl";

END_DQ_RENDER_NAMESPACE

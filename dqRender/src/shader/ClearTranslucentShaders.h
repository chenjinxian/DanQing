// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Clear translucent OIT buffer shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/ClearTranslucent.ts
//
// GLSL functions for clearing the order-independent transparency (OIT)
// translucent buffers. Writes initial values into the two MRT attachments
// used by the translucency pass.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// ClearTranslucent — computeBaseColor
// ---------------------------------------------------------------------------
// Returns transparent black (no contribution to base color).
// Ported from: itwinjs-core ClearTranslucent.ts computeBaseColor
static char const* kClearTranslucentComputeBaseColor = R"glsl(
return vec4(0.0);
)glsl";

// ---------------------------------------------------------------------------
// ClearTranslucent — assignFragData
// ---------------------------------------------------------------------------
// Writes initial OIT values: depth-and-order = (0,0,0,1), nearest-order = (1,0,0,1).
// Requires GL_EXT_draw_buffers / MRT with 2 draw buffers.
// Ported from: itwinjs-core ClearTranslucent.ts assignFragData
static char const* kClearTranslucentAssignFragData = R"glsl(
  FragColor0 = vec4(0.0, 0.0, 0.0, 1.0);
  FragColor1 = vec4(1.0, 0.0, 0.0, 1.0);
)glsl";

// ---------------------------------------------------------------------------
// ClearTranslucent — complete fragment shader
// ---------------------------------------------------------------------------
// Standalone fragment shader that clears the two OIT translucent attachments.
static char const* kClearTranslucentFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
layout(location = 0) out vec4 FragColor0;
layout(location = 1) out vec4 FragColor1;
void main() {
    FragColor0 = vec4(0.0, 0.0, 0.0, 1.0);
    FragColor1 = vec4(1.0, 0.0, 0.0, 1.0);
}
)glsl";

END_DQ_RENDER_NAMESPACE

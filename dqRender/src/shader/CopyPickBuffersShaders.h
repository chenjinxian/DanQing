// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — copy pick buffers shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/CopyPickBuffers.ts
//
// GLSL functions for copying pick buffer data (feature ID and depth/order)
// from one set of textures to another. Used when the pick buffers must be
// preserved across render passes.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// CopyPickBuffers — computeBaseColor
// ---------------------------------------------------------------------------
// Returns white as the base color (not used for final output).
// Ported from: itwinjs-core CopyPickBuffers.ts computeBaseColor
static char const* kCopyPickBuffersComputeBaseColor = R"glsl(
return vec4(1.0);
)glsl";

// ---------------------------------------------------------------------------
// CopyPickBuffers — assignFragData
// ---------------------------------------------------------------------------
// Copies feature ID texture to attachment 0 and depth-and-order texture
// to attachment 1.
// Ported from: itwinjs-core CopyPickBuffers.ts assignFragData
static char const* kCopyPickBuffersAssignFragData = R"glsl(
  FragColor0 = TEXTURE(u_pickFeatureId, v_texCoord);
  FragColor1 = TEXTURE(u_pickDepthAndOrder, v_texCoord);
)glsl";

// ---------------------------------------------------------------------------
// CopyPickBuffers — complete fragment shader
// ---------------------------------------------------------------------------
// Standalone fragment shader that copies both pick buffer textures.
static char const* kCopyPickBuffersFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_pickFeatureId;
uniform sampler2D u_pickDepthAndOrder;
layout(location = 0) out vec4 FragColor0;
layout(location = 1) out vec4 FragColor1;
void main() {
    FragColor0 = texture(u_pickFeatureId, v_texCoord);
    FragColor1 = texture(u_pickDepthAndOrder, v_texCoord);
}
)glsl";

END_DQ_RENDER_NAMESPACE

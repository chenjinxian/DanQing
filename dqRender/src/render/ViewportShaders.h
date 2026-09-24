// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Viewport shader module
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Viewport.ts
//
// Viewport dimensions + the viewport-transformation matrix used to map NDC to
// window coords, plus modelToWindowCoordinates (the segment-drop vertex helper).
// Depends on Vertex (addModelViewMatrix/addProjectionMatrix) + RenderPass.
#pragma once

#include "RenderPassShaders.h"  // addRenderPass
#include "ShaderBindings.h"     // wireViewport, wireViewportTransformation
#include "ShaderBuilder.h"
#include "VertexShaders.h"      // addModelViewMatrix, addProjectionMatrix

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Reference GLSL fragment (exact port from Viewport.ts modelToWindowCoordinates)
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Viewport.ts modelToWindowCoordinates
inline constexpr char const* kModelToWindowCoordinates = R"(
vec4 modelToWindowCoordinates(vec4 position, vec4 next, out vec4 clippedMvpPos, out vec3 clippedMvPos) {
  if (kRenderPass_ViewOverlay == u_renderPass || kRenderPass_Background == u_renderPass) {
    vec4 q = MAT_MV * position;
    clippedMvPos = q.xyz;
    q = u_proj * q;
    clippedMvpPos = q;
    q.xyz /= q.w;
    q.xyz = (u_viewportTransformation * vec4(q.xyz, 1.0)).xyz;
    return q;
  }

  // Negative values are in front of the camera (visible).
  float s_maxZ = -u_frustum.x;            // use -near (front) plane for segment drop test since u_frustum's near & far are pos.
  vec4  q = MAT_MV * position;            // eye coordinates.
  vec4  n = MAT_MV * next;

  if (q.z > s_maxZ) {
    if (n.z > s_maxZ) {
      clippedMvPos = vec3(0.0, 0.0, 1.0);
      clippedMvpPos = vec4(0.0, 0.0, 1.0, 0.0);
      return vec4(0.0, 0.0, 1.0, 0.0);    // Entire segment behind front clip plane.
    }

    float t = (s_maxZ - q.z) / (n.z - q.z);

    q.x += t * (n.x - q.x);
    q.y += t * (n.y - q.y);
    q.z = s_maxZ;                         // q.z + (s_maxZ - q.z) / (n.z - q.z) * (n.z - q.z) = s_maxZ
  }

  clippedMvPos = q.xyz;
  q = u_proj * q;
  clippedMvpPos = q;
  q.xyz /= q.w;                           // normalized device coords
  q.xyz = (u_viewportTransformation * vec4(q.xyz, 1.0)).xyz; // window coords
  return q;
}
)";

// ---------------------------------------------------------------------------
// Wiring (exact ports of Viewport.ts addXxx)
//
// Uniform VALUE bindings (u_viewport, u_viewportTransformation) are registered
// with binding=nullptr per convention.
// ---------------------------------------------------------------------------

/// add the u_viewport (dimensions) uniform.
/// Ported from: itwinjs-core Viewport.ts addViewport()
inline void addViewport(ShaderBuilder& shader)
{
    wireViewport(shader);
}

/// add the u_viewportTransformation matrix uniform.
/// Ported from: itwinjs-core Viewport.ts addViewportTransformation()
inline void addViewportTransformation(ShaderBuilder& shader)
{
    wireViewportTransformation(shader);
}

/// Wire modelToWindowCoordinates: model-view + projection + viewport transform +
/// render pass + the function itself.
/// Ported from: itwinjs-core Viewport.ts addModelToWindowCoordinates()
inline void addModelToWindowCoordinates(ShaderBuilder& vert)
{
    addModelViewMatrix(vert);
    addProjectionMatrix(vert);
    addViewportTransformation(vert);
    addRenderPass(vert);
    vert.addFunction(std::string(kModelToWindowCoordinates));
}

END_DQ_RENDER_NAMESPACE

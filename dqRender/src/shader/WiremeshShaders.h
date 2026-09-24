// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Wiremesh overlay shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Wiremesh.ts
//
// GLSL functions for rendering a wiremesh overlay on triangle meshes.
// Uses gl_VertexID-based barycentric coordinates and screen-space
// derivatives (fwidth) to draw smooth wireframe lines along triangle edges.
// Requires GLSL 330 / OpenGL 3.3 (gl_VertexID, fwidth).
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Wiremesh — computeBarycentric (vertex)
// ---------------------------------------------------------------------------
// Produces barycentric coordinates for each triangle corner using
// gl_VertexID. Requires non-indexed vertices or an index buffer where
// each set of 3 consecutive indices corresponds to one triangle.
// Ported from: itwinjs-core Wiremesh.ts computeBarycentric
static char const* kWiremeshComputeBarycentric = R"glsl(
  int vertIndex = gl_VertexID % 3;
  v_barycentric = vec3(float(0 == vertIndex), float(1 == vertIndex), float(2 == vertIndex));
)glsl";

// ---------------------------------------------------------------------------
// Wiremesh — applyWiremesh (fragment)
// ---------------------------------------------------------------------------
// Draws black wireframe lines for fragments close to triangle edges.
// Uses fwidth() for screen-space line width and smoothstep for anti-aliasing.
// Ported from: itwinjs-core Wiremesh.ts applyWiremesh
static char const* kWiremeshApplyWiremesh = R"glsl(
  const float lineWidth = 1.0;
  const vec3 lineColor = vec3(0.0);
  vec3 delta = fwidth(v_barycentric);
  vec3 factor = smoothstep(vec3(0.0), delta * lineWidth, v_barycentric);
  vec3 color = mix(lineColor, baseColor.rgb, min(min(factor.x, factor.y), factor.z));
  return vec4(color, baseColor.a);
)glsl";

END_DQ_RENDER_NAMESPACE

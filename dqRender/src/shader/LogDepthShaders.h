// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Logarithmic depth buffer shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/LogarithmicDepthBuffer.ts
//
// Minimal logarithmic depth buffer finalization. The finalizeDepth function
// converts linear eye-space Z to logarithmic depth using the u_logZ uniform.
// Based on http://tulrich.com/geekstuff/log_depth_buffer.txt
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Logarithmic depth functions (included by other shaders via ShaderBuilder)
// ---------------------------------------------------------------------------
// Uniforms:   u_logZ (vec2) — x = scale factor, y = log2(far) scaling
// Varyings:   v_eyeSpace (vec3) — eye-space position
// Ported from: itwinjs-core LogarithmicDepthBuffer.ts finalizeDepth
static char const* kLogDepthFunctions = R"glsl(
float finalizeDepth(float eyeZ, vec2 logZ) {
  return 0.0 == logZ.x ? -eyeZ / logZ.y : log(-eyeZ * logZ.x) / logZ.y;
}
)glsl";

END_DQ_RENDER_NAMESPACE

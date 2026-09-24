// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Vertex shader module (transform subset)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Vertex.ts
//
// This header ports the model-view / projection matrix wiring that other glsl
// modules (Viewport, Common.addEyeSpace) depend on. The rest of Vertex.ts
// (quantized positions, vertex LUT sampling, line weight/code, normal matrix)
// is deferred -- it is consumed by the surface/edge/polyline shader builders,
// which is broader work.
#pragma once

#include "ShaderBuilder.h"
#include "ShaderBindings.h"  // wireProjectionMatrix

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Wiring (exact ports of Vertex.ts addProjectionMatrix / addModelViewMatrix)
//
// Uniform VALUE bindings (u_proj, u_mv, u_instanced_modelView) are registered
// with binding=nullptr per convention.
// ---------------------------------------------------------------------------

/// add the projection matrix uniform u_proj.
/// Ported from: itwinjs-core Vertex.ts addProjectionMatrix()
inline void addProjectionMatrix(ShaderBuilder& vert)
{
    wireProjectionMatrix(vert);
}

/// add the model-view matrix. For instanced geometry, declares
/// u_instanced_modelView + the g_mv global (computed from g_modelMatrixRTC);
/// otherwise declares u_mv directly.
/// Ported from: itwinjs-core Vertex.ts addModelViewMatrix()
inline void addModelViewMatrix(ShaderBuilder& vert)
{
    if (vert.usesInstancedGeometry()) {
        vert.addUniform("u_instanced_modelView", VariableType::Mat4, nullptr);
        vert.addGlobal("g_mv", VariableType::Mat4);
        vert.addInitializer("g_mv = u_instanced_modelView * g_modelMatrixRTC;");
    } else {
        wireModelViewMatrix(vert);
    }
}

// TODO (deferred from Vertex.ts): addModelViewProjectionMatrix, addInstancedRtcMatrix,
// addNormalMatrix, addPosition/addPositionFromLUT, addSamplePosition, addLineWeight,
// addLineCode, addAlpha, the unquantizePosition/computeVertexPosition GLSL, and the
// earlyVertexDiscard/vertexDiscard/lateVertexDiscard snippets. These are consumed by
// the surface/edge/polyline shader builders and the quantized-position pipeline.

END_DQ_RENDER_NAMESPACE

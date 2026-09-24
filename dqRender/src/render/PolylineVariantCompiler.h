// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Polyline variant shader compiler
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Technique.ts
//              (line 445-476 — Polyline technique registration via
//              AttributeMap + createPolylineBuilder) + glsl/Polyline.ts
//              createPolylineBuilder() (line 413-429).
//
// Generates Polyline shader GLSL source by composing the faithful 1:1 helpers
// in createPolylineProgramBuilder: addShaderFlags → addCommon →
// polylineAddLineCode → addColor → addEdgeContrast → addWhiteOnWhiteReversal.
// Each helper is a self-contained composition unit that adds its
// uniforms/varyings/functions/slots to a ProgramBuilder.
//
// The 4-attribute set (a_pos / a_prevIndex / a_nextIndex / a_param) matches
// itwinjs AttributeMap.ts:69-74 verbatim. Current-vertex position is resolved
// via the VertexLUT pre-read keyed by a_pos; prev/next via the on-demand
// unquantized 4-texel samplePosition (Vertex.ts:78-89); color via u_color.
#pragma once

#include "MultiVariantTechnique.h"

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// PolylineVariantCompiler — generates Polyline (thick-line miter) shader variants
// Ported from: itwinjs-core Technique.ts:445-476 (Polyline technique registration)
// ---------------------------------------------------------------------------
class PolylineVariantCompiler : public VariantShaderCompiler {
public:
    /// Build one Polyline variant: compose modular helpers into a ProgramBuilder,
    /// generate GLSL source, set the 4-attribute map (a_pos / a_prevIndex /
    /// a_nextIndex / a_param), and attach uniform bindings (u_vertLUT, u_mv,
    /// u_lineWeight, u_color, u_viewport, ...).
    void buildProgram(ShaderProgram& prog, TechniqueFlags const& flags) override;

    char const* getDescription() const override { return "Polyline"; }
};

END_DQ_RENDER_NAMESPACE

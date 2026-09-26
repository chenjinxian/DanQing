// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface variant shader compiler
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Surface.ts
//
// Generates Surface shader GLSL source by composing modular addXxx helpers:
// createCommon, addColor, addSurfaceFlags, addNormal, addTexture, addMaterial,
// addLighting, addFragData.  Each helper is a self-contained composition unit
// that adds its uniforms/varyings/functions/slots to a ProgramBuilder.
#pragma once

#include "MultiVariantTechnique.h"
#include "render/shader/EdgeShaderBuilder.h"  // EdgeBuilderType（EdgeVariantCompiler 参数）

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// SurfaceVariantCompiler — generates Surface shader variants
// ---------------------------------------------------------------------------
class SurfaceVariantCompiler : public VariantShaderCompiler {
public:
    /// Build one Surface variant: compose modular helpers into a ProgramBuilder,
    /// generate GLSL source, and attach uniform bindings (u_frustum, u_mv, etc.).
    void buildProgram(ShaderProgram& prog, TechniqueFlags const& flags) override;

    char const* getDescription() const override { return "Surface"; }
};

// ---------------------------------------------------------------------------
// EdgeVariantCompiler — generates Edge shader variants
// ---------------------------------------------------------------------------
class EdgeVariantCompiler : public VariantShaderCompiler {
public:
    /// @param type which edge builder variant this technique compiles.
    /// Ported from: itwinjs-core Technique.ts:494-509 EdgeTechnique——同一
    /// EdgeTechnique 类按 EdgeBuilderType 参数化，注册三种：
    /// TechniqueId.Edge←"SegmentEdge"（:1088）、SilhouetteEdge←"Silhouette"
    /// （:1089）、IndexedEdge←"IndexedEdge"（:1100）。
    explicit EdgeVariantCompiler(EdgeBuilderType type) : m_type(type) {}

    /// Build one Edge variant: set GLSL source + attribute map (flat strings;
    /// uniform bindings ride the legacy upload path for u_mvp/u_viewport etc.).
    void buildProgram(ShaderProgram& prog, TechniqueFlags const& flags) override;

    char const* getDescription() const override { return "Edge"; }

private:
    EdgeBuilderType m_type;
};

END_DQ_RENDER_NAMESPACE

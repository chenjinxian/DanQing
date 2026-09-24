// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Multi-variant technique (live shader path)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Technique.ts
//               (VariedTechnique + the on-demand variant compilation model)
//
// A MultiVariantTechnique owns a VariantShaderCompiler and lazily builds one
// ShaderProgram per TechniqueFlags combination.  This is the LIVE Surface/Edge
// shader path: RenderPipeline registers MultiVariantTechnique for TechniqueId::Surface
// and ::Edge (constructed with SurfaceVariantCompiler / EdgeVariantCompiler).
//
// Variant programs are built on first request (getShader) via
// VariantShaderCompiler::buildProgram — which sets the GLSL source AND registers
// uniform bindings (addBindings, the linchpin that lets glsl-module addUniform
// callbacks become live ProgramUniform/GraphicUniform binds).  Compilation itself
// is lazy, performed by ShaderProgram::use() on first draw (matching the
// reference's per-program lazy compile).
#pragma once

#include "TechniqueImpl.h"  // Technique, VariantShaderCompiler, TechniqueFlags, ShaderProgram

#include <array>
#include <cstddef>
#include <memory>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// MultiVariantTechnique — on-demand multi-variant shader technique
// Ported from: itwinjs-core Technique.ts VariedTechnique (line 109-265)
//
// The reference VariedTechnique pre-builds every variant in its constructor and
// compiles them in compileShaders().  DanQing instead builds lazily in getShader()
// (compile-on-demand via ShaderProgram::use()), which avoids eagerly compiling
// the full 512-entry variant table at startup while preserving the same
// flags->program mapping.
// ---------------------------------------------------------------------------
class MultiVariantTechnique : public Technique {
public:
    /// Construct with the compiler that generates variant GLSL + bindings.
    /// Ported from: itwinjs-core VariedTechnique constructor (line 169-171)
    explicit MultiVariantTechnique(std::unique_ptr<VariantShaderCompiler> compiler)
        : m_compiler(std::move(compiler)) {}

    /// Return the shader program for the given flags, building it on first use.
    /// Ported from: itwinjs-core VariedTechnique.getShader() (line 241-255)
    ShaderProgram* getShader(TechniqueFlags const& flags) override;

    /// Total number of variant slots (for parity with the reference API).
    /// Ported from: itwinjs-core VariedTechnique.getShaderCount() (line 263-265)
    size_t getShaderCount() const override { return kVariantCount; }

    /// No-op: variants compile lazily in ShaderProgram::use().
    /// RenderPipeline calls this eagerly at construction; the reference compiles
    /// every variant here, but DanQing defers to first draw.
    bool compileShaders(rhi::Driver& /*driver*/) override { return true; }

private:
    /// Map TechniqueFlags to a variant index (9-bit hash).
    /// Ported from: itwinjs-core computeShaderIndex() per-technique.
    static size_t computeShaderIndex(TechniqueFlags const& flags) noexcept;

    // 9 flag bits -> indices 0..511.
    // bit 0 translucent, 1 quantized, 2-3 featureMode, 4 instanced,
    // 5 shadowable, 6 animated, 7 classified, 8 thematic.
    static constexpr size_t kVariantCount = 512;

    std::unique_ptr<VariantShaderCompiler> m_compiler;
    std::array<std::unique_ptr<ShaderProgram>, kVariantCount> m_programs;
};

END_DQ_RENDER_NAMESPACE

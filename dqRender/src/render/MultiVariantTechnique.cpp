// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Multi-variant technique implementation (live shader path)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Technique.ts
//               VariedTechnique (line 109-265)
//
// Lazily builds one ShaderProgram per TechniqueFlags combination via the owned
// VariantShaderCompiler.  This is the Surface/Edge live shader path registered
// by RenderPipeline.  Variant programs are built on first request (getShader):
// buildProgram sets the GLSL source AND registers uniform bindings (addBindings);
// ShaderProgram::use() compiles lazily on first draw (matching the reference).
#include "MultiVariantTechnique.h"

BEGIN_DQ_RENDER_NAMESPACE

// computeShaderIndex — 9-bit hash of TechniqueFlags -> variant slot.
// Ported from: itwinjs-core per-technique computeShaderIndex().
// bit 0 translucent, 1 quantized, 2-3 featureMode, 4 instanced,
// 5 shadowable, 6 animated, 7 classified, 8 thematic.
size_t MultiVariantTechnique::computeShaderIndex(TechniqueFlags const& flags) noexcept
{
    size_t index = 0;
    if (flags.isTranslucent) index |= 1;
    if (flags.usesQuantizedPositions()) index |= 2;
    index |= (static_cast<size_t>(flags.featureMode) & 0x3) << 2;
    if (flags.isInstanced) index |= 16;
    if (flags.isShadowable) index |= 32;
    if (flags.isAnimated) index |= 64;
    if (flags.isClassified) index |= 128;
    if (flags.isThematic) index |= 256;
    return index;
}

// getShader — return the variant program, building it on first request.
// Ported from: itwinjs-core VariedTechnique.getShader() (line 241-255) combined
//               with addShader()/addProgram() (line 176-205) as build-on-demand.
//
// The reference pre-builds every variant in its constructor; DanQing builds
// lazily here so only flag combinations actually drawn pay the build+compile
// cost.  buildProgram sets source + registers bindings; use() compiles.
ShaderProgram* MultiVariantTechnique::getShader(TechniqueFlags const& flags)
{
    if (!m_compiler) return nullptr;

    size_t index = computeShaderIndex(flags);
    if (index >= kVariantCount) return nullptr;

    auto& slot = m_programs[index];
    if (!slot) {
        auto prog = std::make_unique<ShaderProgram>();
        m_compiler->buildProgram(*prog, flags);
        slot = std::move(prog);
    }
    return slot.get();
}

END_DQ_RENDER_NAMESPACE

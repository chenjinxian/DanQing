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

    // M-P P-D：参考 VariedTechnique.getShader（Technique.ts:241-255）——
    // numClipPlanes > 0 → clip 变体（builder 侧 addClipping）；否则 basic。
    auto& slot = flags.hasClip() ? m_clipPrograms[index] : m_programs[index];
    if (!slot) {
        auto prog = std::make_unique<ShaderProgram>();
        m_compiler->buildProgram(*prog, flags);
        slot = std::move(prog);
    }
    return slot.get();
}

// compileAllVariants — debug 面：全变体矩阵枚举（getShader 命中即建，
// ensureCompiled 即编译——不动 getShader 的懒建语义/启动路径零成本）。
// Ported from: itwinjs-core Technique.ts SurfaceTechnique ctor 的守卫枚举
// （:318-348——posType × instanced × animated × shadowable × wiremesh ×
// thematic × edgeTestNeeded × featureMode × translucent；:328 None 模式免
// edgeTest、:329-330 thematic × shadowable 互斥）。
bool MultiVariantTechnique::compileAllVariants(rhi::Driver& driver)
{
    bool all = true;
    for (int posIdx = 0; posIdx <= 1; ++posIdx) {
        auto const posType = static_cast<PositionType>(posIdx);
        for (int instanced = 0; instanced <= 1; ++instanced) {
            for (int animated = 0; animated <= 1; ++animated) {
                for (int shadowable = 0; shadowable <= 1; ++shadowable) {
                    for (int wiremesh = 0; wiremesh <= 1; ++wiremesh) {
                        for (int thematic = 0; thematic <= 1; ++thematic) {
                            for (int edgeTest = 0; edgeTest <= 1; ++edgeTest) {
                                for (auto featureMode :
                                     {FeatureMode::None, FeatureMode::Pick,
                                      FeatureMode::Overrides}) {
                                    for (int translucent = 0; translucent <= 1;
                                         ++translucent) {
                                        if (FeatureMode::None == featureMode
                                            && 0 != edgeTest)
                                            continue;  // :328
                                        if (1 == thematic && 1 == shadowable)
                                            continue;  // :329-330 disallowed

                                        TechniqueFlags flags;
                                        flags.reset(featureMode, instanced != 0,
                                                    shadowable != 0, thematic != 0,
                                                    posType);
                                        flags.isAnimated = animated != 0;
                                        flags.isEdgeTestNeeded = edgeTest != 0;
                                        flags.isTranslucent = translucent != 0;
                                        flags.isWiremesh = wiremesh != 0;
                                        ShaderProgram* prog = getShader(flags);
                                        if (prog == nullptr
                                            || prog->ensureCompiled(driver)
                                                   != CompileStatus::Success)
                                            all = false;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return all;
}

END_DQ_RENDER_NAMESPACE

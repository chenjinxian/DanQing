// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Composite techniques implementation
// Ported from: filament filament/src/details/Engine.cpp composite rendering techniques
#include "CompositeTechniques.h"
#include "ShaderProgramImpl.h"
#include "shader/CompositeShaders.h"
#include "shader/PostProcessShaders.h"  // kFullscreenQuadVert

BEGIN_DQ_RENDER_NAMESPACE

CompositeHiliteTechnique::CompositeHiliteTechnique() : SingularTechnique(ShaderProgram()) {}
bool CompositeHiliteTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    auto frag = compositeHiliteFrag();
    return p->compile(driver, kFullscreenQuadVert, frag.c_str(), "CompositeHilite") == CompileStatus::Success;
}

CompositeHiliteAndTranslucentTechnique::CompositeHiliteAndTranslucentTechnique() : SingularTechnique(ShaderProgram()) {}
bool CompositeHiliteAndTranslucentTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    auto frag = compositeHiliteAndTranslucentFrag();
    return p->compile(driver, kFullscreenQuadVert, frag.c_str(), "CompositeHiliteAndTranslucent") == CompileStatus::Success;
}

CompositeOcclusionTechnique::CompositeOcclusionTechnique() : SingularTechnique(ShaderProgram()) {}
bool CompositeOcclusionTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kCompositeOcclusionFrag, "CompositeOcclusion") == CompileStatus::Success;
}

CompositeTranslucentAndOcclusionTechnique::CompositeTranslucentAndOcclusionTechnique() : SingularTechnique(ShaderProgram()) {}
bool CompositeTranslucentAndOcclusionTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kCompositeTranslucentAndOcclusionFrag, "CompositeTranslucentAndOcclusion") == CompileStatus::Success;
}

CompositeHiliteAndOcclusionTechnique::CompositeHiliteAndOcclusionTechnique() : SingularTechnique(ShaderProgram()) {}
bool CompositeHiliteAndOcclusionTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    auto frag = compositeHiliteAndOcclusionFrag();
    return p->compile(driver, kFullscreenQuadVert, frag.c_str(), "CompositeHiliteAndOcclusion") == CompileStatus::Success;
}

CopyColorTechnique::CopyColorTechnique() : SingularTechnique(ShaderProgram()) {}
bool CopyColorTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kCopyColorFrag, "CopyColor") == CompileStatus::Success;
}

CopyColorNoAlphaTechnique::CopyColorNoAlphaTechnique() : SingularTechnique(ShaderProgram()) {}
bool CopyColorNoAlphaTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kCopyColorNoAlphaFrag, "CopyColorNoAlpha") == CompileStatus::Success;
}

CopyPickBuffersTechnique::CopyPickBuffersTechnique() : SingularTechnique(ShaderProgram()) {}
bool CopyPickBuffersTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kCopyPickBuffersFrag, "CopyPickBuffers") == CompileStatus::Success;
}

ClearPickAndColorTechnique::ClearPickAndColorTechnique() : SingularTechnique(ShaderProgram()) {}
bool ClearPickAndColorTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kClearPickAndColorFrag, "ClearPickAndColor") == CompileStatus::Success;
}

EvsmFromDepthTechnique::EvsmFromDepthTechnique() : SingularTechnique(ShaderProgram()) {}
bool EvsmFromDepthTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kEvsmFromDepthFrag, "EvsmFromDepth") == CompileStatus::Success;
}

END_DQ_RENDER_NAMESPACE

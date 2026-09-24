// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Volume classification techniques implementation
// Ported from: itwinjs-core core/frontend/src/render/VolumeClassifier.ts
#include "VolumeClassTechniques.h"
#include "ShaderProgramImpl.h"
#include "shader/VolumeClassShaders.h"
#include "shader/PostProcessShaders.h"  // kFullscreenQuadVert

BEGIN_DQ_RENDER_NAMESPACE

VolClassColorUsingStencilTechnique::VolClassColorUsingStencilTechnique() : SingularTechnique(ShaderProgram()) {}
bool VolClassColorUsingStencilTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kVolClassColorUsingStencilFrag, "VolClassColorUsingStencil") == CompileStatus::Success;
}

VolClassCopyZTechnique::VolClassCopyZTechnique() : SingularTechnique(ShaderProgram()) {}
bool VolClassCopyZTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kVolClassCopyZFrag, "VolClassCopyZ") == CompileStatus::Success;
}

VolClassSetBlendTechnique::VolClassSetBlendTechnique() : SingularTechnique(ShaderProgram()) {}
bool VolClassSetBlendTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kVolClassSetBlendFrag, "VolClassSetBlend") == CompileStatus::Success;
}

VolClassBlendTechnique::VolClassBlendTechnique() : SingularTechnique(ShaderProgram()) {}
bool VolClassBlendTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kVolClassBlendFrag, "VolClassBlend") == CompileStatus::Success;
}

BlurTestOrderTechnique::BlurTestOrderTechnique() : SingularTechnique(ShaderProgram()) {}
bool BlurTestOrderTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kBlurTestOrderFrag, "BlurTestOrder") == CompileStatus::Success;
}

CombineTexturesTechnique::CombineTexturesTechnique() : SingularTechnique(ShaderProgram()) {}
bool CombineTexturesTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kCombineTexturesFrag, "CombineTextures") == CompileStatus::Success;
}

Combine3TexturesTechnique::Combine3TexturesTechnique() : SingularTechnique(ShaderProgram()) {}
bool Combine3TexturesTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kCombine3TexturesFrag, "Combine3Textures") == CompileStatus::Success;
}

EdlCalcFullTechnique::EdlCalcFullTechnique() : SingularTechnique(ShaderProgram()) {}
bool EdlCalcFullTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kEdlCalcFullFrag, "EdlCalcFull") == CompileStatus::Success;
}

EdlFilterTechnique::EdlFilterTechnique() : SingularTechnique(ShaderProgram()) {}
bool EdlFilterTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kEdlFilterFrag, "EdlFilter") == CompileStatus::Success;
}

EdlMixTechnique::EdlMixTechnique() : SingularTechnique(ShaderProgram()) {}
bool EdlMixTechnique::compileShaders(rhi::Driver& driver) {
    auto* p = getShader({}); if (!p || p->isValid()) return p && p->isValid();
    return p->compile(driver, kFullscreenQuadVert, kEdlMixFrag, "EdlMix") == CompileStatus::Success;
}

END_DQ_RENDER_NAMESPACE

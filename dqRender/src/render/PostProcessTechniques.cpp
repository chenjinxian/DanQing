// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Post-processing techniques implementation
// Ported from: filament filament/src/PostProcessManager.cpp
#include "PostProcessTechniques.h"
#include "ShaderProgramImpl.h"
#include "shader/OitShaders.h"
#include "shader/PostProcessShaders.h"

BEGIN_DQ_RENDER_NAMESPACE

OitClearTechnique::OitClearTechnique()
    : SingularTechnique(ShaderProgram())
{
}

bool OitClearTechnique::compileShaders(rhi::Driver& driver)
{
    auto* prog = getShader({});
    if (!prog || prog->isValid()) return prog && prog->isValid();
    return prog->compile(driver, kOitClearVert, kOitClearFrag, "OitClear") == CompileStatus::Success;
}

OitCompositeTechnique::OitCompositeTechnique()
    : SingularTechnique(ShaderProgram())
{
}

bool OitCompositeTechnique::compileShaders(rhi::Driver& driver)
{
    auto* prog = getShader({});
    if (!prog || prog->isValid()) return prog && prog->isValid();
    return prog->compile(driver, kOitCompositeVert, kOitCompositeFrag, "OitComposite") == CompileStatus::Success;
}

SsaoTechnique::SsaoTechnique()
    : SingularTechnique(ShaderProgram())
{
}

bool SsaoTechnique::compileShaders(rhi::Driver& driver)
{
    auto* prog = getShader({});
    if (!prog || prog->isValid()) return prog && prog->isValid();
    return prog->compile(driver, kFullscreenQuadVert, kSsaoFrag, "SSAO") == CompileStatus::Success;
}

BlurTechnique::BlurTechnique()
    : SingularTechnique(ShaderProgram())
{
}

bool BlurTechnique::compileShaders(rhi::Driver& driver)
{
    auto* prog = getShader({});
    if (!prog || prog->isValid()) return prog && prog->isValid();
    return prog->compile(driver, kFullscreenQuadVert, kBlurFrag, "Blur") == CompileStatus::Success;
}

EdlTechnique::EdlTechnique()
    : SingularTechnique(ShaderProgram())
{
}

bool EdlTechnique::compileShaders(rhi::Driver& driver)
{
    auto* prog = getShader({});
    if (!prog || prog->isValid()) return prog && prog->isValid();
    return prog->compile(driver, kFullscreenQuadVert, kEdlFrag, "EDL") == CompileStatus::Success;
}

CompositeTechnique::CompositeTechnique()
    : SingularTechnique(ShaderProgram())
{
}

bool CompositeTechnique::compileShaders(rhi::Driver& driver)
{
    auto* prog = getShader({});
    if (!prog || prog->isValid()) return prog && prog->isValid();
    return prog->compile(driver, kFullscreenQuadVert, kCompositeFrag, "Composite") == CompileStatus::Success;
}

END_DQ_RENDER_NAMESPACE

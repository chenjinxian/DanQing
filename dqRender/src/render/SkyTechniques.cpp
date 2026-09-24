// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Sky techniques implementation
// Ported from: filament filament/src/Skybox.cpp
#include "SkyTechniques.h"
#include "ShaderProgramImpl.h"
#include "shader/SkyShaders.h"

BEGIN_DQ_RENDER_NAMESPACE

SkyBoxTechnique::SkyBoxTechnique()
    : SingularTechnique(ShaderProgram())
{
}

bool SkyBoxTechnique::compileShaders(rhi::Driver& driver)
{
    auto* prog = getShader({});
    if (!prog || prog->isValid()) return prog && prog->isValid();
    return prog->compile(driver, kSkyBoxVert, kSkyBoxFrag, "SkyBox") == CompileStatus::Success;
}

SkySphereGradientTechnique::SkySphereGradientTechnique()
    : SingularTechnique(ShaderProgram())
{
}

bool SkySphereGradientTechnique::compileShaders(rhi::Driver& driver)
{
    auto* prog = getShader({});
    if (!prog || prog->isValid()) return prog && prog->isValid();
    return prog->compile(driver, kSkySphereGradientVert, kSkySphereGradientFrag, "SkySphereGradient") == CompileStatus::Success;
}

SkySphereTextureTechnique::SkySphereTextureTechnique()
    : SingularTechnique(ShaderProgram())
{
}

bool SkySphereTextureTechnique::compileShaders(rhi::Driver& driver)
{
    auto* prog = getShader({});
    if (!prog || prog->isValid()) return prog && prog->isValid();
    return prog->compile(driver, kSkySphereTextureVert, kSkySphereTextureFrag, "SkySphereTexture") == CompileStatus::Success;
}

END_DQ_RENDER_NAMESPACE

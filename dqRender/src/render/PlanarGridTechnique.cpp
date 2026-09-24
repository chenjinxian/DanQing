// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — PlanarGrid technique implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Technique.ts
#include "PlanarGridTechnique.h"
#include "ShaderProgramImpl.h"
#include "shader/PlanarGridShaders.h"

BEGIN_DQ_RENDER_NAMESPACE

PlanarGridTechnique::PlanarGridTechnique()
    : SingularTechnique(ShaderProgram())
{
}

bool PlanarGridTechnique::compileShaders(rhi::Driver& driver)
{
    // Get the shader program from the base class
    auto* prog = getShader({});
    if (!prog) return false;

    if (prog->isValid()) return true;  // Already compiled

    auto status = prog->compile(driver, kPlanarGridVert, kPlanarGridFrag, "PlanarGrid");
    return status == CompileStatus::Success;
}

END_DQ_RENDER_NAMESPACE

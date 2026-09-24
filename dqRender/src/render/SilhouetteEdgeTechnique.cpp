// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Silhouette edge technique implementation
// Ported from: itwinjs-core core/frontend/src/render/SilhouetteEdge.ts
#include "SilhouetteEdgeTechnique.h"
#include "ShaderProgramImpl.h"
#include "shader/SilhouetteEdgeShaders.h"

BEGIN_DQ_RENDER_NAMESPACE

SilhouetteEdgeTechnique::SilhouetteEdgeTechnique()
    : SingularTechnique(ShaderProgram())
{
}

bool SilhouetteEdgeTechnique::compileShaders(rhi::Driver& driver)
{
    auto* prog = getShader({});
    if (!prog) return false;

    if (prog->isValid()) return true;

    auto status = prog->compile(driver, kSilhouetteEdgeVert, kSilhouetteEdgeFrag, "SilhouetteEdge");
    return status == CompileStatus::Success;
}

END_DQ_RENDER_NAMESPACE

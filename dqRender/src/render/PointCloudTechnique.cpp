// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — point cloud technique implementation
// Ported from: itwinjs-core core/frontend/src/render/PointCloud.ts
#include "PointCloudTechnique.h"
#include "ShaderProgramImpl.h"
#include "shader/PointCloudShaders.h"

BEGIN_DQ_RENDER_NAMESPACE

PointCloudTechnique::PointCloudTechnique()
    : SingularTechnique(ShaderProgram())
{
}

bool PointCloudTechnique::compileShaders(rhi::Driver& driver)
{
    auto* prog = getShader({});
    if (!prog || prog->isValid()) return prog && prog->isValid();
    return prog->compile(driver, kPointCloudVert, kPointCloudFrag, "PointCloud") == CompileStatus::Success;
}

PointStringTechnique::PointStringTechnique()
    : SingularTechnique(ShaderProgram())
{
}

bool PointStringTechnique::compileShaders(rhi::Driver& driver)
{
    auto* prog = getShader({});
    if (!prog || prog->isValid()) return prog && prog->isValid();
    return prog->compile(driver, kPointStringVert, kPointStringFrag, "PointString") == CompileStatus::Success;
}

END_DQ_RENDER_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Common shader module (binding-carrying definitions)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Common.ts
//
// The addUniform bindings need ShaderProgram / TargetImpl, which are kept out
// of the lightweight CommonShaders.h header; the binding-carrying addXxx are
// defined here.
#include "CommonShaders.h"

#include "ShaderProgramImpl.h"  // ShaderProgram, addProgramUniform, ShaderProgramParams, UniformHandle
#include "TargetImpl.h"         // getFrustumUniforms()

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// u_frustum = target's frustum {near, far, type}; a ProgramUniform (re-bound
// only when the frustum changes). Null-guards getTarget() for the legacy/test
// path (callers without a TargetImpl).
// Ported from: itwinjs-core Common.ts addFrustum() (line 115-120)
void addFrustum(ProgramBuilder& builder)
{
    builder.addUniform("u_frustum", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addProgramUniform("u_frustum", [](UniformHandle& uniform, ShaderProgramParams const& params) {
            TargetImpl* target = params.getTarget();
            if (target)
                uniform.setUniform3fv(target->getFrustumUniforms().getFrustumData());
        });
    });

    builder.addGlobal("kFrustumType_Ortho2d", VariableType::Float, "0.0", true);
    builder.addGlobal("kFrustumType_Ortho3d", VariableType::Float, "1.0", true);
    builder.addGlobal("kFrustumType_Perspective", VariableType::Float, "2.0", true);
}

END_DQ_RENDER_NAMESPACE

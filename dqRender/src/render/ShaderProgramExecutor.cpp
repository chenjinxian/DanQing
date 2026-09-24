// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ShaderProgramExecutor implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ShaderProgram.ts
//              ShaderProgramExecutor (line 664-723)
#include "ShaderProgramExecutor.h"
#include "TargetImpl.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Constructor
// Ported from: itwinjs-core ShaderProgramExecutor constructor (line 668-671)
// ---------------------------------------------------------------------------
ShaderProgramExecutor::ShaderProgramExecutor(TargetImpl& target, RenderPass pass,
                                             ShaderProgram* program)
    : m_target(&target)
    , m_pass(pass)
{
    // In itwinjs-core, ShaderProgramParams.init(target, pass) stores the
    // target and render pass for uniform binding callbacks.
    // In DanQing, ShaderProgramParams is a simple uniform value map;
    // target and pass are stored directly on the executor.
    changeProgram(program);
}

// ---------------------------------------------------------------------------
// setProgram — change the current shader program
// Ported from: itwinjs-core ShaderProgramExecutor.setProgram() (line 692)
// ---------------------------------------------------------------------------
bool ShaderProgramExecutor::setProgram(ShaderProgram* program)
{
    return changeProgram(program);
}

// ---------------------------------------------------------------------------
// draw — execute a draw call with the current program
// Ported from: itwinjs-core ShaderProgramExecutor.draw() (line 703-708)
// ---------------------------------------------------------------------------
void ShaderProgramExecutor::draw(DrawParams& params)
{
    if (!m_program) return;
    m_program->draw(params);
}

// ---------------------------------------------------------------------------
// changeProgram — internal program switch
// Ported from: itwinjs-core ShaderProgramExecutor.changeProgram()
// ---------------------------------------------------------------------------
bool ShaderProgramExecutor::changeProgram(ShaderProgram* program)
{
    if (m_program == program)
        return false;

    m_program = program;
    return true;
}

END_DQ_RENDER_NAMESPACE

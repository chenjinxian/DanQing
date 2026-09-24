// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Scoped shader program executor
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ShaderProgram.ts
//              ShaderProgramExecutor (line 664-723)
//
// Manages the lifecycle of a ShaderProgram during command execution.
// Holds ShaderProgramParams (target + renderPass) and delegates draw calls
// to the current program.
#pragma once

#include "ShaderProgramImpl.h"
#include "DrawParams.h"
#include "gl/RenderFlags.h"

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class TargetImpl;

// ---------------------------------------------------------------------------
// ShaderProgramExecutor — scoped program management
// Ported from: itwinjs-core ShaderProgram.ts ShaderProgramExecutor (line 664-723)
// ---------------------------------------------------------------------------
class ShaderProgramExecutor {
public:
    ShaderProgramExecutor(TargetImpl& target, RenderPass pass,
                          ShaderProgram* program = nullptr);
    ~ShaderProgramExecutor() = default;

    ShaderProgramExecutor(ShaderProgramExecutor const&) = delete;
    ShaderProgramExecutor& operator=(ShaderProgramExecutor const&) = delete;

    /// Change the current program. Returns true if the program changed.
    bool setProgram(ShaderProgram* program);

    /// Whether a valid program is set.
    bool isValid() const { return m_program != nullptr; }

    /// Get the target.
    TargetImpl& getTarget() const { return *m_target; }

    /// Get the render pass.
    RenderPass getRenderPass() const { return m_pass; }

    /// Get the shader program params.
    ShaderProgramParams& getParams() { return m_params; }

    /// Draw with the current program.
    /// Ported from: itwinjs-core ShaderProgramExecutor.draw() (line 703-708)
    void draw(DrawParams& params);

private:
    bool changeProgram(ShaderProgram* program);

    TargetImpl* m_target;
    RenderPass m_pass;
    ShaderProgram* m_program = nullptr;
    ShaderProgramParams m_params;
};

END_DQ_RENDER_NAMESPACE

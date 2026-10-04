// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Render system debug control（shader 调试累积面）
// Ported from: itwinjs-core core/frontend/src/internal/render/RenderSystemDebugControl.ts
#include "RenderSystemDebugControl.h"

BEGIN_DQ_RENDER_NAMESPACE

bool isDebugShadersEnabled()
{
    static bool const s_enabled = std::getenv("DANQING_DEBUG_SHADERS") != nullptr;
    return s_enabled;
}

std::vector<DebugShaderFile>& debugShaderFilesRegistry()
{
    static std::vector<DebugShaderFile> s_files;
    return s_files;
}

END_DQ_RENDER_NAMESPACE

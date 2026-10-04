// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Render system debug control（shader 调试累积面）
// Ported from: itwinjs-core core/frontend/src/internal/render/RenderSystemDebugControl.ts
//
// M-O(2) 3d：DebugShaderFile 载体与 compileAllShaders/debugShaderFiles 面升到
// 公开 RenderSystem.h（宿主 OutputShadersTool 消费面）；本文件持：
//   - RenderDiagnostics / GLTimerResult（公开 RenderSystem.h 的同源件——
//     保留内部别名以稳既有 include 面）；
//   - debug-shaders 收集门与累积注册表（System.ts:317 debugShaderFiles +
//     options.debugShaders 的 env 等价）。
#pragma once

#include "dqRender/RenderSystem.h"  // DebugShaderFile / RenderSystemDebugControl

#include <cstdlib>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// RenderDiagnostics — bitmask controlling diagnostic output
// (Ported from: itwinjs-core RenderSystemDebugControl.ts RenderDiagnostics)
// ---------------------------------------------------------------------------
enum class RenderDiagnostics : uint8_t {
    None        = 0,
    DebugOutput = 1 << 1,
    GL          = 1 << 2,
    All         = DebugOutput | GL,
};

/// Bitwise OR for RenderDiagnostics.
inline RenderDiagnostics operator|(RenderDiagnostics a, RenderDiagnostics b) noexcept
{
    return static_cast<RenderDiagnostics>(
        static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

/// Bitwise AND for RenderDiagnostics.
inline RenderDiagnostics operator&(RenderDiagnostics a, RenderDiagnostics b) noexcept
{
    return static_cast<RenderDiagnostics>(
        static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

// ---------------------------------------------------------------------------
// debug-shaders 收集门（options.debugShaders 的 DanQing 等价——env
// DANQING_DEBUG_SHADERS，对齐参考 IMJS_DEBUG_SHADERS；进程首用快照）。
// ---------------------------------------------------------------------------
bool isDebugShadersEnabled();

// 累积注册表（System.instance.debugShaderFiles :317 等价——ShaderProgram
// ::compile 逐条追加、use 标 isUsed；RenderSystemDebugControl.debugShaderFiles
// 指向此处）。
std::vector<DebugShaderFile>& debugShaderFilesRegistry();

END_DQ_RENDER_NAMESPACE

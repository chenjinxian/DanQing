// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Render system debug control interface
// Ported from: itwinjs-core core/frontend/src/internal/render/RenderSystemDebugControl.ts
//
// Provides an interface for controlling render system diagnostic features:
// context loss simulation, diagnostic output, GPU timer profiling, and bulk
// shader compilation.
#pragma once

#include <cstdint>

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
// GLTimerResult — GPU timer profiling result
// (Ported from: itwinjs-core RenderSystemDebugControl.ts GLTimerResult)
//
// no-op queries seem to have ~32ns of noise on some backends.
// ---------------------------------------------------------------------------
struct GLTimerResult {
    /// Label from GLTimer.beginOperation.
    char const* label = nullptr;
    /// Time elapsed in nanoseconds, inclusive of child result times.
    uint64_t nanoseconds = 0;
};

/// Callback type for receiving GPU timer results.
using GLTimerResultCallback = void (*)(GLTimerResult const&);

// ---------------------------------------------------------------------------
// DebugShaderFile — metadata for a compiled shader
// (Ported from: itwinjs-core RenderSystemDebugControl.ts DebugShaderFile)
// ---------------------------------------------------------------------------
struct DebugShaderFile {
    char const* filename = nullptr;
    char const* src = nullptr;
    bool isVS = false;
    bool isGL = false;
    bool isUsed = false;
};

// ---------------------------------------------------------------------------
// IRenderSystemDebugControl — interface for render system debugging
// (Ported from: itwinjs-core RenderSystemDebugControl.ts RenderSystemDebugControl)
// ---------------------------------------------------------------------------
struct IRenderSystemDebugControl {
    virtual ~IRenderSystemDebugControl() = default;

    /// Destroy the graphics context.  Returns false if unsupported.
    virtual bool loseContext() = 0;

    /// Enable or disable diagnostic facilities.
    virtual void enableDiagnostics(RenderDiagnostics enable) = 0;

    /// Attempt to compile all shader programs.  Returns true if all succeed.
    virtual bool compileAllShaders() = 0;

    /// True if the backend supports GPU timer queries.
    virtual bool isGLTimerSupported() const = 0;

    /// Set the callback invoked with GPU timer results each frame.
    virtual void setResultsCallback(GLTimerResultCallback callback) = 0;
};

END_DQ_RENDER_NAMESPACE

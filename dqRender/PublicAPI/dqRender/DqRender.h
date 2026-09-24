// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Rendering engine entry point
//
// Provides namespace, version, and global initialization for the rendering
// engine.  The rendering pipeline is a faithful C++ port of itwinjs-core
// core/frontend (WebGL rendering logic) with Filament backend/ (RHI).
//
// Authored: dqRender module umbrella (DQ_RENDER_EXPORT macro, version, namespace).
// RenderSystem.ts / filament Platform.h are conceptual anchors, not 1:1 ports — the actual
// RenderSystem port lives in RenderSystem.h (audit F2).
#pragma once

#include "Export.h"  // DQ_RENDER_EXPORT

#include <cstdint>

// ---------------------------------------------------------------------------
// dqRender version
// ---------------------------------------------------------------------------
#define DQ_RENDER_VERSION_MAJOR 0
#define DQ_RENDER_VERSION_MINOR 1
#define DQ_RENDER_VERSION_PATCH 0
#define DQ_RENDER_VERSION_STRING "0.1.0"

// ---------------------------------------------------------------------------
// Namespace macros (following dqBase convention)
// ---------------------------------------------------------------------------
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Global initialization (once per process)
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT DqRenderLib {
public:
    /// Initialize the rendering engine.  Must be called once before any
    /// rendering operations.  Idempotent — safe to call multiple times.
    static void initialize() noexcept;

    /// Shut down the rendering engine.  Releases all GPU resources.
    static void shutdown() noexcept;
};

END_DQ_RENDER_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Render target debug control
// Ported from: itwinjs-core core/frontend/src/internal/render/RenderTargetDebugControl.ts
//
// Provides a plain struct with debug toggles that a RenderTarget can expose.
// Unlike the itwinjs interface, this is a concrete struct with default values
// so that targets can simply embed it without needing a separate implementation
// unless they override specific behaviour.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// PrimitiveVisibility — filter which primitives are drawn
// (Ported from: itwinjs-core RenderTargetDebugControl.ts PrimitiveVisibility)
// ---------------------------------------------------------------------------
enum class PrimitiveVisibility : uint8_t {
    All,          /// Draw all primitives.
    Instanced,    /// Only draw instanced primitives.
    Uninstanced,  /// Only draw un-instanced primitives.
};

// ---------------------------------------------------------------------------
// IRenderTargetDebugControl — debug toggles for a render target
// (Ported from: itwinjs-core RenderTargetDebugControl.ts RenderTargetDebugControl)
//
// Public members mirror the itwinjs interface properties.  A render target
// that implements this interface can selectively override behaviour.
// ---------------------------------------------------------------------------
struct IRenderTargetDebugControl {
    virtual ~IRenderTargetDebugControl() = default;

    /// If true, render as if performing off-screen readPixels().
    bool drawForReadPixels = false;

    /// Filter which primitive types are drawn.
    PrimitiveVisibility primitiveVisibility = PrimitiveVisibility::All;

    /// Volume classification supports intersecting volumes.
    bool vcSupportIntersectingVolumes = false;

    /// Display the drape frustum for debugging.
    bool displayDrapeFrustum = false;

    /// Display the mask frustum for debugging.
    bool displayMaskFrustum = false;

    /// override device pixel ratio (0.0 = use system default).
    float devicePixelRatioOverride = 0.0f;

    /// Display reality tile preload regions.
    bool displayRealityTilePreload = false;

    /// Display reality tile bounding ranges.
    bool displayRealityTileRanges = false;

    /// Log reality tile load/unload events.
    bool logRealityTiles = false;

    /// Display normal maps (default true per Target.ts :158 — the interface
    /// property has no default; the implementing Target sets `= true`).
    bool displayNormalMaps = true;

    /// Freeze reality tiles (stop loading new tiles).
    bool freezeRealityTiles = false;
};

END_DQ_RENDER_NAMESPACE

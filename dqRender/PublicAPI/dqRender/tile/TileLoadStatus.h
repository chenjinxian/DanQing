// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Tile load status
// Ported from: itwinjs-core core/frontend/src/tile/Tile.ts TileLoadStatus
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// TileLoadStatus — lifecycle state of a tile's content
// Ported from: itwinjs-core TileLoadStatus enum
// ---------------------------------------------------------------------------
enum class TileLoadStatus : uint8_t {
    NotLoaded = 0,   // No content, no request
    Queued = 1,      // Request queued, waiting for dispatch
    Loading = 2,     // Request dispatched, content loading
    Ready = 3,       // Content loaded, ready to display
    NotFound = 4,    // Content not available
    Abandoned = 5,   // Content disposed (tile expired or memory pressure)
};

// ---------------------------------------------------------------------------
// TileVisibility — result of frustum/LOD culling
// Ported from: itwinjs-core TileVisibility enum
// ---------------------------------------------------------------------------
enum class TileVisibility : uint8_t {
    OutsideFrustum = 0,  // Not visible (culled)
    TooCoarse = 1,       // Visible but needs refinement (load children)
    Visible = 2,         // Visible at appropriate LOD
};

END_DQ_RENDER_NAMESPACE

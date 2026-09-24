// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Tile load priority
// Ported from: itwinjs-core core/frontend/src/tile/TileTree.ts TileLoadPriority
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// TileLoadPriority — request ordering priority
// Ported from: itwinjs-core TileLoadPriority enum
// Lower values = higher priority (loaded first)
// ---------------------------------------------------------------------------
enum class TileLoadPriority : uint8_t {
    Dynamic = 5,    // Actively edited geometry (highest priority)
    Terrain = 10,   // Terrain tiles
    Map = 15,       // Map tiles
    Primary = 20,   // Primary model tiles (typical)
    Context = 40,   // Context/reality model tiles
    Classifier = 50, // Classifier tiles (lowest priority)
};

// ---------------------------------------------------------------------------
// TileTreeLoadStatus — lifecycle state of a tile tree
// Ported from: itwinjs-core TileTreeLoadStatus enum
// ---------------------------------------------------------------------------
enum class TileTreeLoadStatus : uint8_t {
    NotLoaded = 0,  // Tree not yet loaded
    Loading = 1,    // Tree loading in progress
    Loaded = 2,     // Tree loaded and ready
    NotFound = 3,   // Tree content not available
};

END_DQ_RENDER_NAMESPACE

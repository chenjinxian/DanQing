// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Map layer texture parameters
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/MapLayerParams.ts
//
// Tile data and comparison logic for background map layers.
// Determines when tiles need to be re-fetched based on view changes.
#pragma once

#include <cstdint>
#include <cstring>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// LayerTileData — ECEF transform and range for a map tile
// (Ported from: itwinjs-core MapLayerParams.ts LayerTileData)
// ---------------------------------------------------------------------------
struct LayerTileData {
    /// 4x4 ECEF transform (row-major).
    float ecefTransform[16];

    /// Axis-aligned bounding range: [minX, minY, minZ, maxX, maxY, maxZ].
    float range[6];

    /// Default constructor: identity transform, zero range.
    LayerTileData()
    {
        std::memset(ecefTransform, 0, sizeof(ecefTransform));
        ecefTransform[0] = 1.0f;
        ecefTransform[5] = 1.0f;
        ecefTransform[10] = 1.0f;
        ecefTransform[15] = 1.0f;
        std::memset(range, 0, sizeof(range));
    }
};

// ---------------------------------------------------------------------------
// MapLayerViewParams — view parameters for map layer comparison
// (Ported from: itwinjs-core MapLayerParams.ts MapLayerViewParams)
// ---------------------------------------------------------------------------
struct MapLayerViewParams {
    float eyeX = 0.0f;
    float eyeY = 0.0f;
    float eyeZ = 0.0f;
    float fovRadians = 0.78539816f;  // ~45 degrees
    float aspectRatio = 1.0f;
    float nearPlane = 0.1f;
    float farPlane = 100000.0f;
};

// ---------------------------------------------------------------------------
// CompareMapLayer — determine if view change requires tile refresh
// (Ported from: itwinjs-core MapLayerParams.ts compareMapLayer)
//
// Returns true if the view has changed enough to require new map tiles.
// Uses a threshold on eye position delta and FOV delta.
// ---------------------------------------------------------------------------
inline bool CompareMapLayer(MapLayerViewParams const& prev,
                             MapLayerViewParams const& curr)
{
    // Threshold constants (matching itwinjs thresholds).
    constexpr float kPositionThreshold = 1.0f;     // 1 meter
    constexpr float kFovThreshold = 0.01f;          // ~0.57 degrees

    float dx = curr.eyeX - prev.eyeX;
    float dy = curr.eyeY - prev.eyeY;
    float dz = curr.eyeZ - prev.eyeZ;
    float distSq = dx * dx + dy * dy + dz * dz;

    if (distSq > kPositionThreshold * kPositionThreshold) return true;

    float fovDelta = curr.fovRadians - prev.fovRadians;
    if (fovDelta < 0.0f) fovDelta = -fovDelta;
    if (fovDelta > kFovThreshold) return true;

    return false;
}

END_DQ_RENDER_NAMESPACE

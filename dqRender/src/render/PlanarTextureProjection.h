// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Planar texture projection
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/PlanarTextureProjection.ts
//
// Core algorithm for planar texture mapping: computes the frustum and
// projection matrix that maps a planar surface (terrain, mask layer) into
// texture space.  Used by BackgroundMapDrape and TerrainDrape for
// orthoimagery and mask overlay rendering.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// PlanarTextureProjection — planar texture mapping algorithm
// (Ported from: itwinjs-core PlanarTextureProjection.ts)
// ---------------------------------------------------------------------------
class PlanarTextureProjection {
public:
    /// Result of computing a planar texture projection.
    struct ProjectionResult {
        /// Frustum bounds in texture space.
        float left;
        float right;
        float bottom;
        float top;
        float near;
        float far;

        /// 4x4 projection matrix (row-major).
        float projectionMatrix[16];
    };

    /// Compute the projection for planar texture mapping.
    /// @param texturePlaneNormal  Unit normal of the projection plane (3 floats).
    /// @param viewRotation        3x3 view rotation matrix (9 floats, row-major).
    /// @param viewFrustumCorners  8 frustum corner positions (8 * 3 = 24 floats).
    /// @param textureWidth        Texture width in pixels.
    /// @param textureHeight       Texture height in pixels.
    /// @return Projection result with frustum bounds and projection matrix.
    static ProjectionResult computePlanarTextureProjection(
        float const* texturePlaneNormal,
        float const* viewRotation,
        float const* viewFrustumCorners,
        uint32_t textureWidth,
        uint32_t textureHeight);

    /// Check if a tile range overlaps a drape range.
    /// @param tileRange   6 floats: min xyz, max xyz of the tile.
    /// @param drapeRange  6 floats: min xyz, max xyz of the drape region.
    /// @return true if the two ranges overlap.
    static bool isTileRangeInBounds(float const* tileRange, float const* drapeRange);
};

END_DQ_RENDER_NAMESPACE

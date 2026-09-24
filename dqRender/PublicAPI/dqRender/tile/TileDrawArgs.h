// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Tile draw arguments
// Ported from: itwinjs-core core/frontend/src/tile/TileDrawArgs.ts
#pragma once

#include <dqCommon/FrustumPlanes.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class Tile;
class RenderGraphic;

// ---------------------------------------------------------------------------
// TileDrawArgs — context for tile selection and drawing
// Ported from: itwinjs-core TileDrawArgs class
// ---------------------------------------------------------------------------
struct TileDrawArgs {
    /// Camera position in world space (for distance computation)
    float eyePos[3] = {0.0f, 0.0f, 0.0f};

    /// View frustum planes (world space) for culling. Ported from:
    /// TileDrawArgs.frustumPlanes (TileDrawArgs.ts:93-96 — built once per
    /// scene from the viewport's world frustum via FrustumPlanes.fromFrustum).
    /// An invalid (empty) set culls nothing.
    dqCommon::FrustumPlanes frustumPlanes;

    /// World-units-per-pixel (frustum height / viewport pixel height) — the
    /// reference's getPixelSize (TileDrawArgs.ts:138), the scale factor for
    /// screen-space-error LOD tests (SSE = geometricError / pixelSize,
    /// RealityTile.ts:535-542).
    float pixelSizeRatio = 1.0f;

    /// Transform from tree space to world space
    dqGeom::Transform treeToWorld;

    /// Perspective pixel-size inputs (camera on). SSE uses the meters-per-
    /// pixel at the point of the tile's bounding sphere closest to the eye:
    /// pixelSize = dist(closest point) * perspectiveScale, where
    /// perspectiveScale = 2*tan(lensAngle/2)/viewportHeightPx.
    /// Ported from: TileDrawArgs.computePixelSizeInMetersAtClosestPoint
    /// (TileDrawArgs.ts:190-206 — the worldToViewMap round-trip collapsed to
    /// the standard pinhole formula; the near-front-plane clamp keeps tiles
    /// overlapping the near plane finite, matching the reference's guard).
    bool cameraOn = false;
    float cameraEye[3] = {0.0f, 0.0f, 0.0f};
    float perspectiveScale = 0.0f;  // 2*tan(lens/2)/heightPx

    /// Tiles that need content (collected during selection)
    std::vector<Tile*> requestedTiles;

    /// Tiles that are ready to display (collected during selection)
    std::vector<Tile*> readyTiles;

    /// Graphics to render (collected during draw)
    std::vector<RenderGraphic*> graphics;

    /// Maximum screen-space error (pixels) above which a tile refines into
    /// its children. Ported from: itwinjs-core TileDrawArgs
    /// maximumScreenSpaceError default (TileDrawArgs.ts:279 — "Cesium's
    /// default" 16).
    static constexpr float kMaximumScreenSpaceError = 16.0f;

    /// Get the pixel size ratio (world units per pixel).
    float getPixelSizeRatio() const noexcept { return pixelSizeRatio; }

    /// Compute the screen-space size of a tile (approximate).
    /// Ported from: itwinjs-core TileDrawArgs computeScreenSize
    float computeScreenSize(Tile const& tile) const;
};

END_DQ_RENDER_NAMESPACE

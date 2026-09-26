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
    /// orthographic input to computePixelSizeInMetersAtClosestPoint (the
    /// reference derives it from the worldToViewMap round-trip,
    /// TileDrawArgs.ts:208-210; the uniform orthographic segment collapses to
    /// this ratio). Also the scale factor for the RealityTile SSE test.
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

    /// True if a tile and its child tiles should not be drawn simultaneously.
    /// The reference value comes from TileTree.parentsAndChildrenExclusive
    /// (TileTree.ts:110-113 — always true for iModel trees; reality/map trees
    /// override it from their loader, RealityTileTree.ts:246).
    /// Ported from: TileDrawArgs.parentsAndChildrenExclusive
    /// (TileDrawArgs.ts:48/:105/:302).
    bool parentsAndChildrenExclusive = true;

    /// A multiplier applied to a Tile's maximumSize property to adjust level
    /// of detail (viewport.tileSizeModifier, Viewport.ts:2971-2973).
    /// Ported from: TileDrawArgs.tileSizeModifier (TileDrawArgs.ts:313 —
    /// DanQing stores the value as a field; there is no viewport handle here).
    /// EQUIVALENCE: 参考源=Viewport.ts:2971-2973（缺省 =
    /// TileAdmin.defaultTileSizeModifier）；发散=DanQing 尚未接线该 admin 面，
    /// 字段缺省 1.0（不改行为）；验证法=Task 3 SSE 像素锁 + Imdl 回归。
    float tileSizeModifier = 1.0f;

    /// Scale factor applied to the computed pixel size (adjusts for model
    /// display transforms with non-uniform scaling,
    /// computePixelSizeScaleFactor, TileDrawArgs.ts:240-265/:306 — returns 1
    /// unless such a transform exists).
    /// Ported from: TileDrawArgs.pixelSizeScaleFactor (TileDrawArgs.ts:127).
    /// EQUIVALENCE: 参考源=TileDrawArgs.ts:240-265（无非均匀缩放的 model
    /// display transform 时恒 1）；发散=DanQing 无 model display transform 面，
    /// 字段恒 1.0；验证法=Task 3 SSE 像素锁。
    float pixelSizeScaleFactor = 1.0f;

    /// Graphics to render (collected during draw)
    std::vector<RenderGraphic*> graphics;

    /// Maximum screen-space error (pixels) above which a tile refines into
    /// its children. Ported from: itwinjs-core TileDrawArgs
    /// maximumScreenSpaceError default (TileDrawArgs.ts:279 — "Cesium's
    /// default" 16).
    static constexpr float kMaximumScreenSpaceError = 16.0f;

    /// Get the pixel size ratio (world units per pixel).
    float getPixelSizeRatio() const noexcept { return pixelSizeRatio; }

    // --- Selection marker faces ---
    // Ported from: TileDrawArgs.ts insertMissing/markChildrenLoading/markUsed/
    // markReady (:402-421). The reference's missing/ready sets live on the
    // SceneContext/TileDrawArgs as JS Sets; DanQing hosts them here as
    // deduplicated vectors (linear dedup — per-frame sizes are small).

    /// Indicate that graphics for the specified tile are desired but not yet
    /// available. Subsequently a request will be enqueued to load the tile's
    /// graphics. Deduped (the reference Set semantics). The load-status gate
    /// (NotLoaded/Queued/Loading) is NOT applied here — the reference applies
    /// it in SceneContext.insertMissingTile (ViewContext.ts:421-429), which
    /// DanQing mirrors in dqApp's SceneContext.
    /// Ported from: TileDrawArgs.insertMissing (TileDrawArgs.ts:402-404).
    void insertMissing(Tile* tile);

    /// Indicate that some requested child tiles are not yet loaded.
    /// Ported from: TileDrawArgs.markChildrenLoading (TileDrawArgs.ts:407-409
    /// → SceneContext._missingChildTiles, ViewContext.ts:381-383).
    void markChildrenLoading() noexcept { m_missingChildTiles = true; }

    /// Indicate that the specified tile is being used for some purpose by the
    /// viewport (typically "displayed"). Marks the tile's usage timestamp and
    /// records it in the touched set ("keep in memory" — fed to the LRU via
    /// TileAdmin.addTilesForUser, TileAdmin.ts:519-520).
    /// Ported from: TileDrawArgs.markUsed (TileDrawArgs.ts:412-414 —
    /// tile.usageMarker.mark(viewport, now); DanQing's usage-marker face is
    /// the per-tile timestamp, Tile.h:82-87).
    void markUsed(Tile* tile);

    /// Indicate that the specified tile should be displayed and that its
    /// graphics are ready to be displayed.
    /// Ported from: TileDrawArgs.markReady (TileDrawArgs.ts:419-421 — ready
    /// set add only; the LRU "used" marking of ready tiles happens in
    /// TileAdmin.addTilesForUser, TileAdmin.ts:517-518).
    void markReady(Tile* tile);

    /// SceneContext.hasMissingTiles semantics: the children-loading flag OR a
    /// non-empty missing set.
    /// Ported from: SceneContext.hasMissingTiles (ViewContext.ts:386-388).
    bool hasMissingTiles() const noexcept
    {
        return m_missingChildTiles || !m_missingTiles.empty();
    }

    /// The missing set (deduped, insertion-ordered — the reference Set).
    std::vector<Tile*> const& getMissingTiles() const noexcept
    {
        return m_missingTiles;
    }

    /// The ready set (deduped, insertion-ordered — the reference Set).
    std::vector<Tile*> const& getReadyTiles() const noexcept
    {
        return m_readyTiles;
    }

    /// The touched set ("used for some purpose"; keep-in-memory marker).
    std::vector<Tile*> const& getTouchedTiles() const noexcept
    {
        return m_touchedTiles;
    }

    /// Ready-set membership (reference expression:
    /// `args.readyTiles.has(tile)`, TileDrawArgs.ts:111).
    /// Authored: accessor prescribed by the task brief; no named reference
    /// counterpart.
    bool isTileReady(Tile const* tile) const noexcept;

    /// Authored: accessor for the children-loading flag (reference composes it
    /// into SceneContext.hasMissingTiles, ViewContext.ts:386-388).
    bool areChildrenLoading() const noexcept { return m_missingChildTiles; }

    // --- Pixel-size faces ---

    /// Compute the size in pixels of the specified tile at the point on its
    /// bounding sphere closest to the camera.
    /// Ported from: TileDrawArgs.getPixelSize (TileDrawArgs.ts:138-148 —
    /// sphere path; the OBB corner-projection path is not ported, see
    /// TileDrawArgs.cpp for the EQUIVALENCE registration).
    double getPixelSize(Tile const& tile) const;

    /// The point at the center of the tile's volume, in world space.
    /// Ported from: TileDrawArgs.getTileCenter (TileDrawArgs.ts:316 —
    /// location.multiplyPoint3d(tile.center); Tile.center =
    /// boundingSphere.center, Tile.ts:96-97).
    dqGeom::Point3d getTileCenter(Tile const& tile) const;

    /// Half the diagonal of the tile's (location-transformed) range.
    /// Ported from: TileDrawArgs.getTileRadius (TileDrawArgs.ts:319-328).
    double getTileRadius(Tile const& tile) const;

    /// Compute the size in meters of one pixel at the point on a sphere
    /// closest to the camera.
    /// Ported from: TileDrawArgs.computePixelSizeInMetersAtClosestPoint
    /// (TileDrawArgs.ts:190-211 — the worldToViewMap transform0/transform1
    /// round-trip collapsed to the standard pinhole formula; the clamp keeps
    /// tiles overlapping the near plane finite, matching the reference's
    /// near-front-plane guard at :194-196). EQUIVALENCE registration at the
    /// definition (TileDrawArgs.cpp).
    double computePixelSizeInMetersAtClosestPoint(dqGeom::Point3d const& center,
                                                  double radius) const;

    /// Compute the screen-space size of a tile (approximate).
    /// Ported from: itwinjs-core TileDrawArgs computeScreenSize
    float computeScreenSize(Tile const& tile) const;

private:
    /// Tiles desired for the scene but not yet ready (deduped).
    /// Ported from: SceneContext.missingTiles (ViewContext.ts:378 — Set).
    std::vector<Tile*> m_missingTiles;

    /// Tiles that we want to draw and that are ready to draw (deduped).
    /// Ported from: TileDrawArgs.readyTiles (TileDrawArgs.ts:111 — Set).
    std::vector<Tile*> m_readyTiles;

    /// Tiles whose contents should be kept in memory regardless of whether
    /// they are selected for display (deduped).
    /// Ported from: TileDrawArgs.touchedTiles (TileDrawArgs.ts:115 — Set).
    std::vector<Tile*> m_touchedTiles;

    /// "Some requested child tiles are not yet loaded".
    /// Ported from: SceneContext._missingChildTiles (ViewContext.ts:373).
    bool m_missingChildTiles = false;
};

END_DQ_RENDER_NAMESPACE

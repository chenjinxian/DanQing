// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TileDrawArgs implementation
// Ported from: itwinjs-core core/frontend/src/tile/TileDrawArgs.ts
#include "dqRender/tile/TileDrawArgs.h"

#include "dqRender/tile/TileAdmin.h"  // markUsed's clock (nowSeconds)

#include <algorithm>
#include <cmath>

BEGIN_DQ_RENDER_NAMESPACE

namespace {
// Minimum world-space distance used where the reference takes the near-front
// center point (TileDrawArgs.ts:194-196 — "If the sphere overlaps the near
// front plane just use near front point. This also handles behind eye
// conditions."). The DanQing pinhole collapse clamps the closest-point
// distance with this constant instead (kept from the pre-existing adaptation
// in RealityTileTree.cpp computeVisibility, 2026-09-21): the resulting
// meters-per-pixel stays finite for near/behind-eye spheres.
constexpr double kMinimumClosestPointDistance = 0.01;
}  // namespace

// ---------------------------------------------------------------------------
// Selection marker faces
// ---------------------------------------------------------------------------

void TileDrawArgs::insertMissing(Tile* tile)
{
    // Ported from: TileDrawArgs.insertMissing (TileDrawArgs.ts:402-404 →
    // SceneContext.insertMissingTile → missingTiles Set, ViewContext.ts:421-429).
    // The reference Set dedups; DanQing hosts the set as a vector with linear
    // dedup (per-frame sizes are small). The NotLoaded/Queued/Loading gate
    // stays where the reference has it — SceneContext.insertMissingTile
    // (dqApp's SceneContext mirrors it); this container stores as reported.
    if (tile && std::find(m_missingTiles.begin(), m_missingTiles.end(), tile) ==
                    m_missingTiles.end())
        m_missingTiles.push_back(tile);
}

void TileDrawArgs::markUsed(Tile* tile)
{
    // Ported from: TileDrawArgs.markUsed (TileDrawArgs.ts:412-414) —
    // tile.usageMarker.mark(viewport, now) ONLY. DanQing's usage-marker face
    // is the per-tile timestamp (Tile.h:82-87); the per-user "in use" half
    // lives in TileAdmin's LRU selection sets. The reference's touchedTiles
    // set is a separate tree-authored keep-alive collection (sole writer:
    // BatchedTile.ts:83) — markUsed never writes it.
    if (!tile)
        return;
    tile->markUsed(TileAdmin::nowSeconds());
}

void TileDrawArgs::markReady(Tile* tile)
{
    // Ported from: TileDrawArgs.markReady (TileDrawArgs.ts:419-421) —
    // readyTiles.add(tile) only. The reference does NOT mark usage here; the
    // LRU "used" marking of ready tiles happens in TileAdmin.addTilesForUser
    // (TileAdmin.ts:517-518). (The task brief's "ready 集 + markUsed" note is
    // not what the reference source does — §0: the reference is the spec.)
    if (tile && std::find(m_readyTiles.begin(), m_readyTiles.end(), tile) ==
                    m_readyTiles.end())
        m_readyTiles.push_back(tile);
}

bool TileDrawArgs::isTileReady(Tile const* tile) const noexcept
{
    // Ready-set membership — reference expression `readyTiles.has(tile)`
    // (TileDrawArgs.ts:111). Authored accessor (brief-prescribed).
    return std::find(m_readyTiles.begin(), m_readyTiles.end(), tile) !=
           m_readyTiles.end();
}

// ---------------------------------------------------------------------------
// Pixel-size faces
// ---------------------------------------------------------------------------

dqGeom::Point3d TileDrawArgs::getTileCenter(Tile const& tile) const
{
    // Ported from: TileDrawArgs.getTileCenter (TileDrawArgs.ts:316) —
    // location.multiplyPoint3d(tile.center). Tile.center is
    // boundingSphere.center (Tile.ts:96-97) — DanQing's Tile computes it from
    // the range center in its constructor (Tile.cpp:19-22).
    dqGeom::Point3d const center(
        tile.getBoundingSphere().center[0], tile.getBoundingSphere().center[1],
        tile.getBoundingSphere().center[2]);
    return treeToWorld.MultiplyPoint3d(center);
}

double TileDrawArgs::getTileRadius(Tile const& tile) const
{
    // Ported from: TileDrawArgs.getTileRadius (TileDrawArgs.ts:319-328) —
    // 0.5 * |range.high - range.low| after the location transform.
    // EQUIVALENCE: 参考源=TileDrawArgs.ts:321-324（tile.tree.is2d 时把 z 塌缩
    // 到 0，防 2d 树在 3d 视图里 z 撑大半径）；发散=DanQing TileTree 尚无 is2d
    // 标志，跳过塌缩（当前全部 3d 树，行为一致）；验证法=球路径数值锁
    //（TileDrawArgsTest.GetPixelSizeSpherePath），2d 树接入时补塌缩。
    dqGeom::Range3d const range = treeToWorld.MultiplyRange(tile.getRange());
    return 0.5 * range.low.Distance(range.high);
}

double TileDrawArgs::computePixelSizeInMetersAtClosestPoint(
    dqGeom::Point3d const& center, double radius) const
{
    // Ported from: TileDrawArgs.computePixelSizeInMetersAtClosestPoint
    // (TileDrawArgs.ts:190-211).
    // EQUIVALENCE: 参考源=TileDrawArgs.ts:208-210（worldToViewMap
    // transform0/transform1 回程：view 点与其 +1px 邻点的世界距）；
    // 发散=DanQing 无 worldToViewMap，按针孔模型折算——正交段均匀比例 =
    // pixelSizeRatio；透视段 = 最近点距 × perspectiveScale（2·tan(lens/2)/
    // 视口像素高）。
    // 另一处发散（非数值等价，仅保证有限值）：参考 :194-196 在球与近平面
    // 重叠/眼后时把求值点替换为近平面前中心（metersPerPixel 在【近平面距】
    // 求值，通常 >> 0.01 → pixelSize 偏小 → 少细化）；DanQing 把最近点距
    // 夹到 kMinimumClosestPointDistance=0.01（metersPerPixel 极小 →
    // pixelSize 极大 → 恒细化）。这是保留的 2026-09-21 预存适配（本次迁移
    // 行为未变），跨近平面 tile 两者数值发散——非等价主张。
    // 验证法=TileDrawArgsTest.GetPixelSizeSpherePath 三组数值锁（第三组钉住
    // 当前夹紧常数）+ TileTreeRender 像素锁（SSE 判定行为不变）。
    if (cameraOn && perspectiveScale > 0.0f) {
        // Point on the bounding sphere closest to the eye (:198-204).
        double const dx = static_cast<double>(center.x) - cameraEye[0];
        double const dy = static_cast<double>(center.y) - cameraEye[1];
        double const dz = static_cast<double>(center.z) - cameraEye[2];
        double const dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        double const closest = std::max(dist - radius, kMinimumClosestPointDistance);
        return static_cast<double>(perspectiveScale) * closest;
    }
    return static_cast<double>(pixelSizeRatio);
}

double TileDrawArgs::getPixelSize(Tile const& tile) const
{
    // Ported from: TileDrawArgs.getPixelSize (TileDrawArgs.ts:138-148) —
    // sphere path (radius / metersPerPixelAtClosestPoint, 1.0e-3 fallback).
    // EQUIVALENCE: 参考源=TileDrawArgs.ts:151-176（OBB 角点投影路径
    // getPixelSizeFromProjection）；发散=DanQing Tile 仅含包围球
    //（BoundingSphere，Tile.h:32-35），无 OBB 表示——路径不可达，直接走球路径；
    // 验证法=TileDrawArgsTest.GetPixelSizeSpherePath 两组数值锁 + 像素锁。
    // 另参考 :147 的 context.adjustPixelSizeForLOD（RenderTarget.ts:67-68）在
    // RenderSystem.dpiAwareLOD 关闭（DanQing 无该选项）时为恒等——省略。
    double const radius = getTileRadius(tile);
    double const pixelSizeAtPt =
        computePixelSizeInMetersAtClosestPoint(getTileCenter(tile), radius);
    return 0.0 != pixelSizeAtPt ? radius / pixelSizeAtPt : 1.0e-3;
}

// ---------------------------------------------------------------------------
// TileDrawArgs::computeScreenSize — approximate screen-space size of a tile
// Ported from: itwinjs-core TileDrawArgs.computeScreenSize()
// (moved here from RealityTileTree.cpp — the TileDrawArgs method implementations
// live in the file matching the reference source, 2026-09-26)
// ---------------------------------------------------------------------------
float TileDrawArgs::computeScreenSize(Tile const& tile) const
{
    // Simple distance-based approximation:
    // screenSize = boundingSphere.diameter * pixelSizeRatio / distance
    auto const& bs = tile.getBoundingSphere();
    float dx = bs.center[0] - eyePos[0];
    float dy = bs.center[1] - eyePos[1];
    float dz = bs.center[2] - eyePos[2];
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (dist < 1e-6f)
        return 1e6f;  // Very close — very large screen size.

    return (bs.radius * 2.0f * pixelSizeRatio) / dist;
}

END_DQ_RENDER_NAMESPACE

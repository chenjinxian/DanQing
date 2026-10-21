// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewingSpace (transform chain: World → Npc → View)
// Ported from: itwinjs-core core/frontend/src/ViewingSpace.ts
//
// Holds two Map4d (worldToNpcMap, worldToViewMap) rebuilt on every frustum
// change via update(). NPC is corner-anchored [0,1]^3 (the view-volume box);
// View is viewport pixels with Y flipped and z in [-32767, 32767]
// (getViewCorners). The renderer's GL NDC [-1,1] matrix is derived via
// GetWorldToNdcMatrix (NdcFromNpc × worldToNpcMap.transform0) — Decision 1:
// the itwinjs shaders perform the NPC→NDC conversion in GLSL; DanQing hoists it
// into the matrix so the existing u_mvp contract (NDC clip space) is unchanged.
#pragma once

#include "Export.h"

#include <dqCommon/CoordSystem.h>
#include <dqCommon/Frustum.h>
#include <dqGeom/Map4d.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Matrix4d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace dqApp {

class ViewState3d;

// ---------------------------------------------------------------------------
// ViewRect — viewport rectangle in pixels
// Ported from: itwinjs-core core/frontend/src/common/ViewRect.ts
// 参考字段为"非负整数"语义（ViewRect.ts:10-14——_set 每赋值 max(0,floor)），
// DanQing 以 int32 承载。
// ---------------------------------------------------------------------------
struct ViewRect {
    int32_t left = 0;
    int32_t top = 0;
    int32_t right = 0;
    int32_t bottom = 0;

    int32_t width() const { return right - left; }
    int32_t height() const { return bottom - top; }

    // Ported from: itwinjs-core ViewRect.isNull (ViewRect.ts:43)。
    bool isNull() const { return right <= left || bottom <= top; }
    // Ported from: itwinjs-core ViewRect.aspect (ViewRect.ts:53 — isNull → 1.0)。
    double aspect() const
    {
        return isNull() ? 1.0 : static_cast<double>(width()) / static_cast<double>(height());
    }

    // Ported from: itwinjs-core ViewRect.init (ViewRect.ts:57-61)——每字段经
    // _set（ViewRect.ts:24-26）= max(0, floor)。
    void init(double left_, double top_, double right_, double bottom_)
    {
        left   = std::max(0, static_cast<int32_t>(std::floor(left_)));
        top    = std::max(0, static_cast<int32_t>(std::floor(top_)));
        right  = std::max(0, static_cast<int32_t>(std::floor(right_)));
        bottom = std::max(0, static_cast<int32_t>(std::floor(bottom_)));
    }

    // Ported from: itwinjs-core ViewRect.setFrom (ViewRect.ts:77)。
    void setFrom(ViewRect const& other) { init(other.left, other.top, other.right, other.bottom); }

    // Ported from: itwinjs-core ViewRect.inset (ViewRect.ts:102-113)——delta 取
    // floor；塌缩（宽/高不足 2×delta）→ init(0,0,0,0)；否则直接改成员（不经
    // _set——负 delta 扩张可使边界为负，参考行为原样保留）。
    void inset(double deltaX, double deltaY)
    {
        int32_t const dx = static_cast<int32_t>(std::floor(deltaX));
        int32_t const dy = static_cast<int32_t>(std::floor(deltaY));
        if (width() - 2 * dx <= 0 || height() - 2 * dy <= 0) {
            init(0, 0, 0, 0);
            return;
        }
        left += dx;
        right -= dx;
        top += dy;
        bottom -= dy;
    }

    // Ported from: itwinjs-core ViewRect.scaleAboutCenter (ViewRect.ts:123-130)。
    void scaleAboutCenter(double xScale, double yScale)
    {
        double const w = static_cast<double>(width());
        double const h = static_cast<double>(height());
        double const xDelta = (w - (w * xScale)) * 0.5;
        double const yDelta = (h - (h * yScale)) * 0.5;
        inset(xDelta, yDelta);
    }

    // Ported from: itwinjs-core ViewRect.containsPoint (ViewRect.ts:145)——左/上
    // 边界含，右/下边界不含。
    bool containsPoint(dqGeom::Point2d const& point) const
    {
        return point.x >= static_cast<double>(left) && point.x < static_cast<double>(right)
            && point.y >= static_cast<double>(top) && point.y < static_cast<double>(bottom);
    }
    // XAndY 坐标同义的 Point3d 形（Marker.setPosition 以 Point3d 位置调
    // vp.viewRect.containsPoint——Marker.ts:259）。
    bool containsPoint(dqGeom::Point3d const& point) const
    {
        return point.x >= static_cast<double>(left) && point.x < static_cast<double>(right)
            && point.y >= static_cast<double>(top) && point.y < static_cast<double>(bottom);
    }
};

// ---------------------------------------------------------------------------
// ViewingSpace — transform chain for World ↔ Npc ↔ View coordinate conversion.
// Ported from: itwinjs-core ViewingSpace (ViewingSpace.ts).
//
// Stores the worldToNpc + worldToView Map4d (each carries its own inverse as
// transform1). Rebuilt whenever the view frustum changes (via update()).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewingSpace {
public:
    ViewingSpace() = default;

    /// Rebuild the transform chain from the current view state + viewport rect.
    /// Ported from: itwinjs-core ViewingSpace constructor (ViewingSpace.ts:271-372)
    void update(ViewState3d const& view, ViewRect const& viewRect);

    // --- Coordinate transforms (forwarders to the maps' transform0/1) ---
    // Ported from: itwinjs-core ViewingSpace.worldToView/viewToWorld/worldToNpc/npcToWorld
    //              (ViewingSpace.ts:427-457).

    /// world → view pixels (worldToViewMap.transform0).
    dqGeom::Point3d WorldToView(dqGeom::Point3d const& pt) const;
    /// view pixels → world (worldToViewMap.transform1).
    dqGeom::Point3d ViewToWorld(dqGeom::Point3d const& pt) const;
    /// world → NPC[0,1] (worldToNpcMap.transform0).
    dqGeom::Point3d WorldToNpc(dqGeom::Point3d const& pt) const;
    /// NPC[0,1] → world (worldToNpcMap.transform1).
    dqGeom::Point3d NpcToWorld(dqGeom::Point3d const& pt) const;

    /// NPC → view pixels. Ported from: itwinjs-core ViewingSpace.npcToView
    /// (ViewingSpace.ts:404-407, getViewCorners().fractionToPoint).
    dqGeom::Point3d NpcToView(dqGeom::Point3d const& npc) const;
    /// view pixels → NPC. Ported from: itwinjs-core ViewingSpace.viewToNpc
    /// (ViewingSpace.ts:393-398, Transform.initFromRange(getViewCorners())).
    dqGeom::Point3d ViewToNpc(dqGeom::Point3d const& view) const;

    // --- Map access (faithful) ---
    dqGeom::Map4d const& worldToNpcMap() const { return m_worldToNpcMap; }
    dqGeom::Map4d const& worldToViewMap() const { return m_worldToViewMap; }

    // --- Frustum + pixel metrics ---
    /// Ported from: itwinjs-core ViewingSpace.getFrustum (ViewingSpace.ts:471-503).
    /// `sys`=World → npcToWorldArray; View → npcToViewArray. `adjustedBox` is
    /// accepted for signature parity; DanQing does not expand z clip planes (no
    /// viewed-extents/tile-tree subsystem), so the adjusted == unexpanded box.
    void getFrustum(dqCommon::Frustum& out, dqCommon::CoordSystem sys,
                    bool adjustedBox = true) const;
    /// Ported from: itwinjs-core ViewingSpace.getPixelSizeAtPoint
    /// (ViewingSpace.ts:505-510): world-distance spanned by one view-pixel at
    /// `inPoint` (default = NPC center).
    double getPixelSizeAtPoint(dqGeom::Point3d const* inPoint = nullptr) const;

    // --- Renderer matrix access (float[16], column-major) ---
    /// world → GL NDC[-1,1]. Decision 1: NdcFromNpc × worldToNpcMap.transform0
    /// (NPC[0,1]→NDC[-1,1]). Same math itwinjs does in-shader, hoisted here.
    std::array<float, 16> const& GetWorldToNdcMatrix() const { return m_worldToNdc; }
    /// Affine world → view-local (rotation + −origin). DanQing renderer adapter
    /// for the u_mv uniform (view-space lighting/sky); not part of the itwinjs
    /// Map4d pipeline (itwinjs uploads worldToViewMap to shaders instead).
    std::array<float, 16> const& GetViewMatrix() const { return m_viewMatrix; }

    // --- View/camera state ---
    dqGeom::Point3d const& getEyePoint() const { return m_eyePoint; }
    bool IsPerspective() const { return m_perspective; }
    double getFrustFraction() const { return m_frustFraction; }
    // Ported from: itwinjs-core ViewingSpace.viewDelta (ViewingSpace.ts:45 —
    //              public readonly 字段；可能是 adjustZPlanes 调整后的值)。
    dqGeom::Vector3d const& viewDelta() const { return m_viewDelta; }
    // True if adjustZPlanes moved the front/back clip planes to encompass the grid
    // plane (or viewed extents). Ported from: itwinjs-core ViewingSpace.zClipAdjusted.
    bool zClipAdjusted() const noexcept { return m_zClipAdjusted; }

private:
    // Ported from: itwinjs-core ViewingSpace.calcNpcToView (ViewingSpace.ts:250-256).
    dqGeom::Map4d calcNpcToView() const;
    // Ported from: itwinjs-core ViewingSpace.getViewCorners (ViewingSpace.ts:258-269).
    dqGeom::Range3d getViewCorners() const;
    // Affine world → view-local matrix (u_mv adapter). Fills m_viewMatrix from
    // the view's rotation + origin (column-major float[16]).
    void buildAffineViewMatrix(ViewState3d const& view);
    // Ported from: itwinjs-core ViewingSpace.adjustZPlanes (ViewingSpace.ts:147-245).
    // Mutates origin/delta.z to encompass the grid plane (and, when ported, viewed
    // extents). Reads m_rotation / m_viewOrigin / m_viewDelta / m_eyePoint (set before call).
    void adjustZPlanes(dqGeom::Point3d& origin, dqGeom::Vector3d& delta,
                       ViewState3d const& view);
    // Encode a row-major Matrix4d as column-major float[16] for renderer upload.
    static std::array<float, 16> ToColumnMajorFloat(dqGeom::Matrix4d const& m);

    // The two faithful maps (each carries its inverse as transform1).
    dqGeom::Map4d m_worldToNpcMap{dqGeom::Map4d::CreateIdentity()};
    dqGeom::Map4d m_worldToViewMap{dqGeom::Map4d::CreateIdentity()};
    // Derived renderer matrices (column-major float[16]).
    std::array<float, 16> m_worldToNdc{};
    std::array<float, 16> m_viewMatrix{};
    // Viewport pixel rect (getViewCorners high.x = right, low.y = bottom).
    float m_viewW = 1.0f;
    float m_viewH = 1.0f;
    double m_frustFraction = 1.0;
    dqGeom::Point3d m_eyePoint;
    bool m_perspective = false;
    // View origin/delta/rotation snapshots for adjustZPlanes (ViewingSpace.ts:276-300).
    // m_viewOrigin/Delta are the (possibly z-clip-adjusted) values fed to
    // computeWorldToNpc; the *Unexpanded pair detects the adjustment.
    dqGeom::Matrix3d m_rotation{dqGeom::Matrix3d::CreateIdentity()};
    dqGeom::Point3d m_viewOrigin;
    dqGeom::Vector3d m_viewDelta;
    dqGeom::Point3d m_viewOriginUnexpanded;
    dqGeom::Vector3d m_viewDeltaUnexpanded;
    bool m_zClipAdjusted = false;
    // 参考 ViewingSpace 持有 view（ViewingSpace.ts getFrustum:479 的
    // view.computeWorldToNpc 回读）；DanQing 由 update() 系挂（不拥有）。
    ViewState3d const* m_view = nullptr;   // not owned
};

}  // namespace dqApp

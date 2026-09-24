// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — BackgroundMapGeometry depth-range helpers
// Ported from: itwinjs-core core/frontend/src/BackgroundMapGeometry.ts
//
// Background-map geometry for the ViewingSpace.adjustZPlanes depth fit
// (ViewingSpace.ts:191-204). The geometry is either the cartesian ground PLANE
// (GlobeMode.Plane) or the WGS84 earth ELLIPSOID (GlobeMode.Ellipsoid — the
// itwinjs default, BackgroundMapSettings.ts:38); getFrustumIntersectionDepthRange
// picks the branch from the stored geometry variant, exactly as the reference
// switches on `this.geometry instanceof Plane3dByOriginAndUnitNormal`
// (BackgroundMapGeometry.ts:308).
//
// Subset: the depth-range path (constructor fields, getCartesianRange /
// getCartesianTransitionDistance / getPlane / getEarthEllipsoid /
// getFrustumIntersectionDepthRange). The cartographic conversion methods
// (dbToCartographic & friends), the mercator tiling scheme, and
// addFrustumDecorations are TODO (no ported consumer).
#pragma once

#include <dqCommon/Cartographic.h>
#include <dqCommon/Frustum.h>
#include <dqCommon/GlobeMode.h>
#include <dqCommon/ViewFlags.h>
#include <dqGeom/Ellipsoid.h>
#include <dqGeom/Plane3dByOriginAndUnitNormal.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>

#include <optional>
#include <variant>

namespace dqApp {

class IModelConnection;

// Ported from: itwinjs-core getFrustumPlaneIntersectionDepthRange
//              (BackgroundMapGeometry.ts:57-65, exported internal).
// Returns the depth range (along the frustum's view-Z axis) of the polygon
// formed by intersecting the frustum's 4 back→front edge rays with `plane`.
// Returns a null Range1d if the frustum has no rotation or no edge crosses the
// plane (and no eye-point horizon fallback applies).
dqGeom::Range1d getFrustumPlaneIntersectionDepthRange(
    dqCommon::Frustum const& frustum,
    dqGeom::Plane3dByOriginAndUnitNormal const& plane);

// Ported from: itwinjs-core BackgroundMapGeometry (BackgroundMapGeometry.ts:70-487,
// depth-path subset — see file header).
class BackgroundMapGeometry {
public:
    /// If globe is 3D we still consider the map geometry flat within this
    /// distance of the project extents (BackgroundMapGeometry.ts:84).
    static constexpr double maxCartesianDistance = 1.0e4;

    // --- Public readonly fields (reference visibility, BackgroundMapGeometry.ts:71-80) ---
    // 注：参考在构造函数体内逐字段赋值，顺序无关；C++ 成员初始化顺序按声明——
    // geometry 依赖 globeOrigin/globeMatrix（getEarthEllipsoid），故声明在最后。
    dqCommon::GlobeMode const globeMode;
    dqGeom::Point3d const globeOrigin;
    dqGeom::Matrix3d const globeMatrix;
    dqGeom::Range3d const cartesianRange;
    dqGeom::Range3d const cartesianTransitionRange;
    dqGeom::Plane3dByOriginAndUnitNormal const cartesianPlane;
    double const cartesianDiagonal;
    double const cartesianChordHeight;
    double const maxGeometryChordHeight;
    /// Plane3dByOriginAndUnitNormal | Ellipsoid per globeMode (:72, :105).
    std::variant<dqGeom::Plane3dByOriginAndUnitNormal, dqGeom::Ellipsoid> const geometry;

    /// Ported from: BackgroundMapGeometry constructor (BackgroundMapGeometry.ts:91-108).
    /// (the mercator tiling scheme + _mercatorFractionToDb are not ported — TODO
    /// with the cartographic conversion methods.)
    BackgroundMapGeometry(double bimElevationBias, dqCommon::GlobeMode globeMode,
                          IModelConnection const* iModel) noexcept;

    /// Ported from: BackgroundMapGeometry.getCartesianRange (BackgroundMapGeometry.ts:109-113)
    static dqGeom::Range3d getCartesianRange(IModelConnection const* iModel) noexcept;

    /// Ported from: BackgroundMapGeometry.getCartesianTransitionDistance
    /// (BackgroundMapGeometry.ts:114-116)
    static double getCartesianTransitionDistance(IModelConnection const* iModel) noexcept;

    /// The WGS84 earth ellipsoid at globeOrigin with radii enlarged by radiusOffset.
    /// Ported from: BackgroundMapGeometry.getEarthEllipsoid (BackgroundMapGeometry.ts:234-237)
    dqGeom::Ellipsoid getEarthEllipsoid(double radiusOffset = 0.0) const noexcept;

    /// Ported from: BackgroundMapGeometry.dbToCartographic (BackgroundMapGeometry.ts:159-171)：
    /// db → ECEF（_ecefToDb 逆）→ Cartographic.fromEcef。Plane（mercator）分支未
    /// 移植——globeMode 样式图未接线恒 Ellipsoid（TODO 登记 mercator tiling scheme）。
    /// 参考经 expectDefined 抛错；DanQing 无异常 → 逆变换失败返回 nullopt。
    std::optional<dqCommon::Cartographic> dbToCartographic(dqGeom::Point3d const& db) const;

    /// Ported from: BackgroundMapGeometry.cartographicToDb (BackgroundMapGeometry.ts:224-231)：
    /// Cartographic.toEcef → _ecefToDb 正变换。（Plane 分支同上未移植。）
    dqGeom::Point3d cartographicToDb(dqCommon::Cartographic const& cartographic) const;

    /// Ported from: BackgroundMapGeometry.getPlane (BackgroundMapGeometry.ts:239-241).
    /// The background-map ground plane: origin (0, 0, bimElevationBias + offset),
    /// normal iModel +Z.
    dqGeom::Plane3dByOriginAndUnitNormal getPlane(double offset = 0.0) const noexcept;

    /// Ported from: BackgroundMapGeometry.getRayIntersection (BackgroundMapGeometry.ts:243-277).
    /// Intersect a ray with the background-map geometry. Ellipsoid branch: nearest
    /// ray∩ellipsoid hit, corrected to the cartesian plane when the hit is
    /// front-facing and inside cartesianRange (:259-264). Plane branch: ray∩plane.
    /// Returns the hit point + surface normal as a Ray3d; nullopt when no
    /// (positive-only, if requested) intersection exists.
    std::optional<dqGeom::Ray3d> getRayIntersection(dqGeom::Ray3d const& ray,
                                                    bool positiveOnly) const;

    /// Ported from: BackgroundMapGeometry.getFrustumIntersectionDepthRange
    /// (BackgroundMapGeometry.ts:294-433) — BOTH branches:
    ///  - planar (:309-313): intersect the frustum with the ground planes at
    ///    z = bias + heightRange.{low,high} (+ optional gridPlane);
    ///  - ellipsoid (:314-419): WGS84 ellipsoid depth fit — extrema point, eye
    ///    (inside) or silhouette-clip (outside), frustum-plane section arcs clipped
    ///    to the frustum, and the cartesian-rectangle clip.
    dqGeom::Range1d getFrustumIntersectionDepthRange(
        dqCommon::Frustum const& frustum,
        dqGeom::Range3d const& bimRange,
        std::optional<dqGeom::Range1d> const& heightRange,
        std::optional<dqGeom::Plane3dByOriginAndUnitNormal> const& gridPlane,
        bool doGlobalScope) const;

private:
    double m_bimElevationBias;
    dqGeom::Transform m_ecefToDb;   // _ecefToDb (:83) — getMapEcefToDb(bimElevationBias)
};

// Ported from: itwinjs-core DisplayStyleState.getGlobalGeometryAndHeightRange +
//              getIsBackgroundMapVisible + getBackgroundMapGeometry
//              (DisplayStyleState.ts:734-800).
// Returns the background-map geometry (plane or earth ellipsoid per globeMode) +
// height range when a background map is visible; nullopt otherwise.
// heightRange = (-1, 1) (applyTerrain=false, the blank-connection default).
struct GlobalGeometryAndHeightRange {
    BackgroundMapGeometry geometry;
    dqGeom::Range1d heightRange;
};
std::optional<GlobalGeometryAndHeightRange> getGlobalGeometryAndHeightRange(
    IModelConnection const* iModel, dqCommon::ViewFlags const& viewFlags);

}  // namespace dqApp

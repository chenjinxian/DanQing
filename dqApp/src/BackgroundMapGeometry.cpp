// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — BackgroundMapGeometry depth-range helpers
// Ported from: itwinjs-core core/frontend/src/BackgroundMapGeometry.ts
//                accumulateDepthRange / accumulateFrustumPlaneDepthRange (L28-55)
//                getFrustumPlaneIntersectionDepthRange            (L57-65)
//                BackgroundMapGeometry class                      (L70-487, depth-path subset)
//              itwinjs-core core/frontend/src/DisplayStyleState.ts
//                getIsBackgroundMapVisible / getBackgroundMapGeometry /
//                getGlobalGeometryAndHeightRange                  (L734-800)
//              itwinjs-core core/frontend/src/IModelConnection.ts
//                getMapEcefToDb                                   (L571-583)
#include "BackgroundMapGeometry.h"

#include <dqApp/EcefLocation.h>
#include <dqApp/IModelConnection.h>
#include <dqGeom/Arc3d.h>
#include <dqGeom/ClipPlane.h>
#include <dqGeom/ClipUtils.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Point4d.h>
#include <dqGeom/Ray3d.h>
#include <dqGeom/Vector3d.h>

#include <array>
#include <cmath>
#include <vector>

namespace dqApp {
namespace {

// Ported from: itwinjs-core Constant.earthRadiusWGS84
//              (core/geometry/src/Constant.ts:27-31)
constexpr double kEarthRadiusWGS84Polar = 6356752.3142;
constexpr double kEarthRadiusWGS84Equator = 6378137.0;

// Ported from: itwinjs-core accumulateDepthRange (BackgroundMapGeometry.ts:28-31).
// Transforms `point` into view coords (3x3 product — identical to multiplyXYZtoXYZ
// for a rotation) and extends `range`.
void accumulateDepthRange(dqGeom::Point3d const& point,
                          dqGeom::Matrix3d const& viewRotation,
                          dqGeom::Range3d& range)
{
    dqGeom::Vector3d const v = viewRotation.MultiplyVector(dqGeom::Vector3d::From(point));
    range.ExtendPoint(dqGeom::Point3d::From(v.x, v.y, v.z));
}

// Ported from: itwinjs-core accumulateFrustumPlaneDepthRange
//              (BackgroundMapGeometry.ts:33-55).
// eyePoint==nullptr → orthographic: cast 4 back→front edge rays.
// eyePoint!=nullptr → perspective: cast 4 eye→front-edge rays, with horizon fallback.
void accumulateFrustumPlaneDepthRange(dqCommon::Frustum const& frustum,
                                      dqGeom::Plane3dByOriginAndUnitNormal const& plane,
                                      dqGeom::Matrix3d const& viewRotation,
                                      dqGeom::Range3d& range,
                                      dqGeom::Point3d const* eyePoint)
{
    bool includeHorizon = false;
    for (int i = 0; i < 4; ++i) {
        // frustumRay = Ray3d.createStartEnd(eyePoint ? eyePoint : frustum.points[i+4], frustum.points[i])
        dqGeom::Point3d const rayStart = (eyePoint != nullptr) ? *eyePoint : frustum.points[i + 4];
        dqGeom::Ray3d const frustumRay = dqGeom::Ray3d::FromStartEnd(rayStart, frustum.points[i]);

        dqGeom::Point3d scratchPoint{};
        auto const thisFraction = frustumRay.intersectionWithPlane(plane, &scratchPoint);
        // Ported from: itwinjs-core BackgroundMapGeometry.ts:38
        //   if (undefined !== thisFraction && (!eyePoint || thisFraction > 0))
        //     accumulateDepthRange(...); else includeHorizon = true;
        // Any defined fraction is accepted — including extrapolated (<0 or >1)
        // crossings. At a glancing (horizon) angle the depth edges meet the ground
        // plane far outside the [0,1] segment; those far intersections are exactly
        // what extend the depth range so the frustum brackets z=0 and the grid
        // keeps filling at the horizon. (A prior [0,1] ortho clamp rejected them
        // and made the grid vanish at the horizon. Ray3d returns nullopt for truly
        // parallel edges — capping the range at a large-but-finite value — and the
        // viewed-extents branch in adjustZPlanes backstops the fully-degenerate
        // case.)
        if (thisFraction.has_value() && (eyePoint == nullptr || *thisFraction > 0.0))
            accumulateDepthRange(scratchPoint, viewRotation, range);
        else
            includeHorizon = true;
    }
    if (includeHorizon && eyePoint != nullptr) {
        double const eyeHeight = plane.altitude(*eyePoint);
        if (eyeHeight < 0.0) {
            accumulateDepthRange(*eyePoint, viewRotation, range);
        } else {
            dqGeom::Vector3d const viewZ = viewRotation.RowZ();
            double const horizonDistance = std::sqrt(
                eyeHeight * eyeHeight + 2.0 * eyeHeight * kEarthRadiusWGS84Equator);
            // eyePoint.plusScaled(viewZ, -horizonDistance)
            dqGeom::Point3d scratch;
            scratch.x = eyePoint->x + (-horizonDistance) * viewZ.x;
            scratch.y = eyePoint->y + (-horizonDistance) * viewZ.y;
            scratch.z = eyePoint->z + (-horizonDistance) * viewZ.z;
            accumulateDepthRange(scratch, viewRotation, range);
        }
    }
}

// Ported from: itwinjs-core IModelConnection.getMapEcefToDb (IModelConnection.ts:571-583).
//   if (!this.ecefLocation) return Transform.createIdentity();
//   const mapEcefToDb = this.ecefLocation.getTransform().inverse();
//   mapEcefToDb.origin.z += bimElevationBias;  return mapEcefToDb;
// DanQing 的 EcefLocation 为值成员——"ecefLocation undefined" 对应零原点（同
// ViewState.cpp:303 的 geoLocated 约定）。getTransform() = Transform(origin,
// orientation)（iModel 空间 → ECEF，IModel.ts:261-289）。
dqGeom::Transform getMapEcefToDb(dqApp::EcefLocation const& ecef, double bimElevationBias) noexcept
{
    bool const geoLocated = (ecef.origin.x != 0.0 || ecef.origin.y != 0.0 || ecef.origin.z != 0.0);
    if (!geoLocated)
        return dqGeom::Transform::CreateIdentity();
    dqGeom::Transform const ecefFromDb =
        dqGeom::Transform::CreateOriginAndMatrix(ecef.origin, ecef.orientation);
    dqGeom::Transform mapEcefToDb;
    if (!ecefFromDb.Inverse(mapEcefToDb))
        return dqGeom::Transform::CreateIdentity();  // assert(false) in reference
    mapEcefToDb.origin.z += bimElevationBias;
    return mapEcefToDb;
}

}  // namespace

// Ported from: itwinjs-core getFrustumPlaneIntersectionDepthRange
//              (BackgroundMapGeometry.ts:57-65).
dqGeom::Range1d getFrustumPlaneIntersectionDepthRange(
    dqCommon::Frustum const& frustum,
    dqGeom::Plane3dByOriginAndUnitNormal const& plane)
{
    auto const eyePointOpt = frustum.getEyePoint();
    auto const viewRotationOpt = frustum.getRotation();
    if (!viewRotationOpt.has_value())   // expectDefined → else null range
        return dqGeom::Range1d::CreateNull();

    dqGeom::Range3d intersectRange = dqGeom::Range3d::CreateNull();
    accumulateFrustumPlaneDepthRange(
        frustum, plane, *viewRotationOpt, intersectRange,
        eyePointOpt.has_value() ? &(*eyePointOpt) : nullptr);

    return intersectRange.isNull()
        ? dqGeom::Range1d::CreateNull()
        : dqGeom::Range1d::CreateXX(intersectRange.low.z, intersectRange.high.z);
}

// Ported from: BackgroundMapGeometry.getCartesianRange (BackgroundMapGeometry.ts:109-113)
dqGeom::Range3d BackgroundMapGeometry::getCartesianRange(IModelConnection const* iModel) noexcept
{
    dqGeom::Range3d cartesianRange = iModel->GetProjectExtents();   // Range3d.createFrom
    cartesianRange.expandInPlace(BackgroundMapGeometry::maxCartesianDistance);
    return cartesianRange;
}

// Ported from: BackgroundMapGeometry.getCartesianTransitionDistance (:114-116)
double BackgroundMapGeometry::getCartesianTransitionDistance(IModelConnection const* iModel) noexcept
{
    // _transitionDistanceMultiplier = .25 (BackgroundMapGeometry.ts:85)
    constexpr double kTransitionDistanceMultiplier = 0.25;
    return BackgroundMapGeometry::getCartesianRange(iModel).Diagonal().MagnitudeXY()
        * kTransitionDistanceMultiplier;
}

// Ported from: BackgroundMapGeometry constructor (BackgroundMapGeometry.ts:91-108)
BackgroundMapGeometry::BackgroundMapGeometry(double bimElevationBias,
                                             dqCommon::GlobeMode globeMode_,
                                             IModelConnection const* iModel) noexcept
    : globeMode(globeMode_)
    , globeOrigin(getMapEcefToDb(iModel->GetEcefLocation(), bimElevationBias).origin)
    , globeMatrix(getMapEcefToDb(iModel->GetEcefLocation(), bimElevationBias).matrix)
    , cartesianRange(getCartesianRange(iModel))
    , cartesianTransitionRange([this, iModel] {
          dqGeom::Range3d r = cartesianRange;
          r.expandInPlace(getCartesianTransitionDistance(iModel));
          return r;
      }())
    , cartesianPlane(dqGeom::Plane3dByOriginAndUnitNormal::createXYPlane(
          dqGeom::Point3d::From(0.0, 0.0, bimElevationBias)))
    , cartesianDiagonal(cartesianRange.Diagonal().MagnitudeXY())
    , cartesianChordHeight(std::sqrt(cartesianDiagonal * cartesianDiagonal
                                     + kEarthRadiusWGS84Equator * kEarthRadiusWGS84Equator)
                           - kEarthRadiusWGS84Equator)
    , maxGeometryChordHeight((1.0 - std::cos(dqGeom::Angle::kPiOver2 / 10.0)) * kEarthRadiusWGS84Equator)
    // :105 — geometry = (globeMode === Ellipsoid) ? getEarthEllipsoid() : cartesianPlane
    , geometry((globeMode_ == dqCommon::GlobeMode::Ellipsoid)
                   ? std::variant<dqGeom::Plane3dByOriginAndUnitNormal, dqGeom::Ellipsoid>(
                         std::in_place_type<dqGeom::Ellipsoid>, getEarthEllipsoid())
                   : std::variant<dqGeom::Plane3dByOriginAndUnitNormal, dqGeom::Ellipsoid>(
                         std::in_place_type<dqGeom::Plane3dByOriginAndUnitNormal>, cartesianPlane))
    , m_bimElevationBias(bimElevationBias)
    , m_ecefToDb(getMapEcefToDb(iModel->GetEcefLocation(), bimElevationBias))
{
}

// Ported from: BackgroundMapGeometry.getEarthEllipsoid (BackgroundMapGeometry.ts:234-237)
dqGeom::Ellipsoid BackgroundMapGeometry::getEarthEllipsoid(double radiusOffset) const noexcept
{
    double const equatorRadius = kEarthRadiusWGS84Equator + radiusOffset;
    double const polarRadius = kEarthRadiusWGS84Polar + radiusOffset;
    return dqGeom::Ellipsoid::createCenterMatrixRadii(
        globeOrigin, &globeMatrix, equatorRadius, equatorRadius, polarRadius);
}

// Ported from: itwinjs-core BackgroundMapGeometry.dbToCartographic
//              (BackgroundMapGeometry.ts:159-171) —— Ellipsoid 分支（:165-168）：
//   const ecef = expectDefined(this._ecefToDb.multiplyInversePoint3d(db));
//   return expectDefined(Cartographic.fromEcef(ecef, result));
// Plane 分支（:160-164，mercator tiling scheme）未移植——globeMode 样式图未接线
// 恒 Ellipsoid（TODO 登记）；参考 expectDefined 抛错 → DanQing 无异常返回 nullopt。
std::optional<dqCommon::Cartographic> BackgroundMapGeometry::dbToCartographic(
    dqGeom::Point3d const& db) const
{
    dqGeom::Point3d ecef;
    if (!m_ecefToDb.MultiplyInversePoint3d(db, ecef))
        return std::nullopt;
    return dqCommon::Cartographic::fromEcef(ecef);
}

// Ported from: itwinjs-core BackgroundMapGeometry.cartographicToDb
//              (BackgroundMapGeometry.ts:224-231) —— Ellipsoid 分支（:230）：
//   return this._ecefToDb.multiplyPoint3d(cartographic.toEcef());
dqGeom::Point3d BackgroundMapGeometry::cartographicToDb(
    dqCommon::Cartographic const& cartographic) const
{
    return m_ecefToDb.MultiplyPoint3d(cartographic.toEcef());
}

// Ported from: itwinjs-core BackgroundMapGeometry.getPlane (BackgroundMapGeometry.ts:239-241).
//   expectDefined(Plane3dByOriginAndUnitNormal.create(
//       Point3d(0, 0, this._bimElevationBias + offset), Vector3d(0, 0, 1)))
// Normal (0,0,1) is unit, so createXYPlane (normal +Z through origin) is exact.
dqGeom::Plane3dByOriginAndUnitNormal BackgroundMapGeometry::getPlane(double offset) const noexcept
{
    return dqGeom::Plane3dByOriginAndUnitNormal::createXYPlane(
        dqGeom::Point3d::From(0.0, 0.0, m_bimElevationBias + offset));
}

// Ported from: itwinjs-core BackgroundMapGeometry.getRayIntersection
//              (BackgroundMapGeometry.ts:243-277).
std::optional<dqGeom::Ray3d> BackgroundMapGeometry::getRayIntersection(
    dqGeom::Ray3d const& ray, bool positiveOnly) const
{
    std::optional<dqGeom::Ray3d> intersect;

    if (globeMode == dqCommon::GlobeMode::Ellipsoid) {
        // :245-267 — ellipsoid branch: nearest front-facing ray∩ellipsoid hit,
        // corrected to the cartesian plane when inside cartesianRange.
        auto const& ellipsoid = std::get<dqGeom::Ellipsoid>(geometry);
        std::vector<double> fractions;
        std::vector<dqGeom::LongitudeLatitudeNumber> angles;
        // :250 — ellipsoid.intersectRay(ray, fractions, undefined, angles)
        size_t const count = ellipsoid.intersectRay(ray, &fractions, nullptr, &angles);

        std::optional<double> intersectDistance;
        for (size_t i = 0; i < count; ++i) {
            double const thisFraction = fractions[i];
            // :253-254 — positiveOnly gate + nearest-fraction selection
            if ((!positiveOnly || thisFraction > 0.0)
                && (!intersectDistance.has_value() || thisFraction < *intersectDistance)) {
                intersectDistance = thisFraction;
                // :256-257 — intersect = unit-normal ray at the hit's lon/lat
                auto normalRay = ellipsoid.radiansToUnitNormalRay(
                    angles[i].longitudeRadians(), angles[i].latitudeRadians());
                if (!normalRay.has_value())
                    continue;
                intersect = *normalRay;
                // :258 — front-facing (normal opposes the ray direction)
                if (intersect->direction.DotProduct(ray.direction) < 0.0) {
                    // :259-264 — inside the cartesian range → correct to the
                    // planar intersection (origin = plane hit, normal = plane normal)
                    if (cartesianRange.ContainsPoint(intersect->origin)) {
                        dqGeom::Point3d planeHit;
                        auto const planeFraction = ray.intersectionWithPlane(cartesianPlane, &planeHit);
                        if (planeFraction.has_value() && (!positiveOnly || *planeFraction > 0.0)) {
                            intersect->origin = planeHit;
                            intersect->direction = cartesianPlane.getNormalRef();
                        }
                    }
                }
            }
        }
    } else {
        // :268-274 — plane branch: ray∩cartesianPlane, normal = plane normal.
        auto const& plane = std::get<dqGeom::Plane3dByOriginAndUnitNormal>(geometry);
        dqGeom::Point3d planeHit;
        auto const thisFraction = ray.intersectionWithPlane(plane, &planeHit);
        if (thisFraction.has_value() && (!positiveOnly || *thisFraction > 0.0)) {
            intersect = dqGeom::Ray3d::create(planeHit, plane.getNormalRef());
        }
    }
    return intersect;
}

// Ported from: itwinjs-core BackgroundMapGeometry.getFrustumIntersectionDepthRange
//              (BackgroundMapGeometry.ts:294-433) — planar + ellipsoid branches.
dqGeom::Range1d BackgroundMapGeometry::getFrustumIntersectionDepthRange(
    dqCommon::Frustum const& frustum,
    dqGeom::Range3d const& bimRange,
    std::optional<dqGeom::Range1d> const& heightRange,
    std::optional<dqGeom::Plane3dByOriginAndUnitNormal> const& gridPlane,
    bool doGlobalScope) const
{
    auto clipPlanes = frustum.GetRangePlanes(false, false, 0.0);          // :296
    auto const eyePointOpt = frustum.getEyePoint();                       // :297
    auto const viewRotationOpt = frustum.getRotation();                   // :298
    if (!viewRotationOpt.has_value())                                     // :299-300 degenerate frustum
        return dqGeom::Range1d::CreateNull();
    dqGeom::Matrix3d const& viewRotation = *viewRotationOpt;
    dqGeom::Vector3d const viewZ = viewRotation.RowZ();                   // :301
    dqGeom::Point3d const* eyePoint = eyePointOpt.has_value() ? &(*eyePointOpt) : nullptr;

    dqGeom::Range3d const& cartoRange = cartesianTransitionRange;         // :302
    dqGeom::Range3d intersectRange = dqGeom::Range3d::CreateNull();       // :303

    // :306-308 — optional gridPlane accumulates into the shared range.
    if (gridPlane.has_value())
        accumulateFrustumPlaneDepthRange(
            frustum, *gridPlane, viewRotation, intersectRange, eyePoint);

    if (std::holds_alternative<dqGeom::Plane3dByOriginAndUnitNormal>(geometry)) {
        // :309-313 — Intersection with a planar background projection...
        std::array<double, 2> heights;
        int nHeights;
        if (heightRange.has_value()) {
            heights[0] = heightRange->low;
            heights[1] = heightRange->high;
            nHeights = 2;
        } else {
            heights[0] = 0.0;
            nHeights = 1;
        }
        for (int i = 0; i < nHeights; ++i)
            accumulateFrustumPlaneDepthRange(
                frustum, getPlane(heights[i]), viewRotation, intersectRange, eyePoint);
    } else {
        // :314-419 — Ellipsoid background projection (GlobeMode.Ellipsoid).
        double const minOffset = heightRange.has_value() ? heightRange->low : 0.0;              // :315
        double const maxOffset = (heightRange.has_value() ? heightRange->high : 0.0)
                                 + cartesianChordHeight;
        std::vector<double> radiusOffsets{minOffset, maxOffset};                                // :316

        // If we are doing global scope then include minimum ellipsoid that represents
        // the chordal approximation of the low level tiles (:318-321).
        if (doGlobalScope)
            radiusOffsets.push_back(minOffset - maxGeometryChordHeight);

        dqGeom::Transform const toView =
            dqGeom::Transform::CreateOriginAndMatrix(dqGeom::Point3d::FromZero(), viewRotation);  // :323
        dqGeom::Point4d const eyePoint4d = eyePoint != nullptr
            ? dqGeom::Point4d::createFromPointAndWeight(*eyePoint, 1.0)
            : dqGeom::Point4d::createFromPointAndWeight(
                  dqGeom::Point3d::From(viewZ.x, viewZ.y, viewZ.z), 0.0);                          // :324

        for (double const radiusOffset : radiusOffsets) {                                        // :326
            dqGeom::Ellipsoid const ellipsoid = getEarthEllipsoid(radiusOffset);                 // :327
            std::optional<dqGeom::Point3d> eyeLocal;
            if (eyePoint != nullptr)
                eyeLocal = ellipsoid.worldToLocal(*eyePoint);                                    // :328
            if (eyePoint != nullptr && !eyeLocal.has_value()) {
                // If the globe transform cannot map the eye point into ellipsoid-local
                // space, treat the globe as unavailable here and let the caller fall
                // back to its extents-based depth fitting (:329-334).
                return dqGeom::Range1d::CreateNull();
            }
            bool const isInside = eyeLocal.has_value() && eyeLocal->Magnitude() < 1.0;           // :335
            dqGeom::Point3d const center =
                ellipsoid.localToWorld(dqGeom::Point3d::FromZero());                             // :336
            size_t const clipPlaneCount = clipPlanes.planes.size();                              // :337

            // Extrema... (:339-345)
            // (surfaceNormalToAngles / radiansToPoint are non-failing in the ported
            // Ellipsoid API, so the reference's undefined checks collapse away.)
            {
                auto const angles = ellipsoid.surfaceNormalToAngles(viewZ);
                dqGeom::Point3d const extremaPoint = ellipsoid.radiansToPoint(
                    angles.longitudeRadians(), angles.latitudeRadians());
                if ((eyePoint == nullptr
                     || viewZ.DotProduct(dqGeom::Vector3d::FromStartEnd(extremaPoint, *eyePoint)) > 0.0)
                    && clipPlanes.classifyPointContainment({extremaPoint}, false)
                           != dqGeom::ClipPlaneContainment::StronglyOutside) {
                    accumulateDepthRange(extremaPoint, viewRotation, intersectRange);
                }
            }

            if (isInside) {                                                                      // :347-349
                if (eyePoint != nullptr)
                    accumulateDepthRange(*eyePoint, viewRotation, intersectRange);
            } else {                                                                             // :350-382
                auto const silhouette = ellipsoid.silhouetteArc(eyePoint4d);                     // :351
                if (silhouette.IsValid()) {
                    dqGeom::Vector3d silhouetteNormal = silhouette->PerpendicularVector();       // :353
                    // Push the silhouette plane as clip so that we do not include geometry
                    // at other side of ellipsoid. First make sure that it is pointing in
                    // the right direction.
                    if (eyePoint != nullptr) {
                        // Clip toward eye (:356-359).
                        if (silhouetteNormal.DotProduct(viewZ) < 0.0)
                            silhouetteNormal.Negate();
                    } else {
                        // If parallel projection - clip toward side of ellipsoid with BIM
                        // geometry (:360-363).
                        if (dqGeom::Vector3d::FromStartEnd(silhouette->CenterRef(), bimRange.Center())
                                .DotProduct(silhouetteNormal) < 0.0)
                            silhouetteNormal.Negate();
                    }
                    // The silhouette arc is the horizon between the visible and hidden
                    // halves of the globe. Its perpendicular vector is the normal of the
                    // clip plane that keeps us on the visible side. If that plane cannot
                    // be constructed, fall back to a view-aligned plane through the
                    // ellipsoid center instead of pushing an invalid entry (:365-372).
                    auto silhouettePlane = dqGeom::ClipPlane::createNormalAndDistance(
                        silhouetteNormal,
                        silhouetteNormal.DotProduct(dqGeom::Vector3d::From(silhouette->CenterRef())));
                    if (!silhouettePlane.has_value())
                        silhouettePlane = dqGeom::ClipPlane::createNormalAndPoint(viewZ, center);
                    if (!silhouettePlane.has_value())                                            // :373-374
                        return dqGeom::Range1d::CreateNull();
                    clipPlanes.planes.push_back(*silhouettePlane);                               // :375
                } else {
                    auto const centerPlane = dqGeom::ClipPlane::createNormalAndPoint(viewZ, center);  // :377
                    if (!centerPlane.has_value())                                                // :378-379
                        return dqGeom::Range1d::CreateNull();
                    clipPlanes.planes.push_back(*centerPlane);                                   // :380
                }
            }
            if (!isInside || radiusOffset == radiusOffsets[0]) {                                 // :383
                // Intersections of ellipsoid with frustum planes... (:384-403)
                bool const viewingInside = eyePoint != nullptr
                    && viewZ.DotProduct(dqGeom::Vector3d::FromStartEnd(center, *eyePoint)) < 0.0;   // :385
                if (eyePoint == nullptr || !isInside || viewingInside) {                          // :386
                    for (auto const& clipPlane : clipPlanes.planes) {                             // :387
                        auto const plane = clipPlane.getPlane3d();                                // :388
                        auto const arc = ellipsoid.createPlaneSection(plane);                     // :389
                        if (arc.IsValid()) {                                                      // :390
                            arc->announceClipIntervals(                                           // :391
                                clipPlanes,
                                [&](double a0, double a1, dqGeom::Arc3d const& cp) {
                                    if (std::abs(a1 - a0) < 1.0e-8) {
                                        // Tiny sweep - avoid problem with rangeMethod (:392-393)
                                        accumulateDepthRange(cp.FractionToPoint(a0),
                                                             viewRotation, intersectRange);
                                    } else {
                                        auto const segment = cp.clonePartialCurve(a0, a1);        // :395
                                        if (segment.IsValid())                                    // :396
                                            segment->extendRange(intersectRange, &toView);        // :397
                                    }
                                });
                        }
                    }
                }
                // Intersections of the cartesian region with frustum planes (:404-414).
                std::vector<dqGeom::Point3d> cartoRectangle = {
                    dqGeom::Point3d::From(cartoRange.low.x, cartoRange.low.y, radiusOffset),
                    dqGeom::Point3d::From(cartoRange.high.x, cartoRange.low.y, radiusOffset),
                    dqGeom::Point3d::From(cartoRange.high.x, cartoRange.high.y, radiusOffset),
                    dqGeom::Point3d::From(cartoRange.low.x, cartoRange.high.y, radiusOffset),
                    dqGeom::Point3d::From(cartoRange.low.x, cartoRange.low.y, radiusOffset),
                };
                std::vector<dqGeom::Point3d> scratchWork;
                clipPlanes.clipConvexPolygonInPlace(cartoRectangle, scratchWork);                 // :412
                for (auto const& p : cartoRectangle)                                              // :413-414
                    accumulateDepthRange(p, viewRotation, intersectRange);
                while (clipPlanes.planes.size() > clipPlaneCount)   // Remove pushed silhouette plane (:415-416)
                    clipPlanes.planes.pop_back();
            }
        }
    }

    // :421-432 — post-processing.
    if (intersectRange.ZLength() < 5.0) {
        // For the case where the fitted depth is small (less than 5 meters) we must be
        // viewing planar projection or the planar portion of the iModel in plan view.
        // In this case use a constant (arbitrarily 100 meters) depth so that the frustum
        // Z is doesn't change and cause nearly planar geometry to jitter in Z buffer.
        double const zCenter = (intersectRange.low.z + intersectRange.high.z) * 0.5;
        constexpr double kZExpand = 50.0;
        return dqGeom::Range1d::CreateXX(zCenter - kZExpand, zCenter + kZExpand);
    }
    double const diagonalXY = intersectRange.Diagonal().MagnitudeXY();
    double const expansion = diagonalXY * 0.01;
    return dqGeom::Range1d::CreateXX(
        intersectRange.low.z - expansion, intersectRange.high.z + expansion);
}

// Ported from: itwinjs-core DisplayStyleState.getGlobalGeometryAndHeightRange +
//              getIsBackgroundMapVisible + getBackgroundMapGeometry
//              (DisplayStyleState.ts:734-800).
std::optional<GlobalGeometryAndHeightRange> getGlobalGeometryAndHeightRange(
    IModelConnection const* iModel, dqCommon::ViewFlags const& viewFlags)
{
    // getIsBackgroundMapVisible (DisplayStyleState.ts:740-742):
    //   this._hasEarthLocation && (this.viewFlags.backgroundMap || this.anyMapLayersVisible(false))
    // _hasEarthLocation (:734-735): undefined !== this.iModel.ecefLocation.
    // DanQing 的 EcefLocation 是值成员，无 isDefined 谓词——"ecefLocation defined" 对应
    // 非零原点（同 ViewState.cpp:303 的 geoLocated 约定；BlankConnection 仅在传
    // location 时填充）。anyMapLayersVisible 未移植（DanQing 无地图层）。
    // TODO: port EcefLocation.isDefined + BackgroundMapSettings (applyTerrain /
    // globeMode) into the display-style graph for full fidelity.
    if (iModel == nullptr || !viewFlags.backgroundMap())
        return std::nullopt;
    auto const& ecef = iModel->GetEcefLocation();
    bool const hasEarthLocation = (ecef.origin.x != 0.0 || ecef.origin.y != 0.0 || ecef.origin.z != 0.0);
    if (!hasEarthLocation)
        return std::nullopt;

    // getBackgroundMapGeometry (DisplayStyleState.ts:765-780): bimElevationBias =
    // backgroundMapElevationBias. applyTerrain=false (blank default —
    // BackgroundMapSettings not yet wired into the style graph) →
    // backgroundMapSettings.groundBias = 0 (BackgroundMapSettings.ts:142 default).
    double const bimElevationBias = 0.0;

    // globeMode = this.globeMode (DisplayStyleState getter →
    // settings.backgroundMap.globeMode)。样式设置图未接入——取参考默认 Ellipsoid
    // (BackgroundMapSettings.ts:38)。
    // getGlobalGeometryAndHeightRange (DisplayStyleState.ts:786-800):
    //   heightRange = displayTerrain ? terrainRange : Range1d.createXX(-1, 1).
    // displayTerrain = viewFlags.backgroundMap && settings.backgroundMap.applyTerrain;
    // applyTerrain false → heightRange = (-1, 1)。contextRealityModelStates 无
    // （blank connection）→ 参考的 :789-797 分支不触发。
    return GlobalGeometryAndHeightRange{
        BackgroundMapGeometry(bimElevationBias, dqCommon::GlobeMode::Ellipsoid, iModel),
        dqGeom::Range1d::CreateXX(-1.0, 1.0)};
}

}  // namespace dqApp

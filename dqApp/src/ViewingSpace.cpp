// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewingSpace implementation
// Ported from: itwinjs-core core/frontend/src/ViewingSpace.ts
#include "dqApp/ViewingSpace.h"
#include "dqApp/ViewState.h"
#include "dqApp/IModelConnection.h"
#include "BackgroundMapGeometry.h"

#include <dqCommon/GridOrientationType.h>
#include <dqGeom/ClipUtils.h>
#include <dqGeom/ConvexClipPlaneSet.h>

#include <algorithm>
#include <cmath>

#include <optional>
#include <vector>

namespace dqApp {

namespace {
// Matrix3d vector-multiply returns Vector3d; dqGeom has no Point3d::From(Vector3d)
// overload, so convert component-wise (identical math — a rotation has no w/trans).
dqGeom::Point3d asPoint(dqGeom::Vector3d const& v) noexcept
{
    return dqGeom::Point3d::From(v.x, v.y, v.z);
}

// ViewingSpace.getViewedExtents (ViewingSpace.ts:280-289 → computeFitRange).
// projectExtents is FIXED (independent of zoom/pan), so the z-clip depth range it
// produces always brackets the project's z-range (and the grid plane at z=0) —
// using the view's own box here would shrink on zoom and drop the grid.
// TODO: + TiledGraphicsProvider.unionFitRange (not ported).
dqGeom::Range3d getViewedExtents(ViewState3d const& view)
{
    if (auto* imodel = view.GetIModel())
        return imodel->GetProjectExtents();
    // Null iModel (test fixtures only): fall back to the view's own box.
    dqGeom::Point3d const o = view.GetOrigin();
    dqGeom::Vector3d const e = view.GetExtents();
    return dqGeom::Range3d(o, dqGeom::Point3d::From(o.x + e.x, o.y + e.y, o.z + e.z));
}
}  // namespace

// NdcFromNpc: NPC[0,1]^3 → GL NDC[-1,1]^3 (scale 2, bias −1). Decision 1.
// Ported from: itwinjs-core in-shader NPC→NDC — the literal scale+bias the
// reference applies to gl_Position after the worldToViewMap. DanQing hoists it
// into the worldToNdc matrix so the existing u_mvp (NDC clip space) contract
// is unchanged.
static dqGeom::Matrix4d const kNdcFromNpc = dqGeom::Matrix4d::CreateRowValues(
    2.0, 0.0, 0.0, -1.0,
    0.0, 2.0, 0.0, -1.0,
    0.0, 0.0, 2.0, -1.0,
    0.0, 0.0, 0.0, 1.0);

// ---------------------------------------------------------------------------
// update — rebuild the transform chain.
// Ported from: itwinjs-core ViewingSpace constructor (ViewingSpace.ts:271-372).
// The reference constructs a new ViewingSpace per frustum change; DanQing reuses
// one instance via update() (functionally equivalent: both fully recompute the
// two maps from the current view + viewRect).
// ---------------------------------------------------------------------------
void ViewingSpace::update(ViewState3d const& view, ViewRect const& rect)
{
    m_view = &view;   // getFrustum 的未扩展分支经 computeWorldToNpc 回读（ViewingSpace.ts:479）
    // Viewport pixel rect — getViewCorners uses rect.right (high.x) / rect.bottom (low.y).
    m_viewW = static_cast<float>(rect.right);
    m_viewH = static_cast<float>(rect.bottom);
    if (m_viewW < 1.0f) m_viewW = 1.0f;
    if (m_viewH < 1.0f) m_viewH = 1.0f;

    // Affine world → view-local matrix (u_mv renderer adapter). Not part of the
    // itwinjs Map4d pipeline; rebuilt here so view-space lighting/sky uniforms
    // keep working alongside the faithful NPC/View maps.
    buildAffineViewMatrix(view);

    // Snapshot raw view origin/delta/rotation (ViewingSpace.ts:276-300).
    m_rotation = view.getRotation();
    m_viewOrigin = view.GetOrigin();
    m_viewDelta = view.GetExtents();
    // first, make sure none of the deltas are negative (ViewingSpace.ts:292-294).
    m_viewDelta.x = std::abs(m_viewDelta.x);
    m_viewDelta.y = std::abs(m_viewDelta.y);
    m_viewDelta.z = std::abs(m_viewDelta.z);
    m_viewOriginUnexpanded = m_viewOrigin;
    m_viewDeltaUnexpanded = m_viewDelta;
    m_zClipAdjusted = false;
    m_eyePoint = view.IsCameraOn() ? view.getEyePoint() : dqGeom::Point3d{};
    m_perspective = view.IsCameraOn();

    // adjustZPlanes: extend delta.z so the frustum brackets the grid plane at every
    // camera tilt (ViewingSpace.ts:328). Only for 3d views that allow 3d manipulations
    // (ViewingSpace.ts:303-321) — the path that makes the planar grid fill the viewport.
    if (view.is3d() && view.Allow3dManipulations()) {
        dqGeom::Point3d origin = m_viewOrigin;
        dqGeom::Vector3d delta = m_viewDelta;
        adjustZPlanes(origin, delta, view);
        // ViewingSpace.ts:347-349: if we moved the z planes, set zClipAdjusted
        // (exact component equality, matching Point3d.isExactEqual).
        bool const originMoved = (origin.x != m_viewOriginUnexpanded.x ||
                                  origin.y != m_viewOriginUnexpanded.y ||
                                  origin.z != m_viewOriginUnexpanded.z);
        bool const deltaChanged = (delta.x != m_viewDeltaUnexpanded.x ||
                                   delta.y != m_viewDeltaUnexpanded.y ||
                                   delta.z != m_viewDeltaUnexpanded.z);
        if (originMoved || deltaChanged)
            m_zClipAdjusted = true;
        m_viewOrigin = origin;   // ViewingSpace.ts:355
        m_viewDelta = delta;     // ViewingSpace.ts:356
    }

    // 1. worldToNpcMap = view.computeWorldToNpc(rotation, ORIGIN, DELTA, !bgMap)
    //    Pass the (possibly z-clip-adjusted) origin/delta (ViewingSpace.ts:358).
    //    DanQing has no background-map z-buffer gate → enforceFrontToBackRatio = true.
    auto const w2n = view.computeWorldToNpc(&m_rotation, &m_viewOrigin, &m_viewDelta, true);
    if (!w2n.map.has_value()) {
        // Invalid frustum (ViewingSpace.ts:359-362): frustFraction = 0, maps collapse.
        m_worldToNpcMap.SetIdentity();
        m_worldToViewMap = calcNpcToView();
        m_frustFraction = 0.0;
        m_worldToNdc = ToColumnMajorFloat(kNdcFromNpc);
        return;
    }
    m_worldToNpcMap.SetFrom(*w2n.map);
    m_frustFraction = w2n.frustFraction;

    // 2. worldToViewMap = calcNpcToView().multiplyMapMap(worldToNpcMap)
    //    (ViewingSpace.ts:366). multiplyMapMap composes matrix0 = this·other;
    //    operator order is load-bearing (npcToView · worldToNpc, not the reverse).
    m_worldToViewMap = calcNpcToView().MultiplyMapMap(m_worldToNpcMap);

    // 3. worldToNdc = NdcFromNpc · worldToNpcMap.transform0 (Decision 1).
    m_worldToNdc = ToColumnMajorFloat(kNdcFromNpc.MultiplyMatrixMatrix(m_worldToNpcMap.Transform0()));
}

// ---------------------------------------------------------------------------
// adjustZPlanes — extend front/back planes to encompass the grid plane.
// Ported from: itwinjs-core ViewingSpace.adjustZPlanes (ViewingSpace.ts:147-245).
// Computes the depth range where the frustum's back→front edges cross the grid
// plane, then sets delta.z to span it — so the (adjusted) frustum brackets the
// ground at every tilt and the grid's frustum∩plane polygon fills the viewport.
// Reads m_rotation / m_viewOrigin / m_viewDelta (un-adjusted) / m_eyePoint.
// ---------------------------------------------------------------------------
void ViewingSpace::adjustZPlanes(dqGeom::Point3d& origin, dqGeom::Vector3d& delta,
                                 ViewState3d const& view)
{
    static double const kMinDepth = 1.0;   // ViewingSpace._minDepth (ViewingSpace.ts:141)

    // TEMP-DIAG（调试取证，env 门控）：adjustZPlanes 分支轨迹。
    static bool const svTrace = [] {
        char const* e = getenv("DANQING_SV_TRACE");
        return e && *e == '1';
    }();

    if (!view.is3d()) return;                                       // :149-150
    delta.z = std::max(delta.z, kMinDepth);                         // :152

    // ViewingSpace.ts:153: extents = this.getViewedExtents().
    dqGeom::Range3d extents = getViewedExtents(view);
    if (svTrace)
        printf("[SVADJ] in org=(%.2f,%.2f,%.2f) delta=(%.2f,%.2f,%.2f) grid=%d bgMap=%d extents=[(%.1f,%.1f,%.1f),(%.1f,%.1f,%.1f)] null=%d\n",
               origin.x, origin.y, origin.z, delta.x, delta.y, delta.z,
               view.getViewFlags().grid() ? 1 : 0, view.getViewFlags().backgroundMap() ? 1 : 0,
               extents.low.x, extents.low.y, extents.low.z,
               extents.high.x, extents.high.y, extents.high.z,
               extents.isNull() ? 1 : 0);

    // Build the RAW frustum in world coords from UN-adjusted m_viewOrigin/m_viewDelta
    // (ViewingSpace.ts:155-161). enforceFrontToBackRatio=false (measurement frustum).
    auto const w2n = view.computeWorldToNpc(&m_rotation, &m_viewOrigin, &m_viewDelta, false);
    if (!w2n.map.has_value()) return;                               // :158-159
    dqCommon::Frustum frustum;
    for (int i = 0; i < dqCommon::kNpcCornerCount; ++i)             // worldToNpc.transform1·NPC cube
        frustum.points[i] = w2n.map->Transform1().MultiplyPoint3dQuietNormalize(frustum.points[i]);

    // ViewingSpace.ts:162-168 — extents-null gate: only extend depth to include
    // viewed geometry if it is within the frustum. When every extent corner is
    // strongly outside the (raw) frustum's range planes, the extents are nulled so
    // the depth fit below uses ONLY the background-map/global-geometry range — the
    // gate that makes tilted globe views (Iso) keep the canonical ellipsoid fit
    // instead of an extents-expanded depth.
    {
        auto clipPlanes = frustum.GetRangePlanes(false, false, 0.0);
        auto const cornersArr = extents.Corners();
        std::vector<dqGeom::Point3d> const viewedExtentCorners(cornersArr.begin(), cornersArr.end());
        auto const cls = clipPlanes.classifyPointContainment(viewedExtentCorners, false);
        if (svTrace)
            printf("[SVADJ] gate frustumPlanes=%zu classification=%d\n",
                   clipPlanes.planes.size(), static_cast<int>(cls));
        if (cls == dqGeom::ClipPlaneContainment::StronglyOutside)
            extents.SetNull();                                      // extents.setNull()
    }

    // Grid plane (ViewingSpace.ts:171-190).
    std::optional<dqGeom::Plane3dByOriginAndUnitNormal> gridPlane;
    if (view.getViewFlags().grid()) {                               // :171
        // TODO: iModel.globalOrigin for spatial views (:172) — DanQing uses world zero.
        dqGeom::Point3d const gridOrigin = dqGeom::Point3d::From(0.0, 0.0, 0.0);
        switch (view.getGridOrientation()) {                        // :173
            case dqCommon::GridOrientationType::WorldXY:            // :174-176
                gridPlane = dqGeom::Plane3dByOriginAndUnitNormal::create(
                    gridOrigin, dqGeom::Vector3d::From(0.0, 0.0, 1.0));
                break;
            case dqCommon::GridOrientationType::WorldYZ:            // :177-179
                gridPlane = dqGeom::Plane3dByOriginAndUnitNormal::create(
                    gridOrigin, dqGeom::Vector3d::From(1.0, 0.0, 0.0));
                break;
            case dqCommon::GridOrientationType::WorldXZ:            // :180-182
                gridPlane = dqGeom::Plane3dByOriginAndUnitNormal::create(
                    gridOrigin, dqGeom::Vector3d::From(0.0, 1.0, 0.0));
                break;
            case dqCommon::GridOrientationType::AuxCoord:           // :184-188
                // TODO: auxiliaryCoordinateSystem.getRotation().rowZ() — no ACS ported;
                // gridPlane stays nullopt, matching the reference when ACS is undefined.
                break;
            case dqCommon::GridOrientationType::View:                // D11 deferred (needs npcToWorld/Map4d)
                // TODO: View orientation plane (npcToWorld(NpcCenter), vp.rotation) —
                // gridPlane stays nullopt; no z-clip adjustment for the View orientation.
                break;
        }
    }

    // Ported from: itwinjs-core ViewingSpace.adjustZPlanes (ViewingSpace.ts:191-213).
    // The background-map (globalGeometry) depth-range branch — the faithful source
    // of the deep, horizon-extending frustum that keeps the sky sphere's
    // u_worldEye non-degenerate when grid is off (DTA default: backgroundMap on,
    // grid off). gridPlane is an OPTIONAL extra accumulator INSIDE this branch
    // (BackgroundMapGeometry.ts:306-308); only when no background map is present
    // does it fall back to the plane-only intersection (:205-206).
    dqGeom::Range1d depthRange;
    auto const globalGeometry = getGlobalGeometryAndHeightRange(
        view.GetIModel(), view.getViewFlags());
    if (globalGeometry.has_value()) {
        dqGeom::Vector3d const viewZ = m_rotation.RowZ();                  // :192-193
        std::optional<double> eyeDepth;                                     // :194
        if (view.IsCameraOn())                                              // this.eyePoint truthy
            eyeDepth = viewZ.DotProduct(m_eyePoint);
        depthRange = globalGeometry->geometry.getFrustumIntersectionDepthRange(
            frustum, extents, globalGeometry->heightRange, gridPlane,
            /*doGlobalScope=*/false);                                       // view.maxGlobalScopeFactor > 1 — TODO
        // :198-204 — cap the front/back ratio (camera-on only) so a far horizon
        // depth can't push the back plane beyond a 1e6 ratio of the front distance.
        if (eyeDepth.has_value()) {
            constexpr double kMaxBackgroundFrontBackRatio = 1.0e6;
            double const frontDist = std::max(0.1, *eyeDepth - depthRange.high);
            double const backDist = *eyeDepth - depthRange.low;
            if (frontDist > 0.0 && backDist / frontDist > kMaxBackgroundFrontBackRatio)
                depthRange.high = *eyeDepth - backDist / kMaxBackgroundFrontBackRatio;
        }
    } else {
        depthRange = gridPlane.has_value()
            ? getFrustumPlaneIntersectionDepthRange(frustum, *gridPlane)
            : dqGeom::Range1d::CreateNull();
    }

    // ViewingSpace.ts:208-213: extend depthRange by the viewed extents' corners
    // (view-z of each). Without this the grid-plane intersection alone can collapse
    // to a single z (top-down) and shrink delta.z to minDepth, clipping the grid.
    if (!extents.isNull()) {
        dqGeom::Vector3d const viewZ = m_rotation.RowZ();
        for (dqGeom::Point3d const& corner : extents.Corners()) {
            double const d = viewZ.DotProduct(corner);
            if (d < depthRange.low) depthRange.low = d;
            if (d > depthRange.high) depthRange.high = d;
        }
    }

    if (depthRange.isNull()) {
        if (svTrace) printf("[SVADJ] depthRange null → no adjustment\n");  // :215-216
        return;
    }
    if (svTrace)
        printf("[SVADJ] depthRange=[%.2f,%.2f] extentsNull=%d → delta.z %.2f\n",
               depthRange.low, depthRange.high, extents.isNull() ? 1 : 0, delta.z);

    // Rotate origin into view coords, set z to the depth range, rotate back (:218-221).
    // (Matrix3d has no in-place vector multiply; round-trip via Vector3d.)
    origin = asPoint(m_rotation.MultiplyVector(dqGeom::Vector3d::From(origin)));
    origin.z = depthRange.low;                                      // :219
    delta.z = std::max(depthRange.high - depthRange.low, kMinDepth);  // :220
    // DanQing guard: cap delta.z so a near-horizon view (whose depth range extends
    // far toward where the ground plane recedes) doesn't produce a frustum so
    // large it overflows the grid geometry (NaN → glDrawElements crash). itwinjs
    // doesn't cap (its grid quantizes positions via QPoint3dList); DanQing's
    // PlanarGridGraphic uses raw float, so it keeps the guard. When clamping,
    // RE-CENTER origin.z on the depth-range midpoint so the frustum still brackets
    // the grid plane (z=0). Without the re-center, the clamp shifts the view-z
    // range to [depthRange.low, depthRange.low+farCap] which no longer contains 0
    // → the grid plane misses the frustum → grid vanishes at near-horizon angles.
    if (!extents.isNull()) {
        double const farCap = extents.Diagonal().Magnitude() * 1000.0;
        if (delta.z > farCap) {
            double const zCenter = (depthRange.low + depthRange.high) * 0.5;
            origin.z = zCenter - farCap * 0.5;   // clamp the depth, keep it centered on z=0's neighborhood
            delta.z = farCap;
        }
    }
    origin = asPoint(m_rotation.MultiplyTransposeVector(dqGeom::Vector3d::From(origin)));

    if (!view.IsCameraOn()) return;                                 // :223-224

    // Camera-on clamps (ViewingSpace.ts:228-244). Not reached by the ortho blank
    // view; m_eyePoint is set (update() precondition).
    dqGeom::Vector3d eyeOrg = dqGeom::Vector3d::FromStartEnd(origin, m_eyePoint);  // eyePoint − origin
    eyeOrg = m_rotation.MultiplyVector(eyeOrg);                     // :230

    if (eyeOrg.z < 1.0) {                                           // :234-240
        origin = asPoint(m_rotation.MultiplyVector(dqGeom::Vector3d::From(origin)));
        origin.z -= (2.0 - eyeOrg.z);
        origin = asPoint(m_rotation.MultiplyTransposeVector(dqGeom::Vector3d::From(origin)));
        delta.z = 1.0;
        return;
    }
    if (delta.z > eyeOrg.z) delta.z = eyeOrg.z;                     // :243-244
}

// ---------------------------------------------------------------------------
// calcNpcToView — the NPC[0,1] → view-pixel Map4d.
// Ported from: itwinjs-core ViewingSpace.calcNpcToView (ViewingSpace.ts:250-256).
// Maps the unit NPC cube to the screen rectangle (getViewCorners).
// ---------------------------------------------------------------------------
dqGeom::Map4d ViewingSpace::calcNpcToView() const
{
    auto const corners = getViewCorners();
    auto const map = dqGeom::Map4d::CreateBoxMap(
        dqGeom::Point3d::From(0.0, 0.0, 0.0),   // NpcCorners[_000]
        dqGeom::Point3d::From(1.0, 1.0, 1.0),   // NpcCorners[_111]
        corners.low, corners.high);
    // ViewingSpace.ts:254-255: undefined map (zero view-rect dimension) → identity.
    return map.has_value() ? *map : dqGeom::Map4d::CreateIdentity();
}

// ---------------------------------------------------------------------------
// getViewCorners — view-coordinate extents as a Range3d.
// Ported from: itwinjs-core ViewingSpace.getViewCorners (ViewingSpace.ts:258-269).
// high.x = viewRect.right (width), low.y = viewRect.bottom (height) — Y is
// flipped on screen. z = ±32767 (the literal integer, NOT 32768/0x7FFF).
// ---------------------------------------------------------------------------
dqGeom::Range3d ViewingSpace::getViewCorners() const
{
    return dqGeom::Range3d(
        dqGeom::Point3d::From(0.0, m_viewH, -32767.0),   // low  (x=0, y=height, z=-32767)
        dqGeom::Point3d::From(m_viewW, 0.0, 32767.0));    // high (x=width, y=0,    z=+32767)
}

// ---------------------------------------------------------------------------
// buildAffineViewMatrix — world → view-local (rotation + −origin).
// DanQing renderer adapter (u_mv uniform). Column-major float[16]: column i holds
// the i-th view axis vector, translation holds −(axis·origin). Maps a world
// point p to (xVec·(p−origin), yVec·(p−origin), zVec·(p−origin)).
// ---------------------------------------------------------------------------
void ViewingSpace::buildAffineViewMatrix(ViewState3d const& view)
{
    auto const& origin = view.GetOrigin();
    auto const& extents = view.GetExtents();
    auto const& rotation = view.getRotation();
    auto const xVec = rotation.RowX();
    auto const yVec = rotation.RowY();
    auto const zVec = rotation.RowZ();

    // Reference point for the translation: the NEAR-plane center, not the view
    // origin. itwinjs-core FrustumUniforms.lookIn(eye=nearCenter, ...) anchors the
    // view transform at the near plane; the DanQing adapter previously anchored at
    // the view origin (the FAR-plane corner of the ortho frustum), so scene points
    // mapped to POSITIVE eye-z (behind the camera) instead of negative (in front) —
    // the reference modelToWindowCoordinates front-clip (`q.z > s_maxZ`) then culled
    // every polyline stroke (ACS triad X/Y arrows + Z stem rendered zero fragments
    // while PointString/Surface via u_mvp=worldToNdc rendered).
    // Ported from: itwinjs-core FrustumUniforms.changeFrustum() ortho branch.
    dqGeom::Point3d const nearCenter = dqGeom::Point3d::From(
        origin.x + xVec.x * (extents.x * 0.5) + yVec.x * (extents.y * 0.5) + zVec.x * extents.z,
        origin.y + xVec.y * (extents.x * 0.5) + yVec.y * (extents.y * 0.5) + zVec.y * extents.z,
        origin.z + xVec.z * (extents.x * 0.5) + yVec.z * (extents.y * 0.5) + zVec.z * extents.z);

    m_viewMatrix[0] = static_cast<float>(xVec.x);
    m_viewMatrix[1] = static_cast<float>(yVec.x);
    m_viewMatrix[2] = static_cast<float>(zVec.x);
    m_viewMatrix[3] = 0.0f;
    m_viewMatrix[4] = static_cast<float>(xVec.y);
    m_viewMatrix[5] = static_cast<float>(yVec.y);
    m_viewMatrix[6] = static_cast<float>(zVec.y);
    m_viewMatrix[7] = 0.0f;
    m_viewMatrix[8] = static_cast<float>(xVec.z);
    m_viewMatrix[9] = static_cast<float>(yVec.z);
    m_viewMatrix[10] = static_cast<float>(zVec.z);
    m_viewMatrix[11] = 0.0f;
    m_viewMatrix[12] = static_cast<float>(
        -(xVec.x * nearCenter.x + xVec.y * nearCenter.y + xVec.z * nearCenter.z));
    m_viewMatrix[13] = static_cast<float>(
        -(yVec.x * nearCenter.x + yVec.y * nearCenter.y + yVec.z * nearCenter.z));
    m_viewMatrix[14] = static_cast<float>(
        -(zVec.x * nearCenter.x + zVec.y * nearCenter.y + zVec.z * nearCenter.z));
    m_viewMatrix[15] = 1.0f;
}

// ---------------------------------------------------------------------------
// ToColumnMajorFloat — row-major Matrix4d (double) → column-major float[16] (GL).
// Matrix4d stores m_coffs[row*4+col]; GL uploads out[col*4+row]. Transpose+cast.
// ---------------------------------------------------------------------------
std::array<float, 16> ViewingSpace::ToColumnMajorFloat(dqGeom::Matrix4d const& m)
{
    std::array<float, 16> out{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            out[col * 4 + row] = static_cast<float>(m.at(row, col));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Coordinate transforms — forwarders to the maps' transform0/1.
// Ported from: itwinjs-core ViewingSpace.worldToView/viewToWorld/worldToNpc/
//               npcToWorld (ViewingSpace.ts:427-457).
// ---------------------------------------------------------------------------
dqGeom::Point3d ViewingSpace::WorldToView(dqGeom::Point3d const& pt) const
{
    return m_worldToViewMap.Transform0().MultiplyPoint3dQuietNormalize(pt);
}

dqGeom::Point3d ViewingSpace::ViewToWorld(dqGeom::Point3d const& pt) const
{
    return m_worldToViewMap.Transform1().MultiplyPoint3dQuietNormalize(pt);
}

dqGeom::Point3d ViewingSpace::WorldToNpc(dqGeom::Point3d const& pt) const
{
    return m_worldToNpcMap.Transform0().MultiplyPoint3dQuietNormalize(pt);
}

dqGeom::Point3d ViewingSpace::NpcToWorld(dqGeom::Point3d const& pt) const
{
    return m_worldToNpcMap.Transform1().MultiplyPoint3dQuietNormalize(pt);
}

// Ported from: itwinjs-core ViewingSpace.npcToView (ViewingSpace.ts:404-407).
// corners.fractionToPoint — interpolate from low to high by (x,y,z).
dqGeom::Point3d ViewingSpace::NpcToView(dqGeom::Point3d const& npc) const
{
    return getViewCorners().FractionToPoint(npc.x, npc.y, npc.z);
}

// Ported from: itwinjs-core ViewingSpace.viewToNpc (ViewingSpace.ts:393-398).
// Transform.initFromRange(low, high, _, globalToNpc) · pt — inverse of npcToView.
dqGeom::Point3d ViewingSpace::ViewToNpc(dqGeom::Point3d const& view) const
{
    auto const corners = getViewCorners();
    dqGeom::Transform scrToNpc;
    dqGeom::Transform::InitFromRange(corners.low, corners.high, nullptr, &scrToNpc);
    return scrToNpc.MultiplyPoint3d(view);
}

// ---------------------------------------------------------------------------
// getFrustum — 8-point frustum in the requested coordinate system.
// Ported from: itwinjs-core ViewingSpace.getFrustum (ViewingSpace.ts:471-503).
// Seeds the NPC cube then converts to View/World/Npc. The reference recomputes
// the unexpanded box when !adjustedBox && zClipAdjusted; DanQing does not expand
// z clip planes (no viewed-extents/tile-tree subsystem), so zClipAdjusted is
// always false and adjustedBox is a no-op (signature parity only).
// ---------------------------------------------------------------------------
void ViewingSpace::getFrustum(dqCommon::Frustum& out, dqCommon::CoordSystem sys,
                              bool adjustedBox) const
{
    out.initNpc();

    // Ported from: itwinjs-core ViewingSpace.getFrustum (ViewingSpace.ts:477-490) —
    // 取未扩展盒（adjustedBox=false 且 zClipAdjusted）：用未扩展的 origin/delta 重算
    // 未扩展视图的 rootToNpc，其反变换把 NPC 立方体映到未扩展根盒（世界坐标），
    // 再经当前 worldToNpc 映到**扩展**视图的 NPC 空间（root-based maps 的基准）。
    // 这是 ViewRotate 等视图工具的工作视锥来源——用扩展（深）视锥做
    // SetupFromFrustum 会把 extents 炸到背景图深锥尺度（2026-09-20 锚定拖动
    // 视口错乱事故的根因，实测 extents 12→3077）。
    if (!adjustedBox && m_zClipAdjusted && m_view) {
        auto const ueRootToNpc = m_view->computeWorldToNpc(
            &m_rotation, &m_viewOriginUnexpanded, &m_viewDeltaUnexpanded);
        if (!ueRootToNpc.map.has_value())
            return;  // :480-482 — invalid frustum: box 停在 NPC 立方体
        // ueRootBox：未扩展盒的世界角点（NPC 立方体经未扩展 npc→world 反变换）
        dqCommon::Frustum ueRootBox;   // 构造即 NPC 立方体（Frustum.ts:79-82）
        for (auto& p : ueRootBox.points)
            p = ueRootToNpc.map->Transform1().MultiplyPoint3dQuietNormalize(p);
        // 映到扩展视图 NPC：覆盖 out 的点
        for (int i = 0; i < dqCommon::kNpcCornerCount; ++i)
            out.points[i] = WorldToNpc(ueRootBox.points[i]);
    }

    switch (sys) {
        case dqCommon::CoordSystem::View:
            // NPC → view pixels (npcToViewArray, ViewingSpace.ts:495).
            for (int i = 0; i < dqCommon::kNpcCornerCount; ++i)
                out.points[i] = NpcToView(out.points[i]);
            break;
        case dqCommon::CoordSystem::World:
            // NPC → world (npcToWorldArray, ViewingSpace.ts:499).
            for (int i = 0; i < dqCommon::kNpcCornerCount; ++i)
                out.points[i] = NpcToWorld(out.points[i]);
            break;
        case dqCommon::CoordSystem::Npc:
        default:
            break;  // leave the NPC cube.
    }
}

// ---------------------------------------------------------------------------
// getPixelSizeAtPoint — world distance spanned by one view-pixel.
// Ported from: itwinjs-core ViewingSpace.getPixelSizeAtPoint (ViewingSpace.ts:505-510).
// Measures viewToWorld(viewPt).distance(viewToWorld(viewPt + 1 view-x)).
// ---------------------------------------------------------------------------
double ViewingSpace::getPixelSizeAtPoint(dqGeom::Point3d const* inPoint) const
{
    dqGeom::Point3d const viewPt = (inPoint != nullptr)
        ? WorldToView(*inPoint)
        : NpcToView(dqGeom::Point3d::From(0.5, 0.5, 0.5));
    dqGeom::Point3d const viewPt2 = dqGeom::Point3d::From(viewPt.x + 1.0, viewPt.y, viewPt.z);
    dqGeom::Point3d const w0 = ViewToWorld(viewPt);
    dqGeom::Point3d const w1 = ViewToWorld(viewPt2);
    return dqGeom::Vector3d::FromStartEnd(w0, w1).Magnitude();
}

}  // namespace dqApp

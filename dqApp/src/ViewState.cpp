// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewState implementation
// Ported from: itwinjs-core core/frontend/src/ViewState.ts + SpatialViewState.ts
#include "dqApp/ViewState.h"

#include "dqApp/tile/SpatialTileTreeReferences.h"
#include "dqApp/IModelConnection.h"
#include "dqApp/StandardView.h"
#include "dqApp/ViewPose.h"
#include "dqApp/Viewport.h"
#include "BackgroundMapGeometry.h"

#include <dqCommon/Npc.h>
#include <dqGeom/Ray3d.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace dqApp {

// ---------------------------------------------------------------------------
// ViewState
// ---------------------------------------------------------------------------
ViewState::ViewState()
    // ← ViewState.ts:307 — this._gridDecorator = new GridDecorator(this)。
    : m_gridDecorator(std::make_unique<GridDecorator>(this))
{
}
ViewState::~ViewState() = default;

// ---------------------------------------------------------------------------
// ViewState::Decorate — add view-specific decorations (the grid)
// Ported from: itwinjs-core ViewState.decorate (ViewState.ts:641-645)。
// ---------------------------------------------------------------------------
void ViewState::Decorate(DecorateContext& context)
{
    drawGrid(context);  // ← ViewState.ts:644
}

// ---------------------------------------------------------------------------
// ViewState::drawGrid — delegate to the cacheable GridDecorator
// Ported from: itwinjs-core ViewState.drawGrid (ViewState.ts:678-680)。
// ---------------------------------------------------------------------------
void ViewState::drawGrid(DecorateContext& context)
{
    context.AddFromDecorator(m_gridDecorator.get());  // ← ViewState.ts:679
}

// ---------------------------------------------------------------------------
// GridDecorator::Decorate — decorate the viewport with the view's grid
// Ported from: itwinjs-core GridDecorator.decorate (ViewState.ts:183-206)。
// ---------------------------------------------------------------------------
void GridDecorator::Decorate(DecorateContext& context)
{
    auto& vp = context.GetViewport();
    if (!vp.isGridOn())                                          // :185
        return;

    auto* view3d = m_view->AsViewState3d();
    auto const orientation = view3d ? view3d->getGridOrientation()
                                    : dqCommon::GridOrientationType::WorldXY;  // :188
    if (dqCommon::GridOrientationType::AuxCoord < orientation) {  // :189-191
        return;  // NEEDSWORK...
    }
    if (dqCommon::GridOrientationType::AuxCoord == orientation) {  // :192-195
        // ← this._view.auxiliaryCoordinateSystem.drawGrid(context)。
        // DanQing 尚无 AuxCoordSystemState 对象——视图的 ACS 恒为世界原点 ACS
        // （origin=(0,0,0)、rotation=I，AcsTriadDecorator 同一假设），故
        // AuxCoordSys.ts:122-127 drawGrid 的实参为：
        dqGeom::Point2d fixedRepsAuto{0.0, 0.0};   // limit grid to project extents (:125)
        context.DrawStandardGrid(dqGeom::Point3d{0.0, 0.0, 0.0}, dqGeom::Matrix3d::CreateIdentity(),
                                 view3d ? view3d->getGridSpacing() : dqGeom::Point2d{1.0, 1.0},
                                 view3d ? static_cast<double>(view3d->getGridsPerRef()) : 10.0,
                                 false, &fixedRepsAuto);
        return;
    }

    bool const isoGrid = false;                                   // :197
    double const gridsPerRef = view3d ? static_cast<double>(view3d->getGridsPerRef()) : 10.0;  // :198
    auto const spacing = view3d ? view3d->getGridSpacing() : dqGeom::Point2d{1.0, 1.0};        // :199
    dqGeom::Point3d origin{0.0, 0.0, 0.0};                        // :200
    dqGeom::Matrix3d matrix = dqGeom::Matrix3d::CreateIdentity();  // :201
    dqGeom::Point2d fixedRepsAuto{0.0, 0.0};                      // :202

    if (view3d)                                                   // :204
        view3d->getGridSettings(vp, origin, matrix, orientation);
    context.DrawStandardGrid(origin, matrix, spacing, gridsPerRef, isoGrid,  // :205
                             orientation != dqCommon::GridOrientationType::View ? &fixedRepsAuto : nullptr);
}

void ViewState::AttachToViewport(Viewport* vp)
{
    if (!vp) return;
    // Avoid duplicate attachment
    for (auto* existing : m_attachedViewports) {
        if (existing == vp) return;
    }
    m_attachedViewports.push_back(vp);
}

void ViewState::DetachFromViewport(Viewport* vp)
{
    auto it = std::find(m_attachedViewports.begin(), m_attachedViewports.end(), vp);
    if (it != m_attachedViewports.end()) {
        m_attachedViewports.erase(it);
    }
}

bool ViewState::IsAttachedToViewport(Viewport* vp) const
{
    for (auto* existing : m_attachedViewports) {
        if (existing == vp) return true;
    }
    return false;
}

void ViewState::NotifyViewportsNeedRedraw()
{
    // Notify all attached viewports that they need to redraw.
    // This is called when origin/extents change.
    // The Viewport will call InvalidateController() + RequestRedraw().
    for (auto* vp : m_attachedViewports) {
        if (vp) {
            vp->InvalidateController();
            vp->RequestRedraw();
        }
    }
}

// ---------------------------------------------------------------------------
// ViewState::hasSameCoordinates
// Ported from: itwinjs-core ViewState.hasSameCoordinates (ViewState.ts:1221-1240).
// ---------------------------------------------------------------------------
bool ViewState::hasSameCoordinates(const ViewState& other) const
{
    if (GetIModel() != other.GetIModel())   // :1222-1223 — 不同 iModel → false
        return false;

    // :1225-1227 — 双 spatial 视图共享同一坐标系（spatial 视图可看任意数量的
    // spatial 模型，全部共享一个坐标系）。
    if (isSpatialView() && other.isSpatialView())
        return true;

    // :1229-1231 — 恰好一个 spatial → false（参考注释：人们有时误把 2d 模型塞进
    // spatial 视图的 model selector）。
    if (isSpatialView() || other.isSpatialView())
        return false;

    // :1233-1239 — 非 spatial 视图经 forEachModel/viewsModel 比较所视模型；
    // DanQing 无 2D 视图且基类无视模型集（forEachModel 未移植），参考对空模型
    // 迭代结果 allowView=false —— 保守返回 false 与参考一致。2D 落地时补模型比较。
    return false;
}

// ---------------------------------------------------------------------------
// ViewState::adjustViewDelta
// Ported from: itwinjs-core ViewState.adjustViewDelta (ViewState.ts:889-920).
// ---------------------------------------------------------------------------
ViewStatus ViewState::adjustViewDelta(dqGeom::Vector3d& delta, dqGeom::Point3d& origin,
                                      dqGeom::Matrix3d const& rot, std::optional<double> aspect,
                                      OnViewExtentsError const* opts)
{
    auto const origDelta = delta;  // :890

    auto status = ViewStatus::Success;
    auto const& limit = m_extentLimits;   // :893 — extentLimits（已解析值）
    auto limitDelta = [&](double val) {   // :894-903
        if (val < limit.min) { val = limit.min; status = ViewStatus::MinWindow; }
        else if (val > limit.max) { val = limit.max; status = ViewStatus::MaxWindow; }
        return val;
    };

    delta.x = limitDelta(delta.x);  // :905
    delta.y = limitDelta(delta.y);  // :906

    if (aspect && *aspect != 0.0) {  // :908 — skip if either undefined or 0
        double const a = *aspect * getAspectRatioSkew();  // :909
        if (delta.x > a * delta.y)   // :910
            delta.y = delta.x / a;   // :911
        else
            delta.x = delta.y * a;   // :913
    }

    // :916-917 — delta 变化时 origin 半移保持中心：
    // origin += 0.5 * rot^T·(origDelta - delta)。参考
    // delta.vectorTo(origDelta, origDelta) = origDelta - delta
    // （core-geometry Point3dVector3d.ts:360-362 "vector from this to other"）。
    if (!delta.AlmostEqual(origDelta)) {
        auto const shift = rot.MultiplyTransposeVector(dqGeom::Vector3d::From(
            origDelta.x - delta.x, origDelta.y - delta.y, origDelta.z - delta.z));
        origin = dqGeom::Point3d::From(origin.x + shift.x * 0.5,
                                       origin.y + shift.y * 0.5,
                                       origin.z + shift.z * 0.5);
    }

    // :919 — 状态非 Success 且有错误回调时经回调（返回值 = 回调返回值）。
    if (status != ViewStatus::Success && opts && opts->onExtentsError)
        return opts->onExtentsError(status);
    return status;
}

// ---------------------------------------------------------------------------
// Clone — deep copy of ViewState base members.
// Ported from: itwinjs-core ViewState.clone()
// Events and attached viewports are NOT cloned.
// ---------------------------------------------------------------------------
dqBase::RefPtr<ViewState> ViewState::Clone() const
{
    auto* raw = new ViewState();
    raw->m_iModel = m_iModel;  // shallow copy (not owned)
    raw->m_displayStyle.setIModel(m_iModel);   // DisplayStyle iModel 绑定随克隆（同 SetIModel）
    raw->m_origin = m_origin;
    raw->m_extents = m_extents;
    raw->m_extentLimits = m_extentLimits;  // ViewState.ts:315 — clone 复制 _extentLimits
    m_displayStyle.cloneDataTo(raw->m_displayStyle);
    // Clone category selector (events are not cloned).
    for (auto const& catId : m_categorySelector.getCategories()) {
        raw->m_categorySelector.addCategory(catId);
    }
    raw->m_description = m_description;
    // m_attachedViewports intentionally empty — clone is not attached
    return dqBase::RefPtr<ViewState>(raw);
}

// ---------------------------------------------------------------------------
// ViewState3d — rotation + camera math
// Ported from: itwinjs-core ViewState.ts ViewState3d class
// ---------------------------------------------------------------------------
ViewState3d::ViewState3d() = default;
ViewState3d::~ViewState3d() = default;

// Ported from: itwinjs-core ViewState3d.savePose (ViewState.ts:1533)。
std::unique_ptr<ViewPose> ViewState3d::savePose() const
{
    return std::make_unique<ViewPose3d>(m_origin, m_extents, m_rotation, m_camera, m_cameraOn);
}

// Ported from: itwinjs-core ViewState3d.applyPose (ViewState.ts:1536-1547)。
void ViewState3d::applyPose(const ViewPose& val)
{
    if (val.getType() != ViewPoseType::View3d)
        return;
    auto const& p3d = static_cast<const ViewPose3d&>(val);
    m_cameraOn = p3d.IsCameraOn();
    SetOrigin(p3d.origin);       // TS: this.setOrigin（含视口重绘通知）
    SetExtents(p3d.extents);     // TS: this.setExtents
    m_rotation = p3d.rotation;   // TS: this.rotation.setFrom（无事件）
    m_camera = p3d.camera;       // TS: this.camera.setFrom
    // TODO: ViewState.ts:1543 的 this._updateMaxGlobalScopeFactor() 未随移植——
    //       maxGlobalScopeFactor 子系统全仓未移植，落地时在此处补调。
}

// Clone — deep copy including rotation and camera.
dqBase::RefPtr<ViewState> ViewState3d::Clone() const
{
    auto* raw = new ViewState3d();
    raw->m_iModel = m_iModel;
    raw->m_displayStyle.setIModel(m_iModel);   // DisplayStyle iModel 绑定随克隆（同 SetIModel）
    raw->m_origin = m_origin;
    raw->m_extents = m_extents;
    raw->m_extentLimits = m_extentLimits;  // ViewState.ts:315
    m_displayStyle.cloneDataTo(raw->m_displayStyle);
    // Clone category selector (events are not cloned).
    for (auto const& catId : m_categorySelector.getCategories()) {
        raw->m_categorySelector.addCategory(catId);
    }
    raw->m_description = m_description;
    raw->m_rotation = m_rotation;
    raw->m_camera = m_camera;
    raw->m_cameraOn = m_cameraOn;
    return dqBase::RefPtr<ViewState>(raw);
}

dqGeom::Point3d ViewState3d::getCenter() const
{
    // Ported from: itwinjs-core ViewState3d.getCenter()
    // center = origin + rotation^T * extents * 0.5
    auto halfExt = dqGeom::Vector3d::From(
        m_extents.x * 0.5, m_extents.y * 0.5, m_extents.z * 0.5);
    auto rotated = m_rotation.MultiplyTransposeVector(halfExt);
    return dqGeom::Point3d::From(
        m_origin.x + rotated.x,
        m_origin.y + rotated.y,
        m_origin.z + rotated.z);
}

// Ported from: itwinjs-core ViewState.setCenter (ViewState.ts:524-527)：
// diff = center - getCenter(); origin += diff。
void ViewState3d::setCenter(dqGeom::Point3d const& center)
{
    auto const c = getCenter();
    auto const o = GetOrigin();
    SetOrigin(dqGeom::Point3d{o.x + (center.x - c.x),
                              o.y + (center.y - c.y),
                              o.z + (center.z - c.z)});
}

// Ported from: itwinjs-core ViewState.getUpVector (ViewState.ts:1246-1255)。
dqGeom::Vector3d ViewState3d::getUpVector(dqGeom::Point3d const& point) const
{
    auto* im = GetIModel();
    if (!im)
        return dqGeom::Vector3d::From(0.0, 0.0, 1.0);  // Vector3d.unitZ()

    // :1247 — !isGeoLocated（ecefLocation undefined/无有效原点）→ unitZ。
    // DanQing 的 EcefLocation 恒为值成员；参考的 "ecefLocation undefined" 对应
    // DanQing 的默认空原点 (0,0,0)（如未传 location 的 BlankConnectionProps）。
    auto const& ecef = im->GetEcefLocation();
    bool const geoLocated = (ecef.origin.x != 0.0 || ecef.origin.y != 0.0 || ecef.origin.z != 0.0);
    // :1247 — globeMode !== GlobeMode.Ellipsoid → unitZ。
    // settings.backgroundMap.globeMode 尚未接入样式图（同 BackgroundMapGeometry.cpp:202
    // 登记）——参考默认即 Ellipsoid（BackgroundMapSettings.ts:38 默认），此处取默认。
    // :1248 — projectExtents.containsPoint(point) → unitZ。
    if (!geoLocated || im->GetProjectExtents().ContainsPoint(point))
        return dqGeom::Vector3d::From(0.0, 0.0, 1.0);

    // :1250 — earthCenter = iModel.getMapEcefToDb(0).origin
    // getMapEcefToDb(0) = ecefLocation.getTransform().inverse()（IModelConnection.ts:571-583，
    // origin.z += 0）——Transform(origin, orientation).inverse().origin = -(orientationᵀ·ecefOrigin)。
    auto const t = ecef.orientation.MultiplyTransposeVector(
        dqGeom::Vector3d::From(ecef.origin.x, ecef.origin.y, ecef.origin.z));
    dqGeom::Point3d const earthCenter{-t.x, -t.y, -t.z};

    // :1251-1253 — normal = normalize(point - earthCenter)。
    auto normal = dqGeom::Vector3d::FromStartEnd(earthCenter, point);
    normal.Normalize();
    return normal;
}

// Ported from: itwinjs-core ViewState3d.getEarthFocalPoint (ViewState.ts:2272-2293)。
std::optional<dqGeom::Point3d> ViewState3d::getEarthFocalPoint() const
{
    // :2273 — !iModel.ecefLocation || globeMode !== GlobeMode.Ellipsoid → undefined。
    auto* im = GetIModel();
    if (!im)
        return std::nullopt;
    auto const& ecef = im->GetEcefLocation();
    bool const geoLocated = (ecef.origin.x != 0.0 || ecef.origin.y != 0.0 || ecef.origin.z != 0.0);
    // globeMode（ViewDetails3d → backgroundMapSettings.globeMode）样式图未接入——
    // 取参考默认 Ellipsoid（BackgroundMapSettings.ts:38），与 ViewState.cpp:305 约定一致。
    if (!geoLocated)
        return std::nullopt;

    // :2276-2278 — displayStyle.getBackgroundMapGeometry()（门槛仅 _hasEarthLocation，
    // 不要求 backgroundMap 可见性）。DanQing 未在样式层缓存——按同参数现构
    // （bimElevationBias=0、globeMode=Ellipsoid 默认，同 getGlobalGeometryAndHeightRange）。
    dqApp::BackgroundMapGeometry const backgroundMapGeometry(
        0.0, dqCommon::GlobeMode::Ellipsoid, im);
    auto const earthEllipsoid = backgroundMapGeometry.getEarthEllipsoid();  // :2280
    auto const viewZ = getRotation().RowZ();                                // :2281
    auto const center = getCenter();                                        // :2282
    // :2283 — eye = isCameraOn ? camera.getEyePoint()
    //             : center.plusScaled(viewZ, Constant.diameterOfEarth)
    constexpr double kDiameterOfEarth = 12742.0 * 1000.0;  // Constant.diameterOfEarth (Constant.ts:21)
    dqGeom::Point3d const eye = m_cameraOn
        ? getEyePoint()
        : dqGeom::Point3d::From(center.x + viewZ.x * kDiameterOfEarth,
                                center.y + viewZ.y * kDiameterOfEarth,
                                center.z + viewZ.z * kDiameterOfEarth);
    dqGeom::Ray3d const eyeRay = dqGeom::Ray3d::create(eye, viewZ);         // :2284
    std::vector<double> fractions;
    std::vector<dqGeom::Point3d> points;
    size_t const nHits = earthEllipsoid.intersectRay(eyeRay, &fractions, &points, nullptr);
    if (nHits > 0) {                                                        // :2286
        double fraction = -1.0e10;
        int index = -1;
        for (size_t i = 0; i < fractions.size(); ++i) {                     // :2287-2290
            if (fractions[i] > fraction) {
                fraction = fractions[i];
                index = static_cast<int>(i);
            }
        }
        // :2291 — (index >= 0 && fraction < 0) ? points[index] : undefined
        if (index >= 0 && fraction < 0.0)
            return points[static_cast<size_t>(index)];
        return std::nullopt;
    }
    // :2292-2293 — else 分支：eyeRay.projectPointToRay(center)
    double projectFraction = 0.0;
    dqGeom::Point3d closest;
    eyeRay.TryProjectToRay(center, projectFraction, closest);
    return closest;
}

dqGeom::Point3d ViewState3d::GetTargetPoint() const
{
    // Ported from: itwinjs-core ViewState3d.getTargetPoint (ViewState.ts:1877-1884)
    if (!m_cameraOn) {                                                      // :1878-1880
        auto const earthFocalPoint = getEarthFocalPoint();
        if (earthFocalPoint.has_value())
            return *earthFocalPoint;
        return getCenter();                                                 // super.getTargetPoint()
    }
    // :1883 — target = eye + zVec * (-focusDist)
    if (m_cameraOn) {
        auto zVec = GetZVec();
        auto const& eye = m_camera.getEyePoint();
        double fd = m_camera.getFocusDistance();
        return dqGeom::Point3d::From(
            eye.x + zVec.x * (-fd),
            eye.y + zVec.y * (-fd),
            eye.z + zVec.z * (-fd));
    }
    return getCenter();
}

// ===========================================================================
// 相机开启链（lookAt 家族）— 1:1 移植
// Ported from: itwinjs-core ViewState3d（ViewState.ts:1813-2170）
// ===========================================================================
namespace {
constexpr double kOneMillimeter = 0.001;   // Constant.oneMillimeter
constexpr double kOneCentimeter = 0.01;    // Constant.oneCentimeter
constexpr double kOneMeter = 1.0;          // Constant.oneMeter
constexpr double kLookAtPi = 3.14159265358979323846;
}  // namespace

// Ported from: itwinjs-core ViewState3d.calculateMaxDepth (:1813-1820)。
double ViewState3d::calculateMaxDepth(dqGeom::Vector3d const& delta, dqGeom::Vector3d const& zVec)
{
    constexpr double kDepthRatioLimit = 1.0e8;      // Limit for depth Ratio.
    constexpr double kMaxTransformRowRatio = 1.0e5;
    double const minXYComponent = std::min(std::abs(zVec.x), std::abs(zVec.y));
    double const maxDepthRatio = (0.0 == minXYComponent)
        ? kDepthRatioLimit
        : std::min(kMaxTransformRowRatio / minXYComponent, kDepthRatioLimit);
    return std::max(delta.x, delta.y) * maxDepthRatio;
}

// Ported from: itwinjs-core ViewState3d.enableCamera (:1828-1831)。
void ViewState3d::enableCamera()
{
    if (supportsCamera())
        m_cameraOn = true;
}

// Ported from: itwinjs-core ViewState3d.supportsCamera (:1833-1835)。
bool ViewState3d::supportsCamera() const
{
    return Allow3dManipulations();
}

// Ported from: itwinjs-core ViewState3d.calcLensAngle (:1871-1873)
//   Angle.createRadians(2.0 * Math.atan2(this.extents.x * 0.5, focusDist))
double ViewState3d::calcLensAngle() const
{
    return 2.0 * std::atan2(m_extents.x * 0.5, m_camera.getFocusDistance());
}

// Ported from: itwinjs-core ViewState3d.minimumFrontDistance (:1837-1838)
//   Math.max(15.2 * Constant.oneCentimeter, this.forceMinFrontDist)
double ViewState3d::minimumFrontDistance() const
{
    return std::max(15.2 * kOneCentimeter, forceMinFrontDist);
}

// Ported from: itwinjs-core ViewState3d.getFrontDistance (:2096)。
double ViewState3d::getFrontDistance() const
{
    return getBackDistance() - m_extents.z;
}

// Ported from: itwinjs-core ViewState3d.getBackDistance (:2099-2105)：
// backDist = (origin→eye 向量经 rotation 旋转后的 z 分量)。
double ViewState3d::getBackDistance() const
{
    auto eyeOrg = dqGeom::Vector3d::FromStartEnd(m_origin, m_camera.getEyePoint());
    auto const rotated = m_rotation.MultiplyVector(eyeOrg);
    return rotated.z;
}

// Ported from: itwinjs-core ViewState3d.centerEyePoint (:2111-2115)：
//   eyePoint = extents.scale(0.5)；eyePoint.z = backDistance ?? getBackDistance()；
//   eye = origin + rotationᵀ·eyePoint。
void ViewState3d::centerEyePoint(std::optional<double> backDistance)
{
    dqGeom::Point3d eyePoint = dqGeom::Point3d::From(
        m_extents.x * 0.5, m_extents.y * 0.5, 0.0);
    eyePoint.z = backDistance ? *backDistance : getBackDistance();
    auto const rotated = m_rotation.MultiplyTransposeVector(
        dqGeom::Vector3d::From(eyePoint.x, eyePoint.y, eyePoint.z));
    m_camera.setEyePoint(dqGeom::Point3d::From(
        m_origin.x + rotated.x, m_origin.y + rotated.y, m_origin.z + rotated.z));
}

// Ported from: itwinjs-core ViewState3d.verifyFocusPlane (:2131-2166)。
void ViewState3d::verifyFocusPlane()
{
    if (!m_cameraOn)
        return;

    double backDist = getBackDistance();
    double const frontDist = backDist - m_extents.z;

    if (backDist <= 0.0 || frontDist <= 0.0) {
        // 相机位置非法——按视域范围重建（:2140-2146）。
        double const tanAngle = std::tan(m_camera.getLensRadians() / 2.0);
        backDist = m_extents.z / tanAngle;
        m_camera.setFocusDistance(backDist / 2.0);
        centerEyePoint(backDist);
        return;
    }

    double const focusDist = m_camera.getFocusDistance();
    if (focusDist > frontDist && focusDist < backDist)
        return;  // :2153-2155 焦平面已在前/后平面之间

    // 居中到前/后平面中点（:2157-2158）。
    m_camera.setFocusDistance(m_extents.z / 2.0 + frontDist);

    // 焦平面移动 → origin/delta（位于焦平面上）按比例调整，并使相机居中
    //（:2160-2165）：ratio = 新/旧 focusDist；extents.x/y *= ratio；
    // origin = eye + rowZ·(-backDist) + rowX·(-extents.x/2) + rowY·(-extents.y/2)。
    double const ratio = m_camera.getFocusDistance() / focusDist;
    m_extents.x *= ratio;
    m_extents.y *= ratio;
    auto const rowZ = m_rotation.RowZ();
    auto const rowX = m_rotation.RowX();
    auto const rowY = m_rotation.RowY();
    auto const& eye = m_camera.getEyePoint();
    m_origin = dqGeom::Point3d::From(
        eye.x + rowZ.x * -backDist + rowX.x * -0.5 * m_extents.x + rowY.x * -0.5 * m_extents.y,
        eye.y + rowZ.y * -backDist + rowX.y * -0.5 * m_extents.x + rowY.y * -0.5 * m_extents.y,
        eye.z + rowZ.z * -backDist + rowX.z * -0.5 * m_extents.x + rowY.z * -0.5 * m_extents.y);
}

// Ported from: itwinjs-core ViewState3d.getEyeOrOrthographicViewPoint (:2148-2159)。
dqGeom::Point3d ViewState3d::getEyeOrOrthographicViewPoint()
{
    if (m_cameraOn)
        return m_camera.getEyePoint();

    m_camera.validateLens();
    double const tanHalfAngle = std::tan(m_camera.getLensRadians() / 2.0);
    double const halfDelta = m_extents.MagnitudeXY();
    double const eyeDistance = (tanHalfAngle != 0.0) ? (halfDelta / tanHalfAngle) : 0.0;
    auto const zVector = m_rotation.RowZ();
    auto const c = getCenter();
    return dqGeom::Point3d::From(c.x + zVector.x * -eyeDistance,
                                 c.y + zVector.y * -eyeDistance,
                                 c.z + zVector.z * -eyeDistance);
}

// Ported from: itwinjs-core ViewState3d.rootToCartographic (:1602-1606)：
// displayStyle.getBackgroundMapGeometry()?.dbToCartographic(root)。DanQing 未在
// 样式层缓存——按同参数现构（bimElevationBias=0、globeMode=Ellipsoid 默认，
// 同 getEarthFocalPoint 的约定）。
std::optional<dqCommon::Cartographic> ViewState3d::rootToCartographic(
    dqGeom::Point3d const& root) const
{
    auto* im = GetIModel();
    if (!im)
        return std::nullopt;
    auto const& ecef = im->GetEcefLocation();
    bool const geoLocated =
        (ecef.origin.x != 0.0 || ecef.origin.y != 0.0 || ecef.origin.z != 0.0);
    if (!geoLocated)
        return std::nullopt;
    dqApp::BackgroundMapGeometry const backgroundMapGeometry(
        0.0, dqCommon::GlobeMode::Ellipsoid, im);
    return backgroundMapGeometry.dbToCartographic(root);
}

// Ported from: itwinjs-core ViewState3d.cartographicToRoot (:1611-1614)。
dqGeom::Point3d ViewState3d::cartographicToRoot(
    dqCommon::Cartographic const& cartographic) const
{
    auto* im = GetIModel();
    if (!im)
        return dqGeom::Point3d::FromZero();
    dqApp::BackgroundMapGeometry const backgroundMapGeometry(
        0.0, dqCommon::GlobeMode::Ellipsoid, im);
    return backgroundMapGeometry.cartographicToDb(cartographic);
}

// Ported from: itwinjs-core ViewState3d.lookAt (:1891-1990)。
ViewStatus ViewState3d::lookAt(LookAtArgs const& argsIn)
{
    LookAtArgs args = argsIn;

    // :1892-1907 —— lensAngle 变体：以镜头角重定 newExtents 的宽度（等比）。
    if (args.lensAngleRadians) {
        double const lensAngle = *args.lensAngleRadians;
        double const focus = dqGeom::Vector3d::FromStartEnd(
            args.eyePoint,
            args.targetPoint.value_or(args.eyePoint)).Magnitude();  // Set focus at target point
        if (focus <= kOneMillimeter)      // eye and target are too close together
            return ViewStatus::InvalidTargetPoint;
        if (lensAngle < 0.0001 || lensAngle > kLookAtPi)
            return ViewStatus::InvalidLens;

        double const width = 2.0 * std::tan(lensAngle / 2.0) * focus;
        dqGeom::Point2d newExtents = args.newExtents.value_or(
            dqGeom::Point2d{m_extents.x, m_extents.y});
        double const scaleFactor = width / newExtents.x;
        newExtents = dqGeom::Point2d{newExtents.x * scaleFactor, newExtents.y * scaleFactor};
        args.newExtents = newExtents;
    }

    bool const isPerspective = args.targetPoint.has_value();     // :1909
    if (isPerspective && !supportsCamera())                      // :1910-1911
        return ViewStatus::NotCameraView;

    dqGeom::Point3d eye = args.eyePoint;  // :1913（可变——adjustViewDelta 引用调整）
    dqGeom::Vector3d yVec = args.upVector;
    if (yVec.Normalize() == 0.0)                                 // :1914-1916 up 零长
        return ViewStatus::InvalidUpVector;

    double const minFrontDist = minimumFrontDistance();          // :1918
    dqGeom::Vector3d zVec;
    double focusDist;
    if (args.targetPoint) {                                      // :1920-1927
        zVec = dqGeom::Vector3d::FromStartEnd(*args.targetPoint, eye);  // eye→target 反向
        focusDist = zVec.Normalize();                            // set focus at target point
        if (focusDist <= minFrontDist) {                         // too close together
            if (args.opts && args.opts->onExtentsError)
                args.opts->onExtentsError(ViewStatus::InvalidTargetPoint);
            return ViewStatus::InvalidTargetPoint;
        }
    } else {                                                     // :1928-1933
        zVec = args.viewDirection.value_or(dqGeom::Vector3d::From(0, 0, 0));
        zVec.Negate();
        if (zVec.Normalize() == 0.0)
            return ViewStatus::InvalidDirection;
        focusDist = getFocusDistance();
    }

    // :1935-1941 —— 行向量正交化：|y×z| 与 |z×x| 过小（up 平行视线）均失败。
    dqGeom::Vector3d xVec;
    xVec.CrossProduct(yVec, zVec);
    if (xVec.Magnitude() < dqGeom::kSmallMetricDistance)
        return ViewStatus::InvalidUpVector;                      // up is parallel to z
    xVec.Normalize();
    dqGeom::Vector3d yVec2;
    yVec2.CrossProduct(zVec, xVec);
    if (yVec2.Magnitude() < dqGeom::kSmallMetricDistance)
        return ViewStatus::InvalidUpVector;
    yVec2.Normalize();
    yVec = yVec2;

    // :1943 —— 旋转矩阵三行。
    dqGeom::Matrix3d const rotation = dqGeom::Matrix3d::CreateRowValues(
        xVec.x, xVec.y, xVec.z,
        yVec.x, yVec.y, yVec.z,
        zVec.x, zVec.y, zVec.z);

    double backDist = args.backDistance ? *args.backDistance : getBackDistance();   // :1945
    double frontDist = args.frontDistance ? *args.frontDistance : getFrontDistance(); // :1946

    // :1948 —— newExtents 给 x/y（取绝对值），z 保持现值。
    dqGeom::Vector3d delta = args.newExtents
        ? dqGeom::Vector3d::From(std::abs(args.newExtents->x), std::abs(args.newExtents->y), m_extents.z)
        : m_extents;

    // :1951-1953 —— 前/后距离的合理化（frustum 之后还会被调整以包含几何）。
    frontDist = std::min(frontDist, 0.5 * kOneMeter);
    backDist = std::min(backDist, focusDist + 0.5 * kOneMeter);
    if (backDist < focusDist)          // focus 必须在后平面之前
        backDist = focusDist + kOneMillimeter;
    if (frontDist > focusDist)
        frontDist = focusDist - minFrontDist;
    if (frontDist < minFrontDist)
        frontDist = minFrontDist;

    delta.z = backDist - frontDist;    // :1961

    // :1963-1966
    ViewStatus const stat = adjustViewDelta(delta, eye, rotation, std::nullopt, args.opts);
    if (stat != ViewStatus::Success)
        return stat;

    if (delta.z > calculateMaxDepth(delta, zVec))  // :1969 zoomed out too far
        return ViewStatus::MaxDisplayDepth;

    // :1972-1973 —— origin = eye + zVec·(-backDist) + xVec·(-delta.x/2) + yVec·(-delta.y/2)
    //（焦平面视域左下角投影到后平面）。
    dqGeom::Point3d const origin = dqGeom::Point3d::From(
        eye.x + zVec.x * -backDist + xVec.x * -0.5 * delta.x + yVec.x * -0.5 * delta.y,
        eye.y + zVec.y * -backDist + xVec.y * -0.5 * delta.x + yVec.y * -0.5 * delta.y,
        eye.z + zVec.z * -backDist + xVec.z * -0.5 * delta.x + yVec.z * -0.5 * delta.y);

    setEyePoint(args.eyePoint);        // :1977（参考 :2179 setEyePoint）
    SetRotation(rotation);             // :1978
    setFocusDistance(focusDist);       // :1979
    SetOrigin(origin);                 // :1980
    SetExtents(delta);                 // :1981
    SetLensAngle(calcLensAngle());     // :1982
    if (isPerspective)                 // :1983-1986
        enableCamera();
    else
        TurnCameraOff();
    // TODO: ViewState.ts:1988 的 this._updateMaxGlobalScopeFactor() 未随移植
    //       （同 applyPose 登记）——maxGlobalScopeFactor 子系统全仓未移植。
    NotifyViewportsNeedRedraw();
    return ViewStatus::Success;
}

// Ported from: itwinjs-core ViewState3d.lookAtGlobalLocation (:1637-1654)。
double ViewState3d::lookAtGlobalLocation(double eyeHeight, double pitchAngleRadians,
                                         std::optional<GlobalLocation> location,
                                         dqGeom::Point3d const* eyePoint)
{
    auto* im = GetIModel();
    if (!im)
        return 0.0;
    auto const& ecef = im->GetEcefLocation();
    bool const geoLocated =
        (ecef.origin.x != 0.0 || ecef.origin.y != 0.0 || ecef.origin.z != 0.0);
    if (!geoLocated)                    // :1638-1639 !iModel.isGeoLocated
        return 0.0;

    // :1641-1642 location.area 分支（areaToEyeHeight，ViewGlobalLocation.ts:88）
    // 未移植——依赖 GCS 面积换算；测试与本场景 area 均为 undefined（TODO 登记）。

    dqGeom::Point3d const origEyePoint =
        eyePoint ? *eyePoint : getEyeOrOrthographicViewPoint();  // :1644

    // :1646-1652 —— target = 定位中心（或 root→carto）置 height 0 落地；
    // eye = 同一 cartographic 升到 eyeHeight。
    dqGeom::Point3d targetPoint = origEyePoint;
    dqCommon::Cartographic targetPointCartographic =
        location ? location->center : rootToCartographic(targetPoint).value_or(
                                                dqCommon::Cartographic{});
    targetPointCartographic.height = 0.0;
    targetPoint = cartographicToRoot(targetPointCartographic);

    targetPointCartographic.height = eyeHeight;
    dqGeom::Point3d const lEyePoint = cartographicToRoot(targetPointCartographic);

    return finishLookAtGlobalLocation(targetPointCartographic, origEyePoint, lEyePoint,
                                      targetPoint, pitchAngleRadians);
}

// Ported from: itwinjs-core ViewState3d.finishLookAtGlobalLocation (:1683-1710)。
double ViewState3d::finishLookAtGlobalLocation(
    dqCommon::Cartographic targetPointCartographic, dqGeom::Point3d origEyePoint,
    dqGeom::Point3d eyePoint, dqGeom::Point3d targetPoint, double pitchAngleRadians)
{
    // :1684 —— 北向：latitude + .001 弧度处的地表点。
    targetPointCartographic.latitude += 0.001;
    auto const northOfEyePoint = cartographicToRoot(targetPointCartographic);
    dqGeom::Vector3d upVector =
        dqGeom::Vector3d::FromStartEnd(targetPoint, northOfEyePoint);
    upVector.Normalize();  // targetPoint.unitVectorTo(northOfEyePoint)

    // :1687-1688 —— globeMode Plane 分支（up 取分量绝对值）未接线：样式图未接，
    // 恒 Ellipsoid（TODO 登记，同 ViewState.cpp 多处约定）。

    if (0.0 != pitchAngleRadians) {  // :1690-1700
        dqGeom::Vector3d pitchAxis;
        pitchAxis.CrossProduct(
            upVector, dqGeom::Vector3d::FromStartEnd(targetPoint, eyePoint));
        if (pitchAxis.Normalize() != 0.0) {  // unitCrossProduct 成功
            auto const pitchMatrix = dqGeom::Matrix3d::CreateRotationAroundAxis(
                pitchAxis, pitchAngleRadians);
            auto const pitchTransform =
                dqGeom::Transform::CreateFixedPointAndMatrix(targetPoint, pitchMatrix);
            eyePoint = pitchTransform.MultiplyPoint3d(eyePoint);
            upVector = pitchMatrix.MultiplyVector(upVector);
        }
    }

    bool const isCameraEnabled = m_cameraOn;  // :1702
    LookAtArgs args;                          // :1703 lookAt({...})
    args.eyePoint = eyePoint;
    args.targetPoint = targetPoint;
    args.upVector = upVector;
    args.lensAngleRadians = m_camera.getLensRadians();
    lookAt(args);
    if (!isCameraEnabled && m_cameraOn)       // :1704-1705 保持相机开关状态
        TurnCameraOff();

    return eyePoint.Distance(origEyePoint);   // :1707
}

void ViewState3d::SetupView(dqGeom::Point3d const& eye,
                             dqGeom::Point3d const& target,
                             dqGeom::Vector3d const& up)
{
    // Ported from: itwinjs-core ViewState3d.lookAt() — simplified version
    // Build orthonormal rotation from eye → target direction and up vector.
    auto fwd = dqGeom::Vector3d::From(
        target.x - eye.x, target.y - eye.y, target.z - eye.z);
    double fwdLen = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
    if (fwdLen < 1e-12)
        return;

    // zVec points from target toward eye (opposite of look direction)
    auto zVec = dqGeom::Vector3d::From(-fwd.x / fwdLen, -fwd.y / fwdLen, -fwd.z / fwdLen);

    // xVec = up × zVec (right vector)
    auto xVec = dqGeom::Vector3d::From(
        up.y * zVec.z - up.z * zVec.y,
        up.z * zVec.x - up.x * zVec.z,
        up.x * zVec.y - up.y * zVec.x);
    double xLen = std::sqrt(xVec.x * xVec.x + xVec.y * xVec.y + xVec.z * xVec.z);
    if (xLen < 1e-12) {
        // up and look direction are parallel, pick arbitrary right vector
        xVec = dqGeom::Vector3d::From(1, 0, 0);
        if (std::abs(zVec.x) > 0.9)
            xVec = dqGeom::Vector3d::From(0, 1, 0);
        xLen = 1.0;
    }
    xVec.x /= xLen; xVec.y /= xLen; xVec.z /= xLen;

    // yVec = zVec × xVec (up vector in view frame)
    auto yVec = dqGeom::Vector3d::From(
        zVec.y * xVec.z - zVec.z * xVec.y,
        zVec.z * xVec.x - zVec.x * xVec.z,
        zVec.x * xVec.y - zVec.y * xVec.x);

    // Rotation rows: xVec, yVec, zVec
    m_rotation = dqGeom::Matrix3d::CreateRowValues(
        xVec.x, xVec.y, xVec.z,
        yVec.x, yVec.y, yVec.z,
        zVec.x, zVec.y, zVec.z);

    // Set camera parameters
    m_camera.setEyePoint(eye);
    m_camera.setFocusDistance(fwdLen);
    m_cameraOn = true;

    // Compute origin from eye position
    // origin = eye + zVec * (-backDist) + xVec * (-0.5 * extents.x) + yVec * (-0.5 * extents.y)
    double backDist = m_extents.z * 0.5;
    m_origin = dqGeom::Point3d::From(
        eye.x + zVec.x * (-backDist) + xVec.x * (-0.5 * m_extents.x) + yVec.x * (-0.5 * m_extents.y),
        eye.y + zVec.y * (-backDist) + xVec.y * (-0.5 * m_extents.x) + yVec.y * (-0.5 * m_extents.y),
        eye.z + zVec.z * (-backDist) + xVec.z * (-0.5 * m_extents.x) + yVec.z * (-0.5 * m_extents.y));
}

void ViewState3d::LookAtVolume(dqGeom::Range3d const& volume,
                               double const* aspect,
                               MarginOptions const* options)
{
    // Ported from: itwinjs-core ViewState3d.lookAtVolume (ViewState.ts:1083) ->
    //               lookAtViewAlignedVolume (ViewState.ts:1096-1177).
    // Rotate the 8 world corners of `volume` by the view rotation into a view-axis-
    // aligned range, then fit the view to it. (Rotation only — no origin subtract;
    // the origin offset is re-introduced when newOrigin is rotated back to world at
    // ViewState.ts:1153.) aspect/options default to null -> default x1.04 dilation.
    // adjustViewDelta itself IS ported (ViewState.cpp above, ViewState.ts:889-920 —
    // aspect branch included); the clamp CALL SITE (ViewState.ts:1159) +
    // verifyFocusPlane remain unwired below.

    dqGeom::Point3d corners[8] = {
        dqGeom::Point3d::From(volume.low.x, volume.low.y, volume.low.z),
        dqGeom::Point3d::From(volume.high.x, volume.low.y, volume.low.z),
        dqGeom::Point3d::From(volume.low.x, volume.high.y, volume.low.z),
        dqGeom::Point3d::From(volume.high.x, volume.high.y, volume.low.z),
        dqGeom::Point3d::From(volume.low.x, volume.low.y, volume.high.z),
        dqGeom::Point3d::From(volume.high.x, volume.low.y, volume.high.z),
        dqGeom::Point3d::From(volume.low.x, volume.high.y, volume.high.z),
        dqGeom::Point3d::From(volume.high.x, volume.high.y, volume.high.z),
    };
    double minVx = 1e18, minVy = 1e18, minVz = 1e18;
    double maxVx = -1e18, maxVy = -1e18, maxVz = -1e18;
    for (int i = 0; i < 8; ++i) {
        auto v = m_rotation.MultiplyVector(dqGeom::Vector3d::From(corners[i].x, corners[i].y, corners[i].z));
        minVx = std::min(minVx, v.x); maxVx = std::max(maxVx, v.x);
        minVy = std::min(minVy, v.y); maxVy = std::max(maxVy, v.y);
        minVz = std::min(minVz, v.z); maxVz = std::max(maxVz, v.z);
    }

    // ViewState.ts:1046-1047
    dqGeom::Vector3d newDelta = dqGeom::Vector3d::From(maxVx - minVx, maxVy - minVy, maxVz - minVz);
    dqGeom::Point3d newOrigin = dqGeom::Point3d::From(minVx, minVy, minVz);

    // ViewState.ts:1049-1053  enforce minimum depth (Constant.oneMillimeter ~= 0.001).
    //（常量提升至文件级 anon namespace——lookAt 家族共用。）
    if (newDelta.z < kOneMillimeter) {
        newOrigin.z -= (kOneMillimeter - newDelta.z) * 0.5;
        newDelta.z = kOneMillimeter;
    }

    // ViewState.ts:1055-1100 — pick the dilation/margin/padding branch.
    if (is3d() && IsCameraOn()) {
        // ViewState.ts:1055-1058 — camera on: apply no padding/margin (can't tell whether
        // objects are at the front or back of the view, so a margin would be wrong).
    } else if (options != nullptr && options->hasPaddingPercent()) {
        // ViewState.ts:1059-1077 — paddingPercent (uniform number or per-side PaddingPercent).
        double left, right, top, bottom;
        if (options->paddingPercentUniform.has_value()) {
            left = right = top = bottom = *options->paddingPercentUniform;
        } else {
            PaddingPercent const& p = *options->paddingPercent;
            left = p.left; right = p.right; top = p.top; bottom = p.bottom;
        }
        double const width = newDelta.x;
        double const height = newDelta.y;
        newOrigin.x -= left * width;
        newDelta.x += (right + left) * width;
        newOrigin.y -= bottom * height;
        newDelta.y += (top + bottom) * height;
    } else if (options != nullptr && options->marginPercent.has_value()) {
        // ViewState.ts:1078-1095 — marginPercent.
        MarginPercent const& m = *options->marginPercent;
        double const wPercent = m.left + m.right;
        double const hPercent = m.top + m.bottom;
        double const marginHorizontal = wPercent / (1.0 - wPercent) * newDelta.x;
        double const marginVert = hPercent / (1.0 - hPercent) * newDelta.y;
        double const marginLeft = m.left / (1.0 - wPercent) * newDelta.x;
        double const marginBottom = m.bottom / (1.0 - hPercent) * newDelta.y;
        newOrigin.x -= marginLeft;
        newOrigin.y -= marginBottom;
        newDelta.x += marginHorizontal;
        newDelta.y += marginVert;
    } else {
        // ViewState.ts:1096-1100 — default dilation x1.04; re-center origin by the
        // half-offset so the volume stays centered.
        dqGeom::Vector3d origDelta = newDelta;
        newDelta = dqGeom::Vector3d::From(newDelta.x * 1.04, newDelta.y * 1.04, newDelta.z * 1.04);
        newOrigin = dqGeom::Point3d::From(
            newOrigin.x + (origDelta.x - newDelta.x) * 0.5,
            newOrigin.y + (origDelta.y - newDelta.y) * 0.5,
            newOrigin.z + (origDelta.z - newDelta.z) * 0.5);
    }

    // ViewState.ts:1157  viewRot.multiplyTransposeVectorInPlace(newOrigin) -> world.
    auto const originWorldVec = m_rotation.MultiplyTransposeVector(
        dqGeom::Vector3d::From(newOrigin.x, newOrigin.y, newOrigin.z));
    dqGeom::Point3d originWorld =
        dqGeom::Point3d::From(originWorldVec.x, originWorldVec.y, originWorldVec.z);

    // ViewState.ts:1159 — adjustViewDelta(viewDelta, newOrigin, viewRot, aspect, options)：
    // extent-limit clamp + 纵横比分支（aspect=undefined/1.0 且方形 delta 时为 no-op；
    // 参考的 aspect 用例与 padding {right:0.5}/{left:1} 非方形用例都靠它收敛）。
    // 参考在此返回非 Success 时提前 return status；DanQing LookAtVolume 返回 void，
    // blank 场景的适配体积恒在默认限制内（状态仅 MinWindow/MaxWindow）——
    // onExtentsError 回调未随 MarginOptions 移植，传 nullptr。
    {
        ViewStatus const status = adjustViewDelta(
            newDelta, originWorld, m_rotation,
            aspect ? std::optional<double>(*aspect) : std::nullopt, nullptr);
        (void)status;
    }

    // ViewState.ts:1162-1163
    m_extents = newDelta;
    m_origin = dqGeom::Point3d::From(originWorld.x, originWorld.y, originWorld.z);

    if (!is3d())
        return;

    // ViewState.ts:1168-1176  camera placement (runs even with the camera off — the
    // focus/eye fields are used when the camera turns on).
    m_camera.validateLens();  // ViewState.ts:1169
    // ViewState.ts:1171 — frontDist = delta.x / (2*tan(lens/2)). validateLens() above
    // guarantees lens ∈ (π/8, π), so tan(lens/2) is well-defined; no clamp guard (audit D3.3).
    double frontDist = newDelta.x / (2.0 * std::tan(m_camera.getLensRadians() * 0.5));  // :1171
    double backDist = frontDist + newDelta.z;  // :1172
    m_camera.setFocusDistance(frontDist);      // :1174
    // :1175 centerEyePoint(backDist)：eye = origin + axes·(delta.x/2, delta.y/2, backDist)
    //（后平面正后方，过视域中心）。修正：此前手写 eye = getCenter() + zVec·backDist
    // 多算了 zVec·delta.z/2（相机关闭时 eye 不被读取而休眠的偏差）。
    centerEyePoint(backDist);
    // :1176 verifyFocusPlane（焦平面越出前/后平面时居中并联动 origin/extents）。
    verifyFocusPlane();

    NotifyViewportsNeedRedraw();
}

void ViewState3d::SetStandardView(int viewIndex)
{
    // Ported from: itwinjs-core StandardViewTool.onViewChanged() (ViewTool.ts:3496-3509)
    //              StandardView.getStandardRotation() (StandardView.ts:73-75)
    //
    // The standard view rotation matrices define the camera's local frame:
    //   Row 0 = X axis (right), Row 1 = Y axis (up), Row 2 = Z axis (look, inverted)
    //
    // In itwinjs-core, StandardViewTool calls:
    //   vp.view.setRotation(StandardView.getStandardRotation(this._standardViewId))
    // Then LookAtVolume adjusts origin/extents to keep the current center visible.

    auto id = static_cast<StandardViewId>(viewIndex);
    if (id < StandardViewId::Top || id > StandardViewId::RightIso)
        id = StandardViewId::Top;

    // Save current center before changing rotation
    auto center = getCenter();

    // Set the standard rotation
    m_rotation = StandardView::GetStandardRotation(id);

    // Recompute origin so the center stays at the same view-local position.
    // origin = center - rotation^T * (extents * 0.5)
    auto halfExt = dqGeom::Vector3d::From(
        m_extents.x * 0.5, m_extents.y * 0.5, m_extents.z * 0.5);
    auto rotated = m_rotation.MultiplyTransposeVector(halfExt);
    m_origin = dqGeom::Point3d::From(
        center.x - rotated.x,
        center.y - rotated.y,
        center.z - rotated.z);
}

void ViewState3d::FixAspectRatio(float windowAspect)
{
    // Ported from: itwinjs-core ViewState.fixAspectRatio() (ViewState.ts:811-822).
    // ALWAYS adjusts the Y extent: extents.y = extents.x / (windowAspect * skew).
    // (Docstring at ViewState.ts:865-869: "The automatic adjustment ... always adjusts
    //  the Y axis". The conditional X-vs-Y logic lives in adjustViewDelta, used by
    //  lookAt/lookAtVolume — NOT here.)
    if (windowAspect <= 0.0f) return;

    auto origExtents = m_extents;
    auto extents = origExtents;

    // Phase 1: getAspectRatioSkew() is always 1.0.
    double aspectRatioSkew = 1.0;
    extents.y = extents.x / (windowAspect * aspectRatioSkew);

    // isAlmostEqual: bail if Y barely changed (ViewState.ts:815-816).
    double eps = 1e-6 * std::max({std::abs(origExtents.x), std::abs(origExtents.y),
                                  std::abs(origExtents.z), 1.0});
    if (std::abs(extents.y - origExtents.y) < eps)
        return;

    // ViewState.ts:819-820  origin += 0.5 * rotation^T * (origExtents - extents)
    // (extents.vectorTo(origExtents, origExtents) == origExtents - extents). Keeps center.
    auto diff = dqGeom::Vector3d::From(origExtents.x - extents.x,
                                       origExtents.y - extents.y,
                                       origExtents.z - extents.z);
    auto rotatedDiff = m_rotation.MultiplyTransposeVector(diff);
    m_origin = dqGeom::Point3d::From(
        m_origin.x + rotatedDiff.x * 0.5,
        m_origin.y + rotatedDiff.y * 0.5,
        m_origin.z + rotatedDiff.z * 0.5);
    m_extents = extents;

    NotifyViewportsNeedRedraw();
}

// ---------------------------------------------------------------------------
// ViewState3d::computeWorldToNpc
// Ported from: itwinjs-core ViewState.computeWorldToNpc (ViewState.ts:625-701).
//
// Ortho branch (692-697): origin = inOrigin; xExtent/yExtent/zExtent = the
// rotation axis vectors scaled by delta (z falls back to 1.0 when zero);
// frustFraction stays 1.0. Perspective branch (646-691): builds back-plane
// extents from camera.focusDist + the lens-derived front/back fractions and
// emits a non-affine Map4d (frustFraction = frontFraction/backFraction),
// clamping the front/back ratio to the 24-bit z-buffer limit when
// enforceFrontToBackRatio is set. Both branches return
// Map4d.createVectorFrustum(origin, xExtent, yExtent, zExtent, frustFraction);
// nullopt if the (u,v,w) frame is non-invertible.
// NOTE: the perspective branch is ported 1:1 but is exercised end-to-end only
// via lookAt (D3.2, deferred) — the createVectorFrustum primitive it calls is
// covered by Map4dTest VectorFrustumExample (t_DMap4d.cpp:129 golden vector).
ViewState3d::WorldToNpcResult ViewState3d::computeWorldToNpc(
    dqGeom::Matrix3d const* viewRot,
    dqGeom::Point3d const* inOrigin,
    dqGeom::Vector3d const* delta,
    bool enforceFrontToBackRatio) const
{
    dqGeom::Matrix3d const& rot = (viewRot != nullptr) ? *viewRot : m_rotation;
    dqGeom::Vector3d const xVector = rot.RowX();   // ViewState.ts:630
    dqGeom::Vector3d const yVector = rot.RowY();   // ViewState.ts:631
    dqGeom::Vector3d const zVector = rot.RowZ();   // ViewState.ts:632
    dqGeom::Vector3d const ext = (delta != nullptr) ? *delta : m_extents;        // 634-635
    dqGeom::Point3d const origin = (inOrigin != nullptr) ? *inOrigin : m_origin;  // 636-637

    double frustFraction = 1.0;                    // ViewState.ts:639
    dqGeom::Vector3d xExtent, yExtent, zExtent;
    dqGeom::Point3d frustumOrigin;

    if (is3d() && m_cameraOn) {
        // Perspective (ViewState.ts:646-691).
        auto const& camera = m_camera;
        dqGeom::Vector3d eyeToOrigin = dqGeom::Vector3d::FromStartEnd(
            camera.getEyePoint(), origin);                          // 648
        eyeToOrigin = rot.MultiplyVector(eyeToOrigin);              // 649 (multiplyVectorInPlace)

        double const focusDistance = camera.getFocusDistance();     // 650-651
        double zDelta = ext.z;                                      // 651
        double zBack = eyeToOrigin.z;                               // 652
        double zFront = zBack + zDelta;                             // 653

        // 656: DanQing has no renderSystem.supportsLogZBuffer flag → non-log scale.
        double const nearScale = 0.0003;        // ViewingSpace.nearScaleNonLog24
        if (enforceFrontToBackRatio && zFront / zBack < nearScale) {  // 657
            double const maximumBackClip = 10.0 * 1000.0;   // 10 * Constant.oneKilometer (662)
            if (-zBack > maximumBackClip) {                  // 663
                zBack = -maximumBackClip;                    // 664
                eyeToOrigin.z = zBack;                       // 665
            }
            zFront = zBack * nearScale;                      // 668
            zDelta = zFront - eyeToOrigin.z;                 // 669
        }

        double const backFraction = -zBack / focusDistance;         // 673
        double const frontFraction = -zFront / focusDistance;       // 674
        frustFraction = frontFraction / backFraction;               // 675

        xExtent = dqGeom::Vector3d::FromScale(xVector, ext.x * backFraction);  // 678
        yExtent = dqGeom::Vector3d::FromScale(yVector, ext.y * backFraction);  // 679

        // 682: zExtent in view coords, then rotate back to root (683).
        dqGeom::Vector3d const zExtView = dqGeom::Vector3d::From(
            eyeToOrigin.x * (frontFraction - backFraction),
            eyeToOrigin.y * (frontFraction - backFraction),
            zDelta);
        zExtent = rot.MultiplyTransposeVector(zExtView);

        // 685-691: origin in eye coords → rotate to root (690) → + camera.eye (691).
        dqGeom::Vector3d const orgEye = dqGeom::Vector3d::From(
            eyeToOrigin.x * backFraction,
            eyeToOrigin.y * backFraction,
            eyeToOrigin.z);
        dqGeom::Vector3d const orgRoot = rot.MultiplyTransposeVector(orgEye);  // 690
        dqGeom::Point3d const eye = camera.getEyePoint();
        frustumOrigin = dqGeom::Point3d::From(
            eye.x + orgRoot.x, eye.y + orgRoot.y, eye.z + orgRoot.z);           // 691
    } else {
        // Orthographic (ViewState.ts:692-697).
        frustumOrigin = origin;
        xExtent = dqGeom::Vector3d::FromScale(xVector, ext.x);
        yExtent = dqGeom::Vector3d::FromScale(yVector, ext.y);
        double const zExt = (ext.z != 0.0) ? ext.z : 1.0;   // delta.z ? delta.z : 1.0
        zExtent = dqGeom::Vector3d::FromScale(zVector, zExt);
    }

    WorldToNpcResult result;                                    // ViewState.ts:699-700
    result.map = dqGeom::Map4d::CreateVectorFrustum(
        frustumOrigin, xExtent, yExtent, zExtent, frustFraction);
    result.frustFraction = frustFraction;
    return result;
}

// ---------------------------------------------------------------------------
// ViewState3d::calculateFrustum
// Ported from: itwinjs-core ViewState.calculateFrustum (ViewState.ts:707-715).
// Seeds the NPC cube (Frustum.initNpc) then applies computeWorldToNpc().map.
// transform1 (npcToWorld) to obtain the 8 world-space frustum corners.
bool ViewState3d::calculateFrustum(dqCommon::Frustum& out) const
{
    auto const val = computeWorldToNpc();              // ViewState.ts:708
    if (!val.map.has_value()) {
        out.initNpc();
        return false;                                   // 709-710 (undefined map)
    }
    out.initNpc();                                      // 712
    auto const& npcToWorld = val.map->Transform1();     // 713
    for (int i = 0; i < dqCommon::kNpcCornerCount; ++i) {
        out.points[i] = npcToWorld.MultiplyPoint3dQuietNormalize(out.points[i]);
    }
    return true;
}

// ---------------------------------------------------------------------------
// ViewState3d::GetFrustum
// Ported from: itwinjs-core ViewState.calculateFrustum() (ViewState.ts:703-715)
//              + ViewingSpace.getFrustum() (ViewingSpace.ts:471-503)
//              + ViewState.computeWorldToNpc() orthographic branch
//                (ViewState.ts:692-700).
//
// The reference computes a Map4d from origin/extents/rotation (+ camera lens
// for perspective) and applies map.transform1 to the 8-point NPC cube.
// For the orthographic case the map is affine:
//     worldPoint = origin + npcX * xExtent + npcY * yExtent + npcZ * zExtent
// where xExtent = viewRot.rowX() * delta.x (and similarly for Y/Z).
//
// That is algebraically equivalent to:
//     worldCorner = origin + rotation^T * (extents * npc)
// because the rotation rows ARE the view's X/Y/Z axis vectors.
//
// For includeOrientation=false (CoordSystem.View in itwinjs), the reference
// converts NPC → view coordinates via npcToViewArray: the resulting frustum
// is the axis-aligned extents box [0..extents] (no rotation, no translation).
// ---------------------------------------------------------------------------
void ViewState3d::GetFrustum(dqCommon::Frustum& out, bool includeOrientation) const
{
    if (includeOrientation) {
        // World coordinates (CoordSystem.World): delegate to calculateFrustum,
        // which applies computeWorldToNpc().map.transform1 to the NPC cube.
        // This closes D10 — the perspective (camera-on) branch is now handled
        // by computeWorldToNpc instead of the prior ortho-only fallback.
        calculateFrustum(out);
        return;
    }

    // View coordinates (CoordSystem.View): axis-aligned extents box [0..extents],
    // matching ViewingSpace.npcToView for the view-local frustum.
    out.initNpc();
    for (int i = 0; i < dqCommon::kNpcCornerCount; ++i) {
        auto const& npc = out.points[i];
        out.points[i] = dqGeom::Point3d::From(
            m_extents.x * npc.x,
            m_extents.y * npc.y,
            m_extents.z * npc.z);
    }
}

// ---------------------------------------------------------------------------
// ViewState3d::setGridSettings / getGridSettings
// Ported from: itwinjs-core ViewState.setGridSettings (ViewState.ts:953-966)
//              + ViewState.getGridSettings (ViewState.ts:968-1006).
// ---------------------------------------------------------------------------
void ViewState3d::setGridSettings(dqCommon::GridOrientationType orientation,
                                  dqGeom::Point2d spacing, int32_t gridsPerRef)
{
    // ViewState.ts:955-961 — WorldYZ/WorldXZ require a 3d view. Vacuous on ViewState3d
    // (always 3d) but kept 1:1 with the reference guard.
    switch (orientation) {
        case dqCommon::GridOrientationType::WorldYZ:
        case dqCommon::GridOrientationType::WorldXZ:
            if (!is3d())
                return;
            break;
        default:
            break;
    }
    // ViewState.ts:963-965
    m_gridOrientation = orientation;
    m_gridsPerRef = gridsPerRef;
    m_gridSpacing = spacing;
}

void ViewState3d::getGridSettings(dqGeom::Point3d& origin, dqGeom::Matrix3d& rMatrix,
                                  dqCommon::GridOrientationType orientation) const
{
    // 无视口便捷重载（GridSettingsTest 用）：View 分支无 vp 可取向——保持恒等
    // 矩阵与原点（等同参考在 identity-rotation 视图且 NpcCenter 深度即原点深度
    // 时的结果）；有视口路径（GridDecorator）走下方参考签名版本。
    rMatrix.SetIdentity();
    if (isSpatialView() && GetIModel() != nullptr)
        origin = GetIModel()->GetGlobalOrigin();
    else
        origin = dqGeom::Point3d{0.0, 0.0, 0.0};

    switch (orientation) {
        case dqCommon::GridOrientationType::View:
            break;  // 无 vp —— 恒等（参考 :1032-1038 需 vp.npcToView + vp.rotation）
        case dqCommon::GridOrientationType::WorldXY:
            break;
        case dqCommon::GridOrientationType::WorldYZ: {
            auto rowX = rMatrix.RowX();
            auto rowY = rMatrix.RowY();
            auto rowZ = rMatrix.RowZ();
            rMatrix.SetRowValues(rowY.x, rowY.y, rowY.z,
                                 rowZ.x, rowZ.y, rowZ.z,
                                 rowX.x, rowX.y, rowX.z);
            break;
        }
        case dqCommon::GridOrientationType::WorldXZ: {
            auto rowX = rMatrix.RowX();
            auto rowY = rMatrix.RowY();
            auto rowZ = rMatrix.RowZ();
            rMatrix.SetRowValues(rowX.x, rowX.y, rowX.z,
                                 rowZ.x, rowZ.y, rowZ.z,
                                 rowY.x, rowY.y, rowY.z);
            break;
        }
        case dqCommon::GridOrientationType::AuxCoord:
            break;
    }
}

// Ported from: itwinjs-core ViewState.getGridSettings (ViewState.ts:1026-1061)。
void ViewState3d::getGridSettings(Viewport const& vp, dqGeom::Point3d& origin, dqGeom::Matrix3d& rMatrix,
                                  dqCommon::GridOrientationType orientation) const
{
    // ViewState.ts:1027-1029 — start from the spatial-view global origin (else zero) and
    // an identity matrix. Guard GetIModel() against null (the reference dereferences
    // unconditionally; DanQing core is exception-free).
    rMatrix.SetIdentity();
    if (isSpatialView() && GetIModel() != nullptr)
        origin = GetIModel()->GetGlobalOrigin();
    else
        origin = dqGeom::Point3d{0.0, 0.0, 0.0};

    switch (orientation) {
        case dqCommon::GridOrientationType::View: {
            // ViewState.ts:1032-1038 — grid plane = 过视图中心深度的视平面：
            //   center = vp.npcToView(NpcCenter); rMatrix = vp.rotation;
            //   origin = R·origin; origin.z = center.z; origin = Rᵀ·origin。
            dqGeom::Point3d const center = vp.NpcToView(dqCommon::kNpcCenter);  // :1033
            rMatrix = vp.getRotation();                                          // :1034
            auto const rotated = rMatrix.MultiplyVector(                          // :1035
                dqGeom::Vector3d::From(origin.x, origin.y, origin.z));
            origin = dqGeom::Point3d{rotated.x, rotated.y, center.z};            // :1036
            auto const unrotated = rMatrix.MultiplyTransposeVector(               // :1037
                dqGeom::Vector3d::From(origin.x, origin.y, origin.z));
            origin = dqGeom::Point3d{unrotated.x, unrotated.y, unrotated.z};
            break;
        }
        case dqCommon::GridOrientationType::WorldXY:
            break;  // ViewState.ts:1040-1041 — identity matrix, origin unchanged
        case dqCommon::GridOrientationType::WorldYZ: {
            // ViewState.ts:1042-1049 — row cycle (X,Y,Z) -> (Y,Z,X).
            auto rowX = rMatrix.RowX();
            auto rowY = rMatrix.RowY();
            auto rowZ = rMatrix.RowZ();
            rMatrix.SetRowValues(rowY.x, rowY.y, rowY.z,
                                 rowZ.x, rowZ.y, rowZ.z,
                                 rowX.x, rowX.y, rowX.z);
            break;
        }
        case dqCommon::GridOrientationType::WorldXZ: {
            // ViewState.ts:1051-1058 — row cycle (X,Y,Z) -> (X,Z,Y).
            auto rowX = rMatrix.RowX();
            auto rowY = rMatrix.RowY();
            auto rowZ = rMatrix.RowZ();
            rMatrix.SetRowValues(rowX.x, rowX.y, rowX.z,
                                 rowZ.x, rowZ.y, rowZ.z,
                                 rowY.x, rowY.y, rowY.z);
            break;
        }
        case dqCommon::GridOrientationType::AuxCoord:
            break;  // ViewState.ts: AuxCoord grid is drawn via ACS.drawGrid, not a matrix branch here.
    }
}

// ---------------------------------------------------------------------------
// ViewState3d::SetupFromFrustum
// Ported from: itwinjs-core ViewState.setupFromFrustum() (ViewState.ts:734-785).
//
// Algorithm (1:1 with reference):
//   1. fixPointOrder on a local clone.
//   2. Read the three back/front edges from the LBR corner: frustumX, frustumY,
//      frustumZ.
//   3. Orthonormalize via the equivalent of Matrix3d.createRigidFromColumns
//      (Matrix3d.ts:708-724):
//        xDir  = unit(frustumX)
//        zDir  = unit(xDir × frustumY)       // "C1" column
//        yDir  = zDir × xDir                 // already unit (zDir ⊥ xDir)
//      With AxisOrder.XYZ the rigid matrix has columns (xDir, yDir, zDir).
//   4. viewRot = inverse(rigid) = transpose(rigid) → rows are (xDir, yDir, zDir).
//   5. Left-handed frustum check: zDir · frustumZ must be >= 0.
//   6. viewDiagRoot = xDir*(xDir·frustumX) + yDir*(yDir·frustumY) + zDir*zSize
//      where zSize = zDir · frustumZ.
//   7. viewOrg     = frustum.getCenter() + viewDiagRoot * (-0.5)
//      viewDelta   = viewRot * viewDiagRoot
//   8. setOrigin/setExtents/setRotation.
//
// Skipped vs reference (each marked with TODO):
//   - StandardView.adjustToStandardRotation(frustMatrix) (snaps to standard
//     views to remove floating-point fuzz) — not yet ported.
//   - adjustViewDelta clamp CALL (enforces extentLimits, ViewState.ts:833-835;
//     returns ViewStatus in reference, this port keeps bool) — adjustViewDelta
//     itself IS ported (ViewState.cpp above, ViewState.ts:889-920); only this
//     call site remains unwired (unreachable in blank-connection views whose
//     frusta stay within the default extent limits).
// ---------------------------------------------------------------------------
bool ViewState3d::SetupFromFrustum(dqCommon::Frustum const& in)
{
    dqCommon::Frustum frustum = in;  // clone — do not modify caller's frustum
    frustum.fixPointOrder();

    auto const& frustPts = frustum.points;
    constexpr int LBR = static_cast<int>(dqCommon::Npc::LeftBottomRear);
    constexpr int RBR = static_cast<int>(dqCommon::Npc::RightBottomRear);
    constexpr int LTR = static_cast<int>(dqCommon::Npc::LeftTopRear);
    constexpr int LBF = static_cast<int>(dqCommon::Npc::LeftBottomFront);

    auto const& viewOrg0 = frustPts[LBR];

    // frustumX, frustumY, frustumZ are vectors along edges of the frustum;
    // NOT unit vectors (matches reference comment, ViewState.ts:740-744).
    dqGeom::Vector3d frustumX = dqGeom::Vector3d::From(
        frustPts[RBR].x - viewOrg0.x, frustPts[RBR].y - viewOrg0.y, frustPts[RBR].z - viewOrg0.z);
    dqGeom::Vector3d frustumY = dqGeom::Vector3d::From(
        frustPts[LTR].x - viewOrg0.x, frustPts[LTR].y - viewOrg0.y, frustPts[LTR].z - viewOrg0.z);
    dqGeom::Vector3d frustumZ = dqGeom::Vector3d::From(
        frustPts[LBF].x - viewOrg0.x, frustPts[LBF].y - viewOrg0.y, frustPts[LBF].z - viewOrg0.z);

    // Port of Matrix3d.createRigidFromColumns(frustumX, frustumY, AxisOrder.XYZ)
    // (itwinjs Matrix3d.ts:708-724).
    dqGeom::Vector3d xDir = frustumX;
    if (xDir.Normalize() == 0.0)  // |frustumX| == 0 → invalid
        return false;

    dqGeom::Vector3d zDir;
    zDir.CrossProduct(xDir, frustumY);  // zDir = xDir × frustumY
    if (zDir.Normalize() == 0.0)        // xDir parallel to frustumY → invalid
        return false;

    dqGeom::Vector3d yDir;
    yDir.CrossProduct(zDir, xDir);      // yDir = zDir × xDir (unit: both inputs unit & orthogonal)

    // Ported from: itwinjs-core ViewState.setupFromFrustum (ViewState.ts:746-758).
    // frustMatrix = Matrix3d.createRigidFromColumns(frustumX, frustumY, AxisOrder.XYZ) —
    // its columns are the orthonormalized (xDir, yDir, zDir) frame derived above. Build
    // it, snap to the nearest standard rotation to remove floating-point fuzz
    // (StandardView.adjustToStandardRotation, ViewState.ts:751), then re-derive xDir/
    // yDir/zDir from the snapped matrix columns (ViewState.ts:753-755). viewRot =
    // frustMatrix.inverse() (:758); for a rigid matrix inverse == transpose, so viewRot
    // rows are the snapped frustMatrix columns. (When no snap occurs this is bit-identical
    // to the previous direct viewRot construction.)
    dqGeom::Matrix3d frustMatrix = dqGeom::Matrix3d::CreateRowValues(
        xDir.x, yDir.x, zDir.x,
        xDir.y, yDir.y, zDir.y,
        xDir.z, yDir.z, zDir.z);
    StandardView::adjustToStandardRotation(frustMatrix);
    dqGeom::Matrix3d viewRot = frustMatrix.Transpose();  // inverse of a rigid matrix
    xDir = viewRot.RowX();
    yDir = viewRot.RowY();
    zDir = viewRot.RowZ();

    // Left-handed frustum? (ViewState.ts:762-765)
    double zSize = zDir.DotProduct(frustumZ);
    if (zSize < 0.0)
        return false;

    // viewDiagRoot = xDir*(xDir·frustumX) + yDir*(yDir·frustumY) + zDir*zSize
    // (ViewState.ts:767-769)
    double xProj = xDir.DotProduct(frustumX);
    double yProj = yDir.DotProduct(frustumY);
    dqGeom::Vector3d viewDiagRoot = dqGeom::Vector3d::From(
        xDir.x * xProj + yDir.x * yProj + zDir.x * zSize,
        xDir.y * xProj + yDir.y * yProj + zDir.y * zSize,
        xDir.z * xProj + yDir.z * yProj + zDir.z * zSize);

    // viewOrg = center - 0.5 * viewDiagRoot (ViewState.ts:829)
    auto center = frustum.getCenter();
    dqGeom::Point3d viewOrg = dqGeom::Point3d::From(
        center.x - 0.5 * viewDiagRoot.x,
        center.y - 0.5 * viewDiagRoot.y,
        center.z - 0.5 * viewDiagRoot.z);

    // viewDelta = viewRot * viewDiagRoot (ViewState.ts:832)
    dqGeom::Vector3d viewDelta = viewRot.MultiplyVector(viewDiagRoot);

    // TODO: wire the adjustViewDelta clamp call (ViewState.ts:833-835:
    //       `const status = this.adjustViewDelta(viewDelta, viewOrg, viewRot,
    //       undefined, opts); if (ViewStatus.Success !== status) return status;`).
    //       adjustViewDelta itself IS ported (ViewState.cpp above,
    //       ViewState.ts:889-920) — only this call site (and the accompanying
    //       bool → ViewStatus return-shape change) remains unwired; unreachable
    //       in blank-connection views (frusta stay within default extent limits).

    m_origin = viewOrg;
    m_extents = viewDelta;
    m_rotation = viewRot;

    // Ported from: itwinjs-core ViewState3d.setupFromFrustum override
    // (ViewState.ts:1766-1811)——基类公式产出的是**后平面**尺度 extents；透视
    // 视锥必须再按焦距比例收回焦平面尺度，否则 extents 爆炸（2026-09-20 锚定
    // 拖动视口错乱事故的下半截根因——实测 extents 12→35）。
    TurnCameraOff();   // :1771
    {
        // :1775-1780 — 用前/后面 X 宽判别相机/平行视图
        constexpr int RBF = static_cast<int>(dqCommon::Npc::RightBottomFront);
        double const xBack = frustPts[LBR].Distance(frustPts[RBR]);
        double const xFront = frustPts[LBF].Distance(frustPts[RBF]);
        constexpr double kFlatViewFractionTolerance = 1.0e-6;
        if (xFront > xBack * (1.0 + kFlatViewFractionTolerance))
            return false;   // InvalidWindow（bool 端口形）
        double const compression = xFront / xBack;
        if (compression >= (1.0 - kFlatViewFractionTolerance))
            return true;    // :1784-1785 — flat view, done

        // :1788-1793 — 透视视锥：由锥度反推眼点（frustumZ/(1-compression)）
        dqGeom::Point3d viewOrgPersp = frustPts[LBR];
        dqGeom::Vector3d const viewDeltaPersp = m_extents;   // :1789 — 基类刚设的后平面尺度
        dqGeom::Vector3d const zDirPersp = GetZVec();
        dqGeom::Vector3d const frustumZPersp =
            dqGeom::Vector3d::FromStartEnd(viewOrgPersp, frustPts[LBF]);
        dqGeom::Vector3d const frustOrgToEye =
            dqGeom::Vector3d::FromScale(frustumZPersp, 1.0 / (1.0 - compression));
        // :1793 — eyePoint = viewOrg.plus(frustOrgToEye)
        dqGeom::Point3d const eyePoint = dqGeom::Point3d::From(
            viewOrgPersp.x + frustOrgToEye.x,
            viewOrgPersp.y + frustOrgToEye.y,
            viewOrgPersp.z + frustOrgToEye.z);

        // :1795-1797 — 眼到后平面距离 + 焦距比例
        double const backDistance = frustOrgToEye.DotProduct(zDirPersp);
        double const focusDistance = m_camera.isFocusValid()
            ? m_camera.getFocusDistance()
            : (backDistance - (viewDeltaPersp.z / 2.0));
        double const focalFraction = focusDistance / backDistance;

        // :1799-1801 — 原点投回后平面（eyePoint.plus2Scaled(frustOrgToEye,
        // -focalFraction, zDir, focusDistance - backDistance)），extents x/y 按
        // focalFraction 收回焦平面
        dqGeom::Vector3d const backTerm = dqGeom::Vector3d::FromScale(frustOrgToEye, -focalFraction);
        dqGeom::Vector3d const zTerm = dqGeom::Vector3d::FromScale(zDirPersp, focusDistance - backDistance);
        m_origin = dqGeom::Point3d::From(
            eyePoint.x + backTerm.x + zTerm.x,
            eyePoint.y + backTerm.y + zTerm.y,
            eyePoint.z + backTerm.z + zTerm.z);
        m_extents = dqGeom::Vector3d::From(m_extents.x * focalFraction,
                                           m_extents.y * focalFraction,
                                           m_extents.z);

        // :1803-1808
        setEyePoint(eyePoint);
        setFocusDistance(focusDistance);
        SetLensAngle(calcLensAngle());
        enableCamera();
    }
    // (_updateMaxGlobalScopeFactor :1809 随其子系统 TODO 未移植。)

    NotifyViewportsNeedRedraw();
    return true;
}

// ---------------------------------------------------------------------------
// ViewState3d::MoveCameraWorld
// Ported from: itwinjs-core ViewState3d.moveCameraWorld() (ViewState.ts:2029-2040)：
//     if (!cameraOn) { origin += distance; return Success; }
//     targetPoint = getTargetPoint() + distance;
//     eyePoint    = getEyePoint()    + distance;
//     return this.lookAt({ eyePoint, targetPoint, upVector: getYVector() });
// ---------------------------------------------------------------------------
ViewStatus ViewState3d::MoveCameraWorld(dqGeom::Vector3d const& dist)
{
    if (!m_cameraOn) {
        m_origin = dqGeom::Point3d::From(
            m_origin.x + dist.x,
            m_origin.y + dist.y,
            m_origin.z + dist.z);
        NotifyViewportsNeedRedraw();
        return ViewStatus::Success;
    }

    // 相机开启分支：忠实 lookAt 路径（此前为正交等价平移的简化——重建旋转/
    // 焦距/extents 的参考行为被跳过）。
    auto const target = GetTargetPoint();
    dqGeom::Point3d const targetPoint =
        dqGeom::Point3d::From(target.x + dist.x, target.y + dist.y, target.z + dist.z);
    auto const& eye = m_camera.getEyePoint();
    dqGeom::Point3d const eyePoint =
        dqGeom::Point3d::From(eye.x + dist.x, eye.y + dist.y, eye.z + dist.z);

    LookAtArgs args;
    args.eyePoint = eyePoint;
    args.targetPoint = targetPoint;
    args.upVector = GetYVec();
    return lookAt(args);
}

// ---------------------------------------------------------------------------
// SpatialViewState
// ---------------------------------------------------------------------------
SpatialViewState::SpatialViewState()
{
    // ← SpatialViewState.ts:109: this._treeRefs = SpatialTileTreeReferences.create(this)
    //   (the overridable factory seam). Must precede any GetTileTreeRefs use.
    m_treeRefs = SpatialTileTreeReferences::create(*this);

    // Ported from: itwinjs-core SpatialViewState.defaultExtentLimits
    // (SpatialViewState.ts:117) — { min: Constant.oneMillimeter,
    // max: 3 * Constant.diameterOfEarth }（core-geometry Constant.ts:16/24：
    // 0.001 / 3*(12742.0*1000)=38226000；"Increased max by 3X to support globe
    // mode."）。与基类成员默认同值，此处显式标注 3d 空间视图的参考出处。
    m_extentLimits = {0.001, 38226000.0};
}
SpatialViewState::~SpatialViewState() = default;

SpatialTileTreeReferences& SpatialViewState::GetTileTreeRefs() const noexcept
{
    return *m_treeRefs;
}

// <- SpatialViewState.getModelTreeRefs (SpatialViewState.ts:544-548).
void SpatialViewState::ForEachModelTreeRef(
    std::function<void(TileTreeReference&)> const& func) const
{
    m_treeRefs->forEachTileTreeRef(func);
}

// Clone — deep copy including model selector.
dqBase::RefPtr<ViewState> SpatialViewState::Clone() const
{
    auto* raw = new SpatialViewState();
    raw->m_iModel = m_iModel;
    raw->m_displayStyle.setIModel(m_iModel);   // DisplayStyle iModel 绑定随克隆（同 SetIModel）
    raw->m_origin = m_origin;
    raw->m_extents = m_extents;
    raw->m_extentLimits = m_extentLimits;  // ViewState.ts:315
    m_displayStyle.cloneDataTo(raw->m_displayStyle);
    // Clone category selector (events are not cloned).
    for (auto const& catId : m_categorySelector.getCategories()) {
        raw->m_categorySelector.addCategory(catId);
    }
    raw->m_description = m_description;
    raw->m_rotation = m_rotation;
    raw->m_camera = m_camera;
    raw->m_cameraOn = m_cameraOn;
    // Clone model selector data (events are not cloned).
    for (auto const& modelId : m_modelSelector.getModels()) {
        raw->m_modelSelector.addModel(modelId);
    }
    return dqBase::RefPtr<ViewState>(raw);
}

dqBase::RefPtr<SpatialViewState> SpatialViewState::CreateBlank(
    IModelConnection* iModel,
    dqGeom::Point3d const& origin,
    dqGeom::Vector3d const& extents,
    std::optional<dqGeom::Matrix3d> rotation)
{
    // Ported from: itwinjs-core core/frontend/src/SpatialViewState.ts:77-88 (createBlank).
    // Raw defaults only: empty CategorySelector + ModelSelector, default
    // DisplayStyle3dSettings (Wireframe renderMode, lighting off, black background, sky
    // off, grid off), origin/extents set. Rotation: if provided, apply it; otherwise leave
    // the member default (identity = StandardViewId.Top — matches the reference's "rotation
    // undefined -> top view" docstring at SpatialViewState.ts:74). The display-test-app
    // overrides (SmoothShade/lighting/backgroundMap/white/sky) are applied by
    // manufactureSpatialView (ViewPicker.ts:149-169), NOT here.
    dqBase::RefPtr<SpatialViewState> view(new SpatialViewState());
    view->SetIModel(iModel);
    view->SetOrigin(origin);
    view->SetExtents(extents);
    if (rotation)  // SpatialViewState.ts:84-85  if (undefined !== rotation) view.setRotation(rotation)
        view->SetRotation(*rotation);
    return view;
}

// Ported from: itwinjs-core SpatialViewState.computeBaseExtents (SpatialViewState.ts:124-134).
dqGeom::Range3d SpatialViewState::ComputeBaseExtents() const
{
    // SpatialViewState.ts:126 — Range3d.fromJSON(this.iModel.projectExtents). iModel is a
    // SpatialViewState invariant (always set by createBlank); read unconditionally per the
    // reference — no invented fallback (audit D4.1).
    auto ext = m_iModel->GetProjectExtents();
    // SpatialViewState.ts:128 — scale about center by 1.0001 so geometry coincident with
    // the project-extent planes is not clipped.
    ext.scaleAboutCenterInPlace(1.0001);
    // SpatialViewState.ts:131 — extendRange(getGroundExtents()) omitted: the ground plane
    // is off by default in the blank view.
    return ext;
}

// Ported from: itwinjs-core SpatialViewState.computeFitRange (SpatialViewState.ts:145-160).
dqGeom::Range3d SpatialViewState::ComputeFitRange() const
{
    // SpatialViewState.ts:147 — options.baseExtents / ref.unionFitRange (the tile-tree
    // range union) is not ported; a blank connection has no loaded tiles, so `range`
    // stays null and we fall back to computeBaseExtents (SpatialViewState.ts:153-154).
    dqGeom::Range3d range;  // default-constructed == null
    if (range.isNull())
        range = ComputeBaseExtents();
    range.ensureMinLengths(1.0);  // SpatialViewState.ts:157
    return range;
}

}  // namespace dqApp

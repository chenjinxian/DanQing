// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewClip 工具族实现
// Ported from: itwinjs-core core/frontend/src/tools/ClipViewTool.ts
//              ViewClipTool (:77-443) + Clear (:445-480) + ByPlane (:483-558) +
//              ByShape (:564-800) + ByRange (:805-921) + ByElement (:926-1058)
//
// EQUIVALENCE 清单见 ClipViewTool.h 文件头。装饰绘制面（drawClip 族）归 P-F。
#include <dqApp/ClipViewTool.h>

#include <dqApp/Application.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/ViewState.h>
#include <dqApp/tile/DumpIModelConnection.h>

#include <dqGeom/YawPitchRollAngles.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace dqApp {

// ---------------------------------------------------------------------------
// ViewClipTool 基类（:77-443）
// ---------------------------------------------------------------------------

// Ported from: onPostInstall (:107-110).
void ViewClipTool::onPostInstall()
{
    PrimitiveTool::onPostInstall();
    setupAndPromptForNextAction();
}

// Ported from: onUnsuspend (:112).
void ViewClipTool::onUnsuspend()
{
    showPrompt();
}

// Ported from: onRestartTool (:113).
void ViewClipTool::onRestartTool()
{
    exitTool();
}

// Ported from: onResetButtonUp (:116-119 —— onReinitialize + No).
EventHandled ViewClipTool::onResetButtonUp(BeButtonEvent const& /*ev*/)
{
    onReinitialize();
    return EventHandled::No;
}

// Ported from: isCompatibleViewport (:102 —— super 门 + allow3dManipulations).
// DanQing 的 allow3dManipulations = is3d() 简化面（ViewState3d.h :494 登记）。
bool ViewClipTool::isCompatibleViewport(Viewport* vp) const
{
    return vp != nullptr && vp->GetView() != nullptr && vp->GetView()->AsViewState3d() != nullptr;
}

// Ported from: getPlaneInwardNormal (:131-136 —— 上下文旋转矩阵列 2 取负).
// EQUIVALENCE（§11.10）：参考源 = AccuDrawHintBuilder.getContextRotation(
// orientation, viewport)（AccuDraw 未移植）。世界六朝向以世界轴列承载
// （参考 Top/Front/... 的上下文旋转在这些朝向下即世界轴矩阵——无 ACS 旋转
// 时两者合同）；View/Face 取视图旋转 Z 列负（Face = 屏幕朝向语义）。
// 发散 = ACS 上下文锁定旋转缺席；验证法 = ClipViewToolTest.PlaneInwardNormal
// 六朝向断言 + E2E 像素锁。
std::optional<dqGeom::Vector3d> ViewClipTool::getPlaneInwardNormal(ContextRotationId orientation,
                                                                   Viewport const& viewport)
{
    dqGeom::Vector3d inward;
    switch (orientation) {
        case ContextRotationId::Top:
            inward = dqGeom::Vector3d::From(0.0, 0.0, -1.0);
            break;
        case ContextRotationId::Bottom:
            inward = dqGeom::Vector3d::From(0.0, 0.0, 1.0);
            break;
        case ContextRotationId::Front:
            inward = dqGeom::Vector3d::From(0.0, -1.0, 0.0);
            break;
        case ContextRotationId::Back:
            inward = dqGeom::Vector3d::From(0.0, 1.0, 0.0);
            break;
        case ContextRotationId::Left:
            inward = dqGeom::Vector3d::From(-1.0, 0.0, 0.0);
            break;
        case ContextRotationId::Right:
            inward = dqGeom::Vector3d::From(1.0, 0.0, 0.0);
            break;
        case ContextRotationId::View:
        case ContextRotationId::Face: {
            ViewState3d const* view3d = viewport.GetView()->AsViewState3d();
            if (view3d == nullptr)
                return std::nullopt;
            // matrix.getColumn(2).negate() —— 视图 Z 轴（指向观察者）取负。
            dqGeom::Matrix3d const& rotation = view3d->getRotation();
            dqGeom::Vector3d zVec = rotation.ColumnZ();
            zVec.Negate();
            inward = zVec;
            break;
        }
    }
    return inward;
}

// Ported from: enableClipVolume (:138-144).
bool ViewClipTool::enableClipVolume(Viewport& viewport)
{
    dqCommon::ViewFlags const& flags = viewport.GetView()->GetDisplayStyle().getViewFlags();
    if (flags.clipVolume())
        return false;

    auto p = flags.Properties();
    p.clipVolume = true;
    viewport.GetView()->GetDisplayStyle().setViewFlags(dqCommon::ViewFlags(p));
    return true;
}

// Ported from: setViewClip (:146-150 —— view.setViewClip(clip) + setupFromView).
bool ViewClipTool::setViewClip(Viewport& viewport, dqGeom::ClipVector::Ptr const& clip)
{
    viewport.GetView()->setViewClip(clip);
    viewport.SetupFromView();
    return true;
}

// Ported from: doClipToConvexClipPlaneSet (:152-157).
bool ViewClipTool::doClipToConvexClipPlaneSet(Viewport& viewport,
                                              dqGeom::ConvexClipPlaneSet const& planes)
{
    dqGeom::ClipPrimitive::Ptr const prim = dqGeom::ClipPrimitive::createCapture(planes);
    dqGeom::ClipVector::Ptr const clip = dqGeom::ClipVector::createEmpty();
    clip->appendReference(prim);
    return setViewClip(viewport, clip);
}

// Ported from: doClipToPlane (:159-179).
bool ViewClipTool::doClipToPlane(Viewport& viewport, dqGeom::Point3d const& origin,
                                 dqGeom::Vector3d const& normal, bool clearExistingPlanes)
{
    std::optional<dqGeom::Plane3dByOriginAndUnitNormal> const plane =
        dqGeom::Plane3dByOriginAndUnitNormal::create(origin, normal);
    if (!plane.has_value())
        return false;
    std::optional<dqGeom::ConvexClipPlaneSet> planeSet;
    if (!clearExistingPlanes) {
        dqGeom::ClipVector::Ptr const existingClip = viewport.GetView()->getViewClip();
        if (!existingClip.IsNull() && 1 == existingClip->clips().size()) {
            dqGeom::ClipPrimitive::Ptr const& existingPrim = existingClip->clips()[0];
            if (existingPrim->asClipShape() == nullptr) {  // not a ClipShape
                dqGeom::UnionOfConvexClipPlaneSets const* existingPlaneSets = existingPrim->fetchClipPlanesRef();
                if (existingPlaneSets != nullptr && 1 == existingPlaneSets->convexSets().size())
                    planeSet = existingPlaneSets->convexSets()[0];
            }
        }
    }
    if (!planeSet.has_value())
        planeSet = dqGeom::ConvexClipPlaneSet::createEmpty();
    planeSet->addPlaneToConvexSet(dqGeom::ClipPlane::createPlane(*plane));
    return doClipToConvexClipPlaneSet(viewport, *planeSet);
}

// Ported from: doClipToShape (:181-185).
bool ViewClipTool::doClipToShape(Viewport& viewport, std::vector<dqGeom::Point3d> const& xyPoints,
                                  dqGeom::Transform const* transform, std::optional<double> zLow,
                                  std::optional<double> zHigh)
{
    dqGeom::ClipVector::Ptr const clip = dqGeom::ClipVector::createEmpty();
    clip->appendShape(xyPoints, zLow, zHigh, transform);
    return setViewClip(viewport, clip);
}

// Ported from: doClipToRange (:187-195).
bool ViewClipTool::doClipToRange(Viewport& viewport, dqGeom::Range3d const& range,
                                 dqGeom::Transform const* transform)
{
    if (range.isNull() || range.isAlmostZeroX() || range.isAlmostZeroY())
        return false;
    dqGeom::ClipVector::Ptr const clip = dqGeom::ClipVector::createEmpty();
    dqGeom::ClipShape::Ptr const block = dqGeom::ClipShape::createBlock(
        range, range.isAlmostZeroZ() ? dqGeom::ClipMaskXYZRangePlanes::XAndY
                                     : dqGeom::ClipMaskXYZRangePlanes::All,
        false, false, transform);
    clip->appendReference(block);
    return setViewClip(viewport, clip);
}

// Ported from: doClipClear (:196-200).
bool ViewClipTool::doClipClear(Viewport& viewport)
{
    if (!hasClip(viewport))
        return false;
    return setViewClip(viewport);
}

// Ported from: getClipRayTransformed (:203-215).
dqGeom::Ray3d ViewClipTool::getClipRayTransformed(dqGeom::Point3d const& origin,
                                                  dqGeom::Vector3d const& direction,
                                                  dqGeom::Transform const* transform)
{
    dqGeom::Point3d facePt = origin;
    dqGeom::Vector3d faceDir = direction;

    if (transform != nullptr) {
        facePt = transform->MultiplyPoint3d(facePt);
        faceDir = transform->MultiplyVector(faceDir);
        faceDir.Normalize();
    }

    return dqGeom::Ray3d{facePt, faceDir};
}

// Ported from: getOffsetValueTransformed (:217-227).
double ViewClipTool::getOffsetValueTransformed(double offset, dqGeom::Transform const* transform)
{
    if (transform == nullptr)
        return offset;
    // Vector3d.create(offset) —— TS 默认参 (x=offset, y=0, z=0)。
    dqGeom::Vector3d const lengthVec = dqGeom::Vector3d::From(offset, 0.0, 0.0);
    dqGeom::Vector3d const transformed = transform->MultiplyVector(lengthVec);
    double const localOffset = std::sqrt(transformed.x * transformed.x + transformed.y * transformed.y
                                         + transformed.z * transformed.z);
    return (offset < 0 ? -localOffset : localOffset);
}

// Ported from: getClipShapePoints (:312-318).
std::vector<dqGeom::Point3d> ViewClipTool::getClipShapePoints(dqGeom::ClipShape const& shape, double z)
{
    std::vector<dqGeom::Point3d> points;
    for (dqGeom::Point3d const& pt : shape.polygon())
        points.push_back(dqGeom::Point3d::From(pt.x, pt.y, z));
    return points;
}

// Ported from: getClipShapeExtents (:320-343).
dqGeom::Range1d ViewClipTool::getClipShapeExtents(dqGeom::ClipShape const& shape,
                                                  dqGeom::Range3d const& viewRange)
{
    std::optional<double> zLow = shape.zLow();
    std::optional<double> zHigh = shape.zHigh();
    if (!zLow.has_value() || !zHigh.has_value()) {
        dqGeom::Vector3d const zVec = dqGeom::Vector3d::From(0.0, 0.0, 1.0);  // Vector3d.unitZ()
        dqGeom::Point3d const& origin = shape.polygon()[0];
        std::array<dqGeom::Point3d, 8> const cornerArray = viewRange.Corners();
        std::vector<dqGeom::Point3d> corners(cornerArray.begin(), cornerArray.end());
        if (shape.transformToClip() != nullptr) {
            for (dqGeom::Point3d& c : corners)
                c = shape.transformToClip()->MultiplyPoint3d(c);
        }
        for (dqGeom::Point3d const& corner : corners) {
            dqGeom::Vector3d const delta = dqGeom::Vector3d::FromStartEnd(origin, corner);
            double const projection = delta.DotProduct(zVec);
            if (!shape.zLow().has_value() && (!zLow.has_value() || projection < *zLow))
                zLow = projection;
            if (!shape.zHigh().has_value() && (!zHigh.has_value() || projection > *zHigh))
                zHigh = projection;
        }
    }

    if (!zLow.has_value() || !zHigh.has_value())
        return dqGeom::Range1d::CreateNull();
    return dqGeom::Range1d::CreateXX(*zLow, *zHigh);
}

// Ported from: isSingleClipShape (:345-357).
dqGeom::ClipShape const* ViewClipTool::isSingleClipShape(dqGeom::ClipVector const& clip)
{
    if (1 != clip.clips().size())
        return nullptr;
    dqGeom::ClipPrimitive::Ptr const& prim = clip.clips()[0];
    dqGeom::ClipShape const* shape = prim->asClipShape();
    if (shape == nullptr)
        return nullptr;
    if (!shape->isValidPolygon())
        return nullptr;
    return shape;
}

// Ported from: isSingleConvexClipPlaneSet (:385-394).
dqGeom::ConvexClipPlaneSet const* ViewClipTool::isSingleConvexClipPlaneSet(dqGeom::ClipVector const& clip)
{
    if (1 != clip.clips().size())
        return nullptr;
    dqGeom::ClipPrimitive::Ptr const& prim = clip.clips()[0];
    if (prim->asClipShape() != nullptr)
        return nullptr;
    dqGeom::UnionOfConvexClipPlaneSets const* planeSets = prim->fetchClipPlanesRef();
    return (planeSets != nullptr && 1 == planeSets->convexSets().size())
               ? &planeSets->convexSets()[0]
               : nullptr;
}

// Ported from: isSingleClipPlane (:396-402).
dqGeom::ClipPlane const* ViewClipTool::isSingleClipPlane(dqGeom::ClipVector const& clip)
{
    dqGeom::ConvexClipPlaneSet const* clipPlanes = isSingleConvexClipPlaneSet(clip);
    if (clipPlanes == nullptr || 1 != clipPlanes->planes.size())
        return nullptr;
    return &clipPlanes->planes[0];
}

// Ported from: areClipsEqual (:404-440).
bool ViewClipTool::areClipsEqual(dqGeom::ClipVector const& clipA, dqGeom::ClipVector const& clipB)
{
    if (&clipA == &clipB)
        return true;
    if (clipA.clips().size() != clipB.clips().size())
        return false;
    for (size_t iPrim = 0; iPrim < clipA.clips().size(); iPrim++) {
        dqGeom::ClipPrimitive::Ptr const& primA = clipA.clips()[iPrim];
        dqGeom::ClipPrimitive::Ptr const& primB = clipB.clips()[iPrim];
        dqGeom::UnionOfConvexClipPlaneSets const* planesA = primA->fetchClipPlanesRef();
        dqGeom::UnionOfConvexClipPlaneSets const* planesB = primB->fetchClipPlanesRef();
        if (planesA != nullptr && planesB != nullptr) {
            if (planesA->convexSets().size() != planesB->convexSets().size())
                return false;
            for (size_t iPlane = 0; iPlane < planesA->convexSets().size(); iPlane++) {
                dqGeom::ConvexClipPlaneSet const& planeSetA = planesA->convexSets()[iPlane];
                dqGeom::ConvexClipPlaneSet const& planeSetB = planesB->convexSets()[iPlane];
                if (planeSetA.planes.size() != planeSetB.planes.size())
                    return false;
                for (size_t iClipPlane = 0; iClipPlane < planeSetA.planes.size(); iClipPlane++) {
                    dqGeom::ClipPlane const& planeA = planeSetA.planes[iClipPlane];
                    dqGeom::ClipPlane const& planeB = planeSetB.planes[iClipPlane];
                    if (!planeA.isAlmostEqual(planeB))
                        return false;
                }
            }
        } else if (planesA == nullptr && planesB == nullptr) {
            continue;
        } else {
            return false;
        }
    }
    return true;
}

// Ported from: hasClip (:441-443).
bool ViewClipTool::hasClip(Viewport const& viewport)
{
    return !viewport.GetView()->getViewClip().IsNull();
}

// ---------------------------------------------------------------------------
// ViewClipClearTool (:445-480)
// ---------------------------------------------------------------------------

// Ported from: isCompatibleViewport (:449 —— super + hasClip).
bool ViewClipClearTool::isCompatibleViewport(Viewport* vp) const
{
    return ViewClipTool::isCompatibleViewport(vp) && vp != nullptr && hasClip(*vp);
}

// Ported from: doClipClear (:452-460).
bool ViewClipClearTool::doClipClear(Viewport& viewport)
{
    if (!ViewClipTool::doClipClear(viewport))
        return false;
    if (m_clipEventHandler != nullptr)
        m_clipEventHandler->onClearClip(viewport);
    onReinitialize();
    return true;
}

// Ported from: onPostInstall (:462-466 —— targetView 有 clip 则安装即清).
// §3.4：targetView → ToolAdmin 安装目标视口（DanQing 以 ToolAdmin 当前活动
// 视口承载——无安装期槽，工具事件流以 ev.viewport 为准；安装即清路径由
// run() 后 host 驱动，见 DtaTools wiring）。
void ViewClipClearTool::onPostInstall()
{
    ViewClipTool::onPostInstall();
}

// Ported from: onDataButtonDown (:468-473).
EventHandled ViewClipClearTool::onDataButtonDown(BeButtonEvent const& ev)
{
    if (ev.viewport == nullptr)
        return EventHandled::No;
    return doClipClear(*ev.viewport) ? EventHandled::Yes : EventHandled::No;
}

// ---------------------------------------------------------------------------
// ViewClipByPlaneTool (:483-558)
// ---------------------------------------------------------------------------

// Ported from: onDataButtonDown (:545-558).
EventHandled ViewClipByPlaneTool::onDataButtonDown(BeButtonEvent const& ev)
{
    Viewport* targetView = ev.viewport;
    if (targetView == nullptr)
        return EventHandled::No;
    std::optional<dqGeom::Vector3d> const normal = getPlaneInwardNormal(m_orientation, *targetView);
    if (!normal.has_value())
        return EventHandled::No;
    enableClipVolume(*targetView);
    if (!doClipToPlane(*targetView, ev.point, *normal, m_clearExistingPlanes))
        return EventHandled::No;
    if (m_clipEventHandler != nullptr)
        m_clipEventHandler->onNewClipPlane(*targetView);
    onReinitialize();
    return EventHandled::Yes;
}

// ---------------------------------------------------------------------------
// ViewClipByShapeTool (:564-800)
// ---------------------------------------------------------------------------

// Ported from: getClipPoints (:674-709).
// EQUIVALENCE：AccuDrawHintBuilder.projectPointToPlaneInView（视口驱动投影）→
// 沿 _matrix 列 2 法向的平面正交投影（首点为平面原点——参考语义的解析等价）。
std::vector<dqGeom::Point3d> ViewClipByShapeTool::getClipPoints(BeButtonEvent const& ev) const
{
    std::vector<dqGeom::Point3d> points;
    if (ev.viewport == nullptr || m_points.empty())
        return points;
    for (dqGeom::Point3d const& pt : m_points)
        points.push_back(pt);

    if (!m_matrix.has_value())
        return points;

    dqGeom::Vector3d const normal = m_matrix->ColumnZ();

    // projectPointToPlaneInView(ev.point, points[0], normal) —— 正交投影：
    // p - n·(n·(p - p0))
    dqGeom::Point3d currentPt = ev.point;
    dqGeom::Vector3d const delta = dqGeom::Vector3d::FromStartEnd(points[0], currentPt);
    double const dist = delta.DotProduct(normal);
    currentPt = dqGeom::Point3d::From(currentPt.x - normal.x * dist,
                                       currentPt.y - normal.y * dist,
                                       currentPt.z - normal.z * dist);

    if (2 == points.size()
        && (ev.keyModifiers & BeModifierKeys::Control) == BeModifierKeys::None) {
        // 2 点且无 Ctrl → 自动补成矩形（:692-703）：cornerPt = 沿 yDir 投影的
        // 第四角，currentPt 回退到 xDir 反向。
        dqGeom::Vector3d xDir = dqGeom::Vector3d::FromStartEnd(points[0], points[1]);
        double const xLen = std::sqrt(xDir.x * xDir.x + xDir.y * xDir.y + xDir.z * xDir.z);
        xDir.Normalize();
        dqGeom::Vector3d yDir = dqGeom::Vector3d::FromCrossProduct(xDir, normal);
        yDir.Normalize();
        // projectPointToLineInView(currentPt, points[1], yDir) —— 点到直线正交投影。
        dqGeom::Vector3d const d2 = dqGeom::Vector3d::FromStartEnd(points[1], currentPt);
        double const along = d2.DotProduct(yDir);
        dqGeom::Point3d const cornerPt = dqGeom::Point3d::From(
            points[1].x + yDir.x * along, points[1].y + yDir.y * along, points[1].z + yDir.z * along);
        points.push_back(cornerPt);
        currentPt = dqGeom::Point3d::From(cornerPt.x - xDir.x * xLen,
                                          cornerPt.y - xDir.y * xLen,
                                          cornerPt.z - xDir.z * xLen);
    }
    points.push_back(currentPt);
    if (points.size() > 2)
        points.push_back(points[0]);

    return points;
}

// Ported from: onDataButtonDown (:759-800).
EventHandled ViewClipByShapeTool::onDataButtonDown(BeButtonEvent const& ev)
{
    Viewport* targetView = ev.viewport;
    if (targetView == nullptr)
        return EventHandled::No;

    if (m_points.size() > 1
        && (ev.keyModifiers & BeModifierKeys::Control) == BeModifierKeys::None) {
        std::vector<dqGeom::Point3d> points = getClipPoints(ev);
        if (points.size() < 3)
            return EventHandled::No;

        // Transform.createOriginAndMatrix（:771）—— DanQing Transform(origin, matrix) 构造。
        dqGeom::Transform transform = dqGeom::Transform(points[0], *m_matrix);
        for (dqGeom::Point3d& p : points) {
            dqGeom::Point3d const src = p;
            transform.MultiplyInversePoint3d(src, p);
        }
        enableClipVolume(*targetView);
        if (!doClipToShape(*targetView, points, &transform, m_zLow, m_zHigh))
            return EventHandled::No;
        if (m_clipEventHandler != nullptr)
            m_clipEventHandler->onNewClip(*targetView);
        onReinitialize();
        return EventHandled::Yes;
    }

    // 首点：建立上下文旋转矩阵（:781 —— getContextRotation(orientation)）。
    // EQUIVALENCE：AccuDraw 缺席——Top 世界 XY 基承载（getPlaneInwardNormal 同款）。
    if (!m_matrix.has_value()) {
        switch (m_orientation) {
            case ContextRotationId::Top:
                m_matrix = dqGeom::Matrix3d::CreateIdentity();
                break;
            case ContextRotationId::Front:
                m_matrix = dqGeom::Matrix3d::CreateRowValues(1, 0, 0, 0, 0, 1, 0, -1, 0);
                break;
            case ContextRotationId::Left:
                m_matrix = dqGeom::Matrix3d::CreateRowValues(0, 0, 1, 0, 1, 0, -1, 0, 0);
                break;
            case ContextRotationId::Bottom:
                m_matrix = dqGeom::Matrix3d::CreateRowValues(1, 0, 0, 0, 0, -1, 0, 1, 0);
                break;
            case ContextRotationId::Back:
                m_matrix = dqGeom::Matrix3d::CreateRowValues(-1, 0, 0, 0, 0, 1, 0, 1, 0);
                break;
            case ContextRotationId::Right:
                m_matrix = dqGeom::Matrix3d::CreateRowValues(0, 0, -1, 0, 1, 0, 1, 0, 0);
                break;
            case ContextRotationId::View:
            case ContextRotationId::Face: {
                ViewState3d const* view3d = targetView->GetView()->AsViewState3d();
                if (view3d == nullptr)
                    return EventHandled::No;
                m_matrix = view3d->getRotation();
                break;
            }
        }
    }

    // 后续点：投影到首点平面（:784-791 —— planePt 投影同 getClipPoints）。
    dqGeom::Point3d currPt = ev.point;
    if (m_points.size() > 0) {
        dqGeom::Vector3d const normal = m_matrix->ColumnZ();
        dqGeom::Vector3d const delta = dqGeom::Vector3d::FromStartEnd(m_points[0], currPt);
        double const dist = delta.DotProduct(normal);
        currPt = dqGeom::Point3d::From(currPt.x - normal.x * dist, currPt.y - normal.y * dist,
                                       currPt.z - normal.z * dist);
    }

    m_points.push_back(currPt);
    setupAndPromptForNextAction();
    return EventHandled::No;
}

// Ported from: onUndoPreviousStep (:792-800).
bool ViewClipByShapeTool::onUndoPreviousStep()
{
    if (m_points.empty())
        return false;

    m_points.pop_back();
    setupAndPromptForNextAction();
    return true;
}

// ---------------------------------------------------------------------------
// ViewClipByRangeTool (:805-921)
// ---------------------------------------------------------------------------

// Ported from: getClipRange (:840-852).
// EQUIVALENCE：getContextRotation(Top) 的 ACS 锁定 → 世界 Top 恒等基
// （getPlaneInwardNormal 同款登记）。
bool ViewClipByRangeTool::getClipRange(dqGeom::Range3d& range, dqGeom::Transform& transform,
                                       BeButtonEvent const& ev) const
{
    if (ev.viewport == nullptr || !m_corner.has_value())
        return false;
    // Creating clip aligned with ACS when ACS context lock is enabled...
    // （参考 :845-847 —— Top 上下文旋转 = 世界 XY 恒等）
    transform = dqGeom::Transform(*m_corner, dqGeom::Matrix3d::CreateIdentity());
    dqGeom::Point3d pt1, pt2;
    if (!transform.MultiplyInversePoint3d(*m_corner, pt1))
        return false;
    if (!transform.MultiplyInversePoint3d(ev.point, pt2))
        return false;
    range = dqGeom::Range3d::CreateXYZXYZ(std::min(pt1.x, pt2.x), std::min(pt1.y, pt2.y),
                                          std::min(pt1.z, pt2.z), std::max(pt1.x, pt2.x),
                                          std::max(pt1.y, pt2.y), std::max(pt1.z, pt2.z));
    return true;
}

// Ported from: onDataButtonDown (:880-914).
EventHandled ViewClipByRangeTool::onDataButtonDown(BeButtonEvent const& ev)
{
    Viewport* targetView = ev.viewport;
    if (targetView == nullptr)
        return EventHandled::No;

    if (m_corner.has_value()) {
        dqGeom::Range3d range;
        dqGeom::Transform transform = dqGeom::Transform::CreateIdentity();
        if (!getClipRange(range, transform, ev))
            return EventHandled::No;
        enableClipVolume(*targetView);
        if (!doClipToRange(*targetView, range, &transform))
            return EventHandled::No;
        if (m_clipEventHandler != nullptr)
            m_clipEventHandler->onNewClip(*targetView);
        onReinitialize();
        return EventHandled::Yes;
    }

    m_corner = ev.point;
    setupAndPromptForNextAction();
    return EventHandled::No;
}

// Ported from: onUndoPreviousStep (:916-921).
bool ViewClipByRangeTool::onUndoPreviousStep()
{
    if (!m_corner.has_value())
        return false;
    m_corner = std::nullopt;
    setupAndPromptForNextAction();
    return true;
}

// ---------------------------------------------------------------------------
// ViewClipByElementTool (:926-1058)
// ---------------------------------------------------------------------------

// Ported from: doClipToElements (:978-1036).
// §3.4：viewport.iModel.elements.getPlacements(ids) → DumpIModelConnection::
// findPlacement 逐 id（M-N(2) placements 数据面——instances60-placements-v1）；
// computeDisplayTransform 无消费者（M-O(2) I11 登记——缺席恒无）。
bool ViewClipByElementTool::doClipToElements(Viewport& viewport,
                                             std::vector<uint64_t> const& ids, bool alwaysUseRange)
{
    DumpIModelConnection* imodel =
        dynamic_cast<DumpIModelConnection*>(viewport.GetView()->GetIModel());
    if (imodel == nullptr)
        return false;

    // 收集 placements（:980-983 —— 0 个 → false）
    std::vector<DumpIModelConnection::PlacementInfo const*> placements;
    for (uint64_t id : ids) {
        DumpIModelConnection::PlacementInfo const* p = imodel->findPlacement(dqBase::DqId(id));
        if (p != nullptr)
            placements.push_back(p);
    }
    if (placements.empty())
        return false;

    dqGeom::Range3d range = dqGeom::Range3d::CreateNull();
    dqGeom::Transform transform = dqGeom::Transform::CreateIdentity();
    if (!alwaysUseRange && 1 == placements.size()) {
        // Use ElementAlignedBox for single selection...（:1030-1036）
        DumpIModelConnection::PlacementInfo const& placement = *placements[0];
        range = dqGeom::Range3d::CreateXYZXYZ(placement.origin[0] + placement.bboxLow[0],
                                              placement.origin[1] + placement.bboxLow[1],
                                              placement.origin[2] + placement.bboxLow[2],
                                              placement.origin[0] + placement.bboxHigh[0],
                                              placement.origin[1] + placement.bboxHigh[1],
                                              placement.origin[2] + placement.bboxHigh[2]);
        // transform.setFrom(placement.transform) —— placement 的 angles → 旋转矩阵。
        transform = dqGeom::Transform(
            dqGeom::Point3d::From(placement.origin[0], placement.origin[1], placement.origin[2]),
            dqGeom::YawPitchRollAngles::FromJsonDegrees(placement.angles[0], placement.angles[1],
                                                        placement.angles[2])
                .ToMatrix3d());
    } else {
        // 多元素合并 range（:992-994 —— calculateRange = origin±bbox 世界域）
        for (DumpIModelConnection::PlacementInfo const* placement : placements) {
            dqGeom::Range3d const worldBox = dqGeom::Range3d::CreateXYZXYZ(
                placement->origin[0] + placement->bboxLow[0], placement->origin[1] + placement->bboxLow[1],
                placement->origin[2] + placement->bboxLow[2], placement->origin[0] + placement->bboxHigh[0],
                placement->origin[1] + placement->bboxHigh[1], placement->origin[2] + placement->bboxHigh[2]);
            range.ExtendRange(worldBox);
        }
    }

    if (range.isNull())
        return false;

    range.scaleAboutCenterInPlace(1.001);  // pad range slightly...（:1000）
    if (range.isAlmostZeroX() || range.isAlmostZeroY()) {
        if (range.isAlmostZeroZ())
            return false;

        // Invalid XY range for clip, see if XZ or YZ can be used instead...（:1005-1027）
        bool const canUseXZ = !range.isAlmostZeroX();
        bool const canUseYZ = !canUseXZ && !range.isAlmostZeroY();
        if (!canUseXZ && !canUseYZ)
            return false;

        dqGeom::Vector3d zDir = canUseXZ ? dqGeom::Vector3d::From(0, 1, 0)  // unitY
                                         : dqGeom::Vector3d::From(1, 0, 0);  // unitX
        std::array<size_t, 4> const indices =
            dqGeom::Range3d::FaceCornerIndices(canUseXZ ? 3 : 1);
        std::array<dqGeom::Point3d, 8> const cornerArray = range.Corners();
        std::vector<dqGeom::Point3d> points;
        for (size_t index : indices)
            points.push_back(cornerArray[index]);

        for (dqGeom::Point3d& p : points)
            p = transform.MultiplyPoint3d(p);
        zDir = transform.MultiplyVector(zDir);
        transform = dqGeom::Transform(points[0], dqGeom::Matrix3d::CreateRigidHeadsUp(zDir));
        for (dqGeom::Point3d& p : points) {
            dqGeom::Point3d const src = p;
            transform.MultiplyInversePoint3d(src, p);
        }
        enableClipVolume(viewport);
        if (!doClipToShape(viewport, points, &transform))
            return false;
        return true;
    }
    enableClipVolume(viewport);
    if (!doClipToRange(viewport, range, &transform))
        return false;
    return true;
}

// Ported from: onDataButtonDown (:1050-1058 —— doLocate 命中 → doClipToElements).
// EQUIVALENCE：locateManager.doLocate 拾取链 → 拾取命中 id 直取
// （PickAtPoint→SelectionSet 命中链，PickDumpScene 先例）。
EventHandled ViewClipByElementTool::onDataButtonDown(BeButtonEvent const& ev)
{
    Viewport* targetView = ev.viewport;
    if (targetView == nullptr)
        return EventHandled::No;
    uint32_t const hit = targetView->PickAtPoint(
        static_cast<int32_t>(ev.viewPoint.x), static_cast<int32_t>(ev.viewPoint.y));
    if (0 == hit)
        return EventHandled::No;
    return doClipToElements(*targetView, std::vector<uint64_t>{hit}, m_alwaysUseRange)
               ? EventHandled::Yes
               : EventHandled::No;
}

}  // namespace dqApp

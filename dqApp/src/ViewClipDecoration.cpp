// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewClip 装饰绘制面 + ViewClipDecoration + Provider
// Ported from: itwinjs-core core/frontend/src/tools/ClipViewTool.ts
//              ViewClipTool 装饰面 (:232-343, :356-382) + ViewClipDecoration
//              (:1285-1997) + ClipEventType (:2003) +
//              ViewClipDecorationProvider (:2008-2073)
//
// M-P P-F。EQUIVALENCE 清单见 ClipViewTool.h 的 ViewClipDecoration 注。
#include <dqApp/ClipViewTool.h>

#include <dqApp/DecorateContext.h>
#include <dqApp/EditManipulator.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/SelectionSet.h>

#include <dqGeom/LineString3d.h>
#include <dqGeom/Path.h>
#include <dqGeom/PolygonOps.h>
#include <dqRender/GraphicBuilder.h>

#include <QSet>

#include <algorithm>
#include <cmath>

namespace dqApp {

namespace {

// 选集事件栈安全面（§3.4——参考为 TS GC 无显式 delete；DanQing 的选集直挂
// 适配使 clear() 可能在该对象自身选集回调的调用栈内执行：stop() 立即跑
// （其内部 Remove 已由变更门保护），delete 延迟到选集回调深度归零——外层
// 帧（updateControls/createControls…）仍在该对象上执行，delete 即 AV。
int g_selectionEventDepth = 0;
std::vector<ViewClipDecoration*> g_pendingDelete;

void FlushPendingDecorationDeletes()
{
    for (ViewClipDecoration* p : g_pendingDelete)
        delete p;
    g_pendingDelete.clear();
}

}  // namespace

// ---------------------------------------------------------------------------
// ViewClipTool 装饰绘制面（:232-343 + :356-382）
// ---------------------------------------------------------------------------

// Ported from: addClipPlanesLoops (:232-238).
void ViewClipTool::addClipPlanesLoops(dqRender::GraphicBuilder& builder,
                                      std::vector<dqBase::RefPtr<dqGeom::Loop>> const& loops,
                                      bool outline)
{
    for (dqBase::RefPtr<dqGeom::Loop> const& geom : loops) {
        if (geom.IsNull())
            continue;
        if (outline) {
            // builder.addPath(Path.createArray(geom.children)) —— loop 子曲线转 Path。
            dqBase::RefPtr<dqGeom::Path> const path = dqGeom::Path::CreateArray(geom->Curves());
            builder.addPath(*path);
        } else {
            builder.addLoop(*geom);
        }
    }
}

// Ported from: addClipShape (:239-246 —— lo/hi 两多边形 + 竖线连接）.
void ViewClipTool::addClipShape(dqRender::GraphicBuilder& builder, dqGeom::ClipShape const& shape,
                                dqGeom::Range1d const& extents)
{
    std::vector<dqGeom::Point3d> const shapePtsLo = getClipShapePoints(shape, extents.low);
    std::vector<dqGeom::Point3d> const shapePtsHi = getClipShapePoints(shape, extents.high);
    for (size_t i = 0; i < shapePtsLo.size(); i++)
        builder.addLineString(std::vector<dqGeom::Point3d>{shapePtsLo[i], shapePtsHi[i]}.data(), 2);
    builder.addLineString(shapePtsLo.data(), shapePtsLo.size());
    builder.addLineString(shapePtsHi.data(), shapePtsHi.size());
}

// Ported from: drawClipShape (:294-309 —— WorldDecoration 轮廓；隐藏线 overlay
// 面 EQUIVALENCE——isHilite/isFlashed 选集查询 + Code2 虚线宿主通道缺席，
// 单 builder 轮廓承载).
void ViewClipTool::drawClipShape(DecorateContext& context, dqGeom::ClipShape const& shape,
                                 dqGeom::Range1d const& extents, dqCommon::ColorDef const& color,
                                 double weight, std::optional<uint32_t> id)
{
    (void)id;
    Viewport& vp = context.GetViewport();
    dqRender::GraphicBuilderOptions opts;
    opts.type = dqRender::GraphicType::WorldDecoration;
    if (shape.transformFromClip() != nullptr)
        opts.placement = *shape.transformFromClip();
    double const worldPerPixel = vp.GetViewingSpace().getPixelSizeAtPoint(nullptr);
    opts.computeChordTolerance = [worldPerPixel]() { return worldPerPixel; };
    auto builder = vp.createGraphicBuilder(opts);
    if (!builder)
        return;
    builder->setSymbology(color, dqCommon::ColorDef::from(0, 0, 0), static_cast<float>(weight));
    addClipShape(*builder, shape, extents);
    context.AddDecoration(dqRender::GraphicType::WorldDecoration, builder->finish());
}

// Ported from: drawClipPlanesLoops (:356-382).
void ViewClipTool::drawClipPlanesLoops(
    DecorateContext& context, std::vector<dqBase::RefPtr<dqGeom::Loop>> const& loops,
    dqCommon::ColorDef const& color, double weight, bool dashed,
    std::optional<dqCommon::ColorDef> fill, std::optional<uint32_t> id)
{
    (void)dashed;  // Code2 虚线宿主通道缺席——实线承载（EQUIVALENCE）。
    (void)id;
    if (loops.empty())
        return;
    Viewport& vp = context.GetViewport();

    dqRender::GraphicBuilderOptions opts;
    opts.type = dqRender::GraphicType::WorldDecoration;
    double const worldPerPixel = vp.GetViewingSpace().getPixelSizeAtPoint(nullptr);
    opts.computeChordTolerance = [worldPerPixel]() { return worldPerPixel; };
    auto builder = vp.createGraphicBuilder(opts);
    if (getenv("DANQING_CLIPDECO_TRACE") && builder) {
        dqGeom::Range3d loopRange = dqGeom::Range3d::CreateNull();
        for (auto const& geom : loops)
            if (!geom.IsNull())
                loopRange.ExtendRange(geom->Range());
        fprintf(stderr, "[CLIPDECO-DRAW] loops=%zu range=[%.2f,%.2f]x[%.2f,%.2f]x[%.2f,%.2f]\n",
                loops.size(), loopRange.low.x, loopRange.high.x, loopRange.low.y,
                loopRange.high.y, loopRange.low.z, loopRange.high.z);
    }
    if (builder) {
        builder->setSymbology(color, dqCommon::ColorDef::from(0, 0, 0),
                              static_cast<float>(weight));
        addClipPlanesLoops(*builder, loops, true);
        if (fill.has_value()) {
            builder->setSymbology(*fill, *fill, 0.0f);
            addClipPlanesLoops(*builder, loops, false);
        }
        dqRender::RenderGraphic* const g = builder->finish();
        if (getenv("DANQING_CLIPDECO_TRACE"))
            fprintf(stderr, "[CLIPDECO-DRAW] finish=%d\n", g != nullptr ? 1 : 0);
        context.AddDecoration(dqRender::GraphicType::WorldDecoration, g);
    }
}

// ---------------------------------------------------------------------------
// ViewClipDecoration（:1310-1997）
// ---------------------------------------------------------------------------

ViewClipDecoration* ViewClipDecoration::s_decorator = nullptr;
uint32_t ViewClipDecoration::s_nextTransientId = 0xFF000000;
bool ViewClipDecoration::s_clearing = false;

// Ported from: onSelectionChanged (:182-185 面) —— 包一层选集事件深度
// （延迟 delete 的释放点；见文件头匿名命名空间注）。
void ViewClipDecoration::onSelectionChanged()
{
    ++g_selectionEventDepth;
    HandleProvider::onSelectionChanged();
    if (--g_selectionEventDepth == 0)
        FlushPendingDecorationDeletes();
}

// Ported from: ctor (:1335-1344).
ViewClipDecoration::ViewClipDecoration(Viewport& clipView,
                                     ViewClipEventHandler* clipEventHandler)
    : HandleProvider(clipView), m_clipEventHandler(clipEventHandler)
{
    if (!getClipData())
        return;
    m_clipId = ++s_nextTransientId;
    updateDecorationListener(true);
    if (m_clipEventHandler != nullptr && m_clipEventHandler->selectOnCreate())
        m_clipView.GetView()->GetIModel()->GetSelectionSet().Replace(
            QSet<uint32_t>{m_clipId});
}

// Ported from: getControlIndex (:1350).
int ViewClipDecoration::getControlIndex(uint32_t id) const
{
    for (size_t i = 0; i < m_controlIds.size(); ++i)
        if (m_controlIds[i] == id)
            return static_cast<int>(i);
    return -1;
}

// Ported from: stop (:1352-1365).
void ViewClipDecoration::stop()
{
    uint32_t const selectedId =
        (m_clipId != 0
         && m_clipView.GetView()->GetIModel()->GetSelectionSet().Contains(m_clipId))
            ? m_clipId
            : 0;
    m_clipId = 0;  // Invalidate id so that decorator will be dropped...
    HandleProvider::stop();
    if (selectedId != 0)
        m_clipView.GetView()->GetIModel()->GetSelectionSet().Remove(QSet<uint32_t>{selectedId});
}

// Ported from: onViewClose (:1367-1370).
void ViewClipDecoration::onViewClose(Viewport& vp)
{
    if (&m_clipView == &vp)
        ViewClipDecoration::clear();
}

// Ported from: getClipData (:1368-1425).
// EQUIVALENCE：>5 点压缩（compressByChapterError）未移植——不压缩（凸/凹
// 全保真）；验证法 = ClipDecorationTest 轮廓循环面。
bool ViewClipDecoration::getClipData()
{
    m_clip = {};
    m_clipShapeExtents = std::nullopt;
    m_clipShape = nullptr;
    m_clipPlanes = nullptr;
    m_clipPlanesLoops.clear();
    m_clipPlanesLoopsNoncontributing.clear();

    dqGeom::ClipVector::Ptr clip = m_clipView.GetView()->getViewClip();
    if (clip.IsNull())
        return false;

    dqGeom::Range3d const viewRange = m_clipView.computeViewRange();

    dqGeom::ClipShape const* clipShape = ViewClipTool::isSingleClipShape(*clip);
    if (clipShape != nullptr) {
        m_clipShapeExtents = ViewClipTool::getClipShapeExtents(*clipShape, viewRange);
        m_clipShape = clipShape;
    } else {
        dqGeom::ConvexClipPlaneSet const* clipPlanes =
            ViewClipTool::isSingleConvexClipPlaneSet(*clip);
        if (clipPlanes == nullptr || clipPlanes->planes.size() > 12) {
            // Show visual representation for ClipVectors that are not supported
            // for modification...（:1389-1416 只读预览 loops）
            for (dqGeom::ClipPrimitive::Ptr const& primitive : clip->clips()) {
                dqGeom::UnionOfConvexClipPlaneSets const* unionSets =
                    primitive->fetchClipPlanesRef();
                if (unionSets == nullptr)
                    continue;
                for (dqGeom::ConvexClipPlaneSet const& convexSet : unionSets->convexSets()) {
                    for (auto& loop :
                         dqGeom::ClipUtilitiesLoops::loopsOfConvexClipPlaneIntersectionWithRange(
                             convexSet, viewRange, true, false, true))
                        m_clipPlanesLoops.push_back(loop);
                }
            }
            if (m_clipPlanesLoops.empty())
                return false;
            m_clip = clip;
            return true;
        }
        m_clipPlanesLoops =
            dqGeom::ClipUtilitiesLoops::loopsOfConvexClipPlaneIntersectionWithRange(
                *clipPlanes, viewRange, true, false, true);
        if (m_clipPlanesLoops.size() > clipPlanes->planes.size())
            return false;
        m_clipPlanes = clipPlanes;
    }
    m_clip = clip;
    return true;
}

// Ported from: ensureNumControls (:1427-1436).
void ViewClipDecoration::ensureNumControls(size_t numReqControls)
{
    size_t const numCurrent = m_controlIds.size();
    if (numCurrent < numReqControls) {
        for (size_t i = numCurrent; i < numReqControls; i++)
            m_controlIds.push_back(++s_nextTransientId);
    } else if (numCurrent > numReqControls) {
        m_controlIds.resize(numReqControls);
    }
}

// Ported from: getLoopCentroidAreaNormal (:1467-1475).
std::optional<dqGeom::Ray3d> ViewClipDecoration::getLoopCentroidAreaNormal(
    dqGeom::Loop const* geom)
{
    if (geom == nullptr || geom->Curves().size() > 1)
        return std::nullopt;
    if (geom->Curves().empty())
        return std::nullopt;
    dqGeom::CurvePrimitivePtr const& child = geom->Curves()[0];
    if (child.IsNull() || child->GetCurveType() != dqGeom::CurveType::LineString)
        return std::nullopt;
    dqGeom::LineString3d const& lineString = static_cast<dqGeom::LineString3d const&>(*child);
    return dqGeom::PolygonOps::centroidAreaNormal(lineString.Points());
}

// Ported from: createClipShapeControls (:1438-1465).
bool ViewClipDecoration::createClipShapeControls()
{
    if (m_clipShape == nullptr || !m_clipShapeExtents.has_value())
        return false;

    std::vector<dqGeom::Point3d> const shapePtsLo =
        ViewClipTool::getClipShapePoints(*m_clipShape, m_clipShapeExtents->low);
    std::vector<dqGeom::Point3d> const shapePtsHi =
        ViewClipTool::getClipShapePoints(*m_clipShape, m_clipShapeExtents->high);
    std::optional<dqGeom::Ray3d> const shapeArea =
        dqGeom::PolygonOps::centroidAreaNormal(shapePtsLo);
    if (!shapeArea.has_value())
        return false;

    size_t const numControls = shapePtsLo.size() + 1;  // edge midpoints + zLow/zHigh
    ensureNumControls(numControls);
    m_controls.clear();
    m_controls.resize(numControls);

    for (size_t i = 0; i + 2 < numControls; i++) {
        dqGeom::Point3d const midPtLo =
            dqGeom::Point3d::FromInterpolate(shapePtsLo[i], 0.5, shapePtsLo[i + 1]);
        dqGeom::Point3d const midPtHi =
            dqGeom::Point3d::FromInterpolate(shapePtsHi[i], 0.5, shapePtsHi[i + 1]);
        dqGeom::Point3d const faceCenter =
            dqGeom::Point3d::FromInterpolate(midPtLo, 0.5, midPtHi);
        dqGeom::Vector3d const edgeTangent =
            dqGeom::Vector3d::FromStartEnd(shapePtsLo[i], shapePtsLo[i + 1]);
        dqGeom::Vector3d faceNormal =
            dqGeom::Vector3d::FromCrossProduct(edgeTangent, shapeArea->direction);
        faceNormal.Normalize();
        m_controls[i] =
            ViewClipControlArrow(faceCenter, faceNormal, shapePtsLo.size() > 5 ? 0.5 : 0.75);
    }

    dqCommon::ColorDef const zFillColor = dqCommon::ColorDef::from(150, 150, 250);
    m_controls[numControls - 2] = ViewClipControlArrow(
        shapeArea->origin, dqGeom::Vector3d::From(0.0, 0.0, -1.0), 0.75, zFillColor,
        std::nullopt, "zLow");
    // zHigh origin = area 质心 + unitZ×(shapePtsLo[0].distance(shapePtsHi[0]))。
    double const zSpan = shapePtsLo[0].Distance(shapePtsHi[0]);
    m_controls[numControls - 1] = ViewClipControlArrow(
        dqGeom::Point3d::From(shapeArea->origin.x, shapeArea->origin.y,
                              shapeArea->origin.z + zSpan),
        dqGeom::Vector3d::From(0.0, 0.0, 1.0), 0.75, zFillColor, std::nullopt, "zHigh");

    return true;
}

// Ported from: createClipPlanesControls (:1477-1533).
bool ViewClipDecoration::createClipPlanesControls()
{
    if (m_clipPlanes == nullptr)
        return false;

    std::vector<dqGeom::Ray3d> loopData;
    for (dqBase::RefPtr<dqGeom::Loop> const& geom : m_clipPlanesLoops) {
        std::optional<dqGeom::Ray3d> const loopArea = getLoopCentroidAreaNormal(geom.Get());
        if (loopArea.has_value())
            loopData.push_back(*loopArea);
    }

    size_t const numControls = m_clipPlanes->planes.size();
    ensureNumControls(numControls);
    m_controls.clear();
    m_controls.resize(numControls);

    dqCommon::ColorDef const nonContribColor = dqCommon::ColorDef::from(250, 100, 100);
    std::optional<dqGeom::Range3d> viewRange;  // computeViewRange 惰性（:1497）
    size_t iLoop = 0;
    for (size_t i = 0; i < numControls; i++) {
        dqGeom::ClipPlane const& srcPlane = m_clipPlanes->planes[i];
        dqGeom::Plane3dByOriginAndUnitNormal const plane = srcPlane.getPlane3d();
        if (iLoop < loopData.size()) {
            if (std::abs(loopData[iLoop].direction.DotProduct(plane.getNormalRef())) > 0.9999
                && plane.isPointInPlane(loopData[iLoop].origin)) {
                dqGeom::Vector3d outwardNormal = loopData[iLoop].direction;
                outwardNormal.Negate();
                m_controls[i] = ViewClipControlArrow(loopData[iLoop].origin, outwardNormal, 0.75);
                iLoop++;
                continue;
            }
        }

        if (!viewRange.has_value())
            viewRange = m_clipView.computeViewRange();
        dqGeom::Point3d const viewCenter = viewRange->Center();
        // 默认原点：viewRange 中心在平面上的投影（plane.projectPointToPlane）。
        dqGeom::Point3d const defaultOrigin = plane.projectPointToPlane(viewCenter);
        dqGeom::Vector3d defaultOutwardNormal = plane.getNormalRef();
        defaultOutwardNormal.Negate();

        // 非贡献面检测（:1514-1529）：单面 planeSet 与 expandedRange 交环。
        dqGeom::Range3d expandedRange = *viewRange;
        expandedRange.ExtendPoint(defaultOrigin);
        dqGeom::ConvexClipPlaneSet const singlePlaneSet =
            dqGeom::ConvexClipPlaneSet::createPlanes({srcPlane});
        std::vector<dqBase::RefPtr<dqGeom::Loop>> const nonContribLoops =
            dqGeom::ClipUtilitiesLoops::loopsOfConvexClipPlaneIntersectionWithRange(
                singlePlaneSet, expandedRange, true, false, true);
        if (!nonContribLoops.empty()) {
            for (auto& loop : nonContribLoops)
                m_clipPlanesLoopsNoncontributing.push_back(loop);
            std::optional<dqGeom::Ray3d> const loopArea =
                getLoopCentroidAreaNormal(nonContribLoops[0].Get());
            if (loopArea.has_value()) {
                dqGeom::Vector3d outwardNormal = loopArea->direction;
                outwardNormal.Negate();
                m_controls[i] = ViewClipControlArrow(loopArea->origin, outwardNormal, 0.5,
                                                     nonContribColor);
                continue;
            }
        }
        m_controls[i] = ViewClipControlArrow(defaultOrigin, defaultOutwardNormal, 0.5,
                                             nonContribColor);  // Just show arrow for right-click menu options...
    }

    return true;
}

// Ported from: createControls (:1535-1563).
bool ViewClipDecoration::createControls()
{
    // Always update to current view clip to handle post-modify, etc.
    if (m_clipId == 0 || !getClipData())
        return false;

    auto const& selection = m_clipView.GetView()->GetIModel()->GetSelectionSet();
    bool showControls = false;
    if (selection.size() <= static_cast<int>(m_controlIds.size()) + 1
        && selection.Contains(m_clipId)) {
        showControls = true;
        if (selection.size() > 1) {
            for (uint32_t val : selection.GetElements()) {
                bool const isKnown =
                    (val == m_clipId)
                    || std::find(m_controlIds.begin(), m_controlIds.end(), val)
                           != m_controlIds.end();
                if (!isKnown)
                    showControls = false;
            }
        }
    }

    if (!showControls) {
        if (m_clipEventHandler != nullptr && m_clipEventHandler->clearOnDeselect())
            ViewClipDecoration::clear();
        return false;
    }

    if (m_clipShape != nullptr)
        return createClipShapeControls();
    if (m_clipPlanes != nullptr)
        return createClipPlanesControls();
    return false;
}

// Ported from: clearControls (:1565-1568).
void ViewClipDecoration::clearControls()
{
    // Remove any selected controls as they won't continue to be displayed...
    QSet<uint32_t> controlSet;
    for (uint32_t id : m_controlIds)
        controlSet.insert(id);
    if (!controlSet.isEmpty())
        m_clipView.GetView()->GetIModel()->GetSelectionSet().Remove(controlSet);
    HandleProvider::clearControls();
}

// Ported from: modifyControls (:1571-1585).
// EQUIVALENCE：拖拽 modify 工具（ViewClipShapeModifyTool/ViewClipPlanesModifyTool
// :1061-1282）依赖 InputCollector 安装调度（DanQing stub）——false 承载 + TODO。
bool ViewClipDecoration::modifyControls(uint32_t /*sourceId*/, BeButtonEvent const& /*ev*/)
{
    return false;
}

// Ported from: doClipPlaneNegate (:1587-1605).
bool ViewClipDecoration::doClipPlaneNegate(int index)
{
    if (m_clipPlanes == nullptr)
        return false;
    if (index < 0 || index >= static_cast<int>(m_clipPlanes->planes.size()))
        return false;

    dqGeom::ConvexClipPlaneSet planeSet = dqGeom::ConvexClipPlaneSet::createEmpty();
    for (int i = 0; i < static_cast<int>(m_clipPlanes->planes.size()); i++) {
        dqGeom::ClipPlane const& plane =
            (i == index ? m_clipPlanes->planes[i].cloneNegated() : m_clipPlanes->planes[i]);
        planeSet.addPlaneToConvexSet(plane);
    }

    if (!ViewClipTool::doClipToConvexClipPlaneSet(m_clipView, planeSet))
        return false;

    onManipulatorEvent(ManipulatorEventType::Accept);
    return true;
}

// Ported from: doClipPlaneClear (:1607-1635).
bool ViewClipDecoration::doClipPlaneClear(int index)
{
    if (m_clipPlanes == nullptr)
        return false;
    if (index < 0 || index >= static_cast<int>(m_clipPlanes->planes.size()))
        return false;

    if (1 == m_clipPlanes->planes.size()) {
        if (!ViewClipTool::doClipClear(m_clipView))
            return false;
        if (m_clipEventHandler != nullptr)
            m_clipEventHandler->onClearClip(m_clipView);
        ViewClipDecoration::clear();
        return true;
    }

    dqGeom::ConvexClipPlaneSet planeSet = dqGeom::ConvexClipPlaneSet::createEmpty();
    for (int i = 0; i < static_cast<int>(m_clipPlanes->planes.size()); i++) {
        if (i == index)
            continue;
        planeSet.addPlaneToConvexSet(m_clipPlanes->planes[i]);
    }

    if (!ViewClipTool::doClipToConvexClipPlaneSet(m_clipView, planeSet))
        return false;

    onManipulatorEvent(ManipulatorEventType::Accept);
    return true;
}

// Ported from: doClipPlaneOrientView (:1727-1755).
// EQUIVALENCE：getLoopPreferredX 的 FrameBuilder/createRigidFromColumns(z,x,ZXY)
// 精配面未移植——createRigidHeadsUp(anchorRay.direction) 承载（参考 :1752 的
// 回退分支同构）；animateFrustumChange 缺席（终态 1:1）。
bool ViewClipDecoration::doClipPlaneOrientView(int index)
{
    if (index < 0 || index >= static_cast<int>(m_controlIds.size()))
        return false;

    Viewport& vp = m_clipView;
    dqGeom::Ray3d const anchorRay = ViewClipTool::getClipRayTransformed(
        m_controls[index].origin, m_controls[index].direction,
        m_clipShape != nullptr ? m_clipShape->transformFromClip() : nullptr);

    ViewState3d* view3d = vp.GetView()->AsViewState3d();
    if (view3d == nullptr)
        return false;
    dqGeom::Matrix3d const targetMatrix =
        dqGeom::Matrix3d::CreateRigidHeadsUp(anchorRay.direction)
            .MultiplyMatrix(view3d->getRotation());
    dqGeom::Transform const rotateTransform =
        dqGeom::Transform::CreateFixedPointAndMatrix(anchorRay.origin, targetMatrix);
    dqCommon::Frustum newFrustum = vp.getFrustum();
    newFrustum.multiply(rotateTransform);
    view3d->SetupFromFrustum(newFrustum);
    vp.synchWithView();
    return true;
}

// Ported from: getWorldUpPlane (:1757-1766).
// EQUIVALENCE：Top 上下文旋转 = 世界 Z（getPlaneInwardNormal 同款登记）；
// ACS/getAuxCoordOrigin 缺席——世界原点承载。
std::optional<dqGeom::Plane3dByOriginAndUnitNormal> ViewClipDecoration::getWorldUpPlane() const
{
    dqGeom::Vector3d const worldUp = dqGeom::Vector3d::From(0.0, 0.0, 1.0);
    dqGeom::Point3d const planePt = dqGeom::Point3d::FromZero();
    return dqGeom::Plane3dByOriginAndUnitNormal::create(planePt, worldUp);
}

// Ported from: isAlignedToWorldUpPlane (:1762-1765 —— isParallelTo(normal, true)
// 双向平行).
bool ViewClipDecoration::isAlignedToWorldUpPlane(
    dqGeom::Plane3dByOriginAndUnitNormal const& plane,
    dqGeom::Transform const* transformFromClip) const
{
    dqGeom::Vector3d const normal =
        (transformFromClip != nullptr
             ? transformFromClip->MultiplyVector(dqGeom::Vector3d::From(0.0, 0.0, 1.0))
             : dqGeom::Vector3d::From(0.0, 0.0, 1.0));
    return plane.getNormalRef().IsParallelTo(normal);
}

// Ported from: isClipShapeAlignedWithWorldUp (:1767-1798).
bool ViewClipDecoration::isClipShapeAlignedWithWorldUp(dqGeom::Range1d* extents)
{
    if (m_clipShape == nullptr || !m_clipShapeExtents.has_value())
        return false;

    std::optional<dqGeom::Plane3dByOriginAndUnitNormal> const plane = getWorldUpPlane();
    if (!plane.has_value()
        || !isAlignedToWorldUpPlane(*plane, m_clipShape->transformFromClip()))
        return false;

    if (extents == nullptr)
        return true;

    dqGeom::Point3d zLow = dqGeom::Point3d::From(0.0, 0.0, m_clipShapeExtents->low);
    dqGeom::Point3d zHigh = dqGeom::Point3d::From(0.0, 0.0, m_clipShapeExtents->high);
    if (m_clipShape->transformFromClip() != nullptr) {
        zLow = m_clipShape->transformFromClip()->MultiplyPoint3d(zLow);
        zHigh = m_clipShape->transformFromClip()->MultiplyPoint3d(zHigh);
    }

    dqGeom::Vector3d const lowDir = dqGeom::Vector3d::FromStartEnd(
        plane->projectPointToPlane(zLow), zLow);
    dqGeom::Vector3d const highDir = dqGeom::Vector3d::FromStartEnd(
        plane->projectPointToPlane(zHigh), zHigh);
    double zLowWorld = lowDir.Magnitude();
    double zHighWorld = highDir.Magnitude();
    if (lowDir.DotProduct(plane->getNormalRef()) < 0.0)
        zLowWorld = -zLowWorld;
    if (highDir.DotProduct(plane->getNormalRef()) < 0.0)
        zHighWorld = -zHighWorld;

    extents->low = zLowWorld;
    extents->high = zHighWorld;
    return true;
}

// Ported from: doClipShapeSetZExtents (:1800-1828).
bool ViewClipDecoration::doClipShapeSetZExtents(dqGeom::Range1d const& extents)
{
    if (extents.low > extents.high)
        return false;
    if (m_clipShape == nullptr)
        return false;
    std::optional<dqGeom::Plane3dByOriginAndUnitNormal> const plane = getWorldUpPlane();
    if (!plane.has_value()
        || !isAlignedToWorldUpPlane(*plane, m_clipShape->transformFromClip()))
        return false;

    // plane.origin + plane.normal×extents（世界面上点）→ transformToClip。
    dqGeom::Point3d zLow = dqGeom::Point3d::From(
        plane->getOriginRef().x + plane->getNormalRef().x * extents.low,
        plane->getOriginRef().y + plane->getNormalRef().y * extents.low,
        plane->getOriginRef().z + plane->getNormalRef().z * extents.low);
    dqGeom::Point3d zHigh = dqGeom::Point3d::From(
        plane->getOriginRef().x + plane->getNormalRef().x * extents.high,
        plane->getOriginRef().y + plane->getNormalRef().y * extents.high,
        plane->getOriginRef().z + plane->getNormalRef().z * extents.high);
    if (m_clipShape->transformToClip() != nullptr) {
        zLow = m_clipShape->transformToClip()->MultiplyPoint3d(zLow);
        zHigh = m_clipShape->transformToClip()->MultiplyPoint3d(zHigh);
    }

    bool const reversed = (zLow.z > zHigh.z);
    dqGeom::ClipShape::Ptr const shape = dqGeom::ClipShape::createFrom(*m_clipShape);
    shape->initSecondaryProps(m_clipShape->isMask(), reversed ? zHigh.z : zLow.z,
                              reversed ? zLow.z : zHigh.z,
                              m_clipShape->transformFromClip());

    dqGeom::ClipVector::Ptr const clip = dqGeom::ClipVector::createEmpty();
    clip->appendReference(shape);

    if (!ViewClipTool::setViewClip(m_clipView, clip))
        return false;

    onManipulatorEvent(ManipulatorEventType::Accept);
    return true;
}

// Ported from: onRightClick (:1831-1835 —— 无 handler → No；有 → 交 handler）.
bool ViewClipDecoration::onRightClick(uint32_t sourceId, BeButtonEvent const& ev)
{
    if (m_clipEventHandler == nullptr)
        return false;
    return m_clipEventHandler->onRightClick(sourceId, ev);
}

// Ported from: getDecorationToolTip (:1850-1853 —— CoreTools.json
// tools.ViewClip.Message 解析值内联)。
QString ViewClipDecoration::GetDecorationToolTip(uint32_t featureId) const
{
    return featureId == m_clipId ? QString("View Clip") : QString("Modify View Clip");
}

// Ported from: updateDecorationListener override (:1855 —— 注册门 = clipId
// 存在性，非 add 形参——装饰器不只是 resize 手柄）。
void ViewClipDecoration::updateDecorationListener(bool /*add*/)
{
    HandleProvider::updateDecorationListener(m_clipId != 0);
}

// Ported from: onManipulatorEvent (:1841-1846).
void ViewClipDecoration::onManipulatorEvent(ManipulatorEventType eventType)
{
    m_suspendDecorator = false;
    HandleProvider::onManipulatorEvent(eventType);
    if (ManipulatorEventType::Accept == eventType && m_clipEventHandler != nullptr)
        m_clipEventHandler->onModifyClip(m_clipView);
}

// Ported from: testDecorationHit (:1848).
bool ViewClipDecoration::TestDecorationHit(uint32_t featureId) const
{
    if (featureId == m_clipId)
        return true;
    return std::find(m_controlIds.begin(), m_controlIds.end(), featureId) != m_controlIds.end();
}

// Ported from: decorate (:1854-1964).
void ViewClipDecoration::Decorate(DecorateContext& context)
{
    if (getenv("DANQING_CLIPDECO_TRACE"))
        fprintf(stderr, "[CLIPDECO-DEC] enter clipId=%u suspend=%d loops=%zu shape=%d\n",
                m_clipId, (int)m_suspendDecorator, m_clipPlanesLoops.size(),
                m_clipShape != nullptr ? 1 : 0);
    if (m_suspendDecorator)
        return;
    if (m_clipId == 0 || m_clip.IsNull())
        return;
    Viewport& vp = context.GetViewport();
    if (&m_clipView != &vp)
        return;

    dqCommon::ColorDef const white =
        HandleUtils::adjustForBackgroundColor(dqCommon::ColorDef::from(255, 255, 255), vp);
    dqCommon::ColorDef const cyanFill = HandleUtils::adjustForBackgroundColor(
        dqCommon::ColorDef::from(0, 255, 255, 225), vp);

    if (m_clipShape != nullptr) {
        ViewClipTool::drawClipShape(context, *m_clipShape,
                      m_clipShapeExtents.value_or(dqGeom::Range1d::CreateNull()), white, 3.0);
    } else if (m_clipPlanes != nullptr) {
        if (!m_clipPlanesLoops.empty())
            ViewClipTool::drawClipPlanesLoops(context, m_clipPlanesLoops, white, 3.0, false, cyanFill);
        if (!m_clipPlanesLoopsNoncontributing.empty()) {
            dqCommon::ColorDef const red =
                HandleUtils::adjustForBackgroundColor(dqCommon::ColorDef::from(255, 0, 0), vp);
            ViewClipTool::drawClipPlanesLoops(context, m_clipPlanesLoopsNoncontributing, red,
                                              1.0, true);
        }
    } else if (!m_clipPlanesLoops.empty()) {
        ViewClipTool::drawClipPlanesLoops(context, m_clipPlanesLoops, white, 3.0, false, cyanFill);
    }

    if (!m_isActive)
        return;

    // 手柄箭头（:1886-1963）。单面 clip 的 floatingOrigin 迁移（isPointVisibleXY
    // 未移植）EQUIVALENCE——手柄恒位于 loop 质心/平面投影点（文件头登记）。
    dqCommon::ColorDef const outlineColor = HandleUtils::adjustForBackgroundColor(
        dqCommon::ColorDef::from(0, 0, 0, 50), vp);
    dqCommon::ColorDef const fillVisColor = HandleUtils::adjustForBackgroundColor(
        dqCommon::ColorDef::from(150, 250, 200, 175), vp);
    dqCommon::ColorDef const fillHidColor = fillVisColor.withAlpha(225);
    dqCommon::ColorDef const fillSelColor = fillVisColor.inverse().withAlpha(75);
    std::vector<dqGeom::Point3d> const shapePts =
        HandleUtils::getArrowShape(0.0, 0.15, 0.55, 1.0, 0.3, 0.5, 0.1);

    for (size_t iFace = 0; iFace < m_controlIds.size(); iFace++) {
        double const sizeInches = m_controls[iFace].sizeInches;
        if (0.0 == sizeInches)
            continue;

        dqGeom::Ray3d const anchorRay = ViewClipTool::getClipRayTransformed(
            m_controls[iFace].origin, m_controls[iFace].direction,
            m_clipShape != nullptr ? m_clipShape->transformFromClip() : nullptr);
        std::optional<dqGeom::Transform> const transform = HandleUtils::getArrowTransform(
            vp, anchorRay.origin, anchorRay.direction, sizeInches);
        if (!transform.has_value())
            continue;

        // per-control 覆盖色（:1928-1942）：覆写色以 adjustForBackgroundColor 重调
        // 后按基色 alpha 重置。
        dqCommon::ColorDef outlineColorOvr = outlineColor;
        if (m_controls[iFace].outline.has_value()) {
            outlineColorOvr = HandleUtils::adjustForBackgroundColor(
                *m_controls[iFace].outline, vp);
            outlineColorOvr = outlineColorOvr.withAlpha(outlineColor.getAlpha());
        }
        dqCommon::ColorDef fillVisColorOvr = fillVisColor;
        dqCommon::ColorDef fillHidColorOvr = fillHidColor;
        dqCommon::ColorDef fillSelColorOvr = fillSelColor;
        if (m_controls[iFace].fill.has_value()) {
            fillVisColorOvr =
                HandleUtils::adjustForBackgroundColor(*m_controls[iFace].fill, vp);
            fillVisColorOvr = fillVisColorOvr.withAlpha(fillVisColor.getAlpha());
            fillHidColorOvr = fillVisColorOvr.withAlpha(fillHidColor.getAlpha());
            fillSelColorOvr = fillVisColorOvr.inverse().withAlpha(fillSelColor.getAlpha());
        }

        bool const isSelected =
            m_clipView.GetView()->GetIModel()->GetSelectionSet().Contains(m_controlIds[iFace]);

        double const worldPerPixel =
            vp.GetViewingSpace().getPixelSizeAtPoint(&anchorRay.origin);

        // WorldOverlay 半边：轮廓 + blanking 填充（:1944-1948）。
        {
            dqRender::GraphicBuilderOptions opts;
            opts.type = dqRender::GraphicType::WorldOverlay;
            opts.placement = *transform;
            opts.computeChordTolerance = [worldPerPixel]() { return worldPerPixel; };
            auto builder = vp.createGraphicBuilder(opts);
            if (!builder)
                continue;
            builder->setSymbology(outlineColorOvr, outlineColorOvr,
                                  isSelected ? 4.0f : 2.0f);
            builder->addLineString(shapePts.data(), shapePts.size());
            builder->setBlankingFill(isSelected ? fillSelColorOvr : fillVisColorOvr);
            builder->addShape(shapePts.data(), shapePts.size());
            context.AddDecoration(dqRender::GraphicType::WorldOverlay, builder->finish());
        }

        // WorldDecoration 半边：隐藏填充（:1950-1953）。
        {
            dqRender::GraphicBuilderOptions opts;
            opts.type = dqRender::GraphicType::WorldDecoration;
            opts.placement = *transform;
            opts.computeChordTolerance = [worldPerPixel]() { return worldPerPixel; };
            auto builder = vp.createGraphicBuilder(opts);
            if (!builder)
                continue;
            builder->setSymbology(fillHidColorOvr, fillHidColorOvr, 1.0f);
            builder->addShape(shapePts.data(), shapePts.size());
            context.AddDecoration(dqRender::GraphicType::WorldDecoration, builder->finish());
        }
    }
}

// --- 单例面（:1966-1996）---

// Ported from: get (:1966-1970).
ViewClipDecoration* ViewClipDecoration::get(Viewport& vp)
{
    if (s_decorator == nullptr || &vp != &s_decorator->m_clipView)
        return nullptr;
    return s_decorator;
}

// Ported from: create (:1972-1979).
std::optional<uint32_t> ViewClipDecoration::create(Viewport& vp,
                                                   ViewClipEventHandler* clipEventHandler)
{
    if (s_decorator != nullptr)
        ViewClipDecoration::clear();
    if (!ViewClipTool::hasClip(vp))
        return std::nullopt;
    s_decorator = new ViewClipDecoration(vp, clipEventHandler);
    return s_decorator->clipId();
}

// Ported from: clear (:1981-1986).
// s_clearing 重入守卫：DanQing 的选集直挂适配（EditManipulator.h 文件头
// EQUIVALENCE）使 clearControls 的 selectionSet.remove 同步触发
// Synch→createControls→clearOnDeselect→clear()；参考经 ToolAdmin
// manipulatorToolEvent 驱动 Synch，无此同步重入。
void ViewClipDecoration::clear()
{
    if (s_decorator == nullptr || s_clearing)
        return;
    s_clearing = true;
    s_decorator->stop();
    ViewClipDecoration* const doomed = s_decorator;
    s_decorator = nullptr;  // 先摘除——重入 get() 即刻见 null
    if (g_selectionEventDepth > 0)
        g_pendingDelete.push_back(doomed);  // 回调栈上——延迟释放
    else
        delete doomed;
    s_clearing = false;
}

// Ported from: toggle (:1988-1994).
std::optional<uint32_t> ViewClipDecoration::toggle(Viewport& vp,
                                                  ViewClipEventHandler* clipEventHandler)
{
    if (s_decorator == nullptr)
        return ViewClipDecoration::create(vp, clipEventHandler);
    ViewClipDecoration::clear();
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// ViewClipDecorationProvider（:2008-2073）
// ---------------------------------------------------------------------------

ViewClipDecorationProvider* ViewClipDecorationProvider::s_provider = nullptr;

// Ported from: onNewClip (:2031-2034).
void ViewClipDecorationProvider::onNewClip(Viewport& viewport)
{
    ViewClipDecoration::create(viewport, this);
    onActiveClipChanged.Raise(viewport, ClipEventType::New, this);
}

// Ported from: onNewClipPlane (:2036-2039).
void ViewClipDecorationProvider::onNewClipPlane(Viewport& viewport)
{
    ViewClipDecoration::create(viewport, this);
    onActiveClipChanged.Raise(viewport, ClipEventType::NewPlane, this);
}

// Ported from: onModifyClip (:2041-2043).
void ViewClipDecorationProvider::onModifyClip(Viewport& viewport)
{
    onActiveClipChanged.Raise(viewport, ClipEventType::Modify, this);
}

// Ported from: onClearClip (:2045-2048).
void ViewClipDecorationProvider::onClearClip(Viewport& viewport)
{
    ViewClipDecoration::clear();
    onActiveClipChanged.Raise(viewport, ClipEventType::Clear, this);
}

// Ported from: onRightClick (:2050-2056 —— 无监听 → 默认 negate :2048-2049).
bool ViewClipDecorationProvider::onRightClick(uint32_t sourceId, BeButtonEvent const& ev)
{
    ViewClipDecoration* decoration =
        (ev.viewport != nullptr ? ViewClipDecoration::get(*ev.viewport) : nullptr);
    if (decoration == nullptr)
        return false;
    if (onActiveClipRightClick.ListenerCount() == 0) {
        int const index = decoration->getControlIndex(sourceId);
        return decoration->doClipPlaneNegate(index);
    }
    onActiveClipRightClick.Raise(sourceId, ev, this);
    return true;
}

// Ported from: show/hide/toggle/isActive (:2058-2061).
void ViewClipDecorationProvider::showDecoration(Viewport& vp)
{
    ViewClipDecoration::create(vp, this);
}
void ViewClipDecorationProvider::hideDecoration()
{
    ViewClipDecoration::clear();
}
std::optional<uint32_t> ViewClipDecorationProvider::toggleDecoration(Viewport& vp)
{
    return ViewClipDecoration::toggle(vp, this);
}
bool ViewClipDecorationProvider::isDecorationActive(Viewport& vp) const
{
    return ViewClipDecoration::get(vp) != nullptr;
}

// Ported from: static create/clear (:2063-2073).
ViewClipDecorationProvider& ViewClipDecorationProvider::create()
{
    if (s_provider == nullptr) {
        ViewClipDecoration::clear();
        s_provider = new ViewClipDecorationProvider();
    }
    return *s_provider;
}
void ViewClipDecorationProvider::clearProvider()
{
    if (s_provider == nullptr)
        return;
    ViewClipDecoration::clear();
    delete s_provider;
    s_provider = nullptr;
}

}  // namespace dqApp

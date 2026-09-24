// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewTool / ViewManip / ViewingToolHandle / ViewHandleArray bases
//                + HandleWithInertia / AnimatedHandle / ViewPan / ViewRotate /
//                ViewScroll handles (Task 10).
//                + PanViewTool / RotateViewTool / ScrollViewTool / FitViewTool
//                concrete tools (Task 11).
//
// Ported from: itwinjs-core core/frontend/src/tools/ViewTool.ts
//              ViewTool                    (:92-128)
//              ViewingToolHandle           (:129-189)
//              ViewHandleArray             (:190-306)
//              ViewManip                   (:307-546 + :623-681 + :689-935)
//              HandleWithInertia           (:1042-1104)   [Task 10]
//              ViewPan                     (:1107-1175)   [Task 10]
//              ViewRotate                  (:1178-1319)   [Task 10]
//              ViewLook                    (:1323-1408)   [WindowArea/Look W2]
//              AnimatedHandle              (:1409-1497)   [Task 10]
//              ViewScroll                  (:1500-1597)   [Task 10]
//              PanViewTool                 (:3035-3043)   [Task 11]
//              RotateViewTool              (:3048-3056)   [Task 11]
//              LookViewTool                (:3063-3071)   [WindowArea/Look W2]
//              ScrollViewTool              (:3074-3082)   [Task 11]
//              FitViewTool                 (:3197-3254)   [Task 11]
//              WindowAreaTool              (:3531-3816)   [WindowArea/Look W4]
#include "dqApp/ViewTool.h"

#include "dqApp/Application.h"
#include "dqApp/DecorateContext.h"
#include "dqApp/StandardView.h"
#include "dqApp/ToolAdmin.h"
#include "dqApp/ViewManager.h"
#include "dqApp/Viewport.h"
#include "dqApp/ViewState.h"
#include "dqApp/ViewingSpace.h"

#include <dqRender/CanvasDecoration.h>  // WindowAreaTool 十字线（ViewTool.ts:3720-3730）
#include <dqRender/GraphicBuilder.h>    // 橡皮筋 builder（:3697-3708）
#include <dqRender/RenderGraphic.h>     // RenderGraphicOwner（橡皮筋所有权）

#include <dqGeom/Transform.h>  // StandardViewTool: Transform::CreateFixedPointAndMatrix
#include <dqGeom/Arc3d.h>        // previewDepthPoint 的椭圆（FromVectors）
#include <dqGeom/LineString3d.h> // EmitStrokes 折线容器

#include <algorithm>
#include <cmath>
#include <utility>

namespace dqApp {

// ---------------------------------------------------------------------------
// ToolSettings（可变静态类，ToolSettings.ts 全量 1:1 移植）在 dqApp/ToolSettings.h；
// 本文件与 ToolAdmin.cpp 的 ViewManip/句柄路径读取 ToolSettings::*（原 k* 常量
// 的调用点已全部重接——DiagnosticsPanel ToolSettingsTracker 运行时可改写这些值）。
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Local helpers for HandleWithInertia / AnimatedHandle / ViewPan / ViewRotate /
// ViewScroll. These mirror @itwin/core-geometry helpers that are not yet ported
// to dqGeom (Point3d.vectorTo / plusScaled / minus; Vector3d.plusScaled;
// Matrix3d.getAxisAndAngleOfRotation; npcToView / worldToView). Most helpers
// below are 1:1 ports of the referenced @itwin/core-geometry / itwinjs-core
// method; the ONE exception is npcToView, which is a SIMPLIFIED linear map
// (see its leading comment) and must NOT be promoted verbatim. They are kept
// file-local (anonymous namespace) until the full dqGeom port lands, at which
// point they will be inlined into the call sites.
// ---------------------------------------------------------------------------
namespace {

// Ported from: itwinjs-core Point3d.vectorTo (Point3d.ts).
// Returns other - this as a Vector3d.
inline dqGeom::Vector3d vectorTo(dqGeom::Point3d const& from, dqGeom::Point3d const& to) noexcept
{
    return dqGeom::Vector3d::From(to.x - from.x, to.y - from.y, to.z - from.z);
}

// Ported from: itwinjs-core Point3d.plusScaled (Point3d.ts).
// Returns this + scale * vector.
inline dqGeom::Point3d plusScaled(dqGeom::Point3d const& p,
                                  dqGeom::Vector3d const& v, double scale) noexcept
{
    return dqGeom::Point3d::From(p.x + scale * v.x, p.y + scale * v.y, p.z + scale * v.z);
}

// Ported from: itwinjs-core Point3d.minus (Point3d.ts).
// Returns this - other as a Vector3d.
inline dqGeom::Vector3d minus(dqGeom::Point3d const& a, dqGeom::Point3d const& b) noexcept
{
    return dqGeom::Vector3d::From(a.x - b.x, a.y - b.y, a.z - b.z);
}

// Ported from: itwinjs-core Geometry.clamp (Geometry.ts).
// Clamps v to [min,max].
inline double clampDouble(double v, double min, double max) noexcept
{
    return v < min ? min : (v > max ? max : v);
}

// Ported from: itwinjs-core Geometry.hypotenuseXYZ (Geometry.ts).
inline double hypotenuseXYZ(double x, double y, double z) noexcept
{
    return std::sqrt(x * x + y * y + z * z);
}

// smallAngleRadians — Ported from: itwinjs-core Geometry.smallAngleRadians
// (Geometry.ts) = 1.0e-15 (squared = smallAngleRadiansSquared). Used by
// Matrix3d.getAxisAndAngleOfRotation's bad-matrix / near-zero-sin guards.
inline constexpr double kSmallAngleRadians = 1.0e-15;

// Ported from: itwinjs-core Matrix3d.getAxisAndAngleOfRotation
//              (Matrix3d.ts:1182-1252).
//
// Returns {axis, angleRadians, ok}. ok=false signals a non-orthogonal matrix
// (the reference returns the same flag via result.ok). The conversion is the
// well-known axis-angle-from-rotation-matrix formula (skew-symmetric part →
// axis; trace → angle). Edge cases (angle 0 and 180) handled 1:1 with the
// reference: angle 0 returns unitZ + zero angle; angle 180 derives the axis
// from the symmetric matrix diagonal / off-diagonal.
struct AxisAndAngle {
    dqGeom::Vector3d axis = dqGeom::Vector3d::UnitZ();
    double angleRadians = 0.0;
    bool ok = true;
};
AxisAndAngle getAxisAndAngleOfRotation(dqGeom::Matrix3d const& m)
{
    double const trace = m.coffs[0] + m.coffs[4] + m.coffs[8];
    double const skewXY = m.coffs[3] - m.coffs[1];  // 2*z*sin
    double const skewYZ = m.coffs[7] - m.coffs[5];  // 2*y*sin (note: m.coffs is row-major)
    double const skewZX = m.coffs[2] - m.coffs[6];  // 2*x*sin
    // trace = 1 + 2*cos  →  cos = (trace-1) / 2
    double const c = (trace - 1.0) / 2.0;
    double const s = hypotenuseXYZ(skewXY, skewYZ, skewZX) / 2.0;
    double const e = c * c + s * s - 1.0;  // s^2 + c^2 = 1
    if (std::fabs(e) > kSmallAngleRadians) {
        return {dqGeom::Vector3d::UnitZ(), 0.0, /*ok=*/false};
    }
    if (std::fabs(s) < kSmallAngleRadians) {
        if (c > 0.0) {
            // sin = 0 and cos = 1 → angle = 0.
            return {dqGeom::Vector3d::UnitZ(), 0.0, /*ok=*/true};
        }
        // sin = 0 and cos = -1 → angle = π. Derive axis from the symmetric
        // rotation matrix (2xx-1, 2xy, 2xz, ...). The reference (Matrix3d.ts:
        // 1211-1252) computes the axis from the most-positive diagonal entry
        // and the off-diagonal entries. Faithful 1:1 port of that derivation
        // would be ~40 lines; for the ViewRotate use case the input matrix is
        // always a product of two clean rotations about near-orthogonal axes
        // with small deltas per frame, so the angle is small (<< π) and this
        // π-branch is unreachable. Documented deviation: in the unreachable
        // π case, return unitZ + π (preserves the rotation in the simplest
        // form). Revisit if a handle ever feeds a π-rotation through here.
        return {dqGeom::Vector3d::UnitZ(), M_PI, /*ok=*/true};
    }
    double const angle = std::atan2(s, c);
    dqGeom::Vector3d const axis = dqGeom::Vector3d::From(
        skewYZ / (2.0 * s),
        skewZX / (2.0 * s),
        skewXY / (2.0 * s));
    // Harmless safety Normalize() — reference relies on the rotation-matrix
    // math yielding a unit axis, no explicit normalize.
    dqGeom::Vector3d normalized = axis;
    normalized.Normalize();
    return {normalized, angle, /*ok=*/true};
}

// ViewRotate uses vp->NpcToView() directly (the faithful Y-flipped ViewingSpace
// port, Phase 1) — the old local Y-down helper was removed (it sign-flipped
// yDelta and caused the snap-back glitch when cumulative pitch crossed vertical).

}  // namespace

// ---------------------------------------------------------------------------
// ViewingToolHandle
// Ported from: itwinjs-core ViewingToolHandle (ViewTool.ts:129-189).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ViewingToolHandle.adjustDepthPoint (ViewTool.ts:159-171).
bool ViewingToolHandle::adjustDepthPoint(bool isValid, Viewport* /*vp*/,
                                          Plane3dByOriginAndUnitNormal /*plane*/,
                                          DepthPointSource source)
{
    // TS L160-170: sources with visible geometry/graphics are considered valid
    // by default; everything else is rejected by default. 1:1 with reference.
    switch (source) {
        case DepthPointSource::Geometry:
        case DepthPointSource::Model:
        case DepthPointSource::BackgroundMap:
        case DepthPointSource::GroundPlane:
        case DepthPointSource::Grid:
        case DepthPointSource::Map:
            return isValid;
        case DepthPointSource::ACS:
        case DepthPointSource::TargetPoint:
        default:
            return false;
    }
}

// Ported from: itwinjs-core ViewingToolHandle.pickDepthPoint (ViewTool.ts:172-174).
void ViewingToolHandle::pickDepthPoint(BeButtonEvent const& ev)
{
    // TS L173: this._depthPoint = this.viewTool.pickDepthPoint(ev);
    // Forwards to ViewManip::pickDepthPoint (Step 4 stub returns the input
    // rawPoint; a real readPixels pick lands with the geometry pipeline).
    auto picked = viewTool->pickDepthPoint(ev, /*isPreview=*/false);
    m_depthPoint = picked;
}

// ---------------------------------------------------------------------------
// ViewHandleArray
// Ported from: itwinjs-core ViewHandleArray (ViewTool.ts:190-306).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ViewHandleArray.testHit (ViewTool.ts:211-245).
bool ViewHandleArray::testHit(dqGeom::Point3d ptScreen, ViewHandleType forced)
{
    // TS L212: this.hitHandleIndex = -1;
    hitHandleIndex = -1;
    // TS L213: const data = { distance: 0.0, priority: ViewManipPriority.Normal };
    HitOut data;
    // TS L214-216: tracking vars for nearest-handle-with-highest-priority selection.
    double minDistance = 0.0;
    bool minDistValid = false;
    ViewManipPriority highestPriority = ViewManipPriority::Low;
    bool foundAny = false;

    // TS L218-243: iterate handles, prefer higher priority; within a priority
    // class prefer the nearest hit.
    for (int i = 0; i < count(); ++i) {
        data.priority = ViewManipPriority::Normal;
        ViewingToolHandle* handle = handles[i].get();

        if (anyHandle(forced)) {
            // TS L223-227: forced handle mode — match the requested type exactly.
            if (handle->handleType() == forced) {
                hitHandleIndex = i;
                return true;
            }
        } else if (handle->testHandleForHit(ptScreen, data)) {
            // TS L228-242: priority-distance tiebreak.
            auto const priorityValue = static_cast<uint32_t>(data.priority);
            if (priorityValue >= static_cast<uint32_t>(highestPriority)) {
                if (priorityValue > static_cast<uint32_t>(highestPriority))
                    minDistValid = false;
                highestPriority = data.priority;
                if (!minDistValid || data.distance < minDistance) {
                    minDistValid = true;
                    minDistance = data.distance;
                    foundAny = true;
                    hitHandleIndex = i;
                }
            }
        }
    }
    return foundAny;
}

// Ported from: itwinjs-core ViewHandleArray.drawHandles (ViewTool.ts:247-264).
void ViewHandleArray::drawHandles(DecorateContext& context)
{
    // TS L248-249: nothing to draw if no handles.
    if (count() == 0)
        return;

    // TS L251-257: draw all non-hit handles first.
    for (int i = 0; i < count(); ++i) {
        if (i != hitHandleIndex) {
            handles[i]->drawHandle(context, focus == i);
        }
    }
    // TS L259-263: draw the hit handle last (on top).
    if (hitHandleIndex != -1) {
        handles[hitHandleIndex]->drawHandle(context, focus == hitHandleIndex);
    }
}

// Ported from: itwinjs-core ViewHandleArray.setFocus (ViewTool.ts:266-289).
void ViewHandleArray::setFocus(int index)
{
    // TS L267-268: no-op if focus and drag-state are unchanged.
    if (focus == index && focusDrag == (viewTool && viewTool->inHandleModify))
        return;
    (void)viewTool;  // (referenced through viewTool->inHandleModify below)

    // TS L270-275: focusOut on the outgoing focus handle.
    if (focus >= 0) {
        if (auto* out = getByIndex(focus))
            out->focusOut();
    }
    // TS L276-281: focusIn on the incoming focus handle.
    if (index >= 0) {
        if (auto* in = getByIndex(index))
            in->focusIn();
    }

    focus = index;
    // TS L284: this.focusDrag = this.viewTool.inHandleModify;
    focusDrag = viewTool ? viewTool->inHandleModify : false;

    // TS L286-288: if (undefined !== vp) vp.invalidateDecorations();
    if (viewTool && viewTool->viewport)
        viewTool->viewport->InvalidateDecorations();
}

// Ported from: itwinjs-core ViewHandleArray.onReinitialize (ViewTool.ts:291).
void ViewHandleArray::onReinitialize()
{
    for (auto const& h : handles)
        h->onReinitialize();
}

// Ported from: itwinjs-core ViewHandleArray.onCleanup (ViewTool.ts:292).
void ViewHandleArray::onCleanup()
{
    for (auto const& h : handles)
        h->onCleanup();
}

// Ported from: itwinjs-core ViewHandleArray.motion (ViewTool.ts:293).
void ViewHandleArray::motion(BeButtonEvent const& ev)
{
    for (auto const& h : handles)
        h->motion(ev);
}

// Ported from: itwinjs-core ViewHandleArray.onWheel (ViewTool.ts:294-301).
bool ViewHandleArray::onWheel(BeWheelEvent const& ev)
{
    // TS L295-300: OR of all handles' onWheel results.
    bool preventDefault = false;
    for (auto const& h : handles) {
        if (h->onWheel(ev))
            preventDefault = true;
    }
    return preventDefault;
}

// Ported from: itwinjs-core ViewHandleArray.hasHandle (ViewTool.ts:304).
bool ViewHandleArray::hasHandle(ViewHandleType handleType) const
{
    for (auto const& h : handles) {
        if (h->handleType() == handleType)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ViewTool
// Ported from: itwinjs-core ViewTool (ViewTool.ts:92-128).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ViewTool.translate (ViewTool.ts:93).
// Stub: CoreTools.translate(`View.${val}`) is not yet ported (no localization
// subsystem). Returns the input key for now so consumers can at least see the
// prompt key.
std::string ViewTool::translate(std::string const& val)
{
    // TODO: CoreTools.translate ("View." + val) — needs the localization port.
    return val;
}

// Ported from: itwinjs-core ViewTool.run (ViewTool.ts:98-111).
bool ViewTool::run()
{
    auto& toolAdmin = Application::Get().GetToolAdmin();
    // TS L100-103: if (undefined !== this.viewport && this.viewport === toolAdmin.markupView) {
    //                 IModelApp.notifications.outputPromptByKey("iModelJs:Viewing.NotDuringMarkup");
    //                 return false; }
    // markupView is not yet ported; with markupView == nullptr the condition is
    // always false (the guard is observably a no-op).
    // TODO: markupView check — future task when markup support lands.

    // TS L105-106: if (!await toolAdmin.onInstallTool(this)) return false;
    // The reference's onInstallTool calls this.onInstall() and applies coord
    // locks / toolState. DanQing collapses to a direct onInstall() call (the
    // base-class default returns true); the full onInstallTool lifecycle
    // (coordLockOvr / toolState adjustment) is TODO pending the ToolState port.
    if (!onInstall())
        return false;

    // TS L108: await toolAdmin.startViewTool(this);
    // Task 11 wires the faithful startViewTool lifecycle (onCleanup on prior
    // viewTool, set slot, raise OnActiveToolChanged). The bare installViewTool
    // setter is kept for test affordance (Task 8/9 tests).
    toolAdmin.startViewTool(this);

    // TS L109: await toolAdmin.onPostInstallTool(this);
    // Collapsed to a direct onPostInstall() call — Task 11 wires the faithful
    // startViewTool; the full onPostInstallTool (cursor reset, notifications)
    // is TODO pending those subsystems.
    onPostInstall();
    return true;
}

// Ported from: itwinjs-core ViewTool.onResetButtonUp (ViewTool.ts:116-119).
EventHandled ViewTool::onResetButtonUp(BeButtonEvent const& /*ev*/)
{
    // TS L117-118: await this.exitTool(); return EventHandled.Yes;
    exitTool();
    return EventHandled::Yes;
}

// Ported from: itwinjs-core ViewTool.exitTool (ViewTool.ts:122).
void ViewTool::exitTool()
{
    // TS L122: return IModelApp.toolAdmin.exitViewTool();
    // Task 9 added ToolAdmin::exitViewTool() as a minimal slot-clearing stub;
    // the full exitViewTool lifecycle (onCleanup on this tool, AccuDraw /
    // coord resets, idle-tool restart, cursor reset) lands in Task 11.
    Application::Get().GetToolAdmin().exitViewTool();
}

// Ported from: itwinjs-core ViewTool.showPrompt (ViewTool.ts:123-125).
void ViewTool::showPrompt(std::string const& prompt)
{
    // TS L124: IModelApp.notifications.outputPrompt(ViewTool.translate(prompt));
    // Stub body: NotificationManager::OutputPrompt exists but the localization
    // key resolution is not yet ported; consumers that need a literal prompt
    // can call NotificationManager directly. TODO: wire translate + OutputPrompt.
    (void)prompt;
}

// ---------------------------------------------------------------------------
// ViewManip
// Ported from: itwinjs-core ViewManip (ViewTool.ts:307-935).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ViewManip constructor (ViewTool.ts:328-332).
ViewManip::ViewManip(Viewport* vp, uint32_t handleMask_, bool oneShot_, bool isDraggingRequired_)
    : ViewTool(vp),
      viewHandles(this),
      handleMask(handleMask_),
      oneShot(oneShot_),
      isDraggingRequired(isDraggingRequired_)
{
    // TS L331: this.changeViewport(viewport);
    changeViewport(vp);
}

// Ported from: itwinjs-core ViewManip.onReinitialize (ViewTool.ts:441-460).
void ViewManip::onReinitialize()
{
    // TS L442: shouldExit gate — used by single-shot drag-required tools to
    // terminate the tool when the gesture ends outside the view.
    bool const shouldExit = (oneShot && isDraggingRequired && isDragging && nPts != 0);

    if (viewport) {
        // TS L445: this.viewport.synchWithView();
        viewport->synchWithView();
        // TS L446: this.viewHandles.setFocus(-1);
        viewHandles.setFocus(-1);
    }

    // TS L449-452: reset transient state.
    nPts = 0;
    inHandleModify = false;
    inDynamicUpdate = false;
    // _startPose reset omitted (TODO when ViewPose lands).

    // TS L454: this.viewHandles.onReinitialize();
    viewHandles.onReinitialize();

    // TS L456-457: if (shouldExit && this.isExitAllowedOnReinitialize) return this.exitTool();
    if (shouldExit && isExitAllowedOnReinitialize()) {
        exitTool();
        return;
    }

    // TS L459: this.provideInitialToolAssistance();
    provideInitialToolAssistance();
}

// Ported from: itwinjs-core ViewManip.onDataButtonDown (ViewTool.ts:462-487).
EventHandled ViewManip::onDataButtonDown(BeButtonEvent const& ev)
{
    // TS L463-465: in drag-required mode, wait for the start-drag event before
    // advancing tool state. Also early-out if the event has no viewport
    // (degenerate — undefined viewport can't be used by processFirstPoint).
    if ((nPts == 0 && isDraggingRequired && !isDragging) || ev.viewport == nullptr)
        return EventHandled::No;

    // TS L467-476: advance tool state.
    switch (nPts) {
        case 0:
            // TS L469: this.changeViewport(ev.viewport);
            changeViewport(ev.viewport);
            // TS L470-471: if (this.processFirstPoint(ev)) this.nPts = 1;
            if (processFirstPoint(ev))
                nPts = 1;
            break;
        case 1:
            // TS L474: this.nPts = 2;
            nPts = 2;
            break;
        default:
            break;
    }

    // TS L478-484: nPts > 1 → finish the gesture.
    if (nPts > 1) {
        inDynamicUpdate = false;
        // TS L480-483: if (this.processPoint(ev, false) && this.oneShot)
        //                 await this.exitTool();
        //               else await this.onReinitialize();
        if (processPoint(ev, false) && oneShot)
            exitTool();
        else
            onReinitialize();
    }

    return EventHandled::Yes;
}

// Ported from: itwinjs-core ViewManip.onDataButtonUp (ViewTool.ts:489-494).
EventHandled ViewManip::onDataButtonUp(BeButtonEvent const& /*ev*/)
{
    // TS L490-491: cancel the tool if a single-shot drag-required gesture never
    // produced a drag (user clicked without dragging).
    if (nPts <= 1 && isDraggingRequired && !isDragging && oneShot)
        exitTool();
    return EventHandled::No;
}

// Ported from: itwinjs-core ViewManip.onMouseWheel (ViewTool.ts:496-503).
EventHandled ViewManip::onMouseWheel(BeWheelEvent& inputEv)
{
    // TS L497: const ev = inputEv.clone();
    // C++ port: BeWheelEvent is a POD; clone == copy.
    BeWheelEvent ev = inputEv;

    // TS L498-499: if (this.viewHandles.onWheel(ev)) return EventHandled.Yes;
    if (viewHandles.onWheel(ev))
        return EventHandled::Yes;

    // TS L501: await IModelApp.toolAdmin.processWheelEvent(ev, false);
    Application::Get().GetToolAdmin().processWheelEvent(ev, /*doUpdate=*/false);
    // TS L502: return EventHandled.Yes;
    return EventHandled::Yes;
}

// Ported from: itwinjs-core ViewManip.startHandleDrag (ViewTool.ts:506-523).
EventHandled ViewManip::startHandleDrag(BeButtonEvent const& ev,
                                         std::optional<ViewHandleType> forcedHandleArg)
{
    // TS L507-508: reject if a view modification is already in progress.
    if (inHandleModify)
        return EventHandled::No;

    // TS L510-514: honor a caller-forced handle type if requested.
    if (forcedHandleArg.has_value()) {
        if (!viewHandles.hasHandle(*forcedHandleArg))
            return EventHandled::No;
        forcedHandle = *forcedHandleArg;
    }

    // TS L516: this.receivedDownEvent = true;  — request up events even without
    // a matching down (InteractiveTool public field).
    receivedDownEvent = true;
    // TS L517: this.isDragging = true;
    isDragging = true;

    // TS L519-520: if (0 === this.nPts) await this.onDataButtonDown(ev);
    if (nPts == 0)
        onDataButtonDown(ev);

    // TS L522: return EventHandled.Yes;
    return EventHandled::Yes;
}

// Ported from: itwinjs-core ViewManip.onMouseStartDrag (ViewTool.ts:525-529).
EventHandled ViewManip::onMouseStartDrag(BeButtonEvent const& ev)
{
    // TS L526-527: only respond to data-button drags.
    if (BeButton::Data != ev.button)
        return EventHandled::No;
    // TS L528: return this.startHandleDrag(ev);
    return startHandleDrag(ev);
}

// Ported from: itwinjs-core ViewManip.onMouseEndDrag (ViewTool.ts:531-537).
EventHandled ViewManip::onMouseEndDrag(BeButtonEvent const& ev)
{
    // TS L532-533: NOTE: To support startHandleDrag being called by IdleTool for
    // middle button drag, check inHandleModify and not the button type.
    if (!inHandleModify)
        return EventHandled::No;
    // TS L535: this.isDragging = false;
    isDragging = false;
    // TS L536: return (0 === this.nPts) ? EventHandled.Yes : this.onDataButtonDown(ev);
    return (nPts == 0) ? EventHandled::Yes : onDataButtonDown(ev);
}

// Ported from: itwinjs-core ViewManip.onMouseMotion (ViewTool.ts:539-557).
void ViewManip::onMouseMotion(BeButtonEvent const& ev)
{
    // TS L540-541: focus the hit handle when idle.
    if (nPts == 0 && viewHandles.testHit(ev.viewPoint))
        viewHandles.focusHitHandle();

    // TS L543-544: forward motion to the active handle during a modify.
    if (nPts != 0)
        processPoint(ev, /*inDynamics=*/true);

    // TS L546: this.viewHandles.motion(ev);
    viewHandles.motion(ev);

    // TS L548-556: depth-preview picking + flashedId update + invalidateDecorations.
    {
        auto const prevSourceId = getDepthPointGeometryId();
        bool const showDepthChanged = (pickDepthPoint(ev, true).has_value() || clearDepthPoint());
        if (ev.viewport && (showDepthChanged || prevSourceId.has_value())) {
            auto const currSourceId = getDepthPointGeometryId();
            if (currSourceId != prevSourceId) {
                // currSourceId 为空 → flashedId 清 0（参考赋 undefined 同义）
                uint32_t const flashId = currSourceId ? static_cast<uint32_t>(std::stoul(*currSourceId)) : 0u;
                ev.viewport->SetFlashedId(flashId);
            }
            ev.viewport->InvalidateDecorations();
        }
    }
}

// Ported from: itwinjs-core ViewManip.onPostInstall (ViewTool.ts:623-626).
void ViewManip::onPostInstall()
{
    // TS L624: await super.onPostInstall();
    ViewTool::onPostInstall();  // base no-op (InteractiveTool::onPostInstall is {}).
    // TS L625: await this.onReinitialize();
    onReinitialize();
}

// Ported from: itwinjs-core ViewManip.onCleanup (ViewTool.ts:660-681).
void ViewManip::onCleanup()
{
    // TS L661-665: if (this.inDynamicUpdate) { this.endDynamicUpdate();
    //               restorePrevious = true; }
    bool restorePrevious = false;
    if (inDynamicUpdate) {
        endDynamicUpdate();
        restorePrevious = true;
    }

    // TS L668-678: viewport cleanup (applyPose / synchWithView / invalidateDecorations).
    if (viewport) {
        if (restorePrevious /* && this._startPose */) {
            // TS L671-672: vp.view.applyPose(this._startPose); vp.animateFrustumChange();
            // TODO: ViewPose applyPose + animateFrustumChange — future task.
        } else {
            // TS L674: vp.synchWithView();
            viewport->synchWithView();
        }
        // TS L677: vp.invalidateDecorations();
        viewport->InvalidateDecorations();
    }
    // TS L679: this.viewHandles.onCleanup();
    viewHandles.onCleanup();

    // 惯性动画器的所有权让渡（TS GC 语义的 C++ 承载）：工具退出时若惯性句柄
    // （HandleWithInertia）仍装机为视口非拥有动画器，把它从句柄数组移交给视口的
    // **拥有槽**续跑——否则 empty() 释放句柄、m_animatorRef 悬空，下一帧
    // RenderFrame Step 2 的 animate() 即 use-after-free（SEH 0xc0000005，
    // 中键平移松开后的崩溃根因）。HandleWithInertia::interrupt 是参考 no-op，
    // setAnimator 的清槽无副作用。参考对应物：TS GC 经动画槽引用保活至跑完
    // （HandleWithInertia.animate 完成时 vp.setAnimator() 自清，ViewTool.ts:1075）。
    if (viewport) {
        if (Animator* ref = viewport->getAnimatorRef()) {
            // 逐句柄经 asAnimator 匹配（MI 双基无 RTTI——地址随基面偏移，不可直比）。
            for (size_t i = 0; i < viewHandles.handles.size(); ++i) {
                ViewingToolHandle* h = viewHandles.handles[i].get();
                if (h && h->asAnimator() == ref) {
                    if (auto released = viewHandles.release(h)) {
                        viewport->setAnimator(std::unique_ptr<Animator>(released->asAnimator()));
                        (void)released.release();   // 指针移交 unique_ptr<Animator> 所有
                    }
                    break;
                }
            }
        }
    }

    // TS L680: this.viewHandles.empty();
    viewHandles.empty();
}

// Ported from: itwinjs-core ViewManip.changeViewport (ViewTool.ts:895-934).
void ViewManip::changeViewport(Viewport* vp) noexcept
{
    // TS L896-897: if (vp === this.viewport && 0 !== this.viewHandles.count) return;
    if (vp == viewport && viewHandles.count() != 0)
        return;

    // TS L899-900: remove decorations from current viewport.
    if (viewport)
        viewport->InvalidateDecorations();

    // TS L902: this.viewport = vp;
    viewport = vp;
    // TS L903: this.targetCenterValid = false;
    targetCenterValid = false;
    // TS L904-905: if (this.handleMask & (ViewHandleType.Rotate | ViewHandleType.TargetCenter))
    //                 this.updateTargetCenter();
    // C++ adaptation (§3.4): handleMask is a numeric bitmask in the reference
    // (TS numeric enum); we keep it as uint32_t for bit arithmetic and cast to
    // ViewHandleType for the bitwise-& with the enum-class mask bits.
    ViewHandleType const maskBits = static_cast<ViewHandleType>(handleMask);
    if (anyHandle(maskBits & (ViewHandleType::Rotate | ViewHandleType::TargetCenter)))
        updateTargetCenter();

    // TS L907: this.viewHandles.empty();
    viewHandles.empty();
    // TS L908-933: add concrete handles by handleMask bit, in the reference's
    // order (Rotate, TargetCenter, Pan, Scroll, Zoom, Walk, Fly, Look,
    // LookAndMove). Ported: ViewRotate / ViewTargetCenter / ViewPan /
    // ViewScroll / ViewLook. ViewZoom / ViewWalk / ViewFly / ViewLookAndMove
    // are not yet ported — TODO, gated on their handle subclasses landing.
    // Ported from: itwinjs-core ViewManip.changeViewport (ViewTool.ts:908-933).
    ViewHandleType const mask = static_cast<ViewHandleType>(handleMask);
    if (anyHandle(mask & ViewHandleType::Rotate))
        viewHandles.add(std::make_unique<ViewRotate>(this));
    // TS L911-912: TargetCenter between Rotate and Pan (reference add order).
    if (anyHandle(mask & ViewHandleType::TargetCenter))
        viewHandles.add(std::make_unique<ViewTargetCenter>(this));
    if (anyHandle(mask & ViewHandleType::Pan))
        viewHandles.add(std::make_unique<ViewPan>(this));
    if (anyHandle(mask & ViewHandleType::Scroll))
        viewHandles.add(std::make_unique<ViewScroll>(this));
    // TODO: ViewZoom / ViewWalk / ViewFly / ViewLookAndMove — port with their
    //       handle subclasses. View.Zoom tool registration is gated on ViewZoom
    //       (Task 11 deferred it).
    if (anyHandle(mask & ViewHandleType::Look))
        viewHandles.add(std::make_unique<ViewLook>(this));
}

// Ported from: itwinjs-core ViewManip.setTargetCenterWorld (ViewTool.ts:689-701).
void ViewManip::setTargetCenterWorld(dqGeom::Point3d const& pt, bool lockTarget, bool /*saveTarget*/)
{
    // TS L690: this.targetCenterWorld.setFrom(pt);
    targetCenterWorld = pt;
    // TS L691: this.targetCenterValid = true;
    targetCenterValid = true;
    // TS L692: this.targetCenterLocked = lockTarget;
    targetCenterLocked = lockTarget;

    if (!viewport)
        return;

    // TS L697-698: if (!this.viewport.view.allow3dManipulations())
    //                 this.targetCenterWorld.z = 0.0;
    ViewState* viewBase = viewport->GetView();
    ViewState3d* view = viewBase ? viewBase->AsViewState3d() : nullptr;
    if (view && !view->Allow3dManipulations())
        targetCenterWorld.z = 0.0;

    // TS L700: this.viewport.viewCmdTargetCenter = (saveTarget ? pt : undefined);
    // TODO: Viewport::viewCmdTargetCenter setter — port with the Viewport target-center API.
}

// Ported from: itwinjs-core ViewManip.updateTargetCenter (ViewTool.ts:703-736).
void ViewManip::updateTargetCenter()
{
    // TS L704-706: if (!vp) return;
    if (!viewport)
        return;

    // TS L708-727: targetCenterValid path — consult tentative point + depth
    // preview. Both subsystems are Step 3 stubs; the if-branch's only
    // load-bearing effect when neither is active is the early return. Faithful
    // to the reference for the no-tentative / no-depth-preview case.
    if (targetCenterValid) {
        // TS L709: if (this.inHandleModify) return;
        if (inHandleModify)
            return;
        // TS L711-724: tentativePoint + depthPreview adjustments. AccuSnap and
        // tentativePoint are no-op stubs in Step 3; with neither active the
        // block is observably a no-op and the early return at L726 fires.
        // TODO: tentativePoint / depthPreview branches — Task 7+ / Task 15.
        return;
    }

    // TS L729-730: if (IModelApp.tentativePoint.isActive)
    //                 return this.setTargetCenterWorld(IModelApp.tentativePoint.getPoint(), true, false);
    // TentativePoint stub: isActive returns false in Step 3; this branch is
    // unreachable until the tentative-point port lands.
    // TODO: TentativePoint.isActive/getPoint branch — Task 7+.

    // TS L732-733: if (vp.viewCmdTargetCenter && this.isPointVisible(vp.viewCmdTargetCenter))
    //                 return this.setTargetCenterWorld(vp.viewCmdTargetCenter, true, true);
    // viewCmdTargetCenter is not yet ported; this branch is unreachable.
    // TODO: viewCmdTargetCenter branch — port with the Viewport target-center API.

    // TS L735: return this.setTargetCenterWorld(ViewManip.getDefaultTargetPointWorld(vp), false, false);
    setTargetCenterWorld(getDefaultTargetPointWorld(*viewport), /*lockTarget=*/false, /*saveTarget=*/false);
}

// Ported from: itwinjs-core ViewManip.processFirstPoint (ViewTool.ts:738-751).
bool ViewManip::processFirstPoint(BeButtonEvent const& ev)
{
    // TS L739-740: capture and clear any forced handle set by startHandleDrag.
    ViewHandleType const forced = forcedHandle;
    forcedHandle = ViewHandleType::None;

    // TS L742-748: hit-test the handles; if hit, mark begin of handle modify
    // and let the matched handle consume the first point.
    if (viewHandles.testHit(ev.viewPoint, forced)) {
        inHandleModify = true;
        viewHandles.focusHitHandle();
        ViewingToolHandle* handle = viewHandles.hitHandle();
        if (handle && !handle->firstPoint(ev))
            return false;
    }
    // TS L749: this._startPose = this.viewport ? this.viewport.view.savePose() : undefined;
    // TODO: ViewState::savePose — port with the ViewPose API.
    return true;
}

// Ported from: itwinjs-core ViewManip.processPoint (ViewTool.ts:753-760).
bool ViewManip::processPoint(BeButtonEvent const& ev, bool inDynamics)
{
    // TS L754-755: no active handle → nothing to do (success).
    ViewingToolHandle* hitHandle = viewHandles.hitHandle();
    if (hitHandle == nullptr)
        return true;

    // TS L758: const doUpdate = hitHandle.doManipulation(ev, inDynamics);
    bool const doUpdate = hitHandle->doManipulation(ev, inDynamics);
    // TS L759: return inDynamics || (doUpdate && hitHandle.checkOneShot());
    return inDynamics || (doUpdate && hitHandle->checkOneShot());
}

// Ported from: itwinjs-core ViewManip.decorate (ViewTool.ts:334-337).
void ViewManip::decorate(DecorateContext& context)
{
    // TS L335: this.viewHandles.drawHandles(context);
    viewHandles.drawHandles(context);
    // TS L336: this.previewDepthPoint(context);
    previewDepthPoint(context);
    // drawHandles + previewDepthPoint are both Step 4 / Task 15 stubs in the
    // current Task 9 build (handle drawHandle is a no-op; previewDepthPoint is
    // a no-op). The wiring is faithful so enabling decoration rendering later
    // is a body-only change.
}

// Ported from: itwinjs-core ViewManip.getDepthPointGeometryId (ViewTool.ts:377-381)。
std::optional<std::string> ViewManip::getDepthPointGeometryId() const
{
    // TS L379-380: DepthPointSource.Geometry === source ? sourceId : undefined
    if (m_depthPreview && m_depthPreview->source == DepthPointSource::Geometry
        && m_depthPreview->sourceId.has_value())
        return std::to_string(*m_depthPreview->sourceId);
    return std::nullopt;
}

// Ported from: itwinjs-core ViewManip.clearDepthPoint (ViewTool.ts:384-389)。
bool ViewManip::clearDepthPoint() noexcept
{
    // TS L386-388: undefined === _depthPreview ? false : (clear + true)
    if (!m_depthPreview)
        return false;
    m_depthPreview.reset();
    return true;
}

// Ported from: itwinjs-core ViewManip.previewDepthPoint (ViewTool.ts:341-376) —
// 深度预览圆（WorldOverlay 椭圆填充+描边）+ 中心十字（canvas decoration）。
void ViewManip::previewDepthPoint(DecorateContext& context)
{
    if (getenv("DANQING_DP_TRACE")) {
        fprintf(stderr, "[DP] decorate preview=%d inDyn=%d nPts=%d\n",
                m_depthPreview ? 1 : 0, inDynamicUpdate ? 1 : 0, nPts);
    }
    // TS L343: if (undefined === this._depthPreview) return;
    if (!m_depthPreview)
        return;

    // TS L345-347: cursorVp 守卫——只画在光标所在视口
    Viewport* cursorVp = Application::Get().GetToolAdmin().cursorView();
    if (!cursorVp || cursorVp != &context.GetViewport())
        return;

    dqGeom::Point3d origin = m_depthPreview->origin;
    dqGeom::Vector3d normal = m_depthPreview->normal;

    // TS L352-356: 默认深度点——投到视图平面再投回（避免 z 裁剪），圆面朝向视图
    if (m_depthPreview->isDefaultDepth) {
        dqGeom::Point3d v = cursorVp->WorldToView(origin);
        v.z = 0.0;
        origin = cursorVp->ViewToWorld(v);
        auto* view3d = cursorVp->GetView() ? cursorVp->GetView()->AsViewState3d() : nullptr;
        if (view3d)
            normal = view3d->GetZVec();
    }

    // TS L361-362: pixelSize / skew / radius（pickRadius 像素 → 世界半径）
    double const pixelSize = cursorVp->GetViewingSpace().getPixelSizeAtPoint(&origin);
    double const skew = cursorVp->GetView() ? cursorVp->GetView()->getAspectRatioSkew() : 1.0;
    double const radius = m_depthPreview->pickRadius * pixelSize;
    if (radius <= 0.0)
        return;

    // TS L363-364: 椭圆的轴框 + Arc3d（createScaledXYColumns 的 FromVectors 等价）
    dqGeom::Matrix3d const rMatrix = dqGeom::Matrix3d::CreateRigidHeadsUp(normal);
    dqGeom::Vector3d const colX = rMatrix.ColumnX();
    dqGeom::Vector3d const colY = rMatrix.ColumnY();
    auto ellipse = dqGeom::Arc3d::FromVectors(
        origin,
        dqGeom::Vector3d::From(colX.x * radius, colX.y * radius, colX.z * radius),
        dqGeom::Vector3d::From(colY.x * radius / skew, colY.y * radius / skew, colY.z * radius / skew),
        dqGeom::AngleSweep::FullCircle());
    if (ellipse.IsNull())
        return;

    // TS L365-366: 颜色——默认深度红 / 几何绿 / 其他源 = vp.hilite.color
    dqCommon::ColorDef const colorBase =
        m_depthPreview->isDefaultDepth ? dqCommon::ColorDef::red
        : (m_depthPreview->source == DepthPointSource::Geometry ? dqCommon::ColorDef::green
           : cursorVp->GetHiliteColorDef());
    // EditManipulator.HandleUtils.adjustForBackgroundColor（EditManipulator.ts:285-290）：
    // displaySky 开 → 原色；否则按背景对比度调整。
    dqCommon::ColorDef colorLine = colorBase;
    {
        auto* view3d = cursorVp->GetView() ? cursorVp->GetView()->AsViewState3d() : nullptr;
        bool const skyOn = view3d
            && cursorVp->GetView()->GetDisplayStyle().getEnvironment().displaySky;
        if (!skyOn) {
            dqCommon::ColorDef const bg = dqCommon::ColorDef::fromTbgr(
                cursorVp->GetView()->GetDisplayStyle().getBackgroundColor());
            colorLine = colorBase.adjustedForContrast(bg);
        }
    }
    // TS L366-367: colorLine 透明 50 / fill 透明 200；线型：默认深度 Code2 虚线
    colorLine = colorLine.withTransparency(50);
    dqCommon::ColorDef const colorFill = colorLine.withTransparency(200);
    dqCommon::LinePixels const pattern =
        m_depthPreview->isDefaultDepth ? dqCommon::LinePixels::Code2 : dqCommon::LinePixels::Solid;

    // TS L368-372: createGraphicBuilder(WorldOverlay) + setSymbology + addArc ×2
    // （填充圆面 + 描边圆线）。DanQing 通道适配：addArc → Arc3d EmitStrokes 折线化
    // 后 addShape/addLineString（参考 GraphicAssembler.addArc 内部同为折线化）；
    // EQUIVALENCE: 参考源=ViewTool.ts:368-372；发散=addArc 以弦切折线逼近
    // （ComputeStrokeCount 公差控制）；验证法=Rotate 悬停时圆+十字跟手可见。
    dqRender::GraphicBuilderOptions opts;
    opts.type = dqRender::GraphicType::WorldOverlay;
    opts.computeChordTolerance = [pixelSize]() { return pixelSize * 0.25; };
    auto builder = cursorVp->createGraphicBuilder(opts);
    if (builder) {
        dqGeom::LineString3d stroke;
        ellipse->EmitStrokes(stroke, dqGeom::StrokeOptions::CreateForCurves());
        auto const& pts = stroke.Points();
        if (!pts.empty()) {
            builder->setSymbology(colorLine, colorFill, 1, pattern);
            builder->addShape(pts.data(), pts.size());
            builder->addLineString(pts.data(), pts.size());
            if (auto* g = builder->finish())
                context.AddDecoration(dqRender::GraphicType::WorldOverlay, g);
        }
    }

    // TS L375: ViewTargetCenter.drawCross(context, origin, pickRadius*0.5, false)
    ViewTargetCenter::drawCross(context, origin, m_depthPreview->pickRadius * 0.5, /*hasFocus=*/false);
}

// Ported from: itwinjs-core ViewManip.pickDepthPoint (ViewTool.ts:392-431).
std::optional<dqGeom::Point3d> ViewManip::pickDepthPoint(BeButtonEvent const& ev, bool isPreview)
{
    // TS L394-396: 非预览且已有几何命中 → 清 flash（换目标预览）
    if (!isPreview && ev.viewport && getDepthPointGeometryId().has_value())
        ev.viewport->SetFlashedId(0);

    // TS L397: this.clearDepthPoint();
    clearDepthPoint();
    // TS L398-399: if (isPreview && this.inDynamicUpdate) return undefined;
    if (isPreview && inDynamicUpdate)
        return std::nullopt;

    // TS L401-403: vp / hitHandle / needDepthPoint 门
    Viewport* vp = ev.viewport;
    ViewingToolHandle* hitHandle = viewHandles.hitHandle();
    if (!vp || !hitHandle || !hitHandle->needDepthPoint(ev, isPreview))
        return std::nullopt;

    // TS L405-406: pickRadiusPixels = vp.pixelsFromInches(ToolSettings.viewToolPickRadiusInches=0.20,
    // ToolSettings.ts:38)；vp.pickDepthPoint(ev.rawPoint, pickRadiusPixels)
    double const pickRadiusPixels = vp->PixelsFromInches(0.20);
    Viewport::DepthPointResult const result = vp->pickDepthPoint(ev.rawPoint, pickRadiusPixels);

    // TS L407-419: isValidDepth —— 几何/模型/地图直接有效；平面来源看 npc z∈[0,1]
    bool isValidDepth = false;
    switch (result.source) {
        case DepthPointSource::Geometry:
        case DepthPointSource::Model:
        case DepthPointSource::Map:
            isValidDepth = true;
            break;
        default: {
            dqGeom::Point3d const npc = vp->GetViewingSpace().WorldToNpc(result.origin);
            isValidDepth = !(npc.z < 0.0 || npc.z > 1.0);
            break;
        }
    }

    // TS L422: 手柄可否决/改写（adjustDepthPoint 默认 isValid 直通——ViewTool.ts:159-171）
    {
        dqGeom::Plane3dByOriginAndUnitNormal const plane(result.origin, result.normal);
        isValidDepth = hitHandle->adjustDepthPoint(isValidDepth, vp, plane, result.source);
    }

    // TS L424-425: 预览态记录（previewDepthPoint 据此画圆+十字）
    if (isPreview) {
        DepthPreview preview;
        preview.testPoint = ev.rawPoint;
        preview.pickRadius = pickRadiusPixels;
        preview.origin = result.origin;
        preview.normal = result.normal;
        preview.source = result.source;
        preview.isDefaultDepth = !isValidDepth;
        if (result.source == DepthPointSource::Geometry && result.sourceId != 0)
            preview.sourceId = result.sourceId;
        m_depthPreview = preview;
    }

    // TS L427-428: return (isValidDepth || isPreview) ? plane.origin : undefined
    if (isValidDepth || isPreview)
        return result.origin;
    return std::nullopt;
}

// Ported from: itwinjs-core ViewManip.lensAngleMatches (ViewTool.ts:762-767).
bool ViewManip::lensAngleMatches(dqGeom::Angle const& angle, double tolerance) const
{
    // TS L763-765: const cameraView = this.viewport?.view; if (undefined === cameraView) return false;
    if (!viewport)
        return false;
    ViewState* viewBase = viewport->GetView();
    ViewState3d* view = viewBase ? viewBase->AsViewState3d() : nullptr;
    if (!view)
        return false;
    // TS L766: return !cameraView.is3d() ? false
    //          : Math.abs(cameraView.calcLensAngle().radians - angle.radians) < tolerance;
    if (!view->is3d())
        return false;
    // calcLensAngle is not ported; GetLensAngle reads the stored camera lens
    // radians directly. For a non-camera view the lens angle defaults to 0;
    // callers (ViewNavigate.onReinitialize) gate on isCameraOn first.
    double const diff = std::fabs(view->GetLensAngle() - angle.Radians());
    return diff < tolerance;
}

// Ported from: itwinjs-core ViewManip.isZUp (ViewTool.ts:769-778).
bool ViewManip::isZUp() const
{
    // TS L770-771: const view = this.viewport?.view; if (undefined === view) return true;
    if (!viewport)
        return true;
    ViewState* viewBase = viewport->GetView();
    ViewState3d* view = viewBase ? viewBase->AsViewState3d() : nullptr;
    if (!view)
        return true;

    // TS L774-775: NOTE: the reference has a copy-paste bug (viewX is assigned
    // from getXVector twice; line 775 should read viewY = view.getYVector()).
    // DanQing faithfully ports the *intended* behavior (compare viewY to Z) — the
    // bug-for-bug port would make isZUp always return false for non-Z-up views.
    // Filed as a deviation in the report.
    dqGeom::Vector3d const viewX = view->GetXVec();
    dqGeom::Vector3d const viewY = view->GetYVec();
    dqGeom::Vector3d const zVec = dqGeom::Vector3d::UnitZ();
    // TS L777: return (Math.abs(zVec.dotProduct(viewY)) > 0.99 && Math.abs(zVec.dotProduct(viewX)) < 0.01);
    return std::fabs(zVec.DotProduct(viewY)) > 0.99 && std::fabs(zVec.DotProduct(viewX)) < 0.01;
}

// Ported from: itwinjs-core ViewManip.getFocusPlaneNpc (ViewTool.ts:780-783).
double ViewManip::getFocusPlaneNpc(Viewport const& vp)
{
    // TS L781: const pt = vp.worldToNpc(vp.view.getTargetPoint());
    ViewState* viewBase = vp.GetView();
    ViewState3d* view = viewBase ? viewBase->AsViewState3d() : nullptr;
    if (!view)
        return 0.5;
    dqGeom::Point3d const pt = vp.WorldToNpc(view->GetTargetPoint());
    // TS L782: return (pt.z < 0.0 || pt.z > 1.0) ? 0.5 : pt.z;
    return (pt.z < 0.0 || pt.z > 1.0) ? 0.5 : pt.z;
}

// Ported from: itwinjs-core ViewManip.getDefaultTargetPointWorld (ViewTool.ts:785-798).
dqGeom::Point3d ViewManip::getDefaultTargetPointWorld(Viewport const& vp)
{
    // TS L786-787: if (!vp.view.allow3dManipulations()) return vp.npcToWorld(NpcCenter);
    ViewState* viewBase = vp.GetView();
    ViewState3d* view = viewBase ? viewBase->AsViewState3d() : nullptr;
    // NpcCenter = (0.5, 0.5, 0.5) — Ported from @itwin/core-common NpcCenter.
    dqGeom::Point3d const kNpcCenter = dqGeom::Point3d::From(0.5, 0.5, 0.5);
    if (!view || !view->Allow3dManipulations())
        return vp.NpcToWorld(kNpcCenter);

    // TS L789-790: const targetPoint = vp.view.getTargetPoint();
    //               const targetPointNpc = vp.worldToNpc(targetPoint);
    dqGeom::Point3d targetPoint = view->GetTargetPoint();
    dqGeom::Point3d targetPointNpc = vp.WorldToNpc(targetPoint);

    // TS L792-795: if (targetPointNpc.z < 0.0 || targetPointNpc.z > 1.0) {
    //                 targetPointNpc.z = 0.5; vp.npcToWorld(targetPointNpc, targetPoint); }
    if (targetPointNpc.z < 0.0 || targetPointNpc.z > 1.0) {
        targetPointNpc.z = 0.5;
        targetPoint = vp.NpcToWorld(targetPointNpc);
    }
    return targetPoint;
}

// Ported from: itwinjs-core ViewManip.isPointVisible (ViewTool.ts:801-807).
bool ViewManip::isPointVisible(dqGeom::Point3d const& testPt) const
{
    // TS L802-803: const vp = this.viewport; if (!vp) return false;
    if (!viewport)
        return false;
    // TS L806: return vp.isPointVisibleXY(testPt);
    // TODO: Viewport::isPointVisibleXY — port with the Viewport pick/visibility API.
    // For Task 9 there is no caller that depends on the result (used by
    // updateTargetCenter's viewCmdTargetCenter branch, which is unreachable
    // since viewCmdTargetCenter is not yet ported). Returning true is the
    // conservative choice (matches the reference behavior for an in-viewport
    // point) and unblocks future wiring.
    (void)testPt;
    return true;
}

// ---------------------------------------------------------------------------
// HandleWithInertia
// Ported from: itwinjs-core HandleWithInertia (ViewTool.ts:1042-1104).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core inertialDampen (ViewTool.ts:84-86).
// Scales the inertia vector by the clamped damping factor.
static void inertialDampen(dqGeom::Vector3d& v) noexcept
{
    v.Scale(clampDouble(ToolSettings::viewingInertia.damping, 0.75, 0.999));
}

// Ported from: itwinjs-core HandleWithInertia.doManipulation (ViewTool.ts:1047-1064).
bool HandleWithInertia::doManipulation(BeButtonEvent const& ev, bool inDynamics)
{
    // TS L1048-1049: if inertia is enabled and the gesture has ended and we
    // have an inertia vector, kick off the inertia animation.
    if (ToolSettings::viewingInertia.enabled && !inDynamics && m_inertiaVec.has_value())
        return beginAnimation();

    // TS L1051-1053: bail without a viewport (no worldToNpc available).
    Viewport* vp = ev.viewport;
    if (!vp)
        return false;

    // TS L1055-1056: thisPtNpc = worldToNpc(ev.point); preserve the previous z.
    dqGeom::Point3d thisPtNpc = vp->WorldToNpc(ev.point);
    thisPtNpc.z = m_lastPtNpc.z;

    // TS L1058: reset inertia vector before deciding whether to re-establish it.
    m_inertiaVec.reset();
    // TS L1059-1060: no-op if the cursor hasn't moved (almost equal at 1e-10).
    if (m_lastPtNpc.AlmostEqual(thisPtNpc, 1.0e-10))
        return true;

    // TS L1062-1063: inertia vector = lastPtNpc → thisPtNpc; perform the op.
    m_inertiaVec = vectorTo(m_lastPtNpc, thisPtNpc);
    return perform(thisPtNpc);
}

// Ported from: itwinjs-core HandleWithInertia.beginAnimation (ViewTool.ts:1067-1076).
//
// ← 参考 L1072-1073 `vp.setAnimator(this)`：把本句柄装为视口动画器驱动惯性
// 尾巴。参考依赖 TS GC 保证句柄存活；C++ 中句柄由 ViewHandleArray 持有，
// 用 Viewport::setAnimatorRef（非拥有引用）等价实现——RenderFrame Step2
// 每帧调 animate()（Viewport.ts:2573-2575 契约），完成即自清除。
bool HandleWithInertia::beginAnimation()
{
    // TS L1068: this._duration = ToolSettings.viewingInertia.duration;
    m_duration = dqBase::DqDuration::FromMilliseconds(ToolSettings::viewingInertia.duration.ToMilliseconds());
    // TS L1069-1074: install this handle as the viewport animator if the
    // duration is towards the future.
    if (m_duration.IsTowardsFuture()) {
        m_end = dqBase::DqTimePoint::FromNow(m_duration);
        // TS L1072-1073: if (vp) vp.setAnimator(this).
        if (viewTool && viewTool->viewport)
            viewTool->viewport->setAnimatorRef(this);
    }
    return true;
}

// Ported from: itwinjs-core HandleWithInertia.animate (ViewTool.ts:1079-1100).
bool HandleWithInertia::animate()
{
    // TS L1080-1081: bail if the inertia vector has been cleared.
    if (!m_inertiaVec.has_value())
        return true;  // remove this as the animator

    // TS L1085: remaining fraction of the inertia duration.
    double const remaining = static_cast<double>(
        (m_end - dqBase::DqTimePoint::Now()).ToMilliseconds()) /
        static_cast<double>(m_duration.ToMilliseconds());
    // TS L1086: pt = lastPtNpc + remaining * inertiaVec.
    dqGeom::Point3d const pt = plusScaled(m_lastPtNpc, *m_inertiaVec, remaining);

    // TS L1089-1096: end conditions — duration elapsed or movement stalled.
    dqGeom::Vector3d const delta = minus(m_lastPtNpc, pt);
    if (remaining <= 0.0 || delta.MagnitudeSquared() < 1.0e-6) {
        if (!viewTool || !viewTool->viewport)
            return false;
        // TS L1094: vp.saveViewUndo()——undo 栈已移植（Viewport.cpp saveViewUndo）。
        viewTool->viewport->saveViewUndo();
        return true;  // remove this as the animator
    }
    // TS L1097: perform the viewing operation at the interpolated point.
    perform(pt);
    // TS L1098: dampen the inertia vector for the next frame.
    inertialDampen(*m_inertiaVec);
    return false;  // keep animating
}

// ---------------------------------------------------------------------------
// AnimatedHandle
// Ported from: itwinjs-core AnimatedHandle (ViewTool.ts:1409-1497).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core AnimatedHandle.testHandleForHit (ViewTool.ts:1415-1419).
bool AnimatedHandle::testHandleForHit(dqGeom::Point3d /*ptScreen*/, HitOut& out)
{
    out.distance = 0.0;
    out.priority = ViewManipPriority::Medium;
    return true;
}

// Ported from: itwinjs-core AnimatedHandle.getElapsedTime (ViewTool.ts:1421-1425).
double AnimatedHandle::getElapsedTime()
{
    double const prev = m_lastMotionTime;
    // TS L1423: this._lastMotionTime = Date.now();
    m_lastMotionTime = static_cast<double>(dqBase::DqTimePoint::Now().GetTicks()) / 1.0e6;  // ms
    // TS L1424: clamp delta to [0,1000]ms and normalize to seconds.
    return clampDouble(m_lastMotionTime - prev, 0.0, 1000.0) / 1000.0;
}

// Ported from: itwinjs-core AnimatedHandle.doManipulation (ViewTool.ts:1427-1430).
bool AnimatedHandle::doManipulation(BeButtonEvent const& ev, bool /*inDynamics*/)
{
    // TS L1428-1429: track last view-coordinate cursor position; animate()
    // uses this to compute the direction.
    m_lastPtView = ev.viewPoint;
    return true;
}

// Ported from: itwinjs-core AnimatedHandle.animate (ViewTool.ts:1434-1440).
//
// Reference semantics (subtle): the return value is consumed by subclasses
// (ViewScroll.animate) as an INTERNAL "should I proceed with per-frame work"
// flag, NOT as the documented Animator "am I done" flag. The reference:
//   if (undefined !== cursorView) return true;   // cursor in view → proceed
//   this.getElapsedTime();
//   return false;                                // cursor outside → skip
//
// Note this inverts the documented Animator interface (TRUE = "done"); the
// inversion is harmless because AnimatedHandle subclasses (ViewScroll) always
// return false to Viewport at the outer level, honoring the interface there.
//
// cursorView (ToolAdmin.cursorView) is not yet ported. Until it is, the
// "always-in-view" default (returning true) keeps per-frame work enabled,
// matching the normal-use reference behavior. When cursorView lands, replace
// the unconditional `return true` with the cursorView gate.
bool AnimatedHandle::animate()
{
    // TS L1438: refresh elapsed-time cache for this frame.
    (void)getElapsedTime();
    // TODO: cursorView gate — `if (cursorView) return true; else return false;`.
    return true;
}

// Ported from: itwinjs-core AnimatedHandle.firstPoint (ViewTool.ts:1442-1463).
bool AnimatedHandle::firstPoint(BeButtonEvent const& ev)
{
    Viewport* vp = ev.viewport;
    // TS L1444: const tool = this.viewTool;
    // TS L1445: tool.inDynamicUpdate = true;
    if (viewTool)
        viewTool->inDynamicUpdate = true;

    // TS L1446-1457: pick depth point if needed; otherwise anchor at view point.
    if (vp && needDepthPoint(ev, false)) {
        pickDepthPoint(ev);
        if (m_depthPoint.has_value()) {
            m_anchorPtView = vp->WorldToView(*m_depthPoint);
        } else {
            // TS L1451-1453: fall back to focus-plane NPC then convert to view.
            dqGeom::Point3d npc = vp->WorldToNpc(ev.point);
            npc.z = ViewManip::getFocusPlaneNpc(*vp);
            m_anchorPtView = vp->WorldToView(vp->NpcToWorld(npc));
        }
    } else {
        // TS L1456: this._anchorPtView.setFrom(ev.viewPoint);
        m_anchorPtView = ev.viewPoint;
    }
    // TS L1458: this._lastPtView.setFrom(this._anchorPtView);
    m_lastPtView = m_anchorPtView;
    // TS L1459: this._lastMotionTime = Date.now();
    m_lastMotionTime = static_cast<double>(dqBase::DqTimePoint::Now().GetTicks()) / 1.0e6;

    // TS L1460-1461: install this handle as the viewport animator.
    // Same Animator-ownership deviation as HandleWithInertia::beginAnimation —
    // the DanQing Viewport::setAnimator takes unique_ptr, which is incompatible
    // with a handle owned by ViewHandleArray. The continuous-animation path
    // therefore requires the future setAnimator(raw*) overload to drive; the
    // animate() math itself is faithful and unit-testable in isolation.
    // TODO: drive animate() from render loop once setAnimator takes a raw ptr.
    return true;
}

// Ported from: itwinjs-core AnimatedHandle.getDirection (ViewTool.ts:1465-1469).
std::optional<dqGeom::Vector3d> AnimatedHandle::getDirection()
{
    // TS L1466-1468: dir = anchor → lastPt; zero z; dead-zone gate.
    dqGeom::Vector3d dir = vectorTo(m_anchorPtView, m_lastPtView);
    dir.z = 0.0;
    if (dir.MagnitudeSquared() < m_deadZone)
        return std::nullopt;
    return dir;
}

// Ported from: itwinjs-core AnimatedHandle.getInputVector (ViewTool.ts:1471-1480).
std::optional<dqGeom::Vector3d> AnimatedHandle::getInputVector()
{
    if (!viewTool || !viewTool->viewport)
        return std::nullopt;
    auto dir = getDirection();
    if (!dir.has_value())
        return std::nullopt;
    ViewRect const rect = viewTool->viewport->viewRect();
    return dqGeom::Vector3d::From(
        dir->x * (2.0 / static_cast<double>(rect.width())),
        dir->y * (2.0 / static_cast<double>(rect.height())),
        0.0);
}

// Ported from: itwinjs-core AnimatedHandle.onReinitialize (ViewTool.ts:1482-1488).
void AnimatedHandle::onReinitialize()
{
    // TS L1483-1484: inDynamicUpdate = false; clear the animator.
    if (viewTool)
        viewTool->inDynamicUpdate = false;
    if (viewTool && viewTool->viewport)
        viewTool->viewport->setAnimator(nullptr);
}

// Ported from: itwinjs-core AnimatedHandle.onWheel (ViewTool.ts:1491-1496).
bool AnimatedHandle::onWheel(BeWheelEvent const& /*ev*/)
{
    // TS L1492-1494: reset tool state on wheel — start over.
    if (viewTool) {
        viewTool->nPts = 0;
        viewTool->inDynamicUpdate = false;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ViewPan
// Ported from: itwinjs-core ViewPan (ViewTool.ts:1107-1175).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ViewingToolHandle.focusIn (ViewTool.ts:146).
// IModelApp.toolAdmin.setCursor(this.getHandleCursor()) — the handle-focus
// cursor switch (rotate handle → rotate bitmap cursor, pan handle → grab, ...).
void ViewingToolHandle::focusIn()
{
    Application::Get().GetToolAdmin().setCursor(getHandleCursor());
}

// Ported from: itwinjs-core ViewPan.getHandleCursor (ViewTool.ts:1109).
std::string ViewPan::getHandleCursor() const
{
    // TS L1109: this.viewTool.inHandleModify ? grabbingCursor : grabCursor.
    // Cursor names resolve to the bitmap cursor family in Viewport::setCursor
    // (openHand/closedHand converted from the reference's .cur assets).
    return (viewTool && viewTool->inHandleModify) ? std::string("grabbing") : std::string("grab");
}

// Ported from: itwinjs-core ViewPan.firstPoint (ViewTool.ts:1111-1134).
bool ViewPan::firstPoint(BeButtonEvent const& ev)
{
    // TS L1112: this._inertiaVec = undefined;
    m_inertiaVec.reset();

    ViewManip* tool = viewTool;
    Viewport* vp = tool ? tool->viewport : nullptr;
    if (!vp)
        return false;

    // TS L1120: vp.worldToNpc(ev.point, this._lastPtNpc);
    m_lastPtNpc = vp->WorldToNpc(ev.point);

    // TS L1123-1129: if camera on, pick the depth point so we know the z to pan at.
    if (needDepthPoint(ev, false)) {
        pickDepthPoint(ev);
        if (m_depthPoint.has_value())
            m_lastPtNpc = vp->WorldToNpc(*m_depthPoint);
        else
            m_lastPtNpc.z = ViewManip::getFocusPlaneNpc(*vp);
    }

    // TS L1131: tool.beginDynamicUpdate();
    tool->beginDynamicUpdate();
    // TS L1132: tool.provideToolAssistance("Pan.Prompts.NextPoint");
    tool->provideToolAssistance("Pan.Prompts.NextPoint");
    return true;
}

// Ported from: itwinjs-core ViewPan.testHandleForHit (ViewTool.ts:1136-1140).
bool ViewPan::testHandleForHit(dqGeom::Point3d /*ptScreen*/, HitOut& out)
{
    out.distance = 0.0;
    out.priority = ViewManipPriority::Low;
    return true;
}

// Ported from: itwinjs-core ViewPan.perform (ViewTool.ts:1143-1166).
bool ViewPan::perform(dqGeom::Point3d thisPtNpc)
{
    ViewManip* tool = viewTool;
    Viewport* vp = tool ? tool->viewport : nullptr;
    if (!vp)
        return false;

    // TS L1150-1153: world-space pan delta = npcToWorld(lastPt) → npcToWorld(thisPt).
    dqGeom::Point3d const lastWorld = vp->NpcToWorld(m_lastPtNpc);
    dqGeom::Point3d const thisWorld = vp->NpcToWorld(thisPtNpc);
    dqGeom::Vector3d const dist = vectorTo(thisWorld, lastWorld);

    // TS L1154-1161: 3d → moveCameraWorld(dist); 2d → setOrigin(origin + dist).
    ViewState* viewBase = vp->GetView();
    ViewState3d* view = viewBase ? viewBase->AsViewState3d() : nullptr;
    if (view && view->is3d()) {
        // TS L1155: viewingGlobe ? moveCameraGlobal : moveCameraWorld.
        // viewingGlobe is not ported; faithful Step 3 behavior is the
        // moveCameraWorld branch (ViewStatus check folded into the void
        // return of the DanQing wrapper).
        view->MoveCameraWorld(dist);
        // TS L1158: this.changeFocusFromDepthPoint();
        changeFocusFromDepthPoint();
    } else if (view) {
        // TS L1160: view.setOrigin(view.getOrigin().plus(dist));
        dqGeom::Point3d const newOrigin = plusScaled(view->GetOrigin(), dist, 1.0);
        view->SetOrigin(newOrigin);
    } else {
        return false;
    }

    // TS L1163: vp.setupFromView() — sync m_viewingSpace with the just-mutated view.
    // (Direct SetupFromView, not setupViewFromFrustum(getFrustum()): the latter would
    // re-read m_viewingSpace, which is stale until this sync, and revert the pan.)
    vp->SetupFromView();

    // TS L1164: this._lastPtNpc.setFrom(thisPtNpc);
    m_lastPtNpc = thisPtNpc;
    return true;
}

// Ported from: itwinjs-core ViewPan.needDepthPoint (ViewTool.ts:1169-1174).
bool ViewPan::needDepthPoint(BeButtonEvent const& ev, bool /*isPreview*/)
{
    Viewport* vp = ev.viewport;
    if (!vp)
        return false;
    // TS L1173: return vp.isCameraOn && CoordSource.User === ev.coordsFrom;
    return vp->isCameraOn() && CoordSource::User == ev.coordsFrom;
}

// ---------------------------------------------------------------------------
// ViewRotate
// Ported from: itwinjs-core ViewRotate (ViewTool.ts:1178-1319).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ViewRotate.firstPoint (ViewTool.ts:1191-1212).
bool ViewRotate::firstPoint(BeButtonEvent const& ev)
{
    // TS L1192: this._inertiaVec = undefined;
    m_inertiaVec.reset();

    ViewManip* tool = viewTool;
    Viewport* vp = ev.viewport;
    if (!vp)
        return false;

    // TS L1199-1201: pick depth point; if found, set the rotate target center.
    pickDepthPoint(ev);
    if (m_depthPoint.has_value())
        tool->setTargetCenterWorld(*m_depthPoint, /*lockTarget=*/false, /*saveTarget=*/false);

    // TS L1203-1204: anchorPtNpc = worldToNpc(rawPoint); lastPtNpc = anchorPtNpc.
    m_anchorPtNpc = vp->WorldToNpc(ev.rawPoint);
    m_lastPtNpc = m_anchorPtNpc;

    // TS L1206-1207: snapshot the active frustum; _frustum is the working copy.
    m_activeFrustum = vp->getWorldFrustum();
    m_frustum.setFrom(m_activeFrustum);  // (Frustum::setFrom is the ref setFrom)

    // TS L1209-1210: begin dynamic update + prompt.
    tool->beginDynamicUpdate();
    tool->provideToolAssistance("Rotate.Prompts.NextPoint");
    return true;
}

// Ported from: itwinjs-core ViewRotate.testHandleForHit (ViewTool.ts:1185-1189).
bool ViewRotate::testHandleForHit(dqGeom::Point3d /*ptScreen*/, HitOut& out)
{
    out.distance = 0.0;
    // TS L1187: Medium — always prefer over pan handle (which IdleTool force-
    // enables on middle-button).
    out.priority = ViewManipPriority::Medium;
    return true;
}

// Ported from: itwinjs-core ViewRotate.perform (ViewTool.ts:1214-1286).
bool ViewRotate::perform(dqGeom::Point3d ptNpc)
{
    ViewManip* tool = viewTool;
    Viewport* vp = tool ? tool->viewport : nullptr;
    if (!vp)
        return false;

    // TS L1221-1222: snap to anchor if too close to it.
    if (m_anchorPtNpc.AlmostEqual(ptNpc, 1.0e-2))
        ptNpc = m_anchorPtNpc;

    // TS L1224-1231: detect external frustum changes (e.g. another handle);
    // if none, restore our working frustum via setupViewFromFrustum.
    dqCommon::Frustum const currentFrustum = vp->getWorldFrustum();
    bool const frustumChange = !currentFrustum.equals(m_activeFrustum);
    if (frustumChange) {
        m_frustum.setFrom(currentFrustum);
    } else {
        if (!vp->setupViewFromFrustum(m_frustum))
            return false;
    }

    // TS L1233: currPt = npcToView(ptNpc).
    ViewRect const viewRect = vp->viewRect();
    dqGeom::Point3d const currPt = vp->NpcToView(ptNpc);
    // TS L1234-1235: if frustum changed, anchor moves with the cursor.
    if (frustumChange)
        m_anchorPtNpc = ptNpc;

    ViewState* viewBase = vp->GetView();
    ViewState3d* view = viewBase ? viewBase->AsViewState3d() : nullptr;
    if (!view)
        return false;

    // TS L1238-1269: compute (angle, worldAxis) from screen deltas.
    double angle = 0.0;
    dqGeom::Vector3d worldAxis = dqGeom::Vector3d::UnitZ();
    dqGeom::Point3d const worldPt = tool->targetCenterWorld;

    if (!view->Allow3dManipulations()) {
        // TS L1242-1247: 2d branch — rotate about Z by the planar angle from
        // center→firstPt vs center→currPt.
        dqGeom::Point3d const centerPt = vp->WorldToView(worldPt);
        dqGeom::Point3d const firstPt = vp->NpcToView(m_anchorPtNpc);
        // Vector2d.createStartEnd + angleTo in the XY plane. The 2D angle
        // reduces to atan2(cross, dot) on (x,y) — available as AngleToXY on
        // dqGeom::Vector3d.
        dqGeom::Vector3d const vector0 = vectorTo(centerPt, firstPt);
        dqGeom::Vector3d const vector1 = vectorTo(centerPt, currPt);
        angle = vector0.AngleToXY(vector1);
        worldAxis = dqGeom::Vector3d::UnitZ();
    } else {
        // TS L1248-1268: 3d branch — compute xDelta/yDelta in pixels and
        // derive rotation axes (preserveWorldUp path: x-axis = up vector;
        // y-axis = view X). The product yRot * xRot is then converted to
        // axis+angle and inverted to produce the world rotation that maps
        // the frustum.
        dqGeom::Point3d const firstPt = vp->NpcToView(m_anchorPtNpc);
        double const xDelta = currPt.x - firstPt.x;
        double const yDelta = currPt.y - firstPt.y;

        // TS L1258: xAxis = preserveWorldUp && !viewingGlobe
        //                 ? (depthPoint ? view.getUpVector(depthPoint) : UnitZ)
        //                 : rotation.row(1).
        // viewingGlobe not ported (false); view.getUpVector not ported
        // (faithful Step 3 equivalent: UnitZ — the reference's default when
        // isGeoLocated is false / globeMode != Ellipsoid / point in extents).
        dqGeom::Vector3d xAxis;
        if (ToolSettings::preserveWorldUp) {
            xAxis = m_depthPoint.has_value() ? dqGeom::Vector3d::UnitZ()
                                             : dqGeom::Vector3d::UnitZ();
        } else {
            xAxis = vp->getRotation().RowY();
        }
        // TS L1261: yAxis = rotation.row(0).
        dqGeom::Vector3d const yAxis = vp->getRotation().RowX();

        // TS L1263-1264: per-axis rotations; null if delta is zero (→ identity).
        dqGeom::Matrix3d xRMatrix = dqGeom::Matrix3d::CreateIdentity();
        if (xDelta != 0.0) {
            double const w = static_cast<double>(viewRect.width());
            double const angleX = M_PI / (w / xDelta);
            xRMatrix = dqGeom::Matrix3d::CreateRotationAroundAxis(xAxis, angleX);
        }
        dqGeom::Matrix3d yRMatrix = dqGeom::Matrix3d::CreateIdentity();
        if (yDelta != 0.0) {
            double const h = static_cast<double>(viewRect.height());
            double const angleY = M_PI / (h / yDelta);
            yRMatrix = dqGeom::Matrix3d::CreateRotationAroundAxis(yAxis, angleY);
        }
        // TS L1265: worldRMatrix = yRMatrix * xRMatrix.
        dqGeom::Matrix3d const worldRMatrix = yRMatrix.MultiplyMatrix(xRMatrix);
        // TS L1266-1268: axis+angle from the product; invert the angle.
        AxisAndAngle const result = getAxisAndAngleOfRotation(worldRMatrix);
        angle = -result.angleRadians;
        worldAxis = result.axis;
    }

    // TS L1271-1280: apply the world rotation about the target center.
    dqGeom::Matrix3d const worldMatrix = dqGeom::Matrix3d::CreateRotationAroundAxis(worldAxis, angle);
    if (getenv("DANQING_DP_TRACE")) {
        auto fr = [](dqCommon::Frustum const& f) {
            double x0=f.points[0].x, x1=f.points[0].x, y0=f.points[0].y, y1=f.points[0].y, z0=f.points[0].z, z1=f.points[0].z;
            for (auto const& p : f.points) {
                x0=std::min(x0,p.x); x1=std::max(x1,p.x);
                y0=std::min(y0,p.y); y1=std::max(y1,p.y);
                z0=std::min(z0,p.z); z1=std::max(z1,p.z);
            }
            fprintf(stderr, "[DP] frustum span=(%.2f,%.2f,%.2f)\n", x1-x0, y1-y0, z1-z0);
        };
        dqGeom::Point3d const currPtDbg = vp->NpcToView(ptNpc);
        dqGeom::Point3d const anchorDbg = vp->NpcToView(m_anchorPtNpc);
        fprintf(stderr, "[DP] perform: angle=%.4f axis=(%.3f,%.3f,%.3f) worldPt=(%.2f,%.2f,%.2f)\n",
                angle, worldAxis.x, worldAxis.y, worldAxis.z, worldPt.x, worldPt.y, worldPt.z);
        fprintf(stderr, "[DP]   ptNpc=(%.4f,%.4f,%.4f) anchorNpc=(%.4f,%.4f,%.4f) currView=(%.1f,%.1f) anchorView=(%.1f,%.1f) viewRect=%dx%d\n",
                ptNpc.x, ptNpc.y, ptNpc.z, m_anchorPtNpc.x, m_anchorPtNpc.y, m_anchorPtNpc.z,
                currPtDbg.x, currPtDbg.y, anchorDbg.x, anchorDbg.y,
                vp->viewRect().width(), vp->viewRect().height());
        fprintf(stderr, "[DP]   pre-transform "); fr(m_frustum);
    }
    // (createRotationAroundVector always returns a Matrix3d in DanQing; the
    // reference's `undefined` guard is unreachable given a finite axis+angle.)
    dqGeom::Transform const worldTransform =
        dqGeom::Transform::CreateFixedPointAndMatrix(worldPt, worldMatrix);
    dqCommon::Frustum const newFrustum = m_frustum.transformBy(worldTransform);
    if (getenv("DANQING_DP_TRACE")) {
        auto fr = [](dqCommon::Frustum const& f) {
            double x0=f.points[0].x, x1=f.points[0].x, y0=f.points[0].y, y1=f.points[0].y, z0=f.points[0].z, z1=f.points[0].z;
            for (auto const& p : f.points) {
                x0=std::min(x0,p.x); x1=std::max(x1,p.x);
                y0=std::min(y0,p.y); y1=std::max(y1,p.y);
                z0=std::min(z0,p.z); z1=std::max(z1,p.z);
            }
            fprintf(stderr, "[DP]   post-transform span=(%.2f,%.2f,%.2f)\n", x1-x0, y1-y0, z1-z0);
        };
        fr(newFrustum);
    }
    // TS L1275: view.setupFromFrustum(frustum).
    if (!view->SetupFromFrustum(newFrustum))
        return false;
    // TS L1276-1277: if (view.is3d()) view.alignToGlobe(view.getCenter());
    // alignToGlobe not ported — no-op (faithful for non-geo-located iModels).
    // TS L1278: this.changeFocusFromDepthPoint();
    changeFocusFromDepthPoint();
    // TS L1279: vp.setupFromView();
    vp->SetupFromView();

    // TS L1282: vp.getWorldFrustum(this._activeFrustum);
    m_activeFrustum = vp->getWorldFrustum();
    // TS L1283: this._lastPtNpc.setFrom(ptNpc);
    m_lastPtNpc = ptNpc;
    return true;
}

// Ported from: itwinjs-core ViewRotate.onWheel (ViewTool.ts:1288-1296).
bool ViewRotate::onWheel(BeWheelEvent const& ev)
{
    // TS L1289-1295: when rotate is active and target is locked or in modify,
    // route the wheel zoom to targetCenterWorld.
    ViewManip* tool = viewTool;
    if (tool && (tool->targetCenterLocked || tool->inHandleModify)) {
        BeWheelEvent& mutEv = const_cast<BeWheelEvent&>(ev);
        mutEv.point = tool->targetCenterWorld;
        mutEv.coordsFrom = CoordSource::Precision;
    }
    return false;
}

// Ported from: itwinjs-core ViewRotate.needDepthPoint (ViewTool.ts:1299-1304).
bool ViewRotate::needDepthPoint(BeButtonEvent const& ev, bool /*isPreview*/)
{
    Viewport* vp = ev.viewport;
    if (!vp)
        return false;
    ViewState* viewBase = vp->GetView();
    ViewState3d* view = viewBase ? viewBase->AsViewState3d() : nullptr;
    // TS L1303: !targetCenterLocked && view.allow3dManipulations().
    return view && !viewTool->targetCenterLocked && view->Allow3dManipulations();
}

// Ported from: itwinjs-core ViewRotate.adjustDepthPoint (ViewTool.ts:1307-1318).
bool ViewRotate::adjustDepthPoint(bool isValid, Viewport* vp,
                                  Plane3dByOriginAndUnitNormal plane,
                                  DepthPointSource source)
{
    // TS L1308-1312: viewingGlobe branch — not ported; with viewingGlobe=false
    // this branch is unreachable in the reference too.
    // TS L1313-1314: super.adjustDepthPoint gate.
    if (ViewingToolHandle::adjustDepthPoint(isValid, vp, plane, source))
        return true;
    // TS L1316-1317: fall back to the target center; return false (the source
    // is rejected but the plane origin is updated as a side effect in the TS
    // reference — faithful C++ port of that mutation).
    plane.origin = viewTool ? viewTool->targetCenterWorld : plane.origin;
    return false;
}

// ---------------------------------------------------------------------------
// ViewTargetCenter
// Ported from: itwinjs-core ViewTargetCenter (ViewTool.ts:938-1038).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ViewTargetCenter.firstPoint (ViewTool.ts:943-948).
bool ViewTargetCenter::firstPoint(BeButtonEvent const& ev)
{
    if (!ev.viewport)
        return false;
    // TS L946: ev.viewport.viewCmdTargetCenter = undefined ("Clear current saved
    // target, must accept a new location with ctrl...") — viewCmdTargetCenter is
    // not yet ported (ViewTool.cpp setTargetCenterWorld :700 TODO); the field
    // starts undefined in the reference too, so clearing it is a faithful no-op.
    return true;
}

// Ported from: itwinjs-core ViewTargetCenter.testHandleForHit (ViewTool.ts:950-968).
bool ViewTargetCenter::testHandleForHit(dqGeom::Point3d ptScreen, HitOut& out)
{
    // TS L951-952: target center handle is not movable in drag-required mode.
    if (viewTool->isDraggingRequired)
        return false;

    Viewport* vp = viewTool->viewport;
    if (!vp)
        return false;

    // TS L958-963: hit window = 0.15in around the target center (view space XY).
    dqGeom::Point3d const targetPt = vp->WorldToView(viewTool->targetCenterWorld);
    double const distance = targetPt.DistanceXY(ptScreen);
    double const locateThreshold = vp->PixelsFromInches(0.15);
    if (distance > locateThreshold)
        return false;

    out.distance = distance;
    out.priority = ViewManipPriority::High;
    return true;
}

// Ported from: itwinjs-core ViewTargetCenter.drawCross (ViewTool.ts:970-999)。
// 画布十字（白线+黑描边；阴影省略——GL 栅格化后端无 shadowBlur）。
void ViewTargetCenter::drawCross(DecorateContext& context, dqGeom::Point3d const& worldPoint,
                                 double sizePixels, bool hasFocus)
{
    // TS L972-976: half-pixel alignment of both the cross arms and the position.
    double const crossSize = std::floor(sizePixels) + 0.5;
    double const outlineSize = crossSize + 1.0;
    dqGeom::Point3d const position = context.GetViewport().WorldToView(worldPoint);

    dqRender::CanvasDecoration dec;
    dec.position = dqGeom::Point2d::From(std::floor(position.x) + 0.5,
                                         std::floor(position.y) + 0.5);
    dec.drawDecoration = [crossSize, outlineSize, hasFocus](dqRender::CanvasContext& ctx) {
        // TS L977-986: black outline stroke (rgba(0,0,0,.5)).
        ctx.beginPath();
        ctx.setStrokeStyle(dqCommon::ColorDef::from(0, 0, 0, 128));
        ctx.setLineWidth(hasFocus ? 5 : 3);
        ctx.moveTo(-outlineSize, 0); ctx.lineTo(outlineSize, 0);
        ctx.moveTo(0, -outlineSize); ctx.lineTo(0, outlineSize);
        ctx.stroke();
        // TS L988-997: white cross (shadowColor/shadowBlur omitted — the GL
        // rasterizer backend has no canvas shadow support).
        ctx.beginPath();
        ctx.setStrokeStyle(dqCommon::ColorDef::from(255, 255, 255));
        ctx.setLineWidth(hasFocus ? 3 : 1);
        ctx.moveTo(-crossSize, 0); ctx.lineTo(crossSize, 0);
        ctx.moveTo(0, -crossSize); ctx.lineTo(0, crossSize);
        ctx.stroke();
    };
    context.AddCanvasDecoration(std::move(dec), /*atFront=*/true);
}

// Ported from: itwinjs-core ViewTargetCenter.drawHandle (ViewTool.ts:1001-1021)。
void ViewTargetCenter::drawHandle(DecorateContext& context, bool hasFocus)
{
    // TS L1002-1003: only draw in our tool's viewport.
    if (&context.GetViewport() != viewTool->viewport)
        return;

    // TS L1005-1006: don't display the default target center — it will be
    // updated to the pick point on element hover / set by rotate's firstPoint.
    if (!viewTool->targetCenterLocked && !viewTool->inHandleModify)
        return;

    // TS L1008-1009: with focus during modify, the cross is drawn by the
    // preview depth point instead.
    if (hasFocus && viewTool->inHandleModify)
        return;

    // TS L1011-1017: full-size cross normally; small cross while another
    // handle modifies (and only when that handle is rotate, not pan).
    double sizeInches = 0.2;
    if (!hasFocus && viewTool->inHandleModify) {
        ViewingToolHandle* hitHandle = viewTool->viewHandles.hitHandle();
        if (hitHandle && ViewHandleType::Rotate != hitHandle->handleType())
            return;
        sizeInches = 0.1;
    }

    double const crossSize = context.GetViewport().PixelsFromInches(sizeInches);
    drawCross(context, viewTool->targetCenterWorld, crossSize, hasFocus);
}

// Ported from: itwinjs-core ViewTargetCenter.doManipulation (ViewTool.ts:1023-1031)。
bool ViewTargetCenter::doManipulation(BeButtonEvent const& ev, bool inDynamics)
{
    // TS L1024-1025: no-op during dynamics or outside our viewport.
    if (inDynamics || ev.viewport != viewTool->viewport)
        return false;

    // TS L1027-1028: accept the depth point when available, else the event
    // point; lock the target for this tool instance. ev.isControlKey (the
    // saveTarget argument) is not ported on BeButtonEvent (IdleTool.cpp:78
    // 同款缺口) — setTargetCenterWorld ignores saveTarget until
    // Viewport::viewCmdTargetCenter lands (ViewTool.cpp:700 TODO).
    pickDepthPoint(ev);
    viewTool->setTargetCenterWorld(m_depthPoint.has_value() ? *m_depthPoint : ev.point,
                                   /*lockTarget=*/true, /*saveTarget=*/false);

    return false; // TS L1030: false means don't do screen update.
}

// Ported from: itwinjs-core ViewTargetCenter.needDepthPoint (ViewTool.ts:1033-1037)。
bool ViewTargetCenter::needDepthPoint(BeButtonEvent const& /*ev*/, bool /*isPreview*/)
{
    // TS L1035-1036: only when the target-center handle itself has focus while
    // modifying (moving the target point picks a new depth).
    ViewingToolHandle* focusHandle = viewTool->inHandleModify ? viewTool->viewHandles.focusHandle() : nullptr;
    return focusHandle && ViewHandleType::TargetCenter == focusHandle->handleType();
}

// ---------------------------------------------------------------------------
// ViewLook
// Ported from: itwinjs-core ViewLook (ViewTool.ts:1323-1408).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ViewLook.testHandleForHit (ViewTool.ts:1332-1336).
bool ViewLook::testHandleForHit(dqGeom::Point3d /*ptScreen*/, HitOut& out)
{
    out.distance = 0.0;
    // TS L1335: Medium — always prefer over the pan handle (which IdleTool only
    // force-enables on the middle-button action).
    out.priority = ViewManipPriority::Medium;
    return true;
}

// Ported from: itwinjs-core ViewLook.firstPoint (ViewTool.ts:1338-1356).
bool ViewLook::firstPoint(BeButtonEvent const& ev)
{
    ViewManip* tool = viewTool;
    Viewport* vp = ev.viewport;
    // TS L1341-1342: if (undefined === vp) return true;
    if (!vp)
        return true;

    // TS L1344-1346: if (!view || !view.is3d() || !view.allow3dManipulations())
    //                    return false;
    // allow3dManipulations lives on ViewState3d in DanQing (the reference's
    // ViewState base declares it abstract; ViewState2d returns false) — a null
    // or 2d view takes the false branch exactly like the reference.
    ViewState* viewBase = vp->GetView();
    ViewState3d* view = viewBase ? viewBase->AsViewState3d() : nullptr;
    if (!view || !view->Allow3dManipulations())
        return false;

    // TS L1348-1350: snapshot firstPt (view px) / eyePoint / rotation.
    m_firstPtView = ev.viewPoint;
    m_eyePoint = view->getEyePoint();
    m_rotation = vp->getRotation();

    // TS L1352: vp.getWorldFrustum(this._frustum) — DanQing returns by value.
    m_frustum = vp->getWorldFrustum();
    // TS L1353: tool.beginDynamicUpdate();
    tool->beginDynamicUpdate();
    // TS L1354: this.viewTool.provideToolAssistance("Look.Prompts.NextPoint");
    tool->provideToolAssistance("Look.Prompts.NextPoint");
    return true;
}

// Ported from: itwinjs-core ViewLook.onWheel (ViewTool.ts:1358-1367).
bool ViewLook::onWheel(BeWheelEvent const& /*ev*/)
{
    ViewManip* tool = viewTool;
    // TS L1360: if (!tool.inHandleModify) return false;
    if (!tool->inHandleModify)
        return false;

    // TS L1362-1365: start over — reset point count / handle-modify /
    // dynamic-update flags and drop the handle focus.
    tool->nPts = 0;
    tool->inHandleModify = false;
    tool->inDynamicUpdate = false;
    tool->viewHandles.setFocus(-1);
    return false;
}

// Ported from: itwinjs-core ViewLook.doManipulation (ViewTool.ts:1369-1383).
bool ViewLook::doManipulation(BeButtonEvent const& ev, bool /*inDynamics*/)
{
    ViewManip* tool = viewTool;
    Viewport* viewport = tool ? tool->viewport : nullptr;
    // TS L1372-1373: if (undefined === viewport) return false;
    if (!viewport)
        return false;

    // TS L1375-1376: if (ev.viewport !== viewport) return false;
    if (ev.viewport != viewport)
        return false;

    // TS L1378: worldTransform = getLookTransform(viewport, _firstPtView, ev.viewPoint);
    dqGeom::Transform const worldTransform = getLookTransform(*viewport, m_firstPtView, ev.viewPoint);
    // TS L1379: frustum = this._frustum.transformBy(worldTransform);
    dqCommon::Frustum const frustum = m_frustum.transformBy(worldTransform);
    // TS L1380: viewport.setupViewFromFrustum(frustum);
    viewport->setupViewFromFrustum(frustum);
    return true;
}

// Ported from: itwinjs-core ViewLook.getLookTransform (ViewTool.ts:1385-1407).
dqGeom::Transform ViewLook::getLookTransform(Viewport const& vp,
                                             dqGeom::Point3d const& firstPt,
                                             dqGeom::Point3d const& currPt) const
{
    // TS L1386-1388: viewRect extents (px).
    ViewRect const viewRect = vp.viewRect();
    double const xExtent = static_cast<double>(viewRect.width());
    double const yExtent = static_cast<double>(viewRect.height());
    // TS L1389-1390: screen-space drag delta.
    double const xDelta = currPt.x - firstPt.x;
    double const yDelta = currPt.y - firstPt.y;
    // TS L1391-1392: half-viewport drag == ±90° (xAngle/yAngle in radians).
    double const xAngle = -(xDelta / xExtent) * M_PI;
    double const yAngle = -(yDelta / yExtent) * M_PI;

    // TS L1394: inverseRotation = this._rotation.inverse(). DanQing Matrix3d::
    // Inverse is the out-param form (computeCachedInverse port); singular →
    // the reference's undefined branch (identity transform, TS L1398-1399).
    dqGeom::Matrix3d inverseRotation;
    if (!m_rotation.Inverse(inverseRotation))
        return dqGeom::Transform::CreateIdentity();

    // TS L1395-1396: horizontal about unitZ, vertical about unitX. The
    // reference's createRotationAroundVector can return undefined only for a
    // near-zero axis — the unit axes never trigger it, so the TS L1398
    // undefined-check collapses to the inverse check above.
    auto const horizontalRotation = dqGeom::Matrix3d::CreateRotationAroundAxis(dqGeom::Vector3d::UnitZ(), xAngle);
    auto verticalRotation = dqGeom::Matrix3d::CreateRotationAroundAxis(dqGeom::Vector3d::UnitX(), yAngle);

    // TS L1401-1402: a.multiplyMatrixMatrix(b, out) → out = a × b
    // (core-geometry Matrix3d.multiplyMatrixMatrix, Matrix3d.ts:2115-2124);
    // DanQing MultiplyMatrix is the value-returning this×other equivalent.
    verticalRotation = verticalRotation.MultiplyMatrix(m_rotation);       // :1401
    verticalRotation = inverseRotation.MultiplyMatrix(verticalRotation);  // :1402

    // TS L1404: newRotation = horizontalRotation × verticalRotation.
    auto const newRotation = horizontalRotation.MultiplyMatrix(verticalRotation);
    // TS L1405: fixed point = eye (look rotates about the eye, eye invariant).
    return dqGeom::Transform::CreateFixedPointAndMatrix(m_eyePoint, newRotation);
}

// ---------------------------------------------------------------------------
// ViewScroll
// Ported from: itwinjs-core ViewScroll (ViewTool.ts:1500-1597).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ViewScroll.firstPoint (ViewTool.ts:1549-1553).
bool ViewScroll::firstPoint(BeButtonEvent const& ev)
{
    // TS L1550: super.firstPoint(ev).
    AnimatedHandle::firstPoint(ev);
    // TS L1551: provide scroll-specific assistance prompt.
    if (viewTool)
        viewTool->provideToolAssistance("Scroll.Prompts.NextPoint");
    return true;
}

// Ported from: itwinjs-core ViewScroll.animate (ViewTool.ts:1555-1588).
bool ViewScroll::animate()
{
    // TS L1556-1557: run base animate (cursorView gate + elapsed-time refresh).
    if (!AnimatedHandle::animate())
        return false;

    // TS L1559-1561: bail without a direction (cursor in dead zone).
    auto distOpt = getDirection();
    if (!distOpt.has_value())
        return false;
    dqGeom::Vector3d dist = *distOpt;

    // TS L1563: scale by scrollSpeed * elapsedTime.
    dist.Scale(ToolSettings::scrollSpeed * getElapsedTime());

    ViewManip* tool = viewTool;
    Viewport* viewport = tool ? tool->viewport : nullptr;
    if (!viewport)
        return false;

    // TS L1569-1585: camera-on → frustum translate; orthographic → vp.scroll.
    if (viewport->isCameraOn()) {
        // TS L1570-1582: viewToNpc both anchor points; pin z; back to world;
        // build translation; transform world frustum; setupViewFromFrustum.
        dqGeom::Point3d points[2] = {m_anchorPtView,
                                     plusScaled(m_anchorPtView, dist, 1.0)};
        // DanQing Viewport has viewToNpc via the ViewingSpace — but no public
        // array wrapper; convert each point individually through the inverse
        // of npcToView (view.x / width, view.y / height).
        ViewRect const rect = viewport->viewRect();
        double const w = static_cast<double>(rect.width());
        double const h = static_cast<double>(rect.height());
        auto viewToNpc = [&](dqGeom::Point3d const& vp) {
            return dqGeom::Point3d::From(vp.x / w, vp.y / h, vp.z);
        };
        dqGeom::Point3d npc0 = viewToNpc(points[0]);
        dqGeom::Point3d npc1 = viewToNpc(points[1]);
        npc1.z = npc0.z;
        dqGeom::Point3d const w0 = viewport->NpcToWorld(npc0);
        dqGeom::Point3d const w1 = viewport->NpcToWorld(npc1);
        dqGeom::Vector3d const offset = minus(w1, w0);
        dqGeom::Transform const offsetTransform = dqGeom::Transform::CreateTranslation(offset);
        dqCommon::Frustum frustum = viewport->getWorldFrustum();
        frustum.transformBy(offsetTransform);
        viewport->setupViewFromFrustum(frustum);
    } else {
        // TS L1584: viewport.scroll(dist, { noSaveInUndo: true }).
        // The noSaveInUndo option is a no-op without the undo stack port.
        viewport->scroll(dist);
    }

    return false;  // keep animating
}

// Ported from: itwinjs-core ViewScroll.needDepthPoint (ViewTool.ts:1591-1596).
bool ViewScroll::needDepthPoint(BeButtonEvent const& ev, bool /*isPreview*/)
{
    Viewport* vp = ev.viewport;
    if (!vp)
        return false;
    // TS L1595: vp.isCameraOn && CoordSource.User === ev.coordsFrom.
    return vp->isCameraOn() && CoordSource::User == ev.coordsFrom;
}

// ===========================================================================
// Task 11 — Concrete view tools (PanViewTool / RotateViewTool /
// ScrollViewTool / FitViewTool).
//
// PanViewTool / RotateViewTool / ScrollViewTool are thin shells: their entire
// behavior is in the header (ctor forwards to ViewManip with the handleMask;
// isExitAllowedOnReinitialize / provideInitialToolAssistance / getToolId
// overrides). The inherited ViewManip lifecycle (onReinitialize →
// provideInitialToolAssistance, changeViewport → instantiate handles,
// onDataButtonDown / startHandleDrag → handle.firstPoint/doManipulation) does
// all the work. No .cpp definitions needed for those three classes.
//
// FitViewTool extends ViewTool (not ViewManip) and ports its own
// onDataButtonDown / onPostInstall / doFit (ViewTool.ts:3231-3253).
// ===========================================================================

// ---------------------------------------------------------------------------
// FitViewTool
// Ported from: itwinjs-core FitViewTool (ViewTool.ts:3197-3254).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core FitViewTool.onDataButtonDown (ViewTool.ts:3231-3236).
EventHandled FitViewTool::onDataButtonDown(BeButtonEvent const& ev)
{
    // TS L3232-3233: if (ev.viewport) return await this.doFit(...) ? Yes : No;
    if (ev.viewport) {
        bool const ok = doFit(ev.viewport, oneShot, doAnimate, isolatedOnly);
        return ok ? EventHandled::Yes : EventHandled::No;
    }
    // TS L3235: return EventHandled.No;
    return EventHandled::No;
}

// Ported from: itwinjs-core FitViewTool.onPostInstall (ViewTool.ts:3238-3245).
void FitViewTool::onPostInstall()
{
    // TS L3239: await super.onPostInstall();
    // ViewTool has no onPostInstall override (base InteractiveTool::onPostInstall
    // is {}); calling the base explicitly is a no-op but preserves the reference
    // call shape for future lifecycle wiring.
    ViewTool::onPostInstall();  // base no-op

    // TS L3240-3241: if (undefined === this.viewport || !this.oneShot)
    //                   this.provideToolAssistance();
    if (viewport == nullptr || !oneShot)
        provideToolAssistance();  // FitViewTool's own no-arg overload

    // TS L3243-3244: if (this.viewport) await this.doFit(...);
    if (viewport)
        doFit(viewport, oneShot, doAnimate, isolatedOnly);
}

// Ported from: itwinjs-core FitViewTool.doFit (ViewTool.ts:3247-3253).
// （doFit 的 doAnimate 参数与同名类成员遮蔽——沿用本函数既有 oneShot→oneShotArg
// 的后缀避让先例，参数实为参考同名实参。）
bool FitViewTool::doFit(Viewport* vp, bool oneShotArg, bool doAnimateArg, bool /*isolatedOnly*/)
{
    if (!vp)
        return false;

    // TS L3248: if (!isolatedOnly || !await ViewManip.zoomToAlwaysDrawnExclusive(...))
    //               ViewManip.fitViewWithGlobeAnimation(viewport, doAnimate);
    // Step 3 simplification: zoomToAlwaysDrawnExclusive (always-drawn-exclusive
    // element zoom) is not ported (no always-drawn / never-drawn plumbing in
    // Step 3) → returns false → always fall through to fitViewWithGlobeAnimation.
    // fitViewWithGlobeAnimation's globe branch (viewingGlobe / cartographic
    // animation) is not ported (no globe mode in Step 3) → always falls through
    // to the plain fitView path: computeFitRange + lookAtVolume + synchWithView.
    //
    // Ported from: itwinjs-core ViewManip.fitView (ViewTool.ts:821-827) +
    //               computeFitRange (ViewTool.ts:810-819).
    // Step 3 fallback for computeFitRange: the reference calls
    // viewport.computeViewRange() (model extents via TileTree ranges +
    // clip-volume intersection). computeViewRange is not ported (TileTree
    // range union is TODO pending the tile-tree integration); we use the
    // view's CURRENT extents (origin + extents) as the fit range. This is
    // faithful for the no-geometry Step 3 case (BlankConnection with no
    // loaded tiles → model range is undefined → the view's stored extents
    // are the only available bounding range). For the identity-rotation case
    // this is observably a no-op (fit range == current view); for non-identity
    // rotations LookAtVolume re-derives extents from the rotated bounding box.
    // TODO: viewport.computeViewRange + ViewManip.computeFitRange (clip-volume
    //       intersection) — port with the always-drawn / never-drawn / clip-
    //       volume plumbing.
    ViewState* viewBase = vp->GetView();
    ViewState3d* view = viewBase ? viewBase->AsViewState3d() : nullptr;
    if (!view)
        return oneShotArg;  // 2D view fit is TODO (Step 3 only has 3D views).

    // Ported from: itwinjs-core ViewManip.computeFitRange (ViewTool.ts:810-819) ->
    //   viewport.computeViewRange(). computeViewRange (TileTree-range union + clip
    //   intersection) is not ported; SpatialViewState::ComputeFitRange falls back to
    //   projectExtents x1.0001 (SpatialViewState.ts:145-160), which is exactly the
    //   blank-connection case (no loaded tiles). For a non-spatial ViewState3d we keep
    //   the prior fallback of the view's current world extents as the fit range.
    dqGeom::Range3d range;
    if (auto* spatial = view->AsSpatialViewState()) {
        range = spatial->ComputeFitRange();
    } else {
        dqGeom::Point3d const origin = view->GetOrigin();
        dqGeom::Vector3d const extents = view->GetExtents();
        range = dqGeom::Range3d(
            origin.x, origin.y, origin.z,
            origin.x + extents.x, origin.y + extents.y, origin.z + extents.z);
    }

    // TS L824: viewport.view.lookAtVolume(range, aspect, options) —— aspect 必须
    // 传：adjustViewDelta 的纵横比分支把短轴撑大（ViewState.ts:908-914，只增不
    // 缩），fit 后 extents.x = range·aspect（保证 range 完整可见）。此前不传，
    // LookAtVolume 只做 x1.04 膨胀，随后 synchWithView 的 FixAspectRatio 反向压
    // y —— 最终 x 少了 ~aspect 倍（用户所见"Fit 后格子比 DTA 大"）。
    double const aspect = vp->viewRect().aspect();
    view->LookAtVolume(range, &aspect, nullptr);

    // TS L825: viewport.synchWithView({ animateFrustumChange: doAnimate }).
    // synchWithView 的 options.animateFrustumChange 语义（Viewport.ts:3595-3596）：
    // `true === options.animateFrustumChange` —— 显式 true 才动画。FitViewTool 构造
    // 默认 doAnimate=true（ViewTool.ts:3210），此前参数被丢弃恒走无动画分支——
    // 移植缺口，本修复按参考接回。
    ViewChangeOptions opts;
    opts.animateFrustumChange = doAnimateArg;
    vp->synchWithView(opts);
    vp->RequestRedraw();
    // TS L826: viewport.viewCmdTargetCenter = undefined.
    // viewCmdTargetCenter is not yet ported; no-op (faithful — the field starts
    // undefined and remains so for the no-prior-target case).

    // TS L3250-3252: if (oneShot) await this.exitTool(); return oneShot;
    if (oneShotArg) {
        exitTool();
        return true;  // oneShotArg == true
    }
    return false;  // oneShotArg == false
}

// ---------------------------------------------------------------------------
// StandardViewTool
// Ported from: itwinjs-core StandardViewTool (ViewTool.ts:3496-3524).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core StandardViewTool.onPostInstall (ViewTool.ts:3503-3522).
void StandardViewTool::onPostInstall()
{
    // TS L3504: await super.onPostInstall();  (ViewTool base onPostInstall is a no-op.)
    ViewTool::onPostInstall();

    // TS L3505-3521: if (this.viewport) { ... }
    if (viewport) {
        auto* vp = viewport;
        // TS L3506 reads vp.view (a ViewState3d in the reference's 3D path). GetView
        // returns the ViewState base; downcast to ViewState3d (the only concrete Step
        // 3 view) via the type guard — same pattern as FitViewTool::doFit.
        ViewState* viewBase = vp->GetView();
        ViewState3d* view = viewBase ? viewBase->AsViewState3d() : nullptr;
        if (view) {
            // TS L3506: id = vp.view.allow3dManipulations() ? _standardViewId : Top
            auto const id = view->Allow3dManipulations() ? m_standardViewId : StandardViewId::Top;
            // TS L3507: rMatrix = AccuDraw.getStandardRotation(id, vp, vp.isContextRotationRequired)
            // DanQing has no AccuDraw context-rotation (not ported); use the plain standard rotation.
            // TODO(faithful): AccuDraw.getStandardRotation ACS-context path.
            auto const rMatrix = StandardView::GetStandardRotation(id);
            // TS L3508: inverse = rMatrix.inverse(); if (inverse) { ... }
            dqGeom::Matrix3d inverse;
            if (rMatrix.Inverse(inverse)) {
                // TS L3509: targetMatrix = inverse.multiplyMatrixMatrix(vp.rotation)
                auto const targetMatrix = inverse.MultiplyMatrix(view->getRotation());
                // TS L3510: rotateTransform = Transform.createFixedPointAndMatrix(targetPoint, targetMatrix)
                auto const rotateTransform = dqGeom::Transform::CreateFixedPointAndMatrix(
                    ViewManip::getDefaultTargetPointWorld(*vp), targetMatrix);
                // TS L3511-3516: newFrustum = vp.getFrustum(); newFrustum.multiply(rotateTransform);
                //                 vp.view.setupFromFrustum(newFrustum); vp.synchWithView({animateFrustumChange})
                auto newFrustum = vp->getFrustum();
                newFrustum.multiply(rotateTransform);
                view->SetupFromFrustum(newFrustum);
                // TS L3521: vp.synchWithView({ animateFrustumChange: true });
                ViewChangeOptions syncOptions;
                syncOptions.animateFrustumChange = true;
                vp->synchWithView(syncOptions);
            }
        }
    }
    // TS L3523: return this.exitTool();
    exitTool();
}

// ---------------------------------------------------------------------------
// WindowAreaTool
// Ported from: itwinjs-core WindowAreaTool (ViewTool.ts:3531-3816).
// ---------------------------------------------------------------------------

WindowAreaTool::~WindowAreaTool()
{
    // AcsTriadDecorator dtor 同理（AcsTriadDecorator.cpp:189-200）：不
    // disposeGraphic——图形由逐视口 RenderSystem 创建，析构时其 GL 资源可能已随
    // 管线释放；~RenderGraphicOwner 不触 GL（delete 仅释放 owner 结构）。会话内
    // 的逐帧 dispose 由 decorate/disposeBoxGraphic 负责。
    delete m_boxGraphicOwner;
    m_boxGraphicOwner = nullptr;
    m_boxGraphic = nullptr;
}

void WindowAreaTool::disposeBoxGraphic() noexcept
{
    // AcsTriadDecorator::Decorate 既有模式（AcsTriadDecorator.cpp:224-229）：
    // CollectDecorations 在调 decorate 前已 clear 列表（Viewport.cpp CollectDecorations），
    // 旧图形此刻不被引用，可安全 dispose。
    if (m_boxGraphicOwner) {
        m_boxGraphicOwner->disposeGraphic();
        delete m_boxGraphicOwner;
        m_boxGraphicOwner = nullptr;
        m_boxGraphic = nullptr;
    }
}

// Ported from: itwinjs-core WindowAreaTool.onPostInstall (ViewTool.ts:3542-3545).
void WindowAreaTool::onPostInstall()
{
    ViewTool::onPostInstall();  // :3543 — super.onPostInstall()（基类无操作，保留调用形状）
    provideToolAssistance();    // :3544
}

// Ported from: itwinjs-core WindowAreaTool.onReinitialize (ViewTool.ts:3547-3552)。
void WindowAreaTool::onReinitialize()
{
    m_haveFirstPoint = false;      // :3548
    m_firstPtWorld.Zero();         // :3549 — _firstPtWorld.setZero()
    m_secondPtWorld.Zero();        // :3550 — _secondPtWorld.setZero()
    provideToolAssistance();       // :3551
    // 参考不调 super.onReinitialize()（:3547-3552 无 super 调用）——保持 1:1。
}

// Ported from: itwinjs-core WindowAreaTool.onResetButtonUp (ViewTool.ts:3554-3561).
EventHandled WindowAreaTool::onResetButtonUp(BeButtonEvent const& ev)
{
    if (m_haveFirstPoint) {            // :3555
        onReinitialize();              // :3556
        return EventHandled::Yes;      // :3557
    }
    return ViewTool::onResetButtonUp(ev);  // :3560 — super.onResetButtonUp(ev)（exitTool）
}

// Ported from: itwinjs-core WindowAreaTool.onDataButtonDown (ViewTool.ts:3584-3613).
EventHandled WindowAreaTool::onDataButtonDown(BeButtonEvent const& ev)
{
    if (nullptr == ev.viewport)    // :3585
        return EventHandled::Yes;  // :3586

    if (nullptr == viewport) {     // :3588
        viewport = ev.viewport;    // :3589
    } else if (!ev.viewport->GetView()->hasSameCoordinates(*viewport->GetView())) {  // :3590
        if (m_haveFirstPoint)                          // :3591
            return EventHandled::Yes;                  // :3592
        viewport = ev.viewport;                        // :3593
        m_lastPtView = ev.viewPoint;                   // :3594
        Application::Get().GetViewManager().invalidateDecorationsAllViews();  // :3595
        return EventHandled::Yes;                      // :3596
    }

    if (m_haveFirstPoint) {                          // :3599
        m_secondPtWorld = ev.point;                  // :3600 — _secondPtWorld.setFrom(ev.point)
        doManipulation(ev, false);                   // :3601
        onReinitialize();                            // :3602
        viewport->InvalidateDecorations();           // :3603 — invalidateDecorations()
    } else {
        m_firstPtWorld = ev.point;                   // :3605 — _firstPtWorld.setFrom(ev.point)
        m_secondPtWorld = m_firstPtWorld;            // :3606 — _secondPtWorld.setFrom(_firstPtWorld)
        m_haveFirstPoint = true;                     // :3607
        m_lastPtView = ev.viewPoint;                 // :3608
        provideToolAssistance();                     // :3609
    }

    return EventHandled::Yes;                        // :3612
}

// Ported from: itwinjs-core WindowAreaTool.onMouseMotion (ViewTool.ts:3615).
// TODO: 触摸处理 onTouchTap/onTouchMoveStart/onTouchMove/onTouchComplete/
//       onTouchCancel（ViewTool.ts:3617-3637）未移植——DanQing ViewTool 全文件
//       尚无触摸子系统，待其落地时一并移植。
void WindowAreaTool::onMouseMotion(BeButtonEvent const& ev)
{
    doManipulation(ev, true);
}

// Ported from: itwinjs-core WindowAreaTool.computeWindowCorners (ViewTool.ts:3639-3677)。
std::vector<dqGeom::Point3d>* WindowAreaTool::computeWindowCorners() noexcept
{
    Viewport* const vp = viewport;                     // :3640
    if (nullptr == vp)                                 // :3641
        return nullptr;                                // :3642 — undefined

    // :3644-3646 — corners[0].setFrom(_firstPtWorld); corners[1].setFrom(_secondPtWorld)
    m_corners[0] = m_firstPtWorld;
    m_corners[1] = m_secondPtWorld;
    vp->worldToViewArray(m_corners);                   // :3647

    // :3649 — delta = corners[1].minus(corners[0])
    auto const delta = minus(m_corners[1], m_corners[0]);
    // :3650 — ToolSettings.startDragDistanceInches（ToolSettings.ts:34 = 0.15；子系统
    // 未移植 → 按值移植为 ToolSettings::startDragDistanceInches，ViewTool.h:210）。
    if (delta.MagnitudeXY() < vp->PixelsFromInches(ToolSettings::startDragDistanceInches))
        return nullptr;                                // :3651

    auto const currentDelta = vp->viewDelta();         // :3653 — vp.viewDelta
    if (currentDelta.x == 0.0 || delta.x == 0.0)       // :3654
        return nullptr;                                // :3655

    double const skew = vp->GetView()->getAspectRatioSkew();   // :3657
    double const viewAspect = skew * currentDelta.y / currentDelta.x;  // :3658
    double const aspectRatio = std::fabs(delta.y / delta.x);           // :3659

    double halfDeltaX;                                   // :3661-3669
    double halfDeltaY;
    if (aspectRatio < viewAspect) {
        halfDeltaX = std::fabs(delta.x) / 2.0;
        halfDeltaY = halfDeltaX * viewAspect;
    } else {
        halfDeltaY = std::fabs(delta.y) / 2.0;
        halfDeltaX = halfDeltaY / viewAspect;
    }

    // :3671 — center = corners[0].plusScaled(delta, 0.5)
    auto const center = plusScaled(m_corners[0], delta, 0.5);
    m_corners[0].x = center.x - halfDeltaX;              // :3672
    m_corners[0].y = center.y - halfDeltaY;              // :3673
    m_corners[1].x = center.x + halfDeltaX;              // :3674
    m_corners[1].y = center.y + halfDeltaY;              // :3675
    return &m_corners;                                   // :3676 — 参考返回 _corners 成员本身
}

// Ported from: itwinjs-core WindowAreaTool.decorate (ViewTool.ts:3679-3731).
void WindowAreaTool::decorate(DecorateContext& context)
{
    // :3680-3681 — 无视口或坐标系不同 → 不装饰
    if (nullptr == viewport || nullptr == viewport->GetView() ||
        nullptr == context.GetViewport().GetView() ||
        !context.GetViewport().GetView()->hasSameCoordinates(*viewport->GetView()))
        return;
    Viewport* const vp = viewport;
    // :3683 — 对比色（橡皮筋描边/首点 + 十字线共用）
    auto const color = vp->getContrastToBackgroundColor();

    if (m_haveFirstPoint) {                              // :3684
        auto* corners = computeWindowCorners();          // :3685
        if (nullptr == corners)                          // :3686
            return;                                      // :3687
        // （参考此刻的 Decorations 每帧新建，不画旧橡皮筋；DanQing 的列表已被
        //  CollectDecorations clear，不 Add 即等价——旧图形仅在下一次重建时 dispose。）

        // :3689-3694 — 由角点构造闭合矩形（_shapePts[5]，首尾同点）
        auto const& c0 = (*corners)[0];
        auto const& c1 = (*corners)[1];
        m_shapePts[0].x = m_shapePts[3].x = c0.x;
        m_shapePts[1].x = m_shapePts[2].x = c1.x;
        m_shapePts[0].y = m_shapePts[1].y = c0.y;
        m_shapePts[2].y = m_shapePts[3].y = c1.y;
        m_shapePts[0].z = m_shapePts[1].z = m_shapePts[2].z = m_shapePts[3].z = c0.z;
        m_shapePts[4] = m_shapePts[0];
        vp->viewToWorldArray(m_shapePts);                // :3695 — 视口像素 → 世界

        // :3697 — context.createGraphicBuilder(GraphicType.WorldOverlay)。
        // DanQing：DecorateContext 无 createGraphicBuilder——经视口工厂
        // （AcsTriadDecorator.cpp:239-246 既有模式）。
        dqRender::GraphicBuilderOptions opts;
        opts.type = dqRender::GraphicType::WorldOverlay;
        // GraphicBuilder.finish() 需 computeChordTolerance 闭包（PrimitiveBuilder 的
        // LOD/细分闸口）。参考的视口工厂按几何中心取像素尺寸（GraphicBuilder.ts:169-183）；
        // 橡皮筋为直线矩形（无曲线），细分无关——取 NPC 中心像素尺寸（既定简化）。
        double const worldPerPixel = vp->GetViewingSpace().getPixelSizeAtPoint(nullptr);
        opts.computeChordTolerance = [worldPerPixel]() { return worldPerPixel; };
        auto builder = vp->createGraphicBuilder(opts);
        if (!builder)
            return;   // 无渲染管线（headless 单测）——DanQing 适配守卫（参考恒有 RenderSystem）。

        disposeBoxGraphic();   // 重建前 dispose 旧图形（所有权见 ViewTool.h 类注释）

        builder->setBlankingFill(m_fillColor);           // :3699
        builder->addShape(m_shapePts.data(), m_shapePts.size());   // :3700

        // :3702-3703
        builder->setSymbology(color, color, static_cast<uint32_t>(ViewHandleWeight::Thin));
        builder->addLineString(m_shapePts.data(), m_shapePts.size());

        // :3705-3706
        builder->setSymbology(color, color, static_cast<uint32_t>(ViewHandleWeight::FatDot));
        builder->addPointString(&m_firstPtWorld, 1);

        // :3708 — addDecorationFromBuilder(builder) = addDecoration(builder.type, builder.finish())
        auto* graphic = builder->finish();
        if (!graphic)
            return;
        m_boxGraphic = graphic;
        m_boxGraphicOwner = vp->createGraphicOwner(graphic);   // DanQing 所有权（参考靠 GC）
        context.AddDecoration(dqRender::GraphicType::WorldOverlay, graphic);
        return;                                                // :3709
    }

    // :3712-3713 — 全屏十字线只画在光标视口（cursorView = currentInputState.viewport，
    // ToolAdmin.ts:537；W4 已移植 cursorView 访问器）。
    if (!m_lastPtView.has_value() ||
        &context.GetViewport() != Application::Get().GetToolAdmin().cursorView())
        return;

    // :3715-3717 — 像素中心对齐（floor + 0.5，1px 线在像素边界间居中避免抗锯齿模糊）
    auto cursorPt = *m_lastPtView;
    cursorPt.x = std::floor(cursorPt.x) + 0.5;
    cursorPt.y = std::floor(cursorPt.y) + 0.5;
    ViewRect const rect = vp->viewRect();              // :3718

    // :3720-3729 — drawDecoration 体逐字移植
    dqRender::CanvasDecoration decoration;
    decoration.drawDecoration = [cursorPt, rect, color](dqRender::CanvasContext& ctx) {
        ctx.beginPath();
        // :3722 — strokeStyle = (ColorDef.black === color ? "black" : "white")
        ctx.setStrokeStyle(color.equals(dqCommon::ColorDef::black)
            ? dqCommon::ColorDef::black : dqCommon::ColorDef::white);
        ctx.setLineWidth(1.0);                          // :3723 — lineWidth = 1
        ctx.moveTo(static_cast<double>(rect.left), cursorPt.y);    // :3724
        ctx.lineTo(static_cast<double>(rect.right), cursorPt.y);   // :3725
        ctx.moveTo(cursorPt.x, static_cast<double>(rect.top));     // :3726
        ctx.lineTo(cursorPt.x, static_cast<double>(rect.bottom));  // :3727
        ctx.stroke();                                   // :3728
    };
    context.AddCanvasDecoration(std::move(decoration));  // :3730 — { drawDecoration }
}

// Ported from: itwinjs-core WindowAreaTool.doManipulation (ViewTool.ts:3733-3815).
void WindowAreaTool::doManipulation(BeButtonEvent const& ev, bool inDynamics)
{
    m_secondPtWorld = ev.point;                        // :3734 — _secondPtWorld.setFrom(ev.point)
    if (inDynamics) {
        // :3736-3739 — 动态期跨坐标系视口：清十字线点（不在本视口画）
        if (nullptr != viewport && nullptr != ev.viewport && nullptr != ev.viewport->GetView() &&
            nullptr != viewport->GetView() &&
            !ev.viewport->GetView()->hasSameCoordinates(*viewport->GetView())) {
            m_lastPtView = std::nullopt;               // :3737 — undefined
            return;                                    // :3738
        }
        m_lastPtView = ev.viewPoint;                   // :3740
        Application::Get().GetViewManager().invalidateDecorationsAllViews();  // :3741
        return;
    }

    auto* corners = computeWindowCorners();            // :3745
    if (nullptr == corners)                            // :3746
        return;                                        // :3747

    Viewport* const vp = viewport;                     // :3749
    if (nullptr == vp)                                 // :3750
        return;                                        // :3751

    ViewState* const view = vp->GetView();             // :3753
    if (nullptr == view)
        return;   // DanQing 空守卫（参考 :3753 直接取 vp.view——视口按构造恒有视图）
    vp->viewToWorldArray(*corners);                    // :3754

    // :3756-3758 — opts.onExtentsError = (stat) => view.outputStatusMessage(stat)。
    // view.outputStatusMessage（ViewState.ts:883-886）经 IModelApp.notifications 发通知——
    // 通知子系统未移植；回调保留参考的返回语义（原样返回 stat），侧效果 TODO。
    OnViewExtentsError opts;
    opts.onExtentsError = [](ViewStatus stat) { return stat; };

    dqGeom::Vector3d delta;                            // :3760
    auto* view3d = view->AsViewState3d();
    if (nullptr != view3d && view3d->IsCameraOn()) {   // :3762 — view.is3d() && view.isCameraOn
        // TODO: camera path deferred — view.lookAt（ViewState.ts:1891）+
        //       determineVisibleDepthRange（Viewport.ts:1916）未移植；blank 初始正交不走此路。
        //       参考相机路径 :3763-3791（windowArray→windowRange、determineVisibleDepthRange
        //       回退 {minimum:0, maximum: ViewManip::getFocusPlaneNpc(vp)}（:3768-3770）、
        //       getLensAngle/focusDist = delta.x/(2·tan(lens/2))（:3784）、lookAt（:3789）、
        //       globalAlignment = { target: newTarget }（:3791））。落地时按该段逐行移植。
        return;
    }

    // --- 正交路径（:3792-3812，完整移植） ---
    auto const rot = vp->getRotation();                // :3793 — vp.rotation
    // :3794 — rot.multiplyVectorArrayInPlace(corners)。dqGeom 无数组批量 API
    // （multiplyVectorArrayInPlace 未移植）→ 逐点 MultiplyVector（等价循环——
    // Matrix3d.ts multiplyVectorArrayInPlace 内部亦逐点 multiplyVector）。
    for (auto& pt : *corners) {
        auto const v = rot.MultiplyVector(dqGeom::Vector3d::From(pt.x, pt.y, pt.z));
        pt.Init(v.x, v.y, v.z);
    }

    // :3796 — range = Range3d.createArray(corners)
    auto range = dqGeom::Range3d::create({(*corners)[0], (*corners)[1]});
    // :3797 — delta = Vector3d.createStartEnd(range.low, range.high)
    delta = dqGeom::Vector3d::FromStartEnd(range.low, range.high);
    // :3799 — get the view extents: delta.z = view.getExtents().z
    delta.z = view->GetExtents().z;

    // :3801 — originVec = rot.multiplyTransposeXYZ(range.low.x, range.low.y, range.low.z)。
    // 参考返回 Vector3d 并直接传给 adjustViewDelta 的 origin 形参（TS 结构类型：
    // Vector3d 即 XYZ）；C++ 显式转 Point3d（同值转换，adjustViewDelta 原地改它，
    // :916-917 的 origin 半移随后经 setOrigin 生效——与参考 :3809 同一变量）。
    auto const originVecV = rot.MultiplyTransposeVector(
        dqGeom::Vector3d::From(range.low.x, range.low.y, range.low.z));
    dqGeom::Point3d originVec = dqGeom::Point3d::From(originVecV.x, originVecV.y, originVecV.z);

    // :3804-3806 — make sure its not too big or too small
    auto const stat = view->adjustViewDelta(delta, originVec, rot, vp->viewRect().aspect(), &opts);
    if (stat != ViewStatus::Success)
        return;

    view->SetExtents(delta);   // :3808
    view->SetOrigin(originVec);  // :3809
    // :3810-3811 — if (view.is3d()) globalAlignment = { target: range.center }。
    // globalAlignment → synchWithView 选项 → view.alignToGlobe（Viewport.ts:3588-3589）
    // 未移植（DanQing 无 globe）；TODO 随 alignToGlobe 落地。

    // :3814 — vp.synchWithView({ animateFrustumChange: true, globalAlignment })。
    // animateFrustumChange 为语义位（DanQing 无 frustum-change 动画器，Viewport.h:70-73）；
    // globalAlignment 未移植（上注）。
    ViewChangeOptions syncOptions;
    syncOptions.animateFrustumChange = true;
    vp->synchWithView(syncOptions);
}

// ---------------------------------------------------------------------------
// ViewUndoTool / ViewRedoTool
// Ported from: itwinjs-core ViewUndoTool/ViewRedoTool (ViewTool.ts:4111-4134).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ViewUndoTool.onPostInstall (ViewTool.ts:4115-4119)：
// viewport.doUndo(ScreenViewport.animation.time.normal); exitTool()。
// DanQing 无动画——doUndo 去 animationTime 形参（Viewport.h:317 注释）。
void ViewUndoTool::onPostInstall()
{
    if (viewport)
        viewport->doUndo();
    exitTool();
}

// Ported from: itwinjs-core ViewRedoTool.onPostInstall (ViewTool.ts:4129-4133)：
// viewport.doRedo(ScreenViewport.animation.time.normal); exitTool()（动画同上略）。
void ViewRedoTool::onPostInstall()
{
    if (viewport)
        viewport->doRedo();
    exitTool();
}

}  // namespace dqApp

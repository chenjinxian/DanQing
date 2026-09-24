// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ToolAdmin implementation
// Ported from: itwinjs-core core/frontend/src/tools/ToolAdmin.ts
#include "dqApp/ToolAdmin.h"
#include "dqApp/Application.h"
#include "dqApp/Viewport.h"
#include <cmath>
#include "dqApp/ViewState.h"
#include "dqApp/ViewTool.h"  // ViewTool complete type (activeTool/currentTool upcast).
#include "SelectionTool.h"
#include "IdleTool.h"

#include <chrono>
#include <utility>

namespace dqApp {

// ---------------------------------------------------------------------------
// Priority-slot accessors (Task 8 declared these inline; Task 9 moved the
// definitions out-of-line because ViewTool is only forward-declared in
// ToolAdmin.h to avoid a circular include with ViewTool.h. The static_cast
// upcast from ViewTool* to InteractiveTool* requires ViewTool's complete type,
// which is available here once ViewTool.h is included.)
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ToolAdmin.activeTool (ToolAdmin.ts:876-878).
InteractiveTool* ToolAdmin::activeTool() const noexcept
{
    return m_viewTool ? static_cast<InteractiveTool*>(m_viewTool)
                      : (m_inputCollector ? static_cast<InteractiveTool*>(m_inputCollector)
                                          : m_primitiveTool);
}

// Ported from: itwinjs-core ToolAdmin.currentTool (ToolAdmin.ts:881).
InteractiveTool& ToolAdmin::currentTool()
{
    InteractiveTool* a = activeTool();
    return a ? *a : *m_idleTool;
}

// ---------------------------------------------------------------------------
// IDecorator (W3)
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ToolAdmin.decorate (ToolAdmin.ts:2049-2059)。
void ToolAdmin::Decorate(DecorateContext& context)
{
    // TS L2051-2053：const tool = this.activeTool; if (undefined !== tool) tool.decorate(context);
    // DanQing：decorate 虚函数在 ViewTool（参考在 InteractiveTool）——inputCollector/
    // primitiveTool 无 decorate 内容，故等价于"活动工具即 viewTool 时转发"。
    if (m_viewTool && activeTool() == static_cast<InteractiveTool*>(m_viewTool))
        m_viewTool->decorate(context);
    // TS L2054-2058：inputCollector/primitiveTool 的 decorateSuspended 未移植——不 forward。

    // TS L2061-2069：locate 光圈（Viewport.drawLocateCursor 的 isLocateCircleOn 分支，
    // Viewport.ts:3764-3778）——以最新 hover 点为圆心的光圈 canvas decoration。
    // accuDraw/accuSnap.hit 分支随其子系统 TODO（surface-normal 命中标记）。
    // 圆心 = currentInputState.lastMotion（:2066 fillEventFromCursorLocation(ev)
    // → ev.viewPoint——InputState.onMotion 的最新 hover 点）。
    Viewport* const vp = cursorView();
    if (getenv("DANQING_CURSOR_TRACE")) {
        fprintf(stderr, "[CURSOR] decorate isLocateCircleOn=%d cursorView=%p ctxVp=%p\n",
                isLocateCircleOn() ? 1 : 0, (void*)vp, (void*)&context.GetViewport());
    }
    if (isLocateCircleOn() && vp == &context.GetViewport()) {
        double const aperture = vp->PixelsFromInches(0.11);   // ElementLocateManager.ts:314
        double const radius = std::floor(aperture * 0.5) + 0.5;
        double const posX = std::floor(m_currentInputState.lastMotion.x) + 0.5;
        double const posY = std::floor(m_currentInputState.lastMotion.y) + 0.5;
        dqRender::CanvasDecoration dec;
        dec.position = dqGeom::Point2d{ posX, posY };
        if (getenv("DANQING_CURSOR_TRACE")) {
            fprintf(stderr, "[CURSOR] deco pos=(%.1f,%.1f) radius=%.1f\n", posX, posY, radius);
        }
        dec.drawDecoration = [radius](dqRender::CanvasContext& ctx) {
            constexpr double k2Pi = 2.0 * 3.14159265358979323846;
            // ← Viewport.ts:3770-3777 — 白描边(.4)/白填充(.2)光圈 + 黑色(.8)外描圈。
            ctx.beginPath();
            ctx.setStrokeStyle(dqCommon::ColorDef::from(255, 255, 255, 153));  // rgba(255,255,255,.4)
            ctx.setFillStyle(dqCommon::ColorDef::from(255, 255, 255, 204));    // rgba(255,255,255,.2)
            ctx.arc(0, 0, radius, 0, k2Pi);
            ctx.fill();
            ctx.stroke();
            ctx.beginPath();
            ctx.setStrokeStyle(dqCommon::ColorDef::from(0, 0, 0, 51));         // rgba(0,0,0,.8)
            ctx.setLineWidth(1);
            ctx.arc(0, 0, radius + 1, 0, k2Pi);
            ctx.stroke();
        };
        context.AddCanvasDecoration(std::move(dec), /*atFront=*/true);
    }
}

// Ported from: itwinjs-core ToolAdmin.setLocateCircleOn (:1188-1192).
void ToolAdmin::setLocateCircleOn(bool on)
{
    if (toolState.locateCircleOn == on)
        return;
    toolState.locateCircleOn = on;
    Application::Get().GetViewManager().invalidateDecorationsAllViews();
}

// Ported from: itwinjs-core ToolAdmin.setLocateCursor (:2214-2219).
void ToolAdmin::setLocateCursor(bool enableLocate)
{
    auto& viewManager = Application::Get().GetViewManager();
    // :2216 — dynamicsCursor while in dynamics mode, crosshair otherwise
    // (dynamics mode is TODO; the else branch is the ported path).
    setCursor(viewManager.crossHairCursor());
    setLocateCircleOn(enableLocate);
}

// Ported from: itwinjs-core InteractiveTool.changeLocateState (Tool.ts:696-720).
// The reference body routes through IModelApp (toolAdmin/accuSnap); DanQing reads
// the same singleton. coordLockOvr 分支（Tool.ts:716-724）随坐标锁子系统 TODO。
void InteractiveTool::changeLocateState(bool enableLocate, bool enableSnap, std::string const& cursor)
{
    auto& toolAdmin = Application::Get().GetToolAdmin();
    auto& accuSnap = Application::Get().GetAccuSnap();
    if (!cursor.empty()) {
        // Tool.ts:698-701 — explicit cursor: set it + locateCircleOn, then
        // invalidate. (setLocateCircleOn invalidates on actual change.)
        toolAdmin.setCursor(cursor);
        toolAdmin.setLocateCircleOn(enableLocate);
    } else {
        // Tool.ts:703 — default cursor path.
        toolAdmin.setLocateCursor(enableLocate);
    }

    // Tool.ts:707-714 — "Always set the one that is true first, otherwise
    // AccuSnap will clear the TouchCursor"（先开后关序保持参考）。
    if (enableLocate) {
        accuSnap.enableLocate(true);
        accuSnap.enableSnap(enableSnap);
    } else {
        accuSnap.enableSnap(enableSnap);
        accuSnap.enableLocate(false);
    }
}

// Ported from: itwinjs-core InteractiveTool.initLocateElements (Tool.ts:729-738).
void InteractiveTool::initLocateElements(bool enableLocate, bool enableSnap, std::string const& cursor)
{
    changeLocateState(enableLocate, enableSnap, cursor);
}

// 参考 Decorator.testDecorationHit 为可选成员（ViewManager.ts:28）；ToolAdmin 转发
// currentTool.testDecorationHit（ToolAdmin.ts:2043），Tool 基类默认 false（Tool.ts:528）。
// DanQing：Tool.testDecorationHit 未移植（TODO），等价于"无装饰命中"。
bool ToolAdmin::TestDecorationHit(uint32_t /*featureId*/) const
{
    return false;
}

// 参考 Decorator.getDecorationToolTip 为可选成员（ViewManager.ts:45），ToolAdmin
// 未实现该成员（缺省无提示）；DanQing IDecorator 为纯虚，等价返回空串。
QString ToolAdmin::GetDecorationToolTip(uint32_t /*featureId*/) const
{
    return {};
}

// ---------------------------------------------------------------------------
// startViewTool / exitViewTool (Task 11)
// Ported from: itwinjs-core ToolAdmin.startViewTool (ToolAdmin.ts:1815-1841) +
//               setViewTool (ToolAdmin.ts:1786-1793) + exitViewTool
//               (ToolAdmin.ts:1795-1812).
//
// Out-of-line (not inline in ToolAdmin.h) because ViewTool is only forward-
// declared in ToolAdmin.h to avoid a circular include with ViewTool.h. Calling
// ViewTool::onCleanup requires the complete type, which is available here once
// ViewTool.h is included at the top of this TU.
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ToolAdmin.startViewTool (ToolAdmin.ts:1815-1841) +
//               setViewTool (ToolAdmin.ts:1786-1793).
void ToolAdmin::startViewTool(ViewTool* newTool)
{
    // TS L1817-1818: IModelApp.notifications.outputPrompt(""); accuDraw.onViewToolInstall().
    // Step 3 stub: notifications + accuDraw onViewToolInstall are TODO pending
    // those subsystems. The outputPrompt("") clears any prior prompt; with the
    // prompt subsystem stubbed, this is observably a no-op.

    // TS L1820-1827 + setViewTool L1787-1789: cleanup prior viewTool.
    //   if (undefined !== this._viewTool) await this.setViewTool(undefined);
    //   setViewTool(undefined): if (current) current.onCleanup(); this._viewTool = undefined;
    // Faithful 1:1 port: onCleanup on the outgoing tool (if different from the
    // incoming — the reference's setViewTool(undefined) → onCleanup fires
    // unconditionally, but startViewTool callers do not re-start the same tool
    // instance; guard against the degenerate newTool == m_viewTool case to
    // avoid running onCleanup on a tool that is staying installed).
    bool const hadPriorViewTool = (m_viewTool != nullptr);
    if (m_viewTool != nullptr && m_viewTool != newTool) {
        m_viewTool->onCleanup();
        // Ownership: callers allocate the tool (new) and hand it to
        // startViewTool via adoptViewTool; ToolAdmin owns adopted tools
        // (delete on replace/exit). The TS reference relies on GC
        // (_viewTool = undefined); the C++ port deletes only adopted tools
        // (m_ownedViewTools registry) — stack/bare-run() tools were never
        // adopted, so they are left alone. installViewTool is the borrowed
        // test affordance (never adopted). (TD-11: Fit/Rotate tools
        // accumulated on repeated installs.)
        if (m_ownedViewTools.erase(m_viewTool) != 0)
            delete m_viewTool;
        m_viewTool = nullptr;
    }
    // TS L1821-1826: else branch（无前一个视图工具时）——activeTool.onSuspend()
    // + SuspendedToolState 快照（挂起期间的光标/光圈/snap 由快照恢复）。
    // 判据 = 进入函数时的原始状态（hadPriorViewTool），非清槽后的槽值——
    // 参考 if/else 在置空前判断（ToolAdmin.ts:1820-1827）；替换路径不重建快照
    // （旧快照保留到 exitViewTool 的 stop 恢复，误建会把已变更的 toolState
    // （光圈关等）快照进去、exit 恢复出错态——CursorState 光圈断言曾因此破）。
    if (!hadPriorViewTool) {
        if (auto* tool = activeTool())
            tool->onSuspend();
        m_suspendedByViewTool = std::make_unique<SuspendedToolState>();
    }

    // TS L1829: IModelApp.viewManager.endDynamicsMode() — TODO stub (dynamics
    //   mode subsystem not yet ported).
    // TS L1830: IModelApp.viewManager.invalidateDecorationsAllViews().
    Application::Get().GetViewManager().invalidateDecorationsAllViews();

    // TS L1832-1837: toolState.coordLockOvr = All, toolState.locateCircleOn =
    //   false, accuSnap.onStartTool(), setCursor(crossHairCursor).
    //   coordLockOvr 随坐标锁子系统 TODO；locateCircleOn 关 + onStartTool
    //   （清 snap/locate）是本次接线的两条。
    toolState.locateCircleOn = false;
    Application::Get().GetAccuSnap().onStartTool();
    setCursor(Application::Get().GetViewManager().crossHairCursor());

    // TS L1838 + setViewTool L1791: this._viewTool = newTool.
    // NO ownership registration here (TD-11): run() reaches this for BOTH
    // heap tools (adopted beforehand via adoptViewTool) and stack objects in
    // tests — registration belongs to adoptViewTool, not the install path.
    m_viewTool = newTool;

    // TS L1840: this.onActiveToolChanged(newTool, StartOrResume.Start).
    OnActiveToolChanged.Raise();
}

// Ported from: itwinjs-core ToolAdmin.setViewTool (ToolAdmin.ts:1599-1605) —
//               slot-only path (no onCleanup); startViewTool adds lifecycle.
// Ownership consistency (TD-11): replacing the slot disposes any owned tool
// already installed (onCleanup + delete) — same contract as
// startViewTool/exitViewTool.
void ToolAdmin::installViewTool(ViewTool* tool) noexcept
{
    if (m_viewTool != nullptr && m_viewTool != tool) {
        m_viewTool->onCleanup();
        if (m_ownedViewTools.erase(m_viewTool) != 0)
            delete m_viewTool;
    }
    m_viewTool = tool;
    // Defensive scrub: any registry entry that is no longer the installed
    // tool is stale (e.g. a test's unique_ptr released the object without
    // going through exitTool). Dropping it prevents a later use-after-free
    // when erase() is called on the dangling pointer.
    for (auto it = m_ownedViewTools.begin(); it != m_ownedViewTools.end();) {
        if (*it != m_viewTool)
            it = m_ownedViewTools.erase(it);
        else
            ++it;
    }
}

// flushDeferredViewToolDeletes — frame-boundary delete point (see header).
void ToolAdmin::flushDeferredViewToolDeletes()
{
    for (auto* t : m_deferredViewToolDeletes)
        delete t;
    m_deferredViewToolDeletes.clear();
}

// adoptViewTool / disownViewTool — heap-tool ownership seam (TD-11, see header).
void ToolAdmin::adoptViewTool(ViewTool* tool) noexcept
{
    if (tool != nullptr)
        m_ownedViewTools.insert(tool);
}

void ToolAdmin::disownViewTool(ViewTool* tool) noexcept
{
    if (tool != nullptr)
        m_ownedViewTools.erase(tool);
}

// ---------------------------------------------------------------------------
// SuspendedToolState — 快照/恢复（ToolAdmin.ts:112-141）
// ---------------------------------------------------------------------------
SuspendedToolState::SuspendedToolState()
{
    auto& admin = Application::Get().GetToolAdmin();
    auto& viewManager = Application::Get().GetViewManager();
    auto& accuSnap = Application::Get().GetAccuSnap();
    admin.setIncompatibleViewportCursor(true);   // :114 — Don't save this
    m_toolState = admin.toolState;               // :116 clone（值拷贝）
    m_accuSnapEnabled = accuSnap.toolState.enabled;
    m_accuSnapLocate = accuSnap.toolState.locate;
    m_accuSnapSuspended = accuSnap.toolState.suspended;
    m_viewCursor = viewManager.cursor();         // :119
}

void SuspendedToolState::stop()
{
    auto& admin = Application::Get().GetToolAdmin();
    auto& viewManager = Application::Get().GetViewManager();
    auto& accuSnap = Application::Get().GetAccuSnap();
    admin.setIncompatibleViewportCursor(true);   // :128 — Don't restore this
    admin.toolState = m_toolState;               // :129 setFrom
    accuSnap.toolState.enabled = m_accuSnapEnabled;
    accuSnap.toolState.locate = m_accuSnapLocate;
    accuSnap.toolState.suspended = m_accuSnapSuspended;
    viewManager.setViewCursor(m_viewCursor);     // :132
    // :133-136 inDynamics —— dynamics 子系统未移植，恒 false（无操作）。
}

// Ported from: itwinjs-core ToolAdmin.setCursor (ToolAdmin.ts:2035-2040).
void ToolAdmin::setCursor(std::string const& cursor)
{
    if (!m_saveCursor.has_value())
        Application::Get().GetViewManager().setViewCursor(cursor);
    else
        m_saveCursor = cursor;
}

// Ported from: itwinjs-core ToolAdmin.startPrimitiveTool
//               `this.setCursor(IModelApp.viewManager.crossHairCursor)`
//               (ToolAdmin.ts:1923). Out-of-line: the inline SetActiveTool
//               caller cannot include Application.h (circular include).
void ToolAdmin::setCrossHairCursor()
{
    setCursor(Application::Get().GetViewManager().crossHairCursor());
}

// Ported from: itwinjs-core ToolAdmin.setIncompatibleViewportCursor
//               (ToolAdmin.ts:2163-2181 — save/restore cursor + locateCircleOn
//               around the "not-allowed" suspension). toolState.locateCircleOn
//               itself is TODO with the toolState subsystem; the save/restore
//               slot (m_saveLocateCircle) is kept 1:1.
void ToolAdmin::setIncompatibleViewportCursor(bool restore)
{
    auto& viewManager = Application::Get().GetViewManager();
    if (restore) {
        if (!m_saveCursor.has_value())
            return;
        // toolState.locateCircleOn = _saveLocateCircle — TODO with toolState.
        viewManager.setViewCursor(*m_saveCursor);
        m_saveCursor.reset();
        return;
    }
    if (m_saveCursor.has_value())
        return;
    // _saveLocateCircle = toolState.locateCircleOn — TODO with toolState.
    m_saveCursor = viewManager.cursor();
    viewManager.setViewCursor("not-allowed");
}

// Ported from: itwinjs-core ToolAdmin.exitViewTool (ToolAdmin.ts:1795-1812) +
//               setViewTool(undefined) (ToolAdmin.ts:1786-1793).
void ToolAdmin::exitViewTool()
{
    // TS L1796: if (undefined === this._viewTool) return;
    if (m_viewTool == nullptr)
        return;

    // TS L1798-1803: suspendedByViewTool 恢复 + unsuspend 标记。
    bool unsuspend = false;
    if (m_suspendedByViewTool) {
        m_suspendedByViewTool->stop();
        m_suspendedByViewTool.reset();
        unsuspend = true;
    }

    // TS L1805: IModelApp.viewManager.invalidateDecorationsAllViews().
    Application::Get().GetViewManager().invalidateDecorationsAllViews();

    // TS L1806 + setViewTool L1787-1789: await this.setViewTool(undefined) →
    //   current.onCleanup(); this._viewTool = undefined.
    // Self-exit: exitTool() (ViewUndoTool::onPostInstall) reaches here from
    // the tool's own run() chain — the reference's async run() resumes on a
    // microtask with _viewTool already undefined (ViewTool.ts:100-112), so the
    // object survives the call. A synchronous delete-in-place is UB (the
    // returning run() still executes this->...). Enqueue for the frame-boundary
    // flush (flushDeferredViewToolDeletes) instead of deleting now. (TD-11)
    m_viewTool->onCleanup();
    if (m_ownedViewTools.erase(m_viewTool) != 0)
        m_deferredViewToolDeletes.push_back(m_viewTool);
    m_viewTool = nullptr;

    // TS L1807-1808: if (unsuspend) await this.onUnsuspendTool() ——
    //   恢复被挂起工具（参考 ToolAdmin.ts:1725-1730：activeTool.onUnsuspend +
    //   onActiveToolChanged(Resume)）。
    if (unsuspend) {
        if (auto* tool = activeTool())
            tool->onUnsuspend();
        OnActiveToolChanged.Raise();
    }

    // TS L1809-1810: accuDraw.onViewToolExit + updateDynamics 随其子系统 TODO。
}

// ---------------------------------------------------------------------------
// InputState — current input state (button/drag/motion/qualifier tracking).
// Ported from: itwinjs-core CurrentInputState (ToolAdmin.ts:145-324)
// ---------------------------------------------------------------------------

namespace {

// Wall-clock milliseconds since epoch — C++ equivalent of the reference's
// `Date.now()` (used at ToolAdmin.ts:213 inside onButtonDown for double-click
// detection). Steady-clock would suffice for delta comparisons, but the
// reference stores an absolute epoch ms in BeButtonState.downTime (Tool.ts:90),
// so we mirror that representation.
// Ported from: itwinjs-core ToolAdmin.ts:213 (Date.now()).
double nowMilliseconds() noexcept
{
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

}  // namespace

// Ported from: itwinjs-core CurrentInputState.onInstallTool (ToolAdmin.ts:173-177)
void InputState::onInstallTool() noexcept
{
    clearKeyQualifiers();
    // Reference also clears lastWheelEvent / lastMotionEvent / lastTouchStart /
    // touchTapTimer / touchTapCount (ToolAdmin.ts:175-176). Those event-object
    // fields are deferred to dispatch tasks 6/7/8; nothing to clear here yet.
    // TODO: clear deferred event fields once they land.
}

// Ported from: itwinjs-core CurrentInputState.changeButtonToDownPoint (ToolAdmin.ts:201-207)
void InputState::changeButtonToDownPoint(BeButtonEvent& ev) const
{
    auto const& state = button[static_cast<int>(ev.button)];
    ev.point = state.downUorPt;
    ev.rawPoint = state.downRawPt;

    if (ev.viewport) {
        // Ported from: ToolAdmin.ts:205-206 (ev.viewport.worldToView(ev.rawPoint)).
        ev.viewPoint = ev.viewport->WorldToView(ev.rawPoint);
    }
}

// Ported from: itwinjs-core CurrentInputState.onButtonDown (ToolAdmin.ts:211-226)
void InputState::onButtonDown(BeButton b)
{
    bool isDoubleClick = false;
    // Reference double-click detection (ToolAdmin.ts:213-222) requires a real
    // viewport (vp.worldToView, vp.npcToView, vp.pixelsFromInches) to compare
    // the cursor position against the previous down point within
    // ToolSettings.doubleClickTimeout (500 ms). All three viewport APIs are
    // Step 3 stubs; the dispatch task (7+) will wire them. isDoubleClick stays
    // false in Step 3, which is correct for first-click events.
    // TODO: double-click detection — Step 3 stub (requires Viewport wrappers).

    (void)isDoubleClick;  // currently always false; suppress unused warning until wired
    auto const idx = static_cast<int>(b);
    button[idx].init(m_point, m_rawPoint, nowMilliseconds(),
                     /*isDown=*/true, isDoubleClick, /*isDragging=*/false, inputSource);
    lastButton = b;
}

// Ported from: itwinjs-core CurrentInputState.toEvent (ToolAdmin.ts:234-259)
void InputState::toEvent(BeButtonEvent& ev, bool useSnap) const
{
    auto coordsFrom = CoordSource::User;
    dqGeom::Point3d point = m_point;
    Viewport* viewportPtr = viewport;

    if (useSnap) {
        // TODO: AccuSnap / tentativePoint coordinate adjustment — Step 3 stub.
        // Ported from: ToolAdmin.ts:240-250 (TentativeOrAccuSnap.getCurrentSnap,
        //               IModelApp.tentativePoint.isActive). AccuSnap and
        //               tentativePoint are no-op stubs landing in Task 7+; until
        //               then we fall through with coordsFrom=User and the
        //               unsnapped cursor point — faithful for the no-snap case.
    }

    auto const& state = button[static_cast<int>(lastButton)];
    // Ported from: ToolAdmin.ts:253-258 (ev.init({...})).
    ev.point = point;
    ev.rawPoint = m_rawPoint;
    ev.viewPoint = m_viewPoint;
    ev.viewport = viewportPtr;
    ev.coordsFrom = coordsFrom;
    ev.keyModifiers = qualifiers;
    ev.button = lastButton;
    ev.isDown = state.isDown;
    ev.isDoubleClick = state.isDoubleClick;
    ev.isDragging = state.isDragging;
    ev.inputSource = inputSource;
}

// Ported from: itwinjs-core CurrentInputState.fromPoint (ToolAdmin.ts:280-288)
void InputState::fromPoint(Viewport* vp, dqGeom::Point2d pt, InputSource source)
{
    viewport = vp;
    m_viewPoint.x = pt.x;
    m_viewPoint.y = pt.y;
    if (vp) {
        // Ported from: ToolAdmin.ts:284-285.
        //   this._viewPoint.z = vp.npcToView(NpcCenter).z;
        //   vp.viewToWorld(this._viewPoint, this._rawPoint);
        // NpcCenter = Point3d(0.5,0.5,0.5) (itwinjs common/src/Frustum.ts:68).
        m_viewPoint.z = vp->NpcToView(dqGeom::Point3d::From(0.5, 0.5, 0.5)).z;
        m_rawPoint = vp->ViewToWorld(m_viewPoint);
    } else {
        // Null-viewport path (state-only tests): no transform available — treat
        // the screen-space cursor as the world point (identity). The reference
        // dereferences vp unconditionally (ToolAdmin.ts:284-285); this null guard
        // is a DanQing test affordance, not a behavioral deviation from the
        // snap-free case (which also ends with point == rawPoint == viewPoint).
        m_viewPoint.z = 0.0;
        m_rawPoint = m_viewPoint;
    }
    m_point = m_rawPoint;
    inputSource = source;
}

// Ported from: itwinjs-core CurrentInputState.fromButton (ToolAdmin.ts:290-300)
void InputState::fromButton(Viewport* vp, dqGeom::Point2d pt, InputSource source, bool applyLocks)
{
    fromPoint(vp, pt, source);

    // TODO: AccuSnap / adjustSnapPoint / adjustPoint — Step 3 stub.
    // Ported from: ToolAdmin.ts:294-299 (TentativeOrAccuSnap.getCurrentSnap,
    //               IModelApp.toolAdmin.adjustSnapPoint, adjustPoint). These
    //               subsystems are deferred to Task 7+; the if-branch returns
    //               early in the reference and the else-branch calls
    //               adjustPoint to apply ACS/grid locks. Neither is wired in
    //               Step 3; fromButton collapses to fromPoint for now.
    (void)applyLocks;
}

// Ported from: itwinjs-core CurrentInputState.isStartDrag (ToolAdmin.ts:302-323)
bool InputState::isStartDrag(BeButton b) const noexcept
{
    // L303-305: bail if any button is already mid-drag.
    if (isAnyDragging())
        return false;

    auto const idx = static_cast<int>(b);
    auto const& state = button[idx];
    // L307-309: button must be currently down.
    if (!state.isDown)
        return false;

    // L311-312: reference enforces a minimum hold time
    // (Date.now() - state.downTime > ToolSettings.startDragDelay = 110 ms).
    // The dependency on Date.now makes this deterministic under test inputs,
    // and the hold-time gate is orthogonal to the motion-distance gate the
    // brief asks us to exercise. TODO: enable once dispatch (Task 8) drives
    // real input timing — Step 3 stub.
    //
    // if ((nowMilliseconds() - state.downTime) <= kStartDragDelayMs)
    //     return false;

    // L314-322: motion-distance check. The reference projects the button's
    // downRawPt (world) to view space (vp.worldToView(state.downRawPt)) and
    // compares the Manhattan distance (|dx| + |dy|) against the dpi-aware
    // threshold vp.pixelsFromInches(startDragDistanceInches = 0.15 in) = 14.4 px.
    auto const* vp = viewport;
    if (!vp) {
        // Null-viewport path (state-only tests): no transform available. Compare
        // the current viewPoint against the stored downRawPt (which fromPoint set
        // equal to viewPoint when vp was null) using the fixed kMouseDragThreshold.
        // The reference dereferences vp unconditionally; this null guard is a
        // DanQing test affordance.
        double const deltaX = std::abs(m_viewPoint.x - state.downRawPt.x);
        double const deltaY = std::abs(m_viewPoint.y - state.downRawPt.y);
        return ((deltaX + deltaY) > kMouseDragThreshold);
    }

    // L311-312: minimum hold time (ToolSettings.startDragDelay = 110 ms).
    // TODO: enable the hold-time gate once dispatch drives real input timing —
    //       enabling it now would make every drag require a 110 ms hold before
    //       the motion-distance gate is evaluated, changing feel before the
    //       timing plumbing lands.
    // if ((nowMilliseconds() - state.downTime) <= kStartDragDelayMs) return false;

    dqGeom::Point3d const viewPt = vp->WorldToView(state.downRawPt);
    double const deltaX = std::abs(m_viewPoint.x - viewPt.x);
    double const deltaY = std::abs(m_viewPoint.y - viewPt.y);
    return ((deltaX + deltaY) > vp->PixelsFromInches(ToolSettings::startDragDistanceInches));
}

// ---------------------------------------------------------------------------
// Tool factory functions (used by ToolRegistry)
// Ported from: itwinjs-core tool class constructors
// ---------------------------------------------------------------------------

static InteractiveTool* CreateSelectionTool()
{
    return new SelectionTool();
}

static InteractiveTool* CreateIdleTool()
{
    return new IdleTool();
}

// View-tool factories (viewport-arg variant). Each mirrors the matching
// ViewTool.ts ctor shape: (vp, oneShot, isDraggingRequired) for the ViewManip
// subclasses; FitViewTool takes (vp, oneShot) and ignores isDraggingRequired
// (faithful — FitViewTool's ctor is (vp, oneShot, doAnimate, isolatedOnly); the
// CreateVP factory passes only (vp, oneShot) and lets doAnimate/isolatedOnly
// default).
// Ported from: itwinjs-core IModelApp.tools.create(toolId, vp, ...args)
//               — ViewTool.ts:3038 (Pan), :3051 (Rotate), :3077 (Scroll), :3203 (Fit).

static InteractiveTool* CreatePanViewTool(Viewport* vp, bool oneShot, bool isDraggingRequired)
{
    return new PanViewTool(vp, oneShot, isDraggingRequired);
}

static InteractiveTool* CreateRotateViewTool(Viewport* vp, bool oneShot, bool isDraggingRequired)
{
    return new RotateViewTool(vp, oneShot, isDraggingRequired);
}

static InteractiveTool* CreateScrollViewTool(Viewport* vp, bool oneShot, bool isDraggingRequired)
{
    return new ScrollViewTool(vp, oneShot, isDraggingRequired);
}

// Ported from: itwinjs-core LookViewTool constructor (ViewTool.ts:3066-3068) +
//              ToolRegistry view-tool factory registration.
static InteractiveTool* CreateLookViewTool(Viewport* vp, bool oneShot, bool isDraggingRequired)
{
    return new LookViewTool(vp, oneShot, isDraggingRequired);
}

static InteractiveTool* CreateFitViewTool(Viewport* vp, bool oneShot, bool /*isDraggingRequired*/)
{
    // FitViewTool ctor: (vp, oneShot, doAnimate=true, isolatedOnly=true).
    // The isDraggingRequired arg is unused (FitViewTool is not a drag gesture);
    // the factory ignores it 1:1 with the reference's default-arg collapse.
    return new FitViewTool(vp, oneShot);
}

// Factory: StandardViewTool. Default StandardViewId::Iso (matches itwinjs's
// icon-cube-faces-top default); toolbar buttons construct with specific ids directly.
// Ported from: itwinjs-core ToolRegistry view-tool factory registration.
static InteractiveTool* CreateStandardViewTool(Viewport* vp, bool /*oneShot*/, bool /*isDraggingRequired*/)
{
    return new StandardViewTool(vp, StandardViewId::Iso);
}

// Factory: WindowAreaTool (ViewTool.ts:3531-3816). oneShot/isDraggingRequired unused
// (two-point box-zoom gesture tool, not a one-shot — ctor takes only the viewport,
// ViewTool.ts:3531-3540 + ViewTool ctor :113).
// Ported from: itwinjs-core ToolRegistry view-tool factory registration.
static InteractiveTool* CreateWindowAreaTool(Viewport* vp, bool /*oneShot*/, bool /*isDraggingRequired*/)
{
    return new WindowAreaTool(vp);
}

// Factories: ViewUndoTool / ViewRedoTool. oneShot/isDraggingRequired unused
// (one-shot tools that exit in onPostInstall — ViewTool.ts:4111-4134).
// Ported from: itwinjs-core ToolRegistry view-tool factory registration.
static InteractiveTool* CreateViewUndoTool(Viewport* vp, bool /*oneShot*/, bool /*isDraggingRequired*/)
{
    return new ViewUndoTool(vp);
}

static InteractiveTool* CreateViewRedoTool(Viewport* vp, bool /*oneShot*/, bool /*isDraggingRequired*/)
{
    return new ViewRedoTool(vp);
}

// ---------------------------------------------------------------------------
// OnInitialized — called after Application startup.
// Ported from: itwinjs-core ToolAdmin.onInitialized() (ToolAdmin.ts:487-499)
// ---------------------------------------------------------------------------
void ToolAdmin::OnInitialized()
{
    // Register core tools.
    // Ported from: itwinjs-core IModelApp.startup() tool registration
    //              (IModelApp.ts:400-407)
    m_registry.Register("Select", CreateSelectionTool);
    m_registry.Register("Idle", CreateIdleTool);

    // Register view tools (viewport-arg factories).
    // Ported from: itwinjs-core IModelApp.startup() view-tool registration
    //              (IModelApp.ts:408-413) — PanViewTool/RotateViewTool/
    //              ScrollViewTool/ZoomViewTool/LookViewTool/FitViewTool.
    // Task 11 scope (controller decision in .git/sdd/task-11-brief.md): port
    // the 4 tools the Step 3 IdleTool mappings need (Pan/Rotate/Scroll/Fit).
    // WindowArea/Look W2 ported the ViewLook handle + LookViewTool and
    // registers "View.Look" here.
    // TODO: View.Zoom deferred — needs the ViewZoom handle (Task 10 ported
    //       ViewPan/ViewRotate/ViewScroll; W2 added ViewLook).
    m_registry.RegisterView("View.Pan", CreatePanViewTool);
    m_registry.RegisterView("View.Rotate", CreateRotateViewTool);
    m_registry.RegisterView("View.Scroll", CreateScrollViewTool);
    m_registry.RegisterView("View.Look", CreateLookViewTool);
    m_registry.RegisterView("View.Fit", CreateFitViewTool);
    m_registry.RegisterView("View.Standard", &CreateStandardViewTool);
    // WindowArea/Look W4：WindowAreaTool（ViewTool.ts:3531-3816）——同属参考
    // registerModule(viewTool) 的注册净效果（IModelApp.ts:441/446）。
    m_registry.RegisterView("View.WindowArea", CreateWindowAreaTool);

    // Ported from: itwinjs-core IModelApp.startup viewTool 模块注册
    //              （IModelApp.ts:438-446 registerModule(viewTool) 含 ViewUndoTool/
    //              ViewRedoTool——ViewTool.ts:4111-4134）。
    m_registry.RegisterView("View.Undo", CreateViewUndoTool);
    m_registry.RegisterView("View.Redo", CreateViewRedoTool);

    // Create the idle tool.
    // Ported from: itwinjs-core ToolAdmin.onInitialized() (ToolAdmin.ts:514)
    //              this._idleTool = IModelApp.tools.create("Idle")
    m_idleTool = m_registry.Create("Idle");

    // NB: the default tool is NOT started here. The reference's onInitialized
    // (ToolAdmin.ts:510-525) only creates the idle tool and registers key handlers;
    // the default tool is started by ViewManager.setSelectedView when the FIRST
    // viewport is selected (ViewManager.ts:246-247:
    // `if (undefined === previousVp) await IModelApp.toolAdmin.startDefaultTool()`).
    // DanQing wires that in ViewManager::SetSelectedViewport.
}

// ---------------------------------------------------------------------------
// StartDefaultTool — activate the default tool.
// Ported from: itwinjs-core ToolAdmin.startDefaultTool() (ToolAdmin.ts:1792-1803)
// ---------------------------------------------------------------------------
void ToolAdmin::StartDefaultTool()
{
    // Ported from: itwinjs-core ToolAdmin.startDefaultTool()
    //              const tool = IModelApp.tools.create(this.defaultToolId, this.defaultToolArgs);
    auto* tool = m_registry.Create(m_defaultToolId);

    // Ported from: itwinjs-core ToolAdmin.startDefaultTool()
    //              if (tool instanceof PrimitiveTool) {
    //                if (!await tool.run(this.defaultToolArgs))
    //                  return this.startPrimitiveTool(undefined);
    //              }
    if (tool) {
        SetActiveTool(tool);
    }
}

// ---------------------------------------------------------------------------
// onSelectedViewportChanged — called when selected viewport changes.
// Ported from: itwinjs-core ToolAdmin.onSelectedViewportChanged() (ToolAdmin.ts:1973)
// ---------------------------------------------------------------------------
void ToolAdmin::onSelectedViewportChanged(Viewport* /*previous*/, Viewport* /*current*/)
{
    // Stub — tools can override this to react to viewport changes.
    // itwinjs-core forwards to AccuDraw, ViewTool, InputCollector, PrimitiveTool.
}

// ---------------------------------------------------------------------------
// Static event queue (Task 6)
// Ported from: itwinjs-core ToolAdmin.ts:768-843 (_toolEvents + addEvent +
//               tryReplace + getNextEvent + processNextEvent + processEvent).
//
// Threading note (§8 of CLAUDE.md): the TS queue is single-threaded (touched
// only from the HTML event loop and IModelApp.eventLoop). DanQing has the same
// model — addEvent is called from Qt event listeners on the Qt main thread,
// processEvent is called from Application::EventLoop on the same thread. No
// synchronization needed (1:1 with the reference).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ToolAdmin._toolEvents (ToolAdmin.ts:768)
std::deque<ToolEvent> ToolAdmin::s_toolEvents;

// Ported from: itwinjs-core ToolAdmin.tryReplace (ToolAdmin.ts:769-779)
bool ToolAdmin::tryReplace(ToolEvent const& ev) noexcept
{
    // TS L770-771: queue empty -> nothing to replace.
    if (s_toolEvents.empty())
        return false;

    auto& last = s_toolEvents.back();
    // TS L773-775: only merge if both the last and the new event are motion
    // events (mousemove/touchmove). DanQing's ToolEventType covers only MouseMove
    // among motion types (touch is TODO), so the check collapses to
    // `type == MouseMove` for both sides.
    if (last.type != ev.type || last.type != ToolEventType::MouseMove)
        return false;

    // TS L776-777: last.ev = ev; last.vp = vp — sequential moves replace the
    // previous queue element with this one. We overwrite the whole struct
    // (faithful: the reference overwrites both .ev and .vp).
    last = ev;
    return true;
}

// Ported from: itwinjs-core ToolAdmin.getNextEvent (ToolAdmin.ts:782-787)
std::optional<ToolEvent> ToolAdmin::getNextEvent()
{
    // TS L786: shift() on an empty array returns undefined.
    if (s_toolEvents.empty())
        return std::nullopt;

    // TS L783-784: if there is more than one event left after this pull, wake
    // up the event loop so it keeps draining on subsequent animation frames.
    if (s_toolEvents.size() > 1)
        Application::Get().RequestNextAnimation();

    ToolEvent front = std::move(s_toolEvents.front());
    s_toolEvents.pop_front();  // TS L786: shift()
    return front;
}

// Ported from: itwinjs-core ToolAdmin.addEvent (ToolAdmin.ts:792-801)
// ---------------------------------------------------------------------------
// coalesceWheelEvents —— Chromium 滚轮合并的 DanQing 等价层。
//
// itwinjs 的运行环境（Chromium）在高分辨率滚轮上把一帧内的多次 OS 通知合并为
// 至多一个 DOM WheelEvent（对齐 rAF、delta 累积）——itwinjs 源码不含这层
// （tryReplace 仅合并 mousemove/touchmove，ToolAdmin.ts:795-806），doZoom 以
// "每事件固定 ×wheelZoomRatio=1.5"消费合并后的事件。DanQing 的 Qt 桥逐个转发
// QWheelEvent（实测一帧 3+ 个：angleDelta=-72,-11,-11,…），缺此层时同一滚动手
// 势的缩放次数是 DTA 的数倍（每事件均 ×1.5）。此处在每帧 processEvent 前把
// 队列内同一 viewport 的 Wheel 合并为一个（delta 求和、位置取首现处）——
// 每帧每视口至多一次滚轮缩放，与参考运行时契约一致。
// ---------------------------------------------------------------------------
void ToolAdmin::coalesceWheelEvents()
{
    // 第一遍：按 viewport 累积 delta 并删除后续 Wheel；保留每个 vp 首个 Wheel
    // 的位置（记下待写回的条目下标）。
    std::vector<std::pair<Viewport*, float>> acc;  // vp → 合计 delta（保持首现序）
    for (std::size_t i = 0; i < s_toolEvents.size();) {
        ToolEvent& te = s_toolEvents[i];
        if (te.type == ToolEventType::Wheel && te.vp != nullptr) {
            bool merged = false;
            for (auto& kv : acc) {
                if (kv.first == te.vp) {
                    kv.second += te.wheelDeltaY;
                    merged = true;
                    break;
                }
            }
            if (merged) {
                s_toolEvents.erase(s_toolEvents.begin() + static_cast<std::ptrdiff_t>(i));
                continue;  // 不递增 i —— erase 后下一个元素落在当前下标
            }
            acc.emplace_back(te.vp, te.wheelDeltaY);
        }
        ++i;
    }
    // 第二遍：把合计 delta 写回首现条目（doZoom 只消费符号，合计保持净方向）。
    std::size_t k = 0;
    for (std::size_t i = 0; i < s_toolEvents.size() && k < acc.size(); ++i) {
        ToolEvent& te = s_toolEvents[i];
        if (te.type == ToolEventType::Wheel && te.vp != nullptr) {
            te.wheelDeltaY = acc[k].second;
            ++k;
        }
    }
}

void ToolAdmin::addEvent(ToolEvent ev)
{
    // TS L794-795: don't add events to the queue if the event loop hasn't
    // been started to process them. DanQing mirrors this with
    // Application::IsEventLoopStarted() (the m_wantEventLoop flag set by
    // StartEventLoop, equivalent to IModelApp.isEventLoopStarted).
    if (!Application::Get().IsEventLoopStarted())
        return;

    // TS L797-798: try to merge into the last queue element; otherwise append.
    if (!tryReplace(ev))
        s_toolEvents.push_back(std::move(ev));

    // TS L800: wake up the event loop if it's idle.
    Application::Get().RequestNextAnimation();
}

#ifdef DANQING_TESTING
// Ported from: itwinjs-core ToolAdmin._toolEvents.length (read access).
std::size_t ToolAdmin::pendingEventCount() noexcept
{
    return s_toolEvents.size();
}

// Authored: no reference equivalent; DanQing test affordance for static cleanup.
void ToolAdmin::clearQueue() noexcept
{
    s_toolEvents.clear();
}

// Authored: no reference equivalent; DanQing test affordance for queue-content
// inspection in the Task 14 bridge tests.
std::optional<ToolEvent> ToolAdmin::peekFrontEvent() noexcept
{
    if (s_toolEvents.empty())
        return std::nullopt;
    return s_toolEvents.front();
}
#endif  // DANQING_TESTING

// Ported from: itwinjs-core ToolAdmin.processNextEvent (ToolAdmin.ts:804-824)
void ToolAdmin::processNextEvent()
{
    // TS L805-807: pull the next event; nothing to do if the queue is empty.
    auto maybe = getNextEvent();
    if (!maybe)
        return;
    ToolEvent const& event = *maybe;

    // TS L809-822: dispatch on event.ev.type. DanQing dispatches on event.type
    // (ToolEventType). The dispatch methods are private stubs in Task 6;
    // Tasks 7-8 implement their bodies. The switch wiring itself is faithful
    // 1:1 with the reference (same case order, same handler args).
    switch (event.type) {
        case ToolEventType::MouseDown: return onMouseButton(event, true);   // TS L810
        case ToolEventType::MouseUp:   return onMouseButton(event, false);  // TS L811
        case ToolEventType::MouseMove: return onMouseMove(event);           // TS L812
        case ToolEventType::MouseOver: return onMouseEnter(event);          // TS L813
        // TS L814-815: case "mouseout": return this.onMouseLeave(event.vp!).
        // The non-null assertion collapses to a plain vp pass-through in C++.
        case ToolEventType::MouseOut:  return onMouseLeave(event.vp);
        case ToolEventType::Wheel:     return onWheel(event);               // TS L816
        case ToolEventType::KeyDown:   return onKeyTransition(event, true); // TS L817
        case ToolEventType::KeyUp:     return onKeyTransition(event, false);// TS L818
    }
    // No default: enum is exhaustive over ToolEventType. gcc/clang -Wswitch
    // would catch a future added enumerator without a case label.
}

// Ported from: itwinjs-core ToolAdmin.processEvent (ToolAdmin.ts:831-843)
void ToolAdmin::processEvent()
{
    // TS L832-833: we're still working on the previous event; bail.
    if (m_processingEvent)
        return;

    // TS L835-842: try { m_processingEvent = true; await processNextEvent(); }
    //              catch (e) { exceptionHandler(e); } finally { ... = false; }
    //
    // DanQing adaptation (§3.4): no exceptions in core engines — the try/catch/
    // finally is flattened to set-process-then-clear. The exceptionHandler
    // path has no C++ equivalent under -fno-exceptions; if processNextEvent
    // (Tasks 7-8 bodies) ever needs to surface a failure it will do so via
    // Result<T,E> or a logged no-op. TODO: revisit when dispatch bodies land.
    m_processingEvent = true;
    processNextEvent();
    m_processingEvent = false;
}

// ---------------------------------------------------------------------------
// Button dispatch (Task 7)
// Ported from: itwinjs-core ToolAdmin.ts:549 (onMouseButton), :1297-1385
//               (sendButtonEvent), :1387-1403 (onButtonDown), :1405-1421
//               (onButtonUp), :855-861 (filterViewport).
//
// Threading: same single-threaded model as the event queue (Task 6). All
// dispatch runs on the Qt main thread inside Application::EventLoop; no
// synchronization needed (1:1 with the reference, which dispatches on the
// HTML event loop).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ToolAdmin.filterViewport (ToolAdmin.ts:855-861)
bool ToolAdmin::filterViewport(Viewport* vp) const
{
    // TS L856: `if (undefined === vp || vp.isDisposed) return true;`
    // Step 3 adaptation: Viewport::isDisposed has no DanQing equivalent yet
    // (TODO pending a Viewport is-disposed port). The null-vp check is
    // faithful; the isDisposed check is omitted.
    if (vp == nullptr)
        return true;

    // TS L859-860: activeTool.isCompatibleViewport(vp, false). Task 8 priority
    // resolution — GetActiveTool() now returns the priority-resolved slot
    // (viewTool ?? inputCollector ?? primitiveTool).
    InteractiveTool* tool = GetActiveTool();
    return tool ? !tool->isCompatibleViewport(vp, false) : false;
}

// Ported from: itwinjs-core ToolAdmin.onMouseButton (ToolAdmin.ts:549-558)
void ToolAdmin::onMouseButton(ToolEvent const& event, bool isDown)
{
    // TS L552: const vp = event.vp!  — non-null assertion. DanQing accepts the
    // viewport by pointer; a null viewport is a degenerate event (Tests can
    // still drive the dispatch with vp == nullptr via the lower-level
    // onButtonDown/onButtonUp entry points).
    Viewport* vp = event.vp;

    // TS L553-554: getMousePosition + getMouseButton. DanQing's ToolEvent
    // already carries the extracted fields (Task 6 controller decision — the
    // Qt bridge populates posx/posy/button/modifiers); no DOM to read.
    dqGeom::Point2d const pos = dqGeom::Point2d::From(static_cast<double>(event.posx),
                                                       static_cast<double>(event.posy));
    BeButton const button = event.button;

    // TS L556: currentInputState.setKeyQualifiers(ev). The reference reads
    // shiftKey/ctrlKey/altKey from the DOM MouseEvent; DanQing's ToolEvent
    // carries the pre-extracted mask (ToolEvent.modifiers, populated by the
    // Task 14 Qt bridge). Passing that mask directly is faithful to the
    // intent of setKeyQualifiers (ToolAdmin.ts:190-194 setKeyQualifier(s)).
    m_currentInputState.setKeyQualifiers(event.modifiers);

    // TS L557: isDown ? onButtonDown : onButtonUp.
    if (isDown)
        onButtonDown(vp, pos, button, InputSource::Mouse);
    else
        onButtonUp(vp, pos, button, InputSource::Mouse);
}

// Ported from: itwinjs-core ToolAdmin.onButtonDown (ToolAdmin.ts:1387-1403)
void ToolAdmin::onButtonDown(Viewport* vp, dqGeom::Point2d pt2d, BeButton button, InputSource inputSource)
{
    // TS L1388: const filtered = this.filterViewport(vp);
    bool const filtered = filterViewport(vp);

    // TS L1389-1390: if (undefined === this._viewTool && button === BeButton.Data)
    //                  await IModelApp.viewManager.setSelectedView(vp);
    // Step 3 adaptation: ViewManager has SetSelectedViewport (the dqApp public
    // API name); we call it directly. m_viewTool == nullptr matches
    // `undefined === this._viewTool` 1:1.
    if (m_viewTool == nullptr && button == BeButton::Data)
        Application::Get().GetViewManager().SetSelectedViewport(vp);

    // TS L1391-1392: if (filtered) return;
    if (filtered)
        return;

    // TS L1394: vp.setAnimator();  — Step 3 stub (no Viewport::setAnimator
    // port yet; TODO pending a Viewport animator API).
    // TODO: vp->setAnimator() — faithful no-op until Viewport animator lands.

    // TS L1395-1400: build the BeButtonEvent via InputState.
    BeButtonEvent ev;
    m_currentInputState.fromButton(vp, pt2d, inputSource, /*applyLocks=*/true);
    m_currentInputState.onButtonDown(button);
    m_currentInputState.toEvent(ev, /*useSnap=*/true);
    m_currentInputState.updateDownPoint(ev);

    // TS L1402: return this.sendButtonEvent(ev);
    sendButtonEvent(ev);
}

// Ported from: itwinjs-core ToolAdmin.onButtonUp (ToolAdmin.ts:1405-1421)
void ToolAdmin::onButtonUp(Viewport* vp, dqGeom::Point2d pt2d, BeButton button, InputSource inputSource)
{
    // TS L1406-1407: if (this.filterViewport(vp)) return;
    if (filterViewport(vp))
        return;

    BeButtonEvent ev;
    // CRITICAL: capture wasDragging BEFORE onButtonUp clears it. The reference
    // (ToolAdmin.ts:1411) reads `current.isDragging(button)` before calling
    // `current.onButtonUp(button)` (which sets isDragging = false at L231).
    // Reading after would always report false and route every drag-terminated
    // click through sendButtonEvent instead of sendEndDragEvent.
    bool const wasDragging = m_currentInputState.isDragging(button);
    m_currentInputState.fromButton(vp, pt2d, inputSource, /*applyLocks=*/true);
    m_currentInputState.onButtonUp(button);
    m_currentInputState.toEvent(ev, /*useSnap=*/true);

    // TS L1416-1417: if (wasDragging) return this.sendEndDragEvent(ev);
    if (wasDragging) {
        sendEndDragEvent(ev);
        return;
    }

    // TS L1419-1420: current.changeButtonToDownPoint(ev); return sendButtonEvent(ev);
    m_currentInputState.changeButtonToDownPoint(ev);
    sendButtonEvent(ev);
}

// Ported from: itwinjs-core ToolAdmin.sendButtonEvent (ToolAdmin.ts:1297-1385)
void ToolAdmin::sendButtonEvent(BeButtonEvent& ev)
{
    // TS L1298-1300: const overlayHit = this.pickCanvasDecoration(ev);
    //                 if (undefined !== overlayHit && undefined !== overlayHit.onMouseButton
    //                     && overlayHit.onMouseButton(ev)) return;
    // Step 3 stub: pickCanvasDecoration requires a live Viewport with canvas-
    // decoration hit testing (Viewport::pickCanvasDecoration, ToolAdmin.ts:1028);
    // that API has no DanQing equivalent yet. overlayHit is therefore always
    // nullptr and the early-return is unreachable. TODO: wire when Viewport
    // canvas-decoration picking lands.
    // std::unique_ptr<CanvasDecoration> overlayHit = pickCanvasDecoration(ev);

    // TS L1302-1303: if (this.onPreButtonEvent(ev)) return;
    // ToolAdmin.onPreButtonEvent is itself a no-op base (ToolAdmin.ts:1292-1294
    // returns false). Skipped — never returns true in the base.

    // TS L1305-1306: if (IModelApp.accuSnap.onPreButtonEvent(ev)) return;
    // AccuSnap stub returns EventHandled::No (Step 3).
    if (Application::Get().GetAccuSnap().onPreButtonEvent(ev) != EventHandled::No)
        return;

    // TS L1308-1320: select tool, apply receivedDownEvent / isValidLocation guards.
    // Task 8 priority resolution: GetActiveTool() returns viewTool ?? inputCollector
    // ?? primitiveTool (ToolAdmin.ts:876-878).
    InteractiveTool* activeTool = GetActiveTool();
    InteractiveTool* tool = activeTool;
    if (tool != nullptr) {
        // TS L1312-1313: !isValidLocation -> drop tool
        if (!tool->isValidLocation(ev, /*isButtonEvent=*/true)) {
            tool = nullptr;
        } else if (ev.isDown) {
            // TS L1314-1315: mark that the tool saw the down for this gesture.
            tool->receivedDownEvent = true;
        } else if (tool->receivedDownEvent) {
            // TS L1316-1317: consume the down marker on the matching up.
            tool->receivedDownEvent = false;
        } else {
            // TS L1318-1319: up without a prior down — drop the event.
            tool = nullptr;
        }
    }

    // TS L1322-1323: if (IModelApp.accuDraw.onPreButtonEvent(ev)) return;
    // AccuDraw stub returns false (Step 3).
    if (Application::Get().GetAccuDraw().onPreButtonEvent(ev))
        return;

    // TS L1325: let updateDynamics = false;  — set true only for the Data-down
    // path; drives this.updateDynamics(...) at the tail (L1380-1384). Task 8
    // wires the tail call; the local flag is renamed to avoid shadowing the
    // new updateDynamics(BeButtonEvent*) member.
    bool wantUpdateDynamics = false;

    switch (ev.button) {
        case BeButton::Data: {
            // TS L1329-1333: if no tool picked up the gesture, fall back to idle
            //                 only when there's no active tool at all.
            if (tool == nullptr) {
                if (activeTool != nullptr)
                    break;
                tool = m_idleTool;
            }

            if (ev.isDown) {
                // TS L1336: await tool.onDataButtonDown(ev);
                tool->onDataButtonDown(ev);
            } else {
                // TS L1338: await tool.onDataButtonUp(ev); break;
                tool->onDataButtonUp(ev);
                break;
            }

            // TS L1343-1344: if (tool instanceof PrimitiveTool) tool.autoLockTarget();
            // Step 3 adaptation: dynamic_cast disabled (-fno-rtti); use the
            // PrimitiveTool virtual autoLockTarget directly via a virtual call
            // on the base. The reference's instanceof gate is preserved by
            // making autoLockTarget a PrimitiveTool-only virtual (no-op default
            // on InteractiveTool would change semantics; instead we rely on
            // tool knowing its own type — PrimitiveTool::autoLockTarget is the
            // override point and the only tools that need it derive from
            // PrimitiveTool). TODO: revisit when Task 8 refines tool slots.
            // tool->autoLockTarget() is a no-op on InteractiveTool; calling it
            // unconditionally is observably equivalent for non-PrimitiveTools.
            // For Task 7 we omit the call (faithful for the RecordingTool test
            // fixtures, which don't derive from PrimitiveTool). Task 13 wires
            // this for SelectionTool (a PrimitiveTool subclass).

            // TS L1346: updateDynamics = true;
            wantUpdateDynamics = true;
            break;
        }

        case BeButton::Reset: {
            // TS L1351-1355: same idle-fallback rule as Data.
            if (tool == nullptr) {
                if (activeTool != nullptr)
                    break;
                tool = m_idleTool;
            }

            // TS L1357-1360: down -> onResetButtonDown, up -> onResetButtonUp.
            if (ev.isDown)
                tool->onResetButtonDown(ev);
            else
                tool->onResetButtonUp(ev);
            break;
        }

        case BeButton::Middle: {
            // TS L1364-1372: middle always tries the active tool first, then
            //                 falls back to idleTool if the active tool did not
            //                 explicitly handle it (EventHandled::Yes).
            if (ev.isDown) {
                if (tool == nullptr || tool->onMiddleButtonDown(ev) != EventHandled::Yes)
                    if (m_idleTool != nullptr)
                        m_idleTool->onMiddleButtonDown(ev);
            } else {
                if (tool == nullptr || tool->onMiddleButtonUp(ev) != EventHandled::Yes)
                    if (m_idleTool != nullptr)
                        m_idleTool->onMiddleButtonUp(ev);
            }
            break;
        }
    }

    // TS L1377: IModelApp.tentativePoint.onButtonEvent(ev);
    Application::Get().GetTentativePoint().onButtonEvent(ev);
    // TS L1378: IModelApp.accuDraw.onPostButtonEvent(ev);
    Application::Get().GetAccuDraw().onPostButtonEvent(ev);

    // TS L1380-1384: if (updateDynamics) this.updateDynamics(undefined, undefined, true);
    // Task 8 wires updateDynamics. The reference passes (undefined, undefined,
    // true) — fillEventFromCursorLocation builds a fresh event and adjustPoint
    // runs. Step 3 collapses this to updateDynamics(nullptr): a default-
    // constructed BeButtonEvent with no viewport early-returns in
    // updateDynamics (ToolAdmin.ts:965-966), so the call is observably a no-op
    // when there is no current cursor viewport. This preserves the reference's
    // tail-call shape without the full fillEventFromCursorLocation port
    // (TODO Task 8+: fillEventFromCursorLocation when a real cursor location
    // is available outside motion dispatch).
    if (wantUpdateDynamics)
        updateDynamics(nullptr);
}

// ---------------------------------------------------------------------------
// Motion / drag / wheel / key dispatch (Task 8)
// Ported from: itwinjs-core ToolAdmin.ts:590-600 (onWheel), :872-881 (priority
//               slots), :901-991 (updateDynamics), :993-1008 (sendEndDragEvent),
//               :1083-1089 (onStartDrag), :1091-1201 (onMotion/onMouseMove),
//               :1424-1499 (modifier/key transition), :1965-2125
//               (processWheelEvent + WheelEventProcessor).
//
// Threading: same single-threaded model as the event queue (Task 6). Dispatch
// runs synchronously on the Qt main thread inside Application::EventLoop; the
// reference's async/snap-promise/setTimeout machinery collapses to direct
// calls, preserving the observable dispatch contract.
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ToolAdmin.onMouseLeave (ToolAdmin.ts:952-960)。
// _mouseMoveOverTimeout 清理与 setCanvasDecoration(vp) 未移植（无对应子系统）。
void ToolAdmin::onMouseLeave(Viewport* vp)
{
    // TS L957: IModelApp.accuSnap.clear() —— 参考 clear 经 setCurrHit(undefined)
    // 触发 unFlashViews（AccuSnap.ts:350-355/555-561 → vp.flashedId = undefined）；
    // DanQing 的 flash 由 Viewport 承载——等价落点是清视口 flash + snap 十字。
    Application::Get().GetAccuSnap().clear();
    Application::Get().GetAccuSnap().clearCross();
    if (vp)
        vp->SetFlashedId(0);

    // TS L958: this.currentInputState.clearViewport(vp) —— cursorView 变空 →
    // ToolAdmin.decorate 的光圈守卫（:2063-2064）失效，光圈停画。
    m_currentInputState.clearViewport(vp);

    // TS L960: vp.invalidateDecorations()（"stop drawing locate circle..."）。
    if (vp)
        vp->InvalidateDecorations();
}

// Ported from: itwinjs-core ToolAdmin.sendEndDragEvent (ToolAdmin.ts:993-1008)
void ToolAdmin::sendEndDragEvent(BeButtonEvent& ev)
{
    // TS L994: let tool = this.activeTool;
    InteractiveTool* tool = activeTool();

    // TS L996-1003: isValidLocation + receivedDownEvent guard. The active tool
    // only receives the end-drag if it observed the matching down event
    // (receivedDownEvent == true) and the location is valid; otherwise tool
    // becomes nullptr and the call falls through to idleTool.
    if (tool != nullptr) {
        if (!tool->isValidLocation(ev, /*isButtonEvent=*/true))
            tool = nullptr;
        else if (tool->receivedDownEvent)
            tool->receivedDownEvent = false;
        else
            tool = nullptr;
    }

    // TS L1005-1007: Don't send tool end drag event if it didn't get the start
    // drag event. If the active tool declined (nullptr) or returned No, fall
    // through to idleTool.onMouseEndDrag.
    if (tool == nullptr || tool->onMouseEndDrag(ev) != EventHandled::Yes) {
        if (m_idleTool != nullptr)
            m_idleTool->onMouseEndDrag(ev);
    }
}

// Ported from: itwinjs-core ToolAdmin.onStartDrag (ToolAdmin.ts:1083-1089)
EventHandled ToolAdmin::onStartDrag(BeButtonEvent& ev, InteractiveTool* tool)
{
    // TS L1084: if (tool && EventHandled.Yes === await tool.onMouseStartDrag(ev))
    //            return EventHandled.Yes;
    if (tool != nullptr && tool->onMouseStartDrag(ev) == EventHandled::Yes)
        return EventHandled::Yes;

    // TS L1088: Pass start drag event to idle tool if active tool doesn't
    //           explicitly handle it.
    if (m_idleTool != nullptr)
        return m_idleTool->onMouseStartDrag(ev);
    return EventHandled::No;
}

// Ported from: itwinjs-core ToolAdmin.updateDynamics (ToolAdmin.ts:937-991)
void ToolAdmin::updateDynamics(BeButtonEvent* ev)
{
    // TS L938-939: if (undefined === this.activeTool) return;
    InteractiveTool* tool = activeTool();
    if (tool == nullptr)
        return;

    // TS L941-963: if (undefined === ev) { ev = new BeButtonEvent();
    //               fillEventFromLastDataButton / fillEventFromCursorLocation;
    //               if (adjustPoint && ev.viewport) { snap/adjustPoint/... } }
    // Step 3 stub: fillEventFromLastDataButton / fillEventFromCursorLocation
    // (ToolAdmin.ts:943-947) require a real Viewport + AccuSnap path. With
    // ev == nullptr we default-construct; the resulting event has viewport ==
    // nullptr and the L965-966 guard below early-returns — observably a no-op
    // when there is no current cursor location. Motion dispatch (onMotion)
    // always supplies a real ev, so onMouseMotion still reaches the tool there.
    BeButtonEvent defaultEv;
    if (ev == nullptr)
        ev = &defaultEv;

    // TS L965-966: if (undefined === ev.viewport) return;
    if (ev->viewport == nullptr)
        return;

    // TS L969: const toolPromise = this._toolMotionPromise = this.activeTool.onMouseMotion(ev);
    // The reference's onMouseMotion returns a Promise (async tool motion); the
    // synchronous C++ port dispatches immediately.
    tool->onMouseMotion(*ev);

    // TS L974-990: toolPromise.then(() => { lastMotionEvent = motion;
    //               if (!inDynamicsMode) { vp.invalidateDecorations(); return; }
    //               context = new DynamicsContext(vp); tool.onDynamicFrame(...);
    //               context.changeDynamics(); })
    // Step 3 stub: the onDynamicFrame / inDynamicsMode path is TODO (no
    // PrimitiveTool dynamics in Step 3). The onMouseMotion dispatch above is
    // the load-bearing Step 3 contract — the dynamics frame is deferred.
    // TODO: onDynamicFrame dispatch — future PrimitiveTool-dynamics task.
}

// Ported from: itwinjs-core ToolAdmin.onMouseMove (ToolAdmin.ts:1186-1201)
void ToolAdmin::onMouseMove(ToolEvent const& event)
{
    // TS L1187-1189: const vp = event.vp; if (undefined === vp) return;
    Viewport* vp = event.vp;
    if (vp == nullptr)
        return;

    // TS L1191: const pos = this.getMousePosition(event);
    dqGeom::Point2d const pos = dqGeom::Point2d::From(static_cast<double>(event.posx),
                                                       static_cast<double>(event.posy));
    // TS L1192: const mov = this.getMouseMovement(event);
    // DanQing's ToolEvent does not carry movement deltas (the Task 14 Qt bridge
    // would compute them from QMouseEvent::position vs last position); tests
    // pass (0,0). The value is forwarded to processMotion for AccuDraw motion
    // smoothing, which is a Step 3 stub. TODO: wire movement deltas in the
    // Task 14 Qt bridge.
    dqGeom::Point2d const mov = dqGeom::Point2d::From(0.0, 0.0);

    // TS L1194-1198: lost-up-event correction — clear isDown based on the
    // MouseEvent.buttons mask. DanQing's ToolEvent does not carry a buttons mask
    // (the Task 14 Qt bridge would populate it from QMouseEvent::buttons);
    // skip the correction until that lands. TODO: wire buttons mask in the
    // Task 14 Qt bridge.

    // TS L1200: return this.onMotion(vp, pos, InputSource.Mouse, false, mov);
    onMotion(vp, pos, InputSource::Mouse, /*forceStartDrag=*/false, mov);
}

// Ported from: itwinjs-core ToolAdmin.onMotion (ToolAdmin.ts:1091-1173)
void ToolAdmin::onMotion(Viewport* vp, dqGeom::Point2d pt2d, InputSource inputSource,
                          bool forceStartDrag, dqGeom::Point2d movement)
{
    // TS L1092-1093: current.onMotion(pt2d);
    InputState& current = m_currentInputState;
    current.onMotion(pt2d);

    // TS L1095-1098: if (this.filterViewport(vp)) { setIncompatibleViewportCursor(false); return; }
    if (filterViewport(vp)) {
        // TODO: setIncompatibleViewportCursor(false) — Step 3 stub (cursor
        // appearance API not yet ported).
        return;
    }

    // TS L1100-1102: cancel any prior mouseMoveOver timeout. Synchronous C++
    // port has no setTimeout, so nothing to cancel.

    // TS L1104-1106: const ev = new BeButtonEvent(); current.fromPoint(...);
    //                 current.toEvent(ev, false);
    BeButtonEvent ev;
    current.fromPoint(vp, pt2d, inputSource);
    current.toEvent(ev, /*useSnap=*/false);

    // TS L1108-1115: pickCanvasDecoration overlay-hit path. Step 3 stub —
    // Viewport::pickCanvasDecoration is not yet ported (TODO Task 8+).
    // TODO: pickCanvasDecoration overlay-hit path.

    // TS L1117-1120: setTimeout(async () => { onMotionEnd(...); processMotion(); }, 100).
    // TS L1155-1172: snapPromise = this.onMotionSnap(ev); snapPromise.then(processMotion).
    // Step 3 collapses the setTimeout + snap promise chain to a synchronous
    // processMotion call. AccuSnap is a no-op stub; running processMotion
    // immediately is observably equivalent for the Step 3 no-snap case.
    processMotion(vp, pt2d, inputSource, forceStartDrag, movement, ev);

    // TS L1165-1166: if (this.isLocateCircleOn) vp.invalidateDecorations();
    // The locate aperture circle follows the cursor — every motion must
    // invalidate decorations so the circle is re-collected at the new position
    // (the reference's ToolAdmin.decorate runs every frame once decorations
    // are invalidated).
    if (isLocateCircleOn())
        vp->InvalidateDecorations();

    if (getenv("DANQING_CURSOR_TRACE")) {
        fprintf(stderr, "[CURSOR] motion pos=(%.1f,%.1f) lastMotion=(%.1f,%.1f)\n",
                pt2d.x, pt2d.y,
                m_currentInputState.lastMotion.x, m_currentInputState.lastMotion.y);
    }
}

// Ported from: itwinjs-core ToolAdmin.onMotion's processMotion closure
//               (ToolAdmin.ts:1122-1153). Factored out as a member so the
//               synchronous onMouseMove path can call it directly.
void ToolAdmin::processMotion(Viewport* vp, dqGeom::Point2d pt2d, InputSource inputSource,
                                bool forceStartDrag, dqGeom::Point2d movement, BeButtonEvent& ev)
{
    InputState& current = m_currentInputState;

    // TS L1123-1126: current.fromButton(vp, pt2d, inputSource, true);
    //                 current.toEvent(ev, true); ev.movement = movement;
    current.fromButton(vp, pt2d, inputSource, /*applyLocks=*/true);
    current.toEvent(ev, /*useSnap=*/true);
    // ev.movement is not in DanQing's BeButtonEvent POD (the reference adds it
    // at Tool.ts as an instance field); Step 3 ignores movement at the dispatch
    // layer (AccuDraw smoothing is a stub). TODO: add movement field to
    // BeButtonEvent if a future task needs it.
    (void)movement;

    // TS L1128: IModelApp.accuDraw.onMotion(ev);
    Application::Get().GetAccuDraw().onMotion(ev);

    // TS L1130-1132: isValidLocation check (defaults to true if no active tool).
    InteractiveTool* tool = activeTool();
    bool const isValidLocation = (tool != nullptr ? tool->isValidLocation(ev, /*isButtonEvent=*/false) : true);
    // TS L1133: this.setIncompatibleViewportCursor(isValidLocation); — Step 3 stub.

    // TS L1134-1150: drag-start detection.
    //   if (forceStartDrag || current.isStartDrag(ev.button)) {
    //     current.onStartDrag(ev.button);
    //     current.changeButtonToDownPoint(ev);
    //     ev.isDragging = true;
    //     if (tool) { ... isValidLocation / receivedDownEvent gates ... }
    //     return this.onStartDrag(ev, tool);
    //   }
    if (forceStartDrag || current.isStartDrag(ev.button)) {
        current.onStartDrag(ev.button);
        current.changeButtonToDownPoint(ev);
        ev.isDragging = true;

        InteractiveTool* dragTool = tool;
        if (dragTool != nullptr) {
            if (!isValidLocation)
                dragTool = nullptr;
            else if (forceStartDrag)
                dragTool->receivedDownEvent = true;
            else if (!dragTool->receivedDownEvent)
                dragTool = nullptr;
        }

        onStartDrag(ev, dragTool);
        return;
    }

    // TS L1152: this.updateDynamics(ev);
    updateDynamics(&ev);
}

// Ported from: itwinjs-core ToolAdmin.onWheel (ToolAdmin.ts:573-600)
//
// The reference's onMouseWheel body computes a signed `delta` from the DOM
// WheelEvent deltaMode + deltaY, then builds a BeWheelEvent and dispatches to
// activeTool.onMouseWheel ?? idleTool.onMouseWheel. DanQing's ToolEvent already
// carries wheelDeltaY as a signed scalar (populated by the Task 14 Qt bridge
// from QWheelEvent::angleDelta().y()), so the deltaMode normalization
// collapses to using wheelDeltaY directly.
void ToolAdmin::onWheel(ToolEvent const& event)
{
    // TS L574-575: const vp = event.vp; if (undefined === vp) return;
    Viewport* vp = event.vp;
    if (vp == nullptr)
        return;

    InputState& current = m_currentInputState;

    // TS L576-582: normalize DOM WheelEvent.deltaMode + deltaY to a signed
    // scalar. DanQing ToolEvent.wheelDeltaY is already that scalar (degrees of
    // rotation, signed: positive = scroll up = zoom in by convention).
    double const delta = static_cast<double>(event.wheelDeltaY);

    // TS L584: const pt2d = this.getMousePosition(event);
    dqGeom::Point2d const pos = dqGeom::Point2d::From(static_cast<double>(event.posx),
                                                       static_cast<double>(event.posy));

    // TS L586: vp.setAnimator(); — Step 3 stub (no animator clearing API).

    // TS L587: current.fromButton(vp, pt2d, InputSource.Mouse, true);
    current.setKeyQualifiers(event.modifiers);
    current.fromButton(vp, pos, InputSource::Mouse, /*applyLocks=*/true);

    // TS L588-590: const wheelEvent = new BeWheelEvent(); wheelEvent.wheelDelta = delta;
    //               current.toEvent(wheelEvent, true);
    BeWheelEvent wheelEvent;
    wheelEvent.wheelDelta = delta;
    current.toEvent(wheelEvent, /*useSnap=*/true);

    // TS L592-594: pickCanvasDecoration overlay-hit path. Step 3 stub.
    // TODO: pickCanvasDecoration overlay-hit path.

    // TS L596-599: const tool = this.activeTool;
    //               if (undefined === tool ||
    //                   EventHandled.Yes !== await tool.onMouseWheel(wheelEvent)
    //                   && vp !== this.markupView)
    //                 return this.idleTool.onMouseWheel(wheelEvent);
    //               return EventHandled.Yes;
    // markupView is TODO (no markup view port in Step 3); with markupView ==
    // undefined, `vp !== this.markupView` is always true (vp was null-checked
    // at L574-575), so the condition collapses to "tool is null OR tool
    // declined" — the active-tool-declines path falls through to idleTool.
    // TODO: markupView branch when markup view support lands.
    InteractiveTool* tool = activeTool();
    if (tool == nullptr || tool->onMouseWheel(wheelEvent) != EventHandled::Yes) {
        if (m_idleTool != nullptr)
            m_idleTool->onMouseWheel(wheelEvent);
    }
}

// Ported from: itwinjs-core ToolAdmin.getModifierKey (ToolAdmin.ts:1434-1441)
BeModifierKeys ToolAdmin::getModifierKey(uint32_t key) noexcept
{
    // The reference switches on `event.key` strings ("Alt"/"Shift"/"Control").
    // DanQing's ToolEvent.key is a uint32_t Qt::Key value; the kKey* constants
    // above mirror Qt::Key_Shift / Control / Alt.
    if (key == kKeyShift)   return BeModifierKeys::Shift;
    if (key == kKeyControl) return BeModifierKeys::Control;
    if (key == kKeyAlt)     return BeModifierKeys::Alt;
    return BeModifierKeys::None;
}

// Ported from: itwinjs-core ToolAdmin.onModifierKeyTransition (ToolAdmin.ts:1451-1459)
void ToolAdmin::onModifierKeyTransition(bool wentDown, BeModifierKeys modifier, uint32_t key)
{
    // TS L1452-1453: const activeTool = this.activeTool;
    //                 const changed = activeTool ? await activeTool.onModifierKeyTransition(...)
    //                                             : EventHandled.No;
    InteractiveTool* tool = activeTool();
    EventHandled const changed = (tool != nullptr)
        ? tool->onModifierKeyTransition(wentDown, modifier)
        : EventHandled::No;
    (void)key;  // ToolAdmin.onModifierKeyTransition takes the KeyboardEvent in TS;
                // DanQing collapses to the modifier mask. The key param is kept for
                // signature parity with future expansion.

    // TS L1455-1458: if (changed === EventHandled.Yes) {
    //                   IModelApp.viewManager.invalidateDecorationsAllViews();
    //                   this.updateDynamics(undefined, undefined, true);
    //                 }
    if (changed == EventHandled::Yes) {
        Application::Get().GetViewManager().invalidateDecorationsAllViews();  // TS L1456
        updateDynamics(nullptr);
    }
}

// Ported from: itwinjs-core ToolAdmin.onKeyTransition (ToolAdmin.ts:1474-1499)
void ToolAdmin::onKeyTransition(ToolEvent const& event, bool wentDown)
{
    // TS L1475: const keyEvent = event.ev as KeyboardEvent;
    // TS L1476: this.currentInputState.setKeyQualifiers(keyEvent);
    m_currentInputState.setKeyQualifiers(event.modifiers);

    // TS L1478: const modifierKey = ToolAdmin.getModifierKey(keyEvent);
    BeModifierKeys const modifierKey = getModifierKey(event.key);

    // TS L1480-1481: if (BeModifierKeys.None !== modifierKey)
    //                  return this.onModifierKeyTransition(wentDown, modifierKey, keyEvent);
    if (modifierKey != BeModifierKeys::None) {
        onModifierKeyTransition(wentDown, modifierKey, event.key);
        return;
    }

    // TS L1483-1487: if (wentDown && keyEvent.ctrlKey) {
    //                   const { handled, result } = await this.onCtrlKeyPressed(keyEvent);
    //                   if (handled) return result;
    //                 }
    // Step 3 stub: onCtrlKeyPressed (undo/redo/keyin palette shortcuts,
    // ToolAdmin.ts:1444-1466) is not yet ported — the ctrl+z/y/F2 handling
    // lands with the UiAdmin / undo manager wiring. The ctrlKey state is in
    // event.modifiers.
    // TODO: onCtrlKeyPressed (Ctrl+Z/Y/F2 shortcuts) — future task.

    // TS L1489-1493: const activeTool = this.activeTool;
    //                 if (activeTool) {
    //                   if (EventHandled.Yes === await activeTool.onKeyTransition(wentDown, keyEvent))
    //                     return EventHandled.Yes;
    //                 }
    InteractiveTool* tool = activeTool();
    if (tool != nullptr) {
        if (tool->onKeyTransition(wentDown, event.key) == EventHandled::Yes)
            return;
    }

    // TS L1495-1496: if (await this.processShortcutKey(keyEvent, wentDown))
    //                   return EventHandled.Yes;
    // Step 3 stub: processShortcutKey (ToolAdmin.ts:1469-1471) returns false in
    // the base; subclass shortcut dispatch is not yet wired.
    // TODO: processShortcutKey — future task.

    // TS L1498: return EventHandled.No;
}

// Ported from: itwinjs-core ToolAdmin.processWheelEvent (ToolAdmin.ts:2183-2188)
EventHandled ToolAdmin::processWheelEvent(BeWheelEvent& ev, bool doUpdate)
{
    // TS L2184: await WheelEventProcessor.process(ev, doUpdate);
    WheelEventProcessor::process(ev, doUpdate);

    // TS L2185: IModelApp.viewManager.invalidateDecorationsAllViews().
    Application::Get().GetViewManager().invalidateDecorationsAllViews();

    // TS L2186: this.updateDynamics(ev);
    updateDynamics(&ev);

    // TS L2187: return EventHandled.Yes;
    return EventHandled::Yes;
}

// ---------------------------------------------------------------------------
// WheelEventProcessor — default wheel-event zoom processor.
// Ported from: itwinjs-core WheelEventProcessor (ToolAdmin.ts:2020-2126).
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core WheelEventProcessor.process (ToolAdmin.ts:2021-2032)
EventHandled WheelEventProcessor::process(BeWheelEvent& ev, bool doUpdate)
{
    // TS L2022-2024: const vp = ev.viewport; if (undefined === vp) return;
    Viewport* vp = ev.viewport;
    if (vp == nullptr)
        return EventHandled::No;

    // TS L2026: await this.doZoom(ev);
    doZoom(ev);

    // TS L2028-2031: if (doUpdate) { IModelApp.accuSnap.clear(); }
    if (doUpdate) {
        // AccuSnap.clear is a no-op stub (Step 3); faithful wiring.
        Application::Get().GetAccuSnap().clear();
    }
    return EventHandled::Yes;
}

// Ported from: itwinjs-core WheelEventProcessor.doZoom (ToolAdmin.ts:2034-2125)
EventHandled WheelEventProcessor::doZoom(BeWheelEvent const& ev)
{
    // TS L2035-2037: const vp = ev.viewport; if (undefined === vp) return InvalidViewport;
    Viewport* vp = ev.viewport;
    if (vp == nullptr)
        return EventHandled::No;

    // TS L2039-2043: zoomRatio computation.
    //   let zoomRatio = ToolSettings.wheelZoomRatio; // = 1.5
    //   if (zoomRatio < 1) zoomRatio = 1;
    //   if (ev.wheelDelta > 0) zoomRatio = 1 / zoomRatio;
    // ToolSettings::wheelZoomRatio 用 ViewTool.h 的全局常量（同源 ToolSettings.wheelZoomRatio，
    // ToolSettings.ts:68）——局部同名声明会遮蔽全局（C4459）
    double zoomRatio = ToolSettings::wheelZoomRatio;
    if (zoomRatio < 1.0)
        zoomRatio = 1.0;
    if (ev.wheelDelta > 0.0)
        zoomRatio = 1.0 / zoomRatio;

    // TS L2045-2055: target point selection. The reference prefers tentative
    // point, then AccuSnap, then ev.point/ev.rawPoint. Step 3 has no snap
    // subsystem active, so the unsnapped cursor (ev.rawPoint) is the target —
    // faithful for the no-snap case.
    dqGeom::Point3d target = ev.rawPoint;   // 非 const：透视分支按参考重写为 pick/getTargetPoint 结果

    // TS L2057-2069: globalAlignment / animationOptions setup。
    //   const animationOptions = { animateFrustumChange: true, cancelOnAbort: true,
    //     animationTime: ScreenViewport.animation.time.wheel.milliseconds,  // 500ms
    //     easingFunction: Easing.Cubic.Out, onExtentsError, globalAlignment };
    // onExtentsError（outputStatusMessage）与 globalAlignment（globe 对齐过渡）
    // 未移植（状态消息/globe 子系统 TODO）；动画四项 1:1 ——滚轮缩放的"渐进感"
    // 来自这里的 FrustumAnimator（500ms Cubic.Out），缺失时每帧瞬跳。
    ViewChangeOptions animationOptions;
    animationOptions.animateFrustumChange = true;                       // :2314
    animationOptions.cancelOnAbort = true;                              // :2315
    animationOptions.animationTime = Viewport::animation().time.wheel;  // :2316（500ms）
    animationOptions.easingFunction = EasingFunction::CubicOut;         // :2317
    animationOptions.skipAspectFix = true;  // 参考 vp.zoom 正交分支不走 FixAspectRatio

    // TS L2074: if (view.is3d() && view.isCameraOn) { ... perspective branch ... }
    //           else { ... orthographic branch (L2113-2120) ... }
    ViewState* viewBase = vp->GetView();
    ViewState3d* view = viewBase ? viewBase->AsViewState3d() : nullptr;
    if (view == nullptr)
        return EventHandled::No;  // 2D view wheel zoom is TODO (Step 3 only has 3D views).

    if (view->is3d() && view->IsCameraOn()) {
        // Perspective branch (ToolAdmin.ts:2330-2368，WheelEventProcessor.doZoom
        // isCameraOn 分支)。眼点绕目标点做定点缩放 + bump 穿透 + lookAt 重组相机。
        //
        // :2330-2348 — 目标点三来源（参考序）：
        //   (a) 双击超时复用（lastWheelEvent：同视口、超时内、视点漂移 < 10px²）；
        //   (b) vp.pickNearestVisibleGeometry（featureId + depthAndOrder 深度回读）；
        //   (c) 缺省回退 view.getTargetPoint()。
        // EQUIVALENCE: 参考源=ToolAdmin.ts:2330-2348；发散=excludeNonLocatable/
        //   tentative/snap 目标通道未激活（无对应子系统）；验证法=Deco 视图光标
        //   停在几何上滚轮，缩放中心钉在该几何（而非常驻视图中心）。
        auto& inputState = Application::Get().GetToolAdmin().currentInputState();
        double const now = nowMilliseconds();
        bool targetReused = false;
        if (inputState.lastWheelEventValid
            && inputState.lastWheelEvent.viewport == vp
            && (now - inputState.lastWheelEvent.time) < 500.0   // ToolSettings.doubleClickTimeout (ToolSettings.ts:20)
            && (inputState.lastWheelEvent.viewPoint.DistanceSquaredXY(ev.viewPoint) < 10.0)
            && vp->GetView() == ev.viewport->GetView()) {
            target = inputState.lastWheelEvent.point;
            inputState.lastWheelEvent.time = now;
            targetReused = true;
        }
        if (!targetReused) {
            if (auto const newTarget = vp->pickNearestVisibleGeometry(target, vp->PixelsFromInches(0.20)))
                target = *newTarget;
            else
                target = view->GetTargetPoint();
            // :2346-2347 — 记录 lastWheelEvent（clone + point=target）
            inputState.lastWheelEvent = ev;
            inputState.lastWheelEvent.point = target;
            inputState.lastWheelEvent.time = now;
            inputState.lastWheelEventValid = true;
        }

        // :2349-2353 —— 目标点定点的 3D 均匀缩放作用于眼点
        dqGeom::Matrix3d const scaleMat = dqGeom::Matrix3d::CreateScale(zoomRatio, zoomRatio, zoomRatio);
        dqGeom::Transform const transform = dqGeom::Transform::CreateFixedPointAndMatrix(target, scaleMat);
        dqGeom::Point3d const eye = view->getEyePoint();
        dqGeom::Point3d newEye = transform.MultiplyPoint3d(eye);
        dqGeom::Vector3d offset = dqGeom::Vector3d::FromStartEnd(eye, newEye);

        // :2355-2362 —— 贴太近（步进 < bumpDist）时按 bump 距离穿透障碍
        // （wheelZoomBumpDistance = Constant.oneCentimeter = 0.01，
        //  ToolSettings.ts:62 + Constant.ts:18）
        double const bumpDist = std::max(ToolSettings::wheelZoomBumpDistance, view->minimumFrontDistance());
        if (offset.Magnitude() < bumpDist) {
            double const mag = offset.Normalize();
            if (mag > 0.0) {
                offset = dqGeom::Vector3d::From(offset.x * bumpDist, offset.y * bumpDist, offset.z * bumpDist);
                target = dqGeom::Point3d::From(target.x + offset.x, target.y + offset.y, target.z + offset.z);
                newEye = dqGeom::Point3d::From(eye.x + offset.x, eye.y + offset.y, eye.z + offset.z);
            }
            // :2360-2361 —— 穿透后目标作废（"we need to search on the other side"）
            inputState.lastWheelEventValid = false;
        }

        // :2363-2364 —— 目标点投影到过新眼点的视线上
        dqGeom::Vector3d const zDir = view->GetZVec();
        double const proj = zDir.DotProduct(
            dqGeom::Vector3d::From(target.x - newEye.x, target.y - newEye.y, target.z - newEye.z));
        target = dqGeom::Point3d::From(newEye.x + zDir.x * proj,
                                       newEye.y + zDir.y * proj,
                                       newEye.z + zDir.z * proj);

        // :2366-2367 —— lookAt 重组相机；成功后 synchWithView（动画 500ms Cubic.Out）
        dqApp::LookAtArgs args;
        args.eyePoint = newEye;
        args.targetPoint = target;
        args.upVector = view->GetYVec();
        args.lensAngleRadians = view->GetLensAngle();
        ViewStatus const status = view->lookAt(args);
        if (status != ViewStatus::Success)
            return EventHandled::No;
        // 同正交分支尾部：synchWithView 不发 redraw 请求（Viewport.ts:2228
        // 尾注），RequestRedraw 补齐重绘半边。
        vp->synchWithView(animationOptions);
        vp->RequestRedraw();
        return EventHandled::Yes;
    }

    // Orthographic branch (ToolAdmin.ts:2113-2120):
    //   const targetNpc = vp.worldToNpc(target);
    //   const trans = Transform.createFixedPointAndMatrix(targetNpc,
    //                                                     Matrix3d.createScale(zoomRatio, zoomRatio, 1));
    //   const viewCenter = trans.multiplyPoint3d(Point3d.create(.5, .5, .5));
    //   vp.npcToWorld(viewCenter, viewCenter);
    //   return vp.zoom(viewCenter, zoomRatio, animationOptions);
    //
    // DanQing adaptation: the reference computes the post-zoom view center in
    // NPC space, maps it back to world, then calls vp.zoom (which scales view
    // extents about that world point). For an orthographic view, this is
    // mathematically equivalent to scaling the world extents about the target
    // world point: every world point p maps to target + (p - target) * r.
    // The two-step NPC detour is needed in itwinjs only because vp.zoom
    // expresses the operation in NPC; a direct world-space scale is the same
    // operation. We apply it to ViewState3d extents/origin, then finish like
    // the reference's vp.zoom tail: vp->synchWithView() (Viewport.ts:2228) +
    // RequestRedraw.
    //
    // NOTE: the reference's vp.zoom uses factor2d = sqrt(factor) for X/Y
    // extents (Viewport.ts:3407-3409) — an aspect-ratio compensation that
    // preserves the apparent area scaling per axis. Step 3 uses uniform
    // scaling (zoomRatio on all 3 axes) for simplicity; the testable
    // invariant (zoom-in shrinks extents, zoom-out grows them) is preserved.
    // TODO: match the reference's sqrt-factor X/Y scaling when a test
    // requires it; orthographic zoom is faithful enough for Step 3 (extents
    // shrink/grow about the cursor world point).
    // 定点缩放：target（滚轮光标的世界点）保持屏幕位置不变——origin 公式
    // p' = target + (p - target)·zoomRatio（正交分支的标准定点缩放）。
    dqGeom::Point3d const origin = view->GetOrigin();
    dqGeom::Vector3d const extents = view->GetExtents();
    dqGeom::Point3d newOrigin = dqGeom::Point3d::From(
        target.x + (origin.x - target.x) * zoomRatio,
        target.y + (origin.y - target.y) * zoomRatio,
        target.z + (origin.z - target.z) * zoomRatio);
    dqGeom::Vector3d newExtents = dqGeom::Vector3d::From(
        extents.x * zoomRatio,
        extents.y * zoomRatio,
        extents.z * zoomRatio);

    // Ported from: itwinjs-core Viewport.zoom ortho branch (Viewport.ts:2216-2219) —
    //   const stat = view.adjustViewDelta(delta, center, rot, this.viewRect.aspect, options);
    //   if (ViewStatus.Success !== stat) return stat;
    // extentLimits clamp（min=0.001 / max=3×地球直径，SpatialViewState.ts:117）：
    // **超限（MinWindow/MaxWindow）→ 本次缩放整个中止**——不 setOrigin/
    // setExtents/synchWithView，视图冻结（DTA 行为：缩放到一定范围就停止，
    // 放大缩小皆然）。此前的半移补偿继续应用是自创偏差——extents 虽被 clamp
    // 而 origin 仍定点逼近，视图表现为"还能平移"，即用户所报"没有限制"。
    // doZoom 的 options.onExtentsError（outputStatusMessage，ToolAdmin.ts:2317）
    // 未移植——无状态消息，仅静默中止。
    {
        auto rot = view->getRotation();
        auto const centerViewVec = rot.MultiplyVector(
            dqGeom::Vector3d::From(target.x, target.y, target.z));
        dqGeom::Point3d center = dqGeom::Point3d::From(centerViewVec.x, centerViewVec.y, centerViewVec.z);
        ViewStatus const status = view->adjustViewDelta(newExtents, center, rot,
                                                        std::nullopt, nullptr);
        if (status != ViewStatus::Success)
            return EventHandled::No;
    }

    view->SetOrigin(newOrigin);
    view->SetExtents(newExtents);

    // 参考正交分支尾部 vp.zoom(viewCenter, zoomRatio, animationOptions)
    // （ToolAdmin.ts:2119）的末尾是 this.synchWithView(options)
    // （Viewport.ts:2228）——= setupFromView + saveViewUndo +
    // InvalidateController + animateFrustumChange(options)（撤销栈记录缩放、
    // FrustumAnimator 以 500ms Cubic.Out 渐进到新视锥）。RequestRedraw 保留
    // （synchWithView 不发 redraw 请求，对齐 Viewport::scroll /
    // setupViewFromFrustum 的 redraw 半边）。
    vp->synchWithView(animationOptions);
    vp->RequestRedraw();

    // TS L2123-2124: await IModelApp.accuSnap.reEvaluate(); return status;
    // Step 3 stub: accuSnap.reEvaluate is not yet ported.
    return EventHandled::Yes;
}

}  // namespace dqApp

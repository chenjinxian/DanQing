// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ToolAdmin
//
// Ported from: itwinjs-core core/frontend/src/tools/ToolAdmin.ts
// Manages the active tool, tool registry, and tool state.
#pragma once

#include "Export.h"
#include "Decorator.h"  // IDecorator base (W3: ToolAdmin 注册为 ViewManager 装饰器)

#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>

#include <dqBase/DqEvent.h>

#include <cstdint>
#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>

namespace dqApp {

class Viewport;
class ViewTool;          // Forward decl — full port in <dqApp/ViewTool.h> (Task 9).
class HitDetail;        // Forward decl — full port lands with DecorateContext (later task).
class DynamicsContext;  // Forward decl — full port lands with DecorateContext (later task).

// Distance in screen inches the cursor must move while a button is held down
// before the gesture is promoted from a click to a drag. DanQing Step 3 uses a
// fixed pixel fallback (5 px) so the drag-detection logic in InputState can run
// without a real Viewport (Viewport::pixelsFromInches is deferred to Task 7+).
// Ported from: itwinjs-core ToolSettings.startDragDistanceInches
//                (core/frontend/src/tools/ToolSettings.ts:34 = 0.15 in).
// kMouseDragThreshold is the null-viewport fallback (state-only tests). The
// real-viewport path (InputState::isStartDrag) uses the dpi-aware
// Viewport::PixelsFromInches(ToolSettings::startDragDistanceInches) = 14.4 px
// (Task 2); ToolSettings lives in dqApp/ToolSettings.h (full 1:1 port).
inline constexpr double kMouseDragThreshold = 5.0;

// Tool start/resume mode.
// Ported from: itwinjs-core StartOrResume
enum class StartOrResume : uint8_t {
    Start = 1,
    Resume = 2,
};

// --- Faithful Tool.ts ports (Task 1) ---------------------------------------

// The mouse button that generated an event.
// Ported from: itwinjs-core core/frontend/src/tools/Tool.ts:36
//   enum BeButton { Data = 0, Reset = 1, Middle = 2 }
enum class BeButton : uint8_t { Data = 0, Reset = 1, Middle = 2 };

// Mouse button event (legacy POD; the field type was migrated from the
// transitional `MouseButton` enum to the faithful `BeButton` port in Task 15.
// Retained as a POD for AccuDraw stub signatures (buttonEvent/motionEvent),
// which will be migrated to BeButtonEvent when the full AccuDraw state-machine
// lands post-Step-3.)
// (Authored transitional — predates Task 1; field type now faithful.)
struct ButtonEvent {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    BeButton button = BeButton::Data;
    bool isDown = false;
    bool isDoubleClick = false;
    int point = 0;  // 0=undefined, 1=start, 2=end
};

// Whether a tool / view handled an event (consumed by ToolAdmin dispatch).
// Ported from: itwinjs-core core/frontend/src/tools/Tool.ts:487
//   enum EventHandled { No = 0, Yes = 1 }
enum class EventHandled : uint8_t { No = 0, Yes = 1 };

// The source that generated an event.
// Ported from: itwinjs-core core/frontend/src/tools/Tool.ts:53-60
//   enum InputSource { Unknown = 0, Mouse = 1, Touch = 2 }
enum class InputSource : uint8_t {
    Unknown = 0,
    Mouse = 1,
    Touch = 2,
};

// The source that generated a coordinate.
// Ported from: itwinjs-core core/frontend/src/tools/Tool.ts:66-75
//   enum CoordSource { User = 0, Precision = 1, TentativePoint = 2, ElemSnap = 3 }
enum class CoordSource : uint8_t {
    User = 0,
    Precision = 1,
    TentativePoint = 2,
    ElemSnap = 3,
};

// Numeric mask for a set of modifier keys (control, shift, alt).
// Ported from: itwinjs-core core/frontend/src/tools/Tool.ts:81
//   enum BeModifierKeys { None = 0, Control = 1<<0, Shift = 1<<1, Alt = 1<<2 }
enum class BeModifierKeys : uint16_t {
    None    = 0,
    Control = 1 << 0,
    Shift   = 1 << 1,
    Alt     = 1 << 2,
};

// Bitwise OR for BeModifierKeys (bitmask type per §9 enum class).
inline BeModifierKeys operator|(BeModifierKeys a, BeModifierKeys b) noexcept
{
    return static_cast<BeModifierKeys>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}
inline BeModifierKeys operator&(BeModifierKeys a, BeModifierKeys b) noexcept
{
    return static_cast<BeModifierKeys>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

// Modifier key codes used to populate ToolEvent.key for modifier transitions.
// DanQing adaptation (§3.4): the reference ToolAdmin.getModifierKey reads
// `event.key` strings ("Alt"/"Shift"/"Control") from a DOM KeyboardEvent
// (ToolAdmin.ts:1434-1441). DanQing's ToolEvent.key is a uint32_t carrying a
// Qt::Key value (populated by the Task 14 Qt bridge). These constants mirror
// Qt::Key_Shift / Qt::Key_Control / Qt::Key_Alt so dispatch can identify
// modifier transitions without including Qt headers in this public header
// (engine SDK stays Qt-free; dqApp public headers exercise care per §8.2).
// Ported from: itwinjs-core ToolAdmin.getModifierKey (ToolAdmin.ts:1434-1441).
inline constexpr uint32_t kKeyShift   = 0x01000020u;  // Qt::Key_Shift
inline constexpr uint32_t kKeyControl = 0x01000021u;  // Qt::Key_Control
inline constexpr uint32_t kKeyAlt     = 0x01000023u;  // Qt::Key_Alt

// Object sent to Tools that holds information about button/touch/wheel events.
// Ported from: itwinjs-core core/frontend/src/tools/Tool.ts:148-200
// Field set + default values 1:1 with the reference class initializers:
//   point (L182, adjusted world), rawPoint (L185, unadjusted world),
//   viewPoint (L190, screen px), viewport (L154, undefined -> nullptr),
//   coordsFrom (L156 = User), keyModifiers (L158 = None), isDown (L160 = false),
//   isDoubleClick (L162 = false), isDragging (L164 = false),
//   button (L166 = Data), inputSource (L168 = Unknown).
// NOTE: this is a faithful POD port of the TS class's public data members. The
// reference's getter/setter wrappers around _point/_rawPoint/_viewPoint collapse
// to plain fields; methods (init/setFrom/clone/isControlKey/...) are deferred to
// the dispatch tasks (6/7/8) which actually need them.
struct BeButtonEvent {
    dqGeom::Point3d point;                       // adjusted world coordinates
    dqGeom::Point3d rawPoint;                    // unadjusted world coordinates
    dqGeom::Point3d viewPoint;                   // screen (view) coordinates
    Viewport* viewport = nullptr;                // undefined -> invalid event
    CoordSource coordsFrom = CoordSource::User;
    BeModifierKeys keyModifiers = BeModifierKeys::None;
    bool isDown = false;
    bool isDoubleClick = false;
    bool isDragging = false;
    BeButton button = BeButton::Data;
    InputSource inputSource = InputSource::Unknown;
};

// A BeButtonEvent generated by movement of a mouse wheel.
// Ported from: itwinjs-core core/frontend/src/tools/Tool.ts:335-348
//   class BeWheelEvent extends BeButtonEvent { wheelDelta: number; time: number; }
// wheelDelta is a SCALAR number in the reference (used as `ev.wheelDelta > 0`,
// Tool.ts L2042 / ViewTool.ts L2542); it is NOT a Point2d/Point3d. `time` mirrors
// the reference's `Date.now()` default of "now"; here it defaults to 0.0 and the
// dispatch layer (Task 8) stamps the real timestamp at event construction.
struct BeWheelEvent : BeButtonEvent {
    double wheelDelta = 0.0;
    double time = 0.0;
};

// Tag for the kind of event held by a ToolEvent (1:1 with the TS DOM `ev.type`
// strings consumed by ToolAdmin.processNextEvent's switch, ToolAdmin.ts:809-822).
// Ported from: itwinjs-core ToolAdmin.ts:809-822 (processNextEvent case labels).
//
// NOTE: touch events (`touchstart`/`touchend`/`touchmove`/`touchcancel`, TS
// L819-822) are not represented here — DanQing's mouse-driven Step 3 dispatch
// path does not wire them. They are TODO pending a touch-input task.
enum class ToolEventType : uint8_t {
    MouseDown,  // TS "mousedown"  (ToolAdmin.ts:810)
    MouseUp,    // TS "mouseup"    (ToolAdmin.ts:811)
    MouseMove,  // TS "mousemove"  (ToolAdmin.ts:812) — the only type tryReplace merges
    MouseOver,  // TS "mouseover"  (ToolAdmin.ts:813)
    MouseOut,   // TS "mouseout"   (ToolAdmin.ts:815)
    Wheel,      // TS "wheel"      (ToolAdmin.ts:816)
    KeyDown,    // TS "keydown"    (ToolAdmin.ts:817)
    KeyUp,      // TS "keyup"      (ToolAdmin.ts:818)
};

// A first-in-first-out queue element.
// Ported from: itwinjs-core ToolAdmin.ts:768 `_toolEvents: ToolEvent[]` and the
//               internal `{ ev, vp }` shape pushed at L798.
//
// DanQing adaptation (F1 controller decision — see .git/sdd/task-6-brief.md): the
// TS queue stores a live DOM `Event` + `ScreenViewport | undefined`. Qt events
// are short-lived (stack-allocated by Qt's event loop) and the FIFO may outlive
// them, so we cannot hold a `QEvent*`. Instead we store the extracted fields
// the dispatch layer (Tasks 7-8) and the Qt bridge (Task 14) need. This mirrors
// the information content of the reference (type + viewport + the fields
// dispatch reads off the DOM Event) without binding the queue to Qt types —
// preserving §8.2 (engine SDK Qt-free; ToolAdmin.h is a public header).
//
// Field defaults: `type = MouseMove` + `button = Data` match the TS path that
// pushes a fresh `{ ev, vp }` for every enqueued event (no defaults in TS, but
// MouseMove + Data are the most common inbound events and let a bare
// `ToolEvent{}` look like a harmless motion event).
struct ToolEvent {
    ToolEventType type = ToolEventType::MouseMove;
    Viewport* vp = nullptr;
    float posx = 0.f;                  // view pixels (QPointF.x), populated by Task 14 bridge
    float posy = 0.f;                  // view pixels (QPointF.y)
    BeButton button = BeButton::Data;  // mouse button for MouseDown/Up events
    BeModifierKeys modifiers = BeModifierKeys::None;
    float wheelDeltaX = 0.f;           // QWheelEvent angleDelta.x (Task 8 onWheel reads wheelDeltaY)
    float wheelDeltaY = 0.f;           // QWheelEvent angleDelta.y
    uint32_t key = 0;                  // Qt::Key value for KeyDown/Up events
    bool isDoubleClick = false;
};

// Holds the per-button state tracked by CurrentInputState.
// Ported from: itwinjs-core core/frontend/src/tools/Tool.ts:87-110
//   class BeButtonState {
//     _downUorPt: Point3d; _downRawPt: Point3d; downTime: number;
//     isDown: boolean; isDoubleClick: boolean; isDragging: boolean;
//     inputSource: InputSource;
//     init(downUorPt, downRawPt, downTime, isDown, isDoubleClick, isDragging, source);
//   }
// Field set + default values 1:1 with the reference class initializers:
//   downTime (L90 = 0), isDown (L91 = false), isDoubleClick (L92 = false),
//   isDragging (L93 = false), inputSource (L94 = Unknown).
// NOTE: the TS class wraps _downUorPt/_downRawPt in get/set pairs that copy on
// assign; here both are exposed as plain POD value members (Point3d is already
// a value type in dqGeom) — same semantics, no boilerplate.
struct BeButtonState {
    dqGeom::Point3d downUorPt;                       // adjusted world coords at button-down
    dqGeom::Point3d downRawPt;                       // unadjusted world coords at button-down
    double downTime = 0.0;                           // ms since epoch at button-down (Date.now())
    bool isDown = false;
    bool isDoubleClick = false;
    bool isDragging = false;
    InputSource inputSource = InputSource::Unknown;

    // Initialize all fields (1:1 with the reference's 7-arg init, Tool.ts:101-109).
    // Ported from: itwinjs-core BeButtonState.init (Tool.ts:101)
    void init(dqGeom::Point3d const& uorPt, dqGeom::Point3d const& rawPt,
              double time, bool down, bool doubleClick, bool dragging,
              InputSource source) noexcept
    {
        downUorPt = uorPt;
        downRawPt = rawPt;
        downTime = time;
        isDown = down;
        isDoubleClick = doubleClick;
        isDragging = dragging;
        inputSource = source;
    }
};

// Tracks the current input state (button-down/drag tracking, motion, modifier
// qualifiers, viewport) used by ToolAdmin to populate BeButtonEvents.
// Ported from: itwinjs-core core/frontend/src/tools/ToolAdmin.ts:145-324
//   export class CurrentInputState { ... }
//
// Scope (Step 3): the reference also performs AccuSnap adjustment, tentative-
// point coordinate fixing, and worldToView transforms inside fromPoint/
// fromButton/toEvent. Those subsystems are out of scope for Step 3 (accuSnap
// and tentativePoint are no-op stubs landing in Task 7+); they are marked
// `// TODO: <subsystem> — Step 3 stub` per CLAUDE.md §0. The no-snap path sets
// coordsFrom = CoordSource::User and point = rawPoint = viewPoint (the unsnapped
// cursor position), which is faithful for the Step 3 case.
class DQ_APP_EXPORT InputState {
public:
    InputState() = default;

    // --- Public state (1:1 with reference public fields, ToolAdmin.ts:149-159) ---

    BeButtonState button[3];                          // L151: indexed by BeButton
    BeButton lastButton = BeButton::Data;             // L152
    InputSource inputSource = InputSource::Unknown;   // L153
    dqGeom::Point2d lastMotion;                       // L154
    BeModifierKeys qualifiers = BeModifierKeys::None; // L149
    Viewport* viewport = nullptr;                     // L150 (undefined -> nullptr)

    // Reference fields that require BeButtonEvent/BeWheelEvent/BeTouchEvent
    // lifetimes (L155-159: lastMotionEvent, lastWheelEvent, lastTouchStart,
    // touchTapTimer, touchTapCount) are deferred to dispatch tasks (6/7/8) which
    // actually own the event queue; they are not needed for Step 3 state tracking.

    // L158: lastWheelEvent（透视滚轮缩放双击超时的目标复用，
    // ToolAdmin.ts:2333-2343——lastWheelEvent.time/viewPoint/point）。
    BeWheelEvent lastWheelEvent;
    bool lastWheelEventValid = false;

    // --- point/rawPoint/viewPoint accessors (ToolAdmin.ts:161-166 get/set pairs) ---

    dqGeom::Point3d const& rawPoint() const noexcept { return m_rawPoint; }
    void rawPoint(dqGeom::Point3d const& pt) noexcept { m_rawPoint = pt; }
    dqGeom::Point3d const& point() const noexcept { return m_point; }
    void point(dqGeom::Point3d const& pt) noexcept { m_point = pt; }
    dqGeom::Point3d const& viewPoint() const noexcept { return m_viewPoint; }
    void viewPoint(dqGeom::Point3d const& pt) noexcept { m_viewPoint = pt; }

    // Modifier-key convenience queries (ToolAdmin.ts:167-169).
    bool isShiftDown() const noexcept
    {
        return (qualifiers & BeModifierKeys::Shift) != BeModifierKeys::None;
    }
    bool isControlDown() const noexcept
    {
        return (qualifiers & BeModifierKeys::Control) != BeModifierKeys::None;
    }
    bool isAltDown() const noexcept
    {
        return (qualifiers & BeModifierKeys::Alt) != BeModifierKeys::None;
    }

    // --- Public methods (ToolAdmin.ts:171-323) ---

    // Whether `button` is currently being dragged.
    // Ported from: itwinjs-core CurrentInputState.isDragging (ToolAdmin.ts:171)
    bool isDragging(BeButton b) const noexcept
    {
        return button[static_cast<int>(b)].isDragging;
    }

    // Mark `button` as being dragged (called by dispatch when isStartDrag returns true).
    // Ported from: itwinjs-core CurrentInputState.onStartDrag (ToolAdmin.ts:172)
    void onStartDrag(BeButton b) noexcept
    {
        button[static_cast<int>(b)].isDragging = true;
    }

    // Reset transient state when a new tool is installed.
    // Ported from: itwinjs-core CurrentInputState.onInstallTool (ToolAdmin.ts:173-177)
    void onInstallTool() noexcept;

    // Clear all modifier qualifiers.
    // Ported from: itwinjs-core CurrentInputState.clearKeyQualifiers (ToolAdmin.ts:179)
    void clearKeyQualifiers() noexcept { qualifiers = BeModifierKeys::None; }

    // If the current viewport matches `vp`, clear it (event becomes invalid).
    // Ported from: itwinjs-core CurrentInputState.clearViewport (ToolAdmin.ts:180-183)
    void clearViewport(Viewport* vp) noexcept
    {
        if (vp == viewport)
            viewport = nullptr;
    }

    // Update the modifier qualifier mask. DanQing adaptation (§3.4): the reference's
    // setKeyQualifiers takes a DOM MouseEvent/KeyboardEvent/TouchEvent and reads
    // shiftKey/ctrlKey/altKey; without a DOM we accept the pre-extracted mask.
    // Ported from: itwinjs-core CurrentInputState.setKeyQualifiers (ToolAdmin.ts:190-194)
    //               + private setKeyQualifier (ToolAdmin.ts:186-188)
    void setKeyQualifiers(BeModifierKeys mods) noexcept { qualifiers = mods; }

    // Record the latest cursor motion (screen px).
    // Ported from: itwinjs-core CurrentInputState.onMotion (ToolAdmin.ts:196-199)
    void onMotion(dqGeom::Point2d pt2d) noexcept
    {
        lastMotion = pt2d;
    }

    // Reset ev's point/rawPoint/viewPoint to the stored button-down position.
    // Ported from: itwinjs-core CurrentInputState.changeButtonToDownPoint (ToolAdmin.ts:201-207)
    void changeButtonToDownPoint(BeButtonEvent& ev) const;

    // Store ev.point as the new button-down (adjusted) point for ev.button.
    // Ported from: itwinjs-core CurrentInputState.updateDownPoint (ToolAdmin.ts:209)
    void updateDownPoint(BeButtonEvent const& ev) noexcept
    {
        button[static_cast<int>(ev.button)].downUorPt = ev.point;
    }

    // Record a button-down transition: stamps the current point/rawPoint as the
    // button's down position, sets isDown=true, and detects double-click.
    // Ported from: itwinjs-core CurrentInputState.onButtonDown (ToolAdmin.ts:211-226)
    void onButtonDown(BeButton b);

    // Record a button-up transition: clears isDown and isDragging for the button.
    // Ported from: itwinjs-core CurrentInputState.onButtonUp (ToolAdmin.ts:228-232)
    void onButtonUp(BeButton b) noexcept
    {
        button[static_cast<int>(b)].isDown = false;
        button[static_cast<int>(b)].isDragging = false;
        lastButton = b;
    }

    // Populate `ev` from the current state. When `useSnap` is true the reference
    // consults AccuSnap/tentativePoint to override the point; that path is a
    // Step 3 stub (TODO above) and we fall through to coordsFrom=User with the
    // unsnapped cursor — faithful for the no-snap case.
    // Ported from: itwinjs-core CurrentInputState.toEvent (ToolAdmin.ts:234-259)
    void toEvent(BeButtonEvent& ev, bool useSnap) const;

    // From a screen-space cursor position, set viewport + viewPoint + rawPoint
    // + point + inputSource. World-coord transforms require a real viewport;
    // the null-viewport path is a Step 3 identity-stub used by state-only tests.
    // Ported from: itwinjs-core CurrentInputState.fromPoint (ToolAdmin.ts:280-288)
    void fromPoint(Viewport* vp, dqGeom::Point2d pt, InputSource source);

    // fromPoint + AccuSnap/adjustPoint hooks. The snap/adjust calls are Step 3
    // stubs (subsystems deferred to Task 7+); only the fromPoint part runs.
    // Ported from: itwinjs-core CurrentInputState.fromButton (ToolAdmin.ts:290-300)
    void fromButton(Viewport* vp, dqGeom::Point2d pt, InputSource source, bool applyLocks);

    // Whether a drag gesture has started for `button` (button is down, no other
    // button is already dragging, and the cursor moved past the drag threshold).
    // Step 3 stub: the reference additionally enforces a startDragDelay time
    // check (ToolSettings.startDragDelay = 110 ms) and uses a viewport-derived
    // pixel threshold; both are TODO pending dispatch wiring. The motion-distance
    // check uses kMouseDragThreshold (5 px) when no viewport is available.
    // Ported from: itwinjs-core CurrentInputState.isStartDrag (ToolAdmin.ts:302-323)
    bool isStartDrag(BeButton b) const noexcept;

private:
    // Whether any button is currently dragging.
    // Ported from: itwinjs-core CurrentInputState.isAnyDragging (ToolAdmin.ts:185)
    bool isAnyDragging() const noexcept
    {
        return button[0].isDragging || button[1].isDragging || button[2].isDragging;
    }

    dqGeom::Point3d m_rawPoint;    // ToolAdmin.ts:146 (_rawPoint)
    dqGeom::Point3d m_point;       // ToolAdmin.ts:147 (_point)
    dqGeom::Point3d m_viewPoint;   // ToolAdmin.ts:148 (_viewPoint)
};

// Abstract base class for tools.
// Ported from: itwinjs-core core/frontend/src/tools/Tool.ts:358-481
class DQ_APP_EXPORT Tool {
public:
    virtual ~Tool() = default;

    // The tool's unique identifier.
    // Ported from: itwinjs-core Tool.get toolId (Tool.ts:444)
    virtual const char* getToolId() const = 0;

    // Whether this tool is active.
    // Ported from: itwinjs-core InteractiveToolisActive (via ToolAdmin tracking).
    bool isActive() const noexcept { return m_isActive; }

    // Set active state (called by ToolAdmin).
    void setActive(bool active) noexcept { m_isActive = active; }

    // Run this instance of a Tool. Subclasses override to perform some action.
    // Ported from: itwinjs-core Tool.run (Tool.ts:470) — TS `async run(...args): Promise<boolean>`
    // maps to a sync `bool` per §3.4. Variadic args dropped (no C++ equivalent; subclasses
    // override with their own specific argument lists).
    virtual bool run() { return true; }

private:
    bool m_isActive = false;
};

// A Tool that may be installed, via ToolAdmin, to handle user input. The ToolAdmin
// manages the currently installed ViewingTool, PrimitiveTool, InputCollector, and
// IdleTool. Each must derive from this class and there may only be one of each type
// installed at a time.
// Ported from: itwinjs-core core/frontend/src/tools/Tool.ts:494-869 (InteractiveTool)
class DQ_APP_EXPORT InteractiveTool : public Tool {
public:
    // Used to avoid sending tools up events for which they did not receive the down event.
    // Ported from: itwinjs-core InteractiveTool.receivedDownEvent (Tool.ts:497)
    bool receivedDownEvent = false;

    // Override to execute additional logic when tool is installed. Return false to prevent
    // this tool from becoming active.
    // Ported from: itwinjs-core InteractiveTool.onInstall (Tool.ts:500)
    virtual bool onInstall() { return true; }

    // Override to execute additional logic after tool becomes active.
    // Ported from: itwinjs-core InteractiveTool.onPostInstall (Tool.ts:503)
    virtual void onPostInstall() {}

    // Keep the view cursor, locate aperture circle, and snap/locate enable
    // consistent with AccuSnap (Tool.ts:686-720 changeLocateState). The
    // accuSnap.enableLocate/enableSnap calls and coordLockOvr are TODO with
    // those subsystems — the cursor + locateCircleOn part is the ported slice
    // (SelectionTool.installs pass cursor="default" + enableLocate → the
    // locate aperture circle follows the cursor).
    // Ported from: itwinjs-core InteractiveTool.changeLocateState (Tool.ts:696-720).
    void changeLocateState(bool enableLocate, bool enableSnap, std::string const& cursor);

    // Convenience: changeLocateState with the tool's default cursor (undefined
    // in the reference → setLocateCursor). Ported from: Tool.ts:729-738.
    void initLocateElements(bool enableLocate = true, bool enableSnap = false,
                            std::string const& cursor = {});

    // Request that this tool exit and the default tool be started.
    // Ported from: itwinjs-core InteractiveTool.exitTool (Tool.ts:505) — `abstract` in TS;
    // here it has a default `{}` body so the InteractiveToolDefaults test (which subclasses
    // with only getToolId overridden) can instantiate. Concrete tools override.
    virtual void exitTool() {}

    // Override to reset tool to initial state.
    // Ported from: itwinjs-core InteractiveTool.onReinitialize (Tool.ts:508)
    virtual void onReinitialize() {}

    // Invoked when the tool becomes no longer active, to perform additional cleanup logic.
    // Ported from: itwinjs-core InteractiveTool.onCleanup (Tool.ts:511)
    virtual void onCleanup() {}

    // Notification of a ViewTool or InputCollector starting and this tool is being suspended.
    // Applies only to PrimitiveTool and InputCollector; a ViewTool can't be suspended.
    // Ported from: itwinjs-core InteractiveTool.onSuspend (Tool.ts:516)
    virtual void onSuspend() {}

    // Notification of a ViewTool or InputCollector exiting and this tool is being unsuspended.
    // Ported from: itwinjs-core InteractiveTool.onUnsuspend (Tool.ts:521)
    virtual void onUnsuspend() {}

    // Invoked when the reset button is pressed. Default No. Sub-classes may ascribe special
    // meaning to this status. To support right-press menus, a tool should put its reset event
    // processing in onResetButtonUp instead of onResetButtonDown.
    // Ported from: itwinjs-core InteractiveTool.onResetButtonDown (Tool.ts:548)
    virtual EventHandled onResetButtonDown(BeButtonEvent const&) { return EventHandled::No; }
    // Invoked when the reset button is released. Default No.
    // Ported from: itwinjs-core InteractiveTool.onResetButtonUp (Tool.ts:552)
    virtual EventHandled onResetButtonUp(BeButtonEvent const&) { return EventHandled::No; }

    // Invoked when the data button is pressed. Default No.
    // Ported from: itwinjs-core InteractiveTool.onDataButtonDown (Tool.ts:557)
    virtual EventHandled onDataButtonDown(BeButtonEvent const&) { return EventHandled::No; }
    // Invoked when the data button is released. Default No.
    // Ported from: itwinjs-core InteractiveTool.onDataButtonUp (Tool.ts:561)
    virtual EventHandled onDataButtonUp(BeButtonEvent const&) { return EventHandled::No; }

    // Invoked when the middle mouse button is pressed.
    // Ported from: itwinjs-core InteractiveTool.onMiddleButtonDown (Tool.ts:566)
    virtual EventHandled onMiddleButtonDown(BeButtonEvent const&) { return EventHandled::No; }
    // Invoked when the middle mouse button is released.
    // Ported from: itwinjs-core InteractiveTool.onMiddleButtonUp (Tool.ts:571)
    virtual EventHandled onMiddleButtonUp(BeButtonEvent const&) { return EventHandled::No; }

    // Invoked when the cursor is moving.
    // Ported from: itwinjs-core InteractiveTool.onMouseMotion (Tool.ts:574)
    virtual void onMouseMotion(BeButtonEvent const&) {}

    // Invoked when the cursor begins moving while a button is depressed.
    // Ported from: itwinjs-core InteractiveTool.onMouseStartDrag (Tool.ts:579)
    virtual EventHandled onMouseStartDrag(BeButtonEvent const&) { return EventHandled::No; }
    // Invoked when the button is released after onMouseStartDrag.
    // Ported from: itwinjs-core InteractiveTool.onMouseEndDrag (Tool.ts:584) — reference body
    // forwards to onDataButtonDown for BeButton::Data; deferred to dispatch task that needs it.
    virtual EventHandled onMouseEndDrag(BeButtonEvent const&) { return EventHandled::No; }

    // Invoked when the mouse wheel moves.
    // Ported from: itwinjs-core InteractiveTool.onMouseWheel (Tool.ts:599).
    // Non-const BeWheelEvent& matches the reference's mutable-ev contract —
    // ToolAdmin.processWheelEvent (ToolAdmin.ts:1965) writes back into ev via
    // WheelEventProcessor::doZoom, and IdleTool/ViewManip forward through it.
    virtual EventHandled onMouseWheel(BeWheelEvent&) { return EventHandled::No; }

    // Called when any key is pressed or released.
    // Ported from: itwinjs-core InteractiveTool.onKeyTransition (Tool.ts:615) — TS takes a
    // DOM `KeyboardEvent`; C++ port takes the key code as `uint32_t` (matches the existing
    // DanQing OnKeyDown/OnKeyUp key representation; full KeyboardEvent port deferred).
    virtual EventHandled onKeyTransition(bool /*wentDown*/, uint32_t /*key*/) { return EventHandled::No; }

    // Called when Control, Shift, or Alt modifier keys are pressed or released.
    // Ported from: itwinjs-core InteractiveTool.onModifierKeyTransition (Tool.ts:607) — DOM
    // `KeyboardEvent` collapsed to `BeModifierKeys` modifier mask; third param omitted.
    virtual EventHandled onModifierKeyTransition(bool /*wentDown*/, BeModifierKeys /*modifier*/) { return EventHandled::No; }

    // Whether the supplied viewport is compatible with this tool.
    // Ported from: itwinjs-core InteractiveTool.isCompatibleViewport (Tool.ts:642)
    virtual bool isCompatibleViewport(Viewport* /*vp*/, bool /*isSelectedViewChange*/) const { return true; }

    // Whether the supplied location is acceptable for this tool.
    // Ported from: itwinjs-core InteractiveTool.isValidLocation (Tool.ts:643)
    virtual bool isValidLocation(BeButtonEvent const&, bool /*isButtonEvent*/) const { return true; }

    // Called when active view changes. Tool may choose to restart or exit based on current view type.
    // Ported from: itwinjs-core InteractiveTool.onSelectedViewportChanged (Tool.ts:650)
    virtual void onSelectedViewportChanged(Viewport* /*previous*/, Viewport* /*current*/) {}

    // Invoked before the locate tooltip is displayed to retrieve information about the located
    // element. Allows the tool to override the toolTip.
    // Ported from: itwinjs-core InteractiveTool.getToolTip (Tool.ts:658) — TS returns
    // `HTMLElement | string`; without a DOM, C++ returns `std::string` only.
    virtual std::string getToolTip(const HitDetail& /*hit*/) const { return {}; }

    // Called to allow Tool to display dynamic elements.
    // Ported from: itwinjs-core InteractiveTool.onDynamicFrame (Tool.ts:676)
    virtual void onDynamicFrame(BeButtonEvent const&, DynamicsContext& /*context*/) {}
};

// The PrimitiveTool class can be used to implement tools to create or modify geometric elements.
// Ported from: itwinjs-core core/frontend/src/tools/PrimitiveTool.ts:23-229
class DQ_APP_EXPORT PrimitiveTool : public InteractiveTool {
public:
    // Called on data button down event to lock the tool to its current target model.
    // Ported from: itwinjs-core PrimitiveTool.autoLockTarget (PrimitiveTool.ts:145-150)
    // (targetIsLocked flip; full iModel target resolution deferred.)
    virtual void autoLockTarget() {}

    // Called to reverse to a previous tool state (ex. undo last data button).
    // Return false to instead reverse the most recent transaction.
    // Ported from: itwinjs-core PrimitiveTool.onUndoPreviousStep (PrimitiveTool.ts:192)
    virtual bool onUndoPreviousStep() { return false; }
    // Internal undo driver; calls onUndoPreviousStep and refreshes dynamics.
    // Ported from: itwinjs-core PrimitiveTool.undoPreviousStep (PrimitiveTool.ts:195-204) —
    // AccuDraw/viewManager invalidation deferred; here it just forwards.
    virtual bool undoPreviousStep() { return onUndoPreviousStep(); }

    // Called to reinstate to a previous tool state (ex. redo last data button).
    // Ported from: itwinjs-core PrimitiveTool.onRedoPreviousStep (PrimitiveTool.ts:210)
    virtual bool onRedoPreviousStep() { return false; }
    // Ported from: itwinjs-core PrimitiveTool.redoPreviousStep (PrimitiveTool.ts:213-222)
    virtual bool redoPreviousStep() { return onRedoPreviousStep(); }

    // Called from isCompatibleViewport to check for a read only iModel, which is not a valid
    // target for tools that create or modify elements.
    // Ported from: itwinjs-core PrimitiveTool.requireWriteableTarget (PrimitiveTool.ts:156)
    virtual bool requireWriteableTarget() const { return true; }

    // Unique id placeholder for the abstract base; concrete subclasses override.
    const char* getToolId() const override { return "PrimitiveTool"; }
};

// ViewTool's full definition lives in <dqApp/ViewTool.h> (Task 9 expanded it from
// the Task 2 marker to the faithful ViewTool.ts:92-128 surface: viewport member,
// changeViewport, beginDynamicUpdate/endDynamicUpdate, exitTool, run, showPrompt,
// translate). Forward-declared above so ToolAdmin's ViewTool* slot type resolves.

// The InputCollector class can be used to implement a command for gathering input
// (ex. get a distance by snapping to 2 points) without affecting the state of the active
// primitive tool. An InputCollector will suspend the active PrimitiveTool and can be
// suspended by a ViewTool.
// Ported from: itwinjs-core core/frontend/src/tools/Tool.ts:877-896
class DQ_APP_EXPORT InputCollector : public InteractiveTool {
public:
    // Minimal stub — full install/exit dispatch wired in a later task.
    const char* getToolId() const override { return "InputCollector"; }
};

// Tool type — pointer to a Tool subclass.
// Ported from: itwinjs-core Tool.ts ToolType
using ToolType = InteractiveTool* (*)();

// View-tool factory — pointer to a function creating an InteractiveTool with
// (Viewport*, oneShot, isDraggingRequired) args. Used by View.* tools
// (PanViewTool, RotateViewTool, ScrollViewTool, FitViewTool, ...) which mirror
// the itwinjs-core `IModelApp.tools.create(toolId, vp, oneShot, isDraggingRequired)`
// call site (Tool.ts:1020-1023 variadic create + ViewTool.ts:3038/3051/3077/3203
// ctors all take a viewport + oneShot + isDraggingRequired).
//
// DanQing adaptation (§3.4): the TS reference's ToolType is `new (...args) => Tool`
// — a variadic constructor reference. C++ function pointers are not variadic, so
// the no-arg ToolType (Select, Idle) and the viewport-arg ViewToolFactory (View.*)
// each get their own map. This mirrors the reference's intent 1:1 (create(toolId,
// ...args) → constructor(...args)); the split is a C++ type-system necessity, not
// a behavioral deviation. Both maps are searched by their respective Find/Create
// methods.
// Ported from: itwinjs-core Tool.ts ToolType + Tool.ts:1020 create() variadic shape.
using ViewToolFactory = InteractiveTool* (*)(Viewport*, bool, bool);

// The ToolRegistry holds a mapping between toolIds and their corresponding Tool class.
// Ported from: itwinjs-core ToolRegistry (Tool.ts:960-1034)
class DQ_APP_EXPORT ToolRegistry {
public:
    // Register a Tool class. Establishes connection between toolId and the class.
    // Ported from: itwinjs-core ToolRegistry.register() (Tool.ts:982-994)
    void Register(const char* toolId, ToolType toolClass)
    {
        if (!toolId || toolId[0] == '\0')
            return;  // must be an abstract class, ignore it
        m_tools[toolId] = toolClass;
    }

    // Register a view-tool factory (viewport-arg variant). View.* tools are
    // constructed with (Viewport*, oneShot, isDraggingRequired); this method
    // registers a factory closure matching that signature.
    // Ported from: itwinjs-core ToolRegistry.register() (Tool.ts:982-994) — view-tool variant.
    void RegisterView(const char* toolId, ViewToolFactory factory)
    {
        if (!toolId || toolId[0] == '\0')
            return;  // mirror the no-arg Register guard
        m_viewTools[toolId] = factory;
    }

    // Un-register a previously registered Tool class.
    // Ported from: itwinjs-core ToolRegistry.unRegister() (Tool.ts:972-975)
    void UnRegister(const char* toolId)
    {
        m_tools.erase(toolId);
        m_viewTools.erase(toolId);
    }

    // Look up a tool by toolId (no-arg factory).
    // Ported from: itwinjs-core ToolRegistry.find() (Tool.ts:1010-1012)
    ToolType Find(const char* toolId) const
    {
        auto it = m_tools.find(toolId);
        return (it != m_tools.end()) ? it->second : nullptr;
    }

    // Look up a view-tool factory by toolId (viewport-arg factory).
    // Ported from: itwinjs-core ToolRegistry.find() (Tool.ts:1010-1012) — view-tool variant.
    ViewToolFactory FindView(const char* toolId) const
    {
        auto it = m_viewTools.find(toolId);
        return (it != m_viewTools.end()) ? it->second : nullptr;
    }

    // Look up a tool by toolId and create an instance (no-arg factory).
    // Ported from: itwinjs-core ToolRegistry.create() (Tool.ts:1020-1023)
    InteractiveTool* Create(const char* toolId) const
    {
        auto toolClass = Find(toolId);
        return toolClass ? toolClass() : nullptr;
    }

    // Look up a view tool by toolId and create an instance with
    // (viewport, oneShot, isDraggingRequired) args.
    // Ported from: itwinjs-core ToolRegistry.create() (Tool.ts:1020-1023) —
    //              viewport-arg variant (View.* tools).
    InteractiveTool* CreateVP(const char* toolId, Viewport* vp,
                               bool oneShot, bool isDraggingRequired) const
    {
        auto factory = FindView(toolId);
        return factory ? factory(vp, oneShot, isDraggingRequired) : nullptr;
    }

    // Shut down the registry.
    // Ported from: itwinjs-core ToolRegistry.shutdown() (Tool.ts:964-967)
    void shutdown()
    {
        m_tools.clear();
        m_viewTools.clear();
    }

private:
    // No-arg factories (Select, Idle, ...).
    // Ported from: itwinjs-core ToolRegistry.tools (Tool.ts:962).
    std::unordered_map<std::string, ToolType> m_tools;
    // Viewport-arg factories (View.Pan, View.Rotate, View.Scroll, View.Fit, ...).
    // DanQing adaptation (see ViewToolFactory typedef above).
    std::unordered_map<std::string, ViewToolFactory> m_viewTools;
};

// The ToolAdmin manages the active tool, tool registry, and tool state.
// Ported from: itwinjs-core ToolAdmin (ToolAdmin.ts)
//
// W3: ToolAdmin 继承 IDecorator——参考 ToolAdmin 实现 Decorator 接口
// （decorate/testDecorationHit，ToolAdmin.ts:2043-2068），由
// ViewManager.onInitialized 注册（ViewManager.ts:126-130
// `this.addDecorator(IModelApp.toolAdmin)`，DanQing 接线在 Application::Startup）。
// ---------------------------------------------------------------------------
// ToolState — per-session tool input state shared by tools.
// Ported from: itwinjs-core ToolAdmin.ts ToolState (:88-99).
// Only the members the locate-circle chain reads are ported (locateCircleOn);
// coordLockOvr arrives with the AccuSnap/coordinate-lock engine (registered
// TODO in changeLocateState's ported body).
// ---------------------------------------------------------------------------
struct ToolState {
    bool locateCircleOn = false;   // ToolAdmin.ts:93
};

// ---------------------------------------------------------------------------
// SuspendedToolState — 视图工具挂起期间被挂起工具（+光标/光圈/snap）的状态快照
// Ported from: itwinjs-core ToolAdmin.ts SuspendedToolState (:107-141)。
// accuSnap 状态按字段平铺（AccuSnap.h 反向包含本头，不能按类型持有
// AccuSnap::ToolState）；locateManager.options 未移植（LocateManager 子系统
// TODO）；_inDynamics 恒 false（dynamics 未移植，参考 :120-121/:133-136 跳过）。
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT SuspendedToolState {
public:
    SuspendedToolState();   // 构造即快照（参考 ctor :112-122）
    void stop();            // 恢复快照（参考 stop :124-141）
private:
    ToolState m_toolState;              // toolAdmin.toolState.clone()
    bool m_accuSnapEnabled = false;     // accuSnap.toolState.enabled
    bool m_accuSnapLocate = false;      // accuSnap.toolState.locate
    int m_accuSnapSuspended = 0;        // accuSnap.toolState.suspended
    std::string m_viewCursor;           // viewManager.cursor
};

class DQ_APP_EXPORT ToolAdmin : public IDecorator {
public:
    ToolAdmin() = default;
    virtual ~ToolAdmin() = default;

    // Called after Application startup to initialize tools.
    // Ported from: itwinjs-core ToolAdmin.onInitialized() (ToolAdmin.ts:487-499)
    void OnInitialized();

    // Tool input state shared by tools. ← ToolAdmin.toolState (:92 public).
    ToolState toolState;

    // Set whether the locate aperture circle follows the cursor
    // (ToolAdmin.setLocateCircleOn, :1188-1192 — invalidates all views when the
    // state actually changes).
    void setLocateCircleOn(bool on);
    // Whether the locate circle currently shows (mouse always; touch only with a
    // virtual cursor — :2072-2078; touch input is TODO so this collapses to
    // toolState.locateCircleOn).
    bool isLocateCircleOn() const { return toolState.locateCircleOn; }

    // Set the cursor + locate circle consistently for tools that locate
    // elements (ToolAdmin.setLocateCursor, :2214-2219).
    void setLocateCursor(bool enableLocate);

    // Get the tool registry.
    // Ported from: itwinjs-core IModelApp.tools
    ToolRegistry& GetRegistry() { return m_registry; }
    ToolRegistry const& GetRegistry() const { return m_registry; }

    // --- Tool priority slots + accessors (Task 8) ---------------------------
    // Ported from: itwinjs-core ToolAdmin.ts:872-881.
    //
    // The reference resolves the "active" tool by priority:
    //   viewTool ?? inputCollector ?? primitiveTool
    // and the "current" tool as activeTool ?? idleTool. Task 7 stored a single
    // m_activeTool slot; Task 8 introduces the three slots + the priority-
    // resolution getter, preserving the existing SetActiveTool lifecycle by
    // operating on the primitive slot.

    // The currently active InteractiveTool, resolved by priority
    // (viewTool > inputCollector > primitiveTool). Returns nullptr if no tool
    // is installed in any slot.
    // Ported from: itwinjs-core ToolAdmin.activeTool (ToolAdmin.ts:876-878).
    //
    // Defined out-of-line in ToolAdmin.cpp: the derived-to-base pointer
    // conversion `static_cast<InteractiveTool*>(m_viewTool)` requires ViewTool's
    // complete type (it inherits InteractiveTool), but ToolAdmin.h only forward-
    // declares ViewTool to avoid a circular include with ViewTool.h.
    InteractiveTool* activeTool() const noexcept;

    // The current tool — activeTool() if set, else idleTool. Mirrors the
    // reference's non-undefined getter (idleTool is installed by OnInitialized;
    // caller contract is that idleTool is non-null before invocation).
    // Ported from: itwinjs-core ToolAdmin.currentTool (ToolAdmin.ts:881).
    InteractiveTool& currentTool();

    // Get the active tool. Transitional alias for activeTool() — Task 7's
    // dispatch (sendButtonEvent / filterViewport) and the lifecycle tests in
    // ToolAdminTest.cpp read through this getter, so it preserves the
    // priority resolution introduced in Task 8.
    InteractiveTool* GetActiveTool() const noexcept { return activeTool(); }

    // Set the active primitive tool (transitional API preserved for the
    // lifecycle tests + StartDefaultTool).
    // Ported from: itwinjs-core ToolAdmin.startPrimitiveTool (ToolAdmin.ts:1676-1720)
    //               — reduced to the primitive-slot lifecycle (exitViewTool /
    //               exitInputCollector / toolState resets are deferred). The
    //               onCleanup/onSuspend/onPostInstall/onUnsuspend lifecycle
    //               is faithful to Task 2's mapping.
    //
    // Lifecycle mapping (DanQing legacy -> faithful itwinjs Tool.ts):
    //   OnStart(Start)  -> onPostInstall()       (onInstall is a pre-install gate, not yet
    //                                              wired here; declared on InteractiveTool
    //                                              so future dispatch tasks can call it)
    //   OnStart(Resume) -> onUnsuspend()
    //   OnResume(mode)  -> onUnsuspend()         (itwinjs has no onResume; unsuspend is the
    //                                              "resumed from suspension" notification)
    //   OnSuspend()     -> onSuspend()           (direct rename)
    //   OnStop()        -> onCleanup()           ("tool becoming inactive" notification)
    bool SetActiveTool(InteractiveTool* tool, StartOrResume mode = StartOrResume::Start)
    {
        if (m_primitiveTool == tool)
            return true;

        // Outgoing tool: faithful itwinjs emits onSuspend (suspension notification) and
        // onCleanup (no-longer-active cleanup) on the deactivating tool.
        if (m_primitiveTool) {
            m_primitiveTool->onSuspend();
            m_primitiveTool->onCleanup();
        }

        m_primitiveTool = tool;
        if (m_primitiveTool) {
            m_primitiveTool->setActive(true);
            // Incoming tool: itwinjs splits first-install (onInstall -> onPostInstall) from
            // resume-from-suspend (onUnsuspend).
            if (mode == StartOrResume::Start) {
                // ToolAdmin.ts:1923 — every primitive tool installation resets
                // the viewport cursor to the crosshair.
                setCrossHairCursor();
                m_primitiveTool->onPostInstall();
            } else {
                m_primitiveTool->onUnsuspend();
            }
        }

        OnActiveToolChanged.Raise();
        return true;
    }

    // --- Slot accessors (Task 8) --------------------------------------------
    // Direct slot setters without the full startPrimitiveTool / startViewTool /
    // startInputCollector lifecycle (exitXxx cleanup, AccuDraw hooks, cursor
    // changes). Task 11 wires the full lifecycle for concrete view tools; for
    // Task 8 these setters establish the priority slots for motion/wheel/key
    // dispatch tests + installViewTool for Task 11's View.* tools.

    // Set the primitive tool slot directly (no lifecycle). Test affordance +
    // used by SetActiveTool above for the lifecycle path.
    // Ported from: itwinjs-core ToolAdmin._primitiveTool (ToolAdmin.ts:357) —
    //               direct slot assignment equivalent to the reference's
    //               `this._primitiveTool = newTool` line in setPrimitiveTool
    //               (ToolAdmin.ts:1669).
    void setPrimitiveTool(InteractiveTool* tool) noexcept { m_primitiveTool = tool; }

    // Install a view tool into the viewTool slot. The full startViewTool
    // lifecycle (onCleanup on prior viewTool, AccuDraw.onViewToolInstall,
    // coordLockOvr, cursor change, onActiveToolChanged raise) is wired in
    // startViewTool (Task 11); this bare setter remains for test affordance
    // (Task 8/9 tests) + as the slot-setting primitive used by startViewTool.
    // Ported from: itwinjs-core ToolAdmin.setViewTool (ToolAdmin.ts:1599-1605)
    //               — slot-only path (no onCleanup); startViewTool adds lifecycle.
    // Ownership consistency (TD-11): replacing the slot disposes any owned tool
    // already installed (onCleanup + delete) — the same contract as
    // startViewTool/exitViewTool — so an owned tool is never left dangling in
    // the registry after its stack-side caller releases it. Defined out-of-line
    // (ViewTool is forward-declared here; ToolAdmin.cpp has the complete type).
    void installViewTool(ViewTool* tool) noexcept;

    // Adopt ownership of a heap-allocated ViewTool (TD-11). Production callers
    // allocate with `new` (runViewTool) and call this BEFORE run(); ToolAdmin
    // then owns the tool — deleted on replace/exit (deferred when the tool
    // exits itself mid-run). Call disownViewTool if run() fails and the caller
    // deletes the tool instead. Tools run WITHOUT adopting (stack objects in
    // tests, bare run()) are never deleted by ToolAdmin — the TS reference
    // relies on GC for both paths; this split expresses that in C++.
    void adoptViewTool(ViewTool* tool) noexcept;

    // Release ownership taken by adoptViewTool (run() failed — the caller is
    // about to delete the tool itself).
    void disownViewTool(ViewTool* tool) noexcept;

    // Flush the deferred-delete queue (see m_deferredViewToolDeletes). Called
    // by the application at frame boundaries; tests that run self-exiting
    // tools must call it before their owning unique_ptr goes out of scope.
    void flushDeferredViewToolDeletes();

    // Start a view tool with the faithful lifecycle. Called by ViewTool::run()
    // (Task 11) to install a concrete view tool (PanViewTool, RotateViewTool,
    // ScrollViewTool, FitViewTool). Ports the load-bearing parts of
    // ToolAdmin.startViewTool (ToolAdmin.ts:1628-1654) + setViewTool (:1599-1605):
    //   1. onCleanup on the outgoing viewTool (if any, and different from newTool)
    //   2. set m_viewTool = newTool
    //   3. raise OnActiveToolChanged
    // The remaining reference lifecycle (notifications.outputPrompt,
    // accuDraw.onViewToolInstall, viewManager.endDynamicsMode,
    // suspendedByViewTool) is TODO pending those subsystems — the onCleanup-on-
    // prior + raise-event parts are the load-bearing ones for Step 3 tool dispatch.
    // (viewManager.invalidateDecorationsAllViews + toolState resets + cursor
    // change wired at ToolAdmin.cpp.)
    // Ported from: itwinjs-core ToolAdmin.startViewTool (ToolAdmin.ts:1815-1841)
    //               + setViewTool (ToolAdmin.ts:1786-1793).
    void startViewTool(ViewTool* newTool);

    // Set the view cursor on behalf of the active tool. While an incompatible-
    // viewport suspension is active (setIncompatibleViewportCursor), requests
    // are stored and applied on restore — the reference's _saveCursor mechanism.
    // Ported from: itwinjs-core ToolAdmin.setCursor (ToolAdmin.ts:2035-2040).
    void setCursor(std::string const& cursor);

    // setCursor(crossHairCursor) — out-of-line because the inline SetActiveTool
    // below cannot include Application.h (circular include).
    // Ported from: itwinjs-core ToolAdmin.startPrimitiveTool's
    //               `this.setCursor(IModelApp.viewManager.crossHairCursor)`
    //               (ToolAdmin.ts:1923).
    void setCrossHairCursor();

    // Suspend (restore=false) the cursor to "not-allowed" for tools that cannot
    // operate in the current viewport, or restore (true) the saved cursor.
    // Ported from: itwinjs-core ToolAdmin.setIncompatibleViewportCursor
    //              (ToolAdmin.ts:2163-2181).
    void setIncompatibleViewportCursor(bool restore);

    // Clear the view tool slot. Called by ViewTool::exitTool (Task 9) so the
    // faithful `IModelApp.toolAdmin.exitViewTool()` call resolves. Task 11
    // expands this to call onCleanup on the outgoing tool (faithful to
    // setViewTool(undefined) → onCleanup at ToolAdmin.ts:1789); the remaining
    // exitViewTool lifecycle (suspendedByViewTool restore, accuDraw.onViewToolExit,
    // updateDynamics) is TODO pending those subsystems.
    // (invalidateDecorationsAllViews wired at ToolAdmin.cpp — final review FIX-1.)
    // Ported from: itwinjs-core ToolAdmin.exitViewTool (ToolAdmin.ts:1795-1812)
    //               + setViewTool(undefined) (ToolAdmin.ts:1786-1793).
    void exitViewTool();

    // Set the input collector slot directly. Full startInputCollector lifecycle
    // deferred (mirrors installViewTool).
    // Ported from: itwinjs-core ToolAdmin.setInputCollector (ToolAdmin.ts:1548-1554).
    // §8.4: test-only affordance, guarded — no production caller (grep confirms
    //        only EventDispatchTest.cpp reaches this); the full lifecycle path
    //        will go through startInputCollector when it lands.
#ifdef DANQING_TESTING
    void setInputCollector(InputCollector* tool) noexcept { m_inputCollector = tool; }
#endif  // DANQING_TESTING

    // Start the default tool.
    // Ported from: itwinjs-core ToolAdmin.startDefaultTool() (ToolAdmin.ts:1792-1803)
    void StartDefaultTool();

    // Get the default tool Id.
    // Ported from: itwinjs-core ToolAdmin.defaultToolId (ToolAdmin.ts:374-375)
    const char* GetDefaultToolId() const noexcept { return m_defaultToolId; }

    // Set the default tool Id.
    // Ported from: itwinjs-core ToolAdmin.defaultToolId setter (ToolAdmin.ts:377-378)
    void SetDefaultToolId(const char* toolId) { m_defaultToolId = toolId; }

    // Whether a tool is currently active (any of the three slots is occupied).
    // Ported from: itwinjs-core ToolAdmin.activeTool !== undefined.
    bool IsToolActive() const noexcept { return activeTool() != nullptr; }

    // Get the view tool (for mouse interaction).
    // Ported from: itwinjs-core ToolAdmin.viewTool (ToolAdmin.ts:872).
    ViewTool* GetViewTool() const noexcept { return m_viewTool; }

    // Set the view tool. Transitional alias for installViewTool; kept so the
    // existing ToolAdminTest.cpp ViewTool test continues to compile.
    // §8.4: test-only affordance, guarded — no production caller (grep confirms
    //        only ToolAdminTest.cpp reaches this); production code uses
    //        installViewTool / startViewTool / exitViewTool.
#ifdef DANQING_TESTING
    void SetViewTool(ViewTool* tool) noexcept { installViewTool(tool); }
#endif  // DANQING_TESTING

    // Get the input collector.
    // Ported from: itwinjs-core ToolAdmin.inputCollector (referenced via
    //               `_inputCollector` at ToolAdmin.ts:1581, 1593, etc.).
    InputCollector* GetInputCollector() const noexcept { return m_inputCollector; }

    // Get the primitive tool.
    // Ported from: itwinjs-core ToolAdmin.primitiveTool (ToolAdmin.ts:873).
    InteractiveTool* GetPrimitiveTool() const noexcept { return m_primitiveTool; }

    // Get the idle tool.
    // Ported from: itwinjs-core ToolAdmin._idleTool (ToolAdmin.ts:358)
    InteractiveTool* GetIdleTool() const noexcept { return m_idleTool; }

    // Set the idle tool (test affordance + faithful to the reference's
    // `set idleTool(...)` property setter at ToolAdmin.ts:850-852). Production
    // code sets the idle tool once in OnInitialized (ToolAdmin.cpp assigns
    // m_idleTool directly at OnInitialized L373); tests substitute their own
    // RecordingTool here to verify the active→idle dispatch fallback.
    // Ported from: itwinjs-core ToolAdmin.idleTool setter (ToolAdmin.ts:850-852)
    // §8.4: test-only affordance, guarded — no production caller (grep confirms
    //        only EventDispatchTest.cpp reaches this; OnInitialized writes
    //        m_idleTool directly without going through this setter).
#ifdef DANQING_TESTING
    void setIdleTool(InteractiveTool* tool) noexcept { m_idleTool = tool; }
#endif  // DANQING_TESTING

    // Get the current input state (button/drag/motion tracking).
    // Ported from: itwinjs-core ToolAdmin.currentInputState (ToolAdmin.ts:348 —
    //               `public readonly currentInputState = new CurrentInputState()`).
    InputState& currentInputState() noexcept { return m_currentInputState; }
    InputState const& currentInputState() const noexcept { return m_currentInputState; }

    // The viewport the cursor is currently over (last motion event's viewport).
    // Ported from: itwinjs-core ToolAdmin.cursorView (ToolAdmin.ts:537 —
    //              `get cursorView() { return this.currentInputState.viewport; }`)。
    // InputState::viewport 由 fromPoint 在每次 motion 更新（ToolAdmin.cpp:243-265）。
    Viewport* cursorView() const noexcept { return m_currentInputState.viewport; }

    // --- Button dispatch (Task 7) -------------------------------------------
    // Public surface required so tests (and Task 8's drag end path) can drive
    // a pre-built BeButtonEvent through the active→idle routing. The reference
    // declares sendButtonEvent public (ToolAdmin.ts:1297); we mirror that.
    //
    // Public entry: send a pre-built BeButtonEvent through Data/Reset/Middle
    // routing. Called by onButtonDown / onButtonUp (Task 7) and onMouseEndDrag
    // (Task 8).
    // Ported from: itwinjs-core ToolAdmin.sendButtonEvent (ToolAdmin.ts:1297-1385)
    void sendButtonEvent(BeButtonEvent& ev);

    // Send a button-up-after-drag event through onMouseEndDrag dispatch.
    // Public (matches the reference's public sendEndDragEvent, ToolAdmin.ts:993)
    // so Task 8 tests can drive a pre-built BeButtonEvent; onButtonUp calls
    // this from its wasDragging branch.
    // Ported from: itwinjs-core ToolAdmin.sendEndDragEvent (ToolAdmin.ts:993-1008).
    void sendEndDragEvent(BeButtonEvent& ev);

    // Performs default handling of a mouse wheel event (zoom in/out). Public
    // (matches the reference's public processWheelEvent, ToolAdmin.ts:1965) so
    // the IdleTool onMouseWheel path and Task 8 tests can drive it directly.
    // Ported from: itwinjs-core ToolAdmin.processWheelEvent (ToolAdmin.ts:1965-1970).
    EventHandled processWheelEvent(BeWheelEvent& ev, bool doUpdate);

    // Called when the selected viewport changes.
    // Ported from: itwinjs-core ToolAdmin.onSelectedViewportChanged() (ToolAdmin.ts:1973)
    virtual void onSelectedViewportChanged(Viewport* previous, Viewport* current);

    // --- IDecorator (W3：活动工具 decorate 通路) ----------------------------

    // 转发活动工具的 decorate。
    // Ported from: itwinjs-core ToolAdmin.decorate (ToolAdmin.ts:2049-2070)：
    // activeTool.decorate(context)。inputCollector/primitiveTool 的 decorateSuspended
    // 子系统未移植，不 forward（见 .cpp 注释标注）。
    void Decorate(DecorateContext& context) override;

    // 装饰命中测试。参考 Decorator.testDecorationHit 为可选成员
    // （ViewManager.ts:28）；ToolAdmin 的实现转发 currentTool.testDecorationHit
    // （ToolAdmin.ts:2043），而 Tool 基类默认即 false（Tool.ts:528）——DanQing 的
    // Tool.testDecorationHit 未移植（TODO），等价实现为"无装饰命中" return false。
    bool TestDecorationHit(uint32_t featureId) const override;

    // 装饰工具提示。参考 Decorator.getDecorationToolTip 为可选成员
    // （ViewManager.ts:45）；ToolAdmin 未实现该成员（缺省即无提示）——DanQing 的
    // IDecorator 为纯虚，等价实现为返回空串。
    QString GetDecorationToolTip(uint32_t featureId) const override;

    // --- Event queue (1:1 with ToolAdmin.ts:768-843) ------------------------

    // Called from input listeners (HTML in TS, the Qt bridge in DanQing) to
    // enqueue an event for processing on the next animation frame. No-ops if
    // the event loop has not been started (TS L794-795 guard).
    // Ported from: itwinjs-core ToolAdmin.addEvent (ToolAdmin.ts:792-801)
    static void addEvent(ToolEvent ev);

#ifdef DANQING_TESTING
    // Test helper: number of events currently in the FIFO queue.
    // Ported from: itwinjs-core ToolAdmin._toolEvents.length (read access).
    static std::size_t pendingEventCount() noexcept;

    // Test helper: clear the queue.
    // Authored: no reference equivalent — DanQing test affordance so static queue
    //           state does not leak between GoogleTest cases. Not used by
    //           production code.
    static void clearQueue() noexcept;

    // Test helper: read-only peek at the front queue element.
    // Authored: no reference equivalent — DanQing test affordance so the Task 14
    //           Viewport Qt→addEvent bridge tests can assert the ToolEvent fields
    //           extracted from a QMouseEvent/QWheelEvent/QKeyEvent without
    //           processing (and thus mutating) InputState through processEvent.
    //           Returns std::nullopt when the queue is empty.
    static std::optional<ToolEvent> peekFrontEvent() noexcept;
#endif  // DANQING_TESTING

    // Process a single event from the front of the queue. Public entry point
    // invoked by Application::EventLoop (IModelApp.eventLoop equivalent).
    // Reentry-guarded: returns immediately if a previous event is still being
    // processed (TS L832 _processingEvent gate).
    // Ported from: itwinjs-core ToolAdmin.processEvent (ToolAdmin.ts:831-843)
    void processEvent();

    // Ported from: itwinjs-core ToolAdmin.onMouseLeave (ToolAdmin.ts:952-960，
    // 参考为 public——"mouseout" DOM 事件的直接处理入口）。由 Viewport::leaveEvent
    // 调用（QWidget::leaveEvent = DOM mouseout 的 Qt 对应物）。
    void onMouseLeave(Viewport* vp);

    /// 滚轮事件按帧合并（Chromium 等价层）。
    /// 参考运行环境（浏览器）在高分辨率滚轮上把一帧内的多次 OS 滚轮通知合并为
    /// 至多一个 DOM WheelEvent（对齐 rAF 派发、delta 累积）；itwinjs 源码本身
    /// 不做滚轮合并（tryReplace 仅合并 mousemove/touchmove，ToolAdmin.ts:795-806），
    /// 其 WheelEventProcessor.doZoom 以"每事件固定 ×wheelZoomRatio"消费。
    /// DanQing 的 Qt 桥每个 OS 通知都产生一个 QWheelEvent（实测一帧 3+ 个，
    /// angleDelta=-72,-11,-11,…），缺此层则同一滚动手势比 DTA 多应用数倍缩放。
    /// 本方法在每帧 processEvent 之前，把队列中同一 viewport 的全部 Wheel 事件
    /// 合并为一个（wheelDeltaY 求和，位置取首现处），复现"每帧每视口至多一次
    /// 滚轮缩放"的参考运行时契约。由 Application::EventLoop 每帧调用。
    void coalesceWheelEvents();

    // Events
    dqBase::DqEvent<> OnActiveToolChanged;

private:
    // Saved cursor while an incompatible-viewport suspension is active
    // (ToolAdmin._saveCursor, ToolAdmin.ts:111). Empty = no suspension.
    std::optional<std::string> m_saveCursor;
    // Saved toolState.locateCircleOn for the same suspension
    // (ToolAdmin._saveLocateCircle, ToolAdmin.ts:110).
    bool m_saveLocateCircle = false;
    // 视图工具挂起时被挂起工具的状态快照（ToolAdmin._suspendedByViewTool，
    // ToolAdmin.ts:106；startViewTool 建立、exitViewTool 恢复）。
    std::unique_ptr<SuspendedToolState> m_suspendedByViewTool;

    // Merge a consecutive MouseMove into the last queue element (sequential
    // moves are not interesting — keep only the latest). Returns true if the
    // merge happened, false if the event must be appended.
    // Ported from: itwinjs-core ToolAdmin.tryReplace (ToolAdmin.ts:769-779)
    //
    // The TS reference allows merge for both "mousemove" and "touchmove"
    // (L774); DanQing's ToolEventType covers only MouseMove among motion types
    // (touch events are TODO, see ToolEventType above), so the merge check
    // collapses to `type == MouseMove`.
    static bool tryReplace(ToolEvent const& ev) noexcept;

    // Pull the next event from the front of the queue (TS L786: shift()).
    // Returns nullopt if the queue is empty. When more than one event remains
    // after the pull, requests the next animation frame so EventLoop keeps
    // draining (TS L783-784).
    //
    // DanQing adaptation: the TS shift() returns `ToolEvent | undefined`; the
    // faithful C++ shape is `std::optional<ToolEvent>` by value (the queue
    // owns the element by value, and a pointer-into-std::deque front would
    // dangle after pop_front). Documented in .git/sdd/task-6-report.md.
    // Ported from: itwinjs-core ToolAdmin.getNextEvent (ToolAdmin.ts:782-787)
    static std::optional<ToolEvent> getNextEvent();

    // Dispatch the front event to the matching handler based on event.type.
    // Ported from: itwinjs-core ToolAdmin.processNextEvent (ToolAdmin.ts:804-824)
    //
    // The dispatch handlers are private stubs ({}) in Task 6 — Task 7
    // (button) and Task 8 (motion/wheel/key) implement the bodies. This keeps
    // the switch wiring + queue plumbing testable in isolation.
    void processNextEvent();

    // --- Dispatch helpers (Tasks 7-8 implement the bodies) -----------------
    // Ported from: itwinjs-core ToolAdmin dispatch methods (ToolAdmin.ts:810-818).
    // TS signatures take a `ToolEvent` (and onMouseLeave takes the viewport);
    // we mirror them 1:1. The C++ methods are non-virtual private helpers —
    // the dispatch switch in processNextEvent is the sole caller.
    //
    // Task 7 implements onMouseButton / onButtonDown / onButtonUp.
    // Task 8 implements onMouseMove / onWheel / onKeyTransition /
    // sendEndDragEvent (motion/drag/wheel/key paths). onMouseEnter/onMouseLeave
    // remain {no-op} stubs pending a Task 8+ follow-up.
    void onMouseButton(ToolEvent const& ev, bool isDown);
    void onMouseMove(ToolEvent const& ev);
    void onMouseEnter(ToolEvent const& /*ev*/) {}
    void onWheel(ToolEvent const& ev);
    void onKeyTransition(ToolEvent const& ev, bool wentDown);

    // Button-down/up dispatch helpers (Task 7). Build a BeButtonEvent from the
    // cursor position + InputState tracking, then route via sendButtonEvent
    // (down) / sendButtonEvent-or-sendEndDragEvent (up, wasDragging branch).
    // Ported from: itwinjs-core ToolAdmin.onButtonDown (ToolAdmin.ts:1387-1403)
    void onButtonDown(Viewport* vp, dqGeom::Point2d pt2d, BeButton button, InputSource inputSource);
    // Ported from: itwinjs-core ToolAdmin.onButtonUp (ToolAdmin.ts:1405-1421)
    void onButtonUp(Viewport* vp, dqGeom::Point2d pt2d, BeButton button, InputSource inputSource);

    // --- Motion / drag dispatch (Task 8) -----------------------------------
    // Ported from: itwinjs-core ToolAdmin.onMouseMove (ToolAdmin.ts:1186-1201)
    //               + onMotion (ToolAdmin.ts:1091-1173) + onStartDrag
    //               (ToolAdmin.ts:1083-1089) + updateDynamics (ToolAdmin.ts:937-991).
    //
    // The reference's onMotion is async + setTimeout-driven + snap-promise-
    // chained; Step 3 collapses it to a synchronous processMotion call. The
    // observable contract — drag-start detection by motion threshold, else
    // updateDynamics dispatching activeTool.onMouseMotion — is faithful 1:1.
    void onMotion(Viewport* vp, dqGeom::Point2d pt2d, InputSource inputSource,
                  bool forceStartDrag, dqGeom::Point2d movement);
    // Synchronous body of the reference's processMotion closure
    // (ToolAdmin.ts:1122-1153). Factored out so onMouseMove can call it
    // directly without the setTimeout / snapPromise indirection.
    void processMotion(Viewport* vp, dqGeom::Point2d pt2d, InputSource inputSource,
                       bool forceStartDrag, dqGeom::Point2d movement, BeButtonEvent& ev);
    // Dispatch onMouseStartDrag to the supplied tool if non-null + handles Yes,
    // else fall back to idleTool.onMouseStartDrag.
    // Ported from: itwinjs-core ToolAdmin.onStartDrag (ToolAdmin.ts:1083-1089).
    EventHandled onStartDrag(BeButtonEvent& ev, InteractiveTool* tool);
    // Ported from: itwinjs-core ToolAdmin.updateDynamics (ToolAdmin.ts:937-991).
    // Minimal Step 3 port: dispatches onMouseMotion to the active tool. The
    // fillEventFromCursorLocation / onDynamicFrame / inDynamicsMode paths are
    // TODO (no PrimitiveTool dynamics in Step 3) but the onMouseMotion
    // dispatch must reach the active tool.
    void updateDynamics(BeButtonEvent* ev);

    // --- Key dispatch (Task 8) ----------------------------------------------
    // Ported from: itwinjs-core ToolAdmin.onKeyTransition (ToolAdmin.ts:1474-1499)
    //               + onModifierKeyTransition (ToolAdmin.ts:1424-1432)
    //               + getModifierKey (ToolAdmin.ts:1434-1441).
    void onModifierKeyTransition(bool wentDown, BeModifierKeys modifier, uint32_t key);
    static BeModifierKeys getModifierKey(uint32_t key) noexcept;

    // Return true to filter (ignore) events to the given viewport.
    // Step 3 port: the reference additionally checks `vp.isDisposed`
    // (ToolAdmin.ts:856) which has no DanQing Viewport equivalent yet — TODO
    // pending a Viewport isDisposed port. The isCompatibleViewport path is
    // faithful 1:1 with the reference (L859-860).
    // Ported from: itwinjs-core ToolAdmin.filterViewport (ToolAdmin.ts:855-861)
    bool filterViewport(Viewport* vp) const;

    ToolRegistry m_registry;
    // Tool priority slots (Task 8). Ported from: ToolAdmin.ts:356-359.
    //   _viewTool?: ViewTool           -> m_viewTool
    //   _primitiveTool?: PrimitiveTool -> m_primitiveTool (typed InteractiveTool*
    //                                      to keep the transitional SetActiveTool
    //                                      signature compatible with RecordingTool
    //                                      test fixtures that are not PrimitiveTool
    //                                      subclasses; the reference's tighter
    //                                      PrimitiveTool typing is recovered by
    //                                      PrimitiveTool-only callers via the
    //                                      PrimitiveTool virtual surface).
    //   _inputCollector?: InputCollector -> m_inputCollector
    InteractiveTool* m_primitiveTool = nullptr;
    InteractiveTool* m_idleTool = nullptr;
    ViewTool* m_viewTool = nullptr;
    // Ownership registry: tools installed via startViewTool (heap-allocated by
    // callers, e.g. runViewTool's `new ...Tool(vp)`) are owned by ToolAdmin and
    // deleted on replace/exit. installViewTool/SetViewTool and stack-constructed
    // tools that call run() are NOT registered → never deleted. The TS reference
    // relies on GC; the C++ port needs this explicit registry (TD-11).
    std::set<ViewTool*> m_ownedViewTools;
    // Deferred delete queue: tools that exit themSELVES (exitTool() inside
    // onPostInstall, e.g. ViewUndoTool) cannot be deleted synchronously — the
    // run() call chain still executes this->... after exitViewTool returns
    // (the reference's async run() resumes on a microtask with _viewTool
    // already undefined, ViewTool.ts:100-112; a synchronous delete-in-place is
    // UB). exitViewTool enqueues them here; the app flush point
    // (flushDeferredViewToolDeletes, called at frame/test boundaries) performs
    // the actual delete. Test boundary: after any test that runs self-exiting
    // tools, call flushDeferredViewToolDeletes() before the owning unique_ptr/
    // stack object goes out of scope.
    std::vector<ViewTool*> m_deferredViewToolDeletes;
    InputCollector* m_inputCollector = nullptr;
    const char* m_defaultToolId = "Select";

    // Current input state (button/drag/motion/qualifier tracking).
    // Ported from: itwinjs-core ToolAdmin.currentInputState (ToolAdmin.ts:348)
    InputState m_currentInputState;

    // Reentry guard for processEvent.
    // Ported from: itwinjs-core ToolAdmin._processingEvent (ToolAdmin.ts:826)
    bool m_processingEvent = false;

    // Static FIFO event queue.
    // Ported from: itwinjs-core ToolAdmin._toolEvents (ToolAdmin.ts:768)
    //
    // TS uses a single static array; the queue is only touched from the
    // single-threaded event loop (HTML event listeners -> addEvent;
    // IModelApp.eventLoop -> processEvent). DanQing has the same threading
    // model (Qt event loop thread only), so no mutex is needed — matching the
    // reference 1:1.
    static std::deque<ToolEvent> s_toolEvents;
};

// ---------------------------------------------------------------------------
// WheelEventProcessor — default processor to handle wheel events.
// Ported from: itwinjs-core ToolAdmin.ts:2020-2126 (class WheelEventProcessor).
//
// Step 3 scope: orthographic doZoom only (BlankConnection views are
// orthographic — camera-off). The perspective branch (camera-on, requires
// ViewState3d.lookAt + minimumFrontDistance + pickNearestVisibleGeometry) is
// a TODO stub pending the perspective-camera port. doZoom's orthographic math
// adapts the reference's NPC-space transform to a direct ViewState3d extents
// scale about the cursor's world point (the mathematically equivalent
// operation for an orthographic view — see implementation comment).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT WheelEventProcessor {
public:
    // Ported from: itwinjs-core WheelEventProcessor.process (ToolAdmin.ts:2021-2032).
    // Returns EventHandled::Yes for consistency with ToolAdmin.processWheelEvent's
    // return contract (ToolAdmin.ts:1969).
    static EventHandled process(BeWheelEvent& ev, bool doUpdate);

private:
    // Ported from: itwinjs-core WheelEventProcessor.doZoom (ToolAdmin.ts:2034-2125).
    static EventHandled doZoom(BeWheelEvent const& ev);

    friend class ToolAdmin;
};

}  // namespace dqApp

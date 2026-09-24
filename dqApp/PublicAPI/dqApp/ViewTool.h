// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewTool / ViewManip / ViewingToolHandle / ViewHandleArray bases
//                + HandleWithInertia / AnimatedHandle / ViewPan / ViewRotate /
//                ViewScroll handles (Task 10).
//                + PanViewTool / RotateViewTool / ScrollViewTool / FitViewTool
//                concrete tools (Task 11).
//
// Ported from: itwinjs-core core/frontend/src/tools/ViewTool.ts
//              ViewHandleType enum         (:58-69)
//              ViewManipPriority enum      (:72-77)
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
//
// Reference conventions (CLAUDE.md §0/§3): TS class/method names preserved as
// camelCase (this is a TS port); member variables use m_/s_ + camelCase; enum
// class with explicit underlying type; unique_ptr for handle ownership (no
// shared_ptr per CLAUDE.md §9).
//
// Task 9 scope discipline:
//   - Fully ported (state-machine + logic): onDataButtonDown/Up, onMouseWheel,
//     startHandleDrag, onMouseStartDrag/EndDrag, onMouseMotion, processPoint,
//     processFirstPoint, beginDynamicUpdate/endDynamicUpdate,
//     setTargetCenterWorld, onReinitialize, exitTool, run, changeViewport.
//   - Stubbed with `// TODO` per §0 (draw/preview paths): decorate,
//     previewDepthPoint, pickDepthPoint (returns input point — Viewport
//     pickDepthPoint is itself a Step 4 stub), drawHandles, getDepthPointGeometryId,
//     clearDepthPoint, focusIn cursor setting, provideToolAssistance UI.
//
// Task 10 scope discipline (this file):
//   - Fully ported (perform/animate/firstPoint/doManipulation math):
//     HandleWithInertia, AnimatedHandle, ViewPan, ViewRotate, ViewScroll.
//   - Inertia: time-decay via DqDuration/DqTimePoint (ported from
//     imodel-native BeDuration/BeTimePoint — faithful equivalent of the TS
//     BeDuration/BeTimePoint used by the reference). Structure 1:1 with the
//     reference; the only adaptation is the time-machinery class names.
//   - Stubbed: alignToGlobe (not ported), viewingGlobe (not ported → false),
//     view.getUpVector(pt) (not ported → UnitZ, faithful to the no-globe
//     reference branch), CursorView check in AnimatedHandle.animate (not
//     ported → no-op), viewCmdTargetCenter, saveViewUndo.
#pragma once

#include "Export.h"
#include "Animator.h"  // Animator interface (HandleWithInertia, AnimatedHandle).
#include "ToolSettings.h"  // ToolSettings::*（原 k* 常量的全量可变移植）
#include "Viewport.h"  // DepthPointSource（参考属 Viewport.ts 所有）

#include <dqGeom/Plane3dByOriginAndUnitNormal.h>  // adjustDepthPoint 的平面参数类型

#include "dqApp/StandardView.h"  // StandardViewId (StandardViewTool ctor param).
#include "dqApp/ToolAdmin.h"  // BeButton, BeButtonEvent, BeWheelEvent, EventHandled,
                              // InputSource, CoordSource, BeModifierKeys,
                              // InteractiveTool, Tool.

#include <dqBase/DqTime.h>  // DqDuration, DqTimePoint (faithful equivalent of TS BeDuration/BeTimePoint).

#include <dqCommon/ColorDef.h>
#include <dqCommon/Frustum.h>

#include <dqGeom/Angle.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dqRender {
class RenderGraphic;       // WindowAreaTool 橡皮筋图形（所有权见类内注释）
class RenderGraphicOwner;
}

namespace dqApp {

class Viewport;
class DecorateContext;
class ViewManip;  // forward — ViewingToolHandle holds a ViewManip* back-reference.
// ---------------------------------------------------------------------------
// ViewHandleType — bit-flag enumeration of viewing handle kinds.
// Ported from: itwinjs-core ViewHandleType (ViewTool.ts:58-69).
//
// Used as a bitmask in ViewManip.handleMask to request specific handle kinds
// (ViewManip ctor then instantiates the matching ViewingToolHandle subclasses
// in changeViewport — Task 10).
//
// Underlying type is uint16_t because LookAndMove = 1<<8 = 256 exceeds uint8_t.
// ---------------------------------------------------------------------------
enum class ViewHandleType : uint16_t {
    None         = 0,
    Rotate       = 1,
    TargetCenter = 1 << 1,
    Pan          = 1 << 2,
    Scroll       = 1 << 3,
    Zoom         = 1 << 4,
    Walk         = 1 << 5,
    Fly          = 1 << 6,
    Look         = 1 << 7,
    LookAndMove  = 1 << 8,
};

// Bitwise OR/AND for ViewHandleType (bitmask type per CLAUDE.md §9 enum class).
inline ViewHandleType operator|(ViewHandleType a, ViewHandleType b) noexcept
{
    return static_cast<ViewHandleType>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}
inline ViewHandleType operator&(ViewHandleType a, ViewHandleType b) noexcept
{
    return static_cast<ViewHandleType>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}
inline bool anyHandle(ViewHandleType v) noexcept { return v != ViewHandleType::None; }

// ---------------------------------------------------------------------------
// ViewManipPriority — relative priority of handle hit tests.
// Ported from: itwinjs-core ViewManipPriority (ViewTool.ts:72-77).
//
// When multiple handles overlap at the cursor, the higher-priority handle wins
// (with nearest-distance as the tiebreaker within a priority class).
// Marked @internal in the reference.
// ---------------------------------------------------------------------------
enum class ViewManipPriority : uint32_t {
    Low    = 1,
    Normal = 10,
    Medium = 100,
    High   = 1000,
};

// ---------------------------------------------------------------------------
// ViewHandleWeight — line weights (pixels) for view-handle / window-area graphics.
// Ported from: itwinjs-core ViewHandleWeight (ViewTool.ts:43-49, `const enum` —
//              §3.4 映射为 enum class + 显式底层类型；值 1:1)。
// WindowAreaTool 用 Thin（橡皮筋描边）与 FatDot（首点标记，ViewTool.ts:3702-3706）。
// ---------------------------------------------------------------------------
enum class ViewHandleWeight : uint32_t {
    Thin     = 1,
    Normal   = 2,
    Bold     = 3,
    VeryBold = 4,
    FatDot   = 8,
};

// ---------------------------------------------------------------------------
// HitOut — out-parameter shape for ViewingToolHandle.testHandleForHit.
// Ported from: itwinjs-core testHandleForHit out-param shape
//              `{ distance: number, priority: ViewManipPriority }` (ViewTool.ts:144).
// ---------------------------------------------------------------------------
struct HitOut {
    double distance = 0.0;
    ViewManipPriority priority = ViewManipPriority::Normal;
};

// ---------------------------------------------------------------------------
// Plane3dByOriginAndUnitNormal — adjustDepthPoint 的平面参数类型。
// Ported from: itwinjs-core Plane3dByOriginAndUnitNormal（@itwin/core-geometry；
// dqGeom 已完成全量移植，按 TODO 承诺换接真身，删除两字段桩）。
// ---------------------------------------------------------------------------
using Plane3dByOriginAndUnitNormal = dqGeom::Plane3dByOriginAndUnitNormal;

// ---------------------------------------------------------------------------
// ToolSettings — the view-related subset this header used to inline as k*
// constexpr constants now lives in its full 1:1 port dqApp/ToolSettings.h
// (mutable static class, ToolSettings.ts field-by-field — the DiagnosticsPanel
// ToolSettingsTracker edits these values at runtime). Includes below provide
// the name to all users of this header.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// ViewingToolHandle — abstract base for individual view handle kinds
// (rotate, pan, zoom, etc.).
// Ported from: itwinjs-core ViewingToolHandle (ViewTool.ts:129-189).
//
// Concrete handle subclasses (ViewRotate, ViewPan, ViewScroll, ViewZoom,
// ViewLook, ViewWalk, ViewFly, ViewLookAndMove, ViewTargetCenter) land in
// Task 10. The base provides default no-op behavior for most events; each
// concrete handle overrides doManipulation / firstPoint / testHandleForHit /
// handleType plus whichever defaults it needs to specialize.
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewingToolHandle {
public:
    // Ported from: itwinjs-core ViewingToolHandle constructor (ViewTool.ts:133-135).
    explicit ViewingToolHandle(ViewManip* vm) noexcept : viewTool(vm) {}
    virtual ~ViewingToolHandle() = default;

    // C++ 适配（-fno-rtti 无 instanceof）：既是 ViewingToolHandle 又是 Animator 的
    // 句柄（HandleWithInertia 家族）回答自身 Animator 面；其余 nullptr。
    // 用途：ViewManip::onCleanup 把装机惯性动画器的所有权让渡给视口（TS GC 承载）。
    virtual class Animator* asAnimator() noexcept { return nullptr; }

    // Ported from: itwinjs-core ViewingToolHandle.viewTool (ViewTool.ts:133).
    // Public field in the reference; kept public so handle subclasses can call
    // viewTool->viewport, viewTool->setTargetCenterWorld, viewTool->beginDynamicUpdate, etc.
    ViewManip* viewTool;

    // --- Virtual surface 1:1 with reference (ViewTool.ts:136-171) ---

    // Ported from: itwinjs-core ViewingToolHandle.onReinitialize (ViewTool.ts:136).
    virtual void onReinitialize() {}
    // Ported from: itwinjs-core ViewingToolHandle.onCleanup (ViewTool.ts:137).
    virtual void onCleanup() {}
    // Ported from: itwinjs-core ViewingToolHandle.focusOut (ViewTool.ts:138).
    virtual void focusOut() {}
    // Ported from: itwinjs-core ViewingToolHandle.motion (ViewTool.ts:139).
    virtual bool motion(BeButtonEvent const&) { return false; }
    // Ported from: itwinjs-core ViewingToolHandle.checkOneShot (ViewTool.ts:140).
    virtual bool checkOneShot() const { return true; }
    // Ported from: itwinjs-core ViewingToolHandle.getHandleCursor (ViewTool.ts:141).
    virtual std::string getHandleCursor() const { return "default"; }

    // Pure virtual (abstract in reference: ViewTool.ts:142-145).
    // Ported from: itwinjs-core ViewingToolHandle.doManipulation (ViewTool.ts:142).
    virtual bool doManipulation(BeButtonEvent const& ev, bool inDynamics) = 0;
    // Ported from: itwinjs-core ViewingToolHandle.firstPoint (ViewTool.ts:143).
    virtual bool firstPoint(BeButtonEvent const& ev) = 0;
    // Ported from: itwinjs-core ViewingToolHandle.testHandleForHit (ViewTool.ts:144).
    virtual bool testHandleForHit(dqGeom::Point3d ptScreen, HitOut& out) = 0;
    // Ported from: itwinjs-core ViewingToolHandle.handleType getter (ViewTool.ts:145).
    virtual ViewHandleType handleType() const = 0;

    // Ported from: itwinjs-core ViewingToolHandle.focusIn (ViewTool.ts:146).
    // Out-of-line (ViewTool.cpp): routes through ToolAdmin::setCursor — the
    // cursor state machine's handle-focus path (e.g. the rotate tool's handle
    // focus shows the rotate bitmap cursor).
    virtual void focusIn();

    // Ported from: itwinjs-core ViewingToolHandle.drawHandle (ViewTool.ts:147).
    // Stub body — concrete handle decoration rendering is Task 10 / Task 15.
    virtual void drawHandle(DecorateContext& /*context*/, bool /*hasFocus*/) {}

    // Ported from: itwinjs-core ViewingToolHandle.onWheel (ViewTool.ts:148).
    virtual bool onWheel(BeWheelEvent const&) { return false; }

    // Ported from: itwinjs-core ViewingToolHandle.needDepthPoint (ViewTool.ts:158).
    virtual bool needDepthPoint(BeButtonEvent const&, bool /*isPreview*/) { return false; }

    // Ported from: itwinjs-core ViewingToolHandle.adjustDepthPoint (ViewTool.ts:159-171).
    virtual bool adjustDepthPoint(bool isValid, Viewport* /*vp*/,
                                  Plane3dByOriginAndUnitNormal /*plane*/,
                                  DepthPointSource source);

protected:
    // Ported from: itwinjs-core ViewingToolHandle._lastPtNpc (ViewTool.ts:130).
    dqGeom::Point3d m_lastPtNpc;
    // Ported from: itwinjs-core ViewingToolHandle._depthPoint (ViewTool.ts:131).
    std::optional<dqGeom::Point3d> m_depthPoint;

    // Ported from: itwinjs-core ViewingToolHandle.pickDepthPoint (ViewTool.ts:172-174).
    void pickDepthPoint(BeButtonEvent const& ev);

    // Ported from: itwinjs-core ViewingToolHandle.changeFocusFromDepthPoint
    //              (ViewTool.ts:176-186).
    // Stub: ViewState3d.changeFocusFromPoint not yet ported.
    void changeFocusFromDepthPoint() { /* TODO: view.changeFocusFromPoint — future task. */ }
};

// ---------------------------------------------------------------------------
// ViewHandleArray — collection of ViewingToolHandle owned by a ViewManip.
// Ported from: itwinjs-core ViewHandleArray (ViewTool.ts:190-306).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewHandleArray {
public:
    // Ported from: itwinjs-core ViewHandleArray constructor (ViewTool.ts:195).
    explicit ViewHandleArray(ViewManip* vm) noexcept : viewTool(vm) {}

    // Ported from: itwinjs-core ViewHandleArray.handles (ViewTool.ts:191).
    std::vector<std::unique_ptr<ViewingToolHandle>> handles;
    // Ported from: itwinjs-core ViewHandleArray.focus (ViewTool.ts:192).
    int focus = -1;
    // Ported from: itwinjs-core ViewHandleArray.focusDrag (ViewTool.ts:193).
    bool focusDrag = false;
    // Ported from: itwinjs-core ViewHandleArray.hitHandleIndex (ViewTool.ts:194).
    int hitHandleIndex = 0;

    // Ported from: itwinjs-core ViewHandleArray.empty (ViewTool.ts:197-202).
    void empty() noexcept
    {
        focus = -1;
        focusDrag = false;
        hitHandleIndex = -1;  // -1 → onReinitialize is called before testHit sets the index.
        handles.clear();
    }

    // Ported from: itwinjs-core ViewHandleArray.count getter (ViewTool.ts:204).
    int count() const noexcept { return static_cast<int>(handles.size()); }

    // Ported from: itwinjs-core ViewHandleArray.hitHandle getter (ViewTool.ts:205).
    ViewingToolHandle* hitHandle() const noexcept { return getByIndex(hitHandleIndex); }
    // Ported from: itwinjs-core ViewHandleArray.focusHandle getter (ViewTool.ts:206).
    ViewingToolHandle* focusHandle() const noexcept { return getByIndex(focus); }

    // Ported from: itwinjs-core ViewHandleArray.add (ViewTool.ts:207).
    void add(std::unique_ptr<ViewingToolHandle> h) { handles.push_back(std::move(h)); }

    // 按裸指针取出（不删除）——工具退出时把装机惯性动画器的所有权让渡给视口
    // （TS GC 语义：动画槽引用保活直到跑完；C++ 须显式移交，否则 empty() 释放
    // 句柄 → m_animatorRef 悬空 → 下一帧 use-after-free）。未命中返回 nullptr。
    std::unique_ptr<ViewingToolHandle> release(ViewingToolHandle* raw) noexcept
    {
        for (auto it = handles.begin(); it != handles.end(); ++it) {
            if (it->get() == raw) {
                std::unique_ptr<ViewingToolHandle> out = std::move(*it);
                handles.erase(it);
                return out;
            }
        }
        return nullptr;
    }

    // Ported from: itwinjs-core ViewHandleArray.getByIndex (ViewTool.ts:208).
    ViewingToolHandle* getByIndex(int index) const noexcept
    {
        return (index >= 0 && index < count()) ? handles[index].get() : nullptr;
    }

    // Ported from: itwinjs-core ViewHandleArray.focusHitHandle (ViewTool.ts:209).
    void focusHitHandle() { setFocus(hitHandleIndex); }

    // Ported from: itwinjs-core ViewHandleArray.testHit (ViewTool.ts:211-245).
    bool testHit(dqGeom::Point3d ptScreen, ViewHandleType forced = ViewHandleType::None);

    // Ported from: itwinjs-core ViewHandleArray.drawHandles (ViewTool.ts:247-264).
    void drawHandles(DecorateContext& context);

    // Ported from: itwinjs-core ViewHandleArray.setFocus (ViewTool.ts:266-289).
    void setFocus(int index);

    // Ported from: itwinjs-core ViewHandleArray.onReinitialize (ViewTool.ts:291).
    void onReinitialize();
    // Ported from: itwinjs-core ViewHandleArray.onCleanup (ViewTool.ts:292).
    void onCleanup();
    // Ported from: itwinjs-core ViewHandleArray.motion (ViewTool.ts:293).
    void motion(BeButtonEvent const& ev);
    // Ported from: itwinjs-core ViewHandleArray.onWheel (ViewTool.ts:294-301).
    bool onWheel(BeWheelEvent const& ev);
    // Ported from: itwinjs-core ViewHandleArray.hasHandle (ViewTool.ts:304).
    bool hasHandle(ViewHandleType handleType) const;

private:
    // Back-reference to the owning ViewManip. Ported from: itwinjs-core
    // ViewHandleArray.viewTool (ViewTool.ts:195 — ctor param).
    ViewManip* viewTool;
};

// ---------------------------------------------------------------------------
// ViewTool — abstract base for tools that manipulate a view.
// Ported from: itwinjs-core ViewTool (ViewTool.ts:92-128).
//
// Subclasses (ViewManip, FitViewTool, StandardViewTool, WindowAreaTool, etc.)
// override run(), changeViewport(), decorate() to provide concrete behavior.
// ViewManip (below) is the frustum-manipulator base; concrete single-operation
// view tools (PanViewTool, RotateViewTool, ...) derive from ViewManip.
//
// Reference semantics: ViewTool's constructor is `public constructor(public
// viewport?: ScreenViewport)` — public ctor with optional viewport. C++ uses a
// pointer defaulting to nullptr to mirror `undefined`. The reference marks the
// class `abstract` (no abstract methods); C++ cannot mechanically enforce
// "abstract without abstract methods", so this class is concrete but documented
// as a base — concrete tools derive from ViewManip, not ViewTool directly.
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewTool : public InteractiveTool {
public:
    // Ported from: itwinjs-core ViewTool.inDynamicUpdate (ViewTool.ts:95).
    bool inDynamicUpdate = false;

    // Ported from: itwinjs-core ViewTool.viewport (ViewTool.ts:113 ctor param).
    // Public field — subclasses, ToolAdmin and ToolAdmin dispatch reach in.
    Viewport* viewport = nullptr;

    // Ported from: itwinjs-core ViewTool constructor (ViewTool.ts:113-115).
    // `explicit` guards against implicit conversion from Viewport*; a single
    // default arg keeps the no-arg form (`ViewTool vt;`) working for existing
    // callers (ToolAdminTest).
    explicit ViewTool(Viewport* vp = nullptr) noexcept : viewport(vp) {}

    // Ported from: itwinjs-core ViewTool.translate (ViewTool.ts:93).
    static std::string translate(std::string const& val);

    // Ported from: itwinjs-core ViewTool.beginDynamicUpdate (ViewTool.ts:96).
    void beginDynamicUpdate() noexcept { inDynamicUpdate = true; }
    // Ported from: itwinjs-core ViewTool.endDynamicUpdate (ViewTool.ts:97).
    void endDynamicUpdate() noexcept { inDynamicUpdate = false; }

    // Ported from: itwinjs-core ViewTool.run (ViewTool.ts:98-111).
    bool run() override;

    // Ported from: itwinjs-core ViewTool.onResetButtonUp (ViewTool.ts:116-119).
    EventHandled onResetButtonUp(BeButtonEvent const& ev) override;

    // Ported from: itwinjs-core ViewTool.exitTool (ViewTool.ts:122).
    void exitTool() override;

    // Ported from: itwinjs-core ViewTool.showPrompt (ViewTool.ts:123-125).
    static void showPrompt(std::string const& prompt);

    // Ported from: itwinjs-core InteractiveTool.decorate (called by DecorateContext).
    // ViewTool itself does not decorate; the virtual is here so ViewManip can override.
    virtual void decorate(DecorateContext& /*context*/) {}

    // Ported from: itwinjs-core ViewTool.changeViewport (referenced via
    //              ViewManip.changeViewport override at ViewTool.ts:895-934).
    // Base implementation is a simple viewport setter; ViewManip overrides to
    // rebuild its handle array on viewport change.
    virtual void changeViewport(Viewport* vp) noexcept { viewport = vp; }

    // Unique id placeholder; concrete subclasses (PanViewTool, RotateViewTool,
    // FitViewTool, ...) override with their toolId. TestManip uses this default.
    const char* getToolId() const noexcept override { return "ViewTool"; }
};

// ---------------------------------------------------------------------------
// ViewManip — base class for tools that manipulate the frustum of a Viewport
// via a set of ViewingToolHandles.
// Ported from: itwinjs-core ViewManip (ViewTool.ts:307-935).
//
// This class owns a ViewHandleArray (targetCenter, rotate, pan, zoom, etc.)
// and routes input events (button, wheel, motion, drag) through the active
// handle. Concrete subclasses (PanViewTool, RotateViewTool, LookViewTool,
// ScrollViewTool, ZoomViewTool, LookAndMoveTool, WalkViewTool, FlyViewTool)
// only override the constructor (to pick a handleMask) +
// isExitAllowedOnReinitialize + provideInitialToolAssistance.
//
// Like ViewTool above, the reference marks this `abstract class` even though
// none of its methods are abstract. The C++ port is therefore concrete;
// concrete single-operation view tools derive from this class.
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewManip : public ViewTool {
public:
    // --- Public state (1:1 with reference, ViewTool.ts:312-326) ---

    // Ported from: itwinjs-core ViewManip.viewHandles (ViewTool.ts:313).
    // Note: ViewManip is non-copyable (ViewHandleArray holds a back-pointer to
    // this); the viewHandles field is move-only via the implicitly-deleted copy
    // ctor from the unique_ptr vector inside.
    ViewHandleArray viewHandles;
    // Ported from: itwinjs-core ViewManip.frustumValid (ViewTool.ts:314).
    bool frustumValid = false;
    // Ported from: itwinjs-core ViewManip.targetCenterWorld (ViewTool.ts:315).
    dqGeom::Point3d targetCenterWorld;
    // Ported from: itwinjs-core ViewManip.inHandleModify (ViewTool.ts:316).
    bool inHandleModify = false;
    // Ported from: itwinjs-core ViewManip.isDragging (ViewTool.ts:317).
    bool isDragging = false;
    // Ported from: itwinjs-core ViewManip.targetCenterValid (ViewTool.ts:318).
    bool targetCenterValid = false;
    // Ported from: itwinjs-core ViewManip.targetCenterLocked (ViewTool.ts:319).
    bool targetCenterLocked = false;
    // Ported from: itwinjs-core ViewManip.nPts (ViewTool.ts:320).
    int nPts = 0;
    // Ported from: itwinjs-core ViewManip.forcedHandle (ViewTool.ts:322).
    ViewHandleType forcedHandle = ViewHandleType::None;

    // Ported from: itwinjs-core ViewManip ctor params (ViewTool.ts:328).
    uint32_t handleMask = 0;
    bool oneShot = false;
    bool isDraggingRequired = false;

    // Ported from: itwinjs-core ViewManip constructor (ViewTool.ts:328-332).
    ViewManip(Viewport* vp, uint32_t handleMask_, bool oneShot_, bool isDraggingRequired_ = false);

    // --- Event dispatch overrides (ViewTool.ts:462-557) ---

    // Ported from: itwinjs-core ViewManip.onDataButtonDown (ViewTool.ts:462-487).
    EventHandled onDataButtonDown(BeButtonEvent const& ev) override;
    // Ported from: itwinjs-core ViewManip.onDataButtonUp (ViewTool.ts:489-494).
    EventHandled onDataButtonUp(BeButtonEvent const& ev) override;
    // Ported from: itwinjs-core ViewManip.onMouseWheel (ViewTool.ts:496-503).
    EventHandled onMouseWheel(BeWheelEvent& ev) override;
    // Ported from: itwinjs-core ViewManip.startHandleDrag (ViewTool.ts:506-523).
    // `forcedHandle` uses std::optional to mirror the TS `forcedHandle?: ViewHandleType`
    // optional param (std::nullopt === undefined in the reference's hasValue check).
    EventHandled startHandleDrag(BeButtonEvent const& ev,
                                 std::optional<ViewHandleType> forcedHandle = std::nullopt);
    // Ported from: itwinjs-core ViewManip.onMouseStartDrag (ViewTool.ts:525-529).
    EventHandled onMouseStartDrag(BeButtonEvent const& ev) override;
    // Ported from: itwinjs-core ViewManip.onMouseEndDrag (ViewTool.ts:531-537).
    EventHandled onMouseEndDrag(BeButtonEvent const& ev) override;
    // Ported from: itwinjs-core ViewManip.onMouseMotion (ViewTool.ts:539-557).
    void onMouseMotion(BeButtonEvent const& ev) override;

    // Ported from: itwinjs-core ViewManip.onReinitialize (ViewTool.ts:441-460).
    void onReinitialize() override;
    // Ported from: itwinjs-core ViewManip.onPostInstall (ViewTool.ts:623-626).
    void onPostInstall() override;
    // Ported from: itwinjs-core ViewManip.onCleanup (ViewTool.ts:660-681).
    void onCleanup() override;

    // --- Viewport / target management ---

    // Ported from: itwinjs-core ViewManip.changeViewport (ViewTool.ts:895-934).
    void changeViewport(Viewport* vp) noexcept override;
    // Ported from: itwinjs-core ViewManip.setTargetCenterWorld (ViewTool.ts:689-701).
    void setTargetCenterWorld(dqGeom::Point3d const& pt, bool lockTarget, bool saveTarget);
    // Ported from: itwinjs-core ViewManip.updateTargetCenter (ViewTool.ts:703-736).
    void updateTargetCenter();

    // --- View handle state-machine helpers (virtual for test override) ---

    // Ported from: itwinjs-core ViewManip.processFirstPoint (ViewTool.ts:738-751).
    virtual bool processFirstPoint(BeButtonEvent const& ev);
    // Ported from: itwinjs-core ViewManip.processPoint (ViewTool.ts:753-760).
    virtual bool processPoint(BeButtonEvent const& ev, bool inDynamics);

    // --- Decoration (stubbed per Task 9 scope) ---

    // Ported from: itwinjs-core ViewManip.decorate (ViewTool.ts:334-337).
    void decorate(DecorateContext& context) override;
    // Ported from: itwinjs-core ViewManip.previewDepthPoint (ViewTool.ts:340-374) —
    // 深度预览圆（WorldOverlay 椭圆）+ ViewTargetCenter 十字。
    void previewDepthPoint(DecorateContext& context);
    // Ported from: itwinjs-core ViewManip.getDepthPointGeometryId (ViewTool.ts:377-381).
    std::optional<std::string> getDepthPointGeometryId() const;
    // Ported from: itwinjs-core ViewManip.clearDepthPoint (ViewTool.ts:384-389).
    bool clearDepthPoint() noexcept;
    // Ported from: itwinjs-core ViewManip.pickDepthPoint (ViewTool.ts:392-431) —
    // 经 Viewport::pickDepthPoint 真深度回读（featureId + depthAndOrder 附件）。
    std::optional<dqGeom::Point3d> pickDepthPoint(BeButtonEvent const& ev, bool isPreview = false);

    // Ported from: itwinjs-core ViewManip.provideToolAssistance (ViewTool.ts:628-655).
    // Stub body — ToolAssistance UI is not yet ported.
    void provideToolAssistance(std::string const& /*mainInstrKey*/) const {}

    // --- Static view helpers (1:1 with reference, ViewTool.ts:762-807) ---

    // Ported from: itwinjs-core ViewManip.lensAngleMatches (ViewTool.ts:762-767).
    bool lensAngleMatches(dqGeom::Angle const& angle, double tolerance) const;
    // Ported from: itwinjs-core ViewManip.isZUp (ViewTool.ts:769-778).
    bool isZUp() const;
    // Ported from: itwinjs-core ViewManip.getFocusPlaneNpc (ViewTool.ts:780-783).
    static double getFocusPlaneNpc(Viewport const& vp);
    // Ported from: itwinjs-core ViewManip.getDefaultTargetPointWorld (ViewTool.ts:785-798).
    static dqGeom::Point3d getDefaultTargetPointWorld(Viewport const& vp);
    // Ported from: itwinjs-core ViewManip.isPointVisible (ViewTool.ts:801-807).
    bool isPointVisible(dqGeom::Point3d const& testPt) const;

    // Override of ViewTool.getToolId — concrete subclasses (PanViewTool,
    // RotateViewTool, ...) override with their toolId. TestManip uses this default.
    const char* getToolId() const noexcept override { return "ViewManip"; }

    // ViewManip is non-copyable: ViewHandleArray holds a back-pointer to `this`,
    // and the unique_ptr vector inside is move-only. The reference's TS class is
    // also never copied (constructed once per tool invocation).
    ViewManip(ViewManip const&) = delete;
    ViewManip& operator=(ViewManip const&) = delete;

protected:
    // Ported from: itwinjs-core ViewManip.isExitAllowedOnReinitialize (ViewTool.ts:439).
    // A tool must opt in to allowing ViewTool.exitTool to be called from
    // onReinitialize by overriding this to return true.
    virtual bool isExitAllowedOnReinitialize() const noexcept { return false; }
    // Ported from: itwinjs-core ViewManip.provideInitialToolAssistance (ViewTool.ts:658).
    // Called from onReinitialize; concrete subclasses set up first-point prompts.
    virtual void provideInitialToolAssistance() {}

private:
    // Ported from: itwinjs-core ViewManip._startPose (ViewTool.ts:326).
    // TODO: std::optional<ViewPose> m_startPose — wire when ViewPose lands.

    // Ported from: itwinjs-core ViewManip._depthPreview (ViewTool.ts:325-326)。
    // 预览态的深度点记录：previewDepthPoint 据此画圆+十字。
    struct DepthPreview {
        dqGeom::Point3d testPoint;          // 拾取请求点（ev.rawPoint）
        double pickRadius = 0.0;            // 像素半径（pickRadiusPixels）
        dqGeom::Point3d origin = dqGeom::Point3d::From(0, 0, 0);  // plane.origin
        dqGeom::Vector3d normal = dqGeom::Vector3d::From(0, 0, 1);// plane.normal
        DepthPointSource source = DepthPointSource::TargetPoint;
        bool isDefaultDepth = false;        // 无效深度 → 圆面向视图（红色警示）
        std::optional<uint32_t> sourceId;   // 几何命中的 elementId（若有）
    };
    std::optional<DepthPreview> m_depthPreview;
};

// ---------------------------------------------------------------------------
// HandleWithInertia — abstract Animator base for handles that continue briefly
// after the gesture ends (pan, rotate).
// Ported from: itwinjs-core HandleWithInertia (ViewTool.ts:1042-1104).
//
// Reference semantics: doManipulation computes an NPC-space inertia vector on
// each motion; when the gesture ends (!inDynamics) and inertia is enabled,
// beginAnimation() installs this handle as the Viewport's animator. animate()
// then continues the operation with a time-decaying fraction of the last
// inertia vector.
//
// Time machinery: the TS reference uses BeDuration / BeTimePoint; the C++ port
// uses dqBase::DqDuration / dqBase::DqTimePoint — the DanQing 1:1 port of those
// imodel-native types (DqTime.h), so semantics are preserved (IsTowardsFuture,
// Now(), milliseconds conversion).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT HandleWithInertia : public ViewingToolHandle, public Animator {
public:
    // Ported from: itwinjs-core HandleWithInertia constructor (no extra state
    // beyond ViewingToolHandle; reference has no explicit ctor).
    explicit HandleWithInertia(ViewManip* vm) noexcept : ViewingToolHandle(vm) {}
    ~HandleWithInertia() override = default;
    // ← ViewingToolHandle::asAnimator（MI 双基——返回 Animator 面）
    Animator* asAnimator() noexcept override { return this; }

    // Ported from: itwinjs-core HandleWithInertia.doManipulation (ViewTool.ts:1047-1064).
    bool doManipulation(BeButtonEvent const& ev, bool inDynamics) override;

    // Ported from: itwinjs-core HandleWithInertia.animate (ViewTool.ts:1079-1100).
    // Animator interface.
    bool animate() override;
    // Ported from: itwinjs-core HandleWithInertia.interrupt (ViewTool.ts:1102).
    // No-op in the reference.
    void interrupt() override {}

protected:
    // Ported from: itwinjs-core HandleWithInertia.beginAnimation (ViewTool.ts:1067-1076).
    bool beginAnimation();

    // Ported from: itwinjs-core HandleWithInertia.perform (ViewTool.ts:1103, abstract).
    virtual bool perform(dqGeom::Point3d thisPtNpc) = 0;

    // Ported from: itwinjs-core HandleWithInertia._duration (ViewTool.ts:1043).
    dqBase::DqDuration m_duration;
    // Ported from: itwinjs-core HandleWithInertia._end (ViewTool.ts:1044).
    dqBase::DqTimePoint m_end;
    // Ported from: itwinjs-core HandleWithInertia._inertiaVec (ViewTool.ts:1045).
    std::optional<dqGeom::Vector3d> m_inertiaVec;
};

// ---------------------------------------------------------------------------
// AnimatedHandle — abstract base for handles that animate a frustum change
// based on cursor position relative to an anchor point (scroll, walk, fly).
// Ported from: itwinjs-core AnimatedHandle (ViewTool.ts:1409-1497).
//
// Reference semantics: firstPoint captures the anchor and installs this handle
// as the Viewport's animator; animate() runs each frame while the cursor is in
// the dead-zone-outer region, computing a direction vector from anchor→cursor
// and feeding it (scaled by elapsed time) to the concrete handle's operation.
//
// Like HandleWithInertia, AnimatedHandle derives from Animator so it can be
// installed via Viewport::setAnimator(this) — matching the TS reference's
// structural-typing setAnimator call at ViewTool.ts:1461.
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT AnimatedHandle : public ViewingToolHandle, public Animator {
public:
    explicit AnimatedHandle(ViewManip* vm) noexcept : ViewingToolHandle(vm) {}
    ~AnimatedHandle() override = default;

    // Ported from: itwinjs-core AnimatedHandle.testHandleForHit (ViewTool.ts:1415-1419).
    bool testHandleForHit(dqGeom::Point3d ptScreen, HitOut& out) override;
    // Ported from: itwinjs-core AnimatedHandle.doManipulation (ViewTool.ts:1427-1430).
    bool doManipulation(BeButtonEvent const& ev, bool inDynamics) override;
    // Ported from: itwinjs-core AnimatedHandle.interrupt (ViewTool.ts:1433).
    void interrupt() override {}
    // Ported from: itwinjs-core AnimatedHandle.animate (ViewTool.ts:1434-1440).
    bool animate() override;
    // Ported from: itwinjs-core AnimatedHandle.firstPoint (ViewTool.ts:1442-1463).
    bool firstPoint(BeButtonEvent const& ev) override;
    // Ported from: itwinjs-core AnimatedHandle.onReinitialize (ViewTool.ts:1482-1488).
    void onReinitialize() override;
    // Ported from: itwinjs-core AnimatedHandle.onWheel (ViewTool.ts:1491-1496).
    bool onWheel(BeWheelEvent const& ev) override;

protected:
    // Ported from: itwinjs-core AnimatedHandle.getElapsedTime (ViewTool.ts:1421-1425).
    double getElapsedTime();
    // Ported from: itwinjs-core AnimatedHandle.getDirection (ViewTool.ts:1465-1469).
    // Virtual so concrete handles can override (ViewScroll does not, but the
    // reference marks it `protected` and the method is part of the extension
    // surface — kept virtual for parity).
    virtual std::optional<dqGeom::Vector3d> getDirection();
    // Ported from: itwinjs-core AnimatedHandle.getInputVector (ViewTool.ts:1471-1480).
    std::optional<dqGeom::Vector3d> getInputVector();

    // Ported from: itwinjs-core AnimatedHandle._anchorPtView (ViewTool.ts:1410).
    dqGeom::Point3d m_anchorPtView;
    // Ported from: itwinjs-core AnimatedHandle._lastPtView (ViewTool.ts:1411).
    dqGeom::Point3d m_lastPtView;
    // Ported from: itwinjs-core AnimatedHandle._lastMotionTime (ViewTool.ts:1412).
    double m_lastMotionTime = 0.0;
    // Ported from: itwinjs-core AnimatedHandle._deadZone (ViewTool.ts:1413).
    double m_deadZone = 36.0;
};

// ---------------------------------------------------------------------------
// ViewPan — ViewingToolHandle for performing the "pan view" operation.
// Ported from: itwinjs-core ViewPan (ViewTool.ts:1107-1175).
//
// perform() ports :1143-1166: NPC pan delta → world delta → moveCameraWorld
// (3d) or setOrigin (2d) → setupFromView. The 3d branch uses
// view->MoveCameraWorld(dist) (Task 3 port of ViewState3d.moveCameraWorld,
// ViewState.ts:1972-1981). The viewingGlobe branch (moveCameraGlobal) is not
// ported; with viewingGlobe=false the reference always takes the
// moveCameraWorld path, which is the faithful Step 3 behavior.
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewPan : public HandleWithInertia {
public:
    explicit ViewPan(ViewManip* vm) noexcept : HandleWithInertia(vm) {}

    // Ported from: itwinjs-core ViewPan.handleType getter (ViewTool.ts:1108).
    ViewHandleType handleType() const noexcept override { return ViewHandleType::Pan; }
    // Ported from: itwinjs-core ViewPan.getHandleCursor (ViewTool.ts:1109).
    // Stub cursor names ("grab"/"grabbing") — viewManager cursor assets are
    // not ported. Faithful to the reference's string-contract.
    std::string getHandleCursor() const override;
    // Ported from: itwinjs-core ViewPan.firstPoint (ViewTool.ts:1111-1134).
    bool firstPoint(BeButtonEvent const& ev) override;
    // Ported from: itwinjs-core ViewPan.testHandleForHit (ViewTool.ts:1136-1140).
    bool testHandleForHit(dqGeom::Point3d ptScreen, HitOut& out) override;
    // Ported from: itwinjs-core ViewPan.needDepthPoint (ViewTool.ts:1169-1174).
    bool needDepthPoint(BeButtonEvent const& ev, bool isPreview) override;

protected:
    // Ported from: itwinjs-core ViewPan.perform (ViewTool.ts:1143-1166).
    bool perform(dqGeom::Point3d thisPtNpc) override;
};

// ---------------------------------------------------------------------------
// ViewRotate — ViewingToolHandle for performing the "rotate view" operation.
// Ported from: itwinjs-core ViewRotate (ViewTool.ts:1178-1319).
//
// perform() ports :1214-1286: screen delta (currPt vs anchorPt in view space)
// → axis+angle → Matrix3d.createRotationAroundVector (DanQing
// CreateRotationAroundAxis) → Transform.createFixedPointAndMatrix(targetCenter,
// rot) → frustum.transformBy(worldTransform) → view.setupFromFrustum(frustum)
// → vp.setupFromView. The 2d-vs-3d branch keys off view.allow3dManipulations()
// (Task 3 port).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewRotate : public HandleWithInertia {
public:
    explicit ViewRotate(ViewManip* vm) noexcept : HandleWithInertia(vm) {}

    // Ported from: itwinjs-core ViewRotate.handleType getter (ViewTool.ts:1182).
    ViewHandleType handleType() const noexcept override { return ViewHandleType::Rotate; }
    // Ported from: itwinjs-core ViewRotate.getHandleCursor (ViewTool.ts:1183).
    std::string getHandleCursor() const override { return "rotate"; }
    // Ported from: itwinjs-core ViewRotate.firstPoint (ViewTool.ts:1191-1212).
    bool firstPoint(BeButtonEvent const& ev) override;
    // Ported from: itwinjs-core ViewRotate.testHandleForHit (ViewTool.ts:1185-1189).
    bool testHandleForHit(dqGeom::Point3d ptScreen, HitOut& out) override;
    // Ported from: itwinjs-core ViewRotate.onWheel (ViewTool.ts:1288-1296).
    bool onWheel(BeWheelEvent const& ev) override;
    // Ported from: itwinjs-core ViewRotate.needDepthPoint (ViewTool.ts:1299-1304).
    bool needDepthPoint(BeButtonEvent const& ev, bool isPreview) override;
    // Ported from: itwinjs-core ViewRotate.adjustDepthPoint (ViewTool.ts:1307-1318).
    bool adjustDepthPoint(bool isValid, Viewport* vp,
                          Plane3dByOriginAndUnitNormal plane,
                          DepthPointSource source) override;

protected:
    // Ported from: itwinjs-core ViewRotate.perform (ViewTool.ts:1214-1286).
    bool perform(dqGeom::Point3d ptNpc) override;

private:
    // Ported from: itwinjs-core ViewRotate._frustum (ViewTool.ts:1179).
    dqCommon::Frustum m_frustum;
    // Ported from: itwinjs-core ViewRotate._activeFrustum (ViewTool.ts:1180).
    dqCommon::Frustum m_activeFrustum;
    // Ported from: itwinjs-core ViewRotate._anchorPtNpc (ViewTool.ts:1181).
    dqGeom::Point3d m_anchorPtNpc;
};

// ---------------------------------------------------------------------------
// ViewTargetCenter — ViewingToolHandle for modifying the view's target point
// for operations like rotate.
// Ported from: itwinjs-core ViewTargetCenter (ViewTool.ts:938-1038).
//
// drawHandle ports :1001-1021: the fixed cross drawn at targetCenterWorld while
// a rotate drag is anchored (inHandleModify) or the target is locked — the
// "十字固定不动" of the anchored rotate state. drawCross (:971-999) is the
// shared static also used by ViewManip::previewDepthPoint (:375).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewTargetCenter : public ViewingToolHandle {
public:
    explicit ViewTargetCenter(ViewManip* vm) noexcept : ViewingToolHandle(vm) {}

    // Ported from: itwinjs-core ViewTargetCenter.handleType getter (ViewTool.ts:941).
    ViewHandleType handleType() const noexcept override { return ViewHandleType::TargetCenter; }
    // Ported from: itwinjs-core ViewTargetCenter.checkOneShot (ViewTool.ts:942) —
    // "Don't exit tool after moving target in single-shot mode...".
    bool checkOneShot() const noexcept override { return false; }
    // Ported from: itwinjs-core ViewTargetCenter.firstPoint (ViewTool.ts:943-948).
    bool firstPoint(BeButtonEvent const& ev) override;
    // Ported from: itwinjs-core ViewTargetCenter.testHandleForHit (ViewTool.ts:950-968).
    bool testHandleForHit(dqGeom::Point3d ptScreen, HitOut& out) override;
    // Ported from: itwinjs-core ViewTargetCenter.drawCross (ViewTool.ts:970-999).
    static void drawCross(DecorateContext& context, dqGeom::Point3d const& worldPoint,
                          double sizePixels, bool hasFocus);
    // Ported from: itwinjs-core ViewTargetCenter.drawHandle (ViewTool.ts:1001-1021).
    void drawHandle(DecorateContext& context, bool hasFocus) override;
    // Ported from: itwinjs-core ViewTargetCenter.doManipulation (ViewTool.ts:1023-1031).
    bool doManipulation(BeButtonEvent const& ev, bool inDynamics) override;
    // Ported from: itwinjs-core ViewTargetCenter.needDepthPoint (ViewTool.ts:1033-1037).
    bool needDepthPoint(BeButtonEvent const& ev, bool isPreview) override;
};

// ---------------------------------------------------------------------------
// ViewLook — ViewingToolHandle for performing the "look view" operation.
// Ported from: itwinjs-core ViewLook (ViewTool.ts:1323-1408).
//
// firstPoint snapshots the eye point / view rotation / world frustum and begins
// dynamic update; doManipulation computes a fixed-point (eye) rotation from the
// screen-space drag delta (getLookTransform) and re-aims the view via
// viewport.setupViewFromFrustum(frustum.transformBy(worldTransform)).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewLook : public ViewingToolHandle {
public:
    explicit ViewLook(ViewManip* vm) noexcept : ViewingToolHandle(vm) {}

    // Ported from: itwinjs-core ViewLook.handleType getter (ViewTool.ts:1329).
    ViewHandleType handleType() const noexcept override { return ViewHandleType::Look; }
    // Ported from: itwinjs-core ViewLook.getHandleCursor (ViewTool.ts:1330).
    // Stub cursor name — IModelApp.viewManager.lookCursor is not ported.
    std::string getHandleCursor() const override { return "look"; }
    // Ported from: itwinjs-core ViewLook.testHandleForHit (ViewTool.ts:1332-1336).
    bool testHandleForHit(dqGeom::Point3d ptScreen, HitOut& out) override;
    // Ported from: itwinjs-core ViewLook.firstPoint (ViewTool.ts:1338-1356).
    bool firstPoint(BeButtonEvent const& ev) override;
    // Ported from: itwinjs-core ViewLook.onWheel (ViewTool.ts:1358-1367).
    bool onWheel(BeWheelEvent const& ev) override;
    // Ported from: itwinjs-core ViewLook.doManipulation (ViewTool.ts:1369-1383).
    bool doManipulation(BeButtonEvent const& ev, bool inDynamics) override;

private:
    // Ported from: itwinjs-core ViewLook.getLookTransform (ViewTool.ts:1385-1407).
    dqGeom::Transform getLookTransform(Viewport const& vp,
                                       dqGeom::Point3d const& firstPt,
                                       dqGeom::Point3d const& currPt) const;

    // Ported from: itwinjs-core ViewLook._eyePoint (ViewTool.ts:1325).
    dqGeom::Point3d m_eyePoint;
    // Ported from: itwinjs-core ViewLook._firstPtView (ViewTool.ts:1326).
    dqGeom::Point3d m_firstPtView;
    // Ported from: itwinjs-core ViewLook._rotation (ViewTool.ts:1327).
    dqGeom::Matrix3d m_rotation;
    // Ported from: itwinjs-core ViewLook._frustum (ViewTool.ts:1328).
    dqCommon::Frustum m_frustum;
};

// ---------------------------------------------------------------------------
// ViewScroll — ViewingToolHandle for performing the "scroll view" operation.
// Ported from: itwinjs-core ViewScroll (ViewTool.ts:1500-1597).
//
// animate() ports :1555-1588: cursor offset → direction → scroll delta →
// viewport.scroll(dist) (orthographic) or frustum translate (camera-on).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewScroll : public AnimatedHandle {
public:
    explicit ViewScroll(ViewManip* vm) noexcept : AnimatedHandle(vm) {}

    // Ported from: itwinjs-core ViewScroll.handleType getter (ViewTool.ts:1501).
    ViewHandleType handleType() const noexcept override { return ViewHandleType::Scroll; }
    // Ported from: itwinjs-core ViewScroll.getHandleCursor (ViewTool.ts:1502).
    std::string getHandleCursor() const override { return "move"; }
    // Ported from: itwinjs-core ViewScroll.firstPoint (ViewTool.ts:1549-1553).
    bool firstPoint(BeButtonEvent const& ev) override;
    // Ported from: itwinjs-core ViewScroll.animate (ViewTool.ts:1555-1588).
    bool animate() override;
    // Ported from: itwinjs-core ViewScroll.needDepthPoint (ViewTool.ts:1591-1596).
    bool needDepthPoint(BeButtonEvent const& ev, bool isPreview) override;

    // Ported from: itwinjs-core ViewScroll.drawHandle (ViewTool.ts:1504-1547).
    // TODO: canvas-decoration rendering — Task 15. Body is a no-op (base default).
};

// ===========================================================================
// Task 11 — Concrete view tools (PanViewTool / RotateViewTool /
// ScrollViewTool / FitViewTool) + registration.
//
// Each ViewManip subclass configures handleMask + ctor args + toolId + the
// isExitAllowedOnReinitialize / provideInitialToolAssistance overrides. The
// inherited ViewTool::run() / ViewManip::onPostInstall lifecycle wires them
// into ToolAdmin.startViewTool + onReinitialize → provideInitialToolAssistance.
// FitViewTool extends ViewTool (not ViewManip) and ports its own onDataButtonDown
// / onPostInstall / doFit.
//
// View.Zoom is deferred — its handle (ViewZoom) is not ported (Task 10 scope
// was Pan/Rotate/Scroll only; WindowArea/Look W2 added ViewLook). See Task 11
// brief.
// ===========================================================================

// ---------------------------------------------------------------------------
// PanViewTool — concrete tool that performs a Pan view operation.
// Ported from: itwinjs-core PanViewTool (ViewTool.ts:3035-3043).
//
// handleMask = ViewHandleType::Pan → changeViewport instantiates a single
// ViewPan handle. isExitAllowedOnReinitialize = true so single-shot pan gestures
// exit when the drag terminates (ViewManip.onReinitialize shouldExit branch).
// provideInitialToolAssistance shows the FirstPoint prompt.
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT PanViewTool : public ViewManip {
public:
    // Ported from: itwinjs-core PanViewTool constructor (ViewTool.ts:3038).
    // Reference defaults: oneShot=false, isDraggingRequired=false.
    PanViewTool(Viewport* vp, bool oneShot = false, bool isDraggingRequired = false)
        : ViewManip(vp, static_cast<uint32_t>(ViewHandleType::Pan),
                    oneShot, isDraggingRequired) {}

    // Ported from: itwinjs-core PanViewTool.toolId (ViewTool.ts:3036).
    const char* getToolId() const noexcept override { return "View.Pan"; }

protected:
    // Ported from: itwinjs-core PanViewTool.isExitAllowedOnReinitialize (ViewTool.ts:3041).
    bool isExitAllowedOnReinitialize() const noexcept override { return true; }
    // Ported from: itwinjs-core PanViewTool.provideInitialToolAssistance (ViewTool.ts:3042).
    void provideInitialToolAssistance() override
    {
        provideToolAssistance("Pan.Prompts.FirstPoint");
    }
};

// ---------------------------------------------------------------------------
// RotateViewTool — concrete tool that performs a Rotate view operation.
// Ported from: itwinjs-core RotateViewTool (ViewTool.ts:3048-3056).
//
// handleMask = Rotate | Pan | TargetCenter → changeViewport instantiates
// ViewRotate + ViewPan (+ ViewTargetCenter when ported). The rotate handle is
// the primary gesture; Pan and TargetCenter are secondary (rotate handle hit
// by default via testHandleForHit priority Medium > Pan's Low).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT RotateViewTool : public ViewManip {
public:
    // Ported from: itwinjs-core RotateViewTool constructor (ViewTool.ts:3051).
    RotateViewTool(Viewport* vp, bool oneShot = false, bool isDraggingRequired = false)
        : ViewManip(vp,
                    static_cast<uint32_t>(ViewHandleType::Rotate |
                                          ViewHandleType::Pan |
                                          ViewHandleType::TargetCenter),
                    oneShot, isDraggingRequired) {}

    // Ported from: itwinjs-core RotateViewTool.toolId (ViewTool.ts:3049).
    const char* getToolId() const noexcept override { return "View.Rotate"; }

protected:
    // Ported from: itwinjs-core RotateViewTool.isExitAllowedOnReinitialize (ViewTool.ts:3054).
    bool isExitAllowedOnReinitialize() const noexcept override { return true; }
    // Ported from: itwinjs-core RotateViewTool.provideInitialToolAssistance (ViewTool.ts:3055).
    void provideInitialToolAssistance() override
    {
        provideToolAssistance("Rotate.Prompts.FirstPoint");
    }
};

// ---------------------------------------------------------------------------
// LookViewTool — concrete tool that performs a Look view operation.
// Ported from: itwinjs-core LookViewTool (ViewTool.ts:3063-3071).
//
// handleMask = Look | Pan → changeViewport instantiates ViewLook + ViewPan.
// The look handle wins the hit test (priority Medium over Pan's Low,
// ViewTool.ts:1334/:1140 — the
// reference comment at ViewTool.ts:1335 notes Pan is only force-enabled by the
// IdleTool middle-button action).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT LookViewTool : public ViewManip {
public:
    // Ported from: itwinjs-core LookViewTool constructor (ViewTool.ts:3067).
    // Reference defaults: oneShot=false, isDraggingRequired=false.
    LookViewTool(Viewport* vp, bool oneShot = false, bool isDraggingRequired = false)
        : ViewManip(vp,
                    static_cast<uint32_t>(ViewHandleType::Look | ViewHandleType::Pan),
                    oneShot, isDraggingRequired) {}

    // Ported from: itwinjs-core LookViewTool.toolId (ViewTool.ts:3064).
    const char* getToolId() const noexcept override { return "View.Look"; }

protected:
    // Ported from: itwinjs-core LookViewTool.isExitAllowedOnReinitialize (ViewTool.ts:3069).
    bool isExitAllowedOnReinitialize() const noexcept override { return true; }
    // Ported from: itwinjs-core LookViewTool.provideInitialToolAssistance (ViewTool.ts:3070).
    // provideToolAssistance body is a stub (ToolAssistance UI not ported).
    void provideInitialToolAssistance() override
    {
        provideToolAssistance("Look.Prompts.FirstPoint");
    }
};

// ---------------------------------------------------------------------------
// ScrollViewTool — concrete tool that performs a Scroll view operation.
// Ported from: itwinjs-core ScrollViewTool (ViewTool.ts:3074-3082).
//
// handleMask = ViewHandleType::Scroll → changeViewport instantiates a single
// ViewScroll handle (AnimatedHandle that continuously scrolls while the cursor
// is in the outer dead-zone region).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ScrollViewTool : public ViewManip {
public:
    // Ported from: itwinjs-core ScrollViewTool constructor (ViewTool.ts:3077).
    ScrollViewTool(Viewport* vp, bool oneShot = false, bool isDraggingRequired = false)
        : ViewManip(vp, static_cast<uint32_t>(ViewHandleType::Scroll),
                    oneShot, isDraggingRequired) {}

    // Ported from: itwinjs-core ScrollViewTool.toolId (ViewTool.ts:3075).
    const char* getToolId() const noexcept override { return "View.Scroll"; }

protected:
    // Ported from: itwinjs-core ScrollViewTool.isExitAllowedOnReinitialize (ViewTool.ts:3080).
    bool isExitAllowedOnReinitialize() const noexcept override { return true; }
    // Ported from: itwinjs-core ScrollViewTool.provideInitialToolAssistance (ViewTool.ts:3081).
    void provideInitialToolAssistance() override
    {
        provideToolAssistance("Scroll.Prompts.FirstPoint");
    }
};

// ---------------------------------------------------------------------------
// FitViewTool — concrete tool that fits the view to the model extents.
// Ported from: itwinjs-core FitViewTool (ViewTool.ts:3197-3254).
//
// Extends ViewTool (not ViewManip) — fit is a one-shot operation that does not
// use the handle state machine. onPostInstall → doFit immediately; a data-button
// down also triggers doFit. doFit (Step 3 minimal faithful version) uses the
// view's current extents as the fit range; the reference's computeFitRange
// (model extents via viewport.computeViewRange + clip-volume intersection) is
// TODO pending the computeViewRange port (no always-drawn / never-drawn /
// clip-volume plumbing in Step 3).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT FitViewTool : public ViewTool {
public:
    // Ported from: itwinjs-core FitViewTool state (ViewTool.ts:3200-3202).
    bool oneShot = false;
    bool doAnimate = true;
    bool isolatedOnly = true;

    // Ported from: itwinjs-core FitViewTool constructor (ViewTool.ts:3203-3209).
    // Reference defaults: doAnimate=true, isolatedOnly=true. DanQing mirrors these
    // as non-defaulted params so callers explicitly opt in/out (matches the
    // reference's explicit ctor signature).
    FitViewTool(Viewport* vp, bool oneShot, bool doAnimate = true, bool isolatedOnly = true)
        : ViewTool(vp), oneShot(oneShot), doAnimate(doAnimate), isolatedOnly(isolatedOnly) {}

    // Ported from: itwinjs-core FitViewTool.toolId (ViewTool.ts:3198).
    const char* getToolId() const noexcept override { return "View.Fit"; }

    // Ported from: itwinjs-core FitViewTool.onDataButtonDown (ViewTool.ts:3231-3236).
    EventHandled onDataButtonDown(BeButtonEvent const& ev) override;

    // Ported from: itwinjs-core FitViewTool.onPostInstall (ViewTool.ts:3238-3245).
    void onPostInstall() override;

    // Ported from: itwinjs-core FitViewTool.doFit (ViewTool.ts:3247-3253).
    // Returns oneShot (matches the reference's `return oneShot` at L3252).
    bool doFit(Viewport* viewport, bool oneShot, bool doAnimate = true, bool isolatedOnly = true);

    // Ported from: itwinjs-core FitViewTool.provideToolAssistance (ViewTool.ts:3211-3229).
    // No-arg variant (FitViewTool does not take a prompt key — it builds its own
    // instruction set). Step 3 stub body: ToolAssistance UI is not yet ported.
    void provideToolAssistance() const
    {
        // TODO: ToolAssistance.createInstruction + setToolAssistance — Step 3 stub.
    }
};

// ---------------------------------------------------------------------------
// StandardViewTool — rotates the view to a standard orientation (one-shot).
// Ported from: itwinjs-core StandardViewTool (ViewTool.ts:3496-3524).
//
// Extends ViewTool (not ViewManip): onPostInstall rotates the view's frustum
// by the rotation that takes the current orientation to the requested standard
// rotation, then setupFromFrustum recomputes the new rotation/origin/extents.
// Tool exits immediately after applying the rotation (one-shot).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT StandardViewTool : public ViewTool {
public:
    // Ported from: itwinjs-core StandardViewTool constructor (ViewTool.ts:3499).
    StandardViewTool(Viewport* vp, StandardViewId standardViewId) noexcept
        : ViewTool(vp), m_standardViewId(standardViewId) {}

    // Ported from: itwinjs-core StandardViewTool.toolId (ViewTool.ts:3497).
    const char* getToolId() const noexcept override { return "View.Standard"; }

    // Ported from: itwinjs-core StandardViewTool.onPostInstall (ViewTool.ts:3503-3522).
    void onPostInstall() override;

private:
    StandardViewId m_standardViewId = StandardViewId::Iso;  // TS _standardViewId
};

// ---------------------------------------------------------------------------
// WindowAreaTool — performs a Window-area view operation (box zoom: two-point
// window + rubber band + crosshair + zoom + undo).
// Ported from: itwinjs-core WindowAreaTool (ViewTool.ts:3531-3816).
//
// Extends ViewTool (not ViewManip) — the two-point gesture does not use the
// handle state machine. First data point records the anchor + switches the
// decoration from full-screen crosshair to the rubber band; second data point
// applies the zoom (ortho path :3792-3812 fully ported; camera path deferred —
// see doManipulation TODO) and reinitializes for the next window.
//
// 装饰通路：橡皮筋 = WorldOverlay 图形（DecorateContext::AddDecoration，经
// vp->createGraphicBuilder —— AcsTriadDecorator.cpp:239-246 既有模式）；十字线 =
// CanvasDecoration（DecorateContext::AddCanvasDecoration，ViewContext.ts:312-325）。
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT WindowAreaTool : public ViewTool {
public:
    // Ported from: itwinjs-core WindowAreaTool — 参考无显式 ctor（成员初始化器 +
    //              基类 ViewTool ctor 接 viewport，ViewTool.ts:3531-3540 + :113）。
    explicit WindowAreaTool(Viewport* vp) noexcept : ViewTool(vp) {}
    ~WindowAreaTool() override;

    // Ported from: itwinjs-core WindowAreaTool.toolId (ViewTool.ts:3532).
    const char* getToolId() const noexcept override { return "View.WindowArea"; }

    // Ported from: itwinjs-core WindowAreaTool.onPostInstall (ViewTool.ts:3542-3545).
    void onPostInstall() override;
    // Ported from: itwinjs-core WindowAreaTool.onReinitialize (ViewTool.ts:3547-3552).
    void onReinitialize() override;
    // Ported from: itwinjs-core WindowAreaTool.onResetButtonUp (ViewTool.ts:3554-3561).
    EventHandled onResetButtonUp(BeButtonEvent const& ev) override;
    // Ported from: itwinjs-core WindowAreaTool.onDataButtonDown (ViewTool.ts:3584-3613).
    EventHandled onDataButtonDown(BeButtonEvent const& ev) override;
    // Ported from: itwinjs-core WindowAreaTool.onMouseMotion (ViewTool.ts:3615).
    void onMouseMotion(BeButtonEvent const& ev) override;
    // Ported from: itwinjs-core WindowAreaTool.decorate (ViewTool.ts:3679-3731).
    void decorate(DecorateContext& context) override;

    // Ported from: itwinjs-core WindowAreaTool.computeWindowCorners (ViewTool.ts:3639-3677)。
    // 参考标 private（TS private 仅编译期封装）；C++ 端口公开以便单测直接驱动纵横比
    // 适配数学（先例：ViewManip::processFirstPoint/processPoint 为 "virtual for test
    // override" 公开）。返回 _corners 成员数组指针（参考返回同一数组引用——调用方
    // 继续原地变换之，:3754/:3695）；nullptr = 参考的 undefined。
    std::vector<dqGeom::Point3d>* computeWindowCorners() noexcept;

    // Ported from: itwinjs-core WindowAreaTool.provideToolAssistance (ViewTool.ts:3563-3582)。
    // ToolAssistance UI（IModelApp.notifications.setToolAssistance）未移植 → 空 stub
    // （同 ViewManip::provideToolAssistance / FitViewTool::provideToolAssistance 既有模式）。
    void provideToolAssistance() const {}

private:
    // Ported from: itwinjs-core WindowAreaTool.doManipulation (ViewTool.ts:3733-3815)。
    void doManipulation(BeButtonEvent const& ev, bool inDynamics);

    // 橡皮筋图形的 DanQing 所有权辅助（dispose + delete，AcsTriadDecorator 模式）。
    void disposeBoxGraphic() noexcept;

    // --- State (1:1 with reference, ViewTool.ts:3534-3540) ---

    // Ported from: itwinjs-core WindowAreaTool._haveFirstPoint (ViewTool.ts:3534).
    bool m_haveFirstPoint = false;
    // Ported from: itwinjs-core WindowAreaTool._firstPtWorld (ViewTool.ts:3535).
    dqGeom::Point3d m_firstPtWorld;
    // Ported from: itwinjs-core WindowAreaTool._secondPtWorld (ViewTool.ts:3536).
    dqGeom::Point3d m_secondPtWorld;
    // Ported from: itwinjs-core WindowAreaTool._lastPtView (ViewTool.ts:3537 —
    //              `_lastPtView?: Point3d` → std::optional，§3.4)。
    std::optional<dqGeom::Point3d> m_lastPtView;
    // Ported from: itwinjs-core WindowAreaTool._corners (ViewTool.ts:3538 —
    //              `[new Point3d(), new Point3d()]` 定长 2 → vector(2)，§3.4 容器映射；
    //              computeWindowCorners 的出入参（参考原地复用同一数组）。
    std::vector<dqGeom::Point3d> m_corners = std::vector<dqGeom::Point3d>(2);
    // Ported from: itwinjs-core WindowAreaTool._shapePts (ViewTool.ts:3539 — 定长 5，
    //              首尾闭合的橡皮筋矩形)。
    std::vector<dqGeom::Point3d> m_shapePts = std::vector<dqGeom::Point3d>(5);
    // Ported from: itwinjs-core WindowAreaTool._fillColor (ViewTool.ts:3540 —
    //              ColorDef.from(0, 0, 255, 200)，约 78% 透明蓝填充)。
    dqCommon::ColorDef m_fillColor = dqCommon::ColorDef::from(0, 0, 255, 200);

    // 橡皮筋图形的 DanQing 所有权——Decorations 的 GraphicList 非持有（Decorations.h:
    // 66-87），参考依赖每帧新建 Decorations + GC（Viewport.ts:2675）。DanQing 适配
    // （AcsTriadDecorator 既有模式）：每次 decorate 重建时 dispose 旧图形（此时
    // CollectDecorations 已 clear 列表，旧图形不再被引用）。
    dqRender::RenderGraphic* m_boxGraphic = nullptr;
    dqRender::RenderGraphicOwner* m_boxGraphicOwner = nullptr;
};

// ---------------------------------------------------------------------------
// ViewUndoTool — performs a view undo operation (one-shot).
// Ported from: itwinjs-core ViewUndoTool (ViewTool.ts:4111-4120).
// 注：参考注释明示应用也可直接调 Viewport.doUndo 而不建工具。
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewUndoTool : public ViewTool {
public:
    explicit ViewUndoTool(Viewport* vp) noexcept : ViewTool(vp) {}

    // Ported from: itwinjs-core ViewUndoTool.toolId (ViewTool.ts:4112).
    const char* getToolId() const noexcept override { return "View.Undo"; }

    // Ported from: itwinjs-core ViewUndoTool.onPostInstall (ViewTool.ts:4115-4119).
    void onPostInstall() override;
};

// ---------------------------------------------------------------------------
// ViewRedoTool — performs a view redo operation (one-shot).
// Ported from: itwinjs-core ViewRedoTool (ViewTool.ts:4125-4134).
// 注：参考注释明示应用也可直接调 Viewport.doRedo 而不建工具。
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewRedoTool : public ViewTool {
public:
    explicit ViewRedoTool(Viewport* vp) noexcept : ViewTool(vp) {}

    // Ported from: itwinjs-core ViewRedoTool.toolId (ViewTool.ts:4126).
    const char* getToolId() const noexcept override { return "View.Redo"; }

    // Ported from: itwinjs-core ViewRedoTool.onPostInstall (ViewTool.ts:4129-4133).
    void onPostInstall() override;
};

// TODO: View.Zoom deferred — needs the ViewZoom handle (Task 10 ported
//       ViewPan/ViewRotate/ViewScroll; WindowArea/Look W2 ported ViewLook).
//       Port ZoomViewTool (ViewTool.ts:3087-3095) when its handle lands.

}  // namespace dqApp

// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — AccuSnap (element snapping)
// Ported from: itwinjs-core core/frontend/src/AccuSnap.ts
#pragma once

#include "Export.h"
#include "Decorator.h"    // IDecorator (reference: AccuSnap implements Decorator, :182)
#include "Sprites.h"      // SpriteLocation (reference: cross/icon/errorIcon, :192-196)
#include "ToolAdmin.h"    // BeButtonEvent, EventHandled (faithful AccuSnap.onPreButtonEvent/onMotion/onTouchTap take BeButtonEvent)
#include <dqBase/DqEvent.h>

namespace dqApp {

class Viewport;
struct ButtonEvent;

// Snap mode (bitmask values — combinable as active snap modes).
// Ported from: itwinjs-core core/frontend/src/HitDetail.ts:22-32
enum class SnapMode : uint16_t {
    Nearest = 1,
    NearestKeypoint = 1 << 1,    // 2
    MidPoint = 1 << 2,           // 4
    Center = 1 << 3,             // 8
    Origin = 1 << 4,             // 16
    Bisector = 1 << 5,           // 32
    Intersection = 1 << 6,       // 64
    PerpendicularPoint = 1 << 7, // 128
    TangentPoint = 1 << 8,       // 256
};

// ---------------------------------------------------------------------------
// AccuSnap — element snapping system
// Ported from: itwinjs-core AccuSnap.ts
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT AccuSnap : public IDecorator {
public:
    // ToolState — 工具会话级的 snap/locate 开关。
    // Ported from: itwinjs-core AccuSnap.ToolState (AccuSnap.ts:1315-1331)。
    // neverFlash（Id64Set）未移植——装饰 flash 由 Viewport 的 flashedId 承载。
    struct ToolState {
        bool enabled = false;    // snap 默认关（:1316）——工具经 enableSnap 显式开启
        bool locate = false;     // locate 默认关（:1317）
        int suspended = 0;
        void setFrom(ToolState const& other) { enabled = other.enabled; locate = other.locate; suspended = other.suspended; }
        ToolState clone() const { ToolState v; v.setFrom(*this); return v; }
    };

    AccuSnap() = default;
    ~AccuSnap() override = default;

    // ← AccuSnap.ts:206（public readonly toolState）
    ToolState toolState;

    // Called after Application startup.
    // Ported from: itwinjs-core AccuSnap.onInitialized()
    virtual void onInitialized() {}

    // Whether snapping is enabled.
    // Ported from: itwinjs-core AccuSnap.isSnapEnabled (AccuSnap.ts:221)
    bool isSnapEnabled() const noexcept { return toolState.enabled; }
    // Ported from: itwinjs-core AccuSnap.isLocateEnabled (AccuSnap.ts:219)
    bool isLocateEnabled() const noexcept { return toolState.locate; }

    // Ported from: itwinjs-core AccuSnap.enableSnap (AccuSnap.ts:223-236) —
    // 关闭时清当前 snap（clear()）；touchCursor 分支未移植（无触摸子系统）。
    void enableSnap(bool yesNo)
    {
        toolState.enabled = yesNo;
        if (!yesNo)
            clear();
    }
    // Ported from: itwinjs-core AccuSnap.enableLocate (AccuSnap.ts:237-244)。
    void enableLocate(bool yesNo) { toolState.locate = yesNo; }

    // Ported from: itwinjs-core AccuSnap.onStartTool (AccuSnap.ts:1251-1256) —
    // initCmdState/tentativePoint.clear 未移植（命令状态/试凑点子系统 TODO）。
    void onStartTool()
    {
        enableSnap(false);
        enableLocate(false);
    }

    // Get the active snap modes.
    // Ported from: itwinjs-core AccuSnap.getActiveSnapModes()
    virtual SnapMode const* getActiveSnapModes(int& count) const {
        count = 1;
        return &m_activeSnapMode;
    }

    // Clear the current snap.
    // Ported from: itwinjs-core AccuSnap.clear() (:265 — currHit = undefined;
    // the visual clear is cross.deactivate via decorate no longer firing)
    virtual void clear() {}

    // --- Button dispatch hooks (called from ToolAdmin.sendButtonEvent) -------
    // Step 3 no-op stubs — Task 15 may expand. Return EventHandled::No /
    // false so dispatch falls through to the active tool, matching the
    // reference's "no touchCursor / no snap" path (AccuSnap.ts:1135 returns
    // false when this.touchCursor is undefined).
    // Ported from: itwinjs-core AccuSnap.onPreButtonEvent (AccuSnap.ts:1134-1136)
    virtual EventHandled onPreButtonEvent(BeButtonEvent const&) { return EventHandled::No; }
    // Ported from: itwinjs-core AccuSnap.onMotion (AccuSnap.ts:1095)
    virtual void onMotion(BeButtonEvent const&) {}
    // Ported from: itwinjs-core AccuSnap.onTouchTap (AccuSnap.ts:1172-1176 —
    //               returns false when no touchCursor is active).
    virtual bool onTouchTap(BeButtonEvent const&) { return false; }

    // --- Snap cross (visual chain) -------------------------------------------
    // The snap cross sprite locations. ← AccuSnap.ts:192-196
    // (cross = SnapCross/SnapUnfocused at the snap point; icon = per-snapmode
    //  icon — TODO with the snap-mode engine; errorIcon — TODO ditto.)
    SpriteLocation cross;

    // Show the snap cross (hot — SnapCross.png) at a view-coordinate location.
    // ← AccuSnap.ts:484-490 (onSnap → getSnap → cross.activate(crossSprite,
    //   viewport, crossPt); hot = a confirmed snap). Sprites load lazily on
    //   first use from third_party/itwinjs-sprites/ (converted from the
    //   reference's public/sprites PNGs).
    void activateCrossAt(Viewport& vp, double viewX, double viewY);

    // Clear the snap cross. ← AccuSnap.ts clear() path (cross.deactivate when
    // the hit goes away — getSnap miss :355).
    void clearCross();

    // --- IDecorator (reference: AccuSnap implements Decorator, :182) --------
    // ← AccuSnap.ts:1208-1220 (decorate: touchCursor → flashElements →
    //   cross.isActive → cross.decorate + icon.decorate → errorIcon.decorate).
    //   The flash chain lives in Viewport (flashedId); the ported slice here
    //   is the cross sprite. icon/errorIcon arrive with the snap-mode engine.
    void Decorate(DecorateContext& context) override;
    bool UseCachedDecorations() const override { return false; }
    bool TestDecorationHit(uint32_t) const override { return false; }
    QString GetDecorationToolTip(uint32_t) const override { return {}; }

private:
    SnapMode m_activeSnapMode = SnapMode::NearestKeypoint;
    Sprite m_snapCrossHot;      // sprites/SnapCross.png (AccuSnap.ts:484)
    Sprite m_snapCrossUnfocused; // sprites/SnapUnfocused.png (:484 — !isHot)
};

}  // namespace dqApp

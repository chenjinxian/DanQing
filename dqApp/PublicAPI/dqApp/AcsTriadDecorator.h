// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ACS triad decorator (XYZ axis gizmo)
// Ported from: itwinjs-core core/frontend/src/AuxCoordSys.ts:38-306
//                (ACSDisplayOptions / ACSDisplaySizes / isOriginInView /
//                 getAdjustedColor / addAxisLabel / addAxis / createGraphicBuilder /
//                 display)
//              + AccuDraw.ts:2290-2304 (decorate gate + ACS origin pick point)。
//
// Draws the world-origin XYZ triad (X red / Y green / Z blue) as a WorldOverlay
// graphic, sized to ~0.6" screen space, matching itwinjs display-test-app.
//
// Scope: the world-origin identity ACS the blank view uses（参考
// view.auxiliaryCoordinateSystem 默认即世界原点 ACS）。Deferred with the full
// AccuDraw/AuxCoordSystemState port: named/persisted ACS（getOrigin/getRotation
// 非常量）、AccuDraw compass/dynamics、pickable transient-id（取放目前只做
// 视觉等价的蓝点，不注册 pick id——见 Decorate 尾部注释）。
#pragma once

#include "Export.h"
#include "Decorator.h"

namespace dqRender { class RenderGraphicOwner; class RenderGraphic; class RenderSystem; }

namespace dqApp {

class DQ_APP_EXPORT AcsTriadDecorator : public IDecorator {
public:
    AcsTriadDecorator() = default;
    ~AcsTriadDecorator() override;

    // ← AccuDraw.decorate (AccuDraw.ts:2290-2304): draw the triad + the ACS
    //   origin point only when the view's viewFlags.acsTriad is set.
    void Decorate(DecorateContext& context) override;

    // ACS 取放（参考 AccuDraw.testDecorationHit，AccuDraw.ts:2277）：pick id 未接线，
    // 随 AccuDraw/TransientIdSequence 移植落地（见 .cpp Decorate 尾部 TODO）。
    bool TestDecorationHit(uint32_t) const override { return false; }
    QString GetDecorationToolTip(uint32_t) const override { return {}; }

private:
    // Per-decoration graphics: disposed + rebuilt every Decorate so the triad's
    // scale + Z-disc orientation re-bake to the current camera (1:1 with itwinjs
    // AuxCoordSystemState.display, AuxCoordSys.ts:301-305 + AccuDraw.ts:2290-2304
    // per-decorate rebuild, no caching). Decorations::clear() drops the pointer
    // without freeing, so the decorator owns the graphics via RenderGraphicOwners
    // and disposes them at the top of the next Decorate.
    // ~AcsTriadDecorator does NOT dispose (the per-viewport RenderSystem may be
    // gone at shutdown) — the last graphics are leaked and reclaimed by the OS.
    //
    // Cross-viewport lifetime (TD-12 root cause, 2026-09-23): the cached
    // graphics belong to the RenderSystem that CREATED them. When a new
    // viewport appears (previous test's viewport shut down its driver),
    // disposeGraphic would call MeshGraphic::~MeshGraphic → dead-driver
    // destroyBufferObject → SEH 0xc0000005. The fix: remember the creating
    // system; on a system change, drop pointers WITHOUT disposing (same
    // semantics as the destructor — the dead driver's GL resources are
    // already reclaimed with its context; the CPU-side leak is bounded to
    // one triad per viewport transition).
    dqRender::RenderGraphicOwner* m_graphicOwner = nullptr;
    dqRender::RenderGraphic* m_graphic = nullptr;      // current frame's triad; owned by m_graphicOwner
    dqRender::RenderGraphicOwner* m_pickOwner = nullptr;
    dqRender::RenderGraphic* m_pickGraphic = nullptr;  // current frame's ACS origin point; owned by m_pickOwner
    dqRender::RenderSystem* m_creatingSystem = nullptr;  // system that created the cached graphics
};

}  // namespace dqApp

// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — AccuSnap implementation (snap cross visual chain)
// Ported from: itwinjs-core core/frontend/src/AccuSnap.ts:484-490, 1208-1220
#include "dqApp/AccuSnap.h"

#include "dqApp/DecorateContext.h"
#include "dqApp/Viewport.h"

#ifndef DANQING_SPRITE_ASSETS_DIR
#define DANQING_SPRITE_ASSETS_DIR "."
#endif

namespace dqApp {

void AccuSnap::activateCrossAt(Viewport& vp, double viewX, double viewY)
{
    // Lazy sprite load (the reference's IconSprites cache-by-url,
    // Sprites.ts:66-73 — first use loads, later uses reuse).
    if (!m_snapCrossHot.isLoaded())
        m_snapCrossHot.loadFromFile(DANQING_SPRITE_ASSETS_DIR "/SnapCross.png", vp);
    if (!m_snapCrossHot.isLoaded())
        return;   // sprite assets missing — degrade to no cross (no crash path)

    // ← AccuSnap.ts:484-490: hot snap (isHot) shows SnapCross.png.
    // EQUIVALENCE: 参考源=AccuSnap.getSnap（几何 snap 引擎——NearestKeypoint 等
    // 模式在元素几何上求最近 snap 点，AccuSnap.ts:484）；发散=命中判定用
    // Viewport 的 hover 拾取链（PickAtPoint readPixels 命中），snap 点 = 光标
    // 视图坐标（非几何最近关键点）；验证法=悬停 glTF 元素时十字出现于元素上、
    // 离开即消失（PickHiliteSelection 家族回归 + 肉眼）。
    // 未发现的其他发散：snap 模式 icon（SnapNearest.png 等按模式轮换，:486-490）
    // 与 errorIcon（:512）随 snap 模式引擎 TODO。
    cross.activate(m_snapCrossHot, vp, viewX, viewY);
}

void AccuSnap::clearCross()
{
    cross.deactivate();
}

void AccuSnap::Decorate(DecorateContext& context)
{
    // ← AccuSnap.ts:1208-1220 (slice): touchCursor/flash/icon/errorIcon paths
    // arrive with their subsystems; the cross sprite is the ported visual.
    if (cross.isActive())
        cross.decorate(context);
}

}  // namespace dqApp

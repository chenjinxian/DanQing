// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — SelectionTool
//
// Ported from: itwinjs-core core/frontend/src/tools/SelectTool.ts
//              SelectionTool class (line 72)
// Tool for picking elements of interest, selected by the user.
//
// M-O(3) P3：selectByPoints 框选族（:287-439）+ ctrl 增选（processHit
// :408-426 的 Invert 分流）落地。hit-cycling（:480-502）登记延后——参考经
// LocateManager.currHit + accuSnap.doLocate(hitIndex) 在同光标的多深度命中间
// 轮转；DanQing pick 缓冲是深度决胜单值面（无多命中列表），等价轮转需 pick
// 多命中面（EQUIVALENCE 登记在 .cpp 头）。
#pragma once

#include "dqApp/ToolAdmin.h"

#include <dqGeom/Point3d.h>

#include <vector>

namespace dqApp {

class Viewport;

// SelectionMethod — 拾取模式（框选方向语义随 selectByPoints 一起消费）。
// Ported from: itwinjs-core SelectionMethod (SelectTool.ts:60-66)。
enum class SelectionMethod : uint8_t {
    Pick = 0,  // click-pick（缺省）
    Line = 1,  // crossing line（拖拽跨线）
    Box = 2,   // box（拖拽矩形）
};

// SelectionTool — the default interactive tool for element picking.
// Ported from: itwinjs-core SelectionTool (SelectTool.ts:72)
class SelectionTool : public PrimitiveTool {
public:
    // Ported from: itwinjs-core SelectionTool.toolId (SelectTool.ts:74)
    static constexpr const char* ToolId = "Select";

    const char* getToolId() const override { return ToolId; }

    // Ported from: itwinjs-core SelectionTool.requireWriteableTarget
    //              (SelectTool.ts:82) — false: selecting elements does not
    //              require a writable target iModel.
    bool requireWriteableTarget() const override { return false; }

    // Ported from: itwinjs-core SelectionTool.autoLockTarget (SelectTool.ts:83)
    //              Reference body is `{}` — "For selecting elements we only
    //              care about iModel, so don't lock target model automatically."
    //              BlankConnection (Step 3) has no target-model concept, so the
    //              no-op override is both faithful and load-bearing.
    void autoLockTarget() override {}

    // Ported from: itwinjs-core SelectionTool.onPostInstall -> initSelectTool
    // (SelectTool.ts:231-242, 574-577). Lifecycle rename: was OnStart(StartOrResume);
    // the faithful itwinjs flow calls onPostInstall after the tool becomes active
    // (no mode parameter).
    void onPostInstall() override;

    // Ported from: itwinjs-core SelectionTool.processMiss() (SelectTool.ts:244-249)
    // (Internal helper, no override.)
    bool ProcessMiss(BeButtonEvent const& ev);

    // Ported from: itwinjs-core SelectionTool.onDataButtonUp()
    //              (SelectTool.ts:441-469). Task 13 reconciliation: the
    //              reference SelectTool does its locate/pick on the data-button
    //              **UP** transition (SelectTool.ts:441 `onDataButtonUp`), not
    //              on the down transition. M-O(3) P3：ctrl 分流归位
    //              （:414 `ev.isControlKey ? Invert : Replace`——此前恒 Replace）。
    EventHandled onDataButtonUp(BeButtonEvent const& event) override;

    // Ported from: itwinjs-core SelectionTool.onResetButtonUp()
    //              (SelectTool.ts:471-509). Task 13 reconciliation: like the
    //              data path, the reference handles reset on the **UP**
    //              transition.
    //
    //              Body scope: the reference's full reset path does
    //              hit-cycling via IModelApp.locateManager.currHit +
    //              accuSnap.currHit + doLocate + selectDecoration +
    //              accuSnap.resetButton. Hit-cycling 登记（见文件头）；
    //              _isSelectByPoints cleanup 分支随框选落地（M-O(3) P3）。
    EventHandled onResetButtonUp(BeButtonEvent const& event) override;

    // Ported from: itwinjs-core SelectionTool.onMouseStartDrag (SelectTool.ts:428-435
    //              —— M-O(3) P3：selectByPointsStart 起拖).
    EventHandled onMouseStartDrag(BeButtonEvent const& event) override;

    // Ported from: itwinjs-core SelectionTool.onMouseEndDrag (SelectTool.ts:437-439
    //              —— M-O(3) P3：selectByPointsEnd 收框).
    EventHandled onMouseEndDrag(BeButtonEvent const& event) override;

    // Ported from: itwinjs-core SelectionTool.decorate (SelectTool.ts:546
    //              —— M-O(3) P3：selectByPointsDecorate 框选装饰).
    void decorate(DecorateContext& context) override;

    // Ported from: itwinjs-core SelectionTool.onMouseMotion (SelectTool.ts:393-396
    //              —— M-O(3) P3：框选中的失效刷新).
    void onMouseMotion(BeButtonEvent const& event) override;

    // --- selectByPoints 族（protected 面的公开测试口——参考 SelectTool.ts
    //     :287-439） ---
    // :359-369 起拖（Data/Reset 键按下 + 点入 _points）。
    bool selectByPointsStart(BeButtonEvent const& ev);
    // :371-391 收框（Line/Box 分流 + useOverlapSelection + process）。
    bool selectByPointsEnd(BeButtonEvent const& ev);
    // :329-357 候选拾取 + ctrl 分流（Replace/Invert）+ 空面 miss 清。
    bool selectByPointsProcess(dqGeom::Point3d const& origin,
                               dqGeom::Point3d const& corner,
                               BeButtonEvent const& ev, SelectionMethod method,
                               bool overlap);
    // :278-285 overlap 判定（右→左拖 = overlap；Shift 反转）。
    bool useOverlapSelection(BeButtonEvent const& ev) const;

    bool isSelectByPoints() const noexcept { return m_isSelectByPoints; }
    std::vector<dqGeom::Point3d> const& points() const noexcept
    {
        return m_points;
    }

private:
    Viewport* GetViewport() const;

    // :229 _isSelectByPoints / :230 _points。
    bool m_isSelectByPoints = false;
    std::vector<dqGeom::Point3d> m_points;
};

}  // namespace dqApp

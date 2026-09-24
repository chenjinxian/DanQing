// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — SelectionTool
//
// Ported from: itwinjs-core core/frontend/src/tools/SelectTool.ts
//              SelectionTool class (line 72)
// Tool for picking elements of interest, selected by the user.
#pragma once

#include "dqApp/ToolAdmin.h"

namespace dqApp {

class Viewport;

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
    //              on the down transition. Pre-Task-13 the body was named
    //              `onDataButtonDown` (Task 2's mapping of the legacy
    //              `OnMouseButtonDown`); Task 13 renames it to match the
    //              reference's button transition so dispatch (Task 7's
    //              sendButtonEvent) routes the pick on the up event exactly as
    //              itwinjs does. The pick/select/hilite body is preserved.
    EventHandled onDataButtonUp(BeButtonEvent const& event) override;

    // Ported from: itwinjs-core SelectionTool.onResetButtonUp()
    //              (SelectTool.ts:471-509). Task 13 reconciliation: like the
    //              data path, the reference handles reset on the **UP**
    //              transition. Pre-Task-13 the body was named
    //              `onResetButtonDown`; Task 13 renames it to match.
    //
    //              Body scope (Step 3): the reference's full reset path does
    //              hit-cycling via IModelApp.locateManager.currHit +
    //              accuSnap.currHit + doLocate + selectDecoration +
    //              accuSnap.resetButton. Those subsystems are stubs in Step 3
    //              (no LocateManager port; AccuSnap is a no-op stub); the
    //              _isSelectByPoints cleanup branch is unreachable
    //              (selectByPoints is never started without drag-selection
    //              infrastructure). The body is therefore a faithful stub
    //              that returns EventHandled::Yes (matches the reference's
    //              non-clearing behavior — reset does NOT empty the selection
    //              set in itwinjs; it cycles overlapping hits). The pre-Task-13
    //              body emptied the selection set on reset, which was an
    //              invented deviation from the reference (§0 violation) and
    //              has been removed.
    EventHandled onResetButtonUp(BeButtonEvent const& event) override;

private:
    Viewport* GetViewport() const;
};

}  // namespace dqApp

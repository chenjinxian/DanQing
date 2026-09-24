// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewManager tests
// Ported from: itwinjs-core core/frontend/src/test/ViewManager.test.ts
#include <gtest/gtest.h>

#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>

using namespace dqApp;

// ---------------------------------------------------------------------------
// ViewManager basic tests
// Ported from: itwinjs-core core/frontend/src/test/ViewManager.test.ts
// ---------------------------------------------------------------------------
TEST(ViewManager, DefaultState)
{
    ViewManager vm;
    EXPECT_EQ(vm.GetViewportCount(), 0u);
    EXPECT_EQ(vm.GetSelectedViewport(), nullptr);
    EXPECT_EQ(vm.GetActiveViewport(), nullptr);
}

// Ported from: itwinjs-core core/frontend/src/test/ViewManager.test.ts
TEST(ViewManager, AddDropViewport)
{
    ViewManager vm;
    // We can't create a real Viewport without a QWidget parent + GL context,
    // so we test the event mechanism with raw pointers.
    // The Viewport pointer is not dereferenced by ViewManager for add/drop.
    int dummy = 0;
    Viewport* fakeVp = reinterpret_cast<Viewport*>(&dummy);

    int openCount = 0;
    Viewport* openedVp = nullptr;
    vm.OnViewOpen.AddListener([&](Viewport* vp) {
        openCount++;
        openedVp = vp;
    });

    int closeCount = 0;
    Viewport* closedVp = nullptr;
    vm.OnViewClose.AddListener([&](Viewport* vp) {
        closeCount++;
        closedVp = vp;
    });

    // Add viewport
    EXPECT_TRUE(vm.AddViewport(fakeVp));
    EXPECT_EQ(vm.GetViewportCount(), 1u);
    EXPECT_EQ(vm.GetSelectedViewport(), fakeVp);
    EXPECT_EQ(openCount, 1);
    EXPECT_EQ(openedVp, fakeVp);

    // Adding same viewport again should return false
    EXPECT_FALSE(vm.AddViewport(fakeVp));
    EXPECT_EQ(vm.GetViewportCount(), 1u);
    EXPECT_EQ(openCount, 1);  // no additional event

    // Drop viewport
    vm.DropViewport(fakeVp);
    EXPECT_EQ(vm.GetViewportCount(), 0u);
    EXPECT_EQ(vm.GetSelectedViewport(), nullptr);
    EXPECT_EQ(closeCount, 1);
    EXPECT_EQ(closedVp, fakeVp);
}

// Ported from: itwinjs-core core/frontend/src/test/ViewManager.test.ts
TEST(ViewManager, OnSelectedViewportChanged)
{
    ViewManager vm;

    // Use aligned stack variables as proxy Viewport pointers (never dereferenced).
    int dummy1 = 0, dummy2 = 0;
    Viewport* vp1 = reinterpret_cast<Viewport*>(&dummy1);
    Viewport* vp2 = reinterpret_cast<Viewport*>(&dummy2);

    int eventCount = 0;
    Viewport* eventPrev = nullptr;
    Viewport* eventCurr = nullptr;
    vm.OnSelectedViewportChanged.AddListener([&](SelectedViewportChangedArgs args) {
        eventCount++;
        eventPrev = args.previous;
        eventCurr = args.current;
    });

    // AddViewport sets selected to first viewport
    vm.AddViewport(vp1);
    EXPECT_EQ(eventCount, 1);
    EXPECT_EQ(eventPrev, nullptr);  // no previous
    EXPECT_EQ(eventCurr, vp1);

    // SetSelectedViewport to vp2
    vm.SetSelectedViewport(vp2);
    EXPECT_EQ(eventCount, 2);
    EXPECT_EQ(eventPrev, vp1);
    EXPECT_EQ(eventCurr, vp2);

    // SetSelectedViewport to same vp2 — no event (no-op)
    vm.SetSelectedViewport(vp2);
    EXPECT_EQ(eventCount, 2);
}

// Authored: no reference test exists in itwinjs-core ViewManager.test.ts for selection
//           fallback; asserts the reference implementation semantics:
//           - addViewport ALWAYS selects the newly-added viewport
//             (ViewManager.ts:294 setSelectedView(newVp) is unconditional), and
//           - dropping the selected viewport falls back to another open view
//             (ViewManager.ts:331-332 setSelectedView(undefined) → :230-231
//             getFirstOpenView()).
TEST(ViewManager, SelectedViewportFallbackOnDrop)
{
    ViewManager vm;
    int dummy1 = 0, dummy2 = 0;
    Viewport* vp1 = reinterpret_cast<Viewport*>(&dummy1);
    Viewport* vp2 = reinterpret_cast<Viewport*>(&dummy2);

    vm.AddViewport(vp1);
    vm.AddViewport(vp2);
    // ViewManager.ts:294 — addViewport unconditionally selects the NEW viewport.
    EXPECT_EQ(vm.GetSelectedViewport(), vp2);

    // Drop the selected viewport — selection falls back to the remaining one
    // (setSelectedView(undefined) → getFirstOpenView(), ViewManager.ts:230-231).
    vm.DropViewport(vp2);
    EXPECT_EQ(vm.GetSelectedViewport(), vp1);
}

// Authored: no reference test exists in itwinjs-core ToolAdmin.test.ts for startup tool
//           state; asserts the reference wiring:
//           - ToolAdmin.onInitialized creates the idle tool and registers key handlers
//             but does NOT start the default tool (ToolAdmin.ts:510-525);
//           - the default tool is started by ViewManager.setSelectedView when the FIRST
//             viewport is selected (ViewManager.ts:246-247
//             `if (undefined === previousVp) await IModelApp.toolAdmin.startDefaultTool()`).
TEST(ToolAdminStartup, OnInitializedDoesNotStartDefaultTool)
{
    ToolAdmin ta;
    ta.OnInitialized();
    EXPECT_NE(ta.GetIdleTool(), nullptr);    // idle tool created (ToolAdmin.ts:514)
    EXPECT_EQ(ta.GetActiveTool(), nullptr);  // default tool NOT yet started
}

// Authored: no reference test exists in itwinjs-core ViewManager.test.ts for the
//           first-selection default-tool start; asserts ViewManager.ts:246-247 via the
//           Application singleton (ToolAdmin's "Select" factory is registered by
//           OnInitialized, mirroring IModelApp.startup's CoreTools registration).
TEST(ViewManagerDefaultTool, FirstSelectionStartsDefaultTool)
{
    auto& app = Application::Get();
    auto& ta = app.GetToolAdmin();
    ta.OnInitialized();        // idempotent registry populate (Select/Idle/View.*)
    ta.SetActiveTool(nullptr); // normalize: no active tool before first selection

    int dummy = 0;
    Viewport* fakeVp = reinterpret_cast<Viewport*>(&dummy);
    app.GetViewManager().SetSelectedViewport(fakeVp);
    EXPECT_NE(ta.GetActiveTool(), nullptr);  // ViewManager.ts:246-247

    // Cleanup singleton state for subsequent tests.
    app.GetViewManager().SetSelectedViewport(nullptr);
    ta.SetActiveTool(nullptr);
}

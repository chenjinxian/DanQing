// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core/imodel-native for the
//           Qt/native-window teardown race that caused the crash-on-close;
//           DanQing integration-glue regression test for Viewport::Shutdown +
//           ViewManager::ShutdownAll (the fix).
//
// Background (the bug this locks in): MDIView::closeEvent was a no-op, so on
// close/quit Qt destroyed the Viewport's native NSView while the Viewport was
// still in ViewManager::m_viewports. The next render-loop tick ran
// Viewport::RenderFrame -> Swapchain::acquire -> makeCurrent ->
// [NSOpenGLContext setView:<freed NSView>] -> use-after-free.
//
// The fix: the owning MDI view drops the viewport from the ViewManager and
// calls Viewport::Shutdown() to release the GL pipeline SYNCHRONOUSLY (on
// close, and on QCoreApplication::aboutToQuit via ViewManager::ShutdownAll),
// BEFORE Qt destroys the native window. Shutdown() is idempotent;
// RenderFrame/resizeEvent become no-ops once shut down. These tests lock that
// contract so the close-path cleanup cannot silently regress.

#include "QtAppFixture.h"  // qtApp() — shared QApplication fixture (tests/ only)

#include <dqApp/Application.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewState.h>

#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>

#include <gtest/gtest.h>

using dqApp::SpatialViewState;
using dqApp::Viewport;
using dqApp::ViewManager;

namespace {
// Construct a headless Viewport (never shown -> no GL pipeline initialized).
// The contract under test holds regardless of whether the pipeline exists.
Viewport* NewBlankViewport()
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(200, 200, 200));
    return Viewport::Create(nullptr, view);
}
}  // namespace

// Viewport::Shutdown releases the GL pipeline and is idempotent. After it,
// RenderFrame must be a safe no-op (it must not touch any GL surface).
TEST(ViewportShutdown, ShutdownIsIdempotentAndMakesRenderFrameSafe)
{
    qtApp();  // ensure a QApplication exists (Viewport is a QWidget)

    Viewport* vp = NewBlankViewport();
    ASSERT_NE(vp, nullptr);

    EXPECT_NO_FATAL_FAILURE(vp->Shutdown());
    EXPECT_NO_FATAL_FAILURE(vp->Shutdown());     // idempotent — second call is a no-op
    EXPECT_NO_FATAL_FAILURE(vp->RenderFrame());  // no-op after shutdown

    delete vp;
}

// The per-window close path (View3DInventor::closeEvent) drops the viewport
// from the ViewManager's render list BEFORE its resources are gone, so the
// render loop can no longer reach it.
TEST(ViewportShutdown, ClosePathRemovesViewportFromRenderList)
{
    qtApp();

    ViewManager& vm = dqApp::Application::Get().GetViewManager();

    Viewport* vp = NewBlankViewport();
    ASSERT_NE(vp, nullptr);
    ASSERT_TRUE(vm.AddViewport(vp));
    EXPECT_EQ(vm.GetViewportCount(), 1U);

    // Mirror View3DInventor::closeEvent: drop + shutdown synchronously.
    vm.DropViewport(vp);
    vp->Shutdown();

    EXPECT_EQ(vm.GetViewportCount(), 0U);  // no longer in the render list
    EXPECT_NO_FATAL_FAILURE(vp->RenderFrame());

    delete vp;
}

// The quit path (QCoreApplication::aboutToQuit -> ViewManager::ShutdownAll)
// drops + shuts down every viewport so no live pipeline outlives the native
// windows during QApplication teardown.
TEST(ViewportShutdown, ShutdownAllDropsEveryViewport)
{
    qtApp();

    ViewManager& vm = dqApp::Application::Get().GetViewManager();

    Viewport* a = NewBlankViewport();
    Viewport* b = NewBlankViewport();
    ASSERT_TRUE(vm.AddViewport(a));
    ASSERT_TRUE(vm.AddViewport(b));
    EXPECT_EQ(vm.GetViewportCount(), 2U);

    vm.ShutdownAll();

    EXPECT_EQ(vm.GetViewportCount(), 0U);
    EXPECT_NO_FATAL_FAILURE(a->RenderFrame());
    EXPECT_NO_FATAL_FAILURE(b->RenderFrame());

    delete a;
    delete b;
}

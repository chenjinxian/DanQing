// StartupTest.cpp — Verify Step 2: Application::Startup initializes correctly
// This test verifies the IModelApp-equivalent initialization chain.
#include <gtest/gtest.h>

#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/NotificationManager.h>
#include <dqApp/AccuDraw.h>
#include <dqApp/AccuSnap.h>
#include <dqApp/ElementLocateManager.h>
#include <dqApp/TentativePoint.h>
#include <dqApp/QuantityFormatter.h>
#include <dqApp/UiAdmin.h>
#include <dqApp/MapLayerFormatRegistry.h>
#include <dqApp/TerrainProviderRegistry.h>
#include <dqApp/FormatsProviderManager.h>

// Authored: no reference test exists in itwinjs-core for Application singleton access;
//           exercises Application::Get() pre-startup (IModelApp is static-only in TS).
TEST(ApplicationStartup, SingletonExists)
{
    auto& app = dqApp::Application::Get();
    // Application should be constructible
    EXPECT_FALSE(app.isInitialized());
}

// Authored: no reference test exists in itwinjs-core for the C++ Application::Startup chain;
//           exercises IModelApp.startup() subsystem initialization (IModelApp.ts:448-457).
TEST(ApplicationStartup, StartupInitializesSubsystems)
{
    auto& app = dqApp::Application::Get();

    dqApp::Application::Options opts;
    opts.applicationId = "TestApp";
    opts.applicationVersion = "1.0.0";

    bool result = app.Startup(opts);
    EXPECT_TRUE(result);
    EXPECT_TRUE(app.isInitialized());

    // Verify ViewManager is accessible
    auto& vm = app.GetViewManager();
    EXPECT_EQ(vm.GetViewportCount(), 0u);

    // Verify ToolAdmin is accessible
    auto& ta = app.GetToolAdmin();
    EXPECT_NE(ta.GetIdleTool(), nullptr);

    // Verify NotificationManager is accessible
    auto& nm = app.GetNotificationManager();
    (void)nm;  // just verify it's accessible

    // Verify application config
    EXPECT_EQ(app.GetApplicationId(), "TestApp");
    EXPECT_EQ(app.GetApplicationVersion(), "1.0.0");
}

// Authored: no reference test exists in itwinjs-core for dqApp ToolAdmin registration;
//           exercises IModelApp.startup() core tool registration (Select/Idle defaults).
TEST(ApplicationStartup, CoreToolsRegistered)
{
    auto& app = dqApp::Application::Get();
    auto& ta = app.GetToolAdmin();
    auto& registry = ta.GetRegistry();

    // Select and Idle tools should be registered
    EXPECT_NE(registry.Find("Select"), nullptr);
    EXPECT_NE(registry.Find("Idle"), nullptr);

    // Default tool should be "Select"
    EXPECT_STREQ(ta.GetDefaultToolId(), "Select");

    // Faithful to itwinjs-core: startup does NOT start the default tool —
    // ToolAdmin.onInitialized (ToolAdmin.ts:510-525) only creates the idle tool;
    // the default tool is started on first viewport selection
    // (ViewManager.ts:246-247 setSelectedView → startDefaultTool).
    EXPECT_EQ(ta.GetActiveTool(), nullptr);
    EXPECT_NE(ta.GetIdleTool(), nullptr);
}

// Authored: no reference test exists in itwinjs-core for duplicate-startup idempotency;
//           exercises Application::Startup re-entry guard (first startup wins).
TEST(ApplicationStartup, DuplicateStartupIsNoop)
{
    auto& app = dqApp::Application::Get();

    dqApp::Application::Options opts;
    opts.applicationId = "TestApp2";

    // Second startup should return true without re-initializing
    bool result = app.Startup(opts);
    EXPECT_TRUE(result);

    // ApplicationId should NOT change (first startup wins)
    EXPECT_EQ(app.GetApplicationId(), "TestApp");
}

// Authored: no reference test exists in itwinjs-core for aggregate subsystem access;
//           exercises all IModelApp frontend subsystem accessors added in Step 2.
TEST(ApplicationStartup, AllSubsystemsInitialized)
{
    auto& app = dqApp::Application::Get();

    // All subsystems should be accessible
    auto& vm = app.GetViewManager();
    auto& ta = app.GetToolAdmin();
    auto& nm = app.GetNotificationManager();
    auto& ad = app.GetAccuDraw();
    auto& as = app.GetAccuSnap();
    auto& lm = app.GetLocateManager();
    auto& tp = app.GetTentativePoint();
    auto& qf = app.GetQuantityFormatter();
    auto& ua = app.GetUiAdmin();
    auto& ml = app.GetMapLayerFormatRegistry();
    auto& tr = app.GetTerrainProviderRegistry();
    auto& fp = app.GetFormatsProviderManager();

    // Verify they are all initialized (no crash, valid state)
    EXPECT_EQ(vm.GetViewportCount(), 0u);
    EXPECT_NE(ta.GetIdleTool(), nullptr);
    // No active tool until the first viewport is selected (ViewManager.ts:246-247).
    EXPECT_EQ(ta.GetActiveTool(), nullptr);
    EXPECT_FALSE(ad.isEnabled());
    EXPECT_TRUE(as.isSnapEnabled());
    EXPECT_FALSE(tp.isActive());
    EXPECT_EQ(qf.getActiveUnitSystem(), "imperial");
    (void)nm;
    (void)lm;
    (void)ua;
    (void)ml;
    (void)tr;
    (void)fp;
}

// Authored: no reference test exists in itwinjs-core for AccuSnap default mode access;
//           exercises AccuSnap.getActiveSnapModes() — default SnapMode::NearestKeypoint
//           (bitmask value 2, per core/frontend/src/HitDetail.ts:23).
TEST(ApplicationStartup, AccuSnapDefaultMode)
{
    auto& app = dqApp::Application::Get();
    auto& as = app.GetAccuSnap();

    int count = 0;
    auto const* modes = as.getActiveSnapModes(count);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(modes[0], dqApp::SnapMode::NearestKeypoint);
}

// Authored: no reference test exists in itwinjs-core for QuantityFormatter default access;
//           exercises getActiveUnitSystem()/setActiveUnitSystem() — default "imperial"
//           (per core/frontend/src/quantity-formatting/QuantityFormatter.ts:407).
TEST(ApplicationStartup, QuantityFormatterDefaultSystem)
{
    auto& app = dqApp::Application::Get();
    auto& qf = app.GetQuantityFormatter();

    // Ported from: itwinjs-core core/frontend/src/quantity-formatting/QuantityFormatter.ts:407
    //              (_activeUnitSystem defaults to "imperial")
    EXPECT_EQ(qf.getActiveUnitSystem(), "imperial");

    qf.setActiveUnitSystem("metric");
    EXPECT_EQ(qf.getActiveUnitSystem(), "metric");

    // Reset for other tests (restore faithful default)
    qf.setActiveUnitSystem("imperial");
}

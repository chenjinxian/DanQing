// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Application singleton
// Ported from: itwinjs-core core/frontend/src/IModelApp.ts
//              FreeCAD src/Gui/Application.h
#pragma once

#include "Export.h"
#include "AccuDraw.h"
#include "AccuSnap.h"
#include "ElementLocateManager.h"
#include "FormatsProviderManager.h"
#include "MapLayerFormatRegistry.h"
#include "NotificationManager.h"
#include "QuantityFormatter.h"
#include "TerrainProviderRegistry.h"
#include "TentativePoint.h"
#include "ToolAdmin.h"
#include "UiAdmin.h"
#include "ViewManager.h"
#include "EntityState.h"

#include <dqBase/DqEvent.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

class QTimer;  // Qt render-loop timer (defined in Application.cpp); dqApp permits Qt.

namespace dqApp {

// ---------------------------------------------------------------------------
// Application — the global application singleton
//
// Equivalent to itwinjs's IModelApp (all static members, no instances).
// Manages the rendering system, view manager, and event loop.
//
// Ported from: itwinjs-core IModelApp.ts
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT Application {
public:
    // Singleton access (← IModelApp static access)
    static Application& Get();

    // Startup/shutdown (← IModelApp.startup/shutdown)

    // Render system configuration (← IModelApp RenderSystem.Options)
    struct RenderSystemOptions {
        bool dpiAwareViewports = true;
        int antialiasSamples = 4;
        bool debugShaders = false;
        bool logarithmicDepthBuffer = false;
    };

    struct Options {
        std::string applicationId;
        std::string applicationVersion;
        bool noRender = false;
        RenderSystemOptions renderSystemOptions;
    };

    bool Startup(Options const& opts);
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // Engine access (← IModelApp.viewManager, .toolAdmin, etc.)
    ViewManager& GetViewManager() { return m_viewManager; }
    ToolAdmin& GetToolAdmin() { return m_toolAdmin; }
    NotificationManager& GetNotificationManager() { return m_notificationManager; }
    AccuDraw& GetAccuDraw() { return m_accuDraw; }
    AccuSnap& GetAccuSnap() { return m_accuSnap; }
    ElementLocateManager& GetLocateManager() { return m_locateManager; }
    TentativePoint& GetTentativePoint() { return m_tentativePoint; }
    QuantityFormatter& GetQuantityFormatter() { return m_quantityFormatter; }
    UiAdmin& GetUiAdmin() { return m_uiAdmin; }
    MapLayerFormatRegistry& GetMapLayerFormatRegistry() { return m_mapLayerFormatRegistry; }
    TerrainProviderRegistry& GetTerrainProviderRegistry() { return m_terrainProviderRegistry; }
    FormatsProviderManager& GetFormatsProviderManager() { return m_formatsProviderManager; }

    // Configuration access
    std::string const& GetApplicationId() const { return m_applicationId; }
    std::string const& GetApplicationVersion() const { return m_applicationVersion; }
    RenderSystemOptions const& GetRenderSystemOptions() const { return m_renderSystemOptions; }

    // Event loop (← IModelApp.startEventLoop/eventLoop/requestNextAnimation)
    void StartEventLoop();
    void RequestNextAnimation();
    void EventLoop();

    // Whether StartEventLoop has been called (i.e. the event loop is running).
    // Ported from: itwinjs-core IModelApp.isEventLoopStarted (IModelApp.ts).
    // Used by ToolAdmin::addEvent to mirror the TS guard
    // `if (!IModelApp.isEventLoopStarted) return;` (ToolAdmin.ts:794-795).
    bool IsEventLoopStarted() const noexcept { return m_wantEventLoop; }

    // Entity state registration.
    // Ported from: itwinjs-core IModelApp.registerEntityState()
    using EntityFactory = std::function<std::unique_ptr<EntityState>(const dqBase::DqId&)>;
    void RegisterEntityState(const char* classFullName, EntityFactory factory);
    std::unique_ptr<EntityState> LookupEntityClass(const char* classFullName,
                                                    const dqBase::DqId& id) const;

    // Events
    dqBase::DqEvent<> OnAfterStartup;
    dqBase::DqEvent<> OnBeforeShutdown;

private:
    Application();
    ~Application();

    Application(Application const&) = delete;
    Application& operator=(Application const&) = delete;

    void RegisterCoreEntityStates();

    // Core subsystems (← IModelApp static members)
    ViewManager m_viewManager;
    ToolAdmin m_toolAdmin;
    NotificationManager m_notificationManager;
    AccuDraw m_accuDraw;
    AccuSnap m_accuSnap;
    ElementLocateManager m_locateManager;
    TentativePoint m_tentativePoint;
    QuantityFormatter m_quantityFormatter;
    UiAdmin m_uiAdmin;
    MapLayerFormatRegistry m_mapLayerFormatRegistry;
    TerrainProviderRegistry m_terrainProviderRegistry;
    FormatsProviderManager m_formatsProviderManager;

    // Configuration
    std::string m_applicationId;
    std::string m_applicationVersion;
    RenderSystemOptions m_renderSystemOptions;
    std::unordered_map<std::string, EntityFactory> m_entityClasses;

    // State
    bool m_initialized = false;
    bool m_wantEventLoop = false;
    bool m_animationRequested = false;
    QTimer* m_animTimer = nullptr;  // Qt equivalent of requestAnimationFrame → EventLoop()
};

}  // namespace dqApp

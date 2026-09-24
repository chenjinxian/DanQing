// Authored: test-only stub for dqApp::Application (no reference exists in FreeCAD)
// Provides just enough interface for StartupTest.cpp without linking real dqApp modules.
// Ported from: dqApp/PublicAPI/dqApp/Application.h (minimal subset)
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

// Include subsystem stubs BEFORE namespace block to avoid nested namespace issue
#include "ViewManager.h"

namespace dqApp {

// Forward declarations (ViewManager already included above)
class InteractiveTool;
class ToolAdmin;
class NotificationManager;
class AccuDraw;
class AccuSnap;
class ElementLocateManager;
class TentativePoint;
class QuantityFormatter;
class UiAdmin;
class MapLayerFormatRegistry;
class TerrainProviderRegistry;
class FormatsProviderManager;
class EntityState;

// ---------------------------------------------------------------------------
// Stub subsystem classes — only the methods called by StartupTest.cpp
// Full definitions are in separate stub headers (ViewManager.h, etc.)
// ---------------------------------------------------------------------------

class ToolRegistry {
public:
    using ToolType = InteractiveTool* (*)();
    void Register(const char* toolId, ToolType toolClass) { m_tools[toolId] = toolClass; }
    ToolType Find(const char* toolId) const {
        auto it = m_tools.find(toolId);
        return (it != m_tools.end()) ? it->second : nullptr;
    }
private:
    std::unordered_map<std::string, ToolType> m_tools;
};

class InteractiveTool {
public:
    virtual ~InteractiveTool() = default;
    virtual const char* getToolId() const = 0;
};

class Tool {
public:
    virtual ~Tool() = default;
    virtual const char* getToolId() const = 0;
};

class StubTestTool : public InteractiveTool {
public:
    const char* getToolId() const override { return "Select"; }
};

class ToolAdmin {
public:
    ToolAdmin() = default;
    ~ToolAdmin() = default;
    ToolRegistry& GetRegistry() { return m_registry; }
    ToolRegistry const& GetRegistry() const { return m_registry; }
    InteractiveTool* GetActiveTool() const { return m_activeTool; }
    InteractiveTool* GetIdleTool() const { return m_idleTool; }
    const char* GetDefaultToolId() const { return "Select"; }
    // Mirrors real ToolAdmin::OnInitialized (dqApp/src/ToolAdmin.cpp): registers core
    // tools + creates the idle tool — but does NOT start the default tool. Faithful to
    // itwinjs-core ToolAdmin.onInitialized (ToolAdmin.ts:510-525); the default tool is
    // started later by ViewManager.setSelectedView on first viewport selection
    // (ViewManager.ts:246-247).
    void OnInitialized() {
        m_idleTool = &m_stubTool;
    }
private:
    ToolRegistry m_registry;
    InteractiveTool* m_activeTool = nullptr;
    InteractiveTool* m_idleTool = nullptr;
    StubTestTool m_stubTool;
};

class NotificationManager {
public:
    NotificationManager() = default;
    ~NotificationManager() = default;
};

class AccuDraw {
public:
    AccuDraw() = default;
    ~AccuDraw() = default;
    bool isEnabled() const { return false; }
};

enum class SnapMode : uint8_t { Nearest = 0, NearestKeypoint = 1 };

class AccuSnap {
public:
    AccuSnap() = default;
    ~AccuSnap() = default;
    bool isSnapEnabled() const { return true; }
    SnapMode const* getActiveSnapModes(int& count) const {
        count = 1;
        return &m_activeSnapMode;
    }
private:
    SnapMode m_activeSnapMode = SnapMode::NearestKeypoint;
};

class ElementLocateManager {
public:
    ElementLocateManager() = default;
    ~ElementLocateManager() = default;
};

class TentativePoint {
public:
    TentativePoint() = default;
    ~TentativePoint() = default;
    bool isActive() const { return false; }
};

class QuantityFormatter {
public:
    QuantityFormatter() = default;
    ~QuantityFormatter() = default;
    std::string const& getActiveUnitSystem() const { return m_unitSystem; }
    void setActiveUnitSystem(std::string const& s) { m_unitSystem = s; }
private:
    // Mirrors faithful default: itwinjs-core QuantityFormatter.ts:407 ("imperial")
    std::string m_unitSystem = "imperial";
};

class UiAdmin {
public:
    UiAdmin() = default;
    ~UiAdmin() = default;
};

class MapLayerFormatRegistry {
public:
    MapLayerFormatRegistry() = default;
    ~MapLayerFormatRegistry() = default;
};

class TerrainProviderRegistry {
public:
    TerrainProviderRegistry() = default;
    ~TerrainProviderRegistry() = default;
};

class FormatsProviderManager {
public:
    FormatsProviderManager() = default;
    ~FormatsProviderManager() = default;
};

// ---------------------------------------------------------------------------
// Application — stub singleton for test compilation
// Ported from: dqApp/PublicAPI/dqApp/Application.h
// ---------------------------------------------------------------------------
class Application {
public:
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

    static Application& Get() {
        static Application s_instance;
        return s_instance;
    }

    bool Startup(Options const& opts) {
        if (m_initialized) return true;  // duplicate startup is no-op
        m_applicationId = opts.applicationId;
        m_applicationVersion = opts.applicationVersion;
        m_initialized = true;
        m_toolAdmin.GetRegistry().Register("Select", []() -> InteractiveTool* { return nullptr; });
        m_toolAdmin.GetRegistry().Register("Idle", []() -> InteractiveTool* { return nullptr; });
        // itwinjs-core IModelApp.startup → toolAdmin.onInitialized (ToolAdmin.ts:510-525):
        // registers core tools + creates the idle tool; the default tool is NOT started
        // here (it starts on first viewport selection, ViewManager.ts:246-247).
        m_toolAdmin.OnInitialized();
        return true;
    }

    bool isInitialized() const { return m_initialized; }

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

    std::string const& GetApplicationId() const { return m_applicationId; }
    std::string const& GetApplicationVersion() const { return m_applicationVersion; }

private:
    Application() = default;
    ~Application() = default;
    Application(Application const&) = delete;
    Application& operator=(Application const&) = delete;

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

    std::string m_applicationId;
    std::string m_applicationVersion;
    RenderSystemOptions m_renderSystemOptions;
    bool m_initialized = false;
};

} // namespace dqApp

// Ported from: FreeCAD src/Gui/Application.h (minimal subset: view-dispatch + newDocument).
// FreeCAD's Gui::Application is a ~2000-line class; this port implements only the surface the
// command layer needs: sendMsgToActiveView / sendHasMsgToActiveView / activeView / newDocument.
#pragma once

#include <functional>

#include <FCGlobal.h>  // GuiExport macro (empty in DTA)

namespace Gui {

class MainWindow;
class MDIView;

// Ported from: FreeCAD src/Gui/Application.h (class Application, minimal subset)
class GuiExport Application
{
public:
    // Ported from: FreeCAD Application::Instance() singleton
    static Application* Instance();

    // Ported from: FreeCAD Application::setMainWindow (set during startup)
    void setMainWindow(MainWindow* mw) { m_mainWindow = mw; }
    MainWindow* getMainWindow() const { return m_mainWindow; }

    // Ported from: FreeCAD Application.h:215 activeView()
    MDIView* activeView() const;

    // Ported from: FreeCAD Application.h:96/98, Application.cpp:1469/1477
    bool sendMsgToActiveView(const char* pMsg);
    bool sendHasMsgToActiveView(const char* pMsg) const;

    // Ported from: FreeCAD Application::updateActions — refresh command enabled-state.
    void updateActions(bool delay = false);

    // Ported from: FreeCAD Application::newDocument. DTA: creates a new (blank-connection) view
    // via an injectable factory, so the command layer never references View3DInventor (real
    // dqApp) directly. The factory is registered by main.cpp at startup.
    using NewViewFactory = std::function<MDIView*()>;
    void setNewViewFactory(NewViewFactory f) { m_newViewFactory = std::move(f); }
    MDIView* newDocument();

private:
    Application() = default;
    MainWindow* m_mainWindow = nullptr;
    NewViewFactory m_newViewFactory;
};

// Ported from: FreeCAD getGuiApplication() free function (src/Gui/Application.h)
GuiExport Application* getGuiApplication();

}  // namespace Gui

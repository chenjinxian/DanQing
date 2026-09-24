// Ported from: FreeCAD src/Gui/Application.cpp (minimal subset: view-dispatch + newDocument)
#include "Application.h"
#include "MainWindow.h"
#include "MDIView.h"

namespace Gui {

// Ported from: FreeCAD Application::Instance() — function-local static singleton
Application* Application::Instance()
{
    static Application s_instance;
    return &s_instance;
}

// Ported from: FreeCAD Application.h:215 activeView() → getMainWindow()->activeWindow()
MDIView* Application::activeView() const
{
    return m_mainWindow ? m_mainWindow->activeWindow() : nullptr;
}

// Ported from: FreeCAD src/Gui/Application.cpp:1469-1475
bool Application::sendMsgToActiveView(const char* pMsg)
{
    MDIView* pView = m_mainWindow ? m_mainWindow->activeWindow() : nullptr;
    bool res = pView ? pView->onMsg(pMsg) : false;
    updateActions(true);
    return res;
}

// Ported from: FreeCAD src/Gui/Application.cpp:1477-1481
bool Application::sendHasMsgToActiveView(const char* pMsg) const
{
    MDIView* pView = m_mainWindow ? m_mainWindow->activeWindow() : nullptr;
    return pView ? pView->onHasMsg(pMsg) : false;
}

// Ported from: FreeCAD Application::updateActions (calls commandManager->testActive()).
// DTA's private MainWindow::_updateActions() body is exactly m_cmdMgr.testActive(), so call
// the public commandManager().testActive() directly (no public wrapper needed).
//
// The delay=true path is intentionally a no-op: FreeCAD debounces updateActions by 150 ms
// (TimerSingleShot); DTA instead relies on MainWindow's periodic activityTimer (150 ms)
// calling commandManager().testActive(), so command enabled-state stays fresh at runtime
// without per-call re-entry. sendMsgToActiveView() above passes delay=true precisely because
// the activityTimer will pick up the next tick.
void Application::updateActions(bool delay)
{
    if (delay || !m_mainWindow)
        return;
    m_mainWindow->commandManager().testActive();
}

// Ported from: FreeCAD Application::newDocument. DTA: delegate to the registered factory
// (main.cpp registers a BlankConnection View3DInventor creator) and add it to the main window.
MDIView* Application::newDocument()
{
    if (!m_mainWindow || !m_newViewFactory)
        return nullptr;
    MDIView* v = m_newViewFactory();
    if (v)
        m_mainWindow->addWindow(v);
    return v;
}

// Ported from: FreeCAD getGuiApplication() free function
Application* getGuiApplication()
{
    return Application::Instance();
}

}  // namespace Gui

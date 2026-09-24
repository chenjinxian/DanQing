// Ported from: FreeCAD src/Gui/Workbench.h:53-175
#pragma once

#include "MenuManager.h"
#include "ToolBarManager.h"

class QMainWindow;

namespace Gui {

class CommandManager;

// Ported from: FreeCAD src/Gui/Workbench.h:53-143
class Workbench {
public:
    // Non-default destructor: clears s_activeWorkbench if it still points at
    // this. In production the workbench lifetime = app lifetime (no-op); in
    // tests where workbenches are created/destroyed per-test, this prevents
    // s_activeWorkbench from dangling.
    virtual ~Workbench();

    // Ported from: FreeCAD src/Gui/Workbench.cpp:453-480
    bool activate();

    void setMainWindow(QMainWindow* mw) { m_mw = mw; }
    void setManagers(CommandManager* cm, MenuManager* mm, ToolBarManager* tm) {
        m_cm = cm;
        m_mm = mm;
        m_tm = tm;
    }

    virtual MenuItem*    setupMenuBar()  const = 0;
    virtual ToolBarItem* setupToolBars() const = 0;

    // Ported from: FreeCAD src/Gui/Workbench.cpp:360-364 (Workbench::setupContextMenu)
    // Base implementation is a no-op so non-StdWorkbench workbenches (and the DTA
    // TestWorkbench used in MDIChromeTest) don't break. Overrides populate the
    // MenuItem tree based on the recipient ("View", "Tree", ...).
    virtual void setupContextMenu(const char* recipient, MenuItem* item) const;

    // Ported from: FreeCAD src/Gui/WorkbenchManager.cpp (active() accessor)
    // DTA adaptation: FreeCAD tracks the active workbench via the
    // WorkbenchManager singleton; DTA has no WorkbenchManager, so the active
    // workbench is held in a static pointer set by activate(). Returns null if
    // no workbench has been activated (e.g. fresh CommandManager in a unit test).
    static const Workbench* activeWorkbench();

protected:
    QMainWindow*    m_mw = nullptr;
    CommandManager* m_cm = nullptr;
    MenuManager*    m_mm = nullptr;
    ToolBarManager* m_tm = nullptr;

private:
    // Set by activate(); read by activeWorkbench(). DTA has a single static
    // workbench (StdWorkbench in main.cpp) whose lifetime = app lifetime, so the
    // raw pointer is safe.
    static Workbench* s_activeWorkbench;
};

// Ported from: FreeCAD src/Gui/Workbench.h:151-175 + Workbench.cpp:699-906
class StdWorkbench : public Workbench {
public:
    // Ported from: FreeCAD src/Gui/Workbench.cpp:699-844
    MenuItem* setupMenuBar() const override;
    // Ported from: FreeCAD src/Gui/Workbench.cpp:846-906
    ToolBarItem* setupToolBars() const override;
    // Ported from: FreeCAD src/Gui/Workbench.cpp:634-660 (StdWorkbench::setupContextMenu)
    void setupContextMenu(const char* recipient, MenuItem* item) const override;
};

} // namespace Gui

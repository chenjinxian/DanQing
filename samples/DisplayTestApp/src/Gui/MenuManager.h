// Ported from: FreeCAD src/Gui/MenuManager.h
#pragma once

#include <QList>
#include <QString>
#include <string>

class QMenuBar;
class QMenu;

namespace Gui {

class CommandManager;

// Ported from: FreeCAD src/Gui/MenuManager.h:39-68
class MenuItem {
public:
    MenuItem() = default;
    explicit MenuItem(const char* cmd) : m_command(cmd ? cmd : "") {}
    ~MenuItem();

    MenuItem(const MenuItem&) = delete;
    MenuItem& operator=(const MenuItem&) = delete;
    const std::string& command() const { return m_command; }
    void setCommand(const std::string& c) { m_command = c; }
    bool hasItems() const { return !m_items.isEmpty(); }
    const QList<MenuItem*>& getItems() const { return m_items; }
    MenuItem& operator<<(const std::string& cmd);      // append leaf
    MenuItem& operator<<(MenuItem* sub);               // append submenu

private:
    std::string m_command;
    QList<MenuItem*> m_items;
};

// NOTE: FreeCAD's MenuManager::setup(MenuItem*) clears + rebuilds the QMenuBar.
// In this port that top-level loop lives in Workbench::activate (Task 6):
//   bar->clear(); for each top MenuItem: addMenu; recurse via setup(top, menu).
// MenuManager therefore exposes ONLY the recursive builder below.
class MenuManager {
public:
    explicit MenuManager(CommandManager& mgr) : m_mgr(mgr) {}
    // Ported from: FreeCAD src/Gui/MenuManager.cpp:273-338
    void setup(MenuItem* item, QMenu* menu) const;

    // Ported from: FreeCAD src/Gui/MenuManager.cpp:273-338 (setupContextMenu overload)
    // Fills a pre-existing QMenu (rather than rebuilding a QMenuBar) from a
    // MenuItem tree. Used by View3DInventor::contextMenuEvent to render the
    // "View" context group built by StdWorkbench::setupContextMenu.
    // Reuses the recursive setup(MenuItem*, QMenu*) builder — DRY: no logic fork.
    void setupContextMenu(MenuItem* item, QMenu& menu) const;

private:
    CommandManager& m_mgr;
};

} // namespace Gui

// Ported from: FreeCAD src/Gui/MenuManager.cpp
#include "MenuManager.h"
#include "Command.h"
#include <QMenuBar>
#include <QMenu>
#include <QApplication>

namespace Gui {

// Ported from: FreeCAD src/Gui/MenuManager.cpp:47-50
MenuItem::~MenuItem() {
    qDeleteAll(m_items);
}

// Ported from: FreeCAD src/Gui/MenuManager.cpp:158-169
MenuItem& MenuItem::operator<<(const std::string& cmd) {
    auto* item = new MenuItem(cmd.c_str());
    m_items.append(item);
    return *this;
}

MenuItem& MenuItem::operator<<(MenuItem* sub) {
    m_items.append(sub);
    return *this;
}

// Ported from: FreeCAD src/Gui/MenuManager.cpp:273-338
// (recursive submenu builder -- the only method this port needs;
// the menuBar clear+top-level loop is in Workbench::activate, Task 6)
void MenuManager::setup(MenuItem* item, QMenu* menu) const {
    for (MenuItem* child : item->getItems()) {
        const std::string& cmd = child->command();
        if (cmd == "Separator") {
            menu->addSeparator();
            continue;
        }
        if (child->hasItems()) {
            QMenu* sub = menu->addMenu(
                QApplication::translate("Workbench", cmd.c_str()));
            setup(child, sub);
        } else {
            m_mgr.addTo(cmd.c_str(), menu);
        }
    }
}

// Ported from: FreeCAD src/Gui/MenuManager.cpp:273-338 (setupContextMenu overload)
// Fills a QMenu& (context-menu popup) from a MenuItem tree. Reuses setup() —
// the recursive builder is identical for menu-bar submenus and context menus.
// FreeCAD's MenuManager::getInstance()->setupContextMenu(MenuItem*, QMenu&) is
// the context-menu entry point called from NavigationStyle::openPopupMenu; this
// is its DTA equivalent (without the singleton, since DTA's MenuManager is owned
// by MainWindow).
void MenuManager::setupContextMenu(MenuItem* item, QMenu& menu) const {
    setup(item, &menu);
}

} // namespace Gui

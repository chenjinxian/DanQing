// Ported from: FreeCAD src/Gui/ToolBarManager.h:49-94, 167-178
#pragma once
#include <QList>
#include <Base/Parameter.h>
#include <string>
class QToolBar; class QMainWindow;

namespace Gui { class CommandManager; }

namespace Gui {

// Ported from: FreeCAD src/Gui/ToolBarManager.h:49-94
class ToolBarItem {
public:
    enum DefaultVisibility { Visible, Hidden, Unavailable };
    ToolBarItem(ToolBarItem* parent = nullptr, DefaultVisibility v = Visible) : m_visibility(v) {
        if (parent) parent->m_items.append(this);
    }
    ~ToolBarItem() { qDeleteAll(m_items); }
    const std::string& command() const { return m_command; }
    void setCommand(const std::string& c) { m_command = c; }
    DefaultVisibility visibility() const { return m_visibility; }
    const QList<ToolBarItem*>& getItems() const { return m_items; }
    ToolBarItem& operator<<(const std::string& cmd) { auto* item = new ToolBarItem(this, Visible); item->m_command = cmd; return *this; }
private:
    std::string m_command;
    DefaultVisibility m_visibility = Visible;
    QList<ToolBarItem*> m_items;
};

// Ported from: FreeCAD src/Gui/ToolBarManager.h:167-178
class ToolBarManager {
public:
    explicit ToolBarManager(CommandManager& mgr, QMainWindow* mw);
    void setup(ToolBarItem* root);            // create/populate toolbars
    int  toolBarIconSize() const { return 24; }   // Ported from: ToolBarManager.cpp:483 default 24
    // Ported from: FreeCAD src/Gui/ToolBarManager.cpp:853-873 — persist each toolbar's visibility
    // to the parameter group, skipping toolbars whose toggleViewAction is hidden (FreeCAD ignoreSave).
    void saveState() const;
private:
    void setup(ToolBarItem* item, QToolBar* tb);
    CommandManager& m_mgr;
    QMainWindow* m_mw;
    // Ported from: FreeCAD src/Gui/ToolBarManager.h:233 — preferences handle for visibility persistence.
    // FreeCAD path is BaseApp/MainWindow/Toolbars (FreeCAD src/Gui/ToolBarManager.cpp:427).
    ParameterGrp::handle m_hPref;
};

} // namespace Gui

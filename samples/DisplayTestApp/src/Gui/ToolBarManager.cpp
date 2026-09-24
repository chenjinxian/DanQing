// Ported from: FreeCAD src/Gui/ToolBarManager.cpp:680-846
#include "ToolBarManager.h"
#include "Command.h"
#include <QMainWindow>
#include <QToolBar>
#include <QApplication>
#include <App/Application.h>

namespace Gui {

// Ported from: FreeCAD src/Gui/ToolBarManager.cpp:427 — hPref init in ctor.
ToolBarManager::ToolBarManager(CommandManager& mgr, QMainWindow* mw)
    : m_mgr(mgr), m_mw(mw)
{
    m_hPref = App::GetApplication().GetUserParameter()
                  .GetGroup("BaseApp")
                  ->GetGroup("MainWindow")
                  ->GetGroup("Toolbars");
}

// Ported from: FreeCAD src/Gui/ToolBarManager.cpp:680-807 (top level: per toolbar find-or-create)
void ToolBarManager::setup(ToolBarItem* root) {
    if (!root || !m_mw) return;
    for (ToolBarItem* tbItem : root->getItems()) {
        QString name = QString::fromStdString(tbItem->command());
        QToolBar* tb = m_mw->findChild<QToolBar*>(name);
        if (!tb) {
            tb = new QToolBar(QApplication::translate("Workbench", name.toLatin1().constData()), m_mw);
            tb->setObjectName(name);
            tb->setIconSize(QSize(toolBarIconSize(), toolBarIconSize()));
            m_mw->addToolBar(tb);
        }
        tb->clear();
        setup(tbItem, tb);
        tb->setVisible(tbItem->visibility() == ToolBarItem::Visible);
        // Ported from: FreeCAD src/Gui/ToolBarManager.cpp:886 (restoreState) — apply saved
        // visibility override so toolbar state round-trips across sessions.
        tb->setVisible(m_hPref->GetBool(name.toUtf8().constData(), tb->isVisible()));
    }
}

// Ported from: FreeCAD src/Gui/ToolBarManager.cpp:809-846
void ToolBarManager::setup(ToolBarItem* item, QToolBar* tb) {
    for (ToolBarItem* child : item->getItems()) {
        const std::string& cmd = child->command();
        if (cmd == "Separator") { tb->addSeparator(); continue; }
        m_mgr.addTo(cmd.c_str(), tb);
    }
}

// Ported from: FreeCAD src/Gui/ToolBarManager.cpp:853-873 — persist toolbar visibility.
// DanQing adaptation: iterate all QToolBar children of the main window (FreeCAD iterates its
// `toolbarNames` registry; DanQing's ToolBarManager has no such registry — `findChildren`
// is the faithful equivalent for this simplified manager).
void ToolBarManager::saveState() const {
    const QList<QToolBar*> toolbars = m_mw->findChildren<QToolBar*>();
    for (QToolBar* tb : toolbars) {
        // FreeCAD ignoreSave: skip toolbars whose toggleViewAction is not user-visible.
        if (!tb->toggleViewAction()->isVisible()) {
            continue;
        }
        QByteArray tbName = tb->objectName().toUtf8();
        m_hPref->SetBool(tbName.constData(), tb->isVisible());
    }
}

} // namespace Gui

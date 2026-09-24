// Ported from: FreeCAD src/Gui/MainWindow.h
/***************************************************************************
 *   Copyright (c) 2005 Werner Mayer <wmayer[at]users.sourceforge.net>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#pragma once

#include <QByteArray>
#include <QEvent>
#include <QMainWindow>
#include <QMdiArea>
#include <QString>

#include <Base/Console.h>

#include "InputHint.h"

#include "Command.h"

class QMenu;

namespace Gui
{

class MenuManager;
class ToolBarManager;
class MDIView;

/**
 * Identifies which side of the status bar an item belongs to.
 */
enum class StatusBarSlot
{
    Left,
    Right,
};

/**
 * Metadata describing a status-bar item registered through
 * MainWindow::addStatusBarItem().
 */
struct StatusBarItemSpec
{
    QByteArray id;
    QString title;
    StatusBarSlot slot = StatusBarSlot::Right;
    int order = 0;
    bool persistentVisibility = true;
    int stretch = 0;
};

/**
 * MainWindow — FreeCAD UI shell main window.
 * Ported from: FreeCAD src/Gui/MainWindow.h
 */
class GuiExport MainWindow: public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Window);
    ~MainWindow() override;

    bool eventFilter(QObject* o, QEvent* e) override;

    /// Adds an MDI window to the workspace.
    void addWindow(MDIView* view);
    /// Removes an MDI window from the workspace.
    void removeWindow(MDIView* view, bool close = true);

    /// Returns the one and only instance.
    static MainWindow* getInstance();

    /// Ported from: FreeCAD src/Gui/MainWindow.h — Command system accessors
    CommandManager& commandManager() { return m_cmdMgr; }
    MenuManager& menuManager() { return *m_menuMgr; }
    ToolBarManager& toolBarManager() { return *m_toolbarMgr; }
    QMdiArea* mdiArea() const;

    /// Returns the active MDI view, or nullptr if none.
    // Ported from: FreeCAD src/Gui/MainWindow.cpp activeWindow()
    MDIView* activeWindow() const;

    /// Shows a status bar message.
    void showStatus(int type, const QString& message);

    // Deviation: made public for testability (FreeCAD has these as private)
    // Ported from: FreeCAD src/Gui/MainWindow.h — status message types
    enum StatusType { None, Err, Wrn, Pane, Msg, Log, Tmp, Critical };

    void showHints(const std::list<InputHint>& hints = {});
    void hideHints();

    bool isRestoringWindowState() const;

    // Ported from: FreeCAD src/Gui/MainWindow.cpp:2247-2271
    void saveWindowSettings(bool canDelay = false);
    // Ported from: FreeCAD src/Gui/MainWindow.cpp:2224-2245
    void restoreWindowState();
    // Ported from: FreeCAD src/Gui/MainWindow.h:187 / MainWindow.cpp:2119-2222 —
    // geometry + dock state + the single show call. Call AFTER the workbench and
    // toolbars are built (FreeCAD: StartupProcess.cpp:516-518); nothing may
    // resize or re-show the window afterwards.
    void loadWindowSettings();

    // Ported from: FreeCAD src/Gui/MainWindow.h:1708 — populates View→Panels menu
    void populateDockWindowMenu(QMenu* menu);

public Q_SLOTS:
    void setPaneText(int i, QString text);
    void setUserSchema(int userSchema);

    void statusMessageChanged();

    void showMessage(const QString& message, int timeout = 0);
    void setRightSideMessage(const QString& message);
    bool isRightSideMessageVisible() const;

    // Ported from: FreeCAD src/Gui/MainWindow.h:271-296 (MDI window slots)
    // Deviation: made public slots for testability (FreeCAD has these as private);
    // DTA tests exercise them directly.
    /// Arranges all child windows in a tile pattern. Ported from: MainWindow.cpp:970-973
    void tile();
    /// Arranges all the child windows in a cascade pattern. Ported from: MainWindow.cpp:975-978
    void cascade();
    /// Activates the next window in the child window chain. Ported from: MainWindow.cpp:1143-1150
    void activateNextWindow();
    /// Activates the previous window in the child window chain. Ported from: MainWindow.cpp:1152-1159
    void activatePreviousWindow();

    /// Registers and places a widget in the status bar.
    void addStatusBarItem(QWidget* widget, const StatusBarItemSpec& spec);
    /// Removes a previously registered item by id.
    void removeStatusBarItem(const QByteArray& id);
    /// Populates menu with toggle actions for all registered items.
    void buildStatusBarContextMenu(QMenu& menu);
    /// Toggles the visibility of a registered status bar item.
    // Deviation: made public for testability (FreeCAD has these as private)
    void setStatusBarItemEnabled(const QByteArray& id, bool enabled);

    /// Can be called after the caption of an MDIView has changed to update the tab's caption.
    // Ported from: FreeCAD src/Gui/MainWindow.h:149 + MainWindow.cpp:1498-1502
    void tabChanged(MDIView* view);

protected:
    void closeEvent(QCloseEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void timerEvent(QTimerEvent*) override { Q_EMIT timeEvent(); }
    void customEvent(QEvent* e) override;
    bool event(QEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void changeEvent(QEvent* e) override;

private:
    void setupDockWindows();

private Q_SLOTS:
    void _updateActions();
    void clearStatus();

Q_SIGNALS:
    void timeEvent();
    void windowStateChanged(QWidget*);

private:
    void relayoutStatusBar();

    static MainWindow* instance;
    struct MainWindowP* d;

    // Ported from: FreeCAD src/Gui/MainWindow.h — Command system members
    CommandManager m_cmdMgr;
    MenuManager* m_menuMgr = nullptr;
    ToolBarManager* m_toolbarMgr = nullptr;
};

inline MainWindow* getMainWindow()
{
    return MainWindow::getInstance();
}

}  // namespace Gui

/***************************************************************************
 *   Copyright (c) 2007 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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

#include <QObject>
#include <QString>
#include <QList>
#include <QFlags>
#include <FCGlobal.h>

class QDockWidget;
class QWidget;
class QMenu;

namespace Gui
{

// Ported from: FreeCAD src/Gui/DockWindowManager.h:33-44
enum class DockWindowOption : int
{
    Hidden = 0,
    Visible = 1,
    HiddenTabbed = 2,
    VisibleTabbed = 3
};

// Ported from: FreeCAD src/Gui/DockWindowManager.h:45 — `using DockWindowOptions = Base::Flags<DockWindowOption>;`
// DanQing has no Base::Flags; Qt's QFlags<DockWindowOption> provides equivalent testFlag() semantics
// (bit-AND compare). Enum values are bit-encoded: bit0=Visible, bit1=Tabbed — so testFlag() works.
Q_DECLARE_FLAGS(DockWindowOptions, DockWindowOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(DockWindowOptions)

// Ported from: FreeCAD src/Gui/DockWindowManager.h:47-53
struct DockWindowItem
{
    QString name;
    Qt::DockWidgetArea pos;
    bool visibility;
    bool tabbed;
};

// Ported from: FreeCAD src/Gui/DockWindowManager.h:55-69
class DockWindowItems
{
public:
    DockWindowItems() = default;
    ~DockWindowItems() = default;

    void addDockWidget(const char* name, Qt::DockWidgetArea pos, DockWindowOptions option);
    void setDockingArea(const char* name, Qt::DockWidgetArea pos);
    void setVisibility(const char* name, bool v);
    void setVisibility(bool v);
    const QList<DockWindowItem>& dockWidgets() const { return m_items; }

private:
    QList<DockWindowItem> m_items;
};

// Ported from: FreeCAD src/Gui/DockWindowManager.h:75-138
class GuiExport DockWindowManager: public QObject
{
    Q_OBJECT

public:
    static DockWindowManager* instance();

    bool registerDockWindow(const char* name, QWidget* widget);
    QDockWidget* addDockWindow(const char* name, QWidget* widget, Qt::DockWidgetArea pos = Qt::AllDockWidgetAreas);

    // Ported from: FreeCAD src/Gui/DockWindowManager.h:87
    void setup(DockWindowItems* items);
    // Ported from: FreeCAD src/Gui/DockWindowManager.h:113-114
    void saveState();
    void loadState();

    // Ported from: FreeCAD src/Gui/MainWindow.h:1708 — populates View→Panels menu
    static void populateDockWindowMenu(QMenu* menu);

private Q_SLOTS:
    void onDockWidgetDestroyed(QObject*);
    void onWidgetDestroyed(QObject*);

private:
    // Ported from: FreeCAD src/Gui/DockWindowManager.h:130-131
    QDockWidget* findDockWidget(const QList<QDockWidget*>& docked, const QString& name) const;
    void tabifyDockWidgets(DockWindowItems* items);

    DockWindowManager();
    ~DockWindowManager() override;
    static DockWindowManager* _instance;
    struct DockWindowManagerP* d;
};

}  // namespace Gui

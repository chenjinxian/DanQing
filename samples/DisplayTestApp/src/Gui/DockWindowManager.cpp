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

#include <QDockWidget>
#include <QMap>
#include <QMenu>
#include <QPointer>
#include <array>

#include <App/Application.h>

#include "DockWindowManager.h"
#include "MainWindow.h"
#include "OverlayManager.h"
#include "OverlayWidgets.h"

using namespace Gui;

namespace Gui
{
struct DockWindowManagerP
{
    QList<QDockWidget*> _dockedWindows;
    QMap<QString, QPointer<QWidget>> _dockWindows;
    DockWindowItems _dockWindowItems;
    ParameterGrp::handle _hPref;
    QPointer<OverlayManager> overlayManager;
};
}  // namespace Gui

DockWindowManager* DockWindowManager::_instance = nullptr;

DockWindowManager* DockWindowManager::instance()
{
    if (!_instance) {
        _instance = new DockWindowManager;
    }
    return _instance;
}

DockWindowManager::DockWindowManager()
{
    d = new DockWindowManagerP;
    d->_hPref = App::GetApplication().GetUserParameter()
                    .GetGroup("BaseApp")
                    ->GetGroup("MainWindow")
                    ->GetGroup("DockWindows");
    d->overlayManager = OverlayManager::instance();
}

DockWindowManager::~DockWindowManager()
{
    d->_dockedWindows.clear();
    delete d;
}

bool DockWindowManager::registerDockWindow(const char* name, QWidget* widget)
{
    QMap<QString, QPointer<QWidget>>::Iterator it = d->_dockWindows.find(QString::fromUtf8(name));
    if (it != d->_dockWindows.end() || !widget) {
        return false;
    }
    d->_dockWindows[QString::fromUtf8(name)] = widget;
    widget->hide();
    return true;
}

QDockWidget* DockWindowManager::addDockWindow(const char* name, QWidget* widget, Qt::DockWidgetArea pos)
{
    if (!widget) {
        return nullptr;
    }
    QDockWidget* dw = qobject_cast<QDockWidget*>(widget->parentWidget());
    if (dw) {
        return dw;
    }

    MainWindow* mw = getMainWindow();
    dw = new QDockWidget(mw);
    dw->setTitleBarWidget(OverlayTitleBar::createTitleBar(dw));
    dw->hide();
    switch (pos) {
        case Qt::LeftDockWidgetArea:
        case Qt::RightDockWidgetArea:
        case Qt::TopDockWidgetArea:
        case Qt::BottomDockWidgetArea:
            mw->addDockWidget(pos, dw);
            break;
        default:
            break;
    }
    connect(dw, &QObject::destroyed, this, &DockWindowManager::onDockWidgetDestroyed);
    connect(widget, &QObject::destroyed, this, &DockWindowManager::onWidgetDestroyed);

    widget->setParent(dw);
    dw->setWidget(widget);

    dw->setObjectName(QString::fromUtf8(name));
    dw->setWindowTitle(widget->windowTitle());
    dw->setFeatures(
        QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable
    );

    d->_dockedWindows.push_back(dw);

    if (d->overlayManager) {
        d->overlayManager->initDockWidget(dw);
    }

    // Ported from: FreeCAD src/Gui/DockWindowManager.cpp:303-313
    // Auto-save dock visibility on toggle, location change, and float change
    connect(dw->toggleViewAction(), &QAction::triggered, this, [this, dw]() {
        QByteArray dockName = dw->toggleViewAction()->data().toByteArray();
        d->_hPref->SetBool(dockName.constData(), dw->isVisible());
    });

    // Ported from: FreeCAD src/Gui/DockWindowManager.cpp:309-313 — full window-state save on
    // dock move/float, debounced via saveWindowSettings(true) (canDelay=true → 100ms timer).
    auto cb = []() { getMainWindow()->saveWindowSettings(true); };
    connect(dw, &QDockWidget::topLevelChanged, cb);
    connect(dw, &QDockWidget::dockLocationChanged, cb);

    return dw;
}

void DockWindowManager::onDockWidgetDestroyed(QObject* dw)
{
    for (QList<QDockWidget*>::Iterator it = d->_dockedWindows.begin(); it != d->_dockedWindows.end();
         ++it) {
        if (*it == dw) {
            d->_dockedWindows.erase(it);
            break;
        }
    }
}

void DockWindowManager::onWidgetDestroyed(QObject* widget)
{
    for (QList<QDockWidget*>::Iterator it = d->_dockedWindows.begin(); it != d->_dockedWindows.end();
         ++it) {
        if ((*it)->widget() == widget) {
            QDockWidget* dw = *it;
            dw->deleteLater();
            break;
        }
    }
}

// ─── DockWindowItems ────────────────────────────────────────────────

// Ported from: FreeCAD src/Gui/DockWindowManager.cpp:48-56
void DockWindowItems::addDockWidget(const char* name, Qt::DockWidgetArea pos, DockWindowOptions option)
{
    DockWindowItem item;
    item.name = QString::fromUtf8(name);
    item.pos = pos;
    item.visibility = option.testFlag(DockWindowOption::Visible);
    item.tabbed = option.testFlag(DockWindowOption::HiddenTabbed);
    m_items.append(item);
}

// Ported from: FreeCAD src/Gui/DockWindowManager.cpp:58-66
void DockWindowItems::setDockingArea(const char* name, Qt::DockWidgetArea pos)
{
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (it->name == QString::fromUtf8(name)) {
            it->pos = pos;
            break;
        }
    }
}

// Ported from: FreeCAD src/Gui/DockWindowManager.cpp:68-76
void DockWindowItems::setVisibility(const char* name, bool v)
{
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (it->name == QString::fromUtf8(name)) {
            it->visibility = v;
            break;
        }
    }
}

// Ported from: FreeCAD src/Gui/DockWindowManager.cpp:78-83
void DockWindowItems::setVisibility(bool v)
{
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        it->visibility = v;
    }
}

// ─── DockWindowManager setup / tabify / save / load ──────────────────

// Ported from: FreeCAD src/Gui/DockWindowManager.cpp:516-551
void DockWindowManager::setup(DockWindowItems* items)
{
    // save state of current dock windows
    saveState();
    d->_dockWindowItems = *items;

    QList<QDockWidget*> docked = d->_dockedWindows;
    const QList<DockWindowItem>& dws = items->dockWidgets();
    for (const auto& it : dws) {
        QDockWidget* dw = findDockWidget(docked, it.name);
        QByteArray dockName = it.name.toLatin1();
        bool visible = d->_hPref->GetBool(dockName.constData(), it.visibility);

        if (!dw) {
            QMap<QString, QPointer<QWidget>>::Iterator jt = d->_dockWindows.find(it.name);
            if (jt != d->_dockWindows.end()) {
                dw = addDockWindow(jt.value()->objectName().toUtf8(), jt.value(), it.pos);
                jt.value()->show();
                dw->toggleViewAction()->setData(it.name);
                dw->setVisible(visible);
            }
        }
        else {
            dw->setVisible(visible);
            dw->toggleViewAction()->setVisible(true);
            int index = docked.indexOf(dw);
            docked.removeAt(index);
        }

        if (d->overlayManager && dw && visible && !dw->titleBarWidget()) {
            d->overlayManager->setupDockWidget(dw);
        }
    }

    tabifyDockWidgets(items);
}

// Ported from: FreeCAD src/Gui/DockWindowManager.cpp:553-603
void DockWindowManager::tabifyDockWidgets(DockWindowItems* items)
{
    // Tabify dock widgets only once to avoid overriding the current layout
    static bool tabify = false;
    if (tabify) {
        return;
    }

    std::array<QList<QDockWidget*>, 4> areas;
    const QList<DockWindowItem>& dws = items->dockWidgets();
    QList<QDockWidget*> docked = d->_dockedWindows;
    for (const auto& it : dws) {
        QDockWidget* dw = findDockWidget(docked, it.name);
        if (it.tabbed && dw) {
            Qt::DockWidgetArea pos = getMainWindow()->dockWidgetArea(dw);
            switch (pos) {
                case Qt::LeftDockWidgetArea:
                    areas[0] << dw;
                    break;
                case Qt::RightDockWidgetArea:
                    areas[1] << dw;
                    break;
                case Qt::TopDockWidgetArea:
                    areas[2] << dw;
                    break;
                case Qt::BottomDockWidgetArea:
                    areas[3] << dw;
                    break;
                default:
                    break;
            }
        }
    }

    // tabify dock widgets for which "tabbed" is true and which have the same position
    for (auto& area : areas) {
        for (auto it : area) {
            if (it != area.front()) {
                getMainWindow()->tabifyDockWidget(area.front(), it);
                tabify = true;
            }
        }

        // activate the first of the tabbed dock widgets
        if (area.size() > 1) {
            area.front()->raise();
        }
    }
}

// Ported from: FreeCAD src/Gui/DockWindowManager.cpp:605-615
void DockWindowManager::saveState()
{
    const QList<DockWindowItem>& dockItems = d->_dockWindowItems.dockWidgets();
    for (auto it = dockItems.begin(); it != dockItems.end(); ++it) {
        QDockWidget* dw = findDockWidget(d->_dockedWindows, it->name);
        if (dw) {
            QByteArray dockName = dw->toggleViewAction()->data().toByteArray();
            d->_hPref->SetBool(dockName.constData(), dw->isVisible());
        }
    }
}

// Ported from: FreeCAD src/Gui/DockWindowManager.cpp:617-633
void DockWindowManager::loadState()
{
    const QList<DockWindowItem>& dockItems = d->_dockWindowItems.dockWidgets();
    for (auto it = dockItems.begin(); it != dockItems.end(); ++it) {
        QDockWidget* dw = findDockWidget(d->_dockedWindows, it->name);
        if (dw) {
            QByteArray dockName = it->name.toUtf8();
            bool visible = d->_hPref->GetBool(dockName.constData(), it->visibility);
            dw->setVisible(visible);
        }
    }
}

// Ported from: FreeCAD src/Gui/DockWindowManager.cpp:635-644
QDockWidget* DockWindowManager::findDockWidget(const QList<QDockWidget*>& docked, const QString& name) const
{
    for (auto it = docked.begin(); it != docked.end(); ++it) {
        if ((*it)->toggleViewAction()->data().toString() == name) {
            return *it;
        }
    }

    return nullptr;
}

// Ported from: FreeCAD src/Gui/MainWindow.cpp:1708-1718
void DockWindowManager::populateDockWindowMenu(QMenu* menu)
{
    if (!menu) return;

    MainWindow* mw = getMainWindow();
    if (!mw) return;

    QList<QDockWidget*> dock = mw->findChildren<QDockWidget*>();
    for (auto& it : dock) {
        QAction* action = it->toggleViewAction();
        action->setToolTip(QObject::tr("Toggles this dockable window"));
        action->setStatusTip(QObject::tr("Toggles this dockable window"));
        action->setWhatsThis(QObject::tr("Toggles this dockable window"));
        menu->addAction(action);
    }
}

#include "moc_DockWindowManager.cpp"

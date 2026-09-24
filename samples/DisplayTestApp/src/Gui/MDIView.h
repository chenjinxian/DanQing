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

#include <QMainWindow>
#include "View.h"

namespace Gui
{

class Document;

/**
 * MDIView — base class for MDI windows in the FreeCAD UI shell.
 * Ported from: FreeCAD src/Gui/MDIView.h
 */
class GuiExport MDIView: public QMainWindow, public BaseView
{
    Q_OBJECT

    TYPESYSTEM_HEADER_WITH_OVERRIDE();

public:
    MDIView(Gui::Document* pcDocument, QWidget* parent, Qt::WindowFlags wflags = Qt::WindowFlags());
    ~MDIView() override;

    void onRelabel(Gui::Document* pDoc) override;
    bool onMsg(const char* pMsg) override;
    bool onHasMsg(const char* pMsg) const override;

    enum ViewMode { Child, TopLevel, FullScreen };
    ViewMode currentViewMode() const { return currentMode; }

    // Ported from: FreeCAD src/Gui/MDIView.cpp:434-517
    // If mode is FullScreen the MDI view is displayed in full screen mode, if
    // mode is TopLevel then it is displayed as its own top-level window,
    // otherwise (Child) as a tabbed MDI sub-window.
    virtual void setCurrentViewMode(ViewMode mode);

Q_SIGNALS:
    void message(const QString&, int);

protected:
    void closeEvent(QCloseEvent* e) override;
    void changeEvent(QEvent* e) override;

    virtual void windowStateChanged(QWidget*);

private:
    ViewMode currentMode;
    // Ported from: FreeCAD src/Gui/MDIView.h:206 — backup of the window state
    // used to restore the maximize flag when re-entering TopLevel mode.
    Qt::WindowStates wstate;

    friend class MainWindow;
};

}  // namespace Gui

/***************************************************************************
 *   Copyright (c) 2004 Jürgen Riegel <juergen.riegel@web.de>              *
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

// View3DInventor — FreeCAD MDI 3D view, modified for DanQing
//
// Original: FreeCAD src/Gui/View3DInventor.h
// Modification: Replaces View3DInventorViewer (Coin3D) with dqApp::Viewport (dqRender)
//
// This is the MDI window that contains the 3D viewport. It inherits from
// FreeCAD's MDIView (QMainWindow) and wraps a dqApp::Viewport instead of
// the original Coin3D-based View3DInventorViewer.
//
// The FreeCAD UI framework (MainWindow, QMdiArea, QToolBar, QDockWidget)
// is preserved. Only the rendering backend is replaced.

#pragma once

#include <dqBase/RefCounted.h>  // RefPtr — owner of the BlankConnection (see m_connection)
#include <dqGeom/Range3d.h>    // resetBlankConnection 参数

#include <memory>  // std::unique_ptr — owner of the glTF decoration (see m_gltfDecoration)
#include <string>  // resetBlankConnection 参数

#include <QImage>
#include <QMainWindow>
#include <QStackedWidget>
#include <QString>

#include "MDIView.h"

// Forward declarations — dqApp replaces Coin3D
namespace dqApp {
    class Viewport;
    class IModelConnection;
    class GltfDecoration;
}

class QCloseEvent;
class QTimer;

namespace Gui
{

class Document;
class GeometryDecorator;   // DecorationGeometryExample.h — owned by m_geoDecorator

// ---------------------------------------------------------------------------
// View3DInventor — MDI 3D viewport window
//
// Ported from: FreeCAD src/Gui/View3DInventor.h
// Modified: viewer from View3DInventorViewer (Coin3D) to dqApp::Viewport (dqRender)
// ---------------------------------------------------------------------------
class View3DInventor : public MDIView
{
    Q_OBJECT

public:
    // Construction — creates dqApp::Viewport as the rendering widget.
    // Ported from: FreeCAD View3DInventor constructor
    // Modified: replaced View3DInventorViewer with dqApp::Viewport
    View3DInventor(Gui::Document* pcDocument, QWidget* parent,
                   dqApp::IModelConnection* connection = nullptr,
                   Qt::WindowFlags wflags = Qt::WindowFlags());
    ~View3DInventor() override;

    // Clone — creates a new View3DInventor with the same view state.
    // Ported from: FreeCAD View3DInventor::clone()
    View3DInventor* clone();

    // Message handler — routes FreeCAD messages to dqApp operations.
    // Ported from: FreeCAD View3DInventor::onMsg()
    bool onMsg(const char* pMsg) override;
    bool onHasMsg(const char* pMsg) const override;

    // Update — triggers a repaint.
    // Ported from: FreeCAD View3DInventor::onUpdate()
    void onUpdate() override;

    // View operations — delegate to dqApp::Viewport.
    // Ported from: FreeCAD View3DInventor::viewAll()
    void viewAll();

    // Load a glTF file and install it as a pickable decoration on this view's
    // viewport; replaces any previously imported glTF decoration.
    // Returns false on load/install failure.
    // Ported from: itwinjs-core GltfDecoration.ts:157-208 (GltfDecorationTool.run)
    bool loadGltf(QString const& path);

    // View-mode switch — reparents the GL viewport correctly across
    // Child↔TopLevel↔FullScreen transitions.
    // Ported from: FreeCAD src/Gui/View3DInventor.cpp:723-768
    // Modified: operates on dqApp::Viewport (m_viewport) instead of Coin3D
    // View3DInventorViewer::getGLWidget().
    void setCurrentViewMode(ViewMode mode) override;

    // Get the dqApp::Viewport.
    dqApp::Viewport* getUeViewport() const { return m_viewport; }

    // Get the installed glTF decoration (nullptr if none). Non-owning; the
    // unique_ptr member stays the owner. Tests read GetPickableId() to anchor
    // pick/hilite/selection pixel regressions.
    dqApp::GltfDecoration* gltfDecoration() const { return m_gltfDecoration.get(); }

    // Get the installed Decoration Geometry Example decorator (nullptr if
    // none). Non-owning; the unique_ptr member stays the owner.
    Gui::GeometryDecorator* geoDecorator() const { return m_geoDecorator.get(); }
    // Install ownership (openDecorationGeometryExample → the view holds the
    // decorator; ViewManager registration is non-owning). Out-of-line: the
    // unique_ptr reset needs the complete type (View3DInventor.cpp includes
    // DecorationGeometryExample.h).
    void setGeoDecorator(std::unique_ptr<Gui::GeometryDecorator> deco);

    // Get the IModelConnection.
    dqApp::IModelConnection* getConnection() const { return m_connection.Get(); }

    // Ported from: itwinjs-core Surface.ts openBlankConnection(:183-192)——示例以
    // 自带 extents 的**新** blank connection 运行（IModelConnection.projectExtents
    // 创建后不可变；Fit/取景都读它）。换绑连接 + 重建默认视图（ctor 的
    // ViewList→getDefaultView 链同款），无动画（参考是新 viewer，无从旧视图的过渡）。
    void resetBlankConnection(dqGeom::Range3d const& extents, std::string const& name);

    const char* getName() const override;

Q_SIGNALS:
    void viewportCreated();

protected:
    // Ported from: FreeCAD src/Gui/Navigation/NavigationStyle.cpp:2422-2487
    //              (NavigationStyle::openPopupMenu — DTA contextMenuEvent)
    // Modified: no Coin3D SoRayPickAction (GL — TODO: deferred, implement via
    //           itwinjs-core; see design 2026-07-16 §5); no nav-style submenu
    //           (spec §4.4 OUT). Builds the "View" context group via
    //           CommandManager::setupContextMenu → active Workbench, renders via
    //           MenuManager::setupContextMenu, pops up at the click position.
    // Close the MDI view: drop the viewport from the ViewManager and release
    // its GL pipeline SYNCHRONOUSLY, before Qt hides/destroys the native NSView
    // (crash-on-close: otherwise the render timer calls RenderFrame -> acquire
    // -> [ctx setView:<freed NSView>] on a viewport Qt has torn down). Idempotent
    // with ~View3DInventor (DropViewport) and ~Viewport (Shutdown).
    void closeEvent(QCloseEvent* e) override;

    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    void setupToolBar();

private:
    dqApp::Viewport* m_viewport;        // ← replaces View3DInventorViewer
    // Owns the IModelConnection for this view's lifetime. The connection is
    // RefCounted; BlankConnection::create() returns a RefPtr whose only holder
    // would otherwise be a ctor-local that drops at end-of-scope — freeing the
    // connection and leaving ViewState::m_iModel (a raw, "not owned" pointer)
    // dangling. The first selection then dereferences freed memory
    // (SelectionSet::OnChanged.m_mutex garbage → recursive_mutex EINVAL crash).
    // Mirrors itwinjs-core, where the viewing host retains the IModelConnection.
    dqBase::RefPtr<dqApp::IModelConnection> m_connection;
    // Owns the installed glTF decoration (if any). ViewManager::AddDecorator holds a
    // NON-OWNING pointer to the decorator, so it must be DropDecorator'd BEFORE this
    // unique_ptr resets — done in loadGltf (re-import) and ~View3DInventor.
    std::unique_ptr<dqApp::GltfDecoration> m_gltfDecoration;
    // Owns the Decoration Geometry Example decorator (if installed). Same
    // non-owning ViewManager registration → same closeEvent/reset discipline as
    // m_gltfDecoration (the reference's iModel.onClose → dispose, :49).
    std::unique_ptr<Gui::GeometryDecorator> m_geoDecorator;
    QStackedWidget* m_stack;
};

}  // namespace Gui

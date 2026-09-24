// Ported from: FreeCAD src/Gui/OverlayManager.h
// Minimal stub for DisplayTestApp UI shell
#pragma once
#include <FCGlobal.h>
#include <QObject>
class QDockWidget;
namespace Gui {
class GuiExport OverlayManager : public QObject {
    Q_OBJECT
public:
    static OverlayManager* instance() { static OverlayManager m; return &m; }
    void initDockWidget(QDockWidget*) {}
    void setupDockWidget(QDockWidget*, int = 0) {}
    void unsetupDockWidget(QDockWidget*) {}
    void save() {}  // No-op stub for MainWindow::saveWindowSettings
};
}

// MDIView.cpp — minimal stub for FreeCAD UI shell
// Ported from: FreeCAD src/Gui/MDIView.cpp

#include "MDIView.h"
#include "MainWindow.h"
#include <QCloseEvent>
#include <QMdiSubWindow>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QString>

using namespace Gui;

#include "moc_MDIView.cpp"

TYPESYSTEM_SOURCE_ABSTRACT(Gui::MDIView, Gui::BaseView)

MDIView::MDIView(Gui::Document*, QWidget* parent, Qt::WindowFlags wflags)
    : QMainWindow(parent, wflags), BaseView(nullptr), currentMode(Child), wstate(Qt::WindowNoState)
{
    // Establish the [*] dirty-marker convention upfront. FreeCAD's MDIView
    // ctor (MDIView.cpp:57-75) does not set a title — the [*] title is set
    // later by onRelabel when a real Gui::Document triggers it. DTA has no
    // live Document::onRelabel path yet, so the ctor establishes the
    // convention using the placeholder doc name "Unnamed" (FreeCAD
    // MainWindow.cpp:2642 default doc name). Subclasses (StartView) override
    // via setWindowTitle; onRelabel overwrites with a real label later.
    // Ported from: FreeCAD src/Gui/MDIView.cpp:172-199 ([*] convention).
    setWindowTitle(QStringLiteral("Unnamed[*]"));
}

MDIView::~MDIView() {}

// Ported from: FreeCAD src/Gui/MDIView.cpp:172-199
void MDIView::onRelabel(Gui::Document* pDoc)
{
    Q_UNUSED(pDoc)
    if (!bIsPassive) {
        // TODO: deferred — real Document::Label plumbing.
        // FreeCAD: QString docLabel = QString::fromUtf8(pDoc->getDocument()->Label.getValue());
        // DTA stub: Gui::Document is forward-declared and App::Document::Label
        // is absent. Using the placeholder "Unnamed" (FreeCAD
        // MainWindow.cpp:2642 default doc name) until real Gui::Document
        // plumbing lands.
        QString docLabel = QStringLiteral("Unnamed");

        // Try to separate document name and view number if there is one.
        QString cap = windowTitle();
        // Either with dirty flag ...
        QRegularExpression rx(QStringLiteral(R"((\s\:\s\d+\[\*\])$)"));
        QRegularExpressionMatch match;
        // FreeCAD uses boost::ignore_unused on the lastIndexOf return value
        // (MDIView.cpp:181/186); DTA uses static_cast<void> per §9 (禁 C 风格
        // 转换). The match out-param is the desired side-effect.
        static_cast<void>(cap.lastIndexOf(rx, -1, &match));
        if (!match.hasMatch()) {
            // ... or not
            rx.setPattern(QStringLiteral(R"((\s\:\s\d+)$)"));
            static_cast<void>(cap.lastIndexOf(rx, -1, &match));
        }
        if (match.hasMatch()) {
            cap = docLabel + match.captured();
            setWindowTitle(cap);
        }
        else {
            cap = QStringLiteral("%1[*]").arg(docLabel);
            setWindowTitle(cap);
        }
    }
}

bool MDIView::onMsg(const char*) { return false; }

bool MDIView::onHasMsg(const char*) const { return false; }

void MDIView::windowStateChanged(QWidget*) {}

void MDIView::closeEvent(QCloseEvent* e) { QMainWindow::closeEvent(e); }

void MDIView::changeEvent(QEvent* e) { QMainWindow::changeEvent(e); }

// Ported from: FreeCAD src/Gui/MDIView.cpp:434-517 (setCurrentViewMode)
//
// Core window-management for the Child↔TopLevel↔FullScreen mode transitions.
// The FreeCAD action-copy + qApp event filter (:491-500, :406-427) is deferred
// — see "action-copy deferred" note below.
void MDIView::setCurrentViewMode(ViewMode mode)
{
    const ViewMode oldmode = MDIView::currentViewMode();
    if (oldmode == mode) {
        return;
    }

    if (oldmode == Child) {
        // remove window from MDIArea
        if (qobject_cast<QMdiSubWindow*>(parentWidget())) {
            getMainWindow()->removeWindow(this, false);
            setParent(nullptr);
        }
    }
    else if (oldmode == TopLevel) {
        // backup maximize state for top-level mode
        wstate = windowState();
    }

    switch (mode) {
        // go to normal mode
        case Child:
            getMainWindow()->addWindow(this);
            break;

        // go to top-level mode
        case TopLevel:
            if (wstate & Qt::WindowMaximized) {
                // Only calling showMaximized doesn't work when the widget is currently in
                // full-screen mode. We need to exit full-screen mode first or the widget will end
                // up in normal mode. Same if the window is in child mode but maximized.
                setWindowState(windowState() & ~(Qt::WindowMaximized | Qt::WindowFullScreen));
                showMaximized();
            }
            else {
                showNormal();
            }
            break;

        // go to full-screen mode
        case FullScreen:
            showFullScreen();
            break;
    }

    currentMode = mode;

    activateWindow();

    // Ported from: FreeCAD MDIView.cpp:460-500 (minimal subset — full action-copy deferred).
    // FreeCAD, when leaving Child mode, copies every mainwindow action onto this view and
    // installs a qApp event filter (MDIView.cpp:406-427) so global shortcuts keep working
    // while the view is undocked; when re-entering Child mode it removes the filter and
    // strips the action list. DTA does not yet wire global shortcuts through to top-level
    // views, so the action-copy + event filter is deferred. The layout kick below is kept
    // because it is needed for correct reparenting geometry, independent of actions.

    if (mode == Child) {
        // When switching from undocked to docked mode, the widget position is somehow not updated
        // correctly. In this case mapToGlobal(Point()) returns {0, 0} even though the widget is
        // clearly not at the top-left corner of the screen. We fix this by briefly changing the
        // maximum size of the widget.
        const auto oldsize = maximumSize();
        setMaximumSize({1, 1});
        setMaximumSize(oldsize);
    }
}

// MainWindow.cpp — 1:1 port of FreeCAD src/Gui/MainWindow.cpp
// Core UI functions ported; non-UI functions stubbed
// Ported from: FreeCAD src/Gui/MainWindow.cpp

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


#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QGridLayout>
#include <QHash>
#include <QKeySequence>
#include <QLabel>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMenuBar>
#include <QProgressBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>

#include <algorithm>
#include <sstream>
#include <vector>

#include <App/Application.h>
#include <Base/Parameter.h>

#include "MainWindow.h"
#include "MainWindow_p.h"
#include "Tree.h"
#include "Window.h"
#include "MDIView.h"
#include "DockWindowManager.h"
#include "OverlayManager.h"
#include "InputHintWidget.h"
#include "StatusBarLabel.h"
#include "Command.h"
#include "MenuManager.h"
#include "ToolBarManager.h"
#include "PropertyView.h"

using namespace Gui;

MainWindow* MainWindow::instance = nullptr;

// Ported from: FreeCAD src/Gui/MainWindow.cpp MainWindow::MainWindow()
MainWindow::MainWindow(QWidget* parent, Qt::WindowFlags f)
    : QMainWindow(parent, f)
{
    d = new MainWindowP;
    d->activeView = nullptr;
    instance = this;

    d->hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/MainWindow"
    );
    d->hStatusBar = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/StatusBar"
    );

    setDockOptions(dockOptions() | QMainWindow::GroupedDragging);

    // Create the layout containing the workspace and a tab bar
    // Ported from: FreeCAD src/Gui/MainWindow.cpp lines 418-438
    d->mdiArea = new QMdiArea();
    d->mdiArea->setTabsMovable(true);
    d->mdiArea->setTabPosition(QTabWidget::South);
    d->mdiArea->setViewMode(QMdiArea::TabbedView);
    auto tab = d->mdiArea->findChild<QTabBar*>();
    if (tab) {
        tab->setTabsClosable(true);
        tab->setExpanding(false);
        tab->setObjectName(QStringLiteral("mdiAreaTabBar"));
    }

    // Deferred configuration — tab bar may be recreated after first subwindow
    QTimer::singleShot(0, this, [this]() {
        auto* tab = d->mdiArea->findChild<QTabBar*>();
        if (tab) {
            tab->setTabsClosable(true);
            tab->setExpanding(false);
            tab->setObjectName(QStringLiteral("mdiAreaTabBar"));
            // Move close button to right side of tab
            for (int i = 0; i < tab->count(); ++i) {
                QWidget* btn = tab->tabButton(i, QTabBar::LeftSide);
                if (btn) {
                    tab->setTabButton(i, QTabBar::LeftSide, nullptr);
                    tab->setTabButton(i, QTabBar::RightSide, btn);
                }
            }
        }
    });
    d->mdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    d->mdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    d->mdiArea->setOption(QMdiArea::DontMaximizeSubWindowOnActivation, false);
    // Ported from: FreeCAD src/Gui/MainWindow.cpp:434 — HAS_QTBUG_129596 guard.
    // DTA has no equivalent CMake detection; if your Qt build exhibits QTBUG-129596
    // (setActivationOrder corrupts sub-window activation), define HAS_QTBUG_129596
    // via target_compile_definitions to skip this call. Default: call it (FreeCAD's
    // behaviour when the bug is absent).
#ifndef HAS_QTBUG_129596
    d->mdiArea->setActivationOrder(QMdiArea::ActivationHistoryOrder);
#endif
    d->mdiArea->setBackground(QBrush(QColor(160, 160, 160)));
    setCentralWidget(d->mdiArea);

    // Ported from: FreeCAD Application::viewActivated signal wiring.
    // Keep d->activeView in sync with the MDI area's active sub-window so
    // MainWindow::activeWindow() always reflects the focused view.
    connect(d->mdiArea, &QMdiArea::subWindowActivated, this, [this](QMdiSubWindow* sub) {
        d->activeView = sub ? qobject_cast<MDIView*>(sub->widget()) : nullptr;
    });

    // Ported from: FreeCAD src/Gui/MainWindow.cpp:440-584 (status bar setup)
    // Status bar — widgets registered via addStatusBarItem for context menu / persistence / ordering
    statusBar()->setObjectName(QStringLiteral("statusBar"));

    // actionLabel — Left, order 0, stretch 1 (Preselection)
    d->actionLabel = new StatusBarLabel(statusBar());
    d->actionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    d->actionLabel->setElideMode(Qt::ElideRight);
    d->actionLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    d->actionLabel->setWindowTitle(tr("Preselection"));
    addStatusBarItem(d->actionLabel, StatusBarItemSpec("Preselection", QString(), StatusBarSlot::Left, 0, true, 1));

    // hintLabel — Left, order 100 (Input Hints)
    d->hintLabel = new InputHintWidget(statusBar());
    d->hintLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    d->hintLabel->setWindowTitle(tr("Input Hints"));
    addStatusBarItem(d->hintLabel, StatusBarItemSpec("InputHints", QString(), StatusBarSlot::Left, 100, true, 0));

    // sizeLabel — Right, order 1000 (Unit System)
    d->sizeLabel = new DimensionWidget(statusBar());
    d->sizeLabel->setWindowTitle(tr("Unit System"));
    addStatusBarItem(d->sizeLabel, StatusBarItemSpec("UnitSystem", QString(), StatusBarSlot::Right, 1000, true, 0));

    // rightSideLabel — Right, order 400 (Quick Measure)
    d->rightSideLabel = new StatusBarLabel(statusBar());
    d->rightSideLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    d->rightSideLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    d->rightSideLabel->setWindowTitle(tr("Quick Measure"));
    addStatusBarItem(d->rightSideLabel, StatusBarItemSpec("QuickMeasure", QString(), StatusBarSlot::Right, 400, true, 0));

    // Ported from: FreeCAD src/Gui/MainWindow.cpp:493-501 — SequencerBar (progress bar)
    auto* progressBar = new QProgressBar(statusBar());
    progressBar->setObjectName(QStringLiteral("sequencerBar"));
    progressBar->setMaximumWidth(200);
    progressBar->setVisible(false);  // hidden until backend activates it
    addStatusBarItem(progressBar, StatusBarItemSpec("progressBar", QString(), StatusBarSlot::Left, 50, true, 0));

    // Ported from: FreeCAD src/Gui/MainWindow.cpp:528-551 — toggleBottomPanelsButton
    auto* toggleBtn = new QToolButton(statusBar());
    toggleBtn->setIcon(QIcon(QStringLiteral(":/icons/Std_ToggleBottomPanels")));
    toggleBtn->setIconSize(QSize(16, 16));
    toggleBtn->setToolTip(tr("Toggles the bottom dock panels"));
    toggleBtn->setStatusTip(tr("Toggles the bottom dock panels"));
    toggleBtn->setCheckable(true);
    toggleBtn->setChecked(true);
    toggleBtn->setAutoRaise(true);
    toggleBtn->setWindowTitle(tr("Bottom Panel Toggle"));
    addStatusBarItem(toggleBtn, StatusBarItemSpec("toggleBottomPanelsButton", QString(), StatusBarSlot::Right, 700, true, 0));
    // TODO: wire to Std_ToggleBottomPanels command when bottom docks exist (region 3)

    // Ported from: FreeCAD src/Gui/MainWindow.cpp:553-571 — NotificationArea
    // Since no ParameterGrp Observer/GetGroup is available in the stub, default to enabled.
    {
        bool notifyEnabled = d->hGrp->GetBool("NotificationAreaEnabled", true);
        if (notifyEnabled) {
            auto* notifyWidget = new QWidget(statusBar());
            notifyWidget->setObjectName(QStringLiteral("notificationArea"));
            notifyWidget->setStyleSheet(QStringLiteral("text-align: center"));
            notifyWidget->setWindowTitle(tr("Notifications"));
            addStatusBarItem(notifyWidget, StatusBarItemSpec("Notifications", QString(), StatusBarSlot::Right, 800, true, 0));
        }
    }

    // Ported from: FreeCAD src/Gui/MainWindow.cpp:576-584 — context menu for status bar
    statusBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(statusBar(), &QStatusBar::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        buildStatusBarContextMenu(menu);
        menu.exec(statusBar()->mapToGlobal(pos));
    });

    d->currentStatusType = 100;

    // Initialize timers (ported from: FreeCAD MainWindow constructor)
    d->actionTimer = new QTimer(this);
    d->actionTimer->setObjectName(QStringLiteral("actionTimer"));
    connect(d->actionTimer, &QTimer::timeout, d->actionLabel, &QLabel::clear);

    d->statusTimer = new QTimer(this);
    d->statusTimer->setObjectName(QStringLiteral("statusTimer"));
    connect(d->statusTimer, &QTimer::timeout, this, &MainWindow::clearStatus);

    d->activityTimer = new QTimer(this);
    d->activityTimer->setObjectName(QStringLiteral("activityTimer"));
    connect(d->activityTimer, &QTimer::timeout, this, &MainWindow::_updateActions);
    d->activityTimer->setSingleShot(false);
    d->activityTimer->start(150);

    // Ported from: FreeCAD src/Gui/MainWindow.cpp:404-405 — saveStateTimer debounces
    // saveWindowSettings(true) calls (e.g. dock float/move) into one 100ms-deferred save.
    d->saveStateTimer.setSingleShot(true);
    connect(&d->saveStateTimer, &QTimer::timeout, this, [this] { saveWindowSettings(false); });

    // Ported from: FreeCAD src/Gui/MainWindow.cpp:441
    connect(statusBar(), &QStatusBar::messageChanged, this, &MainWindow::statusMessageChanged);

    // Command system managers — replace hand-written menus/toolbars with Command-driven system.
    // Menus/toolbars are built later by Workbench::activate() from command trees.
    // Ported from: FreeCAD MainWindow.cpp:2034-2042
    m_menuMgr = new MenuManager(m_cmdMgr);
    m_toolbarMgr = new ToolBarManager(m_cmdMgr, this);

    // ── Dock Panels (ported from: FreeCAD MainWindow::setupDockWindows) ──
    // Call setupDockWindows() to match FreeCAD's initialization sequence
    setupDockWindows();

    statusBar()->showMessage(tr("Ready"), 2001);

    // NOTE: no restoreWindowState()/show() here. FreeCAD shows the main window
    // ONLY via loadWindowSettings() after the workbench (menus/toolbars) is
    // active (StartupProcess.cpp:516-518) — resize → restoreState → the single
    // max ? showMaximized() : show(). Showing from the constructor then letting
    // main.cpp resize() the visible (possibly maximized) window desynced the
    // title-bar button state from the actual frame on Windows (button showed
    // "maximized" while the frame stayed at the normal size; minimize+restore
    // re-applied the window state and "fixed" it — 2026-09-17 启动最大化 saga).
}

MainWindow::~MainWindow()
{
    // d 先于基类析构被 delete，而 QMdiArea（central widget）在基类析构阶段销毁时
    // 会 emit subWindowActivated —— 此时 connect(this) 尚未随 ~QObject 断开，
    // lambda 访问已 delete 的 d 即 heap-use-after-free（ASan 实证，Qt 6.11 Windows
    // 上崩；mac 堆布局侥幸掩盖）。先显式断开。
    if (d && d->mdiArea)
        disconnect(d->mdiArea, nullptr, this, nullptr);
    delete d;
    if (instance == this) instance = nullptr;
}

// Ported from: FreeCAD src/Gui/MainWindow.h — mdiArea accessor
QMdiArea* MainWindow::mdiArea() const { return d->mdiArea; }

// Ported from: FreeCAD src/Gui/MainWindow.cpp activeWindow()
MDIView* MainWindow::activeWindow() const
{
    return d->activeView;
}

// Ported from: FreeCAD src/Gui/MainWindow.cpp:970-973
void MainWindow::tile()
{
    if (d->mdiArea) {
        d->mdiArea->tileSubWindows();
    }
}

// Ported from: FreeCAD src/Gui/MainWindow.cpp:975-978
void MainWindow::cascade()
{
    if (d->mdiArea) {
        d->mdiArea->cascadeSubWindows();
    }
}

// Ported from: FreeCAD src/Gui/MainWindow.cpp:1143-1150
void MainWindow::activateNextWindow()
{
    if (!d->mdiArea) {
        return;
    }
    auto* tab = d->mdiArea->findChild<QTabBar*>();
    if (tab && tab->count() > 0) {
        int index = (tab->currentIndex() + 1) % tab->count();
        tab->setCurrentIndex(index);
    }
}

// Ported from: FreeCAD src/Gui/MainWindow.cpp:1152-1159
void MainWindow::activatePreviousWindow()
{
    if (!d->mdiArea) {
        return;
    }
    auto* tab = d->mdiArea->findChild<QTabBar*>();
    if (tab && tab->count() > 0) {
        int index = (tab->currentIndex() + tab->count() - 1) % tab->count();
        tab->setCurrentIndex(index);
    }
}

// Ported from: FreeCAD src/Gui/MainWindow.cpp:1498-1502
void MainWindow::tabChanged(MDIView* view)
{
    Q_UNUSED(view)
    _updateActions();
}

// Ported from: FreeCAD src/Gui/MainWindow.cpp MainWindow::addWindow()
void MainWindow::addWindow(MDIView* view)
{
    // make workspace parent of view

    bool isempty = d->mdiArea->subWindowList().isEmpty();
    auto child = qobject_cast<QMdiSubWindow*>(view->parentWidget());
    if (!child) {
        child = new QMdiSubWindow(d->mdiArea->viewport());
        child->setAttribute(Qt::WA_DeleteOnClose);
        child->setWidget(view);
        child->setWindowIcon(view->windowIcon());
        QMenu* menu = child->systemMenu();

        // See StdCmdCloseActiveWindow (#0002631)
        QList<QAction*> acts = menu->actions();
        for (auto& act : acts) {
            if (act->shortcut() == QKeySequence(QKeySequence::Close)) {
                act->setShortcuts(QList<QKeySequence>());
                break;
            }
        }

        QAction* action = menu->addAction(tr("Close All"));
        connect(action, &QAction::triggered, d->mdiArea, &QMdiArea::closeAllSubWindows);
        d->mdiArea->addSubWindow(child);
    }

    connect(view, &MDIView::message, this, &MainWindow::showMessage);
    connect(this, &MainWindow::windowStateChanged, view, &MDIView::windowStateChanged);

    // listen to the incoming events of the view
    view->installEventFilter(this);

    // Show the new window. This will also call onWindowActivated. The very first window is shown in
    // maximized mode.

    if (isempty) {
        view->showMaximized();
    }
    else {
        view->show();
    }

    // 2026-09-20 dead-tab saga: the force-hide of every other sub-window that
    // used to live here (a deviation from FreeCAD) permanently killed the
    // hidden tabs. Qt 6.11 QMdiAreaPrivate::activateWindow (qtbase
    // qmdiarea.cpp:967, `if (child->isHidden() || child == active) return;`)
    // refuses to activate a hidden sub-window in TabbedView, and
    // _q_currentTabChanged (:743-748) *disables* the tab of a hidden window —
    // after opening a Blank Connection the Start tab was dead (click = no-op).
    // FreeCAD's addWindow (MainWindow.cpp:1397-1439) never hides sub-windows;
    // aligned 1:1. The original symptom this hide patched ("Start page floats
    // over the new 3D view") was the shown-maximized presentation bug fixed in
    // loadWindowSettings (below), not a sub-window stacking problem.

    // Track the newly added view as active. Ported from: FreeCAD Application::viewActivated.
    d->activeView = view;
}

// Ported from: FreeCAD src/Gui/MainWindow.cpp MainWindow::removeWindow()
void MainWindow::removeWindow(Gui::MDIView* view, bool close)
{
    if (view->currentViewMode() != MDIView::Child) {
        FC_WARN("tried to remove an MDIView that is not currently in child mode");
        return;
    }

    // free all connections
    disconnect(view, &MDIView::message, this, &MainWindow::showMessage);
    disconnect(this, &MainWindow::windowStateChanged, view, &MDIView::windowStateChanged);

    view->removeEventFilter(this);

    // check if the focus widget is a child of the view
    QWidget* foc = this->focusWidget();
    if (foc) {
        QWidget* par = foc->parentWidget();
        while (par) {
            if (par == view) {
                foc->clearFocus();
                break;
            }
            par = par->parentWidget();
        }
    }

    QWidget* parent = view->parentWidget();

    // The call of 'd->mdiArea->removeSubWindow(parent)' causes the QMdiSubWindow
    // to lose its parent and thus the notification in QMdiSubWindow::closeEvent
    // of other mdi windows to get maximized if this window is maximized will fail.
    // However, we must let it here otherwise deleting MDI child views directly can
    // cause other problems.
    //
    // The above mentioned problem can be fixed by setParent(0) which triggers a
    // ChildRemoved event being handled properly inside QMidArea::viewportEvent()
    //
    auto subwindow = qobject_cast<QMdiSubWindow*>(parent);
    if (subwindow && d->mdiArea->subWindowList().contains(subwindow)) {
        subwindow->setParent(nullptr);
        subwindow->deleteLater();

        assert(!d->mdiArea->subWindowList().contains(subwindow));
    }

    if (close) {
        parent->deleteLater();
    }

    _updateActions();
}

// Ported from: FreeCAD src/Gui/MainWindow.cpp MainWindow::setupDockWindows()
void MainWindow::setupDockWindows()
{
    // Use DockWindowManager to register and manage dock widgets (FreeCAD pattern)
    auto* pDockMgr = DockWindowManager::instance();

    // Model panel — 1:1 port of FreeCAD src/Gui/ComboView.cpp ComboView::ComboView()
    auto* modelWidget = new QWidget();
    modelWidget->setObjectName(QStringLiteral("Model"));
    modelWidget->setWindowTitle(tr("Model"));
    modelWidget->setMinimumWidth(150);

    auto* pLayout = new QGridLayout(modelWidget);
    pLayout->setSpacing(0);
    pLayout->setContentsMargins(0, 0, 0, 0);

    // tabs to switch between Tree/Properties and TaskPanel
    auto* splitter = new QSplitter();
    pLayout->addWidget(splitter, 0, 0);

    // splitter between tree and property view
    splitter->setOrientation(Qt::Vertical);

    // tree panel — 1:1 port of FreeCAD src/Gui/Tree.h TreePanel
    auto* treePanel = new Gui::TreePanel(modelWidget);
    splitter->addWidget(treePanel);

    // property view — 1:1 port of FreeCAD src/Gui/PropertyView.cpp lines 71-102
    auto* propertyView = new PropertyView(modelWidget);
    splitter->addWidget(propertyView);

    pDockMgr->registerDockWindow("Std_ComboView", modelWidget);
    auto* modelDock = pDockMgr->addDockWindow("Model", modelWidget, Qt::LeftDockWidgetArea);
    if (modelDock) {
        modelDock->show();
    }

    // Set tab position for dock widgets (ported from: FreeCAD src/Gui/MainWindow.cpp line 703-708)
    std::vector<QTabWidget::TabPosition> tabPos
        = {QTabWidget::North, QTabWidget::South, QTabWidget::West, QTabWidget::East};
    long value = d->hGrp->GetInt("LeftDockWidgetAreaTabPos", long(tabPos.front()));
    if (value >= 0 && value < long(tabPos.size())) {
        setTabPosition(Qt::LeftDockWidgetArea, tabPos[value]);
    }
}

MainWindow* MainWindow::getInstance() { return instance; }

// ─── Status bar item registry ──────────────────────────────────────────
// Ported from: FreeCAD src/Gui/MainWindow.cpp

namespace {

bool ownsVisibility(QWidget* widget)
{
    return widget->property("userEnabled").isValid();
}

void applyStatusBarItemEnabled(QWidget* widget, bool enabled)
{
    if (ownsVisibility(widget)) {
        widget->setProperty("userEnabled", enabled);
    }
    else {
        widget->setVisible(enabled);
    }
}

}  // namespace

void MainWindow::addStatusBarItem(QWidget* widget, const StatusBarItemSpec& spec)
{
    if (!widget) return;
    removeStatusBarItem(spec.id);
    if (!spec.id.isEmpty()) widget->setObjectName(QString::fromUtf8(spec.id));
    if (!spec.title.isEmpty()) widget->setWindowTitle(spec.title);

    StatusBarItem item;
    item.spec = spec;
    item.widget = widget;
    item.enabled = true;
    if (spec.persistentVisibility && !spec.id.isEmpty()) {
        item.enabled = d->hStatusBar->GetBool(spec.id.constData(), true);
    }
    d->statusBarItems.push_back(item);
    relayoutStatusBar();
}

void MainWindow::removeStatusBarItem(const QByteArray& id)
{
    auto& items = d->statusBarItems;
    auto it = std::find_if(items.begin(), items.end(), [&](const StatusBarItem& i) {
        return i.spec.id == id;
    });
    if (it == items.end()) return;
    if (it->widget) statusBar()->removeWidget(it->widget);
    items.erase(it);
    relayoutStatusBar();
}

void MainWindow::relayoutStatusBar()
{
    QStatusBar* sb = statusBar();
    QHash<QWidget*, bool> wasVisible;
    for (auto& item : d->statusBarItems) {
        if (item.widget) {
            wasVisible.insert(item.widget, item.widget->isVisible());
            sb->removeWidget(item.widget);
        }
    }
    std::stable_sort(d->statusBarItems.begin(), d->statusBarItems.end(),
        [](const StatusBarItem& a, const StatusBarItem& b) {
            if (a.spec.slot != b.spec.slot) return a.spec.slot == StatusBarSlot::Left;
            return a.spec.order < b.spec.order;
        });
    for (auto& item : d->statusBarItems) {
        if (!item.widget) continue;
        if (item.spec.slot == StatusBarSlot::Left) sb->addWidget(item.widget, item.spec.stretch);
        else sb->addPermanentWidget(item.widget, item.spec.stretch);
        if (ownsVisibility(item.widget)) {
            item.widget->setProperty("userEnabled", item.enabled);
            item.widget->setVisible(item.enabled && wasVisible.value(item.widget, false));
        } else {
            item.widget->setVisible(item.enabled);
        }
    }
}

void MainWindow::buildStatusBarContextMenu(QMenu& menu)
{
    for (auto& item : d->statusBarItems) {
        QWidget* widget = item.widget;
        if (!widget) continue;
        const QString title = item.spec.title.isEmpty() ? widget->windowTitle() : item.spec.title;
        if (title.isEmpty()) continue;
        QAction* action = menu.addAction(title);
        action->setCheckable(true);
        action->setChecked(item.enabled);
        const QByteArray id = item.spec.id;
        QObject::connect(action, &QAction::toggled, this, [this, id](bool on) {
            setStatusBarItemEnabled(id, on);
        });
    }
}

void MainWindow::setStatusBarItemEnabled(const QByteArray& id, bool enabled)
{
    auto it = std::find_if(d->statusBarItems.begin(), d->statusBarItems.end(),
        [&](const StatusBarItem& i) { return i.spec.id == id; });
    if (it == d->statusBarItems.end()) return;
    it->enabled = enabled;
    if (it->widget) applyStatusBarItemEnabled(it->widget, enabled);
    if (it->spec.persistentVisibility && !id.isEmpty()) d->hStatusBar->SetBool(id.constData(), enabled);
}

// ─── Hint overlay ──────────────────────────────────────────────────────

void MainWindow::showHints(const std::list<InputHint>& hints) { d->hintLabel->showHints(hints); }
void MainWindow::hideHints() { d->hintLabel->clearHints(); }

// ─── Right-side label ──────────────────────────────────────────────────

void MainWindow::setRightSideMessage(const QString& message) { d->rightSideLabel->setText(message.simplified()); }
bool MainWindow::isRightSideMessageVisible() const { return d->rightSideLabel->isVisible(); }

// ─── Thread-safe message posting ───────────────────────────────────────

void MainWindow::showMessage(const QString& message, int timeout)
{
    if (QApplication::instance()->thread() != QThread::currentThread()) {
        QApplication::postEvent(this, new CustomMessageEvent(MainWindow::Tmp, message, timeout));
        return;
    }
    d->actionLabel->setText(message.simplified());
    if (timeout) { d->actionTimer->setSingleShot(true); d->actionTimer->start(timeout); }
    else { d->actionTimer->stop(); }
}

// ─── Pane text and unit schema ─────────────────────────────────────────

void MainWindow::setPaneText(int i, QString text)
{
    if (i == 1) showStatus(MainWindow::Pane, text);
    else if (i == 2) d->sizeLabel->setText(text);
}

void MainWindow::setUserSchema(int userSchema) { d->sizeLabel->setUserSchema(userSchema); }

// ─── Status bar message type tracking ──────────────────────────────────

void MainWindow::statusMessageChanged()
{
    if (d->currentStatusType < 0) { d->currentStatusType = -d->currentStatusType; }
    else { d->statusTimer->stop(); clearStatus(); }
}

void MainWindow::showStatus(int type, const QString& message)
{
    if (QApplication::instance()->thread() != QThread::currentThread()) {
        QApplication::postEvent(this, new CustomMessageEvent(type, message));
        return;
    }
    if (d->currentStatusType < type) return;
    d->statusTimer->setSingleShot(true);
    d->statusTimer->start(5000);
    QFontMetrics fm(statusBar()->font());
    QString msg = fm.elidedText(message, Qt::ElideMiddle, d->actionLabel->width());
    switch (type) {
        case MainWindow::Err: statusBar()->setStyleSheet(QStringLiteral("#statusBar{color: #ff0000}")); break;
        case MainWindow::Wrn: statusBar()->setStyleSheet(QStringLiteral("#statusBar{color: #ffaa00}")); break;
        case MainWindow::Pane: statusBar()->setStyleSheet(QStringLiteral("#statusBar{}")); break;
        default: statusBar()->setStyleSheet(QStringLiteral("#statusBar{color: #000000}")); break;
    }
    d->currentStatusType = -type;
    statusBar()->showMessage(msg.simplified(), 5000);
}

void MainWindow::clearStatus()
{
    d->currentStatusType = 100;
    statusBar()->setStyleSheet(QStringLiteral("#statusBar{}"));
}

// ─── Qt event override stubs ───────────────────────────────────────────

// Ported from: FreeCAD src/Gui/MainWindow.cpp:1751-1807
void MainWindow::closeEvent(QCloseEvent* e)
{
    Q_UNUSED(e)
    // Ported from: FreeCAD src/Gui/MainWindow.cpp:1786-1788
    if (isVisible()) {
        saveWindowSettings();
    }
}
// Ported from: FreeCAD src/Gui/MainWindow.cpp:2646-2682
// DTA routes WindowTitleChange + ActivationChange. FreeCAD routes
// LanguageChange (retranslate commands/workbench — DTA does not have a live
// workbench/translation cycle here) + ActivationChange (toggle Coin3D SoDB
// real-time sensor — DTA has no SoDB equivalent). The WindowTitleChange arm
// is a DTA addition so the Windows menu (rebuilt on aboutToShow per
// StdCmdWindowsMenu) reflects child-title changes; emitting windowStateChanged
// is the brief's minimal "mark Windows menu dirty" signal.
void MainWindow::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::WindowTitleChange) {
        // A child window's title changed → notify subscribers so the Windows
        // menu action list (rebuilt on aboutToShow) picks up the new text.
        Q_EMIT windowStateChanged(nullptr);
    }
    else if (e->type() == QEvent::ActivationChange) {
        // FreeCAD:2666-2677 toggles SoDB::enableRealTimeSensor on activation
        // changes. DTA's dqRender has no equivalent realtime sensor — kept
        // as an explicit no-op routing marker for future dqApp realtime-hook
        // integration. Do not fall through to QMainWindow::changeEvent here
        // (matches FreeCAD's else-if structure).
    }
    else {
        QMainWindow::changeEvent(e);
    }
}
// Ported from: FreeCAD src/Gui/MainWindow.cpp:2959-3002
void MainWindow::customEvent(QEvent* e)
{
    if (e->type() == QEvent::User) {
        auto* msg = static_cast<CustomMessageEvent*>(e);
        if (msg->type() == MainWindow::Tmp)
            showMessage(msg->message(), msg->timeout());
        else
            showStatus(msg->type(), msg->message());
    }
}
bool MainWindow::eventFilter(QObject*, QEvent*) { return false; }
void MainWindow::dragEnterEvent(QDragEnterEvent*) {}
// Ported from: FreeCAD src/Gui/MainWindow.cpp — event() delegates to QWidget::event()
// so that Qt routes QEvent::User to customEvent() and other standard events propagate.
bool MainWindow::event(QEvent* e) { return QMainWindow::event(e); }
void MainWindow::dropEvent(QDropEvent*) {}
void MainWindow::hideEvent(QHideEvent*) {}
void MainWindow::showEvent(QShowEvent*) {}

// ─── Connected slot stubs ─────────────────────────────────────────────

// Ported from: FreeCAD MainWindow.cpp:2034-2042
void MainWindow::_updateActions() { m_cmdMgr.testActive(); }
bool MainWindow::isRestoringWindowState() const { return d->m_restoringWindowState; }

// ─── Window save/restore ─────────────────────────────────────────────

// Ported from: FreeCAD src/Gui/MainWindow.cpp:2247-2271
void MainWindow::saveWindowSettings(bool canDelay)
{
    if (isRestoringWindowState()) {
        return;
    }

    // Ported from: FreeCAD src/Gui/MainWindow.cpp:2253-2256 — debounce rapid save requests
    // (e.g. dock move/float fires multiple dockLocationChanged signals); the timer invokes
    // saveWindowSettings(false) after 100ms of quiescence.
    if (canDelay) {
        d->saveStateTimer.start(100);
        return;
    }

    d->hGrp->SetBool("Maximized", this->isMaximized());
    d->hGrp->SetBool("StatusBar", this->statusBar()->isVisible());
    d->hGrp->SetASCII("MainWindowState", this->saveState().toBase64().constData());

    std::ostringstream ss;
    QRect rect(this->pos(), this->size());
    ss << rect.left() << " " << rect.top() << " " << rect.width() << " " << rect.height();
    d->hGrp->SetASCII("Geometry", ss.str().c_str());

    DockWindowManager::instance()->saveState();
    OverlayManager::instance()->save();
    // Ported from: FreeCAD src/Gui/MainWindow.cpp:2270 — persist toolbar visibility.
    m_toolbarMgr->saveState();
}

// Ported from: FreeCAD src/Gui/MainWindow.cpp:2119-2222 (loadWindowSettings) —
// default size/center, stored geometry, dock state, then the ONE show call.
// Called from main.cpp after the workbench/toolbars are built (FreeCAD calls it
// from StartupProcess.cpp:516-518 for the same reason). The multi-screen
// bounds-clamping at 2150-2185 is out of scope for the DTA shell (kept from the
// previous restoreWindowState note).
void MainWindow::loadWindowSettings()
{
    // Default geometry (FreeCAD MainWindow.cpp:2130-2133): 1800×1000 bounded to
    // the primary screen's available geometry, centered.
    QSize const frameSizeDiff = frameSize() - size();
    QRect const rect = QApplication::primaryScreen()->availableGeometry();
    QSize winSize
        = (QSize(1800, 1000).boundedTo(rect.size()) - frameSizeDiff).expandedTo(minimumSize());
    QPoint winPos = rect.center() - QRect({}, (winSize + frameSizeDiff) / 2).bottomRight();

    // Stored geometry (FreeCAD MainWindow.cpp:2146-2150) — the same "Geometry"
    // key saveWindowSettings() writes ("x y w h").
    std::istringstream geometryStream(d->hGrp->GetASCII("Geometry", ""));
    if (int x, y, w, h; geometryStream >> x >> y >> w >> h) {
        winPos = QPoint(x, y);
        winSize = QSize(w, h).expandedTo(minimumSize());
    }
    bool const max = d->hGrp->GetBool("Maximized", false);

    // Geometry settles BEFORE any show (FreeCAD MainWindow.cpp:2196-2198).
    resize(winSize);
    move(winPos);

    // StatusBar + dock MainWindowState (body of the former restoreWindowState).
    restoreWindowState();

    // FreeCAD MainWindow.cpp:2200 does `max ? showMaximized() : show()` as the
    // single show call. DTA deviates for `max` on Windows: showMaximized() as
    // the window's FIRST show on Windows 11 + per-monitor 200% DPI (Qt 6.11.1)
    // presents the central-widget subtree on screen at 2x logical coordinates —
    // the tabbed QMdiArea tab strip and status bar never appear, content is
    // drawn oversized and clipped (QWidget::grab() renders correctly; only the
    // screen presentation is wrong; 30-line pure-Qt repro shows
    // maximize-as-first-show = broken, show-normal-then-maximize = correct).
    // So: show at the restored normal geometry first, then maximize on the next
    // event-loop turn — the same transition as clicking the maximize button.
    // This is NOT the 2026-09-17 desync pattern (a resize() landing on an
    // already-maximized *visible* window desynced the title-bar button state);
    // nothing resizes or re-shows after the maximize lands.
    show();
    if (max) {
        QTimer::singleShot(0, this, [this]() { showMaximized(); });
    }
}

// Restore non-geometry window state (StatusBar + dock layout). Ported from the
// FreeCAD loadWindowSettings body (MainWindow.cpp:2128-2195 minus the geometry
// and show steps, which live in loadWindowSettings).
void MainWindow::restoreWindowState()
{
    // StatusBar (FreeCAD MainWindow.cpp:2131, 2142) — default true (matches FreeCAD's
    // `bool showStatusBar = config.value("StatusBar", true).toBool();` propagation).
    // Using `sb->isVisible()` as the default would mis-read as false during restore
    // (the main window has not been shown yet → child visibility reports false).
    if (auto* sb = statusBar()) {
        sb->setVisible(d->hGrp->GetBool("StatusBar", true));
    }

    // MainWindowState (FreeCAD MainWindow.cpp:2143-2145, 2188-2191) — base64-encoded
    // QMainWindow::saveState blob; also reloads per-dock visibility via DockWindowManager.
    std::string encoded = d->hGrp->GetASCII("MainWindowState", "");
    if (!encoded.empty()) {
        QByteArray windowState = QByteArray::fromBase64(QByteArray::fromStdString(encoded));
        d->m_restoringWindowState = true;
        QMainWindow::restoreState(windowState);
        DockWindowManager::instance()->loadState();
        d->m_restoringWindowState = false;
    }
}

// Ported from: FreeCAD src/Gui/MainWindow.cpp:1708-1718
void MainWindow::populateDockWindowMenu(QMenu* menu)
{
    DockWindowManager::populateDockWindowMenu(menu);
}

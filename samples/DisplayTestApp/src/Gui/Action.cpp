// Ported from: FreeCAD src/Gui/Action.cpp
#include "Action.h"
#include "Command.h"
#include "MainWindow.h"
#include "MainWindow_p.h"

#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QRegularExpression>
#include <QToolBar>
#include <QToolButton>

namespace Gui {

// =====================================================================
// Action (base) — Ported from: FreeCAD src/Gui/Action.cpp
// =====================================================================

// Ported from: FreeCAD src/Gui/Action.cpp:71-78
Action::Action(Command* pcCmd, QObject* parent)
    : QObject(parent)
    , m_cmd(pcCmd)
    , m_action(new QAction(parent)) {
    m_action->setObjectName(QString::fromLatin1(m_cmd->getName()));
    connect(m_action, &QAction::triggered, this, &Action::onToggled);
}

// Ported from: FreeCAD src/Gui/Action.cpp:90-93
Action::~Action() {
    delete m_action;
}

// Ported from: FreeCAD src/Gui/Action.cpp:98-101
void Action::addTo(QWidget* w) {
    w->addAction(m_action);
}

// Ported from: FreeCAD src/Gui/Action.cpp:114-117
void Action::onToggled() {
    if (m_cmd) m_cmd->activated(m_action->isChecked() ? 1 : 0);
}

// =====================================================================
// ActionGroup — Ported from: FreeCAD src/Gui/Action.cpp:434-608
// =====================================================================

// Ported from: FreeCAD src/Gui/Action.cpp:434-444
ActionGroup::ActionGroup(Command* pcCmd, QObject* parent)
    : Action(pcCmd, parent)
    , m_group(nullptr)
    , m_dropDown(false)
    , m_isMode(false)
    , m_rememberLast(true)
{
    m_group = new QActionGroup(this);
    connect(m_group, &QActionGroup::triggered, this, qOverload<QAction*>(&ActionGroup::onActivated));
    connect(m_group, &QActionGroup::hovered, this, &ActionGroup::onHovered);
}

// Ported from: FreeCAD src/Gui/Action.cpp:446-449
ActionGroup::~ActionGroup()
{
    delete m_group;
}

// Ported from: FreeCAD src/Gui/Action.cpp:454-493
void ActionGroup::addTo(QWidget* widget)
{
    if (m_dropDown) {
        if (widget->inherits("QMenu")) {
            auto menu = new QMenu(widget);
            QAction* item = qobject_cast<QMenu*>(widget)->addMenu(menu);
            item->setMenuRole(action()->menuRole());
            menu->setTitle(action()->text());
            menu->addActions(groupAction()->actions());

            QObject::connect(menu, &QMenu::aboutToShow, [this, menu]() { Q_EMIT aboutToShow(menu); });
            QObject::connect(menu, &QMenu::aboutToHide, [this, menu]() { Q_EMIT aboutToHide(menu); });
        }
        else if (widget->inherits("QToolBar")) {
            widget->addAction(action());
            QToolButton* tb = widget->findChildren<QToolButton*>().constLast();
            tb->setPopupMode(QToolButton::MenuButtonPopup);
            tb->setObjectName(QStringLiteral("qt_toolbutton_menubutton"));
            QList<QAction*> acts = groupAction()->actions();
            auto menu = new QMenu(tb);
            menu->addActions(acts);
            tb->setMenu(menu);

            QObject::connect(menu, &QMenu::aboutToShow, [this, menu]() { Q_EMIT aboutToShow(menu); });
            QObject::connect(menu, &QMenu::aboutToHide, [this, menu]() { Q_EMIT aboutToHide(menu); });
        }
        else {
            widget->addActions(groupAction()->actions());
        }
    }
    else {
        widget->addActions(groupAction()->actions());
    }
}

// Ported from: FreeCAD src/Gui/Action.cpp:495-498
void ActionGroup::setEnabled(bool check)
{
    Action::setEnabled(check);
    groupAction()->setEnabled(check);
}

// Ported from: FreeCAD src/Gui/Action.cpp:501-504
void ActionGroup::setDisabled(bool check)
{
    Action::setEnabled(!check);
    groupAction()->setDisabled(check);
}

// Ported from: FreeCAD src/Gui/Action.cpp:507-509
void ActionGroup::setExclusive(bool check)
{
    groupAction()->setExclusive(check);
}

// Ported from: FreeCAD src/Gui/Action.cpp:512-514
bool ActionGroup::isExclusive() const
{
    return groupAction()->isExclusive();
}

// Ported from: FreeCAD src/Gui/Action.cpp:517-520
void ActionGroup::setVisible(bool check)
{
    Action::setVisible(check);
    groupAction()->setVisible(check);
}

// Ported from: FreeCAD src/Gui/Action.cpp:523-525
void ActionGroup::setRememberLast(bool remember)
{
    m_rememberLast = remember;
}

// Ported from: FreeCAD src/Gui/Action.cpp:528-530
bool ActionGroup::doesRememberLast() const
{
    return m_rememberLast;
}

// Ported from: FreeCAD src/Gui/Action.cpp:533-535
QAction* ActionGroup::addAction(QAction* action)
{
    return groupAction()->addAction(action);
}

// Ported from: FreeCAD src/Gui/Action.cpp:538-540
QAction* ActionGroup::addAction(const QString& text)
{
    return groupAction()->addAction(text);
}

// Ported from: FreeCAD src/Gui/Action.cpp:543-545
QList<QAction*> ActionGroup::actions() const
{
    return groupAction()->actions();
}

// Ported from: FreeCAD src/Gui/Action.cpp:548-552
int ActionGroup::checkedAction() const
{
    auto checked = groupAction()->checkedAction();
    return actions().indexOf(checked);
}

// Ported from: FreeCAD src/Gui/Action.cpp:555-566
void ActionGroup::setCheckedAction(int index)
{
    auto acts = groupAction()->actions();
    QAction* act = acts.at(index);
    act->setChecked(true);
    this->setIcon(act->icon());

    if (!m_isMode) {
        this->action()->setToolTip(act->toolTip());
    }
    this->setProperty("defaultAction", QVariant(index));
}

// Ported from: FreeCAD src/Gui/Action.cpp:571-573
void ActionGroup::onActivated()
{
    if (m_cmd) m_cmd->activated(this->property("defaultAction").toInt());
}

// Ported from: FreeCAD src/Gui/Action.cpp:576-579
void ActionGroup::onToggled(bool check)
{
    Q_UNUSED(check)
    onActivated();
}

// Ported from: FreeCAD src/Gui/Action.cpp:585-607
void ActionGroup::onActivated(QAction* act)
{
    int index = groupAction()->actions().indexOf(act);
    this->setIcon(act->icon());
    if (m_rememberLast) {
        if (!m_isMode) {
            this->action()->setToolTip(act->toolTip());
        }
        this->setProperty("defaultAction", QVariant(index));
    }
    else {
        // for Std_RecentMacros and Std_RecentFiles
        if (!m_isMode) {
            QString str = act->text();
            // remove index from toolTip text
            static const QRegularExpression regex(QString::fromUtf8("^&?[0-9]+ "));
            str = str.remove(regex);
            this->setToolTip(act->toolTip());
        }
        // recent index is always 0
        this->setProperty("defaultAction", QVariant(0));
    }
    if (m_cmd) m_cmd->activated(index);
}

// Ported from: FreeCAD src/Gui/Action.cpp:613+ (simplified)
void ActionGroup::onHovered(QAction* act)
{
    Q_UNUSED(act)
    // No-op in DTA — full tooltip logic from FreeCAD omitted
}

// M-L(2)：RecentFilesAction/RecentMacrosAction（含 numberToLabel）已删——
// RecentFiles/RecentMacros 永久空子菜单（分析报告 §3.1）。

// =====================================================================
// WindowAction — Ported from: FreeCAD src/Gui/CommandWindow.cpp:479-497
//               + src/Gui/MainWindow.cpp:1615-1671
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:479-497 (StdCmdWindowsMenu::createAction)
// DTA deviation: FreeCAD's WindowAction ctor is empty and StdCmdWindowsMenu::createAction
// populates the 10 placeholders + separator. Region 1+2 placed the loop here so a bare
// `new WindowAction(cmd)` is usable in tests without the command system. Region 4 task 1
// extends this loop to also wire each placeholder's triggered -> onWindowTriggered
// (FreeCAD MainWindow.cpp:1626-1636 connects on first menu show via QSignalMapper; DTA
// connects here directly because actions are created here — behaviourally equivalent:
// triggered -> setActiveSubWindow(target), with target set via QAction::setData on each
// menu show in onWindowsMenuAboutToShow).
WindowAction::WindowAction(Command* pcCmd, QObject* parent)
    : ActionGroup(pcCmd, parent)
    , m_menu(nullptr)
{
    // Pre-create 10 placeholder QAction items (checkable, with &1..&9 mnemonics)
    for (int i = 0; i < 10; i++) {
        QAction* window = this->addAction(QObject::tr(pcCmd->getToolTipText()));
        window->setCheckable(true);
        window->setToolTip(QCoreApplication::translate(pcCmd->className(), pcCmd->getToolTipText()));
        window->setStatusTip(QCoreApplication::translate(pcCmd->className(), pcCmd->getStatusTip()));
        window->setWhatsThis(QCoreApplication::translate(pcCmd->className(), pcCmd->getWhatsThis()));
        // Ported from: FreeCAD src/Gui/MainWindow.cpp:1634 — connect(action, triggered, mapper, map).
        // DTA collapses the QSignalMapper hop into a direct slot; the target QMdiSubWindow*
        // is stored on the action via setData in onWindowsMenuAboutToShow.
        QObject::connect(window, &QAction::triggered, this, &WindowAction::onWindowTriggered);
    }

    QAction* sep = this->addAction(QLatin1String(""));
    sep->setSeparator(true);
}

// Ported from: FreeCAD src/Gui/Action.cpp onWindowsMenuAboutToShow
void WindowAction::addTo(QWidget* widget)
{
    ActionGroup::addTo(widget);

    // Find the menu this action was added to, and connect aboutToShow
    if (widget->inherits("QMenu")) {
        m_menu = qobject_cast<QMenu*>(widget);
        connect(m_menu, &QMenu::aboutToShow, this, &WindowAction::onWindowsMenuAboutToShow);
    }
}

// Ported from: FreeCAD src/Gui/MainWindow.cpp:1615-1671 onWindowsMenuAboutToShow
void WindowAction::onWindowsMenuAboutToShow()
{
    auto* mainWindow = MainWindow::getInstance();
    if (!mainWindow) return;

    QMdiArea* mdiArea = mainWindow->findChild<QMdiArea*>();
    if (!mdiArea) return;

    QList<QMdiSubWindow*> windows = mdiArea->subWindowList(QMdiArea::CreationOrder);
    QWidget* active = mdiArea->activeSubWindow();

    QList<QAction*> actions = this->actions();
    if (actions.isEmpty()) return;

    int numWindows = std::min<int>(actions.count() - 1, windows.count());
    for (int index = 0; index < numWindows; index++) {
        QWidget* child = windows.at(index);
        QAction* action = actions.at(index);
        // Ported from: FreeCAD MainWindow.cpp:1660 — d->windowMapper->setMapping(action, child).
        // DTA stores the sub-window on the action itself; onWindowTriggered reads it back
        // when the action fires. Re-set on every menu show so add/remove/reorder is honoured.
        action->setData(QVariant::fromValue(child));
        QString title = child->windowTitle();
        int lastIndex = title.lastIndexOf(QStringLiteral("[*]"));
        if (lastIndex > 0) {
            title = title.left(lastIndex);
            if (child->isWindowModified()) {
                title = QStringLiteral("%1*").arg(title);
            }
        }
        QString text;
        if (index < 9) {
            text = QStringLiteral("&%1 %2").arg(index + 1).arg(title);
        }
        else {
            text = QStringLiteral("%1 %2").arg(index + 1).arg(title);
        }
        action->setText(text);
        action->setVisible(true);
        action->setChecked(child == active);
    }

    // if less windows than actions
    for (int index = numWindows; index < actions.count(); index++) {
        actions[index]->setVisible(false);
    }
    // show the separator
    if (numWindows > 0) {
        actions.last()->setVisible(true);
    }
}

// Ported from: FreeCAD src/Gui/MainWindow.cpp:1520-1527 (setActiveSubWindow) +
//              :1634,1660 (windowMapper wiring). DTA collapses the QSignalMapper
// hop into a direct slot: the triggered QAction carries its QMdiSubWindow* via
// Qt::UserRole data (set in onWindowsMenuAboutToShow); this slot forwards it to
// QMdiArea::setActiveSubWindow. Behaviourally equivalent to FreeCAD's mapper.
void WindowAction::onWindowTriggered()
{
    auto* action = qobject_cast<QAction*>(sender());
    if (!action) {
        return;
    }
    auto* subWindow = qobject_cast<QMdiSubWindow*>(action->data().value<QWidget*>());
    if (!subWindow) {
        return;
    }
    auto* mainWindow = MainWindow::getInstance();
    if (!mainWindow) {
        return;
    }
    QMdiArea* mdiArea = mainWindow->mdiArea();
    if (!mdiArea) {
        return;
    }
    mdiArea->setActiveSubWindow(subWindow);
}

// M-L(2)：WorkbenchGroup 已删（WorkbenchSelector 死路径——分析报告 §3.1）。

} // namespace Gui

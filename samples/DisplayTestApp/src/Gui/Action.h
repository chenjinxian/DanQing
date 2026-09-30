// Ported from: FreeCAD src/Gui/Action.h
#pragma once

#include <memory>
#include <string>
#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QObject>

class QMenu;
class QWidget;

namespace Gui { class Command; }

namespace Gui {

// Ported from: FreeCAD src/Gui/Action.h:44 (Action = link between Command and QAction)
class Action : public QObject {
    Q_OBJECT
public:
    // Ported from: FreeCAD src/Gui/Action.h:49
    explicit Action(Command* pcCmd, QObject* parent = nullptr);
    ~Action() override;

    // Ported from: FreeCAD src/Gui/Action.h:54
    virtual void addTo(QWidget* w);

    // Ported from: FreeCAD src/Gui/Action.h:77-79
    QAction* action() const { return m_action; }

    // Ported from: FreeCAD src/Gui/Action.h:106-108
    Command* command() const { return m_cmd; }

    // Ported from: FreeCAD src/Gui/Action.h:56-76 — transparent delegation
    void setIcon(const QIcon& i)       { m_action->setIcon(i); }
    void setText(const QString& s)     { m_action->setText(s); }
    void setToolTip(const QString& s)  { m_action->setToolTip(s); }
    void setStatusTip(const QString& s){ m_action->setStatusTip(s); }
    void setWhatsThis(const QString& s){ m_action->setWhatsThis(s); }
    void setEnabled(bool on)           { m_action->setEnabled(on); }
    void setVisible(bool on)           { m_action->setVisible(on); }
    void setCheckable(bool on)         { m_action->setCheckable(on); }
    void setChecked(bool on)           { m_action->setChecked(on); }
    void setShortcut(const QString& s) { m_action->setShortcut(QKeySequence(s)); }
    void setMenuRole(QAction::MenuRole r) { m_action->setMenuRole(r); }

protected Q_SLOTS:
    // Ported from: FreeCAD src/Gui/Action.h:112-113
    virtual void onToggled();

protected:
    Command* m_cmd;
    QAction* m_action;
};

// --------------------------------------------------------------------
// Ported from: FreeCAD src/Gui/Action.h:135 (ActionGroup)
// ActionGroup = Action that manages a QActionGroup of child actions.
// --------------------------------------------------------------------
class ActionGroup : public Action {
    Q_OBJECT
public:
    // Ported from: FreeCAD src/Gui/Action.h:140
    explicit ActionGroup(Command* pcCmd, QObject* parent = nullptr);
    ~ActionGroup() override;

    // Ported from: FreeCAD src/Gui/Action.h:143
    void addTo(QWidget* widget) override;

    // Ported from: FreeCAD src/Gui/Action.h:144-145
    void setEnabled(bool);
    void setDisabled(bool);
    void setExclusive(bool);
    bool isExclusive() const;

    // Ported from: FreeCAD src/Gui/Action.h:148
    void setVisible(bool);

    // Ported from: FreeCAD src/Gui/Action.h:149-152
    void setIsMode(bool check) { m_isMode = check; }
    void setRememberLast(bool);
    bool doesRememberLast() const;

    // Ported from: FreeCAD src/Gui/Action.h:157-163
    void setDropDownMenu(bool check) { m_dropDown = check; }
    QAction* addAction(QAction*);
    QAction* addAction(const QString&);
    QList<QAction*> actions() const;
    int checkedAction() const;
    void setCheckedAction(int);

    // Ported from: FreeCAD src/Gui/Action.h:167-169
    QActionGroup* groupAction() const { return m_group; }

    // Bring base class onToggled into scope to avoid -Woverloaded-virtual
    using Action::onToggled;

public Q_SLOTS:
    // Ported from: FreeCAD src/Gui/Action.h:173-176
    void onActivated();
    void onToggled(bool);
    void onActivated(QAction*);
    void onHovered(QAction*);

Q_SIGNALS:
    // Ported from: FreeCAD src/Gui/Action.h:180-182
    void aboutToHide(QMenu*);
    void aboutToShow(QMenu*);

private:
    QActionGroup* m_group;
    bool m_dropDown;
    bool m_isMode;
    bool m_rememberLast;
};

// M-L(2)：RecentFilesAction/RecentMacrosAction 已删——RecentFiles/RecentMacros
// 是永久空子菜单（Action 有完整实现但无 appendFile 调用方——无文件系统入口；
// 宏无 Python 宿主），分析报告 §3.1。

// --------------------------------------------------------------------
// Ported from: FreeCAD src/Gui/Action.h:409 (WindowAction)
// WindowAction = ActionGroup that manages the Windows menu list.
// --------------------------------------------------------------------
class WindowAction : public ActionGroup {
    Q_OBJECT
public:
    // Ported from: FreeCAD src/Gui/Action.h:414
    explicit WindowAction(Command* pcCmd, QObject* parent = nullptr);
    void addTo(QWidget* widget) override;

public Q_SLOTS:
    // Ported from: FreeCAD src/Gui/MainWindow.cpp:1634,1660 + setActiveSubWindow
    // (d->windowMapper wiring). DTA collapses the QSignalMapper layer into a
    // direct slot: each window QAction stores its QMdiSubWindow* via setData
    // in onWindowsMenuAboutToShow, and this slot reads sender()->data() and
    // calls QMdiArea::setActiveSubWindow. Public so StdCmdWindowsMenu::createAction
    // can connect triggered -> here (QSignalMapper::map is also public in FreeCAD).
    void onWindowTriggered();

private Q_SLOTS:
    // Ported from: FreeCAD MainWindow.cpp:1615 onWindowsMenuAboutToShow
    void onWindowsMenuAboutToShow();

private:
    QMenu* m_menu;
};

// M-L(2)：WorkbenchGroup 已删——WorkbenchSelector 死路径（无 workbench 注册、
// 承载它的 FreeCAD 工具栏不构建——分析报告 §3.1）。

} // namespace Gui

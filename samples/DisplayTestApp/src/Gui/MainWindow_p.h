// MainWindow_p.h — Private header for MainWindow internals
// Contains MainWindowP, CustomMessageEvent, StatusBarItem
// Only included by MainWindow*.cpp translation units
// Ported from: FreeCAD src/Gui/MainWindow.cpp

#pragma once

#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QTimer>
#include <QMdiArea>

#include <Base/Parameter.h>

#include "MainWindow.h"
#include "Window.h"
#include "StatusBarLabel.h"
#include "InputHintWidget.h"

namespace Gui {

// Ported from: FreeCAD src/Gui/MainWindow.cpp CustomMessageEvent
class CustomMessageEvent: public QEvent
{
public:
    CustomMessageEvent(int t, const QString& s)
        : QEvent(QEvent::User), _type(t), msg(s) {}
    ~CustomMessageEvent() override = default;
    int type() const { return _type; }
    const QString& message() const { return msg; }
private:
    int _type;
    QString msg;
};

// Ported from: FreeCAD src/Gui/MainWindow.cpp StatusBarItem
struct StatusBarItem
{
    StatusBarItemSpec spec;
    QPointer<QWidget> widget;
    bool enabled = true;
};

// DimensionWidget（Unit System 状态栏右件）已删（2026-10-07 删除侧）：FreeCAD
// 单位 schema 选择器，DTA 状态栏（index.html #status-bar）只有 keyin/FPS/
// tileLoad/snap/showstatus/showerror——无单位系统面。stubs/Base/UnitsApi.h
// 随之整删（本文件是其唯一 include 方）。

// Ported from: FreeCAD src/Gui/MainWindow.cpp MainWindowP
struct MainWindowP
{
    // actionLabel（Preselection 瞬态消息条）已删（2026-10-07 删除侧）：DTA 状态栏
    // 无对应面——showStatus 收敛为 Utils.ts:8-26 的 span 单写。
    InputHintWidget* hintLabel;
    std::vector<StatusBarItem> statusBarItems;
    ParameterGrp::handle hStatusBar;
    QTimer* activityTimer;
    QMdiArea* mdiArea;
    QPointer<MDIView> activeView;
    bool m_restoringWindowState = false;
    ParameterGrp::handle hGrp;
    // Ported from: FreeCAD src/Gui/MainWindow.cpp:334 — single-shot timer that debounces
    // saveWindowSettings(true) (e.g. dock move/float) so consecutive events coalesce into one save.
    QTimer saveStateTimer;
};

} // namespace Gui

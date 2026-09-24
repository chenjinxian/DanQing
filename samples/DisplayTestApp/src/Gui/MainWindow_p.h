// MainWindow_p.h — Private header for MainWindow internals
// Contains MainWindowP, DimensionWidget, CustomMessageEvent, StatusBarItem
// Only included by MainWindow*.cpp translation units
// Ported from: FreeCAD src/Gui/MainWindow.cpp

#pragma once

#include <QActionGroup>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QMdiArea>

#include <Base/Parameter.h>
#include <Base/UnitsApi.h>

#include "MainWindow.h"
#include "Window.h"
#include "StatusBarLabel.h"
#include "InputHintWidget.h"

namespace Gui {

// Ported from: FreeCAD src/Gui/MainWindow.cpp CustomMessageEvent
class CustomMessageEvent: public QEvent
{
public:
    CustomMessageEvent(int t, const QString& s, int timeout = 0)
        : QEvent(QEvent::User), _type(t), msg(s), _timeout(timeout) {}
    ~CustomMessageEvent() override = default;
    int type() const { return _type; }
    const QString& message() const { return msg; }
    int timeout() const { return _timeout; }
private:
    int _type;
    QString msg;
    int _timeout;
};

// Ported from: FreeCAD src/Gui/MainWindow.cpp StatusBarItem
struct StatusBarItem
{
    StatusBarItemSpec spec;
    QPointer<QWidget> widget;
    bool enabled = true;
};

// Ported from: FreeCAD src/Gui/MainWindow.cpp DimensionWidget
class DimensionWidget: public QPushButton, WindowParameter
{
    Q_OBJECT

public:
    // Ported from: FreeCAD src/Gui/MainWindow.cpp:195-234 (DimensionWidget ctor)
    explicit DimensionWidget(QWidget* parent)
        : QPushButton(parent)
        , WindowParameter("Units")
    {
        setFlat(true);
        setText(qApp->translate("Gui::MainWindow", "Dimension"));
        setMinimumWidth(120);
        setWindowTitle(qApp->translate("Gui::MainWindow", "Unit System"));

        auto* menu = new QMenu(this);
        auto* actionGrp = new QActionGroup(menu);

        auto setAction = [&, index{0}](const std::string&) mutable {
            QAction* action = menu->addAction(QStringLiteral("UnitSchema%1").arg(index));
            actionGrp->addAction(action);
            action->setCheckable(true);
            action->setData(index++);
        };
        auto descriptions = Base::UnitsApi::getDescriptions();
        std::for_each(descriptions.begin(), descriptions.end(), setAction);

        QObject::connect(actionGrp, &QActionGroup::triggered, this, [this](QAction* action) {
            int userSchema = action->data().toInt();
            setUserSchema(userSchema);
        });
        setMenu(menu);

        // Ported from: FreeCAD src/Gui/MainWindow.cpp:232-234
        retranslateUi();
        unitChanged();

        // TODO: wire ParameterGrp Observer when stub supports Attach/Detach/OnChange
        // FreeCAD: getWindowParameter()->Attach(this);
    }

    // Ported from: FreeCAD src/Gui/MainWindow.cpp:237-240
    ~DimensionWidget() override = default;

    // TODO: wire when ParameterGrp stub supports Attach/Detach/OnChange
    // FreeCAD: getWindowParameter()->Detach(this);

    // Ported from: FreeCAD src/Gui/MainWindow.cpp:241-247
    // void OnChange(Base::Subject<const char*>& rCaller, const char* sReason) override
    // {
    //     Q_UNUSED(rCaller)
    //     if (strcmp(sReason, "UserSchema") == 0) {
    //         unitChanged();
    //     }
    // }

    // Ported from: FreeCAD src/Gui/MainWindow.cpp:248-257
    void changeEvent(QEvent* event) override
    {
        if (event->type() == QEvent::LanguageChange) {
            retranslateUi();
        }
        else {
            QPushButton::changeEvent(event);
        }
    }

    // Ported from: FreeCAD src/Gui/MainWindow.cpp:259-275
    void setUserSchema(int userSchema)
    {
        // Ported from: FreeCAD src/Gui/MainWindow.cpp:266-270
        // FreeCAD saves to Document.UnitSystem or falls back to parameter group
        getWindowParameter()->SetInt("UserSchema", userSchema);

        // Ported from: FreeCAD src/Gui/MainWindow.cpp:272-274
        unitChanged();
        // TODO: Base::UnitsApi::setSchema(userSchema) when stub supports it
        // TODO: Gui::Application::Instance->onUpdate() when Application stub supports it
    }

private:
    // Ported from: FreeCAD src/Gui/MainWindow.cpp:278-294
    void unitChanged()
    {
        int userSchema = getWindowParameter()->GetInt("UserSchema", 0);

        auto actions = menu()->actions();
        if (userSchema < 0 || userSchema >= actions.size()) {
            userSchema = 0;
        }
        actions[userSchema]->setChecked(true);
    }

    // Ported from: FreeCAD src/Gui/MainWindow.cpp:296-305
    void retranslateUi()
    {
        auto actions = menu()->actions();
        auto addAction = [&, index{0}](const std::string& action) mutable {
            actions[index++]->setText(QString::fromStdString(action));
        };
        auto descriptions = Base::UnitsApi::getDescriptions();
        std::for_each(descriptions.begin(), descriptions.end(), addAction);
    }
};

// Ported from: FreeCAD src/Gui/MainWindow.cpp MainWindowP
struct MainWindowP
{
    DimensionWidget* sizeLabel;
    StatusBarLabel* actionLabel;
    InputHintWidget* hintLabel;
    StatusBarLabel* rightSideLabel;
    std::vector<StatusBarItem> statusBarItems;
    ParameterGrp::handle hStatusBar;
    QTimer* actionTimer;
    QTimer* statusTimer;
    QTimer* activityTimer;
    QMdiArea* mdiArea;
    QPointer<MDIView> activeView;
    int currentStatusType = 100;
    bool m_restoringWindowState = false;
    ParameterGrp::handle hGrp;
    // Ported from: FreeCAD src/Gui/MainWindow.cpp:334 — single-shot timer that debounces
    // saveWindowSettings(true) (e.g. dock move/float) so consecutive events coalesce into one save.
    QTimer saveStateTimer;
};

} // namespace Gui

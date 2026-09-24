// Ported from: FreeCAD src/Gui/WorkbenchSelector.cpp
#include "WorkbenchSelector.h"
#include "Action.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QScreen>
#include <QToolBar>

namespace Gui {

// Ported from: FreeCAD src/Gui/WorkbenchSelector.cpp:46-61
WorkbenchComboBox::WorkbenchComboBox(ActionGroup* aGroup, QWidget* parent)
    : QComboBox(parent)
{
    setIconSize(QSize(16, 16));
    setToolTip(aGroup->action()->toolTip());
    setStatusTip(aGroup->action()->statusTip());
    setWhatsThis(aGroup->action()->whatsThis());
    refreshList(aGroup->actions());
    connect(aGroup->groupAction(), &QActionGroup::triggered, this, [this, aGroup](QAction* action) {
        setCurrentIndex(aGroup->actions().indexOf(action));
    });
    connect(this, qOverload<int>(&WorkbenchComboBox::activated), aGroup, [aGroup](int index) {
        aGroup->actions()[index]->trigger();
    });
}

// Ported from: FreeCAD src/Gui/WorkbenchSelector.cpp:63-73
void WorkbenchComboBox::showPopup()
{
    int rows = count();
    if (rows > 0) {
        int height = view()->sizeHintForRow(0);
        int maxHeight = QApplication::primaryScreen()->size().height();
        view()->setMinimumHeight(qMin(height * rows, maxHeight / 2));
    }

    QComboBox::showPopup();
}

// Ported from: FreeCAD src/Gui/WorkbenchSelector.cpp:75-102
void WorkbenchComboBox::refreshList(QList<QAction*> actionList)
{
    clear();

    for (QAction* action : actionList) {
        QIcon icon = action->icon();
        if (icon.isNull()) {
            addItem(action->text());
        }
        else {
            addItem(icon, action->text());
        }

        if (action->isChecked()) {
            this->setCurrentIndex(this->count() - 1);
        }
    }
}

} // namespace Gui

// Ported from: FreeCAD src/Gui/WorkbenchSelector.h
#pragma once

#include <QComboBox>
#include <QList>
#include <QAction>

namespace Gui {

class ActionGroup;

// Ported from: FreeCAD src/Gui/WorkbenchSelector.h WorkbenchComboBox
// WorkbenchComboBox = QComboBox that lists workbenches with 16px icons.
class WorkbenchComboBox : public QComboBox {
    Q_OBJECT
public:
    // Ported from: FreeCAD src/Gui/WorkbenchSelector.cpp:46-61
    explicit WorkbenchComboBox(ActionGroup* aGroup, QWidget* parent = nullptr);

    // Ported from: FreeCAD src/Gui/WorkbenchSelector.cpp:75-102
    void refreshList(QList<QAction*> actionList);

protected:
    // Ported from: FreeCAD src/Gui/WorkbenchSelector.cpp:63-73
    void showPopup() override;
};

} // namespace Gui

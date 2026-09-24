// PropertyView.h — 1:1 port of FreeCAD src/Gui/PropertyView.h
// PropertyEditor (QTreeWidget-based) + PropertyView container with two tabs
// Ported from: FreeCAD src/Gui/PropertyView.h

#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QBrush>
#include <QColor>

class QGridLayout;

namespace Gui
{
namespace PropertyEditor
{

// Ported from: FreeCAD src/Gui/propertyeditor/PropertyEditor.h — minimal placeholder
class PropertyEditor : public QTreeWidget
{
    Q_OBJECT
    // Ported from: FreeCAD src/Gui/propertyeditor/PropertyEditor.h:67-68
    //              (declared so freecad.qss qproperty-group* rules apply without warning)
    Q_PROPERTY(QBrush groupBackground READ groupBackground WRITE setGroupBackground DESIGNABLE true SCRIPTABLE true)
    Q_PROPERTY(QColor groupTextColor READ groupTextColor WRITE setGroupTextColor DESIGNABLE true SCRIPTABLE true)
public:
    explicit PropertyEditor(QWidget* parent = nullptr);

    // Ported from: FreeCAD src/Gui/propertyeditor/PropertyEditor.h:97-99
    QBrush groupBackground() const { return m_groupBackground; }
    void setGroupBackground(const QBrush& brush) { m_groupBackground = brush; }
    QColor groupTextColor() const { return m_groupTextColor; }
    void setGroupTextColor(const QColor& color) { m_groupTextColor = color; }

private:
    QBrush m_groupBackground;
    QColor m_groupTextColor;
};

}  // namespace PropertyEditor

// Ported from: FreeCAD src/Gui/PropertyView.h:57-118
class PropertyView : public QWidget
{
    Q_OBJECT
public:
    explicit PropertyView(QWidget* parent = nullptr);

    Gui::PropertyEditor::PropertyEditor* propertyEditorView;
    Gui::PropertyEditor::PropertyEditor* propertyEditorData;
    QTabWidget* tabs() const { return m_tabs; }

public Q_SLOTS:
    void tabChanged(int index);

private:
    QTabWidget* m_tabs;
};

}  // namespace Gui

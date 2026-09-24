// PropertyView.cpp — 1:1 port of FreeCAD src/Gui/PropertyView.cpp (lines 71-102)
// PropertyEditor placeholder + PropertyView container with LastTabIndex persistence
// Ported from: FreeCAD src/Gui/PropertyView.cpp:71-102

#include <QGridLayout>

#include <App/Application.h>
#include <Base/Parameter.h>

#include "PropertyView.h"

using namespace Gui;

// Ported from: FreeCAD src/Gui/PropertyView.cpp:53-62 — _GetParam helper
static Base::ParameterGrp::handle _GetParam()
{
    static Base::ParameterGrp::handle hGrp;
    if (!hGrp.isValid()) {
        hGrp = App::GetApplication().GetParameterGroupByPath(
            "User parameter:BaseApp/Preferences/PropertyView"
        );
    }
    return hGrp;
}

// Ported from: FreeCAD src/Gui/PropertyView.cpp:71-102 — PropertyEditor constructor
Gui::PropertyEditor::PropertyEditor::PropertyEditor(QWidget* parent)
    : QTreeWidget(parent)
{
    // Ported from: FreeCAD src/Gui/propertyeditor/PropertyEditor.cpp — column setup
    setColumnCount(3);
    setHeaderLabels({tr("Property"), tr("Value"), tr("Type")});
    setRootIsDecorated(false);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
}

// Ported from: FreeCAD src/Gui/PropertyView.cpp:71-102 — PropertyView constructor
PropertyView::PropertyView(QWidget* parent)
    : QWidget(parent)
{
    auto* pLayout = new QGridLayout(this);
    pLayout->setSpacing(0);
    pLayout->setContentsMargins(0, 0, 0, 0);

    // Ported from: FreeCAD src/Gui/PropertyView.cpp:83-86 — tabs setup
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("propertyTab"));
    m_tabs->setTabPosition(QTabWidget::South);
    pLayout->addWidget(m_tabs, 0, 0);

    // Ported from: FreeCAD src/Gui/PropertyView.cpp:88-91 — View tab
    propertyEditorView = new Gui::PropertyEditor::PropertyEditor();
    propertyEditorView->setObjectName(QStringLiteral("propertyEditorView"));
    m_tabs->addTab(propertyEditorView, tr("View"));

    // Ported from: FreeCAD src/Gui/PropertyView.cpp:93-96 — Data tab
    propertyEditorData = new Gui::PropertyEditor::PropertyEditor();
    propertyEditorData->setObjectName(QStringLiteral("propertyEditorData"));
    m_tabs->addTab(propertyEditorData, tr("Data"));

    // Ported from: FreeCAD src/Gui/PropertyView.cpp:98-102 — default tab from preferences
    int preferredTab = _GetParam()->GetInt("LastTabIndex", 1);
    if (preferredTab > 0 && preferredTab < m_tabs->count()) {
        m_tabs->setCurrentIndex(preferredTab);
    }

    // connect after adding all tabs, so adding doesn't thrash the parameter
    // Ported from: FreeCAD src/Gui/PropertyView.cpp:105
    connect(m_tabs, &QTabWidget::currentChanged, this, &PropertyView::tabChanged);
}

// Ported from: FreeCAD src/Gui/PropertyView.cpp:600-603 — tabChanged persistence
void PropertyView::tabChanged(int index)
{
    _GetParam()->SetInt("LastTabIndex", index);
}

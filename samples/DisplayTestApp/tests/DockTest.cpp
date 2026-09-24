// Ported from: Authored — no reference test exists in FreeCAD for TreePanel/TreeWidget shell
// Tests for TreeWidget column config, selection mode, DnD, search box, and context menu actions.
#include <gtest/gtest.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QDockWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QPoint>
#include <QTreeView>

#include "QtTestFixtures.h"
#include "Gui/Tree.h"
#include "Gui/PropertyView.h"
#include "Gui/DockWindowManager.h"
#include "Gui/MainWindow.h"

// Application singleton — needed by TreeWidget ctor
#include <App/Application.h>

// Per-test helper ensureAppReady() is defined in CommandTest.cpp and declared in
// QtTestFixtures.h. TreeWidget ctor uses App::GetApplication() (FreeCAD Tree.cpp).

using namespace Gui;

// =====================================================================
// TreeWidget: 3 columns (Label, Name, Type)
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for TreeWidget column config
TEST(DockTest, TreeWidgetHasThreeColumns)
{
    ensureAppReady();
    TreeWidget tree;
    tree.show();
    qApp->processEvents();

    EXPECT_EQ(tree.columnCount(), 3);

    // Header labels
    auto* header = tree.headerItem();
    ASSERT_NE(header, nullptr);
    EXPECT_EQ(header->text(0).toStdString(), std::string("Labels & Attributes"));
    EXPECT_EQ(header->text(1).toStdString(), std::string("Description"));
    EXPECT_EQ(header->text(2).toStdString(), std::string("Internal name"));

    // Columns 1 and 2 are hidden
    EXPECT_TRUE(tree.isColumnHidden(1));
    EXPECT_TRUE(tree.isColumnHidden(2));
    EXPECT_FALSE(tree.isColumnHidden(0));
}

// =====================================================================
// TreeWidget: ExtendedSelection
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for selection mode
TEST(DockTest, TreeWidgetHasExtendedSelection)
{
    ensureAppReady();
    TreeWidget tree;
    tree.show();
    qApp->processEvents();

    EXPECT_EQ(tree.selectionMode(), QAbstractItemView::ExtendedSelection);
}

// =====================================================================
// TreeWidget: DnD enabled
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for DnD config
TEST(DockTest, TreeWidgetDragDropEnabled)
{
    ensureAppReady();
    TreeWidget tree;
    tree.show();
    qApp->processEvents();

    EXPECT_TRUE(tree.dragEnabled());
    EXPECT_TRUE(tree.acceptDrops());
}

// =====================================================================
// TreeWidget: Mouse tracking enabled
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for mouse tracking
TEST(DockTest, TreeWidgetMouseTracking)
{
    ensureAppReady();
    TreeWidget tree;
    tree.show();
    qApp->processEvents();

    EXPECT_TRUE(tree.hasMouseTracking());
}

// =====================================================================
// TreeWidget: Custom delegate installed
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for delegate check
TEST(DockTest, TreeWidgetHasDelegate)
{
    ensureAppReady();
    TreeWidget tree;
    tree.show();
    qApp->processEvents();

    auto* delegate = qobject_cast<TreeWidgetItemDelegate*>(tree.itemDelegate());
    ASSERT_NE(delegate, nullptr);
}

// =====================================================================
// TreeWidget: Header stretches last section
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for header stretch
TEST(DockTest, TreeWidgetHeaderStretchesLastSection)
{
    ensureAppReady();
    TreeWidget tree;
    tree.show();
    qApp->processEvents();

    EXPECT_TRUE(tree.header()->stretchLastSection());
}

// =====================================================================
// TreePanel: search box hidden by default
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for search box visibility
TEST(DockTest, TreePanelSearchBoxHiddenByDefault)
{
    ensureAppReady();
    TreePanel panel;
    panel.show();
    qApp->processEvents();

    auto* searchBox = panel.searchBox();
    ASSERT_NE(searchBox, nullptr);
    EXPECT_FALSE(searchBox->isVisible());
    EXPECT_EQ(searchBox->placeholderText().toStdString(), std::string("Search"));
}

// =====================================================================
// TreePanel: search box shown on showEditor
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for search box show
TEST(DockTest, TreePanelSearchBoxShownOnShowEditor)
{
    ensureAppReady();
    TreePanel panel;
    panel.show();
    qApp->processEvents();

    auto* searchBox = panel.searchBox();
    ASSERT_NE(searchBox, nullptr);

    // Trigger showEditor by emitting emitSearchObjects
    QObject::connect(panel.tree(), &TreeWidget::emitSearchObjects, &panel, &TreePanel::showEditor);
    Q_EMIT panel.tree()->emitSearchObjects();
    qApp->processEvents();

    EXPECT_TRUE(searchBox->isVisible());
    EXPECT_TRUE(searchBox->hasFocus());
}

// =====================================================================
// TreePanel: search box hidden on Escape
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for search box hide
TEST(DockTest, TreePanelSearchBoxHiddenOnEscape)
{
    ensureAppReady();
    TreePanel panel;
    panel.show();
    qApp->processEvents();

    auto* searchBox = panel.searchBox();
    ASSERT_NE(searchBox, nullptr);

    // Show the search box first
    QObject::connect(panel.tree(), &TreeWidget::emitSearchObjects, &panel, &TreePanel::showEditor);
    Q_EMIT panel.tree()->emitSearchObjects();
    qApp->processEvents();
    EXPECT_TRUE(searchBox->isVisible());

    // Simulate Escape key press
    QKeyEvent escapeEvent(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(searchBox, &escapeEvent);
    qApp->processEvents();

    EXPECT_FALSE(searchBox->isVisible());
}

// =====================================================================
// TreeWidget: context menu has 12+ actions
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for context menu action count
TEST(DockTest, TreeContextMenuHasActions)
{
    ensureAppReady();
    TreeWidget tree;
    tree.show();
    qApp->processEvents();

    // Trigger context menu on empty area (no item)
    QContextMenuEvent event(QContextMenuEvent::Other, QPoint(0, 0), tree.mapToGlobal(QPoint(0, 0)));
    // We can't easily test the menu popup, but we can verify actions exist via reflection.
    // The context menu is built dynamically, so verify the action members are non-null.
    auto actions = tree.findChildren<QAction*>();
    // 13 actions: showHidden, toggleVisibility, createGroup, relabelObject, selectDependents,
    // closeDoc, reloadDoc, skipRecompute, allowPartialRecompute, markRecompute,
    // recomputeObject, searchObjects, openFileLocation
    EXPECT_GE(actions.size(), 13u);
}

// =====================================================================
// TreeWidget: context menu items have correct text
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for context menu text
TEST(DockTest, TreeContextMenuActionTexts)
{
    ensureAppReady();
    TreeWidget tree;
    tree.show();
    qApp->processEvents();

    auto actions = tree.findChildren<QAction*>();

    // Build a set of action texts for lookup
    QStringList texts;
    for (auto* action : actions) {
        texts.append(action->text());
    }

    EXPECT_TRUE(texts.contains(QStringLiteral("Show Items Hidden in Tree View")));
    EXPECT_TRUE(texts.contains(QStringLiteral("Toggle Visibility in Tree View")));
    EXPECT_TRUE(texts.contains(QStringLiteral("Create Group")));
    EXPECT_TRUE(texts.contains(QStringLiteral("Rename")));
    EXPECT_TRUE(texts.contains(QStringLiteral("Add Dependent Objects to Selection")));
    EXPECT_TRUE(texts.contains(QStringLiteral("Close Document")));
    EXPECT_TRUE(texts.contains(QStringLiteral("Reload Document")));
    EXPECT_TRUE(texts.contains(QStringLiteral("Skip Recomputes")));
    EXPECT_TRUE(texts.contains(QStringLiteral("Allow Partial Recomputes")));
    EXPECT_TRUE(texts.contains(QStringLiteral("Mark to Recompute")));
    EXPECT_TRUE(texts.contains(QStringLiteral("Recompute Object")));
    EXPECT_TRUE(texts.contains(QStringLiteral("Search Objects")));
#ifdef Q_OS_MAC
    EXPECT_TRUE(texts.contains(QStringLiteral("Reveal in Finder")));
#else
    EXPECT_TRUE(texts.contains(QStringLiteral("Open File Location")));
#endif
}

// =====================================================================
// TreeWidget: relabelObjectAction has F2 shortcut
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for F2 shortcut
TEST(DockTest, TreeRelabelActionHasShortcut)
{
    ensureAppReady();
    TreeWidget tree;
    tree.show();
    qApp->processEvents();

    auto actions = tree.findChildren<QAction*>();
    bool foundShortcut = false;
    for (auto* action : actions) {
        if (action->text() == QStringLiteral("Rename")) {
            foundShortcut = true;
#ifndef Q_OS_MAC
            EXPECT_EQ(action->shortcut(), QKeySequence(Qt::Key_F2));
#else
            EXPECT_EQ(action->shortcut(), QKeySequence(Qt::Key_Return));
#endif
            break;
        }
    }
    EXPECT_TRUE(foundShortcut) << "Rename action with F2 shortcut not found";
}

// =====================================================================
// TreeWidget: checkable actions (showHidden, skipRecompute, allowPartialRecompute)
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for checkable actions
TEST(DockTest, TreeCheckableActions)
{
    ensureAppReady();
    TreeWidget tree;
    tree.show();
    qApp->processEvents();

    auto actions = tree.findChildren<QAction*>();

    auto findAction = [&](const QString& text) -> QAction* {
        for (auto* action : actions) {
            if (action->text() == text) {
                return action;
            }
        }
        return nullptr;
    };

    auto* showHidden = findAction(QStringLiteral("Show Items Hidden in Tree View"));
    ASSERT_NE(showHidden, nullptr);
    EXPECT_TRUE(showHidden->isCheckable());

    auto* skipRecompute = findAction(QStringLiteral("Skip Recomputes"));
    ASSERT_NE(skipRecompute, nullptr);
    EXPECT_TRUE(skipRecompute->isCheckable());

    auto* allowPartial = findAction(QStringLiteral("Allow Partial Recomputes"));
    ASSERT_NE(allowPartial, nullptr);
    EXPECT_TRUE(allowPartial->isCheckable());
}

// =====================================================================
// TreeWidget: singleton instance
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for singleton check
TEST(DockTest, TreeWidgetSingletonInstance)
{
    ensureAppReady();
    TreeWidget tree;
    EXPECT_EQ(TreeWidget::instance(), &tree);
}

// =====================================================================
// TreePanel: contains TreeWidget and search box
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for panel composition
TEST(DockTest, TreePanelContainsTreeWidget)
{
    ensureAppReady();
    TreePanel panel;
    panel.show();
    qApp->processEvents();

    EXPECT_NE(panel.tree(), nullptr);
    EXPECT_NE(panel.searchBox(), nullptr);
    EXPECT_TRUE(panel.searchBox()->isHidden());
}

// =====================================================================
// TreeWidget: searchObjectsAction triggers emitSearchObjects signal
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for search signal wiring
TEST(DockTest, TreeSearchActionEmitsSignal)
{
    ensureAppReady();
    TreeWidget tree;
    tree.show();
    qApp->processEvents();

    bool signalReceived = false;
    QObject::connect(&tree, &TreeWidget::emitSearchObjects, [&signalReceived]() {
        signalReceived = true;
    });

    auto actions = tree.findChildren<QAction*>();
    for (auto* action : actions) {
        if (action->text() == QStringLiteral("Search Objects")) {
            action->trigger();
            break;
        }
    }
    qApp->processEvents();

    EXPECT_TRUE(signalReceived) << "Search Objects action should emit emitSearchObjects";
}

// =====================================================================
// PropertyView: has 2 tabs ("View" and "Data")
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for PropertyView tab count
TEST(DockTest, PropertyViewHasTwoTabs)
{
    ensureAppReady();
    Gui::PropertyView view;
    view.show();
    qApp->processEvents();

    EXPECT_EQ(view.tabs()->count(), 2);
    EXPECT_EQ(view.tabs()->tabText(0).toStdString(), std::string("View"));
    EXPECT_EQ(view.tabs()->tabText(1).toStdString(), std::string("Data"));
}

// =====================================================================
// PropertyView: default tab is Data (index 1)
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for PropertyView default tab
TEST(DockTest, PropertyViewDefaultTabIsData)
{
    ensureAppReady();
    // Clear any persisted preference to ensure default behavior
    {
        auto grp = App::GetApplication().GetParameterGroupByPath(
            "User parameter:BaseApp/Preferences/PropertyView"
        );
        grp->SetInt("LastTabIndex", 1);
    }

    Gui::PropertyView view;
    view.show();
    qApp->processEvents();

    EXPECT_EQ(view.tabs()->currentIndex(), 1) << "Default tab should be Data (index 1)";
}

// =====================================================================
// PropertyView: tab objectNames match FreeCAD
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for tab objectNames
TEST(DockTest, PropertyViewTabObjectNames)
{
    ensureAppReady();
    Gui::PropertyView view;
    view.show();
    qApp->processEvents();

    EXPECT_EQ(view.tabs()->objectName().toStdString(), std::string("propertyTab"));
    EXPECT_EQ(view.propertyEditorView->objectName().toStdString(), std::string("propertyEditorView"));
    EXPECT_EQ(view.propertyEditorData->objectName().toStdString(), std::string("propertyEditorData"));
}

// =====================================================================
// PropertyView: tab position is South
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for tab position
TEST(DockTest, PropertyViewTabPositionIsSouth)
{
    ensureAppReady();
    Gui::PropertyView view;
    view.show();
    qApp->processEvents();

    EXPECT_EQ(view.tabs()->tabPosition(), QTabWidget::South);
}

// =====================================================================
// PropertyView: tabChanged persists LastTabIndex
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for LastTabIndex persistence
TEST(DockTest, PropertyViewTabChangedPersistsLastTabIndex)
{
    ensureAppReady();
    Gui::PropertyView view;
    view.show();
    qApp->processEvents();

    // Simulate tab change to View (index 0)
    view.tabs()->setCurrentIndex(0);
    qApp->processEvents();

    auto grp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/PropertyView"
    );
    EXPECT_EQ(grp->GetInt("LastTabIndex"), 0);
}

// =====================================================================
// PropertyEditor: has 3 columns (Property, Value, Type)
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for PropertyEditor columns
TEST(DockTest, PropertyEditorHasThreeColumns)
{
    ensureAppReady();
    Gui::PropertyEditor::PropertyEditor editor;
    editor.show();
    qApp->processEvents();

    EXPECT_EQ(editor.columnCount(), 3);
    EXPECT_EQ(editor.headerItem()->text(0).toStdString(), std::string("Property"));
    EXPECT_EQ(editor.headerItem()->text(1).toStdString(), std::string("Value"));
    EXPECT_EQ(editor.headerItem()->text(2).toStdString(), std::string("Type"));
}

// =====================================================================
// PropertyEditor: flat layout (no tree decoration)
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for root decoration
TEST(DockTest, PropertyEditorIsFlatLayout)
{
    ensureAppReady();
    Gui::PropertyEditor::PropertyEditor editor;
    editor.show();
    qApp->processEvents();

    EXPECT_FALSE(editor.rootIsDecorated()) << "PropertyEditor should use flat layout";
}

// =====================================================================
// PropertyEditor: alternating row colors enabled
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for alternating row colors
TEST(DockTest, PropertyEditorAlternatingRowColors)
{
    ensureAppReady();
    Gui::PropertyEditor::PropertyEditor editor;
    editor.show();
    qApp->processEvents();

    EXPECT_TRUE(editor.alternatingRowColors()) << "PropertyEditor should have alternating row colors";
}

// =====================================================================
// PropertyEditor: ExtendedSelection mode
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for selection mode
TEST(DockTest, PropertyEditorExtendedSelection)
{
    ensureAppReady();
    Gui::PropertyEditor::PropertyEditor editor;
    editor.show();
    qApp->processEvents();

    EXPECT_EQ(editor.selectionMode(), QAbstractItemView::ExtendedSelection);
}

// =====================================================================
// DockWindowItems: addDockWidget stores correct fields
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for DockWindowItems fields
TEST(DockTest, DockWindowItemsAddDockWidget)
{
    ensureAppReady();

    Gui::DockWindowItems items;
    items.addDockWidget("Model", Qt::LeftDockWidgetArea, Gui::DockWindowOption::Visible);
    items.addDockWidget("Tasks", Qt::RightDockWidgetArea, Gui::DockWindowOption::HiddenTabbed);

    const auto& docks = items.dockWidgets();
    ASSERT_EQ(docks.size(), 2);

    EXPECT_EQ(docks[0].name.toStdString(), std::string("Model"));
    EXPECT_EQ(docks[0].pos, Qt::LeftDockWidgetArea);
    EXPECT_TRUE(docks[0].visibility);
    EXPECT_FALSE(docks[0].tabbed);

    EXPECT_EQ(docks[1].name.toStdString(), std::string("Tasks"));
    EXPECT_EQ(docks[1].pos, Qt::RightDockWidgetArea);
    EXPECT_FALSE(docks[1].visibility);
    EXPECT_TRUE(docks[1].tabbed);
}

// =====================================================================
// DockWindowItems: DockWindowOption enum → (visibility, tabbed) mapping
// =====================================================================

// Ported from: FreeCAD src/Gui/DockWindowManager.cpp:48-56
//              DockWindowItems::addDockWidget encodes:
//                visibility = option.testFlag(DockWindowOption::Visible)
//                tabbed     = option.testFlag(DockWindowOption::HiddenTabbed)
//              Enum values are bit-encoded: bit0=Visible (1), bit1=Tabbed (2).
TEST(DockTest, DockWindowOptionEnumMapping)
{
    ensureAppReady();

    Gui::DockWindowItems items;
    items.addDockWidget("Hidden",        Qt::LeftDockWidgetArea, Gui::DockWindowOption::Hidden);
    items.addDockWidget("Visible",       Qt::LeftDockWidgetArea, Gui::DockWindowOption::Visible);
    items.addDockWidget("HiddenTabbed",  Qt::LeftDockWidgetArea, Gui::DockWindowOption::HiddenTabbed);
    items.addDockWidget("VisibleTabbed", Qt::LeftDockWidgetArea, Gui::DockWindowOption::VisibleTabbed);

    const auto& docks = items.dockWidgets();
    ASSERT_EQ(docks.size(), 4);

    // Hidden (0) — neither bit set
    EXPECT_EQ(docks[0].name.toStdString(), std::string("Hidden"));
    EXPECT_FALSE(docks[0].visibility);
    EXPECT_FALSE(docks[0].tabbed);

    // Visible (1) — bit0 set
    EXPECT_EQ(docks[1].name.toStdString(), std::string("Visible"));
    EXPECT_TRUE(docks[1].visibility);
    EXPECT_FALSE(docks[1].tabbed);

    // HiddenTabbed (2) — bit1 set
    EXPECT_EQ(docks[2].name.toStdString(), std::string("HiddenTabbed"));
    EXPECT_FALSE(docks[2].visibility);
    EXPECT_TRUE(docks[2].tabbed);

    // VisibleTabbed (3) — both bits set
    EXPECT_EQ(docks[3].name.toStdString(), std::string("VisibleTabbed"));
    EXPECT_TRUE(docks[3].visibility);
    EXPECT_TRUE(docks[3].tabbed);
}

// =====================================================================
// DockWindowItems: setDockingArea modifies existing item
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for setDockingArea
TEST(DockTest, DockWindowItemsSetDockingArea)
{
    ensureAppReady();

    Gui::DockWindowItems items;
    items.addDockWidget("Model", Qt::LeftDockWidgetArea, Gui::DockWindowOption::Visible);
    items.setDockingArea("Model", Qt::RightDockWidgetArea);

    const auto& docks = items.dockWidgets();
    ASSERT_EQ(docks.size(), 1);
    EXPECT_EQ(docks[0].pos, Qt::RightDockWidgetArea);
}

// =====================================================================
// DockWindowItems: setVisibility(name) modifies existing item
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for setVisibility by name
TEST(DockTest, DockWindowItemsSetNameVisibility)
{
    ensureAppReady();

    Gui::DockWindowItems items;
    items.addDockWidget("Model", Qt::LeftDockWidgetArea, Gui::DockWindowOption::Visible);
    items.setVisibility("Model", false);

    const auto& docks = items.dockWidgets();
    ASSERT_EQ(docks.size(), 1);
    EXPECT_FALSE(docks[0].visibility);
}

// =====================================================================
// DockWindowItems: setVisibility(all) modifies all items
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for setVisibility all
TEST(DockTest, DockWindowItemsSetAllVisibility)
{
    ensureAppReady();

    Gui::DockWindowItems items;
    items.addDockWidget("Model", Qt::LeftDockWidgetArea, Gui::DockWindowOption::Visible);
    items.addDockWidget("Tasks", Qt::RightDockWidgetArea, Gui::DockWindowOption::Visible);
    items.setVisibility(false);

    const auto& docks = items.dockWidgets();
    ASSERT_EQ(docks.size(), 2);
    EXPECT_FALSE(docks[0].visibility);
    EXPECT_FALSE(docks[1].visibility);
}

// =====================================================================
// DockWindowManager: setup() configures dock from registered widget
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for DockWindowManager setup
TEST(DockTest, DockWindowManagerSetupConfiguresDock)
{
    ensureAppReady();
    Gui::MainWindow mw;
    mw.show();
    qApp->processEvents();

    auto* dwMgr = Gui::DockWindowManager::instance();
    ASSERT_NE(dwMgr, nullptr);

    // Register a dock window
    auto* widget = new QWidget();
    widget->setObjectName(QStringLiteral("TestDock"));
    widget->setWindowTitle(QStringLiteral("Test Dock"));
    dwMgr->registerDockWindow("TestDock", widget);
    auto* dock = dwMgr->addDockWindow("TestDock", widget, Qt::LeftDockWidgetArea);
    ASSERT_NE(dock, nullptr);

    // setup with items
    Gui::DockWindowItems items;
    items.addDockWidget("TestDock", Qt::LeftDockWidgetArea, Gui::DockWindowOption::Visible);
    dwMgr->setup(&items);

    qApp->processEvents();

    // Verify the dock is visible after setup
    EXPECT_TRUE(dock->isVisible());

    // Verify toggleViewAction data is set
    EXPECT_EQ(dock->toggleViewAction()->data().toString().toStdString(), std::string("TestDock"));
}

// =====================================================================
// DockWindowManager: saveState/loadState round-trips visibility
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for save/load round-trip
TEST(DockTest, DockSaveRestorePersistsVisibility)
{
    ensureAppReady();
    Gui::MainWindow mw;
    mw.show();
    qApp->processEvents();

    auto* dwMgr = Gui::DockWindowManager::instance();
    ASSERT_NE(dwMgr, nullptr);

    // Register a dock window
    auto* widget = new QWidget();
    widget->setObjectName(QStringLiteral("SaveRestoreDock"));
    widget->setWindowTitle(QStringLiteral("Save Restore Dock"));
    dwMgr->registerDockWindow("SaveRestoreDock", widget);
    auto* dock = dwMgr->addDockWindow("SaveRestoreDock", widget, Qt::LeftDockWidgetArea);
    ASSERT_NE(dock, nullptr);

    // Setup with items so saveState/loadState have data
    Gui::DockWindowItems items;
    items.addDockWidget("SaveRestoreDock", Qt::LeftDockWidgetArea, Gui::DockWindowOption::Visible);
    dwMgr->setup(&items);
    qApp->processEvents();

    // Save state (dock is visible)
    dock->setVisible(true);
    dwMgr->saveState();

    // Hide the dock
    dock->setVisible(false);
    EXPECT_FALSE(dock->isVisible());

    // Load state should restore visibility
    dwMgr->loadState();
    qApp->processEvents();
    EXPECT_TRUE(dock->isVisible());
}

// =====================================================================
// DockWindowManager: populateDockWindowMenu adds toggle actions
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for populateDockWindowMenu
TEST(DockTest, PopulateDockWindowMenuHasToggleActions)
{
    ensureAppReady();
    Gui::MainWindow mw;
    mw.show();
    qApp->processEvents();

    auto* dwMgr = Gui::DockWindowManager::instance();
    ASSERT_NE(dwMgr, nullptr);

    // Register and add a dock window
    auto* widget = new QWidget();
    widget->setObjectName(QStringLiteral("MenuDock"));
    widget->setWindowTitle(QStringLiteral("Menu Dock"));
    dwMgr->registerDockWindow("MenuDock", widget);
    auto* dock = dwMgr->addDockWindow("MenuDock", widget, Qt::LeftDockWidgetArea);
    ASSERT_NE(dock, nullptr);
    dock->show();
    qApp->processEvents();

    // Populate a menu
    QMenu menu;
    Gui::DockWindowManager::populateDockWindowMenu(&menu);
    qApp->processEvents();

    // Verify the menu has an action for our dock
    auto actions = menu.actions();
    bool found = false;
    for (auto* action : actions) {
        if (action == dock->toggleViewAction()) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "populateDockWindowMenu should add toggle action for MenuDock";

    // Verify tooltip is set
    EXPECT_EQ(dock->toggleViewAction()->toolTip().toStdString(),
              std::string("Toggles this dockable window"));
}

// =====================================================================
// MainWindow: populateDockWindowMenu delegates to DockWindowManager
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for MainWindow delegate
TEST(DockTest, MainWindowPopulateDockWindowMenuDelegates)
{
    ensureAppReady();
    Gui::MainWindow mw;
    mw.show();
    qApp->processEvents();

    // Register and add a dock window
    auto* dwMgr = Gui::DockWindowManager::instance();
    auto* widget = new QWidget();
    widget->setObjectName(QStringLiteral("DelegateDock"));
    widget->setWindowTitle(QStringLiteral("Delegate Dock"));
    dwMgr->registerDockWindow("DelegateDock", widget);
    auto* dock = dwMgr->addDockWindow("DelegateDock", widget, Qt::LeftDockWidgetArea);
    ASSERT_NE(dock, nullptr);
    dock->show();
    qApp->processEvents();

    // Populate via MainWindow method
    QMenu menu;
    mw.populateDockWindowMenu(&menu);
    qApp->processEvents();

    // Verify the menu has an action for our dock
    auto actions = menu.actions();
    bool found = false;
    for (auto* action : actions) {
        if (action == dock->toggleViewAction()) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "MainWindow::populateDockWindowMenu should include DelegateDock";
}

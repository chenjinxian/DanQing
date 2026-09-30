// Ported from: Authored — no reference test exists in FreeCAD for DockWindowManager shell
// M-L(2)：TreePanel/TreeWidget/PropertyView/PropertyEditor 的 24 项测试随被测对象
// 一并移除——Model 停靠面板改造为 Gui::TileTreePanel（"Models/瓦树"陈列面板，
// 数据源 = 打开产物注册表；分析报告 §3.1 裁决档）。§5(f) Authored 测试随被测件
// 生命周期。DockWindowManager 域测试保持。
#include <gtest/gtest.h>

#include <QDockWidget>
#include <QMenu>

#include "QtTestFixtures.h"
#include "Gui/DockWindowManager.h"
#include "Gui/MainWindow.h"

// Application singleton — needed by MainWindow ctor
#include <App/Application.h>

// Per-test helper ensureAppReady() is defined in CommandTest.cpp and declared in
// QtTestFixtures.h. MainWindow ctor uses App::GetApplication() (FreeCAD MainWindow.cpp:84).

using namespace Gui;

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

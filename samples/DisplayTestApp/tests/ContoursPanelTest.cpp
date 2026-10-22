// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp tests — M-X 主会话集成 ContoursPanel（面板状态机 + wiring）
//
// Authored: no reference test exists in itwinjs-core / display-test-app for
//           ContoursSettings / ContoursPanel（无 .test.ts；行为锚 = Contours.ts：
//           display 复选 handler :79-83 / loadContourDef :253-279 /
//           getTerrainProps :229-251 / addContourDef :281-300（五组截断 :287）/
//           applyContourDef :302-317 / deleteContourDef :319-340 /
//           Viewer.ts:411-427 createDropDown 构造）。
//
// 覆盖面：
//  - display 复选 → withDisplayContours 写回 + 门面事件（:79-83）。
//  - 组切换 loadContourDef 载入回显（:253-279）+ 越界组隐藏编辑器
//    （:259-262 toggleCurrentGroup(false)）。
//  - Apply → getTerrainProps 组装对账（:229-251——toJSON/fromJSON 语义往返）。
//  - Add 五组截断门（:287-289）+ 新组选中（:295-297）。
//  - Delete 回落组 0（:329-337）。
//  - 空/垃圾 subCategories = default group（:233——compressIds 忽略无效 Id；
//    ContourDisplay.ts:298-300 isDefaultGroup）。
//  - wiring：item 24 点击开面板（openDropDownCount RED→GREEN——激活前置灰
//    QAction 无 handler，count 恒 0）。

#include <gtest/gtest.h>
// 保证 QApplication 存在（CameraPathsPanelTest 同款；进程级只建一次）
#include <QApplication>
namespace { struct QtEnvCT { QtEnvCT() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvCT s_qtCT;

#include "Gui/ContoursPanel.h"
#include "DtaToolBars.h"

#include <QCheckBox>
#include <QComboBox>
#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QToolBar>
#include <QToolButton>
#include <QAction>

#include <dqBase/CompressedId64Set.h>
#include <dqBase/DqEvent.h>
#include <dqBase/DqId.h>
#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewPicker.h>  // ViewList（wiring 夹具——DtaToolBarsTest 同源）
#include <dqApp/ViewState.h>
#include <dqApp/Viewport.h>
#include <dqCommon/ContourDisplay.h>

#include <dqGeom/Range3d.h>

namespace {

// CameraPathsPanelTest/ContoursFacade 同形夹具
struct ContoursFixture {
    dqBase::RefPtr<dqApp::BlankConnection> conn;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    dqApp::Viewport* vp = nullptr;

    ContoursFixture()
    {
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(dqGeom::Point3d::From(-100.0, -100.0, -100.0),
                                        dqGeom::Point3d::From(100.0, 100.0, 100.0));
        conn = dqApp::BlankConnection::create(props);
        view = dqApp::SpatialViewState::CreateBlank(conn.Get(), dqGeom::Point3d::FromZero(),
                                                    dqGeom::Vector3d::From(200.0, 200.0, 200.0));
        vp = dqApp::Viewport::Create(nullptr, view);
    }

    ~ContoursFixture()
    {
        delete vp;
    }
};

// 面板子件查找（参考元素 id → objectName）
QCheckBox* displayCheckbox(Gui::ContoursPanel& p)
{
    return p.findChild<QCheckBox*>(QStringLiteral("cbx_toggleDisplayContours"));
}
QComboBox* groupCombo(Gui::ContoursPanel& p)
{
    return p.findChild<QComboBox*>(QStringLiteral("viewAttr_renderingStyle"));
}
QWidget* curGroupDiv(Gui::ContoursPanel& p)
{
    return p.findChild<QWidget*>(QStringLiteral("curGroupDiv"));
}
QLineEdit* nameEdit(Gui::ContoursPanel& p)
{
    return p.findChild<QLineEdit*>(QStringLiteral("contours_name"));
}
QLineEdit* subCatEdit(Gui::ContoursPanel& p)
{
    return p.findChild<QLineEdit*>(QStringLiteral("contours_subCatIds"));
}
QCheckBox* showElement(Gui::ContoursPanel& p)
{
    return p.findChild<QCheckBox*>(QStringLiteral("cbx_toggleShowElement"));
}
QLineEdit* majorInterval(Gui::ContoursPanel& p)
{
    return p.findChild<QLineEdit*>(QStringLiteral("major_interval"));
}
QLineEdit* minorInterval(Gui::ContoursPanel& p)
{
    return p.findChild<QLineEdit*>(QStringLiteral("minor_interval"));
}
QLineEdit* majorColor(Gui::ContoursPanel& p)
{
    return p.findChild<QLineEdit*>(QStringLiteral("major_color"));
}
QLineEdit* minorColor(Gui::ContoursPanel& p)
{
    return p.findChild<QLineEdit*>(QStringLiteral("minor_color"));
}
QSlider* majorWidth(Gui::ContoursPanel& p)
{
    return p.findChild<QSlider*>(QStringLiteral("major_width"));
}
QSlider* minorWidth(Gui::ContoursPanel& p)
{
    return p.findChild<QSlider*>(QStringLiteral("minor_width"));
}
QComboBox* majorStyle(Gui::ContoursPanel& p)
{
    return p.findChild<QComboBox*>(QStringLiteral("major_style"));
}
QComboBox* minorStyle(Gui::ContoursPanel& p)
{
    return p.findChild<QComboBox*>(QStringLiteral("minor_style"));
}
QPushButton* addButton(Gui::ContoursPanel& p)
{
    return p.findChild<QPushButton*>(QStringLiteral("contours_add"));
}
QPushButton* applyButton(Gui::ContoursPanel& p)
{
    return p.findChild<QPushButton*>(QStringLiteral("contours_apply"));
}
QPushButton* deleteButton(Gui::ContoursPanel& p)
{
    return p.findChild<QPushButton*>(QStringLiteral("contours_delete"));
}

// 预置两组成员（A=定制值 / B=默认）进 displayStyle（面板 ctor 前）。
void seedTwoGroups(ContoursFixture& f)
{
    dqCommon::ContourDisplay cd;
    dqCommon::ContourGroup a;
    a.name = "A";
    a.contourDef.majorStyle.color = dqCommon::RgbColor(255, 0, 0);      // #ff0000
    a.contourDef.majorStyle.pixelWidth = 2.5;
    a.contourDef.majorStyle.pattern = dqCommon::LinePixels::Code2;
    a.contourDef.minorStyle.color = dqCommon::RgbColor(0, 0, 255);      // #0000ff
    a.contourDef.minorInterval = 0.5;
    a.contourDef.majorIntervalCount = 10;
    a.contourDef.showGeometry = false;
    dqBase::DqIdSet ids;
    ids.insert(dqBase::DqId(uint64_t{0x1a}));
    a.subCategories = dqBase::CompressedId64Set::SortAndCompress(ids);
    cd.groups.push_back(a);
    cd.groups.push_back(dqCommon::ContourGroup{});  // B=默认组
    f.view->GetDisplayStyle().setContours(cd);
}

}  // namespace

// ---------------------------------------------------------------------------
// display 复选（:79-83）——withDisplayContours 写回 + 门面事件恰一次/翻转。
// ---------------------------------------------------------------------------
TEST(ContoursPanel, DisplayCheckboxTogglesFlagAndRaisesFacadeEvent)
{
    ContoursFixture f;
    Gui::ContoursPanel panel(f.vp, nullptr);

    int events = 0;
    auto scope = f.view->GetDisplayStyle().OnContoursChanged.AddListener(
        [&events]() { ++events; });  // ContoursFacadeTest.SetterSemantics 同形

    ASSERT_NE(displayCheckbox(panel), nullptr);
    EXPECT_FALSE(f.view->GetDisplayStyle().getContours().displayContours);  // 默认 false

    displayCheckbox(panel)->setChecked(true);   // toggled → handler（:79-83）
    EXPECT_TRUE(f.view->GetDisplayStyle().getContours().displayContours);
    EXPECT_EQ(events, 1);

    displayCheckbox(panel)->setChecked(false);
    EXPECT_FALSE(f.view->GetDisplayStyle().getContours().displayContours);
    EXPECT_EQ(events, 2);
}

// ---------------------------------------------------------------------------
// 组切换 loadContourDef（:253-279）——载入回显 + 越界组隐藏编辑器
// （:259-262）+ 回落组 0 再显。
// ---------------------------------------------------------------------------
TEST(ContoursPanel, GroupComboLoadsContourDefIntoFields)
{
    ContoursFixture f;
    seedTwoGroups(f);
    Gui::ContoursPanel panel(f.vp, nullptr);

    // ctor 尾 loadContourDef(0)（:209）——A 组回显。
    ASSERT_NE(curGroupDiv(panel), nullptr);
    EXPECT_FALSE(curGroupDiv(panel)->isHidden());
    EXPECT_EQ(nameEdit(panel)->text(), QStringLiteral("A"));
    EXPECT_EQ(subCatEdit(panel)->text(), QStringLiteral("0x1a"));
    EXPECT_EQ(majorColor(panel)->text(), QStringLiteral("#ff0000"));
    EXPECT_EQ(minorColor(panel)->text(), QStringLiteral("#0000ff"));
    EXPECT_EQ(majorWidth(panel)->value(), 5);   // 2.5（×2 整数域）
    ASSERT_EQ(majorStyle(panel)->currentIndex(), 2);  // Code2（Solid=0/Code1=1/Code2=2）
    EXPECT_EQ(majorInterval(panel)->text(), QStringLiteral("10"));
    EXPECT_EQ(minorInterval(panel)->text(), QStringLiteral("0.5"));
    EXPECT_FALSE(showElement(panel)->isChecked());

    // 切组 1（:101-103 handler）——B 默认组回显（Contour 默认：major
    // pixelWidth=2/黑/Solid、minorInterval=1、majorIntervalCount=5、
    // showGeometry=true）。
    groupCombo(panel)->setCurrentIndex(1);
    EXPECT_EQ(panel.currentContourIndex(), 1);
    EXPECT_EQ(nameEdit(panel)->text(), QString());
    EXPECT_EQ(subCatEdit(panel)->text(), QString());
    EXPECT_EQ(majorColor(panel)->text(), QStringLiteral("#000000"));
    EXPECT_EQ(majorWidth(panel)->value(), 4);   // 2.0（major 默认 pixelWidth=2）
    EXPECT_EQ(majorStyle(panel)->currentIndex(), 0);  // Solid
    EXPECT_EQ(majorInterval(panel)->text(), QStringLiteral("5"));
    EXPECT_EQ(minorInterval(panel)->text(), QStringLiteral("1"));
    EXPECT_TRUE(showElement(panel)->isChecked());

    // 越界组 2（groups.size()=2 → :259-262 早退隐藏）。
    groupCombo(panel)->setCurrentIndex(2);
    EXPECT_TRUE(curGroupDiv(panel)->isHidden());
    EXPECT_EQ(panel.currentContourIndex(), 2);

    // 回落组 0 再显。
    groupCombo(panel)->setCurrentIndex(0);
    EXPECT_FALSE(curGroupDiv(panel)->isHidden());
    EXPECT_EQ(nameEdit(panel)->text(), QStringLiteral("A"));
}

// ---------------------------------------------------------------------------
// Apply（:302-317 + getTerrainProps :229-251）——UI → ContourGroupProps 组装
// 对账（经 setContours fromJSON 后的 ContourGroup 全字段）。
// ---------------------------------------------------------------------------
TEST(ContoursPanel, ApplyAssemblesGroupProps)
{
    ContoursFixture f;
    seedTwoGroups(f);
    Gui::ContoursPanel panel(f.vp, nullptr);

    nameEdit(panel)->setText(QStringLiteral("Terrain"));
    subCatEdit(panel)->setText(QStringLiteral("0x1a,0x2b"));
    majorColor(panel)->setText(QStringLiteral("#ff0000"));
    majorWidth(panel)->setValue(5);   // 2.5
    majorStyle(panel)->setCurrentIndex(2);   // Code2（Solid=0/Code1=1/Code2=2）
    minorColor(panel)->setText(QStringLiteral("#00ff00"));
    minorWidth(panel)->setValue(3);   // 1.5
    minorStyle(panel)->setCurrentIndex(8);   // Hidden Line
    majorInterval(panel)->setText(QStringLiteral("10"));
    minorInterval(panel)->setText(QStringLiteral("0.5"));
    showElement(panel)->setChecked(false);

    applyButton(panel)->click();

    auto const& groups = f.view->GetDisplayStyle().getContours().groups;
    ASSERT_EQ(groups.size(), 2u);
    dqCommon::ContourGroup const& g = groups[0];
    EXPECT_EQ(g.name, "Terrain");
    dqBase::DqIdSet expected;
    expected.insert(dqBase::DqId(uint64_t{0x1a}));
    expected.insert(dqBase::DqId(uint64_t{0x2b}));
    EXPECT_EQ(g.subCategories, dqBase::CompressedId64Set::SortAndCompress(expected));
    EXPECT_TRUE(dqCommon::RgbColor(255, 0, 0).equals(g.contourDef.majorStyle.color));
    EXPECT_DOUBLE_EQ(g.contourDef.majorStyle.pixelWidth, 2.5);
    EXPECT_EQ(g.contourDef.majorStyle.pattern, dqCommon::LinePixels::Code2);
    EXPECT_TRUE(dqCommon::RgbColor(0, 255, 0).equals(g.contourDef.minorStyle.color));
    EXPECT_DOUBLE_EQ(g.contourDef.minorStyle.pixelWidth, 1.5);
    EXPECT_EQ(g.contourDef.minorStyle.pattern, dqCommon::LinePixels::HiddenLine);
    EXPECT_EQ(g.contourDef.majorIntervalCount, 10);
    EXPECT_DOUBLE_EQ(g.contourDef.minorInterval, 0.5);
    EXPECT_FALSE(g.contourDef.showGeometry);

    // loadContourDef 回读 round-trip（subCatIds 文本 = 解压后逗号 join）。
    panel.loadContourDef(0);
    EXPECT_EQ(subCatEdit(panel)->text(), QStringLiteral("0x1a,0x2b"));
}

// ---------------------------------------------------------------------------
// Add（:281-300）——五组截断门（:287-289）+ 新组选中/编辑器显隐。
// ---------------------------------------------------------------------------
TEST(ContoursPanel, AddGrowsGroupsTruncatesAtFive)
{
    ContoursFixture f;
    Gui::ContoursPanel panel(f.vp, nullptr);

    EXPECT_TRUE(curGroupDiv(panel)->isHidden());  // ctor 尾：空组 → 隐藏（:208/:259）

    for (int i = 0; i < 6; ++i)
        addButton(panel)->click();

    auto const& groups = f.view->GetDisplayStyle().getContours().groups;
    ASSERT_EQ(groups.size(), 5u);                 // 第 6 次被 :287 截断
    EXPECT_EQ(panel.currentContourIndex(), 4);    // :295-297 新组选中
    EXPECT_EQ(groupCombo(panel)->currentIndex(), 4);
    EXPECT_FALSE(curGroupDiv(panel)->isHidden()); // 默认组编辑器可见
    EXPECT_EQ(nameEdit(panel)->text(), QString());
}

// ---------------------------------------------------------------------------
// Delete（:319-340）——删当前组 → 回落组 0（:335-337）+ 组删除对账。
// ---------------------------------------------------------------------------
TEST(ContoursPanel, DeleteRemovesGroupAndResetsToIndexZero)
{
    ContoursFixture f;
    seedTwoGroups(f);
    Gui::ContoursPanel panel(f.vp, nullptr);

    groupCombo(panel)->setCurrentIndex(1);   // 选中 B
    deleteButton(panel)->click();

    auto const& groups = f.view->GetDisplayStyle().getContours().groups;
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].name, "A");           // B 被删
    EXPECT_EQ(panel.currentContourIndex(), 0);
    EXPECT_EQ(groupCombo(panel)->currentIndex(), 0);
    EXPECT_EQ(nameEdit(panel)->text(), QStringLiteral("A"));  // :337 重载回显
}

// ---------------------------------------------------------------------------
// 空/垃圾 subCategories = default group（:233——split(",") 的空/垃圾 token
// 被 compressIds 忽略；ContourDisplay.ts:298-300 isDefaultGroup）。
// ---------------------------------------------------------------------------
TEST(ContoursPanel, EmptyOrGarbageSubCategoriesLandsDefaultGroup)
{
    ContoursFixture f;
    Gui::ContoursPanel panel(f.vp, nullptr);
    addButton(panel)->click();   // groups=[默认组]

    subCatEdit(panel)->setText(QStringLiteral("abc,,"));   // 全垃圾 token
    applyButton(panel)->click();
    auto const& groups = f.view->GetDisplayStyle().getContours().groups;
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_TRUE(groups[0].subCategories.empty());
    EXPECT_TRUE(groups[0].isDefaultGroup());

    subCatEdit(panel)->setText(QString());   // 空文本（split → [""]）
    applyButton(panel)->click();
    EXPECT_TRUE(f.view->GetDisplayStyle().getContours().groups[0].isDefaultGroup());
}

// ---------------------------------------------------------------------------
// wiring（Viewer.ts:411-427——item 24 点击即开 ContoursPanel）。
// 激活前 RED：置灰 QAction 无 handler → openDropDownCount 恒 0。
// ---------------------------------------------------------------------------
TEST(ContoursPanel, ContoursButtonOpensLivePanel)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);

    // 活动视口（CameraPathsButtonOpensLivePanel 同形）
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(-1000, -1000, -100, 1000, 1000, 100);
    auto conn = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
    auto* vp = dqApp::Viewport::Create(nullptr, view);
    dqApp::Application::Get().GetViewManager().AddViewport(vp);

    QAction* a = bars.mainToolBar()->actions()[23];  // 位 24 = Contours
    ASSERT_EQ(a->text(), QStringLiteral("Contours"));
    auto* btn = qobject_cast<QToolButton*>(bars.mainToolBar()->widgetForAction(a));
    ASSERT_NE(btn, nullptr);
    emit btn->clicked();  // InstantPopup 无菜单——直发信号驱动 handler
    EXPECT_EQ(bars.openDropDownCount(), 1);
    QWidget* panelWidget = btn->findChild<QWidget*>(QStringLiteral("contoursSettings"));
    ASSERT_NE(panelWidget, nullptr);
    EXPECT_NE(panelWidget->findChild<QCheckBox*>(QStringLiteral("cbx_toggleDisplayContours")), nullptr);
    EXPECT_NE(panelWidget->findChild<QComboBox*>(QStringLiteral("viewAttr_renderingStyle")), nullptr);
    EXPECT_NE(panelWidget->findChild<QLineEdit*>(QStringLiteral("contours_name")), nullptr);

    bars.closeOpenDropDowns();
    // WA_DeleteOnClose 的 deleteLater 落地（CameraPathsButtonOpensLivePanel 同款）
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    dqApp::Application::Get().GetViewManager().DropViewport(vp);
    delete vp;
}

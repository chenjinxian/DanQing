// ViewSettingsPanelTest — View Settings 弹出面板
// Authored: no reference test exists in display-test-app for ViewAttributes
//           (test app ships no tests); control inventory transcribes
//           ViewAttributes.ts:302-327（View Flags 12 项 + Camera + Monochrome）与
//           renderMode 下拉；行为断言对应 DTA 的 setViewFlags + synchWithView 路径。
#include <gtest/gtest.h>
#include <QApplication>
#include <QFile>
#include <QPixmap>
#include <QImage>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QWidget>
#include "ViewSettingsPanel.h"
#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/Viewport.h>
#include <dqCommon/DisplayStyleSettings.h>

namespace { struct QtEnv { QtEnv() { if (!qApp) { static int argc=1; static char n[]="t"; static char* av[]={n,nullptr}; new QApplication(argc,av);} } }; }
static QtEnv s_qt;

namespace {
struct VpGuard {
    dqBase::RefPtr<dqApp::BlankConnection> conn;
    dqApp::Viewport* vp = nullptr;
    VpGuard() {
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(-1000,-1000,-100, 1000,1000,100);
        conn = dqApp::BlankConnection::create(props);
        auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
        vp = dqApp::Viewport::Create(nullptr, view);
        dqApp::Application::Get().GetViewManager().AddViewport(vp);
    }
    ~VpGuard() {
        dqApp::Application::Get().GetViewManager().DropViewport(vp);
        delete vp;
    }
};
QCheckBox* findBox(Gui::ViewSettingsPanel& p, const QString& text) {
    return p.findChild<QCheckBox*>(text);   // objectName 设为 flag 文本
}
}  // namespace

// Authored: no reference test exists in display-test-app for the ViewAttributes
//           panel (test app ships no tests); control inventory transcribes
//           ViewAttributes.ts:302-313（12 flags）+ Camera(:367) + Monochrome +
//           renderMode 下拉。
TEST(ViewSettingsPanel, ControlInventory)
{
    Gui::ViewSettingsPanel panel;
    const char* flags[] = { "ACS Triad", "Grid", "Fill", "Materials", "Textures",
        "Constructions", "Transparency", "Line Weights", "Line Styles",
        "Clip Volume", "Force Surface Discard", "White-on-white Reversal",
        "Camera", "Monochrome" };
    for (auto* f : flags)
        EXPECT_NE(findBox(panel, f), nullptr) << f;
    auto* rm = panel.findChild<QComboBox*>(QStringLiteral("RenderMode"));
    ASSERT_NE(rm, nullptr);
    EXPECT_EQ(rm->count(), 4);  // dqCommon::RenderMode 现有 4 值
    // 置灰分区标注（Environment editor / Background Map / Edge Display / AO / Thematic）
    for (auto* l : panel.findChildren<QLabel*>())
        if (l->text().contains("not yet implemented"))
            { SUCCEED(); return; }
    FAIL() << "expected a disabled-section label";
}

// Authored: no reference test exists in display-test-app for view-flag toggling
//           (test app ships no tests); scenario transcribes the DTA flag-toggle →
//           setViewFlags + synchWithView path（勾选 ACS/Grid → 活动视口 viewFlags 同步）。
TEST(ViewSettingsPanel, FlagTogglesWriteViewFlags)
{
    VpGuard g;
    Gui::ViewSettingsPanel panel;
    panel.syncFromViewport();

    auto* acs = findBox(panel, "ACS Triad");
    auto* grid = findBox(panel, "Grid");
    ASSERT_NE(acs, nullptr); ASSERT_NE(grid, nullptr);
    EXPECT_FALSE(acs->isChecked());   // blank 默认关（manufacture 不置 acsTriad）
    EXPECT_FALSE(grid->isChecked());

    acs->setChecked(true);
    grid->setChecked(true);
    auto const& vf = g.vp->GetView()->GetDisplayStyle().getViewFlags();
    EXPECT_TRUE(vf.acsTriad());
    EXPECT_TRUE(vf.grid());
}

// Authored: no reference test exists in display-test-app for the camera toggle
//           (test app ships no tests); scenario transcribes ViewAttributes.ts:367
//           的 Camera 开关（EnableCamera/TurnCameraOff）。
TEST(ViewSettingsPanel, CameraToggle)
{
    VpGuard g;
    Gui::ViewSettingsPanel panel;
    panel.syncFromViewport();
    auto* cam = findBox(panel, "Camera");
    ASSERT_NE(cam, nullptr);
    EXPECT_FALSE(cam->isChecked());   // CreateBlank 相机默认关
    cam->setChecked(true);
    EXPECT_TRUE(g.vp->GetView()->AsViewState3d()->IsCameraOn());
    cam->setChecked(false);
    EXPECT_FALSE(g.vp->GetView()->AsViewState3d()->IsCameraOn());
}

// Authored: no reference test exists in display-test-app for render-mode selection
//           (test app ships no tests); scenario transcribes the renderMode 下拉
//           （选 Wireframe → viewFlags.renderMode 变化；默认 SmoothShade——
//           manufactureSpatialView 覆盖）。
TEST(ViewSettingsPanel, RenderModeSelection)
{
    VpGuard g;
    Gui::ViewSettingsPanel panel;
    panel.syncFromViewport();
    auto* rm = panel.findChild<QComboBox*>(QStringLiteral("RenderMode"));
    ASSERT_NE(rm, nullptr);
    EXPECT_EQ(rm->currentText(), "Smooth Shade");  // manufacture 覆盖后的值
    rm->setCurrentText("Wireframe");
    EXPECT_EQ(g.vp->GetView()->GetDisplayStyle().getViewFlags().renderMode(),
              dqCommon::RenderMode::Wireframe);
}

// Authored: no reference test exists in display-test-app for camera-toggle undo
//           wiring (test app ships no tests); scenario transcribes
//           ViewAttributes.ts:367-373（Camera 勾选 → turnCameraOn/Off →
//           sync(true) → vp.synchWithView）——切换后撤销栈应新增 1 条目
//           （cameraOn 属 ViewPose3d::equalState 比较项）。
TEST(ViewSettingsPanel, CameraToggleSavesUndoEntry)
{
    VpGuard g;
    Gui::ViewSettingsPanel panel;
    panel.syncFromViewport();
    auto* cam = findBox(panel, "Camera");
    ASSERT_NE(cam, nullptr);
    EXPECT_FALSE(g.vp->isUndoPossible());   // Create 建基线后栈为空

    cam->setChecked(true);                  // EnableCamera + synchWithView
    EXPECT_TRUE(g.vp->GetView()->AsViewState3d()->IsCameraOn());
    EXPECT_TRUE(g.vp->isUndoPossible());    // 相机分支接 synchWithView 后应有 1 条目
}



// M-L(3) 接线级 #4：Edge Display 开关（渲染侧 M-I(4) 已通，只差面板开关）。
// Authored: no reference test exists in display-test-app for the ViewAttributes
//           panel (test app ships no tests); scenario transcribes
//           ViewAttributes.addEdgeDisplay (:835-1008)——"Visible Edges" →
//           viewFlags.visibleEdges (:863-869)、"Hidden Edges" →
//           viewFlags.hiddenEdges (:878-884)；回读半边对应参考 addEdgeDisplay
//           的 _updates sync (:972-983)。
TEST(ViewSettingsPanel, EdgeDisplaySwitchesWriteViewFlags)
{
    VpGuard g;
    Gui::ViewSettingsPanel panel;
    panel.syncFromViewport();

    auto* visEdges = findBox(panel, "Visible Edges");
    auto* hidEdges = findBox(panel, "Hidden Edges");
    ASSERT_NE(visEdges, nullptr);
    ASSERT_NE(hidEdges, nullptr);

    // blank 默认双关（ViewFlagsProperties 缺省 visibleEdges=false/hiddenEdges=false）。
    EXPECT_FALSE(visEdges->isChecked());
    EXPECT_FALSE(hidEdges->isChecked());

    visEdges->setChecked(true);
    hidEdges->setChecked(true);
    auto const& vf = g.vp->GetView()->GetDisplayStyle().getViewFlags();
    EXPECT_TRUE(vf.visibleEdges());
    EXPECT_TRUE(vf.hiddenEdges());

    // 回读（syncFromViewport——弹出时回读当前视口状态，参考 _updates sync）。
    visEdges->setChecked(false);
    hidEdges->setChecked(false);
    Gui::ViewSettingsPanel panel2;
    panel2.syncFromViewport();
    EXPECT_FALSE(findBox(panel2, "Visible Edges")->isChecked());
    EXPECT_FALSE(findBox(panel2, "Hidden Edges")->isChecked());

    visEdges->setChecked(true);
    Gui::ViewSettingsPanel panel3;
    panel3.syncFromViewport();
    EXPECT_TRUE(findBox(panel3, "Visible Edges")->isChecked());
    EXPECT_FALSE(findBox(panel3, "Hidden Edges")->isChecked());
}

// M-O(1) I1：Monochrome Color/Scaled 子项（ViewAttributes.ts:386-419 addMonochrome
// ——Color 输入写 settings.monochromeColor、"Scaled" 复选写 settings.monochromeMode
// [Scaled:Flat]、子行可见性随 viewFlags.monochrome 的 _updates push :410-418）。
// Authored: no reference test exists in display-test-app for the ViewAttributes
//           panel (test app ships no tests); scenario transcribes the reference
//           handlers verbatim（色对话框本身模态，写通道经 applyMonochromeColor
//           可测槽直锁）。
TEST(ViewSettingsPanel, MonochromeColorAndScaledWriteDisplayStyle)
{
    VpGuard g;
    Gui::ViewSettingsPanel panel;
    panel.syncFromViewport();

    auto* mono = findBox(panel, "Monochrome");
    auto* row = panel.findChild<QWidget*>(QStringLiteral("MonochromeRow"));
    auto* scaled = panel.findChild<QCheckBox*>(QStringLiteral("MonochromeScaled"));
    ASSERT_NE(mono, nullptr);
    ASSERT_NE(row, nullptr);
    ASSERT_NE(scaled, nullptr);

    // blank 默认位关 → 子行隐藏（:415-416 else 分支 display:none）。
    EXPECT_FALSE(g.vp->GetView()->GetDisplayStyle().getViewFlags().monochrome());
    EXPECT_FALSE(row->isVisibleTo(&panel));

    // 位开 → flag 写 + 子行显示。
    mono->setChecked(true);
    EXPECT_TRUE(g.vp->GetView()->GetDisplayStyle().getViewFlags().monochrome());
    EXPECT_TRUE(row->isVisibleTo(&panel));

    // Scaled 复选（:400-402——monochromeMode = enabled ? Scaled : Flat）。
    EXPECT_FALSE(scaled->isChecked());   // DisplayStyleSettings 默认 Flat（:154）
    scaled->setChecked(true);
    EXPECT_EQ(g.vp->GetView()->GetDisplayStyle().getMonochromeMode(),
              dqCommon::MonochromeMode::Scaled);
    scaled->setChecked(false);
    EXPECT_EQ(g.vp->GetView()->GetDisplayStyle().getMonochromeMode(),
              dqCommon::MonochromeMode::Flat);

    // Color 写通道（:393-396——settings.monochromeColor = ColorDef.create(color)）。
    panel.applyMonochromeColor(QColor(255, 0, 0));
    EXPECT_EQ(g.vp->GetView()->GetDisplayStyle().getMonochromeColor(), 0xFF0000FFu);

    // 回读：新面板从视口恢复（swatch/位/Scaled——:410-418 updates push）。
    Gui::ViewSettingsPanel panel2;
    panel2.syncFromViewport();
    EXPECT_TRUE(findBox(panel2, "Monochrome")->isChecked());
    EXPECT_TRUE(panel2.findChild<QCheckBox*>(QStringLiteral("MonochromeScaled"))
                    ->isVisibleTo(&panel2));
}

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
#include "ViewSettingsPanel.h"
#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/Viewport.h>

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



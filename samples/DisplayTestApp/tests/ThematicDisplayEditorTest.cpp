// ThematicDisplayEditorTest — M-S S-g Thematic 编辑器锁。
//
// 锚定：display-test-app ThematicDisplay.ts:37-777（ThematicDisplayEditor 全量
// ——首启副作用 :180-208 / displayMode range 副作用 :236-243 / gradientMode 条目
// 重建 :707-712 / Custom 三伪项键值表 :276-284+:312-321 / 传感器网格 :38-65 /
// Add/Delete :619-649 / Reset :767-772 / 写回环 :758-763 + S-f 门面事件）。
//
// Authored: no reference test exists in display-test-app for the
//           ThematicDisplayEditor（DTA 交互件无测试面——行为锚 = handler 体逐条）。
#include <gtest/gtest.h>

#include <QComboBox>
#include <QDoubleSpinBox>

#include "ThematicDisplayEditor.h"

#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/DisplayStyle.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>

namespace {

struct EditorFixture {
    dqBase::RefPtr<dqApp::BlankConnection> conn;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    dqApp::Viewport* vp = nullptr;

    EditorFixture()
    {
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(
            dqGeom::Point3d::From(-100.0, -100.0, -100.0),
            dqGeom::Point3d::From(100.0, 100.0, 100.0));
        conn = dqApp::BlankConnection::create(props);
        view = dqApp::SpatialViewState::CreateBlank(
            conn.Get(), dqGeom::Point3d::From(0.0, 0.0, 0.0),
            dqGeom::Vector3d::From(200.0, 200.0, 200.0));
        vp = dqApp::Viewport::Create(nullptr, view);
        dqApp::Application::Get().GetViewManager().AddViewport(vp);
    }

    ~EditorFixture()
    {
        dqApp::Application::Get().GetViewManager().DropViewport(vp);
        delete vp;
    }
};

dqCommon::ThematicDisplay const& curThematic(EditorFixture const& f)
{
    return f.view->GetDisplayStyle().getThematic();
}

}  // namespace

// 首启副作用（:180-208——range=extents.z / cutoff=xLength/25 / 四传感器位与值 /
// vf.thematicDisplay 位 / 控件显隐）。
TEST(ThematicDisplayEditorTest, EnableFirstTimeSideEffects)
{
    EditorFixture f;
    Gui::ThematicDisplayEditor editor;

    editor.enableThematicDisplay(true);

    EXPECT_TRUE(f.view->getViewFlags().thematicDisplay());
    auto const& td = curThematic(f);
    EXPECT_EQ(td.displayMode, dqCommon::ThematicDisplayMode::Height);
    ASSERT_FALSE(td.range.isNull());
    EXPECT_DOUBLE_EQ(td.range.low, -100.0);
    EXPECT_DOUBLE_EQ(td.range.high, 100.0);
    // cutoff = xLength()/25 = 200/25 = 8。
    EXPECT_DOUBLE_EQ(td.sensorSettings.distanceCutoff, 8.0);

    // 四传感器：XY 对角线 25/50/65/75% @ z=0（extents z 中点），值定序。
    ASSERT_EQ(td.sensorSettings.sensors.size(), 4u);
    double const expectXy[] = { -50.0, 0.0, 30.0, 50.0 };
    double const expectVal[] = { 0.025, 0.5, 0.025, 0.75 };
    for (int i = 0; i < 4; ++i) {
        EXPECT_DOUBLE_EQ(td.sensorSettings.sensors[i].position.x, expectXy[i]) << i;
        EXPECT_DOUBLE_EQ(td.sensorSettings.sensors[i].position.y, expectXy[i]) << i;
        EXPECT_DOUBLE_EQ(td.sensorSettings.sensors[i].position.z, 0.0) << i;
        EXPECT_DOUBLE_EQ(td.sensorSettings.sensors[i].value, expectVal[i]) << i;
    }

    // 关 → vf 位落。
    editor.enableThematicDisplay(false);
    EXPECT_FALSE(f.view->getViewFlags().thematicDisplay());
}

// displayMode 副作用（:236-243——Slope→[0,90]；离 Slope→默认域回填）+
// 渐变条目随模式重建（:707-712——Height 四项/其余两项）。
TEST(ThematicDisplayEditorTest, DisplayModeSlopeRangeAndGradientEntries)
{
    EditorFixture f;
    Gui::ThematicDisplayEditor editor;
    editor.enableThematicDisplay(true);

    auto* gm = editor.findChild<QComboBox*>(QStringLiteral("thematic_gradientMode"));
    ASSERT_NE(gm, nullptr);
    ASSERT_EQ(gm->count(), 4);  // Height → Smooth/Stepped/SteppedWithDelimiter/IsoLines

    editor.setDisplayMode(static_cast<int>(dqCommon::ThematicDisplayMode::Slope));
    {
        auto const& td = curThematic(f);
        EXPECT_EQ(td.displayMode, dqCommon::ThematicDisplayMode::Slope);
        ASSERT_FALSE(td.range.isNull());
        EXPECT_DOUBLE_EQ(td.range.low, 0.0);
        EXPECT_DOUBLE_EQ(td.range.high, 90.0);
    }
    EXPECT_EQ(gm->count(), 2);  // 非 Height → Smooth/Stepped

    // 离 Slope → 默认域（extents.z）回填 + 条目回四项。
    editor.setDisplayMode(static_cast<int>(dqCommon::ThematicDisplayMode::Height));
    {
        auto const& td = curThematic(f);
        EXPECT_EQ(td.displayMode, dqCommon::ThematicDisplayMode::Height);
        ASSERT_FALSE(td.range.isNull());
        EXPECT_DOUBLE_EQ(td.range.low, -100.0);
        EXPECT_DOUBLE_EQ(td.range.high, 100.0);
    }
    EXPECT_EQ(gm->count(), 4);
}

// Color Scheme Custom 三伪项（:312-321——scheme=Custom + 键值表 :276-284）。
TEST(ThematicDisplayEditorTest, ColorSchemeCustomPseudoEntries)
{
    EditorFixture f;
    Gui::ThematicDisplayEditor editor;
    editor.enableThematicDisplay(true);

    int constexpr custom = static_cast<int>(dqCommon::ThematicGradientColorScheme::Custom);

    // Custom (opaque)——3 键，键 0 = value 0.0 / (r255,g255,b0,t0)。
    editor.setColorScheme(custom + 0);
    {
        auto const& gs = curThematic(f).gradientSettings;
        EXPECT_EQ(gs.colorScheme, dqCommon::ThematicGradientColorScheme::Custom);
        ASSERT_EQ(gs.customKeys.size(), 3u);
        EXPECT_DOUBLE_EQ(gs.customKeys[0].value, 0.0);
        EXPECT_EQ(gs.customKeys[0].color.getTbgr(),
                  dqCommon::ColorDef::computeTbgrFromComponents(255, 255, 0, 0));
        EXPECT_EQ(gs.customKeys[2].color.getTbgr(),
                  dqCommon::ColorDef::computeTbgrFromComponents(0, 255, 255, 0));
    }
    // Custom (transparent)——4 键，键 1 = value 0.25。
    editor.setColorScheme(custom + 1);
    {
        auto const& gs = curThematic(f).gradientSettings;
        ASSERT_EQ(gs.customKeys.size(), 4u);
        EXPECT_DOUBLE_EQ(gs.customKeys[1].value, 0.25);
        EXPECT_EQ(gs.customKeys[1].color.getTbgr(),
                  dqCommon::ColorDef::computeTbgrFromComponents(0, 255, 0, 0xbf));
    }
    // Custom (mixed)——5 键，键 4 = value 0.8。
    editor.setColorScheme(custom + 2);
    {
        auto const& gs = curThematic(f).gradientSettings;
        ASSERT_EQ(gs.customKeys.size(), 5u);
        EXPECT_DOUBLE_EQ(gs.customKeys[4].value, 0.8);
        EXPECT_EQ(gs.customKeys[4].color.getTbgr(),
                  dqCommon::ColorDef::computeTbgrFromComponents(0, 255, 255, 0));
    }
    // 内置项 → 仅 scheme 位（参考不动 customKeys）。
    editor.setColorScheme(static_cast<int>(dqCommon::ThematicGradientColorScheme::BlueRed));
    EXPECT_EQ(curThematic(f).gradientSettings.colorScheme,
              dqCommon::ThematicGradientColorScheme::BlueRed);
}

// 传感器网格/增删/编辑（:38-65 + :619-649 + :528-614）。
TEST(ThematicDisplayEditorTest, SensorGridAddDeleteAndEdit)
{
    EditorFixture f;
    Gui::ThematicDisplayEditor editor;
    editor.enableThematicDisplay(true);

    // 32×32 网格（z=中点 0；17 值循环——[0]=0.1、[17] 回绕=0.1）。
    editor.createSensorGrid();
    ASSERT_EQ(editor.sensorCount(), 1024);
    EXPECT_DOUBLE_EQ(editor.sensorAt(0).value, 0.1);
    EXPECT_DOUBLE_EQ(editor.sensorAt(17).value, 0.1);   // 循环回绕
    EXPECT_DOUBLE_EQ(editor.sensorAt(0).position.x, -100.0);
    EXPECT_DOUBLE_EQ(editor.sensorAt(0).position.y, -100.0);
    EXPECT_DOUBLE_EQ(editor.sensorAt(0).position.z, 0.0);
    EXPECT_DOUBLE_EQ(editor.sensorAt(1023).position.x, 100.0);
    EXPECT_DOUBLE_EQ(editor.sensorAt(1023).position.y, 100.0);

    // Delete（选中=末项[createSensorGrid 后置末]）→ 1023。
    editor.deleteSensor();
    ASSERT_EQ(editor.sensorCount(), 1023);

    // Add（中点 0.5——:67-83 无参臂）→ 1024，末项被选中。
    editor.addSensor();
    ASSERT_EQ(editor.sensorCount(), 1024);
    EXPECT_EQ(editor.selectedSensor(), 1023);
    EXPECT_DOUBLE_EQ(editor.sensorAt(1023).value, 0.5);
    EXPECT_DOUBLE_EQ(editor.sensorAt(1023).position.x, 0.0);
    EXPECT_DOUBLE_EQ(editor.sensorAt(1023).position.z, 0.0);

    // 编辑选中项：值 + X 分量。（参考的选择态属主=combo 控件本身——
    // sensorSettings 处理器读 select.selectedIndex；测试直驱控件选择。）
    auto* sensorCombo = editor.findChild<QComboBox*>(QStringLiteral("thematic_sensor"));
    ASSERT_NE(sensorCombo, nullptr);
    sensorCombo->setCurrentIndex(0);
    editor.setSensorValue(0.9);
    editor.setSensorComponent(0, -42.5);
    EXPECT_DOUBLE_EQ(editor.sensorAt(0).value, 0.9);
    EXPECT_DOUBLE_EQ(editor.sensorAt(0).position.x, -42.5);
    EXPECT_DOUBLE_EQ(editor.sensorAt(0).position.y, -100.0);  // 其余分量不动
}

// Reset（:767-772——fromJSON(defaultSettings) + 条目重建 + sync + UI 刷新）+
// 写回环经 S-f 门面（OnThematicChanged 事件）+ UI 回读默认域显示（:695-698）。
TEST(ThematicDisplayEditorTest, ResetAndFacadeEventAndUiReadback)
{
    EditorFixture f;
    Gui::ThematicDisplayEditor editor;

    int events = 0;
    auto scope = f.view->GetDisplayStyle().OnThematicChanged.AddListener(
        [&events]() { ++events; });

    editor.enableThematicDisplay(true);  // 事件 1（门面——defaultSettings 与默认
                                         // thematic 不等：range/4 传感器在场）
    int const afterEnable = events;
    EXPECT_GE(afterEnable, 1);

    // 写回环事件面：range 改 → 门面事件；同值再写 → equals 短路无新事件。
    editor.setRangeLow(-42.0);
    EXPECT_EQ(events, afterEnable + 1);
    editor.setRangeLow(-42.0);
    EXPECT_EQ(events, afterEnable + 1);
    EXPECT_DOUBLE_EQ(curThematic(f).range.low, -42.0);

    // UI 回读：rangeLow 控件显示 -42。
    auto* lowSpin = editor.findChild<QDoubleSpinBox*>(QStringLiteral("thematic_rangeLow"));
    ASSERT_NE(lowSpin, nullptr);
    editor.updateThematicDisplayUI();
    EXPECT_DOUBLE_EQ(lowSpin->value(), -42.0);

    // Reset → 默认态（首启已把 defaultSettings 的 range/cutoff/传感器就位——
    // 参考的 defaultSettings 为可变共享态，Reset 读其现值）。
    editor.setDisplayMode(static_cast<int>(dqCommon::ThematicDisplayMode::Slope));
    editor.resetThematicDisplay();
    auto const& td = curThematic(f);
    EXPECT_EQ(td.displayMode, dqCommon::ThematicDisplayMode::Height);
    ASSERT_FALSE(td.range.isNull());
    EXPECT_DOUBLE_EQ(td.range.low, -100.0);
    EXPECT_DOUBLE_EQ(td.range.high, 100.0);
    EXPECT_DOUBLE_EQ(td.sensorSettings.distanceCutoff, 8.0);
    EXPECT_EQ(td.sensorSettings.sensors.size(), 4u);
    // 回读显示默认域。
    editor.updateThematicDisplayUI();
    EXPECT_DOUBLE_EQ(lowSpin->value(), -100.0);
}

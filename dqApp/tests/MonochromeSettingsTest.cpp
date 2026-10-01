// MonochromeSettingsTest — DisplayStyle monochrome color/mode 通道（M-O(1) I1）
// Ported from: itwinjs-core core/common/src/DisplayStyleSettings.ts
//              TEST(DisplayStyleTest, ...) 对应面——参考 monochrome 通道测试在
//              DisplayStyle.test.ts 的 settings 序列化/默认值面；本文件锁
//              DanQing DisplayStyle 适配器的读写/事件/默认值语义
//              （monochromeColor setter :632-636 raise onMonochromeColorChanged；
//               monochromeMode setter :647 raise onMonochromeModeChanged；
//               默认 :550 monochromeColor=white / Flat=0）。
#include <gtest/gtest.h>

#include <dqApp/ViewState.h>
#include <dqApp/DisplayStyle.h>

#include <dqCommon/DisplayStyleSettings.h>

// Ported from: DisplayStyleSettings.ts:550/:83（monochromeColor 默认 white；
// MonochromeMode 默认 Flat——"The color of the geometry is replaced..."=0）。
TEST(MonochromeSettings, DefaultsMatchReference)
{
    auto view = dqApp::SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(1000, 1000, 1000));
    ASSERT_NE(view.Get(), nullptr);
    auto& style = view->GetDisplayStyle();
    // ColorDef.white 的 tbgr = 0x00FFFFFF（参考 ColorDef ctor alpha 默认 0——
    // from(255,255,255) 不带 alpha；DisplayStyleSettings.ts:550 monochromeColor
    // 默认 white）。
    EXPECT_EQ(style.getMonochromeColor(), 0x00FFFFFFu);
    EXPECT_EQ(style.getMonochromeMode(), dqCommon::MonochromeMode::Flat);
}

// Ported from: DisplayStyleSettings.ts:632-647（双 setter + 双事件——
// onMonochromeColorChanged/onMonochromeModeChanged 分别在各自赋值后 raise）。
TEST(MonochromeSettings, SettersRoundTripAndRaiseEvents)
{
    auto view = dqApp::SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(1000, 1000, 1000));
    auto& style = view->GetDisplayStyle();

    int colorEvents = 0, modeEvents = 0;
    dqBase::DqEventScope scope;
    scope.add(style.OnMonochromeColorChanged.AddListener([&colorEvents]() { ++colorEvents; }));
    scope.add(style.OnMonochromeModeChanged.AddListener([&modeEvents]() { ++modeEvents; }));

    style.setMonochromeColor(0xFF0000FFu);   // 红 tbgr
    EXPECT_EQ(style.getMonochromeColor(), 0xFF0000FFu);
    EXPECT_EQ(colorEvents, 1);
    EXPECT_EQ(modeEvents, 0);

    style.setMonochromeMode(dqCommon::MonochromeMode::Scaled);
    EXPECT_EQ(style.getMonochromeMode(), dqCommon::MonochromeMode::Scaled);
    EXPECT_EQ(colorEvents, 1);
    EXPECT_EQ(modeEvents, 1);
}

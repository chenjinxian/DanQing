// DisplayStyleSwitchTest — M-O(4) P5 Display Style 运行时切换引擎锁。
//
// 锚定：ViewAttributes.ts:1026-1066 populate（handler :1054-1059 =
// style.load + `vp.displayStyle = style` + invalidateScene）+ ViewState.ts:644
// displayStyle setter（整对象赋值 + onDisplayStyleChanged）。
// EQUIVALENCE（ViewState.h SetDisplayStyle 声明注）：DanQing 以
// DisplayStyle3dSettings 值拷贝承载整对象替换语义。
//
// RED（M-O(4) P5 落地前）：ViewState::SetDisplayStyle 不存在——编译期缺 API 红。
//
// Authored: no reference test exists in itwinjs-core for display-style runtime
//           switching（DisplayStyleState 无单测；行为锚定如上）。
#include <gtest/gtest.h>

#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>

#include <dqCommon/ColorDef.h>
#include <dqCommon/DisplayStyleSettings.h>
#include <dqCommon/LightSettings.h>
#include <dqCommon/ThematicDisplay.h>  // M-S S-f：ThematicDisplay/Props + Range1d
#include <dqCommon/ViewFlags.h>

namespace {

struct SwitchFixture {
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    dqApp::Viewport* vp = nullptr;

    SwitchFixture()
    {
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(
            dqGeom::Point3d::From(-100.0, -100.0, -100.0),
            dqGeom::Point3d::From(100.0, 100.0, 100.0));
        imodel = dqApp::BlankConnection::create(props);
        view = dqApp::SpatialViewState::CreateBlank(
            imodel.Get(), dqGeom::Point3d::From(0.0, 0.0, 0.0),
            dqGeom::Vector3d::From(200.0, 200.0, 200.0));
        vp = dqApp::Viewport::Create(nullptr, view);
        view->SetExtents(dqGeom::Vector3d::From(
            200.0, 200.0 * vp->height() / vp->width(), 200.0));
        vp->setupViewFromFrustum(vp->getFrustum(true));
    }

    ~SwitchFixture()
    {
        delete vp;
    }
};

}  // namespace

// 全字段面：viewFlags/背景色/隐藏线/灯光——切换后视图读取面逐项来自新样式
// + OnDisplayStyleChanged Raise 一次。
TEST(DisplayStyleSwitch, ReplacesAllSettingsAndRaisesEvent)
{
    SwitchFixture f;
    int events = 0;
    auto scope = f.view->OnDisplayStyleChanged.AddListener([&events]() { ++events; });

    // 构造第二样式：viewFlags 翻面 + 背景红 + hline 宽 7 + 灯光 solar 0.5。
    dqApp::DisplayStyle newStyle;
    {
        dqCommon::ViewFlagsProperties p = newStyle.getViewFlags().Properties();
        p.grid = true;
        p.acsTriad = true;
        p.visibleEdges = true;
        newStyle.setViewFlags(dqCommon::ViewFlags(p));
        newStyle.setBackgroundColor(dqCommon::ColorDef::from(255, 0, 0).getTbgr());
        dqCommon::HiddenLineSettingsProps hl;
        hl.visible = dqCommon::HiddenLineStyleProps{};
        hl.visible->width = 7;
        newStyle.getSettings().setHiddenLineSettings(
            dqCommon::HiddenLineSettings::fromJSON(hl));
        dqCommon::LightSettingsProps lights;
        dqCommon::SolarLightProps solar;
        solar.intensity = 0.5;
        lights.solar = solar;
        newStyle.getSettings().setLights(dqCommon::LightSettings::fromJSON(lights));
    }

    f.view->SetDisplayStyle(newStyle);
    EXPECT_EQ(1, events);

    // viewFlags 逐位。
    EXPECT_TRUE(f.view->getViewFlags().grid());
    EXPECT_TRUE(f.view->getViewFlags().acsTriad());
    EXPECT_TRUE(f.view->getViewFlags().visibleEdges());
    // 背景色（tbgr）。
    EXPECT_EQ(dqCommon::ColorDef::from(255, 0, 0).getTbgr(),
              f.view->GetDisplayStyle().getBackgroundColor());
    // hline 宽。
    auto const& hl = f.view->GetDisplayStyle().getSettings().getHiddenLineSettings();
    ASSERT_TRUE(hl.visible.width.has_value());
    EXPECT_EQ(7, *hl.visible.width);
    // 灯光 solar intensity。
    EXPECT_NEAR(0.5,
                f.view->GetDisplayStyle().getSettings().getLights().solar.intensity,
                1e-12);
}

// 视口失效面（Viewport.cpp:723-728 的 OnDisplayStyleChanged 监听——
// changeFlags.SetDisplayStyle + InvalidateRenderPlan）。
TEST(DisplayStyleSwitch, AttachedViewportInvalidatesRenderPlan)
{
    SwitchFixture f;
    dqApp::Application::Get().GetViewManager().AddViewport(f.vp);
    // 帧稳定（renderPlan 校验位就位）。
    f.vp->RenderFrame();

    dqApp::DisplayStyle newStyle;
    dqCommon::ViewFlagsProperties p;
    p.grid = true;
    newStyle.setViewFlags(dqCommon::ViewFlags(p));
    f.view->SetDisplayStyle(newStyle);

    // RenderFrame 后 renderPlan 从新样式重建（背景/旗标面经 RenderPlan 消费）。
    f.vp->RenderFrame();
    EXPECT_TRUE(f.vp->GetView()->getViewFlags().grid());

    dqApp::Application::Get().GetViewManager().DropViewport(f.vp);
}

// M-Q Q-a：Viewport::overrideDisplayStyle——viewflags 合并 + 在场段等价失效。
// Ported from: itwinjs-core Viewport.overrideDisplayStyle (Viewport.ts:657-659)
//              + common DisplayStyleSettings._applyOverrides（合并语义面）。
// Authored: viewport-level 面（参考 DisplayStyle.test.ts 只锁 settings 层；
//           失效链无参考单测——行为锚 = ChangeFlags 监听面逐段）。
TEST(DisplayStyleSwitch, OverrideDisplayStyleMergesAndInvalidates)
{
    SwitchFixture f;
    dqApp::Application::Get().GetViewManager().AddViewport(f.vp);
    f.vp->RenderFrame();

    // 基态：grid 开（用户态偏离默认——merge 保位判据）。
    {
        dqApp::DisplayStyle& style = f.view->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = true;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    f.vp->RenderFrame();

    dqCommon::DisplayStyle3dSettingsProps o;
    dqCommon::ViewFlagProps vf;
    vf.renderMode = dqCommon::RenderMode::SolidFill;
    vf.monochrome = true;
    o.viewflags = vf;
    o.backgroundColor = 0xF0FFF0u;  // honeydew
    dqCommon::LightSettingsProps lights;
    lights.numCels = 2;
    o.lights = lights;
    dqCommon::EnvironmentProps env;
    env.sky = dqCommon::SkyBoxProps{};
    env.sky->display = true;
    o.environment = env;

    f.vp->overrideDisplayStyle(o);

    // 合并面：renderMode/monochrome 应用，grid 保位。
    auto const out = f.view->GetDisplayStyle().getViewFlags().Properties();
    EXPECT_EQ(out.renderMode, dqCommon::RenderMode::SolidFill);
    EXPECT_TRUE(out.monochrome);
    EXPECT_TRUE(out.grid) << "absent viewflag bits must keep current values";
    // 在场段应用：背景色 + 灯光 + 环境。
    EXPECT_EQ(f.view->GetDisplayStyle().getSettings().getBackgroundColor().getTbgr(),
              0xF0FFF0u);
    EXPECT_EQ(f.view->GetDisplayStyle().GetLightSettings().numCels, 2);
    EXPECT_TRUE(f.view->GetDisplayStyle().getEnvironment().displaySky);

    // 失效链：RenderFrame 后渲染计划从新设置重建（P5 同判据——读取面即新值）
    // + 装饰失效（环境段 → 天空指纹链可达）。
    f.vp->RenderFrame();
    EXPECT_EQ(f.view->GetDisplayStyle().getSettings().getBackgroundColor().getTbgr(),
              0xF0FFF0u);

    dqApp::Application::Get().GetViewManager().DropViewport(f.vp);
}

// ---------------------------------------------------------------------------
// M-S S-f：DisplayStyle::setThematic 门面（DisplayStyleSettings.ts:1221-1227
// setter——equals 短路 → **先 raise 后赋值** → 赋值）+ OnThematicChanged 接线
//（ported-but-uncalled 清偿：事件与 Viewport 监听面[Viewport.cpp:678]早已在，
// 本件前无 raise 方）。
// Authored: no reference test exists in itwinjs-core for the thematic setter
//           event（common/frontend 两侧测试目录均无 onThematicChanged 用例；
//           行为锚 = setter 原文序）。
// ---------------------------------------------------------------------------
TEST(DisplayStyleSwitch, ThematicFacadeSetterSemantics)
{
    SwitchFixture f;
    auto& style = f.view->GetDisplayStyle();

    int events = 0;
    dqCommon::ThematicDisplay seenAtRaise;  // 默认（range null）
    seenAtRaise.displayMode = dqCommon::ThematicDisplayMode::Slope;  // 哨兵：与默认 Height 区分
    seenAtRaise.range = dqGeom::Range1d(-777.0, -776.0);             // 哨兵值
    auto scope = style.OnThematicChanged.AddListener([&]() {
        ++events;
        seenAtRaise = style.getThematic();  // 事件内读取（参考序=旧值可观测）
    });

    // 等值写入 → 短路无事件（默认构造彼此相等——ThematicDisplay.equals 全字段面）。
    dqCommon::ThematicDisplay same;
    style.setThematic(same);
    EXPECT_EQ(events, 0);

    // 变更写入 → 事件恰一次 + 事件内读=**旧值**（参考 raise-先于-赋值序）+
    // 返回后新值就位。
    dqCommon::ThematicDisplay changed;
    changed.displayMode = dqCommon::ThematicDisplayMode::Slope;
    changed.range = dqGeom::Range1d(0.0, 90.0);
    style.setThematic(changed);
    EXPECT_EQ(events, 1);
    EXPECT_EQ(seenAtRaise.displayMode, dqCommon::ThematicDisplayMode::Height)
        << "listener must observe the PRE-assignment value (reference raise-first order)";
    EXPECT_TRUE(seenAtRaise.range.isNull());
    EXPECT_EQ(style.getThematic().displayMode, dqCommon::ThematicDisplayMode::Slope);
    EXPECT_FALSE(style.getThematic().range.isNull());

    // 再写同值 → 短路（equals 覆盖 displayMode+range 段）。
    style.setThematic(changed);
    EXPECT_EQ(events, 1);
}

// M-S S-f：projectExtents 补齐（DisplayStyleState.ts:1004-1016——overrides 携
// thematic 且新模式==Height 且无 range → projectExtents.z 填充 + setter 重写）。
// Authored: 同上（参考无此链单测；行为锚 = DisplayStyleState 监听体原文）。
TEST(DisplayStyleSwitch, ThematicHeightWithoutRangeFillsFromProjectExtents)
{
    SwitchFixture f;  // extents z=[-100,100]
    dqApp::Application::Get().GetViewManager().AddViewport(f.vp);
    f.vp->RenderFrame();

    int events = 0;
    auto scope = f.view->GetDisplayStyle().OnThematicChanged.AddListener(
        [&events]() { ++events; });

    // ①Height 无 range → 填 projectExtents.z（-100..100）+ 门面事件发。
    {
        dqCommon::DisplayStyle3dSettingsProps o;
        dqCommon::ThematicDisplayProps td;
        td.displayMode = dqCommon::ThematicDisplayMode::Height;
        o.thematic = td;
        f.vp->overrideDisplayStyle(o);
    }
    auto const& filled = f.view->GetDisplayStyle().getThematic();
    ASSERT_FALSE(filled.range.isNull());
    EXPECT_DOUBLE_EQ(filled.range.low, -100.0);
    EXPECT_DOUBLE_EQ(filled.range.high, 100.0);
    EXPECT_GE(events, 1);

    // ②Height 显式 range → 保持（补齐门不开）。
    {
        dqCommon::DisplayStyle3dSettingsProps o;
        dqCommon::ThematicDisplayProps td;
        td.displayMode = dqCommon::ThematicDisplayMode::Height;
        td.range = dqGeom::Range1d(3.0, 7.0);
        o.thematic = td;
        f.vp->overrideDisplayStyle(o);
    }
    auto const& kept = f.view->GetDisplayStyle().getThematic();
    ASSERT_FALSE(kept.range.isNull());
    EXPECT_DOUBLE_EQ(kept.range.low, 3.0);
    EXPECT_DOUBLE_EQ(kept.range.high, 7.0);

    // ③Slope 无 range → 不填（参考条件仅 Height；Slope range 是角度域）。
    {
        dqCommon::DisplayStyle3dSettingsProps o;
        dqCommon::ThematicDisplayProps td;
        td.displayMode = dqCommon::ThematicDisplayMode::Slope;
        o.thematic = td;
        f.vp->overrideDisplayStyle(o);
    }
    auto const& slope = f.view->GetDisplayStyle().getThematic();
    EXPECT_EQ(slope.displayMode, dqCommon::ThematicDisplayMode::Slope);
    EXPECT_TRUE(slope.range.isNull());

    dqApp::Application::Get().GetViewManager().DropViewport(f.vp);
}

// ---------------------------------------------------------------------------
// M-T T-b：DisplayStyle::setAmbientOcclusionSettings 门面（同 S-f thematic
// 门面形——equals 短路 → 先 raise 后赋值 → 赋值）+ OnAmbientOcclusionChanged
// 接线（ported-but-uncalled 清偿：事件与 Viewport 监听面[Viewport.cpp:690]
// 早已在、本件前无 raise 方）。
// Authored: no reference test exists in itwinjs-core for the AO settings
//           setter event（行为锚 = setter 族原文序 + AmbientOcclusion.test.ts
//           数据面已由 dqCommon AmbientOcclusionTest 覆盖）。
// ---------------------------------------------------------------------------
TEST(DisplayStyleSwitch, AoFacadeSetterSemantics)
{
    SwitchFixture f;
    auto& style = f.view->GetDisplayStyle();

    int events = 0;
    double seenIntensity = -1.0;  // 哨兵
    auto scope = style.OnAmbientOcclusionChanged.AddListener([&]() {
        ++events;
        seenIntensity = style.getAmbientOcclusionSettings().intensity;  // 事件内读=旧值
    });

    // 等值写入 → 短路无事件。
    dqCommon::AmbientOcclusion::Settings same;
    style.setAmbientOcclusionSettings(same);
    EXPECT_EQ(events, 0);

    // 变更写入 → 事件恰一次 + 事件内读=**旧值**（参考 raise-先于-赋值序——
    // 默认 intensity=1.0 旧值观测）+ 返回后新值就位。
    dqCommon::AmbientOcclusion::Settings changed;
    changed.intensity = 3.5;
    style.setAmbientOcclusionSettings(changed);
    EXPECT_EQ(events, 1);
    EXPECT_DOUBLE_EQ(seenIntensity, 1.0);
    EXPECT_DOUBLE_EQ(style.getAmbientOcclusionSettings().intensity, 3.5);

    // 再写同值 → 短路（equals 全字段面——blurSigma 翻面试真）。
    dqCommon::AmbientOcclusion::Settings sameAsChanged = changed;
    style.setAmbientOcclusionSettings(sameAsChanged);
    EXPECT_EQ(events, 1);
    sameAsChanged.blurSigma = 4.0;
    style.setAmbientOcclusionSettings(sameAsChanged);
    EXPECT_EQ(events, 2);
}

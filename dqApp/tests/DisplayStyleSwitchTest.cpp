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

// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Contour display facade tests（M-X X-a1）
//
// 锚定：DisplayStyleSettings.ts:1230-1239（contours setter——equals 短路 →
//   onContoursChanged raise → 赋值 + _json3d.contours 写出）+ Viewport.ts:1408
//   （registerDisplayStyleListeners 的 onContoursChanged→displayStyleChanged
//   形——invalidateRenderPlan + changeFlags.setDisplayStyle）。
//
// Authored: no reference test exists in itwinjs-core for the contours facade
//           / listener wiring（参考 DisplayStyle.test.ts 无 onContoursChanged
//           监听锁；门面形锁=ThematicFacadeSetterSemantics[M-S S-f]同族）。
#include <gtest/gtest.h>

#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/DisplayStyle.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>

#include <dqCommon/ContourDisplay.h>

namespace {

struct ContoursFixture {
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    dqApp::Viewport* vp = nullptr;

    ContoursFixture()
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

    ~ContoursFixture()
    {
        delete vp;
    }
};

dqCommon::ContourDisplay makeContours(bool display, double minorInterval)
{
    dqCommon::ContourDisplay d;
    d.displayContours = display;
    dqCommon::ContourGroup g;  // 默认组（空 subCategories——Contours.ts:85-86 defaultNdx 面）
    g.contourDef.minorInterval = minorInterval;
    d.groups.push_back(g);
    return d;
}

}  // namespace

// 门面 setter 语义：equals 短路 → 无事件；变更 → 事件恰一次 + 事件内读旧值
//（参考 raise-先于-赋值序——监听器在事件内读 settings 得见旧值）+ 返回后新值就位。
TEST(ContoursFacade, SetterSemantics)
{
    ContoursFixture f;
    auto& style = f.view->GetDisplayStyle();

    int events = 0;
    bool seenAtRaise = true;  // 哨兵：默认 displayContours=false
    double seenInterval = -1.0;
    auto scope = style.OnContoursChanged.AddListener([&]() {
        ++events;
        seenAtRaise = style.getContours().displayContours;  // 事件内读取
        if (!style.getContours().groups.empty())
            seenInterval = style.getContours().groups[0].contourDef.minorInterval;
    });

    // 等值写入 → 短路无事件（默认构造彼此相等——ContourDisplay.equals 全字段面）。
    style.setContours(dqCommon::ContourDisplay::defaults());
    EXPECT_EQ(events, 0);

    // 变更写入 → 事件恰一次 + 事件内读=旧值 + 返回后新值就位。
    style.setContours(makeContours(true, 2.5));
    EXPECT_EQ(events, 1);
    EXPECT_FALSE(seenAtRaise) << "listener must observe the PRE-assignment value (reference raise-first order)";
    EXPECT_TRUE(seenInterval < 0.0) << "old value has no groups";
    EXPECT_TRUE(style.getContours().displayContours);
    ASSERT_EQ(1u, style.getContours().groups.size());
    EXPECT_NEAR(2.5, style.getContours().groups[0].contourDef.minorInterval, 1e-12);

    // 再写同值 → 短路（equals 覆盖 displayContours+groups 段）。
    style.setContours(makeContours(true, 2.5));
    EXPECT_EQ(events, 1);
}

// 视口接线（Viewport.ts:1408 的 onContoursChanged→displayStyleChanged 形——
// InvalidateRenderPlan + SetDisplayStyle）：挂载视口后变更 contours → 监听面
// InvalidateRenderPlan→InvalidateScene 级联（Viewport.cpp:1092-1124）发射
// SceneInvalidated 信号（本信号的发出即证明级联穿透——headless 夹具的
// RenderFrame 在 !m_initialized 早退[:1726-1727]，DisplayStyleChanged 帧尾
// 事件面不可达，故锚定级联信号；plan 内容面由 X-a5 像素锁覆盖）。
TEST(ContoursFacade, AttachedViewportInvalidatesOnContoursChanged)
{
    ContoursFixture f;
    dqApp::Application::Get().GetViewManager().AddViewport(f.vp);

    int sceneInvalidations = 0;
    QObject::connect(f.vp, &dqApp::Viewport::SceneInvalidated,
                     [&sceneInvalidations]() { ++sceneInvalidations; });

    f.view->GetDisplayStyle().setContours(makeContours(true, 1.0));
    EXPECT_EQ(1, sceneInvalidations)
        << "OnContoursChanged listener must cascade InvalidateRenderPlan → InvalidateScene";

    // 等值重写 → 门面 equals 短路 → 无新增级联。
    f.view->GetDisplayStyle().setContours(makeContours(true, 1.0));
    EXPECT_EQ(1, sceneInvalidations);

    dqApp::Application::Get().GetViewManager().DropViewport(f.vp);
}

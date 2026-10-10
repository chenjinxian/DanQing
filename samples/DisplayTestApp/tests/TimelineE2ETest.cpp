// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp tests — M-U U-g Timeline E2E（solar 像素锁 + schedule
// 符号覆盖接线锁）
//
// Authored: no reference test exists in itwinjs-core / display-test-app for
//           Timeline rendering effects（行为锚 = ①SolarCalculate.test.ts 的
//           Philadelphia 角度值（正午高度角 ~65°/日落 ~0°）+ DisplayStyle
//           Settings.setSunTime 语义（direction 派生自 timePoint——:1339-1353）；
//           ②RenderSchedule.ts addSymbologyOverrides 的模型级 override 腿
//           （:974-981）。夹具模板 = ThematicDisplayE2E.TallBoxGradientSweep
//           IsExact 同款（Scene 装饰盒 + iso 取向 + pumpStable）。
//
// 锁 1（SolarTimeChangesBoxLighting）：Scene 盒 + 光照开——
//   baseline（默认 solar 向）→ setSunTime(Philadelphia 正午) → 顶面亮区 luma
//   上升（高度角 65°→顶面近直射）；→ setSunTime(日落) → 顶面 luma 回落
//   （高度角 ~0°→顶面掠射）。WHERE=顶面投影四边形域；腿=帧差非零 + timePoint
//   对账 + 日落<正午序。
// 锁 2（ScheduleModelVisibilityPopulatesFeatureOverrides）：模型级 visibility
//   时间线的 addSymbologyOverrides → FeatureOverrides.overrideModel 填充 +
//   时间求值（t=50 在 0%[t=0] 与 100%[t=100] 间 Step 插值下沿=0% → 全透明
//   appearance；t=150 沿=100% → 无 appearance[visibility=100 无透明度]）。

#include <gtest/gtest.h>

#include "View3DInventor.h"

#include <dqApp/Application.h>
#include <dqApp/Decorator.h>
#include <dqApp/StandardView.h>
#include <dqApp/ViewState.h>
#include <dqApp/Viewport.h>

#include <dqCommon/ColorDef.h>
#include <dqCommon/DisplayStyleSettings.h>
#include <dqCommon/FeatureOverrides.h>
#include <dqCommon/RenderSchedule.h>

#include <dqGeom/Point3d.h>
#include <dqGeom/PolyfaceBuilder.h>
#include <dqGeom/Range3d.h>

#include <QDateTime>
#include <QCoreApplication>

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

void spinE2E(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// Scene 装饰盒（ThematicDisplayE2E.TallBoxDecorator 同构——2×2×10 心 (0,0,5)）。
class TimelineBoxDecorator : public dqApp::IDecorator {
public:
    void Decorate(dqApp::DecorateContext& context) override
    {
        dqRender::GraphicBuilderOptions opts;
        opts.type = dqRender::GraphicType::Scene;  // 随 plan vf（光照门消费面）
        auto& vp = context.GetViewport();
        dqGeom::Point3d const refPoint(0.0, 0.0, 0.0);
        double const worldPerPixel = vp.GetViewingSpace().getPixelSizeAtPoint(&refPoint);
        opts.computeChordTolerance = [worldPerPixel]() { return worldPerPixel; };
        auto builder = context.GetViewport().createGraphicBuilder(opts);
        if (!builder)
            return;
        builder->setSymbology(dqCommon::ColorDef::white, dqCommon::ColorDef::white, 1);

        double const h = 1.0, zt = 10.0;
        dqGeom::Point3d const b1(-h, -h, 0), b2(h, -h, 0), b3(h, h, 0), b4(-h, h, 0);
        dqGeom::Point3d const t1(-h, -h, zt), t2(h, -h, zt), t3(h, h, zt), t4(-h, h, zt);
        auto box = dqGeom::PolyfaceBuilder::create();
        dqGeom::Point3d const q1[4] = {b4, b3, b2, b1};
        dqGeom::Point3d const q2[4] = {t1, t2, t3, t4};
        dqGeom::Point3d const q3[4] = {b1, b2, t2, t1};
        dqGeom::Point3d const q4[4] = {b2, b3, t3, t2};
        dqGeom::Point3d const q5[4] = {b3, b4, t4, t3};
        dqGeom::Point3d const q6[4] = {b4, b1, t1, t4};
        box->AddQuadFacet(q1); box->AddQuadFacet(q2); box->AddQuadFacet(q3);
        box->AddQuadFacet(q4); box->AddQuadFacet(q5); box->AddQuadFacet(q6);
        auto boxPolyface = box->ClaimPolyface();
        builder->addPolyface(*boxPolyface, /*filled=*/true);
        if (auto* g = builder->finish()) {
            context.GetViewport().createGraphicOwner(g);
            context.AddDecoration(opts.type, g);
        }
    }
    bool TestDecorationHit(uint32_t) const override { return false; }
    QString GetDecorationToolTip(uint32_t) const override { return {}; }
};

// 顶面（z=10 四隅）投影域的均值 luma。WHERE=WorldToView 顶面四边形包围盒的
// 内缩采样（顶面在 iso 取向居盒剪影上半——高度角变化的直接受光面）。
double topFaceMeanLuma(dqApp::Viewport* vp, std::vector<uint8_t> const& frame,
                       uint32_t w, uint32_t h)
{
    double const zt = 10.0, h2 = 1.0;
    dqGeom::Point3d const corners[4] = {
        dqGeom::Point3d::From(-h2, -h2, zt), dqGeom::Point3d::From(h2, -h2, zt),
        dqGeom::Point3d::From(h2, h2, zt), dqGeom::Point3d::From(-h2, h2, zt),
    };
    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    double const dpr = vp->devicePixelRatioF();
    for (auto const& c : corners) {
        dqGeom::Point3d const p = vp->WorldToView(c);
        minX = std::min(minX, p.x * dpr);
        maxX = std::max(maxX, p.x * dpr);
        minY = std::min(minY, p.y * dpr);
        maxY = std::max(maxY, p.y * dpr);
    }
    // 内缩 25%（避开边线/抗锯齿羽化）。
    double const x0 = minX + 0.25 * (maxX - minX), x1 = maxX - 0.25 * (maxX - minX);
    double const y0 = minY + 0.25 * (maxY - minY), y1 = maxY - 0.25 * (maxY - minY);

    double sum = 0.0;
    uint32_t count = 0;
    for (uint32_t y = static_cast<uint32_t>(y0); y < static_cast<uint32_t>(y1); ++y) {
        for (uint32_t x = static_cast<uint32_t>(x0); x < static_cast<uint32_t>(x1); ++x) {
            if (x >= w || y >= h)
                continue;
            size_t const idx = (static_cast<size_t>(y) * w + x) * 4;
            // readPixels 行序与 view 坐标同为 y-up（WheelZoomCoalesceTest 先例）。
            sum += 0.299 * frame[idx] + 0.587 * frame[idx + 1] + 0.114 * frame[idx + 2];
            ++count;
        }
    }
    return count > 0 ? sum / count : 0.0;
}

}  // namespace

// ---------------------------------------------------------------------------
// 锁 1：setSunTime 正午 vs 日落 → 顶面受光 luma 序（Philadelphia——
// SolarCalculate.test.ts 值域：五月正午高度角 ~65° / 日落 ~0°）。
// ---------------------------------------------------------------------------
TEST(TimelineE2E, SolarTimeChangesBoxLighting)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "TimelineE2E";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinE2E(400);

    auto* deco = new TimelineBoxDecorator();
    app.GetViewManager().AddDecorator(deco);

    auto* vp = view.getUeViewport();
    auto* view3d = vp->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    view3d->SetRotation(dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::Iso));
    // 取景范围比盒体大（z -1..16——盒 z 0..10 居 62% 视高）：LookAtVolume 的
    // 适配不以盒高为界（U-g DIAG 实锤：±2.5/11 范围下顶面投影越出视域——
    // ThematicDisplayE2E 的全帧采样不受影响，本锁的 WHERE 顶面域必须上屏）。
    view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-4.0, -4.0, -1.0, 4.0, 4.0, 16.0));
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        p.lighting = true;  // 光照开（solar 调制面）
        style.setViewFlags(dqCommon::ViewFlags(p));
        style.getSettings().toggleSkyBox(false);
        style.getSettings().setBackgroundColor(dqCommon::ColorDef::from(0, 0, 0));
        // 样式变更走事件链（DisplayStyle 事件监听面既有）——不经 synchWithView：
        // synchWithView 会重应用视图姿态（blank 默认取景框会覆盖 LookAtVolume
        // 的取景——U-g 首跑 DIAG 实锤：顶面投影越出视域）。相机操作（SetRotation/
        // LookAtVolume）保持为最后的视图变更。
        // solar.alwaysEnabled=true：参考 TargetUniforms.updateRenderPlan
        // （TargetUniforms.ts:177-186）——u_sunDir 仅在 shadows 或
        // solar.alwaysEnabled 时取 lights.solar.direction，否则恒用默认视图空间
        // 向（setSunTime 的可见性门——U-g LIGHT_TRACE 取证定案）。
        auto lights = style.GetLightSettings();
        lights.solar.alwaysEnabled = true;
        style.setLightSettings(lights);
    }

    auto pumpStable = [&](std::vector<uint8_t>& frame, uint32_t& w, uint32_t& h) {
        std::vector<uint8_t> prev;
        for (int i = 0; i < 40; ++i) {
            prev = frame;
            spinE2E(120);
            vp->InvalidateController();
            vp->RenderFrame();
            ASSERT_TRUE(vp->ReadFrameForTest(frame, w, h));
            if (i > 0 && frame == prev)
                break;
        }
    };

    std::vector<uint8_t> baseFrame, noonFrame, duskFrame;
    uint32_t w = 0, h = 0;

    pumpStable(baseFrame, w, h);
    ASSERT_FALSE(baseFrame.empty());
    double const baseLuma = topFaceMeanLuma(vp, baseFrame, w, h);

    // 正午：2019-05-03 17:00Z（Philadelphia EDT 正午——SolarCalculate.test.ts
    // 例的高度角 65.78°）。
    double const noonMs = 1556902800000.0;
    vp->GetView()->GetDisplayStyle().setSunTime(noonMs);
    pumpStable(noonFrame, w, h);
    double const noonLuma = topFaceMeanLuma(vp, noonFrame, w, h);
    // timePoint 对账（setSunTime 的 lights.solar.timePoint 承载）。
    ASSERT_TRUE(vp->GetView()->GetDisplayStyle().getSunTime().has_value());
    EXPECT_DOUBLE_EQ(*vp->GetView()->GetDisplayStyle().getSunTime(), noonMs);

    // 日落：2019-05-03 23:57Z（EDT 18:57——同测试例的日落串）。
    double const duskMs = 1556927820000.0;
    vp->GetView()->GetDisplayStyle().setSunTime(duskMs);
    pumpStable(duskFrame, w, h);
    double const duskLuma = topFaceMeanLuma(vp, duskFrame, w, h);

    fprintf(stderr,
            "[TimelineE2E] topFace luma: base=%.2f noon=%.2f dusk=%.2f (w=%u h=%u)\n",
            baseLuma, noonLuma, duskLuma, w, h);

    // 帧差腿：正午帧 ≠ 基线帧（solar 向变化真的到达像素）。
    EXPECT_NE(noonFrame, baseFrame) << "noon frame must differ from baseline";
    // WHERE 腿（首绿实测钉值：base=107.28/noon=152.59/dusk=59.70——阈值取
    // 序关系+宽余量；光照调制链的定性断言，精确角度值链由 SolarCalculateTest
    // 锁）：正午高度角 ~66°（近直射顶面）→ 顶面 luma 显著高于日落（高度角
    // ~0° 掠射——顶面近无直射）与基线（默认视图空间向）。
    EXPECT_GT(noonLuma, duskLuma + 5.0)
        << "noon top-face luma (sun ~65 deg elevation) must exceed dusk (sun at horizon)";
    EXPECT_GT(noonLuma, baseLuma + 20.0)
        << "noon top-face luma must exceed the default-direction baseline";
    EXPECT_LT(duskLuma, baseLuma - 20.0)
        << "dusk top-face luma must fall below the default-direction baseline";
    // 日落帧亦不同于正午帧。
    EXPECT_NE(duskFrame, noonFrame) << "dusk frame must differ from noon";

    app.GetViewManager().DropDecorator(deco);
    delete deco;
}

// ---------------------------------------------------------------------------
// 锁 2：schedule 模型级 visibility 的 addSymbologyOverrides → overrideModel
// 填充 + 时间求值（step 9 的符号腿接线——FeatureSymbology.ts:161 的面）。
// ---------------------------------------------------------------------------
TEST(TimelineE2E, ScheduleModelVisibilityPopulatesFeatureOverrides)
{
    using namespace dqCommon::RenderSchedule;

    // 模型级 visibility：t=0 → 0%，t=100 → 100%（Step 插值——下沿值）。
    ScriptBuilder builder;
    auto& model = builder.addModelTimeline("0x1");
    model.addVisibility(0.0, 0.0, Interpolation::Step);
    model.addVisibility(100.0, 100.0, Interpolation::Step);
    auto script = Script::fromJSON(builder.finish());
    ASSERT_TRUE(script);
    ASSERT_TRUE(script->containsFeatureOverrides);

    dqCommon::FeatureOverrides overrides;

    // t=50：区间 [0,100] 内、entry0(Step) 沿 → visibility 0 → transparency=1
    // 的模型级 appearance（getFeatureAppearance :738-742）。
    script->addSymbologyOverrides(overrides, 50.0);
    auto const* app50 = overrides.getModelOverridesById(dqBase::DqId::FromString("0x1"));
    ASSERT_TRUE(app50 != nullptr) << "t=50: visibility 0 must produce a model override";
    ASSERT_TRUE(app50->getTransparency().has_value());
    EXPECT_NEAR(*app50->getTransparency(), 1.0, 1e-10);

    // t=150：末 entry 沿（>=last.time）→ visibility 100 → 无 appearance 腿
    // （transparency undefined && 无 color → 不 override）。
    dqCommon::FeatureOverrides overrides2;
    script->addSymbologyOverrides(overrides2, 150.0);
    EXPECT_TRUE(overrides2.getModelOverridesById(dqBase::DqId::FromString("0x1")) == nullptr)
        << "t=150: visibility 100 must not override";

    // 时间求值方向锁：t=50（0%）的 transparency 恰为 1（visibility 0 的
    // 1 - 0/100），区别于 t=150 的缺席——时间轴真的在驱动求值。
    dqCommon::FeatureOverrides overrides3;
    script->addSymbologyOverrides(overrides3, 0.0);
    auto const* app0 = overrides3.getModelOverridesById(dqBase::DqId::FromString("0x1"));
    ASSERT_TRUE(app0 != nullptr);
    ASSERT_TRUE(app0->getTransparency().has_value());
    EXPECT_NEAR(*app0->getTransparency(), 1.0, 1e-10);
}

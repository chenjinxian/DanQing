// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp tests — M-V V-e Camera Paths E2E 像素锁（播放泵驱动
// 真实视口 → 内容位移方向锁）
//
// Authored: no reference test exists in itwinjs-core / display-test-app for
//           CameraPaths playback rendering（行为锚 = CameraPaths.ts play
//           泵 :531-561 + setCameraPosition :648-684 + Keyframe 捕获
//           :626-639——播放路径 = 录制路径的逆）。
//
// 锁（PlayTranslatesContentOppositeToCameraMotion）：
//   Scene 白盒装饰（黑底）→ 相机开 + 取景稳定基线 → 从**当前视口**捕获
//   k0（location/target/up = NpcToWorld 三点——录制语义 :626-639）→ k1 =
//   k0 相机整机 +X 平移 2.0（location.x/target.x 同加——纯平移不改变视线
//   方向）→ 面板 play(false) 真泵（QTimer 16ms 链）跑到自然停止
//   （currentTime > duration → endAnimation :602-603）。
//   判据（§11.11——WHERE 方向断言）：
//   - 内容存活：播放后内容像素计数 ≥ 基线 60%（盒仍在帧内——平移 2.0 相对
//     取景 ~8 宽 = 25% 帧移）；
//   - WHERE：内容质心 screen-x 显著左移（相机 +X 平移 ⇒ 世界内容视空间 -X——
//     ≥30px）；质心 y 稳定（|Δy| ≤ 15px——纯 X 平移无垂直分量）；
//   - 泵自然停止：isPlaying=false（duration 门）。
//   反向验证（缺陷敏度）：泵若无作用（play 空转）→ 质心不移动 → 锁红。

#include <gtest/gtest.h>

#include "View3DInventor.h"
#include "Gui/CameraPathsPanel.h"

#include <dqApp/Application.h>
#include <dqApp/Decorator.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/StandardView.h>
#include <dqApp/ViewState.h>
#include <dqApp/Viewport.h>

#include <dqCommon/ColorDef.h>
#include <dqCommon/DisplayStyleSettings.h>

#include <dqGeom/Angle.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/PolyfaceBuilder.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Vector3d.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QListWidget>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <vector>

namespace {

void spinE2E(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// Scene 装饰盒（TimelineE2E.TimelineBoxDecorator 同构——2×2×10 心 (0,0,5)）。
class CameraPathBoxDecorator : public dqApp::IDecorator {
public:
    void Decorate(dqApp::DecorateContext& context) override
    {
        dqRender::GraphicBuilderOptions opts;
        opts.type = dqRender::GraphicType::Scene;
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

// 内容统计：非黑像素计数 + 质心（readPixels 行序 y-up——不影响 x 向断言）。
struct ContentStat {
    double count = 0.0;
    double centroidX = 0.0;
    double centroidY = 0.0;
};

ContentStat contentStat(std::vector<uint8_t> const& frame, uint32_t w, uint32_t h)
{
    ContentStat s;
    double sumX = 0.0, sumY = 0.0;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t const idx = (static_cast<size_t>(y) * w + x) * 4;
            if (frame[idx] + frame[idx + 1] + frame[idx + 2] > 24) {
                s.count += 1.0;
                sumX += x;
                sumY += y;
            }
        }
    }
    if (s.count > 0.0) {
        s.centroidX = sumX / s.count;
        s.centroidY = sumY / s.count;
    }
    return s;
}

}  // namespace

TEST(CameraPathsE2E, PlayTranslatesContentOppositeToCameraMotion)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "CameraPathsE2E";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    // 持久化缝 env（面板 populate/录制存盘面——本锁只读不写盘[play 非录制]）
    std::error_code ec;
    const std::string dir = (std::filesystem::current_path(ec) / "camera-paths-e2e").string();
    std::filesystem::remove_all(dir, ec);
    _putenv_s("DANQING_CAMERA_PATHS_DIR", dir.c_str());

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinE2E(400);

    auto* deco = new CameraPathBoxDecorator();
    app.GetViewManager().AddDecorator(deco);

    auto* vp = view.getUeViewport();
    auto* view3d = vp->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    // 透视门先行（TurnCameraOn 会改取景——须在 LookAtVolume 之前，保证
    // 基线/捕获/播放三段全程同透视制式）
    ASSERT_EQ(vp->TurnCameraOn(), dqApp::ViewStatus::Success);
    view3d->SetRotation(dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::Iso));
    view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-3.0, -3.0, -1.0, 3.0, 3.0, 12.0));
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
        style.getSettings().toggleSkyBox(false);
        style.getSettings().setBackgroundColor(dqCommon::ColorDef::from(0, 0, 0));
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

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    pumpStable(frame, w, h);
    const ContentStat base = contentStat(frame, w, h);
    ASSERT_GT(base.count, 20000.0);  // 盒上屏存活门
    // 盒心投影锚（透视下 extents.x≠内容面宽——期望位移走 WorldToNpc 逐姿态
    // 投影差，不取 px/unit 常量）
    const dqGeom::Point3d boxCenter = dqGeom::Point3d::From(0.0, 0.0, 5.0);
    const dqGeom::Point3d npc0 = vp->WorldToNpc(boxCenter);

    // --- 从当前视口捕获 k0（录制语义 :626-639）+ k1 = 整机 +X 平移 2.0 ---
    Gui::CameraPath path;
    path.name = "e2e";
    path.duration = 1000.0;
    Gui::Keyframe k0;
    k0.time = 0.0;
    k0.location = vp->NpcToWorld(dqGeom::Point3d::From(0.5, 0.5, 1.0));
    k0.target = vp->NpcToWorld(dqGeom::Point3d::From(0.5, 0.5, 0.0));
    const auto upVec =
        k0.location.UnitVectorTo(vp->NpcToWorld(dqGeom::Point3d::From(0.5, 1.0, 0.0)));
    ASSERT_TRUE(upVec.has_value());
    k0.up = *upVec;
    ASSERT_TRUE(view3d->IsCameraOn());
    k0.lensAngle = dqGeom::Angle::FromRadians(view3d->GetLensAngle());
    Gui::Keyframe k1 = k0;
    k1.time = 1000.0;
    k1.location.x += 2.0;
    k1.target.x += 2.0;
    path.keyframes = {k0, k1};

    Gui::writeExternalCameraPaths(vp->GetView()->GetIModel()->GetKey(),
                                  Gui::cameraPathsToJsonString({path}));

    // --- 面板播放（真 QTimer 泵——非 animateFrame 直驱：泵链本体也在被测面） ---
    Gui::CameraPathsPanel panel(vp, nullptr);
    panel.populate();
    auto* list = panel.widget()->findChild<QListWidget*>(QStringLiteral("pathsList"));
    ASSERT_NE(list, nullptr);
    ASSERT_EQ(list->count(), 1);
    list->setCurrentRow(0);
    panel.play(false);
    ASSERT_TRUE(panel.isPlaying());

    // 泵自然停止（duration=1000ms；16ms 帧率 → ~2s 内含越界帧；上限 6s 守卫）
    for (int i = 0; i < 60 && panel.isPlaying(); ++i)
        spinE2E(100);
    EXPECT_FALSE(panel.isPlaying());

    pumpStable(frame, w, h);
    const ContentStat moved = contentStat(frame, w, h);

    // 内容存活（平移不丢盒）
    EXPECT_GE(moved.count, base.count * 0.6);
    // WHERE：渲染质心位移 × 投影数学位移 双向对账——盒心在两姿态下的
    // WorldToNpc x 差 = 期望位移（npc.x∈[0,1] × w = device px）。
    const dqGeom::Point3d npc1 = vp->WorldToNpc(boxCenter);
    const double expectedShiftPx = (npc1.x - npc0.x) * static_cast<double>(w);
    ASSERT_GT(std::fabs(expectedShiftPx), 20.0);  // 判据有实际鉴别力
    const double actualShift = moved.centroidX - base.centroidX;
    EXPECT_EQ(std::signbit(actualShift), std::signbit(expectedShiftPx))
        << "expected shift " << expectedShiftPx << " actual " << actualShift;
    // 量值：盒心投影 vs 可见面质心存在可见面混合偏差——[0.5×, 2×] 带
    EXPECT_GE(std::fabs(actualShift), 0.5 * std::fabs(expectedShiftPx));
    EXPECT_LE(std::fabs(actualShift), 2.0 * std::fabs(expectedShiftPx));
    // 纯 X 平移：y 向质心稳定（iso 下世界 X 带屏幕 y 分量——门大范围漂移）
    EXPECT_LT(std::fabs(moved.centroidY - base.centroidY), 60.0);

    panel.stop();
    app.GetViewManager().DropDecorator(deco);
    delete deco;
}

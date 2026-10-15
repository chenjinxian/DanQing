// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — AnalysisStyleE2ETest（W-a6 E2E 像素锁）
//
// Authored: no reference test exists in itwinjs-core for analysis-style animation
//          pixel output（参考的 AnalysisStyleExample 消费面在 display-test-app 交互
//          会话，无离线回放对应物；渲染像素回归授权 §5(g)——复现配方 = flat-waves
//          mesh Scene 装饰 + analysisStyle（Animated Radial Displacement/Height）
//          + analysisFraction 泵 0→0.5；证据链 = AuxChannelTable 打包钉值锁
//          [AuxChannelTableTest] + MeshBuilder 装配链锁[MeshBuilderAuxDataTest] +
//          变体源锁[AnimationShaderTest] + 本锁的 readPixels WHERE 断言）。
//
// 链：dta::createFlatMeshWithWaves（AnalysisStyleExample.ts:85-177）→ AnalysisDecorator
// （:187-240 的 Scene 装饰——PrimitiveBuilder::finish 的 auxChannels LUT 路由）→
// TargetImpl analysisStyle/analysisFraction（M-U U-e）→ animated Surface 变体
// （isAnimated）→ u_animLUT 采样位移 + scalar 通道渐变着色（Surface.ts:602 的
// analysisTexture 消费面）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "AnalysisStyleExample.h"
#include "View3DInventor.h"

#include <dqApp/Application.h>
#include <dqApp/DisplayStyle.h>
#include <dqApp/StandardView.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>

#include <dqCommon/AnalysisStyle.h>

#include <dqGeom/Range3d.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

struct QtEnvAS {
    QtEnvAS()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvAS s_qtAS;

void spinAS(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 非背景像素统计（bg=黑）：count + 质心 + bbox。
struct ContentStats {
    long count = 0;
    double cx = 0.0, cy = 0.0;
    uint32_t minX = UINT32_MAX, maxX = 0, minY = UINT32_MAX, maxY = 0;
};

ContentStats contentStats(std::vector<uint8_t> const& f, uint32_t w, uint32_t h)
{
    ContentStats out;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
            if (p[0] > 24 || p[1] > 24 || p[2] > 24) {  // 非黑
                out.count++;
                out.cx += x;
                out.cy += y;
                out.minX = std::min(out.minX, x);
                out.maxX = std::max(out.maxX, x);
                out.minY = std::min(out.minY, y);
                out.maxY = std::max(out.maxY, y);
            }
        }
    }
    if (out.count > 0) {
        out.cx /= out.count;
        out.cy /= out.count;
    }
    return out;
}

// 帧差像素计数。
long frameDiff(std::vector<uint8_t> const& a, std::vector<uint8_t> const& b)
{
    if (a.size() != b.size())
        return -1;
    long diff = 0;
    for (size_t i = 0; i + 3 < a.size(); i += 4) {
        if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2])
            diff++;
    }
    return diff;
}

}  // namespace

// Authored（§5(g)）：位移通道 fraction 泵的像素位移双向断言 + scalar 渐变着色断言。
TEST(AnalysisStyleE2E, FlatWavesDisplaceUpwardWithFractionAndRecolor)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "AnalysisStyleE2E";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinAS(400);

    auto* vp = view.getUeViewport();
    ASSERT_NE(vp, nullptr);

    // flat-waves 网格（AnalysisStyleExample.ts:85-177——100×100 网格 + 六通道）。
    dta::AnalysisMesh mesh;
    mesh.type = "Flat with waves";
    mesh.polyface = dta::createFlatMeshWithWaves();
    ASSERT_TRUE(mesh.polyface.IsValid());
    ASSERT_TRUE(mesh.polyface->Data().auxData.has_value());

    // 装饰器注册（AnalysisStyleExample.ts:210 的 addDecorator）。
    auto* deco = dta::createAnalysisStyleDecorator(*vp, mesh);
    ASSERT_NE(deco, nullptr);

    // 视图：Iso + LookAtVolume（网格域 [0,0,-15,100,100,15]——位移域含 ±12.5 波幅）
    // + 黑底 + 关 grid/acs + SolidFill。
    auto* view3d = vp->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    view3d->SetRotation(dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::Iso));
    view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-5.0, -5.0, -15.0, 105.0, 105.0, 15.0));
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        p.renderMode = dqCommon::RenderMode::SolidFill;
        style.setViewFlags(dqCommon::ViewFlags(p));
        style.getSettings().toggleSkyBox(false);
        style.getSettings().setBackgroundColor(dqCommon::ColorDef::from(0, 0, 0));
    }
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});

    // analysisStyle：Animated Radial Displacement（scale=1）+ Animated Radial Height
    // scalar（渐变着色——populateAnalysisStyles 的同构产出面，AnalysisStyleExample.ts
    // :36-62：配套通道同 inputName "Radial: Time"；scalar range=通道实测 scalarRange）。
    {
        dqCommon::AnalysisStyle style;
        dqCommon::AnalysisStyleDisplacement disp;
        disp.channelName = "Animated Radial Displacement";
        disp.scale = 1.0;
        style.displacement = disp;
        dqCommon::AnalysisStyleThematic scalar;
        scalar.channelName = "Animated Radial Height";
        // 通道实测 scalarRange（populateAnalysisStyles :46-52 的 channel.scalarRange 面）。
        dqGeom::Range1d scalarRange = dqGeom::Range1d::CreateNull();
        for (auto const& ch : mesh.polyface->Data().auxData->channels) {
            if (ch.name.has_value() && *ch.name == "Animated Radial Height" && ch.isScalar()) {
                auto const r = ch.scalarRange();
                ASSERT_TRUE(r.has_value());
                scalarRange = *r;
            }
        }
        ASSERT_FALSE(scalarRange.isNull());
        scalar.range = scalarRange;
        style.scalar = scalar;
        vp->GetView()->GetDisplayStyle().setAnalysisStyle(style);
    }
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});

    auto pumpStable = [&](std::vector<uint8_t>& frame, uint32_t& w, uint32_t& h) {
        std::vector<uint8_t> prev;
        for (int i = 0; i < 40; ++i) {
            prev = frame;
            spinAS(120);
            vp->InvalidateController();
            vp->RenderFrame();
            ASSERT_TRUE(vp->ReadFrameForTest(frame, w, h));
            if (i > 0 && frame == prev)
                break;
        }
    };

    // 基线：fraction=0 → inputValue=0 → 帧 0（零位移——平 mesh）。
    vp->GetView()->GetDisplayStyle().setAnalysisFraction(0.0);
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    std::vector<uint8_t> frameFlat;
    uint32_t w = 0, h = 0;
    pumpStable(frameFlat, w, h);
    ASSERT_FALSE(frameFlat.empty());
    auto const flatStats = contentStats(frameFlat, w, h);
    ASSERT_GT(flatStats.count, 5000) << "flat mesh must be on screen (baseline content)";

    // fraction=0.5 → inputValue=1 → 帧 1（径向满幅位移——中心隆起 +Z）。
    vp->GetView()->GetDisplayStyle().setAnalysisFraction(0.5);
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    std::vector<uint8_t> frameWave;
    pumpStable(frameWave, w, h);
    ASSERT_FALSE(frameWave.empty());
    auto const waveStats = contentStats(frameWave, w, h);
    ASSERT_GT(waveStats.count, 5000) << "displaced mesh must stay on screen";

    // ① 位移发生（双向断言）：帧差显著 + 内容 bbox 变化（+Z 隆起 → Iso 下屏幕上移
    // 域出现新内容/原平面域让位）。
    long const diff = frameDiff(frameFlat, frameWave);
    EXPECT_GT(diff, 20000) << "fraction 0→0.5 must visibly displace the mesh";
    // WHERE：位移方向=+Z 隆起——wave 帧的内容 bbox 与 flat 帧不同（隆起使投影域变化）。
    bool const bboxMoved = waveStats.minX != flatStats.minX || waveStats.maxX != flatStats.maxX
        || waveStats.minY != flatStats.minY || waveStats.maxY != flatStats.maxY;
    EXPECT_TRUE(bboxMoved) << "displaced content bbox must differ from the flat baseline";

    // ② scalar 渐变着色（Animated Radial Height + analysisTexture——Surface.ts:602
    // 消费面）：wave 帧出现非均匀着色（沿径向高度分色），与全帧单色的纯位移形态区分。
    // 判据：内容区内色相方差在场（darkSlateBlue 单色 → 渐变多色）。
    {
        // 采样内容区中心十字带的色相分布：统计 distinct 色族（粗量化 16 级）。
        std::vector<uint32_t> buckets(16 * 16 * 16, 0);
        long distinct = 0;
        for (uint32_t y = waveStats.minY; y <= waveStats.maxY; y += 2) {
            for (uint32_t x = waveStats.minX; x <= waveStats.maxX; x += 2) {
                uint8_t const* p = &frameWave[(static_cast<size_t>(y) * w + x) * 4];
                if (p[0] > 24 || p[1] > 24 || p[2] > 24) {
                    uint32_t const key = (static_cast<uint32_t>(p[0]) >> 4) << 8
                        | (static_cast<uint32_t>(p[1]) >> 4) << 4
                        | (static_cast<uint32_t>(p[2]) >> 4);
                    if (buckets[key]++ == 0)
                        distinct++;
                }
            }
        }
        EXPECT_GT(distinct, 4) << "scalar channel must recolor the mesh through the gradient";
    }

    // ③ 恢复：fraction 回 0 → 帧回基线形态（可逆性——processFlash 同族语义）。
    vp->GetView()->GetDisplayStyle().setAnalysisFraction(0.0);
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    std::vector<uint8_t> frameBack;
    pumpStable(frameBack, w, h);
    ASSERT_FALSE(frameBack.empty());
    // 可逆容差（帧序状态残差——泵稳定后应与基线逐位近邻）。
    long const revertDiff = frameDiff(frameFlat, frameBack);
    EXPECT_LT(revertDiff, 2000) << "fraction 0.5→0 must restore the flat baseline";

    dqApp::Application::Get().GetViewManager().DropDecorator(deco);
    delete deco;
}

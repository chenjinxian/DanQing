// Authored: no reference test exists in itwinjs-core for real-window grid
//           rendering (openBlankViewport asserts color sets only)。
// 真窗口回归测试：应用真实渲染路径（pipeline+事件循环）勾 Grid 后的合成帧。
// 回归历史（2026-09-12 OIT 链修复，详见 SceneCompositorImpl.cpp/OpenGLState.cpp
// 注释）：(1) OIT 清屏 accum.a 误为 0（参考 ClearTranslucent.ts 为 1）→ 空像素
// 合成出黑覆盖全帧；(2) _translucentRenderState 混合因子按 GL 参数序误读
// （itwinjs setBlendFuncSeparate 签名是 srcRgb,srcAlpha,dstRgb,dstAlpha）；
// (3) rhi::BlendFunction/BlendEquation 序数当 GL 常量强转 → glBlendFuncSeparatei
// 无效枚举静默丢失；(4) OpenGLState 去重缓存与 RenderState 裸 GL 通道分叉 →
// 深度清屏/混合关闭被跳过。断言：合成帧不得全黑（天空必须穿透 OIT 合成）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QThread>
#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/Viewport.h>
#include <dqCommon/ViewFlags.h>
#include <cstdio>
#include <vector>
namespace { struct QtEnv { QtEnv() { if (!qApp) { static int argc=1; static char n[]="t"; static char* av[]={n,nullptr}; new QApplication(argc,av);} } }; }
static QtEnv s_qt;

TEST(GridAppDiag, GridOnFrameContent)
{
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(-1000,-1000,-100, 1000,1000,100);
    auto conn = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
    auto* vp = dqApp::Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);
    vp->resize(300, 200);
    vp->move(0, 0);
    vp->show();
    {
        auto& app = dqApp::Application::Get();
        if (!app.isInitialized()) {
            dqApp::Application::Options opts;
            opts.applicationId = "GridAppDiag";
            opts.applicationVersion = "1.0";
            opts.noRender = true;
            ASSERT_TRUE(app.Startup(opts));
        }
        app.GetViewManager().AddViewport(vp);
    }
    for (int i = 0; i < 20; ++i) { QCoreApplication::processEvents(); QThread::msleep(20); }
    // 勾 Grid（ViewSettingsPanel.applyFlags 同路径）
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = true;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    for (int i = 0; i < 20; ++i) { QCoreApplication::processEvents(); QThread::msleep(20); }
    for (int i = 0; i < 40; ++i) { QCoreApplication::processEvents(); QThread::msleep(50); }
    vp->RenderFrame();
    std::vector<uint8_t> frame;
    uint32_t fw = 0, fh = 0;
    bool ok = vp->ReadFrameForTest(frame, fw, fh);
    printf("[APPDIAG] readback=%d size=%zu w=%u h=%u", ok ? 1 : 0, frame.size(), fw, fh);
    putchar(10);
    if (ok && frame.size() >= static_cast<size_t>(fw) * fh * 4) {
        int hist[8] = {};
        size_t const w = fw;
        size_t const h = fh;
        for (size_t i = 0; i < frame.size(); i += 4) {
            int l = (frame[i] * 299 + frame[i+1] * 587 + frame[i+2] * 114) / 1000;
            hist[l * 8 / 256]++;
        }
        for (int b = 0; b < 8; ++b)
            if (hist[b]) printf("[APPDIAG] lum[%3d-%3d]=%d", b*32, b*32+31, hist[b]), putchar(10);
        uint8_t const* c = &frame[((h - 1 - h/2) * w + w/2) * 4];
        printf("[APPDIAG] center=(%d,%d,%d)", c[0], c[1], c[2]);
        putchar(10);
        // 合成帧不得全黑：天空（142,205,255，lum≈199 → bucket 6）必须穿透 OIT 合成。
        // 回归根因：OIT clear accum.a=0（参考 ClearTranslucent.ts 为 1）+ 混合因子
        // 误读（itwinjs setBlendFuncSeparate 签名是 srcRgb,srcAlpha,dstRgb,dstAlpha），
        // 空 accum 像素合成出 (0,0,0,0) 覆盖天空。参考行为：空像素 → opaque 原样。
        int nonBlack = 0;
        for (int b = 1; b < 8; ++b) nonBlack += hist[b];
        EXPECT_GT(nonBlack, 0) << "composited frame is all black — OIT chain killed the opaque scene";
        // 网格必须可见：黑网格以 planeTransparency=0.9/line=0.75 淡铺在天空上
        // （GridDecorator→drawStandardGrid→WorldDecoration→OIT），全帧不得只剩纯天空。
        EXPECT_LT(hist[5] + hist[6] + hist[7], static_cast<int>(w * h))
            << "grid on: frame must not be plain sky (grid plane + lines missing)";
    }
    dqApp::Application::Get().GetViewManager().DropViewport(vp);
    delete vp;
}

// Authored: no reference test exists in itwinjs-core for real-window ACS triad
//           rendering。真窗口回归：勾 ACS Triad 后，WorldOverlay 三色轴
//           （AuxCoordSys.ts:215 X红/Y绿/Z蓝 + :2298 原点蓝点）必须出现在合成帧。
TEST(GridAppDiag, AcsTriadOnFrameContent)
{
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(-1000,-1000,-100, 1000,1000,100);
    auto conn = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
    auto* vp = dqApp::Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);
    vp->resize(300, 200);
    vp->move(0, 0);
    vp->show();
    {
        auto& app = dqApp::Application::Get();
        if (!app.isInitialized()) {
            dqApp::Application::Options opts;
            opts.applicationId = "GridAppDiag";
            opts.applicationVersion = "1.0";
            opts.noRender = true;
            ASSERT_TRUE(app.Startup(opts));
        }
        app.GetViewManager().AddViewport(vp);
    }
    for (int i = 0; i < 20; ++i) { QCoreApplication::processEvents(); QThread::msleep(20); }

    auto countDominant = [&](int& red, int& green, int& deepBlue) {
        red = green = deepBlue = 0;
        std::vector<uint8_t> frame;
        uint32_t fw = 0, fh = 0;
        vp->RenderFrame();
        if (!vp->ReadFrameForTest(frame, fw, fh)) return;
        for (size_t i = 0; i + 3 < frame.size(); i += 4) {
            int r = frame[i], g = frame[i+1], b = frame[i+2];
            if (r > g + 40 && r > b + 40) ++red;
            else if (g > r + 40 && g > b + 40) ++green;
            // 深蓝（Z 盘/尖点/原点蓝点）：天空 (142,205,255) 同为蓝主色但 g=205，
            // 以 g<120 区分（Z 盘蓝 78% 叠天空 g≈75，纯蓝点 g≈60）。
            else if (b > 150 && r < 100 && g < 120) ++deepBlue;
        }
    };

    // 门控（AccuDraw.ts:2291 viewFlags.acsTriad）：未勾时帧内无红/绿轴与深蓝像素。
    int r0, g0, b0;
    countDominant(r0, g0, b0);
    printf("[APPDIAG] acs-off dominant rgb=(%d,%d,%d)", r0, g0, b0); putchar(10);

    // 勾 ACS Triad（ViewSettingsPanel.applyFlags 同路径）。
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.acsTriad = true;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    for (int i = 0; i < 20; ++i) { QCoreApplication::processEvents(); QThread::msleep(20); }

    int r1, g1, b1;
    countDominant(r1, g1, b1);
    printf("[APPDIAG] acs-on dominant rgb=(%d,%d,%d)", r1, g1, b1); putchar(10);
    EXPECT_EQ(r0, 0) << "ACS gate: red axis visible without acsTriad flag";
    EXPECT_EQ(g0, 0) << "ACS gate: green axis visible without acsTriad flag";
    EXPECT_EQ(b0, 0) << "ACS gate: blue disc/point visible without acsTriad flag";
    EXPECT_GT(r1, 10) << "X axis (red) must render when acsTriad is on";
    EXPECT_GT(g1, 10) << "Y axis (green) must render when acsTriad is on";
    EXPECT_GT(b1, 10) << "Z disc/tip + origin point (blue) must render when acsTriad is on";

    // CheckVisible|Active（AccuDraw.ts:2292）→ isOriginInView（AuxCoordSys.ts:136-165）：
    // ACS 原点移出视口时，triad 钳回视口边缘（Deemphasized：pixelSize×0.8、
    // 透明度 150/225）而不是消失。把视图原点移开使世界原点出视口，triad 必须仍可见。
    {
        auto* v3 = vp->GetView()->AsViewState3d();
        ASSERT_NE(v3, nullptr);
        auto o = v3->GetOrigin();
        v3->SetOrigin(dqGeom::Point3d{o.x + 5000.0, o.y, o.z});  // 世界原点甩出左缘外
        vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
        for (int i = 0; i < 20; ++i) { QCoreApplication::processEvents(); QThread::msleep(20); }
        int r2, g2, b2;
        countDominant(r2, g2, b2);
        printf("[APPDIAG] acs-offview dominant rgb=(%d,%d,%d)", r2, g2, b2); putchar(10);
        // 参考行为（AuxCoordSys.ts:275-276 + :150-154）：原点出视口 → triad 钳回
        // 视口边缘并以 Deemphasized（×0.8、透明度 150/225）绘制——颜色被稀释
        // （红/绿填充 alpha≈0.12/描边≈0.41，落到非"主色"区间），故按"左半屏存在
        // 非天空像素"断言可见性，而非主色计数。
        {
            std::vector<uint8_t> frame;
            uint32_t fw = 0, fh = 0;
            vp->RenderFrame();
            int nonSkyLeft = 0, redIsh = 0, greenIsh = 0, darkish = 0;
            if (vp->ReadFrameForTest(frame, fw, fh)) {
                for (uint32_t y = 0; y < fh; ++y) {
                    for (uint32_t x = 0; x < fw / 2; ++x) {
                        uint8_t const* p = &frame[((size_t)y * fw + x) * 4];
                        int r = p[0], g = p[1], b = p[2];
                        bool const sky = std::abs(int(r) - 142) < 12 && std::abs(int(g) - 205) < 12
                                      && std::abs(int(b) - 255) < 12;
                        if (sky) continue;
                        ++nonSkyLeft;
                        if (r > g + 15 && r > b + 5) ++redIsh;
                        if (g > r + 15 && g > b + 10) ++greenIsh;
                        if (r < 110 && g < 150 && b < 180) ++darkish;
                    }
                }
                printf("[APPDIAG] offview nonSkyLeft=%d redIsh=%d greenIsh=%d darkish=%d",
                       nonSkyLeft, redIsh, greenIsh, darkish); putchar(10);
            }
            EXPECT_GT(nonSkyLeft, 100)
                << "origin off-view: triad must be clamped into view (AuxCoordSys.ts:150-154)";
            EXPECT_GT(redIsh, 5) << "X axis (diluted red, Deemphasized) must stay visible";
            EXPECT_GT(greenIsh, 5) << "Y axis (diluted green, Deemphasized) must stay visible";
            EXPECT_GT(darkish, 5) << "axis labels/outline (diluted dark) must stay visible";
        }
    }

    dqApp::Application::Get().GetViewManager().DropViewport(vp);
    delete vp;
}

// View3DResizeTest — View3DInventor viewport survives window-resize storms.
//
// Authored: no reference test exists in display-test-app/FreeCAD for this —
// 用户报告"多次缩放后 ACS 和 Grid 都看不到了，点 Fit 也没用了"（2026-09-13）。
// 证据链：用户会话两个 WER dump（11:36/11:44，c0000409 FAIL_FAST =
// QMessageLogger::fatal，栈 processGeometryChangeEvent → QWidgetRepaintManager::
// paintAndFlush → QBackingStore::resize）都落在窗口几何变化路径；dqApp 层已由
// WheelZoomCoalesceTest.DeepZoomKeepsDecorationsAlive 证明深度缩放下装饰健康，
// 故本测试把范围收敛到 DisplayTestApp 的 View3DInventor（WA_PaintOnScreen
// native viewport 子窗口 × 父窗口 resize）——崩溃即失败（不崩即过）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QScreen>
#include <QThread>

#include <dqCommon/ViewFlags.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "View3DInventor.h"

#include <dqApp/ToolAdmin.h>
#include <dqApp/Viewport.h>

// 保证 QApplication 存在（DtaToolBarsTest 同款模式；进程级只建一次）。
namespace { struct QtEnvR { QtEnvR() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvR s_qtR;

// App::Application 桩单例定义在 DtaToolBarsTest.cpp（同一测试目标，仅一份）。
#include <App/Application.h>

namespace {

// 泵事件 ~ms 毫秒（墙钟），让 swapchain init / resize / paint 事件全部送达。
void spin(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

}  // namespace

// 真实 View3DInventor（BlankConnection + dqApp::Viewport + AddViewport）经 resize
// 风暴后不崩溃、视口图形管线仍可用（后续帧渲染不崩）。
TEST(View3DResize, ResizeStormKeepsViewportHealthy)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1200, 800);
    view.show();
    spin(400);  // swapchain init + 首帧

    QSize const sizes[] = {
        {900, 600}, {1400, 900}, {800, 500}, {1300, 850},
        {1500, 950}, {1000, 700}, {1200, 800},
    };
    for (auto const& sz : sizes) {
        view.resize(sz);
        spin(120);
        // 不崩即健康（崩溃会终止测试进程 → gtest 报失败）。
    }

    // 关闭路径（closeEvent → DropViewport + Shutdown）也在测试覆盖内。
    view.close();
    spin(200);
}

// 真 showMaximized() + 像素级 Grid 存活断言——复现用户报告"最大化后 Grid
// 消失"（resize 模拟覆盖不到 showMaximized 的窗口管理器路径：SetWindowPos
// 序列/中间态/dpr 变化）。GridAppDiag 同款像素采样（readPixels 合成帧）。
// Authored: no reference test exists（渲染输出级回归，参考无对应测试）。
TEST(View3DResize, MaximizeKeepsGridVisible)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1200, 800);
    view.show();
    spin(500);

    // 勾 Grid（GridAppDiag 的 synch 路径）。
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = true;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spin(400);
    view.getUeViewport()->RenderFrame();

    // TEMP-DIAG: 装饰收集与场景状态探针。
    printf("[MAXGRID] diag: world=%zu overlay=%zu sky=%d sceneValid=%d decoValid=%d\n",
           view.getUeViewport()->GetDecorations().world.size(),
           view.getUeViewport()->GetDecorations().worldOverlay.size(),
           view.getUeViewport()->GetDecorations().skyBox ? 1 : 0,
           view.getUeViewport()->GetDecorationsValidForTest() ? 1 : 0,
           view.getUeViewport()->GetDecorationsValidForTest() ? 1 : 0);

    // 基线：最大化前中心带 Grid 对比像素数（背景取左下角）。
    std::vector<uint8_t> before;
    uint32_t bw = 0, bh = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(before, bw, bh));
    auto contrastBand = [](std::vector<uint8_t> const& f, uint32_t w, uint32_t h) {
        uint8_t const* bg = &f[(static_cast<size_t>(h - 4) * w + 4) * 4];
        int count = 0;
        for (uint32_t y = h / 2 - h / 20; y < h / 2 + h / 20; ++y)
            for (uint32_t x = 0; x < w; x += 2) {
                uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
                int const d = std::abs(int(p[0]) - int(bg[0]))
                            + std::abs(int(p[1]) - int(bg[1]))
                            + std::abs(int(p[2]) - int(bg[2]));
                if (d > 40) ++count;
            }
        return count;
    };
    int const beforeBand = contrastBand(before, bw, bh);
    printf("[MAXGRID] before: %ux%u band=%d centerRGB=(%d,%d,%d) cornerRGB=(%d,%d,%d)\n",
           bw, bh, beforeBand,
           before[((static_cast<size_t>(bh / 2) * bw + bw / 2) * 4)],
           before[((static_cast<size_t>(bh / 2) * bw + bw / 2) * 4) + 1],
           before[((static_cast<size_t>(bh / 2) * bw + bw / 2) * 4) + 2],
           before[(static_cast<size_t>(bh - 4) * bw + 4) * 4],
           before[(static_cast<size_t>(bh - 4) * bw + 4) * 4 + 1],
           before[(static_cast<size_t>(bh - 4) * bw + 4) * 4 + 2]);
    ASSERT_GT(beforeBand, 200) << "基线帧无 Grid（测试环境异常）";

    // 真 最大化（窗口管理器路径）。诊断已证明：resizeEvent→InvalidateController→
    // RequestRedraw→drawFrame（translucentCmds=3，grid 在命令中）机制正常——
    // 丢失发生在 GL 输出层（OIT 合成/像素呈现），见 memory 登记的下一步。
    // 本测试锁定当前行为基线（已知问题），防止进一步回归。
    view.showMaximized();
    spin(1200);
    view.getUeViewport()->RequestRedraw();
    view.getUeViewport()->RenderFrame();

    std::vector<uint8_t> after;
    uint32_t aw = 0, ah = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(after, aw, ah));
    int const afterBand = contrastBand(after, aw, ah);
    printf("[MAXGRID] after maximize: %ux%u band=%d\n", aw, ah, afterBand);
    // Grid 不得消失（参考 PlanarGrid 自适应倍率允许密度变化，下限为存在性）。
    EXPECT_GT(afterBand, 200) << "最大化后 Grid 消失";

    view.close();
    spin(200);
}

// 深度 zoom-in 后最大化——用户报告的完整崩溃序列（"不停放大后，窗口最大化
// 按钮点击后会崩溃"）。深度缩放后 extents 极小（extentLimits clamp 边界），
// 最大化触发的大幅度 resize 在此状态下可能崩溃。不崩即过。
TEST(View3DResize, DeepZoomThenMaximize)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1200, 800);
    view.show();
    spin(400);

    auto* vp = view.findChild<dqApp::Viewport*>();
    ASSERT_NE(vp, nullptr);

    // 深度 zoom-in：连续滚轮手势（每帧合并 1 次，持续 200 帧——同
    // WheelZoomCoalesceTest.DeepZoomInClampKeepsDecorationsAndFit 的形态）。
    for (int frame = 0; frame < 200; ++frame) {
        dqApp::ToolEvent te;
        te.type = dqApp::ToolEventType::Wheel;
        te.vp = vp;
        te.posx = 400.0f;
        te.posy = 300.0f;
        te.wheelDeltaY = 120.0f;
        dqApp::ToolAdmin::addEvent(te);
        spin(16);
    }
    spin(800);  // 动画收敛

    // 最大化（极端 resize）。
    auto const screen = QGuiApplication::primaryScreen()->availableGeometry();
    view.resize(screen.width(), screen.height());
    spin(300);
    // 不崩即健康。

    view.close();
    spin(200);
}

// 视口内滚轮缩放（经 ToolAdmin 队列，应用级同路径）与 resize 交错——覆盖用户
// "多次缩放 + 调整窗口"的复合序列。
TEST(View3DResize, ZoomThenResizeInterleaved)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1200, 800);
    view.show();
    spin(400);

    auto* vp = view.findChild<dqApp::Viewport*>();
    ASSERT_NE(vp, nullptr) << "viewport widget inside View3DInventor";

    for (int round = 0; round < 4; ++round) {
        for (int i = 0; i < 10; ++i) {
            dqApp::ToolEvent te;
            te.type = dqApp::ToolEventType::Wheel;
            te.vp = vp;
            te.posx = 400.0f;
            te.posy = 300.0f;
            te.wheelDeltaY = (round % 2 == 0) ? 120.0f : -120.0f;
            dqApp::ToolAdmin::addEvent(te);
            spin(25);
        }
        spin(300);  // 动画收敛
        view.resize(QSize(1000 + 150 * round, 700 + 60 * round));
        spin(200);
    }
    view.close();
    spin(200);
}

// ---------------------------------------------------------------------------
// 最小化→恢复循环后【屏幕】不黑屏（用户 2026-09-14 报告：多次最小化再打开
// 依然黑屏，resize 可救——SURFHEAL(Show→rebind) 首版未根治）。屏幕级断言
// （GDI 抓窗口区域平均亮度）+ FBO 对照：FBO 亮而屏幕黑=呈现/重绘未发生；
// 双黑=渲染输入丢。自愈断言：恢复后只 spin 泵事件（不手动 RenderFrame——
// 用户不按键，窗口系统事件链必须自己触发重绘）。
// Authored: no reference test exists（窗口最小化行为，参考浏览器无对应物）。
// ---------------------------------------------------------------------------
TEST(View3DResize, MinimizeRestoreKeepsScreenAlive)
{
#ifdef _WIN32
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1100, 700);
    view.show();
    spin(500);

    auto* vp = view.findChild<dqApp::Viewport*>();
    ASSERT_NE(vp, nullptr);
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = true;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spin(400);

    // 视口区域屏幕平均亮度（GDI BitBlt；前台守卫后抓取）。
    auto screenMean = [&]() -> int {
        view.window()->raise();
        view.window()->activateWindow();
        spin(250);
        double const dpr = vp->devicePixelRatioF();
        QPoint const g = vp->mapToGlobal(QPoint(0, 0));
        int const px = static_cast<int>(g.x() * dpr), py = static_cast<int>(g.y() * dpr);
        int const pw = static_cast<int>(vp->width() * dpr), ph = static_cast<int>(vp->height() * dpr);
        HDC screenDC = GetDC(nullptr);
        HDC memDC = CreateCompatibleDC(screenDC);
        HBITMAP bmp = CreateCompatibleBitmap(screenDC, pw, ph);
        HGDIOBJ old = SelectObject(memDC, bmp);
        BitBlt(memDC, 0, 0, pw, ph, screenDC, px, py, SRCCOPY | CAPTUREBLT);
        long long sum = 0;
        int const n = 64;
        for (int i = 0; i < n; ++i) {
            int const x = (pw * (2 * (i % 8) + 1)) / 16;
            int const y = (ph * (2 * (i / 8) + 1)) / 16;
            COLORREF const c = GetPixel(memDC, x, y);
            sum += (GetRValue(c) + GetGValue(c) + GetBValue(c)) / 3;
        }
        SelectObject(memDC, old);
        DeleteObject(bmp);
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return static_cast<int>(sum / n);
    };
    auto fboMean = [&]() -> int {
        std::vector<uint8_t> f;
        uint32_t w = 0, h = 0;
        if (!vp->ReadFrameForTest(f, w, h)) return -1;
        long long sum = 0;
        for (uint32_t i = 0; i < 64; ++i) {
            size_t const idx = ((h / 8 * (i / 8) + h / 16) * w + (w / 8 * (i % 8) + w / 16)) * 4;
            sum += (f[idx] + f[idx + 1] + f[idx + 2]) / 3;
        }
        return static_cast<int>(sum / 64);
    };

    int const base = screenMean();
    printf("[MINREST] baseline screen=%d fbo=%d\n", base, fboMean());
    ASSERT_GT(base, 60) << "基线即黑（测试环境异常）";

    for (int cycle = 1; cycle <= 3; ++cycle) {
        int const pres0 = vp->m_dbgPresentCount;
        view.showMinimized();
        spin(700);
        int const presMin = vp->m_dbgPresentCount;
        view.showNormal();
        spin(900);  // 自愈窗口：窗口系统事件 + 动画计时器应触发重绘上屏
        int const presHeal = vp->m_dbgPresentCount;
        HWND const hwnd = reinterpret_cast<HWND>(vp->winId());
        int const heal = screenMean();       // 自愈态（无手动干预）
        // TEMP-DIAG：显式画一帧再测——区分"没触发重绘"（显式后亮）与
        // "present 上不了屏"（显式后仍黑）。
        vp->RequestRedraw();
        vp->RenderFrame();
        spin(400);
        int const forced = screenMean();
        int const presForced = vp->m_dbgPresentCount;
        printf("[MINREST] cycle %d: iconic=%d heal=%d forced=%d fbo=%d present=%d/%d/%d/%d\n",
               cycle, IsIconic(hwnd) ? 1 : 0, heal, forced, fboMean(),
               pres0, presMin, presHeal, presForced);
        // 屏幕必须不黑（内容存活，非仅 FBO）。
        EXPECT_GT(heal, 60) << "最小化恢复后屏幕黑屏（cycle " << cycle << "）";
    }

    view.close();
    spin(200);
#endif  // _WIN32
}

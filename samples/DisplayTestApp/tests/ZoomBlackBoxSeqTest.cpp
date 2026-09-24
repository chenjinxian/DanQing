// ZoomBlackBoxSeqTest — 用户完整操作序列的截图像素 ground truth（2026-09-14）：
//   打开 blank 视图(带 Grid) → 最大化 → 连续放大 → 每阶段存 PNG。
// 背景：band 计数器在 spacing=1（默认 DisplayStyle，线距≈1px 的 alpha wash）下
// 只对 ref 线芯（相位敏感）计数——1100 逻辑宽"消失"是测量伪象；accum 逐值
// 验证渲染公式正确（单层、quantization/权重/合成全对）。用户可见症状
// （最大化后消失/放大出现黑色大方块）需真实序列 + 截图判定。
// Authored: TEMP-DIAG 诊断工具（无对应参考测试）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>
#include <QThread>
#include <QImage>
#include <cmath>

#include <dqCommon/ViewFlags.h>

#include "View3DInventor.h"

#include <dqApp/ToolAdmin.h>
#include <dqApp/Viewport.h>
#include "Application.h"   // Gui::Application（真实 app 的 MainWindow/newDocument 路径）
#include "MainWindow.h"    // Gui::MainWindow（addWindow 重父化进 MDI）

#include <dqApp/Application.h>
#include <dqGeom/Point3d.h>
#include <cmath>

#include <cstdio>
#include <cstring>
#include <vector>
#include <windows.h>

// 保证 QApplication 存在（DtaToolBarsTest 同款模式；进程级只建一次）。
namespace { struct QtEnvQ { QtEnvQ() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvQ s_qtQ;

// App::Application 桩单例定义在 DtaToolBarsTest.cpp（同一测试目标，仅一份）。
#include <App/Application.h>

namespace {

void spin(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 屏幕上视口区域 vs FBO 的逐像素对照：相同=呈现链无辜（黑区是误判/桌面），
// 不同=present 路径真丢像素。
void diffScreenVsFbo(dqApp::Viewport* vp, char const* tag)
{
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    if (!vp->ReadFrameForTest(frame, w, h))
        return;
    // grabWindow 返回物理像素；mapToGlobal 是逻辑坐标——DPR 下必须 ×dpr 对齐
    //（此前逻辑坐标直接裁物理图 = 2× 偏移，diff 全是伪差）。
    double const dpr = vp->devicePixelRatioF();
    printf("[SEQ] %s sizeCheck: widget=%dx%d logical (x%.2f = %dx%d physical), FBO=%ux%u\n",
           tag, vp->width(), vp->height(), dpr,
           static_cast<int>(vp->width() * dpr), static_cast<int>(vp->height() * dpr), w, h);
    // 遮挡守卫：先前台化再抓屏（测试进程自己的终端窗口会盖在 app 上——
    // "屏幕黑块/100% diff"曾是遮挡伪象）。同时记录 GL 子窗口的裁剪区域/样式。
    {
        QWidget* topLevel = vp->window();
        topLevel->raise();
        topLevel->activateWindow();
        spin(300);
        HWND hwnd = reinterpret_cast<HWND>(vp->winId());
        HRGN rgn = CreateRectRgn(0, 0, 0, 0);
        int const rgnType = GetWindowRgn(hwnd, rgn);
        RECT wr{};
        GetWindowRect(hwnd, &wr);
        LONG_PTR const exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        LONG_PTR const style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        printf("[SEQ] %s hwndDiag: hwndNow=%p hwndInit=%p %s rgnType=%d winRect=(%ld,%ld %ldx%ld) "
               "exStyle=0x%llx WS_VISIBLE=%d WS_EX_LAYERED=%d\n",
               tag, static_cast<void*>(hwnd), vp->m_dbgInitHwnd,
               (static_cast<void*>(hwnd) == vp->m_dbgInitHwnd) ? "SAME" : "***RECREATED***",
               rgnType, static_cast<long>(wr.left), static_cast<long>(wr.top),
               static_cast<long>(wr.right - wr.left), static_cast<long>(wr.bottom - wr.top),
               static_cast<unsigned long long>(exStyle), (style & WS_VISIBLE) ? 1 : 0,
               (exStyle & WS_EX_LAYERED) ? 1 : 0);
        DeleteObject(rgn);
    }
    QPoint const glLogical = vp->mapToGlobal(QPoint(0, 0));
    QPoint const gl(static_cast<int>(glLogical.x() * dpr),
                    static_cast<int>(glLogical.y() * dpr));
    QPixmap const pm = QGuiApplication::primaryScreen()->grabWindow(0);
    QImage const screen = pm.toImage().convertToFormat(QImage::Format_RGB888);
    int const vw = static_cast<int>(w), vh = static_cast<int>(h);
    if (gl.x() < 0 || gl.y() < 0 || gl.x() + vw > screen.width() || gl.y() + vh > screen.height()) {
        printf("[SEQ] %s diff: viewport global rect (%d,%d %dx%d) outside screen %dx%d\n",
               tag, gl.x(), gl.y(), vw, vh, screen.width(), screen.height());
        return;
    }
    QImage const crop = screen.copy(gl.x(), gl.y(), vw, vh);
    char path[160];
    std::snprintf(path, sizeof(path), "D:\\Github\\DanQing\\build\\seqscr-%s.png", tag);
    crop.save(path, "PNG");

    // FBO 底朝上：行翻转后对照。差异>40 的像素计数 + 最大差 + 分块统计（8x8）。
    int bad = 0, worst = 0;
    int worstBlockX = 0, worstBlockY = 0;
    int blockBad[8][8] = {};
    for (int y = 0; y < vh; ++y)
        for (int x = 0; x < vw; ++x) {
            uint8_t const* f = &frame[(static_cast<size_t>(vh - 1 - y) * vw + x) * 4];
            uint8_t const* s = crop.scanLine(y) + x * 3;
            int const d = std::abs(int(f[0]) - int(s[0]))
                        + std::abs(int(f[1]) - int(s[1]))
                        + std::abs(int(f[2]) - int(s[2]));
            if (d > 40) {
                ++bad;
                worst = std::max(worst, d);
                ++blockBad[y * 8 / vh][x * 8 / vw];
                if (blockBad[y * 8 / vh][x * 8 / vw] > blockBad[worstBlockY][worstBlockX]) {
                    worstBlockY = y * 8 / vh;
                    worstBlockX = x * 8 / vw;
                }
            }
        }
    printf("[SEQ] %s diff: bad=%d/%d (%.1f%%) worst=%d worstBlock=(%d,%d) counts=[",
           tag, bad, vw * vh, 100.0 * bad / (vw * vh), worst, worstBlockX, worstBlockY);
    for (int by = 0; by < 8; ++by) {
        printf("%s", by ? ";" : "");
        for (int bx = 0; bx < 8; ++bx)
            printf("%s%d", bx ? "," : "", blockBad[by][bx] * 100 / (vw * vh / 64 + 1));
    }
    printf("]\n");

    // TEMP-DIAG（仅坏态，bad > 10%）：分层判定——(a) 强制再 present×10 是否解冻；
    // (b) GDI StretchDIBits 直拷 FBO 到子窗口是否上屏。
    if (bad * 100 > vw * vh / 10 && std::getenv("DANQING_BLACKBOX_PROBE")) {
        auto screenGrab = [&]() {
            QPixmap const p2 = QGuiApplication::primaryScreen()->grabWindow(0);
            return p2.toImage().convertToFormat(QImage::Format_RGB888)
                       .copy(gl.x(), gl.y(), vw, vh);
        };
        auto diffCount = [&](QImage const& a, QImage const& b) {
            int n = 0;
            for (int y = 0; y < vh; y += 4)
                for (int x = 0; x < vw; x += 4) {
                    uint8_t const* pa = a.scanLine(y) + x * 3;
                    uint8_t const* pb = b.scanLine(y) + x * 3;
                    if (std::abs(int(pa[0]) - int(pb[0])) + std::abs(int(pa[1]) - int(pb[1]))
                        + std::abs(int(pa[2]) - int(pb[2])) > 40)
                        ++n;
                }
            return n;
        };
        QImage const before = screenGrab();
        // (a) 再 present 10 次
        for (int i = 0; i < 10; ++i) {
            vp->RequestRedraw();
            vp->RenderFrame();
            spin(50);
        }
        QImage const afterPresent = screenGrab();
        printf("[SEQ] %s probe: presentx10 changed=%d (0=仍冻结) \n",
               tag, diffCount(before, afterPresent));

        // (b0..b3) 唤醒探测：依次试各操作，谁让屏幕变化谁就是修复机制。
        HWND hwnd = reinterpret_cast<HWND>(vp->winId());
        auto presentOnce = [&]() {
            vp->RequestRedraw();
            vp->RenderFrame();
            spin(120);
        };
        QImage prev = afterPresent;

        // b0: 子窗口 ±2px（驱动重建交换面）
        vp->resize(vp->width() + 2, vp->height());
        spin(150);
        presentOnce();
        {
            QImage const g = screenGrab();
            printf("[SEQ] %s heal: resize+2px changed=%d\n", tag, diffCount(prev, g));
            prev = g;
        }
        vp->resize(vp->width() - 2, vp->height());
        spin(150);
        presentOnce();
        {
            QImage const g = screenGrab();
            printf("[SEQ] %s heal: resize-2px changed=%d\n", tag, diffCount(prev, g));
            prev = g;
        }

        // b1: 强制 WM_PAINT 校验
        UpdateWindow(hwnd);
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
        presentOnce();
        {
            QImage const g = screenGrab();
            printf("[SEQ] %s heal: UpdateWindow/RedrawWindow changed=%d\n", tag, diffCount(prev, g));
            prev = g;
        }

        // b2: Qt update 周期
        vp->update();
            spin(150);
        presentOnce();
        {
            QImage const g = screenGrab();
            printf("[SEQ] %s heal: qt-update changed=%d\n", tag, diffCount(prev, g));
            prev = g;
        }

        // b3: 顶层二次状态跃迁 normal→maximized
        QWidget* top = vp->window();
        top->showNormal();
        spin(250);
        presentOnce();
        top->showMaximized();
        spin(250);
        presentOnce();
        {
            QImage const g = screenGrab();
            printf("[SEQ] %s heal: normal->max dance changed=%d\n", tag, diffCount(prev, g));
            prev = g;
        }
    }
}

void shotFbo(dqApp::Viewport* vp, char const* tag)
{
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    if (!vp->ReadFrameForTest(frame, w, h)) {
        printf("[SEQ] %s FBO read FAIL\n", tag);
        return;
    }
    // GL 底朝上 → 翻转为顶朝上后存 PNG。
    QImage img(w, h, QImage::Format_RGB888);
    for (uint32_t y = 0; y < h; ++y)
        std::memcpy(img.scanLine(h - 1 - y), &frame[y * w * 4], w * 3);
    char path[160];
    std::snprintf(path, sizeof(path), "D:\\Github\\DanQing\\build\\seqfbo-%s.png", tag);
    img.save(path, "PNG");
    int black = 0, dark = 0;
    for (uint32_t y = 0; y < h; y += 4)
        for (uint32_t x = 0; x < w; x += 4) {
            uint8_t const* p = &frame[(y * w + x) * 4];
            if (p[0] < 40 && p[1] < 40 && p[2] < 40) ++black;
            if (p[0] < 80 && p[1] < 80 && p[2] < 90) ++dark;
        }
    printf("[SEQ] FBO-%s %ux%u black=%d dark=%d\n", tag, w, h, black, dark);
}

void shot(char const* tag)
{
    QPixmap const pm = QGuiApplication::primaryScreen()->grabWindow(0);
    char path[160];
    std::snprintf(path, sizeof(path), "D:\\Github\\DanQing\\build\\seq-%s.png", tag);
    pm.save(path, "PNG");
    QImage const img = pm.toImage().convertToFormat(QImage::Format_RGB888);
    int black = 0, dark = 0;
    for (int y = 0; y < img.height(); y += 2)
        for (int x = 0; x < img.width(); x += 2) {
            uint8_t const* p = img.scanLine(y) + x * 3;
            if (p[0] < 40 && p[1] < 40 && p[2] < 40) ++black;
            if (p[0] < 80 && p[1] < 80 && p[2] < 90) ++dark;
        }
    printf("[SEQ] %s %dx%d black=%d dark=%d\n", tag, img.width(), img.height(), black, dark);
}

}  // namespace

TEST(ZoomBlackBoxSeq, MaximizeThenDeepZoomScreenshots)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1200, 800);
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
    vp->RenderFrame();
    shot("A-open1200x800");
    shotFbo(vp, "A-open1200x800");
    diffScreenVsFbo(vp, "A-open1200x800");

    view.showMaximized();
    spin(1200);
    vp->RequestRedraw();
    vp->RenderFrame();
    shot("B-maximized");
    shotFbo(vp, "B-maximized");
    diffScreenVsFbo(vp, "B-maximized");

    auto zoomIn = [&](int steps, char const* tagStart) {
        for (int i = 0; i < steps; ++i) {
            dqApp::ToolEvent te;
            te.type = dqApp::ToolEventType::Wheel;
            te.vp = vp;
            te.posx = 600.0f;
            te.posy = 400.0f;
            te.wheelDeltaY = 120.0f;
            dqApp::ToolAdmin::addEvent(te);
            spin(20);
        }
        spin(900);  // 动画收敛
        vp->RenderFrame();
        char tag[32];
        std::snprintf(tag, sizeof(tag), "%s-%d", tagStart, steps);
        shot(tag);
    };

    zoomIn(10, "C-zoom");
    zoomIn(40, "D-zoom");
    zoomIn(120, "E-zoom");

    view.close();
    spin(200);
}

// 真实 app 路径：Gui::MainWindow + Application::newDocument（工厂 + addWindow
// 重父化进 MDI 区——裸 View3DInventor 测试覆盖不到的 HWND/交换链路径）+
// 经 WheelEventProcessor 的真实缩放（ToolAdmin 队列在测试桩下不派发）。
TEST(ZoomBlackBoxSeq, RealMainWindowMdiPath)
{
    // MainWindow 构造读 App::Application 参数组（DtaToolBarsTest 同款桩初始化）。
    if (!App::Application::_pcSingleton)
        App::Application::_pcSingleton = new App::Application();

    Gui::MainWindow mw;
    mw.resize(1200, 800);
    mw.show();
    spin(600);
    Gui::Application::Instance()->setMainWindow(&mw);

    auto* view = new Gui::View3DInventor(nullptr, nullptr, nullptr);
    mw.addWindow(view);
    spin(700);

    auto* vp = view->findChild<dqApp::Viewport*>();
    ASSERT_NE(vp, nullptr);
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = true;
        p.acsTriad = true;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spin(500);
    vp->RenderFrame();
    shot("R-open");
    shotFbo(vp, "R-open");
    diffScreenVsFbo(vp, "R-open");

    mw.showMaximized();
    spin(1200);
    vp->RequestRedraw();
    vp->RenderFrame();
    shot("R-max");
    shotFbo(vp, "R-max");
    diffScreenVsFbo(vp, "R-max");

    // 真实缩放链（WheelEventProcessor 直驱；位置漂移模拟手部移动）。
    dqApp::BeWheelEvent ev;
    ev.viewport = vp;
    ev.wheelDelta = 120.0;
    char tag[32];
    for (int step = 0; step < 150; ++step) {
        // 缩放点钉在屏幕中心（=初始 fit 视图的原点/ACS triad 处）。BeWheelEvent.rawPoint
        // 语义是世界点（参考 ToolAdmin rawPoint=unsnap 光标世界位置）——此前直接喂
        // 屏幕坐标是误用（每步向错误世界点锚定 → 视图漂移、triad 被推出视野）。
        // 此处按 InputState::fromPoint 同式构造：viewPoint=(中心x,中心y,NpcToView中心z)
        // → ViewToWorld。
        dqGeom::Point3d const centerView = vp->NpcToView(dqGeom::Point3d::From(0.5, 0.5, 0.5));
        ev.rawPoint = vp->ViewToWorld(dqGeom::Point3d::From(
            vp->width() / 2.0, vp->height() / 2.0, centerView.z));
        dqApp::WheelEventProcessor::process(ev, false);
        spin(25);
        if (step == 9 || step == 39 || step == 119) {
            spin(800);
            vp->RenderFrame();
            std::snprintf(tag, sizeof(tag), "R-zoom-%d", step + 1);
            shot(tag);
            shotFbo(vp, tag);
            diffScreenVsFbo(vp, tag);
        }
    }
    spin(900);
    vp->RenderFrame();
    shot("R-zoom-settled");
    shotFbo(vp, "R-zoom-settled");
    diffScreenVsFbo(vp, "R-zoom-settled");


    mw.close();
    spin(300);
}

// ---------------------------------------------------------------------------
// MDI 路径的 ACS Z 轴圆盘随缩放存活（2026-09-14 用户报告：匀速放大 ~20 次圆盘
// 填充消失、缩小恢复、resize 不恢复）。
// Authored: no reference test exists —— 参考（浏览器）无 MDI 结构；DTA 空白连接
// 不画 ACS triad（CDP 实测 2026-09-14：acs viewflag 开、世界原点居中、无 triad
// 像素），无参考可见行为可对齐。判据=盘填充 b-g>60（四轮校准史见 fboBlues 注释：
// 天空/网格背景两次污染判据的教训是**像素判据必须先打印剖面标定**）。FBO+屏幕双测。
// 修复（Geometry::getPolyfaces/getStrokes 容差随 placement 缩放换算）后单跑全绿；
// 全量序列中 SEH 0xc0000005 于 MainWindow 构造（前序测试泄漏视口连锁）——TD-12
// 跨测试污染家族（CLAUDE.md §14），非本测试缺陷。
// ---------------------------------------------------------------------------
TEST(ZoomBlackBoxSeq, MdiAcsDiscSurvivesZoomPixels)
{
    if (!App::Application::_pcSingleton)
        App::Application::_pcSingleton = new App::Application();

    Gui::MainWindow mw;
    mw.resize(1284, 725);  // 真实 app 几何：视口 1280×677 逻辑（+MDI chrome 边）
    mw.show();
    // 2026-09-14 夜：用户消失场景均为【最大化】MDI 窗（非最大化格子此前未测）。
    mw.showMaximized();
    spin(600);
    Gui::Application::Instance()->setMainWindow(&mw);

    auto* view = new Gui::View3DInventor(nullptr, nullptr, nullptr);
    mw.addWindow(view);
    spin(700);

    auto* vp = view->findChild<dqApp::Viewport*>();
    ASSERT_NE(vp, nullptr);
    auto* v3 = vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr;
    ASSERT_NE(v3, nullptr);
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = true;
        p.acsTriad = true;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spin(500);
    vp->RenderFrame();
    spin(300);

    // 圆盘判据修正四（2026-09-14 夜，b-g 判别子）：前三轮判据均被背景污染——
    // 天空 (142,205,255) b-g=50 恰过 b>g+40；网格背景 (120,175,220) g<185 也过；
    // 自校准 g-delta 的基线随网格密度漂移（margin<10 误判）。像素剖面标定：
    // 盘填充=(78-100,112-144,195-235)（蓝 alpha0.216 叠背景）→ b-g≈83-124；
    // 盘死后的中心亮区 (128,184,229) b-g=45；背景 b-g=35-50。判别子
    // 【b-g>60 && b>r+30】：活着计满、死亡归零、背景/网格全部排除。
    // 采样半径 4-14 物理 px（盘半径 ~21；避开中心 Z 点 <4 与箭头杆，角度偏 7.5°）。
    static int s_discCtrlStep = -1;
    auto fboBlues = [&](int& blues) {
        std::vector<uint8_t> px;
        uint32_t w = 0, h = 0;
        blues = -1;
        if (!vp->ReadFrameForTest(px, w, h) || px.empty()) return;
        auto sample = [&](uint32_t dy, int a, int* r, int* g, int* b) -> bool {
            double const ang = (a + 0.5) * 3.14159265 / 4;
            int const x = static_cast<int>(w / 2 + dy * std::cos(ang));
            int const y = static_cast<int>(h / 2 + dy * std::sin(ang));
            if (x < 0 || y < 0 || x >= static_cast<int>(w) || y >= static_cast<int>(h)) return false;
            size_t const idx = (static_cast<size_t>(y) * w + x) * 4;
            *r = px[idx]; *g = px[idx + 1]; *b = px[idx + 2];
            return true;
        };
        blues = 0;
        for (uint32_t dy = 4; dy <= 14; dy += 2)
            for (int a = 0; a < 8; ++a) {
                int r = 0, g = 0, b = 0;
                if (sample(dy, a, &r, &g, &b) && b - g > 60 && b > r + 30) ++blues;
            }
        if (s_discCtrlStep >= 0) {
            printf("[MDIDISC-PIX]");
            for (uint32_t dy : {4u, 8u, 12u, 20u, 28u, 36u}) {
                int const x = static_cast<int>(w / 2 + dy);
                if (x >= static_cast<int>(w)) continue;
                size_t const idx = (static_cast<size_t>(h / 2) * w + x) * 4;
                printf(" r%u:(%d,%d,%d)", dy, px[idx], px[idx + 1], px[idx + 2]);
            }
            printf(" | ring6:");
            for (int a = 0; a < 8; ++a) {
                double const ang = (a + 0.5) * 3.14159265 / 4;
                int const x = static_cast<int>(w / 2 + 6 * std::cos(ang));
                int const y = static_cast<int>(h / 2 + 6 * std::sin(ang));
                size_t const idx = (static_cast<size_t>(y) * w + x) * 4;
                printf(" (%d,%d,%d)", px[idx], px[idx + 1], px[idx + 2]);
            }
            printf("\n");
        }
    };
    auto screenBlues = [&]() {
        double const dpr = vp->devicePixelRatioF();
        QPoint const g = vp->mapToGlobal(QPoint(0, 0));
        int const px = static_cast<int>(g.x() * dpr), py = static_cast<int>(g.y() * dpr);
        int const pw = static_cast<int>(vp->width() * dpr), ph = static_cast<int>(vp->height() * dpr);
        HDC screenDC = GetDC(nullptr);
        HDC memDC = CreateCompatibleDC(screenDC);
        HBITMAP bmp = CreateCompatibleBitmap(screenDC, pw, ph);
        HGDIOBJ old = SelectObject(memDC, bmp);
        BitBlt(memDC, 0, 0, pw, ph, screenDC, px, py, SRCCOPY | CAPTUREBLT);
        int blues = 0;
        for (uint32_t dy = 4; dy <= 14; dy += 2)
            for (int a = 0; a < 8; ++a) {
                double const ang = (a + 0.5) * 3.14159265 / 4;
                int const x = pw / 2 + static_cast<int>(dy * std::cos(ang));
                int const y = ph / 2 + static_cast<int>(dy * std::sin(ang));
                if (x < 0 || y < 0 || x >= pw || y >= ph) continue;
                COLORREF c = GetPixel(memDC, x, ph - 1 - y);
                if (GetBValue(c) - GetGValue(c) > 60 && GetBValue(c) > GetRValue(c) + 30) ++blues;
            }
        SelectObject(memDC, old);
        DeleteObject(bmp);
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return blues;
    };

    dqApp::BeWheelEvent ev;
    ev.viewport = vp;
    ev.wheelDelta = 120.0;
    int firstLost = -1;          // zoom-in（放大）方向
    // t 系列实测消失点 ext≈0.006（zoom-out 反推）——2000·(2/3)^N=0.006 → N≈41
    // 格；用户记录 ext=0.103（N≈23.5）。滚到 46 步覆盖到 extentLimits.min=0.001
    // clamp 底。
    for (int i = 0; i <= 46; ++i) {
        if (i > 0) {
            dqGeom::Point3d const centerView = vp->NpcToView(dqGeom::Point3d::From(0.5, 0.5, 0.5));
            ev.rawPoint = vp->ViewToWorld(dqGeom::Point3d::From(
                vp->width() / 2.0, vp->height() / 2.0, centerView.z));
            dqApp::WheelEventProcessor::process(ev, false);
            spin(200);  // 匀速打断式（用户形态）
        }
        vp->RenderFrame();
        s_discCtrlStep = (i == 0 || i == 8 || i == 23 || i == 38 || i == 46) ? i : -1;
        int fb = -1;
        fboBlues(fb);
        int const sb = screenBlues();
        printf("[MDIDISC] in %2d ext=(%.6g,%.6g) fbo=%d screen=%d\n",
               i, v3->GetExtents().x, v3->GetExtents().y, fb, sb);
        if (i > 0 && fb == 0 && firstLost < 0) firstLost = i;
    }
    // zoom-out 方向（wheelDelta<0，ext ×1.5）——真实 app 的 t 系列复现方向。
    ev.wheelDelta = -120.0;
    for (int i = 0; i < 35; ++i) {
        dqGeom::Point3d const centerView = vp->NpcToView(dqGeom::Point3d::From(0.5, 0.5, 0.5));
        ev.rawPoint = vp->ViewToWorld(dqGeom::Point3d::From(
            vp->width() / 2.0, vp->height() / 2.0, centerView.z));
        dqApp::WheelEventProcessor::process(ev, false);
        spin(200);
        vp->RenderFrame();
        s_discCtrlStep = (i == 0 || i == 12 || i == 25 || i == 34) ? 1000 + i : -1;
        int fb = -1;
        fboBlues(fb);
        int const sb = screenBlues();
        printf("[MDIDISC] out %2d ext=(%.6g,%.6g) fbo=%d screen=%d\n",
               i, v3->GetExtents().x, v3->GetExtents().y, fb, sb);
        if (fb == 0 && firstLost < 0) firstLost = 1000 + i;
    }

    view->close();
    spin(200);
    mw.close();
    spin(300);
    EXPECT_EQ(firstLost, -1) << "ACS disc lost in MDI path at step " << firstLost;
}

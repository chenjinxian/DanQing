// DecoGeometryPixelTest — Decoration Geometry Example 按钮链路回归：17 个
// 装饰几何上屏（readPixels 断言），四行纹理组合可辨（颜色行差）。
// Authored: no reference test exists（渲染输出级回归；行为锚定 DTA
// DecorationGeometryExample.ts:24-42/70-131 布局与轮换）。
// 复现配方：Views 工具栏 "Deco" 按钮 == openDecorationGeometryExample()。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QMouseEvent>
#include <QWheelEvent>

#include "View3DInventor.h"
#include "DecorationGeometryExample.h"

#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewTool.h>
#include <dqApp/Viewport.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace { struct QtEnvDG { QtEnvDG() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvDG s_qtDG;

namespace {
void spinDG(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}
}  // namespace

TEST(DecoGeometryPixel, GridRendersAndRowsDiffer)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinDG(400);

    Gui::openDecorationGeometryExample(view);
    spinDG(2500);
    view.getUeViewport()->RenderFrame();

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));

    // 采样带：几何网格 x∈[0,10] y∈[-3,7]（z 0..1），Iso 相机 fit 后占屏中央。
    // 中央盒（x∈[25%,75%]、y∈[15%,85%]）分**上下两半**采样——参考布局 y=6/3
    // 与 y=0/-3 两行纹理组合不同（无纹理 vs 双纹理），行带均值应有可辨差异；
    // 全体均值须非天空色（几何上屏）。
    double topSum = 0, botSum = 0, allSum = 0;
    size_t topCnt = 0, botCnt = 0, allCnt = 0;
    for (uint32_t y = h * 15 / 100; y < h * 85 / 100; y += 2) {
        for (uint32_t x = w * 25 / 100; x < w * 75 / 100; x += 2) {
            size_t px = (static_cast<size_t>(y) * w + x) * 4;
            double const lum = 0.299 * frame[px + 0] + 0.587 * frame[px + 1] + 0.114 * frame[px + 2];
            allSum += lum; ++allCnt;
            if (y < h / 2) { topSum += lum; ++topCnt; }
            else           { botSum += lum; ++botCnt; }
        }
    }
    ASSERT_GT(allCnt, 0u);
    double const all = allSum / allCnt;
    double const top = topCnt ? topSum / topCnt : 0.0;
    double const bot = botCnt ? botSum / botCnt : 0.0;
    printf("[DECOGEO] all=%.2f top=%.2f bottom=%.2f\n", all, top, bot);

    // 判据 1（内容存活）：中央盒内几何暗像素（蓝/红/绿/砖的亮度远低于双色天空
    // ~230+）占比 > 2%。（天空颜色修复后背景转亮——旧判据"均值低于暗蓝天"失效，
    // 改为直接数几何像素。校准：砖纹/蓝球 lum<120，天空 lum≈235。）
    size_t darkCnt = 0;
    for (uint32_t y = h * 15 / 100; y < h * 85 / 100; y += 2) {
        for (uint32_t x = w * 25 / 100; x < w * 75 / 100; x += 2) {
            size_t px = (static_cast<size_t>(y) * w + x) * 4;
            double const lum = 0.299 * frame[px + 0] + 0.587 * frame[px + 1] + 0.114 * frame[px + 2];
            if (lum < 120.0) ++darkCnt;
        }
    }
    double const darkFrac = allCnt ? static_cast<double>(darkCnt) / allCnt : 0.0;
    printf("[DECOGEO] all=%.2f top=%.2f bottom=%.2f darkFrac=%.3f\n", all, top, bot, darkFrac);
    EXPECT_GT(darkFrac, 0.02)
        << "central band has no dark geometry pixels (sky only?)";
    (void)all;

    // 判据 2（WHERE）：上下半带差异——行组纹理/颜色组合不同的最小信号。
    // 校准（2026-09-17 [DECOGEO] 探针实测）：top=187.73 bottom=159.78
    // diff=27.95；阈值取 4（>6 倍余量——行组合全同时差 → 0）。
    EXPECT_GT(std::abs(top - bot), 4.0)
        << "top=" << top << " bottom=" << bot;

    // view.close() 收尾——GeometryDecorator 的生命周期由 View3DInventor 持有
    // （与 GltfDecoration 同模式：closeEvent 在 Shutdown 前 Drop+dispose，
    // View3DInventor.cpp:149-159——装饰的 GL 资源在驱动存活时释放）。
    view.close();
    spinDG(200);
}

// ---------------------------------------------------------------------------
// 纹理存在性 + 拾取粒度 + 逐条目 flash 回归
// Authored: no reference test exists in itwinjs-core for the DTA example's
// rendered output/pick granularity（渲染输出级回归；行为锚定
// DecorationGeometryExample.ts:87 pickable 每装饰独立 id + :97-99 四行纹理组合）。
// ---------------------------------------------------------------------------
namespace {

// 9×9 窗口亮度标准差（frame 为 device px RGBA8）。
double lumStdev(std::vector<uint8_t> const& f, uint32_t w, uint32_t h, uint32_t cx, uint32_t cy)
{
    double sum = 0, sum2 = 0;
    size_t n = 0;
    for (int dy = -4; dy <= 4; ++dy) {
        for (int dx = -4; dx <= 4; ++dx) {
            uint32_t const x = cx + dx, y = cy + dy;
            if (x >= w || y >= h) continue;
            size_t const px = (static_cast<size_t>(y) * w + x) * 4;
            double const lum = 0.299 * f[px + 0] + 0.587 * f[px + 1] + 0.114 * f[px + 2];
            sum += lum; sum2 += lum * lum; ++n;
        }
    }
    if (n == 0) return 0.0;
    double const mean = sum / n;
    return std::sqrt(std::max(0.0, sum2 / n - mean * mean));
}

double lumMean(std::vector<uint8_t> const& f, uint32_t w, uint32_t h, uint32_t cx, uint32_t cy)
{
    double sum = 0; size_t n = 0;
    for (int dy = -4; dy <= 4; ++dy)
        for (int dx = -4; dx <= 4; ++dx) {
            uint32_t const x = cx + dx, y = cy + dy;
            if (x >= w || y >= h) continue;
            size_t const px = (static_cast<size_t>(y) * w + x) * 4;
            sum += 0.299 * f[px + 0] + 0.587 * f[px + 1] + 0.114 * f[px + 2];
            ++n;
        }
    return n ? sum / n : 0.0;
}

}  // namespace

TEST(DecoGeometryPixel, TexturesPicksAndFlashPerEntry)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinDG(400);

    Gui::openDecorationGeometryExample(view);
    spinDG(2500);
    view.getUeViewport()->RenderFrame();

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));

    auto* vp = view.getUeViewport();
    int const vw = vp->width();
    int const vh = vp->height();
    double const dpr = static_cast<double>(w) / vw;

    // ---- 拾取粒度：28×20 网格扫描（PickAtPoint 走 readPickData 真通道）----
    // 参考：每条 decoration 独立 pickable id（:87）+ multi-feature 行 4 个
    // （:162-185）共 20 个。粗网格覆盖不全（细锥体可漏）——阈值取 12。
    std::map<uint32_t, std::vector<std::pair<int, int>>> hitsById;
    for (int gy = 0; gy < 20; ++gy) {
        for (int gx = 0; gx < 28; ++gx) {
            int const px = (gx + 1) * vw / 29;
            int const py = (gy + 1) * vh / 21;
            uint32_t const id = vp->PickAtPoint(px, py);
            if (id != 0)
                hitsById[id].emplace_back(px, py);
        }
    }
    printf("[DECOGEO2] distinct ids=%zu\n", hitsById.size());
    EXPECT_GE(hitsById.size(), 12u)
        << "per-entry pickable ids collapsed (single-feature batch?)";

    // 每个 id 的质心（CSS → device px）
    struct IdProbe { uint32_t id; uint32_t dx; uint32_t dy; double stdev; };
    std::vector<IdProbe> probes;
    for (auto const& kv : hitsById) {
        double sx = 0, sy = 0;
        for (auto const& p : kv.second) { sx += p.first; sy += p.second; }
        uint32_t const dx = static_cast<uint32_t>(sx / kv.second.size() * dpr);
        uint32_t const dy = static_cast<uint32_t>(sy / kv.second.size() * dpr);
        double const sd = lumStdev(frame, w, h, dx, h - 1 - dy);  // FBO bottom-up
        probes.push_back({ kv.first, dx, dy, sd });
    }
    // ---- 纹理存在性：砖纹（行 2/3 共 8 条目）局部方差高；行 0/multi 平坦 ----
    size_t textured = 0, flat = 0;
    for (auto const& p : probes) {
        printf("[DECOGEO2] id=%u dev=(%u,%u) stdev=%.2f\n", p.id, p.dx, p.dy, p.stdev);
        if (p.stdev > 6.0) ++textured;       // 砖缝/法线浮雕
        else if (p.stdev < 2.5) ++flat;      // 平色（行 0 与 multi-feature 行）
    }
    EXPECT_GE(textured, 4u) << "no brick/normal-map texture variation detected";
    EXPECT_GE(flat, 4u) << "flat-color row not flat (texture leaked everywhere?)";

    // ---- 逐条目 flash：悬停单个几何只亮它自己（对照区域不动） ----
    // 选 stdev 最大的（砖纹块，区域大、信号稳）做悬停目标；对照选另一个远端 id。
    if (probes.size() >= 2) {
        auto bySd = probes;
        std::sort(bySd.begin(), bySd.end(), [](IdProbe const& a, IdProbe const& b) { return a.stdev > b.stdev; });
        IdProbe const& target = bySd.front();
        IdProbe const& control = bySd.back();

        double const t0 = lumMean(frame, w, h, target.dx, h - 1 - target.dy);
        double const c0 = lumMean(frame, w, h, control.dx, h - 1 - control.dy);

        // 悬停注入（CursorPositionTest 同款）：flash 强度逐帧爬升——多帧后读。
        QPoint const cssPos(static_cast<int>(target.dx / dpr), static_cast<int>(target.dy / dpr));
        uint32_t const directPick = vp->PickAtPoint(cssPos.x(), cssPos.y());
        printf("[DECOGEO2] hover css=(%d,%d) directPick=%u targetId=%u\n",
               cssPos.x(), cssPos.y(), directPick, target.id);
        QMouseEvent move(QEvent::MouseMove, cssPos, vp->mapToGlobal(cssPos),
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &move);
        spinDG(300);
        for (int i = 0; i < 8; ++i) { vp->RenderFrame(); spinDG(30); }

        std::vector<uint8_t> frame2;
        uint32_t w2 = 0, h2 = 0;
        ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame2, w2, h2));
        ASSERT_EQ(w2, w); ASSERT_EQ(h2, h);
        double const t1 = lumMean(frame2, w, h, target.dx, h - 1 - target.dy);
        double const c1 = lumMean(frame2, w, h, control.dx, h - 1 - control.dy);
        printf("[DECOGEO2] flash target %.1f→%.1f control %.1f→%.1f (id=%u)\n",
               t0, t1, c0, c1, target.id);
        EXPECT_GT(std::abs(t1 - t0), 2.0) << "hover did not flash the hit decoration";
        EXPECT_LT(std::abs(c1 - c0), 1.0) << "flash leaked to a different decoration";
    }

    view.close();
    spinDG(200);
}

// ---------------------------------------------------------------------------
// Fit + 滚轮缩放回归（2026-09-18 用户报告：Deco 示例里 Fit 后几何全丢、
// 滚轮完全不动）。
// Authored: no reference test exists（窗口行为回归；行为锚定
// Surface.ts:159-162 示例 extents + ToolAdmin.ts:2330-2368 透视滚轮缩放分支）。
// 双根因：(a) 示例没用自带 extents (-1,-1,-1,13,2,2) 的 connection（参考
// openBlankConnection 语义）→ Fit 取景到默认 1000 单位空范围；(b) 相机开启时
// doZoom 走透视分支——DanQing 该分支是桩（return No）→ 事件静默丢弃。
// ---------------------------------------------------------------------------
TEST(DecoGeometryPixel, FitKeepsGeometryVisibleAndWheelZooms)
{
    // idleTool 由 Application::Startup 创建（滚轮经 idleTool 派发——
    // WheelZoomCoalesceTest.cpp:40-48 同款补装；幂等）。
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "DecoGeometryPixel";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinDG(400);

    Gui::openDecorationGeometryExample(view);
    spinDG(2500);

    auto* vp = view.getUeViewport();
    ASSERT_NE(vp, nullptr);
    auto* view3d = vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr;
    ASSERT_NE(view3d, nullptr);

    // 相机开启的断言（透视分支前提——若示例哪天关相机，本用例路径即失效）
    ASSERT_TRUE(view3d->IsCameraOn());

    auto darkFraction = [&]() {
        std::vector<uint8_t> frame;
        uint32_t w = 0, h = 0;
        if (!vp->ReadFrameForTest(frame, w, h)) return 0.0;
        size_t dark = 0, total = 0;
        for (size_t i = 0; i + 2 < frame.size(); i += 4 * 7) {  // 抽稀采样
            double const lum = 0.299 * frame[i] + 0.587 * frame[i + 1] + 0.114 * frame[i + 2];
            if (lum < 120.0) ++dark;
            ++total;
        }
        return total ? static_cast<double>(dark) / total : 0.0;
    };

    vp->RenderFrame();
    double const beforeFit = darkFraction();
    ASSERT_GT(beforeFit, 0.02) << "baseline broken: geometry not on screen before Fit";

    // ---- Fit（工具栏 Fit 按钮同路径：View3DInventor::viewAll） ----
    view.viewAll();
    spinDG(1500);   // animateFrustumChange 1s
    vp->RenderFrame();
    double const afterFit = darkFraction();
    printf("[DECOGEO3] fit darkFrac %.3f -> %.3f, extents x %.2f\n",
           beforeFit, afterFit, view3d->GetExtents().x);
    EXPECT_GT(afterFit, 0.02)
        << "Fit lost the geometry (framed the 1000-unit blank extents instead of "
           "the example connection extents)";

    // ---- 滚轮（透视分支：眼点绕拾取目标点缩放） ----
    // 相机开时 lookAt 以镜头角重算 extents（width = 2·tan(lens/2)·|eye-target|，
    // ViewState.ts:1892-1907）——目标点是**拾取的几何点**（doZoom 的 pickNearestVisibleGeometry
    // 语义），不是旧视图焦点；extents 的绝对值因此随拾取距离跳变。可判的不变量：
    // 缩进时眼点逼近目标点、退出时远离（定点缩放的眼点运动学，ToolAdmin.ts:2349-2353）。
    double const extentBefore = view3d->GetExtents().x;
    dqGeom::Point3d const eyeBefore = view3d->getEyePoint();
    for (int i = 0; i < 5; ++i) {
        QWheelEvent we(QPointF(500, 350), vp->mapToGlobal(QPoint(500, 350)),
                       QPoint(0, 0), QPoint(0, 120 * 5),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(vp, &we);
    }
    spinDG(800);
    vp->RenderFrame();
    dqGeom::Point3d const eyeAfterIn = view3d->getEyePoint();
    dqGeom::Point3d const targetAfterIn = view3d->GetTargetPoint();   // ← 拾取的几何锚点
    // 以拾取锚点为基准量眼距（缩进应缩小：定点缩放的眼点运动学，ToolAdmin.ts:2349-2353）。
    double const distBeforeIn = dqGeom::Vector3d::FromStartEnd(targetAfterIn, eyeBefore).Magnitude();
    double const distAfterIn = dqGeom::Vector3d::FromStartEnd(targetAfterIn, eyeAfterIn).Magnitude();
    printf("[DECOGEO3] wheel-in eyeDist-to-anchor %.2f -> %.2f (extents %.2f -> %.2f)\n",
           distBeforeIn, distAfterIn, extentBefore, view3d->GetExtents().x);
    EXPECT_LT(distAfterIn, distBeforeIn * 0.85)
        << "wheel zoom-in did not move the eye toward the target (perspective doZoom dead?)";

    // 反向等量回退（对称性）
    for (int i = 0; i < 5; ++i) {
        QWheelEvent we(QPointF(500, 350), vp->mapToGlobal(QPoint(500, 350)),
                       QPoint(0, 0), QPoint(0, -120 * 5),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(vp, &we);
    }
    spinDG(800);
    vp->RenderFrame();
    dqGeom::Point3d const eyeBackOut = view3d->getEyePoint();
    dqGeom::Point3d const targetBackOut = view3d->GetTargetPoint();
    double const distAfterOut = dqGeom::Vector3d::FromStartEnd(targetBackOut, eyeBackOut).Magnitude();
    double const distBeforeOut = dqGeom::Vector3d::FromStartEnd(targetBackOut, eyeAfterIn).Magnitude();
    printf("[DECOGEO3] wheel-out eyeDist-to-anchor %.2f -> %.2f\n", distBeforeOut, distAfterOut);
    EXPECT_GT(distAfterOut, distBeforeOut * 1.2)
        << "wheel zoom-out did not move the eye away from the target";

    view.close();
    spinDG(200);
}

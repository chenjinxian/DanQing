// JoesHouseHoverTest — joeshouse hover（flash）高亮像素锁（M-J Task 2：
// hover 高亮触发时部分构件颜色丢失修复的像素回归面）。
//
// 取证（2026-09-30，本锁的先行 forensic 阶段，§11.9）：
//   真实 app（build/mj2-hover*.ps1 桌面注入）+ 测试内 harness 双通道 sweep：
//   saved 视图 / kZoom=2..256 / flash 爬升-换元-清除 burst / 真实 app 缩放中
//   hover——全部帧 diff 只见"被悬停元素增亮"（+≈0.2×255/通道，flash Brighten
//   语义），零变暗、零消失、清除后帧全等。即：报告的"部分构件颜色丢失"在
//   当前资产面上不能经像素通道复现；其机制面（drawPass 以"LUT 对象存在"选
//   Overrides 变体 + getOrCreateFeatureOverrideLUT 对无表 batch 造死 LUT）已
//   由 §11.8 参考核读定性为与参考 BatchUniforms.ts:74（anyOverridden 门）的
//   真实偏差——修复前 RED 见 FeatureOverrideLutWebGlTest 三机制锁；本文件是
//   该修复的行为面像素锁（§5(g) 渲染像素回归授权）。
//
// 判据（§11.11 位置断言制度）：
//   ①hover 语义正确性：被悬停构件的探测像素增亮（flash Brighten：u_flash_
//     intensity×0.2/通道，FlashSettings 0.25s 爬满 ≈ +51/通道上限；断言门
//     ≥15/通道）——WHERE：探测点即拾取命中点；
//   ②非 hover 构件颜色不丢失：红管/蓝管/绿地三类探测像素（基帧按色类实测
//     定位，非本构件）逐通道不变（≤2）——修复前若发生跨 batch LUT 污染此
//     处即红（机制锁已证其门此前不设防）；
//   ③清 flash 后全帧逐像素还原（≤2/通道）——排除任何驻留态污染。
//
// Authored: no reference test exists in itwinjs-core for hover-flash pixel
//           behavior on an imdl dump scene（浏览器 WebGL 无对应窗口测试；
//           行为锚定 Viewport.ts:2682-2687 target.setFlashed +
//           FeatureOverrides.ts:338-375 updateFlashed + glsl/FeatureSymbology
//           .ts doApplyFlash(:685-707, maxBrighten 0.2/Brighten 默认)——真实
//           读过的参考行号）。dump 资产只读（§11.11）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"

#include "DumpOpenHelper.h"

#include <dqApp/Application.h>
#include <dqApp/Viewport.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {

std::string const kDumpRoot = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";

void spinJHH(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

struct RgbJHH { int r, g, b; };

RgbJHH pixelAtJHH(std::vector<uint8_t> const& f, uint32_t w, uint32_t x, uint32_t y)
{
    uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
    return {p[0], p[1], p[2]};
}

int chanDeltaJHH(RgbJHH const& a, RgbJHH const& b)
{
    return std::max(std::abs(a.r - b.r), std::max(std::abs(a.g - b.g), std::abs(a.b - b.b)));
}

}  // namespace

// ---------------------------------------------------------------------------
// 锁：hover flash 只增亮被悬停构件、不动别的构件、清除后全帧还原。
// 配方：打开 joeshouse saved 视图（DumpOpenChain 同款打开链 + 泵至静默 +
// 帧稳定）→ 帧心内容像素真实拾取（PickAtPoint>0）→ SetFlashedId（AccuSnap.
// onMotion → vp.flashedId 的帧合并等价）→ flash 爬满（≥0.25s）→ 前后帧 diff。
// ---------------------------------------------------------------------------
TEST(JoesHouseHover, FlashBrightensTargetAndSparesOtherComponents)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "JoesHouseHover";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    // 真实 app 同级视口（1573×1005 CSS ×2 DPR——mj2 注入实测的窗口域）。
    view.resize(1573, 1005);
    view.show();
    spinJHH(400);

    dta::DumpOpenPackage pkg;
    pkg.imodelRoot = kDumpRoot + "/joeshouse-v1";
    pkg.tileRoots = {kDumpRoot + "/joeshouse-v1", kDumpRoot + "/joeshouse-drill-v1",
                     kDumpRoot + "/joeshouse-drill-v2"};
    auto opened = dta::openDumpIModel(view, pkg);
    ASSERT_TRUE(opened.has_value()) << "open chain failed: " << pkg.imodelRoot;

    // 泵至静默（强制选择帧形——NotFound 回退饥饿预防，DumpBrowse 同款）。
    size_t lastLogSize = 0;
    int stable = 0;
    bool quiesced = false;
    for (int i = 0; i < 200 && !quiesced; ++i) {
        view.getUeViewport()->InvalidateController();
        spinJHH(100);
        size_t const logSize = opened->fetcher->requestLog().size();
        if (!opened->fetcher->requestLog().empty()
            && logSize == lastLogSize
            && opened->fetcher->getActiveCount() == 0) {
            if (++stable >= 6)
                quiesced = true;
        } else {
            stable = 0;
        }
        lastLogSize = logSize;
    }
    ASSERT_TRUE(quiesced) << "saved view never quiesced";

    auto* vp = view.getUeViewport();
    vp->RenderFrame();
    std::vector<uint8_t> base;
    uint32_t w = 0, h = 0;
    {
        std::vector<uint8_t> prev;
        bool frameStable = false;
        for (int i = 0; i < 40 && !frameStable; ++i) {
            spinJHH(150);
            vp->RenderFrame();
            std::vector<uint8_t> cur;
            uint32_t cw = 0, ch = 0;
            ASSERT_TRUE(vp->ReadFrameForTest(cur, cw, ch));
            w = cw;
            h = ch;
            if (!prev.empty() && prev == cur)
                frameStable = true;
            prev = std::move(cur);
        }
        ASSERT_TRUE(frameStable) << "frame never stabilized";
        base = std::move(prev);
    }

    // WHERE 探测点（基帧实测定位——非硬编码坐标）：
    //   target = 帧心向外的首个内容像素（saved 等轴测的甲板板域）；
    //   红管/蓝管/绿地 = 基帧色类扫描的首个像素（joeshouse 色表构件）。
    auto isBgLike = [](uint8_t const* p) {
        return p[0] + p[1] + p[2] >= 600 || (p[2] > 200 && p[0] > 140);
    };
    uint32_t targetX = w / 2, targetY = h / 2;
    {
        bool found = false;
        for (uint32_t r = 0; r < std::min(w, h) / 2 && !found; ++r) {
            for (uint32_t a = 0; a < 64 && !found; ++a) {
                double const th = a * 3.14159265358979 / 32.0;
                long const x = long(targetX) + long(r * std::cos(th));
                long const y = long(targetY) + long(r * std::sin(th));
                if (x < 0 || y < 0 || x >= long(w) || y >= long(h))
                    continue;
                uint8_t const* p = &base[(static_cast<size_t>(y) * w + x) * 4];
                if (!isBgLike(p)) {
                    targetX = uint32_t(x);
                    targetY = uint32_t(y);
                    found = true;
                }
            }
        }
        ASSERT_TRUE(found) << "no content pixel near frame center";
    }
    auto findColorPixel = [&](bool wantRed, bool wantBlue, bool wantGreen,
                              uint32_t& ox, uint32_t& oy) -> bool {
        for (uint32_t y = 0; y < h; y += 2)
            for (uint32_t x = 0; x < w; x += 2) {
                uint8_t const* p = &base[(static_cast<size_t>(y) * w + x) * 4];
                if (wantRed && p[0] >= 120 && p[0] > p[1] + 40 && p[0] > p[2] + 40) {
                    ox = x; oy = y; return true;
                }
                if (wantBlue && p[2] >= 120 && p[2] > p[0] + 40 && p[2] > p[1] + 40) {
                    ox = x; oy = y; return true;
                }
                if (wantGreen && p[1] >= 110 && p[1] > p[0] + 30 && p[1] > p[2] + 30) {
                    ox = x; oy = y; return true;
                }
            }
        return false;
    };
    uint32_t redX = 0, redY = 0, blueX = 0, blueY = 0, greenX = 0, greenY = 0;
    ASSERT_TRUE(findColorPixel(true, false, false, redX, redY))
        << "no red pipe pixel in the saved frame";
    ASSERT_TRUE(findColorPixel(false, true, false, blueX, blueY))
        << "no blue pipe pixel in the saved frame";
    ASSERT_TRUE(findColorPixel(false, false, true, greenX, greenY))
        << "no green floor pixel in the saved frame";

    double const dpr = w / static_cast<double>(vp->width());
    uint32_t const hit = vp->PickAtPoint(int(std::lround(targetX / dpr)),
                                         int(std::lround(targetY / dpr)));
    ASSERT_GT(hit, 0u) << "probe pixel not pickable (pick view chain broken)";
    printf("[JHH] target dev=(%u,%u) hit=0x%08x; probes red=(%u,%u) blue=(%u,%u) "
           "green=(%u,%u)\n", targetX, targetY, hit, redX, redY, blueX, blueY,
           greenX, greenY);

    RgbJHH const targetBefore = pixelAtJHH(base, w, targetX, targetY);
    RgbJHH const redBefore = pixelAtJHH(base, w, redX, redY);
    RgbJHH const blueBefore = pixelAtJHH(base, w, blueX, blueY);
    RgbJHH const greenBefore = pixelAtJHH(base, w, greenX, greenY);

    // hover flash：强度爬满（FlashSettings 默认 0.25s → 1.0）。
    vp->SetFlashedId(hit);
    {
        qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
        while (QDateTime::currentMSecsSinceEpoch() - t0 < 500) {
            spinJHH(30);
            vp->RenderFrame();
        }
    }
    std::vector<uint8_t> flashed;
    uint32_t fw = 0, fh = 0;
    ASSERT_TRUE(vp->ReadFrameForTest(flashed, fw, fh));
    ASSERT_EQ(fw, w);
    ASSERT_EQ(fh, h);

    // ①hover 语义正确性：目标探测像素增亮（Brighten：rgb += intensity×0.2）。
    RgbJHH const targetFlashed = pixelAtJHH(flashed, w, targetX, targetY);
    printf("[JHH] target rgb (%d,%d,%d) -> (%d,%d,%d)\n",
           targetBefore.r, targetBefore.g, targetBefore.b,
           targetFlashed.r, targetFlashed.g, targetFlashed.b);
    EXPECT_GE(targetFlashed.r - targetBefore.r, 15) << "hover 后目标未增亮（flash Brighten）";
    EXPECT_GE(targetFlashed.g - targetBefore.g, 15);
    EXPECT_GE(targetFlashed.b - targetBefore.b, 15);

    // ②非 hover 构件颜色不丢失（WHERE：三类色构件探测像素逐通道不变）。
    RgbJHH const redFlashed = pixelAtJHH(flashed, w, redX, redY);
    RgbJHH const blueFlashed = pixelAtJHH(flashed, w, blueX, blueY);
    RgbJHH const greenFlashed = pixelAtJHH(flashed, w, greenX, greenY);
    printf("[JHH] probes delta red=%d blue=%d green=%d\n",
           chanDeltaJHH(redBefore, redFlashed), chanDeltaJHH(blueBefore, blueFlashed),
           chanDeltaJHH(greenBefore, greenFlashed));
    EXPECT_LE(chanDeltaJHH(redBefore, redFlashed), 2)
        << "红管像素被 hover 改写（跨构件 override 污染——颜色丢失类）";
    EXPECT_LE(chanDeltaJHH(blueBefore, blueFlashed), 2)
        << "蓝管像素被 hover 改写（跨构件 override 污染——颜色丢失类）";
    EXPECT_LE(chanDeltaJHH(greenBefore, greenFlashed), 2)
        << "绿地像素被 hover 改写（跨构件 override 污染——颜色丢失类）";

    // ③清 flash → 全帧逐像素还原（排除任何驻留态污染）。
    vp->SetFlashedId(0);
    {
        qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
        while (QDateTime::currentMSecsSinceEpoch() - t0 < 400) {
            spinJHH(30);
            vp->RenderFrame();
        }
    }
    std::vector<uint8_t> restored;
    uint32_t rw = 0, rh = 0;
    ASSERT_TRUE(vp->ReadFrameForTest(restored, rw, rh));
    long restoreOver = 0;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            RgbJHH const a = pixelAtJHH(base, w, x, y);
            RgbJHH const b = pixelAtJHH(restored, w, x, y);
            if (chanDeltaJHH(a, b) > 2)
                ++restoreOver;
        }
    EXPECT_EQ(restoreOver, 0l)
        << "清 flash 后帧未还原（驻留色移 " << restoreOver << " px——颜色丢失类）";
}

// PickCollapseTest — Baytown/Bridge 拾取（hover flash）与选中（hilite）激活时
// 内容塌缩回归锁（2026-10-21 用户报告取证面）。
//
// 用户症状：
//   ①Baytown：拾取与选中两种状态下大部分构件直接消失；
//   ②Bridge：拾取和选中时整个 tile 消失。
//
// 判据（§11.11 内容存活 + WHERE）：激活 flash/hilite 后帧内容像素计数
// ≥ 基线 0.5×（用户症状 = 大面积塌缩，RED 实锤后按实测重钉）；背景探针
// 不变；选中态含向 hilite 色 0x23bbfc 移动的簇（高亮确实上屏，非"靠
// 不激活换稳定"）；清选/清 flash 后内容恢复基线带。
//
// Authored: no reference test exists in itwinjs-core for hover/hilite
//           content-survival regression（§5(f)——参考的 hover/选中交互在
//           DTA 人工操作域；§5(g) 像素回归授权：复现配方 = 打开链 +
//           PickAtPoint + SetFlashedId / SelectionTool onDataButtonUp，
//           证据链 = 前后帧 readPixels 内容计数对账）。参照
//           JoesHouseHover/PickDumpScene 两锁的驱动形。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"

#include "DumpOpenHelper.h"

#include <dqApp/Application.h>
#include <dqApp/Viewport.h>
#include <dqApp/ToolAdmin.h>       // BeButtonEvent/EventHandled/Select 工厂
#include <dqApp/ViewManager.h>     // SetSelectedViewport

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {

std::string const kDumpRoot = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";

void spinPC(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

struct RgbPC { int r, g, b; };

RgbPC pixelAtPC(std::vector<uint8_t> const& f, uint32_t w, uint32_t x, uint32_t y)
{
    uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
    return {p[0], p[1], p[2]};
}

// 固定背景内容计数——DumpBrowseTest countWithBg 同款（阈值 90 先验同源）。
long countContentWithBg(std::vector<uint8_t> const& f, uint32_t w, uint32_t h,
                        RgbPC const& bg)
{
    long c = 0;
    uint8_t const* b = f.data();
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t const* p = &b[(static_cast<size_t>(y) * w + x) * 4];
            if (std::abs(p[0] - bg.r) + std::abs(p[1] - bg.g)
                    + std::abs(p[2] - bg.b) > 90)
                ++c;
        }
    return c;
}

// BMP 落盘（DumpBrowseTest dumpBmp 同款——BGRX 行序换 R/B）。
void dumpBmpPC(std::vector<uint8_t> const& frame, uint32_t w, uint32_t h,
               char const* path)
{
    FILE* f = fopen(path, "wb");
    if (!f)
        return;
    uint32_t const rowBytes = w * 4;
    unsigned char head[54] = {};
    head[0] = 'B';
    head[1] = 'M';
    *reinterpret_cast<uint32_t*>(&head[2]) = 54 + rowBytes * h;
    *reinterpret_cast<uint32_t*>(&head[10]) = 54;
    *reinterpret_cast<uint32_t*>(&head[14]) = 40;
    *reinterpret_cast<uint32_t*>(&head[18]) = w;
    *reinterpret_cast<uint32_t*>(&head[22]) = h;
    *reinterpret_cast<uint16_t*>(&head[26]) = 1;
    *reinterpret_cast<uint16_t*>(&head[28]) = 32;
    fwrite(head, 1, 54, f);
    std::vector<unsigned char> row(rowBytes);
    for (uint32_t y = 0; y < h; ++y) {
        memcpy(row.data(), &frame[static_cast<size_t>(y) * rowBytes], rowBytes);
        for (uint32_t x = 0; x < w; ++x)
            std::swap(row[x * 4 + 0], row[x * 4 + 2]);
        fwrite(row.data(), 1, rowBytes, f);
    }
    fclose(f);
}

// 共享夹具：模型打开 → 泵至静默 → 帧稳定 → 基线内容计数 + 命中像素 +
// 背景探针。model = "baytown" | "bridge"。
struct PickCollapse {
    Gui::View3DInventor view{nullptr, nullptr, nullptr};
    dqApp::Viewport* vp = nullptr;
    std::optional<dta::DumpOpenResult> opened;
    std::vector<uint8_t> base;
    uint32_t w = 0, h = 0;
    uint32_t px = 0, py = 0;    // 命中像素（设备像素）
    uint32_t bgx = 0, bgy = 0;  // 背景探针
    double dpr = 1.0;
    long baseContent = 0;
    RgbPC bg{0, 0, 0};

    bool setup(char const* model)
    {
        auto& app = dqApp::Application::Get();
        if (!app.isInitialized()) {
            dqApp::Application::Options opts;
            opts.applicationId = "PickCollapse";
            opts.applicationVersion = "1.0";
            if (!app.Startup(opts))
                return false;
        }

        view.resize(1000, 700);
        view.show();
        spinPC(400);

        dta::DumpOpenPackage pkg;
        if (0 == std::strcmp(model, "baytown")) {
            pkg.imodelRoot = kDumpRoot + "/baytown-v1";
            pkg.tileRoots = {kDumpRoot + "/baytown-v1", kDumpRoot + "/baytown-drill-v1"};
        } else {
            pkg.imodelRoot = kDumpRoot + "/bridge-edit-v1";
            pkg.tileRoots = {kDumpRoot + "/bridge-edit-v1",
                             kDumpRoot + "/bridge-edit-drill-v1",
                             kDumpRoot + "/bridge-edit-sweep-v1",
                             kDumpRoot + "/bridge-edit-mag-missing-v1",
                             kDumpRoot + "/bridge-edit-mag-missing-v2",
                             kDumpRoot + "/bridge-edit-mag-missing-v3",
                             kDumpRoot + "/bridge-edit-mag-missing-v4",
                             kDumpRoot + "/bridge-edit-coverage-v1"};
            {
                std::string const frontier =
                    kDumpRoot + "/bridge-edit-frontier-d13-v1";
                std::error_code ec;
                if (std::filesystem::exists(frontier + "/manifest.json", ec))
                    pkg.tileRoots.push_back(frontier);
            }
            pkg.frameToWorldContent = true;  // 坑 24 取景路径
        }
        opened = dta::openDumpIModel(view, pkg);
        if (!opened.has_value())
            return false;

        // 关 grid/ACS（装饰不可拾取且污染内容计数——RpcDumpRenderTest 同款
        // 视图旗标关断；内容计数与拾取探针都必须只落在模型几何上）。
        {
            auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
            auto p = style.getViewFlags().Properties();
            p.grid = false;
            p.acsTriad = false;
            style.setViewFlags(dqCommon::ViewFlags(p));
            view.getUeViewport()->InvalidateController();
        }

        // 泵至静默（强制选择帧形——NotFound 回退饥饿预防）。
        size_t lastLogSize = 0;
        int stable = 0;
        bool quiesced = false;
        for (int i = 0; i < 200 && !quiesced; ++i) {
            view.getUeViewport()->InvalidateController();
            spinPC(100);
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
        if (!quiesced)
            return false;

        vp = view.getUeViewport();
        // 帧稳定（连续两帧逐像素相等——视图动画收敛，JoesHouseHover 同款）。
        {
            std::vector<uint8_t> prev;
            bool frameStable = false;
            for (int i = 0; i < 40 && !frameStable; ++i) {
                spinPC(150);
                vp->RenderFrame();
                std::vector<uint8_t> cur;
                uint32_t cw = 0, ch = 0;
                if (!vp->ReadFrameForTest(cur, cw, ch))
                    return false;
                w = cw;
                h = ch;
                if (!prev.empty() && prev == cur)
                    frameStable = true;
                prev = std::move(cur);
            }
            if (!frameStable)
                return false;
            base = std::move(prev);
        }

        // 背景探针 = 左上角内侧（内容 bbox 判定后可换角）。
        bgx = 8;
        bgy = 8;
        {
            uint8_t const* p = &base[(static_cast<size_t>(bgy) * w + bgx) * 4];
            bg = {p[0], p[1], p[2]};
        }
        baseContent = countContentWithBg(base, w, h, bg);
        if (baseContent <= 0)
            return false;

        // 命中像素 = 内容 bbox 中心附近首个内容像素。
        {
            long long sx = 0, sy = 0;
            long n = 0;
            uint32_t minX = w, maxX = 0, minY = h, maxY = 0;
            for (uint32_t y = 0; y < h; ++y)
                for (uint32_t x = 0; x < w; ++x) {
                    uint8_t const* p =
                        &base[(static_cast<size_t>(y) * w + x) * 4];
                    if (std::abs(p[0] - bg.r) + std::abs(p[1] - bg.g)
                            + std::abs(p[2] - bg.b) > 90) {
                        ++n;
                        sx += x;
                        sy += y;
                        if (x < minX) minX = x;
                        if (x > maxX) maxX = x;
                        if (y < minY) minY = y;
                        if (y > maxY) maxY = y;
                    }
                }
            if (n <= 0)
                return false;
            uint32_t const cx = uint32_t(sx / n);
            uint32_t const cy = uint32_t(sy / n);
            bool found = false;
            for (uint32_t r = 0; r < (maxX - minX) && !found; ++r)
                for (int a = 0; a < 64 && !found; ++a) {
                    double const th = a * 3.14159265358979 / 32.0;
                    long const x = long(cx) + long(r * std::cos(th));
                    long const y = long(cy) + long(r * std::sin(th));
                    if (x < 0 || y < 0 || x >= long(w) || y >= long(h))
                        continue;
                    uint8_t const* p =
                        &base[(static_cast<size_t>(y) * w + x) * 4];
                    if (std::abs(p[0] - bg.r) + std::abs(p[1] - bg.g)
                            + std::abs(p[2] - bg.b) > 90) {
                        px = uint32_t(x);
                        py = uint32_t(y);
                        found = true;
                    }
                }
            if (!found)
                return false;
        }

        // 背景探针若落在内容域内，换右下角。
        dpr = w / static_cast<double>(vp->width());
        return true;
    }

    int cssX(uint32_t devX) const { return int(std::lround(devX / dpr)); }
    int cssY(uint32_t devY) const { return int(std::lround(devY / dpr)); }

    // 取证面：20px 网格扫描拾取命中数（pick 附件覆盖面对账——hit 内容像素
    // 应有非零 elementId；全 0 = pick 附件未写或反读断）。
    void pickSweep(char const* label) const
    {
        long tried = 0, hits = 0;
        for (uint32_t y = 4; y < h; y += 20)
            for (uint32_t x = 4; x < w; x += 20) {
                uint8_t const* p = &base[(static_cast<size_t>(y) * w + x) * 4];
                bool const content = std::abs(p[0] - bg.r) + std::abs(p[1] - bg.g)
                                     + std::abs(p[2] - bg.b) > 90;
                if (!content)
                    continue;
                ++tried;
                if (vp->PickAtPoint(cssX(x), cssY(y)) != 0)
                    ++hits;
            }
        printf("[PICKCOLLAPSE %s] pickSweep content-px tried=%ld hits=%ld\n",
               label, tried, hits);
    }

    // 背景探针必须是真实非内容像素（setup 后调用——命中域定后再选角）。
    bool ensureBackgroundProbe()
    {
        // 扫四角内侧 32px 域找一个非内容像素。
        uint32_t const corners[4][2] = {{8, 8}, {w - 8, 8}, {8, h - 8}, {w - 8, h - 8}};
        for (auto const& c : corners) {
            uint8_t const* p = &base[(static_cast<size_t>(c[1]) * w + c[0]) * 4];
            if (std::abs(p[0] - bg.r) + std::abs(p[1] - bg.g)
                    + std::abs(p[2] - bg.b) <= 90) {
                bgx = c[0];
                bgy = c[1];
                return true;
            }
        }
        return false;
    }
};

dqApp::PrimitiveTool* makeSelectionToolPC()
{
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();
    return static_cast<dqApp::PrimitiveTool*>(
        admin.GetRegistry().Create("Select"));
}

// 通用断言体：hover（flash）+ select（点选高亮）两态内容存活。
// 由 baytown/bridge 两锁共用（判据同形，钉值各自 printf 记录）。
void runHoverAndSelect(PickCollapse& s, char const* label)
{
    ASSERT_TRUE(s.ensureBackgroundProbe());
    printf("[PICKCOLLAPSE %s] base content=%ld\n", label, s.baseContent);
    dumpBmpPC(s.base, s.w, s.h,
              (std::string("D:/Github/DanQing/build/pickcollapse-") + label + "-base.bmp").c_str());
    s.pickSweep(label);

    // 探针 = 第一个真实拾取命中像素（内容像素逐个试——用户症状的触发前提
    // 是真实 element 激活 flash/hilite；pick 全帧 0 命中即 RED 断言本身）。
    uint32_t hit = 0;
    {
        bool found = false;
        for (uint32_t y = 4; y < s.h && !found; y += 8)
            for (uint32_t x = 4; x < s.w && !found; x += 8) {
                uint8_t const* p =
                    &s.base[(static_cast<size_t>(y) * s.w + x) * 4];
                bool const content =
                    std::abs(p[0] - s.bg.r) + std::abs(p[1] - s.bg.g)
                    + std::abs(p[2] - s.bg.b) > 90;
                if (!content)
                    continue;
                uint32_t const id = s.vp->PickAtPoint(s.cssX(x), s.cssY(y));
                if (id != 0) {
                    hit = id;
                    s.px = x;
                    s.py = y;
                    found = true;
                }
            }
        ASSERT_NE(hit, 0u)
            << "全帧内容像素拾取零命中——pick 附件对本模型为空（拾取链断）";
    }
    printf("[PICKCOLLAPSE %s] hit=0x%08x dev=(%u,%u)\n", label, hit, s.px, s.py);

    // --- hover（flash 爬满）---
    s.vp->SetFlashedId(hit);
    {
        qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
        while (QDateTime::currentMSecsSinceEpoch() - t0 < 500) {
            spinPC(30);
            s.vp->RenderFrame();
        }
    }
    std::vector<uint8_t> flashed;
    uint32_t fw = 0, fh = 0;
    ASSERT_TRUE(s.vp->ReadFrameForTest(flashed, fw, fh));
    ASSERT_EQ(fw, s.w);
    ASSERT_EQ(fh, s.h);
    dumpBmpPC(flashed, fw, fh,
              (std::string("D:/Github/DanQing/build/pickcollapse-") + label + "-hover.bmp").c_str());
    long const hoverContent = countContentWithBg(flashed, fw, fh, s.bg);
    printf("[PICKCOLLAPSE %s] hover content=%ld (%.1f%% of base)\n",
           label, hoverContent, 100.0 * hoverContent / s.baseContent);
    s.vp->SetFlashedId(0);
    {
        qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
        while (QDateTime::currentMSecsSinceEpoch() - t0 < 400) {
            spinPC(30);
            s.vp->RenderFrame();
        }
    }

    // --- select（点选高亮）---
    dqApp::Application::Get().GetViewManager().SetSelectedViewport(s.vp);
    auto* tool = makeSelectionToolPC();
    ASSERT_NE(tool, nullptr);
    tool->onPostInstall();
    dqApp::BeButtonEvent ev;
    ev.button = dqApp::BeButton::Data;
    ev.isDown = false;
    ev.viewport = s.vp;
    ev.viewPoint = dqGeom::Point3d::From(double(s.cssX(s.px)),
                                         double(s.cssY(s.py)), 0.0);
    ASSERT_EQ(tool->onDataButtonUp(ev), dqApp::EventHandled::Yes);
    spinPC(250);
    s.vp->RenderFrame();
    std::vector<uint8_t> selected;
    ASSERT_TRUE(s.vp->ReadFrameForTest(selected, fw, fh));
    dumpBmpPC(selected, fw, fh,
              (std::string("D:/Github/DanQing/build/pickcollapse-") + label + "-select.bmp").c_str());
    long const selContent = countContentWithBg(selected, fw, fh, s.bg);
    printf("[PICKCOLLAPSE %s] select content=%ld (%.1f%% of base)\n",
           label, selContent, 100.0 * selContent / s.baseContent);

    // 高亮确实上屏：存在向 hilite 色 0x23bbfc 移动的像素（色距严格变小）。
    RgbPC const kHilite{0x23, 0xbb, 0xfc};
    long moved = 0;
    for (uint32_t y = 0; y < fh; ++y)
        for (uint32_t x = 0; x < fw; ++x) {
            RgbPC const a = pixelAtPC(s.base, s.w, x, y);
            RgbPC const b = pixelAtPC(selected, fw, x, y);
            if (std::abs(a.r - kHilite.r) + std::abs(a.g - kHilite.g)
                    + std::abs(a.b - kHilite.b)
                > std::abs(b.r - kHilite.r) + std::abs(b.g - kHilite.g)
                      + std::abs(b.b - kHilite.b))
                ++moved;
        }
    printf("[PICKCOLLAPSE %s] hilite-moved px=%ld\n", label, moved);

    // 清选（点背景）→ 内容恢复基线带。
    dqApp::BeButtonEvent evBg;
    evBg.button = dqApp::BeButton::Data;
    evBg.isDown = false;
    evBg.viewport = s.vp;
    evBg.viewPoint = dqGeom::Point3d::From(double(s.cssX(s.bgx)),
                                           double(s.cssY(s.bgy)), 0.0);
    tool->onDataButtonUp(evBg);
    spinPC(250);
    s.vp->RenderFrame();
    std::vector<uint8_t> cleared;
    ASSERT_TRUE(s.vp->ReadFrameForTest(cleared, fw, fh));
    long const clearedContent = countContentWithBg(cleared, fw, fh, s.bg);
    printf("[PICKCOLLAPSE %s] cleared content=%ld (%.1f%% of base)\n",
           label, clearedContent, 100.0 * clearedContent / s.baseContent);

    dqApp::Application::Get().GetToolAdmin().setPrimitiveTool(nullptr);
    delete tool;

    // --- 判据（内容存活——用户症状 = 大面积塌缩，RED 面）---
    EXPECT_GE(hoverContent, s.baseContent / 2)
        << "hover（flash）激活后内容塌缩——大部分构件消失（用户症状①/②）";
    EXPECT_GE(selContent, s.baseContent / 2)
        << "选中（hilite）激活后内容塌缩——大部分构件消失（用户症状①/②）";
    EXPECT_GT(moved, 0l)
        << "选中后无向 hilite 色移动的像素——高亮未上屏（判据有效性腿）";
    EXPECT_GE(clearedContent, s.baseContent / 2)
        << "清选后内容未恢复";
}

}  // namespace

// Baytown（正交 saved 视图 + polyline/pointString 大头）。
TEST(PickCollapse, BaytownHoverAndSelectKeepContent)
{
    PickCollapse s;
    ASSERT_TRUE(s.setup("baytown")) << "baytown open chain failed";
    runHoverAndSelect(s, "baytown");
}

// Bridge（坑 24 取景 + 全树域 + compact 边 + 超大单瓦）。
TEST(PickCollapse, BridgeHoverAndSelectKeepContent)
{
    PickCollapse s;
    ASSERT_TRUE(s.setup("bridge")) << "bridge open chain failed";
    runHoverAndSelect(s, "bridge");
}

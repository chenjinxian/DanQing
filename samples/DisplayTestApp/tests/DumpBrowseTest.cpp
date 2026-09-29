// DumpBrowseTest — 双模型浏览零缺失锁（M-H Task 5 判据核心）。
//
// 浏览脚本（双模型同形）：打开链进入（DumpOpenHelper——连接 → saved 视图 →
// modelSelector 逐 model 树 → 多根 fetcher）→ saved 视图泵至静默（打开面）
// → zoom 泵 kZoom=2^n 绕视域中心至 SSE 饱和（drill 采集键全连通 = 采集侧
// 饱和点）+ 1 级过冲（饱和形证实——过冲放大子 NotFound 白名单面）→ ×0.5
// 回退 2 级 → 平移两方向（viewCenterWorld ± 视图内偏移——平移面保持在
// sweep 全树域内[×≤8 各处可达]，深放大[×16+]只在中轴支[drill 域]）。
//
// 判据 = **浏览零缺失**（全程 requestLog 对账）：
// ① 域内键零 NotFound——请求键 ∩ 联合 manifest 域（sweep ∪ drill）全部
//    Completed（零 Error = dump 资产自洽；零重复 = 无请求抖动）；
// ② 域外键白名单——仅过冲放大子（SSE 越过采集饱和顶的派生：V4 请求键形
//    逐键形验 + 清单钉死）；出现任何细分/兄弟支域外键 = 配方越界或引擎
//    发散（§11.8 处置——不烤白名单，先分析）；
// ③ Completed→graphics 计数对账 + 树侧零在途终态（零 graphics 键 = 裁决
//    白名单逐键钉死：空瓦叶根 + TD-27 unquantized 形态——见 joeshouse 锁
//    对账块的全裁决链）；
// ④ 双像素锚（§11.11 位置断言）——saved 初始帧 + 最深饱和帧（内容存活
//    + 质心中央带；首绿实测钉死）。
//
// Authored: no reference test exists in itwinjs-core/imodel-native for
//           dump-replay browse-session zero-missing reconciliation
//           （§5(f)——参考的浏览覆盖在 DTA 交互式集成，无离线回放对应物；
//           渲染像素回归授权 §5(g)：复现配方 = 打开链 + 确定性 zoom 泵/
//           回退/平移脚本；证据链 = fetcher requestLog 全程对账 +
//           readPixels 双锚）。dump 资产只读（§11.11），钉值 = 首绿实测
//           （阈值注释在各断言处）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"

#include "DumpOpenHelper.h"

#include <dqApp/Application.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqRender/tile/ImdlTileTree.h>
#include <dqRender/tile/TileAdmin.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {

std::string const kDumpRoot = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";

struct QtEnvBrowse {
    QtEnvBrowse()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvBrowse s_qtBrowse;

void spin(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 内容像素统计——DumpOpenChainTest.cpp:74-103 同源同构（判别阈值 90 先验
// 同款：背景天空渐变最大通道差和 72 < 90 < 瓦几何最小 126）。
bool contentStats(std::vector<uint8_t> const& f, uint32_t w, uint32_t h,
                  long& count, double& cx, double& cy,
                  uint32_t& minX, uint32_t& maxX, uint32_t& minY, uint32_t& maxY)
{
    uint8_t const* bg = &f[0];
    long long sx = 0, sy = 0;
    count = 0;
    minX = w; maxX = 0; minY = h; maxY = 0;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
            int const dr = std::abs(p[0] - bg[0]);
            int const dg = std::abs(p[1] - bg[1]);
            int const db = std::abs(p[2] - bg[2]);
            if (dr + dg + db > 90) {
                ++count;
                sx += x;
                sy += y;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    if (count > 0) {
        cx = static_cast<double>(sx) / count;
        cy = static_cast<double>(sy) / count;
    }
    return count > 0;
}

// 角落探针——DumpOpenChainTest.cpp:106-122 同源。
double cornerContentRatio(std::vector<uint8_t> const& f, uint32_t w, uint32_t h,
                          uint32_t x0, uint32_t y0, uint32_t box)
{
    uint8_t const* bg = &f[0];
    long total = 0, hits = 0;
    for (uint32_t y = y0; y < y0 + box && y < h; ++y)
        for (uint32_t x = x0; x < x0 + box && x < w; ++x) {
            uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
            ++total;
            int const dr = std::abs(p[0] - bg[0]);
            int const dg = std::abs(p[1] - bg[1]);
            int const db = std::abs(p[2] - bg[2]);
            if (dr + dg + db > 90)
                ++hits;
        }
    return total > 0 ? static_cast<double>(hits) / total : 0.0;
}

// 树内 graphics 就绪瓦计数——DumpOpenChainTest.cpp:125-134 同源。
void countGraphicsReady(dqRender::Tile* tile, long& ready, long& total)
{
    if (!tile)
        return;
    ++total;
    if (tile->hasGraphics())
        ++ready;
    for (dqRender::Tile* child : tile->getChildren())
        countGraphicsReady(child, ready, total);
}

void dumpBmp(std::vector<uint8_t> const& frame, uint32_t w, uint32_t h,
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
        fwrite(row.data(), 1, rowBytes, f);
    }
    fclose(f);
    printf("[BROWSE] frame dumped to %s\n", path);
}

// 泵至静默——DumpOpenChainTest.cpp:166-196 同源（M-G(2) 强制选择帧形：
// NotFound 交付不触发失效级联，强制帧把请求面推到不动点，键级幂等）。
struct PumpContext {
    Gui::View3DInventor* view;
    dta::DumpOpenResult* opened;
    long readyTiles = 0;
    long totalTiles = 0;
};

int pumpToQuiesce(PumpContext& ctx)
{
    size_t lastLogSize = 0;
    int stable = 0;
    for (int i = 0; i < 200; ++i) {
        ctx.view->getUeViewport()->InvalidateController();
        spin(100);
        ctx.readyTiles = 0;
        ctx.totalTiles = 0;
        for (auto& t : ctx.opened->trees)
            countGraphicsReady(t->getRootTile(), ctx.readyTiles, ctx.totalTiles);
        size_t const logSize = ctx.opened->fetcher->requestLog().size();
        if (!ctx.opened->fetcher->requestLog().empty()
            && logSize == lastLogSize
            && ctx.opened->fetcher->getActiveCount() == 0) {
            if (++stable >= 6)
                return i;
        } else {
            stable = 0;
        }
        lastLogSize = logSize;
    }
    return -1;
}

// 绕世界点 center 的 kZoom 缩放取景——DumpOpenChainTest.cpp:201-212 同源
//（LookAtVolume 向心缩 ≈ 参考 viewport.zoom 绕视口中心语义，旋转保持）。
void zoomViewAbout(Gui::View3DInventor& view, dqGeom::Point3d const& center,
                   dqGeom::Vector3d const& baseDiag, double kZoom)
{
    dqGeom::Range3d const zoomed = dqGeom::Range3d::CreateXYZXYZ(
        center.x - baseDiag.x / (2.0 * kZoom), center.y - baseDiag.y / (2.0 * kZoom),
        center.z - baseDiag.z / (2.0 * kZoom), center.x + baseDiag.x / (2.0 * kZoom),
        center.y + baseDiag.y / (2.0 * kZoom), center.z + baseDiag.z / (2.0 * kZoom));
    auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
    view3d->LookAtVolume(zoomed);
    view.getUeViewport()->InvalidateController();
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
}

// 世界向平移取景（LookAtVolume 平移语义——同 extents 盒心位移，旋转/缩放
// 保持；盒 diag = 当前 aspect-fixed extents ⇒ 无二次 aspect 修正——
// zoomViewAbout 泵形同源）。
void panViewTo(Gui::View3DInventor& view, dqGeom::Point3d const& newCenter)
{
    auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
    dqGeom::Vector3d const ext = view3d->GetExtents();
    dqGeom::Range3d const box = dqGeom::Range3d::CreateXYZXYZ(
        newCenter.x - ext.x / 2.0, newCenter.y - ext.y / 2.0,
        newCenter.z - ext.z / 2.0, newCenter.x + ext.x / 2.0,
        newCenter.y + ext.y / 2.0, newCenter.z + ext.z / 2.0);
    view3d->LookAtVolume(box);
    view.getUeViewport()->InvalidateController();
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
}

dqGeom::Point3d viewCenterWorld(dqApp::ViewState3d const* view3d)
{
    return view3d->getCenter();
}

// miss 键形验（V4 请求键形——RpcDumpRenderTest.cpp:1494-1525 同源）：派生形
// "-b-<d≥1>-<i>-<j>-<k>-<mult>"（全段 hex），或 V4 根形 "-b-0-0-0-0-1"
//（:299-301 回退请求根——采集缺根字节是登记在案的 dump 限制）。
bool isV4RequestKeyShape(std::string const& m)
{
    if (m.rfind("-b-", 0) != 0)
        return false;
    std::vector<std::string> segs;
    size_t start = 0;
    while (true) {
        size_t const sep = m.find('-', start);
        if (sep == std::string::npos) {
            segs.push_back(m.substr(start));
            break;
        }
        segs.push_back(m.substr(start, sep - start));
        start = sep + 1;
    }
    if (segs.size() != 7)
        return false;
    for (size_t s = 2; s < 7; ++s) {
        if (segs[s].empty()
            || segs[s].find_first_not_of("0123456789abcdef") != std::string::npos)
            return false;
    }
    return segs[1] == "b" && segs[6].size() <= 2
           && (segs[2] != "0" || m == "-b-0-0-0-0-1");
}

// 联合 manifest 域（主根 ∪ fallback 根——多根 fetcher 的查找域；
// DumpOpenChainTest.cpp:327-335 同源）。
std::set<std::string> unionManifestKeys(std::vector<std::string> const& roots)
{
    std::set<std::string> keys;
    for (auto const& root : roots) {
        auto manifest = dqApp::loadDumpManifest(root);
        if (!manifest.has_value())
            continue;
        for (auto const& t : manifest->tiles)
            keys.insert(t.treeId + "/" + t.contentId);
    }
    return keys;
}

}  // namespace

// ---------------------------------------------------------------------------
// 锁 1：instances60 浏览零缺失（单 model ⇒ 单树；saved 视图进入；中轴支 =
//   depth-2 放大链——drill 采集域 ×1..×16）。
// ---------------------------------------------------------------------------
// 判据（首绿实测钉值见各断言处）：
// ① 打开面 = 恰 1 键 "-b-2-0-0-0-1" Completed（DumpOpenChain 锁 1 ③ 同钉
//   ——saved 视图下根/d1 canSkip 穿透[maxInitialTilesToSkip=3]）；
// ② zoom 泵 kZoom=2^n 绕 saved 视域中心：drill 5 键全连通（饱和）+ 1 级
//   过冲（m32 派生——采集 ×16 饱和顶外）；泵形级数实测钉死；
// ③ ×0.5 回退 2 级 + 平移 ±（0.75×视图宽沿世界 X——内容域内）：新增请求
//   键实测钉死（预期零——放大链瓦覆盖全树范围，回退/平移复用已就绪瓦）；
// ④ 终态对账：零 Error/零重复；Completed ⊆ 联合域（instances60-v1 ∪
//   instances60-drill-v1）；域外白名单 = 恰 1 枚过冲放大子
//   "-b-2-0-0-0-20"（×32——M-G drill 锁同款过冲形）；Completed→graphics
//   计数相等 + 零在途；
// ⑤ 双像素锚：saved 帧（与 DumpOpenChain 锁 1 ④ 同钉值源——首绿
//   410953 px 的 0.2× 门 + 质心中央带 + 四角背景）+ 最深饱和帧（首绿实测
//   钉死——内容存活 + 质心中央带）。
TEST(DumpBrowse, Instances60BrowseSessionZeroMissing)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "DumpBrowse";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin(400);

    dta::DumpOpenPackage pkg;
    pkg.imodelRoot = kDumpRoot + "/instances60-imodel-v1";
    pkg.tileRoots = {kDumpRoot + "/instances60-v1", kDumpRoot + "/instances60-drill-v1"};
    auto opened = dta::openDumpIModel(view, pkg);
    ASSERT_TRUE(opened.has_value()) << "open chain failed: " << pkg.imodelRoot;
    ASSERT_EQ(1u, opened->trees.size());

    // ① 打开面（saved 视图泵至静默）——恰 1 键 Completed。
    PumpContext ctx{&view, &*opened};
    int const openQ = pumpToQuiesce(ctx);
    ASSERT_GE(openQ, 0) << "open face never quiesced (log="
                        << opened->fetcher->requestLog().size() << ")";
    view.getUeViewport()->RenderFrame();
    {
        auto const& log = opened->fetcher->requestLog();
        ASSERT_EQ(1u, log.size()) << "open face drifted from the saved-view pin";
        EXPECT_EQ("-b-2-0-0-0-1", log[0].contentId);
        EXPECT_EQ(dqApp::DumpTileFetcher::DumpFetchOutcome::Completed, log[0].outcome);
    }

    // ⑤ 锚 A：saved 帧（与 DumpOpenChain 锁 1 ④ 同钉值源——帧内容同 Recipe
    //    同窗口尺寸 ⇒ 同实测基线 410953 px；阈值 = 0.2×）。
    {
        std::vector<uint8_t> frame;
        uint32_t w = 0, h = 0;
        ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
        dumpBmp(frame, w, h,
                DANQING_TILE_ASSETS_DIR "/../../build/browse-instances60-saved.bmp");
        long count = 0;
        double cx = 0, cy = 0;
        uint32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
        ASSERT_TRUE(contentStats(frame, w, h, count, cx, cy, minX, maxX, minY, maxY))
            << "no content at the saved view (see BMP)";
        printf("[BROWSE] instances60 saved anchor: content=%ld px (%.3f%%) "
               "bbox=(%u,%u)-(%u,%u) centroid=(%.0f,%.0f) frame=%ux%u\n",
               count, 100.0 * count / (static_cast<double>(w) * h),
               minX, minY, maxX, maxY, cx, cy, w, h);
        EXPECT_GE(count, 82000l)
            << "saved-view content barely visible (threshold = 0.2x of the "
               "first GREEN measurement 410953)";
        EXPECT_LT(std::abs(cx - w / 2.0), w * 0.2) << "centroid x off-center";
        EXPECT_LT(std::abs(cy - h / 2.0), h * 0.2) << "centroid y off-center";
        uint32_t const box = std::max(1u, std::min(w, h) * 6 / 100);
        EXPECT_LE(cornerContentRatio(frame, w, h, 0, 0, box), 0.01);
        EXPECT_LE(cornerContentRatio(frame, w, h, w - box, 0, box), 0.01);
        EXPECT_LE(cornerContentRatio(frame, w, h, 0, h - box, box), 0.01);
        EXPECT_LE(cornerContentRatio(frame, w, h, w - box, h - box, box), 0.01);
    }

    // ② zoom 泵 kZoom=2^n 绕 saved 视域中心（= drill 采集的 viewport.zoom
    //    绕视口中心语义——instances60-drill-v1 采集配方）。
    std::set<std::string> const drillKeys = {
        "-b-2-0-0-0-1", "-b-2-0-0-0-2", "-b-2-0-0-0-4",
        "-b-2-0-0-0-8", "-b-2-0-0-0-10",
    };
    auto connectedDrillCount = [&]() -> size_t {
        size_t n = 0;
        for (auto const& rec : opened->fetcher->requestLog())
            if (rec.outcome == dqApp::DumpTileFetcher::DumpFetchOutcome::Completed
                && drillKeys.count(rec.contentId) > 0)
                ++n;
        return n;
    };
    auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    dqGeom::Point3d const kCenter = viewCenterWorld(view3d);  // saved 视域中心
    dqGeom::Vector3d const baseDiag = view3d->GetExtents();   // aspect fix 后 saved
    printf("[BROWSE] instances60 zoom center=(%.4f,%.4f,%.4f) baseDiag=(%.4f,%.4f,%.4f)\n",
           kCenter.x, kCenter.y, kCenter.z, baseDiag.x, baseDiag.y, baseDiag.z);

    long deepCount = 0;
    double deepCx = 0, deepCy = 0;
    uint32_t deepW = 0, deepH = 0;
    int firstFullLevel = -1, levelsRun = 0;
    for (int n = 1; n <= 8; ++n) {
        double const kZoom = static_cast<double>(1u << n);
        zoomViewAbout(view, kCenter, baseDiag, kZoom);
        int const q = pumpToQuiesce(ctx);
        ASSERT_GE(q, 0) << "zoom level " << n << " never quiesced (log="
                        << opened->fetcher->requestLog().size() << ")";
        levelsRun = n;
        printf("[BROWSE] instances60 level %d (kZoom=%.0f): log=%zu connected=%zu "
               "ready=%ld/%ld\n",
               n, kZoom, opened->fetcher->requestLog().size(),
               connectedDrillCount(), ctx.readyTiles, ctx.totalTiles);
        if (connectedDrillCount() == drillKeys.size()) {
            if (firstFullLevel < 0) {
                firstFullLevel = n;
                // ⑤ 锚 B：最深饱和帧（drill 5 键全就绪级）。
                view.getUeViewport()->RenderFrame();
                std::vector<uint8_t> frameB;
                ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frameB, deepW, deepH));
                dumpBmp(frameB, deepW, deepH,
                        DANQING_TILE_ASSETS_DIR "/../../build/browse-instances60-deepest.bmp");
                uint32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
                ASSERT_TRUE(contentStats(frameB, deepW, deepH, deepCount, deepCx, deepCy,
                                         minX, maxX, minY, maxY))
                    << "no content at the deepest saturated view (see BMP)";
                printf("[BROWSE] instances60 deepest anchor (level %d): content=%ld px "
                       "(%.3f%%) bbox=(%u,%u)-(%u,%u) centroid=(%.0f,%.0f) frame=%ux%u\n",
                       n, deepCount, 100.0 * deepCount / (static_cast<double>(deepW) * deepH),
                       minX, minY, maxX, maxY, deepCx, deepCy, deepW, deepH);
            }
            if (n > firstFullLevel)
                break;  // 过冲/终态级已采——停
        }
    }
    ASSERT_GE(firstFullLevel, 0) << "zoom pump never reached the captured "
                                    "saturation point (drill keys incomplete)";
    // 泵形实钉（首绿实测钉死——harness 变更时按机制复核重钉，M-G drill 锁
    //    同款登记）：m16（-b-2-0-0-0-10）于第 4 级（kZoom=16）连通（saved
    //    视域 extents[19.33×13.53] 小于 M-G fit 体积 ⇒ 饱和较 M-G drill 锁
    //    的 m16@32 提前一级——同机制、基面不同）；过冲级第 5 级（kZoom=32）
    //    派生 m32 后停泵。
    EXPECT_EQ(4, firstFullLevel);
    EXPECT_EQ(5, levelsRun);
    // 锚 B 断言（首绿实测钉死：content=2367075 px[帧 84.54%]、质心
    //    (948,750) 对帧心 (1000,700) 偏移 (-52,+50)——阈值 = 0.2× 实测，
    //    WHERE = 质心中央带[±20%，实测偏移 2.6%/3.6% 的大余量]）。
    EXPECT_GE(deepCount, 473000l)
        << "deepest saturated frame content barely visible (threshold = 0.2x "
           "of the first GREEN measurement 2367075)";
    EXPECT_LT(std::abs(deepCx - deepW / 2.0), deepW * 0.2)
        << "deepest-frame centroid x off-center";
    EXPECT_LT(std::abs(deepCy - deepH / 2.0), deepH * 0.2)
        << "deepest-frame centroid y off-center";

    // ③ ×0.5 回退 2 级 + 平移两方向（回退/平移面新增请求键实测钉死）。
    size_t const logAfterZoom = opened->fetcher->requestLog().size();
    for (int i = 1; i <= 2; ++i) {
        double const kZoom = static_cast<double>(1u << (levelsRun - i));
        zoomViewAbout(view, kCenter, baseDiag, kZoom);
        int const q = pumpToQuiesce(ctx);
        ASSERT_GE(q, 0) << "fallback level " << i << " never quiesced";
        printf("[BROWSE] instances60 fallback %d (kZoom=%.0f): log=%zu\n",
               i, kZoom, opened->fetcher->requestLog().size());
    }
    {
        auto* v3 = view.getUeViewport()->GetView()->AsViewState3d();
        double const d = 0.75 * v3->GetExtents().x;  // 0.75×视图宽（内容域内）
        dqGeom::Point3d const panA =
            dqGeom::Point3d::From(kCenter.x + d, kCenter.y, kCenter.z);
        panViewTo(view, panA);
        int const qA = pumpToQuiesce(ctx);
        ASSERT_GE(qA, 0) << "pan +x never quiesced";
        printf("[BROWSE] instances60 pan +x (d=%.3f): log=%zu\n",
               d, opened->fetcher->requestLog().size());
        dqGeom::Point3d const panB =
            dqGeom::Point3d::From(kCenter.x - d, kCenter.y, kCenter.z);
        panViewTo(view, panB);
        int const qB = pumpToQuiesce(ctx);
        ASSERT_GE(qB, 0) << "pan -x never quiesced";
        printf("[BROWSE] instances60 pan -x: log=%zu ready=%ld/%ld\n",
               opened->fetcher->requestLog().size(), ctx.readyTiles, ctx.totalTiles);
    }
    // 回退+平移新增键实钉（首绿实测）：零——放大链瓦覆盖全树范围（同范围
    //    放大子，TileMetadata.ts:785-799），回退选中更粗已就绪瓦、平移不越
    //    d2 瓦范围 ⇒ 无新请求。任何新增键必须 Completed 且在联合域（下方
    //    总对账兜住）。
    EXPECT_EQ(logAfterZoom, opened->fetcher->requestLog().size())
        << "fallback/pan phases issued new tile requests (first GREEN measured "
           "zero — magnified tiles cover the whole tree range)";

    // ④ 终态对账（全程 requestLog）。
    std::set<std::string> const unionKeys = unionManifestKeys(pkg.tileRoots);
    auto const& log = opened->fetcher->requestLog();
    std::set<std::string> requestedKeys;
    size_t numCompleted = 0, numNotFound = 0, numError = 0;
    std::vector<std::string> misses;
    std::set<std::string> completedIds;
    for (auto const& rec : log) {
        std::string const key = rec.treeId + "/" + rec.contentId;
        requestedKeys.insert(key);
        EXPECT_EQ("25_1d-E:6_0x1c", rec.treeId) << "request escaped the tree domain";
        switch (rec.outcome) {
        case dqApp::DumpTileFetcher::DumpFetchOutcome::Completed:
            ++numCompleted;
            completedIds.insert(rec.contentId);
            EXPECT_TRUE(unionKeys.count(key) > 0)
                << "Completed key not in the union domain: " << key;
            break;
        case dqApp::DumpTileFetcher::DumpFetchOutcome::NotFound:
            ++numNotFound;
            misses.push_back(rec.contentId);
            break;
        case dqApp::DumpTileFetcher::DumpFetchOutcome::Error:
            ++numError;
            break;
        }
    }
    printf("[BROWSE] instances60 reconcile: requested=%zu (completed=%zu "
           "notFound=%zu error=%zu) ready=%ld/%ld\n",
           log.size(), numCompleted, numNotFound, numError,
           ctx.readyTiles, ctx.totalTiles);
    for (auto const& rec : log)
        printf("[BROWSE]   log %d %s\n", static_cast<int>(rec.outcome),
               rec.contentId.c_str());

    EXPECT_EQ(0u, numError) << "dump asset integrity break";
    EXPECT_EQ(requestedKeys.size(), log.size()) << "duplicate tile requests";
    // drill 5 键全 Completed（浏览主判据——视口可达面 = 采集面）。
    for (auto const& key : drillKeys)
        EXPECT_TRUE(completedIds.count(key) > 0)
            << "drill key never connected: " << key;
    // 域外白名单实钉（首绿实测）= 恰 1 枚过冲放大子 "-b-2-0-0-0-20"
    //    （mult=0x20——SSE 越过采集饱和顶 ×16 的派生；M-G drill 锁同款过冲
    //    形）。出现细分/兄弟支域外键 = 配方越界或引擎发散（§11.8）——本
    //    断言即其暴露面。
    ASSERT_EQ(1u, misses.size());
    EXPECT_EQ("-b-2-0-0-0-20", misses[0]);
    for (auto const& m : misses)
        EXPECT_TRUE(isV4RequestKeyShape(m))
            << "miss key is not a V4 request-key shape: " << m;
    // 计数实钉（首绿实测）：Completed=5（打开面 m1 + 泵 m2/m4/m8/m16）；
    //    总请求=6（+1 过冲 miss）；零重复。
    EXPECT_EQ(5u, numCompleted);
    EXPECT_EQ(6u, log.size());
    // Completed→graphics 对账 + 树侧零在途终态（首绿实测钉死）。过冲键
    //    -b-2-0-0-0-20 是 NotFound（域外）不入 Completed——TD-27 的
    //    unquantized 形态在本模型只出现于该域外过冲键（joeshouse 锁的
    //    0x3f/-b-2-0-0-0-20 是域内采集键，裁决白名单见该锁）。
    EXPECT_EQ(static_cast<long>(numCompleted), ctx.readyTiles)
        << "Completed tiles without graphics — readContent/graphics chain break";
    EXPECT_EQ(0u, opened->fetcher->getActiveCount()) << "requests still in flight";

    view.getUeViewport()->DropTiledGraphicsProvider(opened->provider.get());
    view.close();
    spin(200);
}

// ---------------------------------------------------------------------------
// 锁 2：joeshouse 浏览零缺失（10 model ⇔ 10 树；saved 等轴测进入；中轴支 =
//   0x3f/0x4d 的 depth-2 放大链——drill 采集域 0x3f ×1..×32 / 0x4d ×1..×16
//   + 4 中树 d2-m1）。
// ---------------------------------------------------------------------------
// 判据（首绿实测钉值见各断言处）：
// ① 打开面 = 恰 10 键（4 叶根树根键 + 6 非叶树 d2 跳级键——DumpOpenChain
//   锁 2 ③-open 同钉）；
// ② zoom 泵 kZoom=2^n 绕内容质心 [7.302,4.264,4.496]（= drill targets[0]）
//   ——drill 15 键全连通（饱和）+ 1 级过冲；泵形级数实测钉死（与
//   DumpOpenChain 锁 2 同 Recipe 同窗口 ⇒ 同源钉值 firstFull=6/run=7）；
// ③ ×0.5 回退 2 级（128×→32×）+ 平移 ±（0.75×视图宽沿世界 X——房屋域内，
//   平移面保持在 sweep 全树域/drill 中轴域内）：新增请求键实测钉死；
// ④ 终态对账：零 Error/零重复；Completed ⊆ 联合域（joeshouse-v1 ∪
//   joeshouse-drill-v1 ∪ joeshouse-drill-v2[M-I(5) P2c 缩远态域]）；域外白名单 = 恰 3 枚
//   过冲放大子（0x4d/-20、0x3f/-40、0x4d/-40——DumpOpenChain 锁 2 ③d 同钉）；
//   Completed→graphics 对账 + 零在途；
// ⑤ 双像素锚：saved 帧（DumpOpenChain 锁 2 ④ 同钉值源——首绿 1798847 px
//   的 0.2× 门 + 质心中央带）+ 最深饱和帧（64×——首绿实测钉死）。
TEST(DumpBrowse, JoesHouseBrowseSessionZeroMissing)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "DumpBrowse";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin(400);

    dta::DumpOpenPackage pkg;
    pkg.imodelRoot = kDumpRoot + "/joeshouse-v1";
    pkg.tileRoots = {kDumpRoot + "/joeshouse-v1", kDumpRoot + "/joeshouse-drill-v1",
                     kDumpRoot + "/joeshouse-drill-v2"};
    auto opened = dta::openDumpIModel(view, pkg);
    ASSERT_TRUE(opened.has_value()) << "open chain failed: " << pkg.imodelRoot;
    ASSERT_EQ(10u, opened->trees.size());

    // ① 打开面（saved 视图泵至静默）——恰 10 键全 Completed。
    PumpContext ctx{&view, &*opened};
    int const openQ = pumpToQuiesce(ctx);
    ASSERT_GE(openQ, 0) << "open face never quiesced (log="
                        << opened->fetcher->requestLog().size() << ")";
    view.getUeViewport()->RenderFrame();
    {
        std::set<std::string> const expectedOpenFace = {
            "25_1d-E:6_0x26/-b-0-0-0-0-1", "25_1d-E:6_0x3d/-b-0-0-0-0-1",
            "25_1d-E:6_0x43/-b-0-0-0-0-1", "25_1d-E:6_0x4b/-b-0-0-0-0-1",
            "25_1d-E:6_0x3f/-b-2-0-0-0-1", "25_1d-E:6_0x41/-b-2-0-0-0-1",
            "25_1d-E:6_0x45/-b-2-0-0-0-1", "25_1d-E:6_0x47/-b-2-0-0-0-1",
            "25_1d-E:6_0x49/-b-2-0-0-0-1", "25_1d-E:6_0x4d/-b-2-0-0-0-1",
        };
        std::set<std::string> openFaceKeys;
        for (auto const& rec : opened->fetcher->requestLog()) {
            openFaceKeys.insert(rec.treeId + "/" + rec.contentId);
            EXPECT_EQ(dqApp::DumpTileFetcher::DumpFetchOutcome::Completed,
                      rec.outcome)
                << "open-face key not connected: " << rec.contentId;
        }
        EXPECT_EQ(expectedOpenFace, openFaceKeys)
            << "open face drifted from the DumpOpenChain lock-2 pin";
    }

    // ⑤ 锚 A：saved 帧（打开面静默态——10 瓦全新就绪）。首绿实测
    //    content=907420 px（帧 32.41%）、质心 (943,702) 对帧心 (1000,700)
    //    偏移 (-57,+2)、bbox 满幅[saved 等轴测取景的合法形态——不作四角
    //    断言]。注：DumpOpenChain 锁 2 ④ 的同视图锚钉 1798847 是**浏览后
    //    缓存态**帧——本锚钉的是**打开面缓存态**（m1 直绘）；两值差异的
    //    机制推断：浏览后 m1 瓦全程未选中过期（pruneAndPurge 20s 常数 ×
    //    会话时长——TileAdmin.cpp:523-544）→ m2 子瓦过渡替补上屏更密
    //    （未逐瓦取证，仅两缓存相实测值在案；两相皆参考合法形态）。阈值
    //    = 0.2× 首绿实测；WHERE = 质心中央带（±20%，实测偏移 2.9%/0.1%
    //    大余量）。
    {
        std::vector<uint8_t> frame;
        uint32_t w = 0, h = 0;
        ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
        dumpBmp(frame, w, h,
                DANQING_TILE_ASSETS_DIR "/../../build/browse-joeshouse-saved.bmp");
        long count = 0;
        double cx = 0, cy = 0;
        uint32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
        ASSERT_TRUE(contentStats(frame, w, h, count, cx, cy, minX, maxX, minY, maxY))
            << "no joeshouse content at the saved view (see BMP)";
        printf("[BROWSE] joeshouse saved anchor: content=%ld px (%.3f%%) "
               "bbox=(%u,%u)-(%u,%u) centroid=(%.0f,%.0f) frame=%ux%u\n",
               count, 100.0 * count / (static_cast<double>(w) * h),
               minX, minY, maxX, maxY, cx, cy, w, h);
        EXPECT_GE(count, 181000l)
            << "saved-view content barely visible (threshold = 0.2x of the "
               "first GREEN measurement 907420)";
        EXPECT_LT(std::abs(cx - w / 2.0), w * 0.2) << "centroid x off-center";
        EXPECT_LT(std::abs(cy - h / 2.0), h * 0.2) << "centroid y off-center";
    }

    // ② zoom 泵 kZoom=2^n 绕内容质心（drill 采集配方——DumpOpenChain 锁 2
    //    相 2 同形）。停止规则 = drill 15 键全连通后再一级（过冲/终态形）。
    std::set<std::string> const drillKeys = {
        "25_1d-E:6_0x3f/-b-2-0-0-0-1", "25_1d-E:6_0x3f/-b-2-0-0-0-2",
        "25_1d-E:6_0x3f/-b-2-0-0-0-4", "25_1d-E:6_0x3f/-b-2-0-0-0-8",
        "25_1d-E:6_0x3f/-b-2-0-0-0-10", "25_1d-E:6_0x3f/-b-2-0-0-0-20",
        "25_1d-E:6_0x4d/-b-2-0-0-0-1", "25_1d-E:6_0x4d/-b-2-0-0-0-2",
        "25_1d-E:6_0x4d/-b-2-0-0-0-4", "25_1d-E:6_0x4d/-b-2-0-0-0-8",
        "25_1d-E:6_0x4d/-b-2-0-0-0-10",
        "25_1d-E:6_0x41/-b-2-0-0-0-1", "25_1d-E:6_0x45/-b-2-0-0-0-1",
        "25_1d-E:6_0x47/-b-2-0-0-0-1", "25_1d-E:6_0x49/-b-2-0-0-0-1",
    };
    auto connectedDrillCount = [&]() -> size_t {
        size_t n = 0;
        for (auto const& rec : opened->fetcher->requestLog())
            if (rec.outcome == dqApp::DumpTileFetcher::DumpFetchOutcome::Completed
                && drillKeys.count(rec.treeId + "/" + rec.contentId) > 0)
                ++n;
        return n;
    };
    auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    dqGeom::Vector3d const baseDiag = view3d->GetExtents();  // aspect fix 后 saved
    dqGeom::Point3d const kContentCentroid =
        dqGeom::Point3d::From(7.302, 4.264, 4.496);  // drill targets[0]

    long deepCount = 0;
    double deepCx = 0, deepCy = 0;
    uint32_t deepW = 0, deepH = 0;
    int firstFullLevel = -1, levelsRun = 0;
    for (int n = 1; n <= 10; ++n) {
        double const kZoom = static_cast<double>(1u << n);
        zoomViewAbout(view, kContentCentroid, baseDiag, kZoom);
        int const q = pumpToQuiesce(ctx);
        ASSERT_GE(q, 0) << "zoom level " << n << " never quiesced (log="
                        << opened->fetcher->requestLog().size() << ")";
        levelsRun = n;
        printf("[BROWSE] joeshouse level %d (kZoom=%.0f): log=%zu connected=%zu "
               "ready=%ld/%ld\n",
               n, kZoom, opened->fetcher->requestLog().size(),
               connectedDrillCount(), ctx.readyTiles, ctx.totalTiles);
        if (connectedDrillCount() == drillKeys.size()) {
            if (firstFullLevel < 0) {
                firstFullLevel = n;
                // ⑤ 锚 B：最深饱和帧（drill 15 键全就绪级——64×）。
                view.getUeViewport()->RenderFrame();
                std::vector<uint8_t> frameB;
                ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frameB, deepW, deepH));
                dumpBmp(frameB, deepW, deepH,
                        DANQING_TILE_ASSETS_DIR "/../../build/browse-joeshouse-deepest.bmp");
                uint32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
                ASSERT_TRUE(contentStats(frameB, deepW, deepH, deepCount, deepCx, deepCy,
                                         minX, maxX, minY, maxY))
                    << "no content at the deepest saturated view (see BMP)";
                printf("[BROWSE] joeshouse deepest anchor (level %d): content=%ld px "
                       "(%.3f%%) bbox=(%u,%u)-(%u,%u) centroid=(%.0f,%.0f) frame=%ux%u\n",
                       n, deepCount, 100.0 * deepCount / (static_cast<double>(deepW) * deepH),
                       minX, minY, maxX, maxY, deepCx, deepCy, deepW, deepH);
            }
            if (n > firstFullLevel)
                break;
        }
    }
    ASSERT_GE(firstFullLevel, 0) << "zoom pump never reached the captured "
                                    "saturation point (drill keys incomplete)";
    // 泵形实钉（与 DumpOpenChain 锁 2 ③e 同源钉值——同 Recipe 同窗口尺寸；
    //    harness 变更时按机制复核重钉）。
    EXPECT_EQ(6, firstFullLevel);  // m32（0x3f -20）于 kZoom=64 连通
    EXPECT_EQ(7, levelsRun);       // 过冲级 kZoom=128 后停泵
    // 锚 B 断言（首绿实测钉死：content=943958 px[帧 33.71%——64× 下房屋细
    //    节满幅分布]；复测稳定值 943933（M-H(5) 终审独立复跑 ×2——终审
    //    Minor 1 登记：首绿/复测两值差 0.003%，阈值 0.2× 余量内同一钉死
    //    比例）；WHERE = 质心右上象限实钉——首绿 (1402,351) = 帧
    //    (70.1%w, 25.1%h)：zoom 中心[内容质心世界点]映到帧心，房屋几何在
    //    该点右上分布[两层主体+屋顶在右上天际侧]——位置断言钉方向+量值
    //    带[实测 ±0.15 余量]，比中央带更强的 WHERE）。
    EXPECT_GE(deepCount, 188000l)
        << "deepest saturated frame content barely visible (threshold = 0.2x "
           "of the first GREEN measurement 943958)";
    EXPECT_GT(deepCx, deepW * 0.55) << "deepest-frame centroid lost the right-half placement";
    EXPECT_LT(deepCx, deepW * 0.85) << "deepest-frame centroid x drifted";
    EXPECT_GT(deepCy, deepH * 0.10) << "deepest-frame centroid lost the upper-half placement";
    EXPECT_LT(deepCy, deepH * 0.45) << "deepest-frame centroid y drifted";

    // ③ ×0.5 回退 2 级（128×→32×）+ 平移两方向（±0.75×视图宽沿世界 X——
    //    房屋域内；平移面保持在 sweep 全树域/drill 中轴域内）。
    size_t const logAfterZoom = opened->fetcher->requestLog().size();
    for (int i = 1; i <= 2; ++i) {
        double const kZoom = static_cast<double>(1u << (levelsRun - i));
        zoomViewAbout(view, kContentCentroid, baseDiag, kZoom);
        int const q = pumpToQuiesce(ctx);
        ASSERT_GE(q, 0) << "fallback level " << i << " never quiesced";
        printf("[BROWSE] joeshouse fallback %d (kZoom=%.0f): log=%zu\n",
               i, kZoom, opened->fetcher->requestLog().size());
    }
    {
        auto* v3 = view.getUeViewport()->GetView()->AsViewState3d();
        double const d = 0.75 * v3->GetExtents().x;  // 0.75×视图宽（房屋域内）
        dqGeom::Point3d const panA = dqGeom::Point3d::From(
            kContentCentroid.x + d, kContentCentroid.y, kContentCentroid.z);
        panViewTo(view, panA);
        int const qA = pumpToQuiesce(ctx);
        ASSERT_GE(qA, 0) << "pan +x never quiesced";
        printf("[BROWSE] joeshouse pan +x (d=%.3f): log=%zu\n",
               d, opened->fetcher->requestLog().size());
        dqGeom::Point3d const panB = dqGeom::Point3d::From(
            kContentCentroid.x - d, kContentCentroid.y, kContentCentroid.z);
        panViewTo(view, panB);
        int const qB = pumpToQuiesce(ctx);
        ASSERT_GE(qB, 0) << "pan -x never quiesced";
        printf("[BROWSE] joeshouse pan -x: log=%zu ready=%ld/%ld\n",
               opened->fetcher->requestLog().size(), ctx.readyTiles, ctx.totalTiles);
    }
    // 回退+平移新增键实钉（首绿实测——任何新增键必须 Completed 且在联合
    //    域[下方总对账兜住]；预期零——放大链瓦覆盖全树范围）。
    EXPECT_EQ(logAfterZoom, opened->fetcher->requestLog().size())
        << "fallback/pan phases issued new tile requests (first GREEN measured "
           "zero — magnified tiles cover the whole tree range)";

    // ④ 终态对账（全程 requestLog）。
    std::set<std::string> const unionKeys = unionManifestKeys(pkg.tileRoots);
    auto const& log = opened->fetcher->requestLog();
    std::set<std::string> requestedKeys;
    size_t numCompleted = 0, numNotFound = 0, numError = 0;
    std::vector<std::string> misses;
    std::set<std::string> completedKeys;
    for (auto const& rec : log) {
        std::string const key = rec.treeId + "/" + rec.contentId;
        requestedKeys.insert(key);
        switch (rec.outcome) {
        case dqApp::DumpTileFetcher::DumpFetchOutcome::Completed:
            ++numCompleted;
            completedKeys.insert(key);
            EXPECT_TRUE(unionKeys.count(key) > 0)
                << "Completed key not in the union domain: " << key;
            break;
        case dqApp::DumpTileFetcher::DumpFetchOutcome::NotFound:
            ++numNotFound;
            misses.push_back(key);
            break;
        case dqApp::DumpTileFetcher::DumpFetchOutcome::Error:
            ++numError;
            break;
        }
    }
    printf("[BROWSE] joeshouse reconcile: requested=%zu (completed=%zu "
           "notFound=%zu error=%zu) levels=%d (full@%d) ready=%ld/%ld\n",
           log.size(), numCompleted, numNotFound, numError, levelsRun,
           firstFullLevel, ctx.readyTiles, ctx.totalTiles);
    for (auto const& rec : log)
        printf("[BROWSE]   log %d %s/%s\n", static_cast<int>(rec.outcome),
               rec.treeId.c_str(), rec.contentId.c_str());

    EXPECT_EQ(0u, numError) << "dump asset integrity break";
    EXPECT_EQ(requestedKeys.size(), log.size()) << "duplicate tile requests";
    // drill 15 键全 Completed（浏览主判据——视口可达面 = 采集面）。
    for (auto const& key : drillKeys)
        EXPECT_TRUE(completedKeys.count(key) > 0)
            << "drill key never connected: " << key;
    // 域外白名单实钉（DumpOpenChain 锁 2 ③d 同钉——恰 3 枚过冲放大子：
    //    0x4d ×32[采集 0x4d 饱和 ×16]、0x3f/0x4d ×64[过冲级 kZoom=128]；
    //    全为 V4 放大派生形[mult 段 hex]）。出现细分/兄弟支域外键 = 配方
    //    越界或引擎发散（§11.8）——本断言即其暴露面。
    ASSERT_EQ(3u, misses.size());
    EXPECT_EQ("25_1d-E:6_0x4d/-b-2-0-0-0-20", misses[0]);
    EXPECT_EQ("25_1d-E:6_0x3f/-b-2-0-0-0-40", misses[1]);
    EXPECT_EQ("25_1d-E:6_0x4d/-b-2-0-0-0-40", misses[2]);
    for (auto const& m : misses) {
        std::string const cid = m.substr(m.find('/') + 1);
        EXPECT_TRUE(isV4RequestKeyShape(cid))
            << "miss key is not a V4 request-key shape: " << m;
    }
    // 计数实钉（DumpOpenChain 锁 2 ③c 同钉——Completed=19[打开面 10 + 泵
    //    9]；总请求=22[零重复]）。
    EXPECT_EQ(19u, numCompleted);
    EXPECT_EQ(22u, log.size());
    // Completed→graphics 对账 + 树侧零在途终态（首绿实测钉死）。逐瓦走查
    //    （M-E 锁 ⑤ 同形——RpcDumpRenderTest.cpp:1140-1165）：Completed 且
    //    无 graphics 的键集 = **裁决白名单**（逐键钉死——白名单外任何
    //    Completed 无 graphics = 新缺口，即红）：
    //    (a) 4 叶根树根键（0x26/0x3d/0x43/0x4b）——**空瓦语义**：byteLength
    //        424、header numElementsIncluded=0、glTF 零 primitive（本任务
    //        取证实钉）——参考侧空内容同样零 graphic；
    //    (b) "0x3f/-b-2-0-0-0-20"——**TD-27**：numRgbaPerVertex=5 +
    //        usesUnquantizedPositions=true 的 unquantized-LUT 顶点表形态
    //        （20B/顶点：位置=4 texel 转置 f32 + 每 texel .w 装
    //        featureAndMaterial 字节，42 元素 36+6 instances 实内容）。
    //        参考消费链在案（VertexLUT.ts:99 !usesUnquantizedPositions →
    //        glsl Vertex.ts:227/:254 分支 + Color.ts:17 + Surface.ts:397-
    //        398/:461 texel 源切换）；DanQing 的 shader 解码/pre-read 分支
    //        已 1:1 预移植（VertexTableShaders.h kComputeUnquantizedPosition-
    //        FromLUT/kPreReadVertexDataUnquantized + addVertexTable(quantized)
    //        选择器）——缺口在解析层（TilesetJson.h 未携带该字段）与接受层
    //       （ImdlGraphics.cpp:208 numRgba!=4 拒绝）+ 变体标志把"LUT 几何"
    //        与"16-bit 解码"混为一维（SurfaceCommon.h:118-126——
    //        quantized=false 走非 LUT attribute 路径）。与 TD-20 遗留清单的
    //        "12B SimpleBuilder 拒绝入 LUT"同函数同形态先例（登记未修）。
    //        用户可见症状限于 ×32 极端放大（父瓦 m16 LOD 兜底显示——锚 B
    //        在 64× 实测内容 33.71% 上屏即该兜底的实证）。
    {
        std::set<std::string> const kAdjudicatedNoGraphics = {
            "25_1d-E:6_0x26/-b-0-0-0-0-1", "25_1d-E:6_0x3d/-b-0-0-0-0-1",
            "25_1d-E:6_0x43/-b-0-0-0-0-1", "25_1d-E:6_0x4b/-b-0-0-0-0-1",
            "25_1d-E:6_0x3f/-b-2-0-0-0-20",
        };
        std::map<std::string, dqRender::Tile*> tilesByKey;
        std::function<void(dqRender::Tile*, std::string const&)> walk =
            [&](dqRender::Tile* tile, std::string const& tid) {
                if (!tile)
                    return;
                auto* imdl = static_cast<dqRender::ImdlTile*>(tile);
                tilesByKey[tid + "/" + imdl->getContentId()] = tile;
                for (dqRender::Tile* child : tile->getChildren())
                    walk(child, tid);
            };
        // trees[i] ⇔ treeLoadLog[i]（openDumpIModel 同循环同序推送）。
        for (size_t i = 0; i < opened->trees.size(); ++i)
            walk(opened->trees[i]->getRootTile(), opened->treeLoadLog[i]);
        std::set<std::string> noGraphics;
        for (auto const& kv : tilesByKey)
            if (completedKeys.count(kv.first) > 0 && !kv.second->hasGraphics())
                noGraphics.insert(kv.first);
        EXPECT_EQ(kAdjudicatedNoGraphics, noGraphics)
            << "Completed-without-graphics set drifted from the adjudicated "
               "whitelist (empty-tile roots + TD-27 unquantized form) — a new "
               "consumption gap surfaced";
        EXPECT_EQ(static_cast<long>(numCompleted - noGraphics.size()),
                  ctx.readyTiles)
            << "Completed tiles without graphics beyond the adjudicated "
               "whitelist — readContent/graphics chain break";
    }
    EXPECT_EQ(0u, opened->fetcher->getActiveCount()) << "requests still in flight";

    view.getUeViewport()->DropTiledGraphicsProvider(opened->provider.get());
    view.close();
    spin(200);
}

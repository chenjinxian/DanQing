// RpcDumpRenderTest — RPC 真实数据端到端渲染锁（M-D(3)）。
//
// 真实 itwinjs 后端产的 imdl 树/瓦（仓外 danqing-rpc-tools collector 从
// display-test-app + CompatibilityTestSeed.bim / Mirukuru.xlsx 采集，阶段1
// M-D Task 1；dump 资产 third_party/tile-sample-assets/rpc-dumps/ 采集后只读
// §11.11）经 DumpTileFetcher（本地 manifest 回放，零网络 §8.2）+ ImdlTileTree
// （树 props 装配——PrimaryTileTree.createTileTree 的离线对应物）→
// TiledGraphicsProvider 装树 → 真窗口渲染 → readPixels 断言。
//
// Authored: no reference test exists in itwinjs-core for RPC replay rendering
//           (参考的 tile 渲染覆盖在 DTA 人工/集成测试；其 tile 管线单测
//           TileMetadata.test.ts/TileAdmin.test.ts 不驱动真窗口)。Authorized
//           by CLAUDE.md §5(g)（渲染像素回归）+ §11.11（判据三维：内容存活
//           占比 + WHERE 位置断言（质心中央带、四象限角为背景）+ 多瓦计数
//           （graphics 提交瓦数——SelectTilesProtocol 消费面级联））。
//           dump 是采集资产，禁止原地突变（§11.11）。
//
// ---------------------------------------------------------------------------
// 两只锁的分工（TD-25 取证的结论，见下）：
//   - mirukuru-v1 = **像素锁**：其单瓦（files/6.imdl，1652B）是无 instances
//     修饰的平面片，顶点量化域即 iModel 坐标（decodeMatrix 平移 -919/-637）
//     ——现有 LUT 消费链（U7）可直接上屏。①内容存活 + ②WHERE + 消费计数
//     全部在此断言。
//   - compatseed-v1 = **LOD 链消费锁**：7 瓦链（-b-0..-b-6）验证请求键覆写
//     （IModelTileTree.ts:398）→ manifest 键域命中 → 字节回放 → 协议选择 →
//     ≥2 瓦 graphics 提交。其瓦内容全部带 instances 修饰（transformCenter
//     -98.755 + 每实例变换），参考经 InstancedGraphicParams→
//     createInstancedGraphic（ImdlGraphicsCreator.ts:243-257、webgl
//     System.ts:518/:577）按实例放置；DanQing 未消费（TD-25 登记）——几何落
//     在局部量化盒（decodedMin/Max ±0.25）→ 取景域外，像素断言归 TD-25
//     清偿时补（勿以调取景域的方式绕过——那是把缺陷烤进判据，§12.8）。
//
// 已登记的载体缺口（Task 2 报告 + 本任务取证，非本锁断言对象）：
//   - 树 props 的 location（99.755 平移——iModel→world）未被 ImdlTileTree
//     消费。两只锁的取景都用树 contentRange 的 iModel 坐标域（瓦顶点同域
//     ——mirror 验证：compatseed/mirukuru 的 tile 头 contentRange == 树
//     contentRange），故内容存活/形状可断言；世界位置忠实性归 location
//     载体（若几何整体出视口且取景域正确，根因即此——§12.10 取证修，勿调
//     阈值）。
//   - instances（TD-25，见 compatseed 锁头注释）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"

#include "DumpMount.h"  // 多树全量挂载入口（M-E Task 2——manifest trees 迭代 + fit 并集）

#include <dqApp/Application.h>
#include <dqApp/Viewport.h>
#include <dqApp/StandardView.h>
#include <dqApp/ViewTool.h>
#include <dqApp/ViewState.h>
#include <dqRender/tile/ImdlTileTree.h>
#include <dqRender/tile/TileAdmin.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {
struct QtEnvR9 {
    QtEnvR9()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvR9 s_qtR9;

void spin(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// "内容像素"（与背景差异显著）计数 + 包围盒 + 质心。判别阈值 90（通道差和）
// ——实测背景是天空+地面渐变（首像素基准 (142,205,255)，向下渐变到
// (106,169,255)，最大通道差和 72）；瓦几何（灰 192,192,192）对渐变域的最小
// 通道差和 126——90 在两者之间，先验定出（§11.11 判别器校准，非结果调参）。
bool contentStats(std::vector<uint8_t> const& f, uint32_t w, uint32_t h,
                  long& count, double& cx, double& cy,
                  uint32_t& minX, uint32_t& maxX, uint32_t& minY, uint32_t& maxY)
{
    uint8_t const* bg = &f[0];
    long sx = 0, sy = 0;
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

// 角落探针：count 与背景差异显著的比例（四象限角为背景的位置断言面）。
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

// 绿 dominance 计数（完整加载锁的轻量像素锚——上下文 prim0 均匀绿 65280；
// 与 Instances60RendersAllInstances 的完整分类判据分工：本锁只钉"消费的
// 根瓦实际上屏"，逐实例着色分布归那只锁）。
long countGreenPixels(std::vector<uint8_t> const& f, uint32_t w, uint32_t h)
{
    uint8_t const* bg = &f[0];
    long green = 0;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4u];
            int const dr = std::abs(p[0] - bg[0]);
            int const dg = std::abs(p[1] - bg[1]);
            int const db = std::abs(p[2] - bg[2]);
            if (dr + dg + db <= 90)
                continue;  // 背景域
            int const r = p[0], g = p[1], b = p[2];
            if (g > r + 20 && g > b + 20)
                ++green;
        }
    }
    return green;
}

// 树内 graphics 就绪瓦计数（多瓦锁判据——"graphics 提交计数 >0"的瓦数）。
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
    printf("[RPC-RENDER] frame dumped to %s\n", path);
}

}  // namespace

// ---------------------------------------------------------------------------
// 像素锁：mirukuru-v1 真后端瓦上屏（内容存活 + WHERE + 消费计数）。
// 装载经 DumpMount.h 多树全量挂载入口（M-E Task 2：manifest trees 迭代，
// 每树一 TileTree——本 dump 2 树全装，0x28 空树随之入 provider，参与
// fit 并集时以其 contentRange 为 null 跳过）。
// ---------------------------------------------------------------------------
// Authored: 见文件头。该瓦无 instances 修饰、顶点量化域 = iModel 坐标域——
// 现有 LUT 消费链的忠实上屏。判据：
// ①内容存活：非背景像素 ≥ 250000（≈ 首绿实测 572660 px 的 0.44×，20.45%→0.2% 退化必红）；
// ②WHERE：质心在视口中央带（±20%）+ 四象限角为背景（6% 角框 ≤1% 内容）；
// ③消费计数：≥1 瓦 graphics 提交 + ≥1 次 dispatch（该 dump 只有 1 瓦）。
TEST(RpcDumpRender, MirukuruRendersRealBackendTile)
{
    std::string dumpRoot = std::string(DANQING_TILE_ASSETS_DIR)
                           + "/rpc-dumps/mirukuru-v1";
    if (char const* env = std::getenv("DANQING_RPC_DUMP"))
        dumpRoot = env;

    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "RpcDumpRender";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin(400);

    // 1. 多树全量装载（DumpMount.h 挂载入口：manifest + props + fetcher
    //    注入 + 每树 ImdlTileTree 装配——树序 == manifest trees[] 序）。
    auto mount = dta::mountDump(*view.getUeViewport(), dumpRoot);
    ASSERT_TRUE(mount.has_value()) << "mount failed: " << dumpRoot;
    ASSERT_EQ(2u, mount->manifest.trees.size());  // mirukuru-v1：2 树 1 瓦
    ASSERT_EQ(1u, mount->manifest.tiles.size());
    ASSERT_EQ(mount->trees.size(), mount->manifest.trees.size());
    std::string const treeId = mount->manifest.trees[0].treeId;
    auto props = mount->props->byTreeId(treeId);
    ASSERT_TRUE(props.has_value()) << "byTreeId failed: " << treeId;
    // 前置（资产自洽，非回归断言）：RPC dump 携带 formatVersion——M-D(3) 起
    // 消费（ContentIdProvider V4 请求键形态的切换依据，IModelTileTree.ts:396）。
    ASSERT_EQ(2424832u, props->metadata.formatVersion);  // 37.0（major<<16）

    // 2. 请求键锁（IModelTileTree.ts:398 rootContentId 覆写——根键必须是
    //    manifest 的 depth-0 瓦键，否则 fetch NotFound）。trees[0] = 0x1c。
    {
        ASSERT_EQ("-b-0-0-0-0-1", mount->manifest.tiles[0].contentId);
        auto* root =
            static_cast<dqRender::ImdlTile*>(mount->trees[0]->getRootTile());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(mount->manifest.tiles[0].contentId, root->getContentId())
            << "root request key must be the manifest key (IModelTileTree.ts:398 "
               "rootContentId override)";
    }

    // 3. 装树（TiledGraphicsProvider 应用通道）+ 取景 + 泵帧。
    view.getUeViewport()->AddTiledGraphicsProvider(&mount->provider);
    ASSERT_TRUE(view.getUeViewport()->HasTiledGraphicsProvider(&mount->provider));

    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        // 取景 = fitRange（iModelInfo 缺省——本 dump provenance 无 iModel；
        // 回退树 contentRange 并集 = 0x1c 单树域，0x28 空树 null range 跳过），
        // 30% 外扩 + zEps=1.0（z 向亚厘米薄盒给 Iso 视线留深度——M-D(3)
        // 既有取景参数，DumpMount.h 归并）。
        view3d->LookAtVolume(
            dta::mountDumpFitVolume(mount->fitRange, /*zEps=*/1.0));
        view.getUeViewport()->InvalidateController();
    }
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    {
        auto* tool = new dqApp::StandardViewTool(view.getUeViewport(),
                                                 dqApp::StandardViewId::Iso);
        if (!tool->run())
            delete tool;
    }

    // 泵帧到瓦就绪（fetch → processCompleted → 失效级联 → 重绘；有界等待）。
    uint32_t const dispatchedBefore =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests;
    long readyTiles = 0, totalTiles = 0;
    for (int i = 0; i < 80; ++i) {  // ≤8s 有界
        spin(100);
        readyTiles = 0;
        totalTiles = 0;
        for (auto& t : mount->trees)
            countGraphicsReady(t->getRootTile(), readyTiles, totalTiles);
        if (readyTiles >= 1 && i >= 8)  // 瓦就绪且至少跑了几帧重绘
            break;
    }
    view.getUeViewport()->RenderFrame();
    uint32_t const dispatched =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests
        - dispatchedBefore;
    printf("[RPC-RENDER] graphics-ready tiles=%ld/%ld dispatched=%u\n",
           readyTiles, totalTiles, dispatched);

    // 4. 像素判据。
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    // 绝对路径锚（§12.10.4：CWD 相对路径是历次假观测来源）——资产宏是绝对
    // 路径，仓库根 = 其上两级。
    dumpBmp(frame, w, h,
            DANQING_TILE_ASSETS_DIR "/../../build/rpc-dump-mirukuru.bmp");

    long count = 0;
    double cx = 0, cy = 0;
    uint32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
    ASSERT_TRUE(contentStats(frame, w, h, count, cx, cy, minX, maxX, minY, maxY))
        << "no replayed RPC tile content rendered — dump replay chain broken "
           "(fetch keys? tree assembly? see [TILE-TRACE]/BMP)";

    // ① 内容存活（阈值 250000 ≈ 首绿实测 572660 px 的 0.44×——20.45%→0.2%
    //    的退化（~45× 跌落）必红；首绿质心 (999,698) 对帧心 (1000,700)、
    //    bbox (284,284)-(1714,1111) 近对称——几何即取景域内的模型平面）。
    printf("[RPC-RENDER] content=%ld px (%.3f%% of %ux%u) bbox=(%u,%u)-(%u,%u) "
           "centroid=(%.0f,%.0f) frame center=(%.0f,%.0f)\n",
           count, 100.0 * count / (static_cast<double>(w) * h), w, h,
           minX, minY, maxX, maxY, cx, cy, w / 2.0, h / 2.0);
    EXPECT_GT(count, 250000)
        << "replayed imdl content barely visible — threshold is a pinned "
           "fraction of the measured GREEN baseline, not a fudge factor";

    // ② WHERE：质心在视口中央带（±20%）——整体偏移 = 变换链缺陷（location/
    //    instances 载体缺口在"取景域=顶点域"下不可见，见文件头；此断言抓的
    //    是取景域内偏移）。
    EXPECT_LT(std::abs(cx - w / 2.0), w * 0.2) << "centroid x off-center";
    EXPECT_LT(std::abs(cy - h / 2.0), h * 0.2) << "centroid y off-center";
    // 四象限角为背景（角落探针 = 6% 边长方框，内容像素占比 ≤1%）。
    uint32_t const box = std::max(1u, std::min(w, h) * 6 / 100);
    EXPECT_LE(cornerContentRatio(frame, w, h, 0, 0, box), 0.01)
        << "content leaked into the top-left corner";
    EXPECT_LE(cornerContentRatio(frame, w, h, w - box, 0, box), 0.01)
        << "content leaked into the top-right corner";
    EXPECT_LE(cornerContentRatio(frame, w, h, 0, h - box, box), 0.01)
        << "content leaked into the bottom-left corner";
    EXPECT_LE(cornerContentRatio(frame, w, h, w - box, h - box, box), 0.01)
        << "content leaked into the bottom-right corner";

    // ③ 消费计数（该 dump 仅 1 瓦——两树中 0x28 是空树无瓦条目）。
    EXPECT_GE(readyTiles, 1) << "the replayed tile never produced graphics";
    EXPECT_GE(dispatched, 1u) << "expected the manifest tile to be dispatched";

    view.getUeViewport()->DropTiledGraphicsProvider(&mount->provider);
    view.close();
    spin(200);
}

// ---------------------------------------------------------------------------
// LOD 链消费锁：compatseed-v1 的 7 瓦链（-b-0..-b-6）经回放链到 graphics 提交。
// ---------------------------------------------------------------------------
// Authored: 见文件头。compatseed 的瓦内容全部带 instances 修饰（每瓦
// transformCenter -98.755 + 每实例变换；参考经 ImdlGraphicsCreator.getModifiers
// :243-257 → InstancedGraphicParams（InstancedGraphicParams.ts:20-37）→
// webgl System.createRenderGraphic/createInstancedGraphic（System.ts:518/:577）
// 按实例放置）；DanQing 未消费（**TD-25**）——几何落在局部量化盒
//（primitive decodedMin/Max ±0.25，JSON 实测）→ 原点附近 → 取景域
//（-100.005..-97.505）外 → 像素不可断言。TD-25 清偿时把像素判据加回本锁。
// 本锁钉住链上今天的真实消费面：
// ①请求键覆写：根键 = manifest 键域的 "-b-0-0-0-0-1"（IModelTileTree.ts:398
//   rootContentId 覆写——props 的 V1 形 id 不直接作为请求键）；
// ②字节回放：链上瓦按 manifest byteLength 精确回放（fetch 错误路径零触发）；
// ③协议选择级联：SSE refine → 链上 ≥2 瓦（-b-0 根 + -b-1 子）graphics 提交，
//   dispatch ≥ 2（SelectTiles 请求 → fetch → readContent → graphic）。
TEST(RpcDumpRender, CompatSeedReplaysLodChainToGraphicsReady)
{
    std::string dumpRoot = std::string(DANQING_TILE_ASSETS_DIR)
                           + "/rpc-dumps/compatseed-v1";
    if (char const* env = std::getenv("DANQING_RPC_DUMP"))
        dumpRoot = env;

    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "RpcDumpRender";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin(400);

    // 多树全量装载（本 dump 1 树——入口行为与多树同码路径）。
    auto mount = dta::mountDump(*view.getUeViewport(), dumpRoot);
    ASSERT_TRUE(mount.has_value()) << "mount failed: " << dumpRoot;
    ASSERT_EQ(1u, mount->manifest.trees.size());  // compatseed-v1：1 树 7 瓦
    ASSERT_EQ(7u, mount->manifest.tiles.size());
    std::string const treeId = mount->manifest.trees[0].treeId;
    auto props = mount->props->byTreeId(treeId);
    ASSERT_TRUE(props.has_value()) << "byTreeId failed: " << treeId;
    ASSERT_EQ(2424832u, props->metadata.formatVersion);  // 37.0（major<<16）

    // ① 请求键覆写（:398）：根键 = manifest 的 depth-0 条目。
    {
        std::string rootKey;
        for (auto const& t : mount->manifest.tiles)
            if (t.treeId == treeId && t.contentId.rfind("-b-0-0-0-0-1", 0) == 0)
                rootKey = t.contentId;
        ASSERT_FALSE(rootKey.empty()) << "no depth-0 entry in manifest";
        auto* root =
            static_cast<dqRender::ImdlTile*>(mount->trees[0]->getRootTile());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(rootKey, root->getContentId())
            << "root request key must be the manifest key (IModelTileTree.ts:398 "
               "rootContentId override)";
        EXPECT_EQ(treeId + "/" + rootKey, mount->trees[0]->contentUrl(rootKey));
    }

    view.getUeViewport()->AddTiledGraphicsProvider(&mount->provider);
    ASSERT_TRUE(view.getUeViewport()->HasTiledGraphicsProvider(&mount->provider));

    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        // 取景 = fitRange（树 contentRange 并集——单树即其域；iModelInfo 缺省
        // 回退面，instances60/本 dump 实态），30% 外扩（M-D(3) 既有参数）。
        view3d->LookAtVolume(dta::mountDumpFitVolume(mount->fitRange));
        view.getUeViewport()->InvalidateController();
    }
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    {
        auto* tool = new dqApp::StandardViewTool(view.getUeViewport(),
                                                 dqApp::StandardViewId::Iso);
        if (!tool->run())
            delete tool;
    }

    // 泵帧到 ≥2 瓦就绪（-b-0 根 + -b-1 子——SSE refine 的前两层）。
    uint32_t const dispatchedBefore =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests;
    long readyTiles = 0, totalTiles = 0;
    for (int i = 0; i < 120; ++i) {  // ≤12s 有界（7 瓦链逐级 refine）
        spin(100);
        readyTiles = 0;
        totalTiles = 0;
        for (auto& t : mount->trees)
            countGraphicsReady(t->getRootTile(), readyTiles, totalTiles);
        if (readyTiles >= 2 && i >= 8)
            break;
    }
    view.getUeViewport()->RenderFrame();
    uint32_t const dispatched =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests
        - dispatchedBefore;
    printf("[RPC-RENDER] graphics-ready tiles=%ld/%ld dispatched=%u\n",
           readyTiles, totalTiles, dispatched);

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    dumpBmp(frame, w, h,
            DANQING_TILE_ASSETS_DIR "/../../build/rpc-dump-compatseed.bmp");

    // ③ 协议选择级联：≥2 瓦 graphics 提交 + dispatch ≥2。
    EXPECT_GE(readyTiles, 2)
        << "expected at least root (-b-0) + one LOD child (-b-1) with graphics";
    EXPECT_GE(dispatched, 2u) << "expected at least 2 dispatched tile requests";

    // ② 字节回放完整性：链上瓦按 manifest byteLength 精确回放（fetch 的
    //    byteLength 不符走 onError → 瓦 NotFound → graphics 缺失——③的失败
    //    即含此因；这里再直接核对已就绪瓦的内容范围来自真实字节）。
    auto* root =
        static_cast<dqRender::ImdlTile*>(mount->trees[0]->getRootTile());
    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(root->hasGraphics()) << "root tile (-b-0) has no graphics";

    view.getUeViewport()->DropTiledGraphicsProvider(&mount->provider);
    view.close();
    spin(200);
}

// ---------------------------------------------------------------------------
// TD-25 清偿像素锁：instances60-v1 根瓦的 60 实例经 InstancedGraphicParams
// 链（ImdlGraphicsCreator.ts:243-257 getModifiers → InstancedGraphicParams →
// webgl System.ts:518/:577 createInstancedGraphic → InstancedGeometry.draw
// drawArraysInstanced）按实例变换放置上屏。
// ---------------------------------------------------------------------------
// Authored: 见文件头。修复前几何落在原点附近的局部量化盒（decodedMin/Max
// ±0.53）→ 取景域（树 contentRange -88.05..-69.89 域）外 → 零实例像素；
// 修复后 60 实例分布上屏。判据（§11.11，asymmetric marker = 逐实例
// symbologyOverrides 色 vs 上下文均匀绿——prim0 uniformColor 65280=绿，
// 实例 a_instanceRgba 逐实例覆盖（紫/红/蓝…，Color.ts:32-35 applyInstanceColor））：
// ①内容存活：着色实例像素（非绿 dominance）≥ 阈值——0 实例消费时恒 0；
// ②WHERE 方向性：着色像素的屏幕分布来自 transforms 实测世界分布
//  （实例世界盒 x[-84.1,-70.4] y[-41.0,-33.5]（transforms 12float/实例 解码
//   实测——task-3 报告取证），对上下文盒 x[-87.7,-85.5] 的展开）——断言
//   着色质心对绿质心的偏移 > 80px（首绿实测 407 的 0.2×，数据驱动钉值）；
// ③着色 bbox 展布 ≥ 阈值（60 实例 13.7m×7.5m 世界分布 → 屏幕显著展布，
//   单点/局部盒不可能满足）；
// ④消费计数：根瓦 graphics 提交 + dispatch ≥ 1。
TEST(RpcDumpRender, Instances60RendersAllInstances)
{
    std::string dumpRoot = std::string(DANQING_TILE_ASSETS_DIR)
                           + "/rpc-dumps/instances60-v1";
    if (char const* env = std::getenv("DANQING_RPC_DUMP"))
        dumpRoot = env;

    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "RpcDumpRender";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin(400);

    // 多树全量装载（本 dump 1 树 3587 瓦；iModelInfo 缺省——provenance 无
    // iModel 字段，fit 回退树 contentRange 并集，DumpMount 入口实态）。
    auto mount = dta::mountDump(*view.getUeViewport(), dumpRoot);
    ASSERT_TRUE(mount.has_value()) << "mount failed: " << dumpRoot;
    ASSERT_EQ(1u, mount->manifest.trees.size());  // instances60-v1：1 树
    ASSERT_EQ(3587u, mount->manifest.tiles.size());
    std::string const treeId = mount->manifest.trees[0].treeId;
    auto props = mount->props->byTreeId(treeId);
    ASSERT_TRUE(props.has_value()) << "byTreeId failed: " << treeId;
    ASSERT_EQ(2424832u, props->metadata.formatVersion);  // 37.0（major<<16）

    view.getUeViewport()->AddTiledGraphicsProvider(&mount->provider);
    ASSERT_TRUE(view.getUeViewport()->HasTiledGraphicsProvider(&mount->provider));

    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        // 取景 = fitRange（树 contentRange 并集——本 dump 单树；30% 外扩 +
        // zEps=1.0 同 mirukuru/compatseed 既有参数）。
        view3d->LookAtVolume(dta::mountDumpFitVolume(mount->fitRange, /*zEps=*/1.0));
        view.getUeViewport()->InvalidateController();
    }
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    // 不跑 StandardViewTool：其 view 过渡动画的落点对薄 z 盒 + 偏心内容的
    // 取景不稳（TD-25 取证：泵帧期间内容锚点从 ndc (-0.086,-0.056) 逐帧
    // 漂到 (-0.395,-2.011) 甩出取景）。TileTreeRender 像素锁成熟配方 =
    // LookAtVolume 直取（ImdlTilesetRendersRecordedFixture 同款）。

    // 泵帧到根瓦就绪（fetch → processCompleted → 失效级联 → 重绘；有界等待）。
    uint32_t const dispatchedBefore =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests;
    long readyTiles = 0, totalTiles = 0;
    for (int i = 0; i < 120; ++i) {  // ≤12s 有界
        spin(100);
        readyTiles = 0;
        totalTiles = 0;
        for (auto& t : mount->trees)
            countGraphicsReady(t->getRootTile(), readyTiles, totalTiles);
        if (readyTiles >= 1 && i >= 8)  // 根瓦就绪且至少跑了几帧重绘
            break;
    }
    view.getUeViewport()->RenderFrame();
    uint32_t const dispatched =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests
        - dispatchedBefore;
    printf("[RPC-RENDER] graphics-ready tiles=%ld/%ld dispatched=%u\n",
           readyTiles, totalTiles, dispatched);

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    dumpBmp(frame, w, h,
            DANQING_TILE_ASSETS_DIR "/../../build/rpc-dump-instances60.bmp");

    // 像素分类（§11.11 判别器校准）：先经背景差门（>90，同 contentStats——
    // 排除天空渐变域），再按通道 dominance 分绿（prim0 均匀绿 65280）与
    // 着色（实例 symbologyOverrides 色）。天空蓝 (142,205,255)：g 高 →
    // 不构成 dominance；差门亦排除。
    uint8_t const* bg = &frame[0];
    long colored = 0, green = 0;
    double ccx = 0.0, ccy = 0.0, gcx = 0.0, gcy = 0.0;
    uint32_t cMinX = w, cMaxX = 0, cMinY = h, cMaxY = 0;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t const* p = &frame[(static_cast<size_t>(y) * w + x) * 4u];
            int const dr = std::abs(p[0] - bg[0]);
            int const dg = std::abs(p[1] - bg[1]);
            int const db = std::abs(p[2] - bg[2]);
            if (dr + dg + db <= 90)
                continue;  // 背景域
            int const r = p[0], g = p[1], b = p[2];
            if (g > r + 20 && g > b + 20) {
                ++green;
                gcx += x; gcy += y;
            } else if (r > g + 30 || b > g + 30) {
                ++colored;
                ccx += x; ccy += y;
                if (x < cMinX) cMinX = x;
                if (x > cMaxX) cMaxX = x;
                if (y < cMinY) cMinY = y;
                if (y > cMaxY) cMaxY = y;
            }
        }
    }
    if (colored > 0) { ccx /= colored; ccy /= colored; }
    if (green > 0) { gcx /= green; gcy /= green; }
    printf("[RPC-RENDER] colored=%ld green=%ld coloredCentroid=(%.0f,%.0f) "
           "greenCentroid=(%.0f,%.0f) coloredBbox=(%u,%u)-(%u,%u) frame=%ux%u\n",
           colored, green, ccx, ccy, gcx, gcy, cMinX, cMinY, cMaxX, cMaxY, w, h);

    // ① 内容存活：着色实例像素 ≥ 20000（首绿实测 123069 的 0.16×——0 实例
    //    消费恒 0、个位数实例 ~2k 亦红；阈值是首绿实测的钉死比例，非调参）。
    EXPECT_GE(colored, 20000l)
        << "no instanced content rendered — instances modifier not consumed "
           "(TD-25 regression)";
    // 上下文 prim0（绿墙）持续上屏的轻量锚（非本锁主判据——防"全帧蒸发"
    // 类故障被着色判据掩盖）。
    EXPECT_GE(green, 1000l) << "context primitive (prim0) no longer renders";

    // ③ WHERE 展布：60 实例世界分布 13.7m×7.5m（transforms 实测）→ 屏幕
    //    着色 bbox 显著展布（首绿实测 976×466——单点/局部盒不可能满足，
    //    修复前几何即局部盒；阈值 ≈ 首绿的 0.4×）。
    if (colored > 0) {
        EXPECT_GE(cMaxX - cMinX, 400u) << "colored content not spread in x";
        EXPECT_GE(cMaxY - cMinY, 150u) << "colored content not spread in y";
        // ② WHERE 方向性：着色质心对绿质心的偏移（实例世界盒 x[-84.1,-70.4]
        //    y[-41.0,-33.5] 相对上下文盒 x[-87.7,-85.5] y[-37.8,-34.3] 展开
        //    的屏幕投影——首绿实测 delta=(239.7,167.7)（|d|₁=407）；
        //    阈值 80 ≈ 首绿的 0.2×）。
        double const dx = ccx - gcx;
        double const dy = ccy - gcy;
        printf("[RPC-RENDER] centroid delta=(%.1f,%.1f)\n", dx, dy);
        EXPECT_GT(std::abs(dx) + std::abs(dy), 80.0)
            << "colored centroid must be offset from the green context centroid "
               "(instances spread away from the context chunk)";
        // 角落探针：内容在帧中央带（同 mirukuru 锁的角落背景断言）。
        uint32_t const box = std::max(1u, std::min(w, h) * 6 / 100);
        EXPECT_LE(cornerContentRatio(frame, w, h, 0, 0, box), 0.01)
            << "content leaked into the top-left corner";
        EXPECT_LE(cornerContentRatio(frame, w, h, w - box, 0, box), 0.01)
            << "content leaked into the top-right corner";
        EXPECT_LE(cornerContentRatio(frame, w, h, 0, h - box, box), 0.01)
            << "content leaked into the bottom-left corner";
        EXPECT_LE(cornerContentRatio(frame, w, h, w - box, h - box, box), 0.01)
            << "content leaked into the bottom-right corner";
    }

    // ④ 消费计数：根瓦 graphics 提交 + 至少 1 次 dispatch。
    EXPECT_GE(readyTiles, 1) << "the replayed root tile never produced graphics";
    EXPECT_GE(dispatched, 1u) << "expected the manifest tile to be dispatched";

    view.getUeViewport()->DropTiledGraphicsProvider(&mount->provider);
    view.close();
    spin(200);
}

// ---------------------------------------------------------------------------
// 完整加载 E2E 对账锁（M-E Task 4）：instances60-v1 全树前缀加载——驱动
// selectTiles 至静默后，DumpTileFetcher 侧记录的全部请求键与 manifest 键集合
// 逐一对账：凡 manifest 有键且被请求的瓦全部消费（Completed → graphics），
// 对不上（NotFound）的键全部是 V4 细分派生形——即 TD-25/task-3 登记的子瓦
// contentId 派生链差异（DanQing computeImdlChildTileProps 的 "-b-d-i-j-k-1"
// vs 真实后端采集键域的尾段 quadrant/magnification 形），非链断。
// ---------------------------------------------------------------------------
// Authored: 见文件头。判据设计（不盲目 ready==manifest.tiles.size——子瓦派生
// 链差异使绝大多数采集键不在今天派生链的可达面内，盲目全等会把已登记的
// 派生差异烤成失败）：
// ① 零 Error：请求日志里无文件缺失/byteLength 不符/畸形 url（dump 资产
//    自洽性破口必红）；
// ② 零重复：每键至多一条请求（TileAdmin 非 NotLoaded 不重建请求的门）；
// ③ 链通：Completed 键 ⊆ manifest 键集，且根键 "-b-0-0-0-0-1" 在其中
//    （IModelTileTree.ts:398 rootContentId 覆写——V4 形根键命中采集域）；
// ④ 差异形：requested \ manifest 的每个键都是 "-b-<d>-<i>-<j>-<k>-1"
//    （V4 细分派生、mult=1、d≥1）——不含采集域的尾段 -{2,4,8} 形/畸形键；
// ⑤ 树侧终态全消费：树内每个瓦——contentId ∈ manifest 命中集 → hasGraphics()；
//    否则（派生 miss）→ loadStatus == NotFound。无 Loading/Queued/NotLoaded
//    残留（泵至静默的"全部就绪"语义：在途清零）；
// ⑥ 计数对账：|Completed| == 树侧 graphics 瓦数（每个完成的请求都被消费）；
// ⑦ 覆盖率报告（printf 非断言）：命中/3587——首绿实测 1/3587（唯一非空
//    octant 的子键亦落采集域外）——今天派生链可达键域的占比，其余键域可达
//    性归子瓦派生链任务（与 TD-25 遗留呼应，不判失败）；
// ⑧ 像素锚：消费的根瓦上屏（内容 ≥ 20000 + 绿上下文 ≥ 1000——首绿实测
//    231141/56399 的下界钉值）。
// ⑨ dispatch ≥ 1。
TEST(RpcDumpRender, Instances60FullLoadReconcilesManifestKeys)
{
    std::string dumpRoot = std::string(DANQING_TILE_ASSETS_DIR)
                           + "/rpc-dumps/instances60-v1";
    if (char const* env = std::getenv("DANQING_RPC_DUMP"))
        dumpRoot = env;

    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "RpcDumpRender";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin(400);

    // 多树全量装载（本 dump 1 树 3587 瓦——全树前缀加载的对账输入）。
    auto mount = dta::mountDump(*view.getUeViewport(), dumpRoot);
    ASSERT_TRUE(mount.has_value()) << "mount failed: " << dumpRoot;
    ASSERT_EQ(1u, mount->manifest.trees.size());
    ASSERT_EQ(3587u, mount->manifest.tiles.size());
    ASSERT_NE(nullptr, mount->fetcher);
    std::string const treeId = mount->manifest.trees[0].treeId;

    view.getUeViewport()->AddTiledGraphicsProvider(&mount->provider);
    ASSERT_TRUE(view.getUeViewport()->HasTiledGraphicsProvider(&mount->provider));

    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        // 取景 = fitRange（树 contentRange 并集；30% 外扩 + zEps=1.0 同
        // Instances60RendersAllInstances 的成熟配方——不跑 StandardViewTool）。
        view3d->LookAtVolume(dta::mountDumpFitVolume(mount->fitRange, /*zEps=*/1.0));
        view.getUeViewport()->InvalidateController();
    }
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});

    // 泵至静默：根瓦就绪 + 请求日志尺寸连续 6 次迭代不变 + 投递队列排空
    // （每键至多请求一次——日志稳定即"想要的全要了"；有界 20s）。
    uint32_t const dispatchedBefore =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests;
    long readyTiles = 0, totalTiles = 0;
    size_t lastLogSize = 0;
    int stable = 0, quiesceIter = -1;
    for (int i = 0; i < 200; ++i) {
        spin(100);
        readyTiles = 0;
        totalTiles = 0;
        for (auto& t : mount->trees)
            countGraphicsReady(t->getRootTile(), readyTiles, totalTiles);
        size_t const logSize = mount->fetcher->requestLog().size();
        if (readyTiles >= 1 && !mount->fetcher->requestLog().empty()
            && logSize == lastLogSize
            && mount->fetcher->getActiveCount() == 0) {
            if (++stable >= 6) {
                quiesceIter = i;
                break;
            }
        } else {
            stable = 0;
        }
        lastLogSize = logSize;
    }
    ASSERT_GE(quiesceIter, 0)
        << "tile load never quiesced — in-flight requests remain (log="
        << mount->fetcher->requestLog().size() << " ready=" << readyTiles << ")";
    view.getUeViewport()->RenderFrame();
    uint32_t const dispatched =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests
        - dispatchedBefore;

    // --- 对账：请求日志 vs manifest 键集合 ---
    std::set<std::string> manifestKeys;
    for (auto const& t : mount->manifest.tiles)
        manifestKeys.insert(t.treeId + "/" + t.contentId);

    auto const& log = mount->fetcher->requestLog();
    std::set<std::string> requestedKeys;
    size_t numCompleted = 0, numNotFound = 0, numError = 0;
    std::vector<std::string> misses;  // requested \ manifest（treeId/contentId）
    for (auto const& rec : log) {
        std::string const key = rec.treeId + "/" + rec.contentId;
        requestedKeys.insert(key);
        switch (rec.outcome) {
        case dqApp::DumpTileFetcher::DumpFetchOutcome::Completed:
            ++numCompleted;
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
    printf("[RPC-RENDER] full-load reconcile: requested=%zu (completed=%zu "
           "notFound=%zu error=%zu) manifest=%zu coverage=%zu/%zu quiesceIter=%d "
           "ready=%ld/%ld dispatched=%u\n",
           log.size(), numCompleted, numNotFound, numError, manifestKeys.size(),
           requestedKeys.size() - misses.size(), manifestKeys.size(),
           quiesceIter, readyTiles, totalTiles, dispatched);

    // ① 零 Error（dump 资产自洽性破口）。
    EXPECT_EQ(0u, numError) << "dump asset integrity break (missing file / "
                               "byteLength mismatch / malformed url)";
    // ② 零重复请求（每键至多一条）。
    EXPECT_EQ(requestedKeys.size(), log.size())
        << "duplicate requests for the same key — re-request thrash";
    // ③ 链通：Completed 键 ⊆ manifest，根键在其中。
    for (auto const& rec : log) {
        if (rec.outcome != dqApp::DumpTileFetcher::DumpFetchOutcome::Completed)
            continue;
        EXPECT_TRUE(manifestKeys.count(rec.treeId + "/" + rec.contentId) > 0)
            << "Completed key not in manifest: " << rec.contentId;
    }
    EXPECT_GT(numCompleted, 0u) << "no manifest key was ever requested";
    // 首绿实测钉值（2026-09-28）：链通集恰 = 根键一个——contentRange 裁剪后
    // 唯一非空 octant 的子键 "-b-1-0-0-0-1" 落采集域外（采集域 depth-1 =
    // "-b-1-0-0-0-{2,4,8}"，即 TD-25/task-3 登记的子瓦派生链差异实形）。
    // 归链任务修复派生后此处随实测更新（期望：miss 缩小/消失、覆盖率上升）。
    EXPECT_EQ(1u, numCompleted) << "unexpected chain-connected set — reconcile "
                                   "the new consumption face consciously";
    // ④ 差异形：每个 miss 都是 "-b-<d>-<i>-<j>-<k>-1"（V4 细分派生、mult=1）。
    for (auto const& m : misses) {
        bool derivedForm = false;
        if (m.rfind("-b-", 0) == 0) {
            // 段：['', 'b', d, i, j, k, mult] —— mult 恒 "1"、d ≥ 1。
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
            derivedForm = segs.size() == 7 && segs[1] == "b" && segs[6] == "1"
                          && segs[2] != "0";
        }
        EXPECT_TRUE(derivedForm)
            << "miss key is not a V4 subdivision derivation: " << m
            << " (collection-domain shape leaked or malformed)";
    }
    // 差异键实钉：今天派生链的唯一 miss 即 task-3 登记的 "-b-1-0-0-0-1"
    //（vs 采集域 "-b-1-0-0-0-{2,4,8}"——真实后端尾段 quadrant/magnification
    // 编号差异）。派生链修复后本断言随新实测更新。
    ASSERT_EQ(1u, misses.size());
    EXPECT_EQ("-b-1-0-0-0-1", misses[0])
        << "the registered derivation-difference key changed — update the pin "
           "with the fixed derivation's measured shape";

    // ⑤ 树侧终态全消费：manifest 命中键 → hasGraphics；miss 键 → NotFound。
    std::map<std::string, dqRender::Tile*> tilesByKey;
    std::function<void(dqRender::Tile*)> walk = [&](dqRender::Tile* tile) {
        if (!tile)
            return;
        auto* imdl = static_cast<dqRender::ImdlTile*>(tile);
        tilesByKey[treeId + "/" + imdl->getContentId()] = tile;
        for (dqRender::Tile* child : tile->getChildren())
            walk(child);
    };
    for (auto& t : mount->trees)
        walk(t->getRootTile());
    long graphicsTiles = 0, terminalMisses = 0, inFlight = 0;
    for (auto const& kv : tilesByKey) {
        bool const hit = manifestKeys.count(kv.first) > 0;
        if (hit) {
            if (kv.second->hasGraphics())
                ++graphicsTiles;
            else
                ++inFlight;  // manifest 键已请求完成却未消费成 graphics
        } else {
            if (kv.second->getLoadStatus() == dqRender::TileLoadStatus::NotFound)
                ++terminalMisses;
            else
                ++inFlight;  // 派生 miss 未达终态
        }
    }
    printf("[RPC-RENDER] tree-side: tiles=%zu graphics=%ld terminalMiss=%ld "
           "inFlight=%ld\n", tilesByKey.size(), graphicsTiles, terminalMisses,
           inFlight);
    EXPECT_EQ(0l, inFlight) << "tiles left in non-terminal state at quiescence";
    // ⑥ 计数对账：每个 Completed 请求都被消费成 graphics。
    EXPECT_EQ(static_cast<long>(numCompleted), graphicsTiles)
        << "completed requests without consumed graphics (readContent drop?)";

    // ⑧ 像素锚：消费的根瓦上屏（阈值 = 首绿实测下界，见测试头）。
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    dumpBmp(frame, w, h,
            DANQING_TILE_ASSETS_DIR "/../../build/rpc-dump-instances60-full.bmp");
    long count = 0;
    double cx = 0, cy = 0;
    uint32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
    ASSERT_TRUE(contentStats(frame, w, h, count, cx, cy, minX, maxX, minY, maxY));
    long const green = countGreenPixels(frame, w, h);
    printf("[RPC-RENDER] content=%ld px green=%ld px\n", count, green);
    EXPECT_GE(count, 20000l) << "consumed root tile not rendered — load chain "
                                "disconnected from draw";
    EXPECT_GE(green, 1000l) << "context primitive (green wall) not rendered";
    // ⑨ dispatch ≥ 1。
    EXPECT_GE(dispatched, 1u);

    view.getUeViewport()->DropTiledGraphicsProvider(&mount->provider);
    view.close();
    spin(200);
}

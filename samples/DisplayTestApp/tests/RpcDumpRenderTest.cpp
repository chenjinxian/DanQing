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
    long long sx = 0, sy = 0;  // long long：内容 >2.1M px 时 32-bit long 累加溢出
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
        // BI_RGB 32bpp 的文件字节序是 BGRX，而 ReadFrameForTest 回读的内存帧是
        // RGBA（glReadPixels GL_RGBA——TargetImpl::readColorData）。逐像素交换
        // R/B 后落盘，dump 才是真彩色。M-J(3) 取证教训（§11.11 仪器自检）：
        // 此前 RGBA 字节直写 BGRX 文件，所有历史 dump 的 R/B 通道互换——
        // M-J(1) 的"实例球行色发散"（前排蓝 + teal 球 + 黄/橙行缺失）即该
        // 仪器伪影（mi5-inst-1-saved.png 真彩桌面截图同view六行全对；换算后
        // dump 的蓝/青/缺行 = 红/黄/橙行的 R/B 互换），非引擎缺陷。
        for (uint32_t x = 0; x < w; ++x)
            std::swap(row[x * 4 + 0], row[x * 4 + 2]);
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
// ②字节回放：链上瓦按 manifest byteLength 精确回放（fetch 错误路径零触发）
//   ——M-G(2) 起证据钉实际消费面（Completed ⊆ manifest + 最深在域键 -b-6
//   实钉 + Completed→graphics 对账），原"root->hasGraphics"直查随
//   maxInitialTilesToSkip=6 载体接线演化（根恒 canSkip 不被钻取视图请求，
//   参考同构——见锁内"M-G(2) 配方订正"）；
// ③协议选择级联：SSE refine → 链上 ≥2 瓦 graphics 提交（M-G(2) 后实测 =
//   钻取层 d6（跳级预算底 :329-331）+ fit 层 d5（Visible）），dispatch ≥ 2
//   （SelectTiles 请求 → fetch → readContent → graphic）。
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
    // M-F(1) 确定性钻取：采集 manifest 的细分链是参考前端钻取（zoom-in）时
    // 逐级 Visible→insertMissing 的产物；本锁只要求 ≥2 瓦就绪，故一步缩到
    // d1/d2 均 Visible 的视距（fit 视距下 d1=20821px > 2048 TooCoarse、其子
    // 出 frustum——SelectParent 协议下无请求面；模拟采集的钻取 mid-point）。
    // 不跑 StandardViewTool（Instances60FullLoad 成熟配方——避免
    // animateFrustumChange 的动画期未稳视距；LookAtVolume+synch 即刻 settle）。
    //
    // M-G(2) 配方订正（maxInitialTilesToSkip=6 props 载体接线——TileProps.ts:63
    // → IModelTileTree.ts:390；实测驱动，机制全部实钉）：
    //  ①根恒 canSkip（depth 0<6），钻取视图不再经预算路径请求根（参考同构
    //    ——活体采集的 d0 字节来自采集初期根 Visible 视图 + wrapper 竞态窗
    //    口，dump README 登记）；
    //  ②相序必须"钻取先于 fit"：本 dump 每张瓦的 imdl header 都解出
    //    sizeMultiplier=1.0（TD-25/TD-24 域同型），瓦内容到达后其子代派生
    //    恒走**放大分支**（TileMetadata.ts:785-799）——细分子 d6 只能在
    //    d5 **内容到达前**派生（:282 loadChildren 用瓦的当前元数据）。若
    //    fit 先行，d5 在 fit 级就绪 → d6 永不可达（d5 的子代只有放大子
    //    ——实测：fit 先行的日志 = [d5, -b-5-0-0-0-{8,4,2} 放大过冲]，d6
    //    缺席）。故相 1 = kZoom=12 钻取（d5 内容未达 → 细分派生 d6 → d6
    //    达跳级预算底（depth=6=maxInitial）→ :329-331 预算路径被请求
    //    Completed；d6 内容到达后 :148 门（children>1 → disposeChildren）
    //    使子代从细分子重派生为放大子 → 过冲 miss 两枚——-b-6-0-0-0-4 先
    //    于 -b-6-0-0-0-2 入日志：m2 TooCoarse 同帧派生 m4 Visible 被请求，
    //    m2 自身经 :299-301 回退后补请求），相 2 = 回 fit（既有 d5 瓦转
    //    Visible → insertMissing → Completed）——两相合计 d6+d5 两瓦就绪，
    //    "≥2 瓦 graphics 提交"的锁目不变。活体采集的 d0..d6 全细分链即
    //    "zoom 泵逐级赶在内容到达前"的同机制产物（采集泵 12s/级 vs 后端
    //    生成时延）；本锁两相即足够钉住链消费面，不追求复刻全链时序。
    //  ③原 "20821/12≈1735（d1 Visible）" 的估算随实测订正（d1 在 kZoom=12
    //    仍 TooCoarse——canSkip 穿透直到预算底）。
    // 泵至静默（强制选择帧形——NotFound 交付不触发失效级联，:299-301 回退
    // 在突发帧停后饥饿（M-F(2) 相 1 实测——见 Instances60FullLoadReconciles-
    // ManifestKeys 泵注）；强制帧把请求面推到不动点，键级幂等不改变请求面）。
    uint32_t const dispatchedBefore =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests;
    long readyTiles = 0, totalTiles = 0;
    auto pumpToQuiesce = [&]() -> int {
        size_t lastLogSize = 0;
        int stable = 0;
        for (int i = 0; i < 200; ++i) {
            view.getUeViewport()->InvalidateController();
            spin(100);
            readyTiles = 0;
            totalTiles = 0;
            for (auto& t : mount->trees)
                countGraphicsReady(t->getRootTile(), readyTiles, totalTiles);
            size_t const logSize = mount->fetcher->requestLog().size();
            if (!mount->fetcher->requestLog().empty()
                && logSize == lastLogSize
                && mount->fetcher->getActiveCount() == 0) {
                if (++stable >= 6)
                    return i;
            } else {
                stable = 0;
            }
            lastLogSize = logSize;
        }
        return -1;
    };

    // 相 1：kZoom=12 钻取泵至静默（d6 预算路径 + 放大过冲 miss 的不动点；
    // 首帧即钻取视图——初始突发帧在 LookAtVolume(k12) 之后经泵触发，d5 在
    // fit 视图无选择帧窗口，内容不达 → d6 细分派生不受 :148 门污染）。
    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        dqGeom::Range3d const fit = dta::mountDumpFitVolume(mount->fitRange);
        auto const center = fit.Center();
        double constexpr kZoom = 12.0;
        dqGeom::Range3d const zoomed = dqGeom::Range3d::CreateXYZXYZ(
            center.x - (center.x - fit.low.x) / kZoom,
            center.y - (center.y - fit.low.y) / kZoom,
            center.z - (center.z - fit.low.z) / kZoom,
            center.x + (fit.high.x - center.x) / kZoom,
            center.y + (fit.high.y - center.y) / kZoom,
            center.z + (fit.high.z - center.z) / kZoom);
        view3d->LookAtVolume(zoomed);
        view.getUeViewport()->InvalidateController();
        view.getUeViewport()->synchWithView(
            dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    }
    int const quiesceDrill = pumpToQuiesce();
    ASSERT_GE(quiesceDrill, 0)
        << "drill-phase tile load never quiesced (log="
        << mount->fetcher->requestLog().size() << " ready=" << readyTiles << ")";

    // 相 2：回 fit 泵至静默（既有 d5 瓦转 Visible → insertMissing →
    // Completed——第二枚就绪瓦；d5 内容到达时其子代已是 [d6]（相 1 细分派
    // 生），:148 ≤1 门保留，无放大重派生）。
    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        view3d->LookAtVolume(dta::mountDumpFitVolume(mount->fitRange));
        view.getUeViewport()->InvalidateController();
        view.getUeViewport()->synchWithView(
            dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    }
    int const quiesceFit = pumpToQuiesce();
    ASSERT_GE(quiesceFit, 0)
        << "fit-phase tile load never quiesced (log="
        << mount->fetcher->requestLog().size() << " ready=" << readyTiles << ")";
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
    for (auto const& rec : mount->fetcher->requestLog())
        printf("[RPC-RENDER]   compatseed-log %d %s\n",
               static_cast<int>(rec.outcome), rec.contentId.c_str());
    EXPECT_GE(readyTiles, 2)
        << "expected at least two LOD-chain tiles with graphics";
    EXPECT_GE(dispatched, 2u) << "expected at least 2 dispatched tile requests";

    // ② 字节回放完整性（M-G(2) 演化——原"root->hasGraphics"直查：根恒
    //    canSkip 不被钻取视图请求（参考同构，见上方配方订正），字节回放证
    //    据改钉实际消费面）：fetch 的 byteLength 不符走 onError → 瓦
    //    NotFound；故 (a) 每个 Completed 键 ∈ manifest（请求键覆写命中采集
    //    键域）；(b) 最深在域键 -b-6-0-0-0-1 ∈ Completed（钻取层预算路径
    //    消费——字节回放的最深证据）；(c) Completed 键树侧全消费成
    //    graphics（计数对账）；(d) miss 全为 V4 请求键形（细分/放大派生）。
    std::set<std::string> manifestKeys;
    for (auto const& t : mount->manifest.tiles)
        manifestKeys.insert(t.treeId + "/" + t.contentId);
    size_t numCompleted = 0, numNotFound = 0, numError = 0;
    std::set<std::string> completedIds;
    std::vector<std::string> misses;
    for (auto const& rec : mount->fetcher->requestLog()) {
        switch (rec.outcome) {
        case dqApp::DumpTileFetcher::DumpFetchOutcome::Completed:
            ++numCompleted;
            completedIds.insert(rec.contentId);
            EXPECT_TRUE(manifestKeys.count(rec.treeId + "/" + rec.contentId) > 0)
                << "Completed key not in manifest: " << rec.contentId;
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
    EXPECT_EQ(0u, numError) << "dump asset integrity break";
    EXPECT_TRUE(completedIds.count("-b-6-0-0-0-1") > 0)
        << "deepest in-manifest key -b-6-0-0-0-1 never connected — budget-path "
           "consumption at the skip floor broken";
    // Completed 恰两枚（d6 钻取层预算路径 + d5 fit 层 Visible——相序与机制
    // 见上方配方订正②）。
    EXPECT_EQ(2u, numCompleted) << "unexpected chain-connected set";
    for (auto const& m : misses) {
        bool derivedForm = false;
        if (m.rfind("-b-", 0) == 0) {
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
            bool allHex = segs.size() == 7;
            if (allHex) {
                for (size_t s = 2; s < 7; ++s) {
                    if (segs[s].empty()
                        || segs[s].find_first_not_of("0123456789abcdef")
                           != std::string::npos) {
                        allHex = false;
                        break;
                    }
                }
            }
            derivedForm = allHex && segs[1] == "b" && segs[6].size() <= 2
                          && (segs[2] != "0" || m == "-b-0-0-0-0-1");
        }
        EXPECT_TRUE(derivedForm)
            << "miss key is not a V4 request-key shape: " << m;
    }
    // miss 清单实钉（相 1 钻取层实测——d6 内容到达后 :148 门重派生的放大
    // 过冲，机制见配方订正②；采集域只含细分链 d0..d6，放大键全域外）。
    ASSERT_EQ(2u, misses.size());
    EXPECT_EQ("-b-6-0-0-0-4", misses[0]);
    EXPECT_EQ("-b-6-0-0-0-2", misses[1]);
    // Completed → graphics 计数对账（每个完成的请求都被消费）。
    EXPECT_EQ(static_cast<long>(numCompleted), readyTiles)
        << "completed requests without consumed graphics (readContent drop?)";

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
    // M-G(2) 增补：每迭代强制 InvalidateController——maxInitialTilesToSkip=3
    // 载体接线后，根本相不再经预算路径被请求（:265 深度<3 恒 canSkip），而
    // 经 :299-301 回退（-b-1 NotFound 终态 → 请求父）；NotFound 交付不触发
    // 失效级联，不强制选择帧时回退饥饿（M-F(2) 相 1 实测停 20s——见
    // Instances60FullLoadReconcilesManifestKeys 泵注）。就绪后帧内容 = 根瓦
    // （同字节同视图——像素锚形态与 M-E 原始钉值同源）。
    uint32_t const dispatchedBefore =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests;
    long readyTiles = 0, totalTiles = 0;
    for (int i = 0; i < 120; ++i) {  // ≤12s 有界
        view.getUeViewport()->InvalidateController();
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
    //    复测登记（2026-09-28 M-G(2) 终审）：colored=139796（首绿的 1.136×，
    //    隔离 ×2 确定性复现）——漂移窗口含 M-F setContent 归位与本任务
    //    maxInitialTilesToSkip 接线，源未归因；阈值/bbox/质心锚余量充足，
    //    请求键/字节钉值不变（§11.11 实测登记，非判据调参）。
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
// 完整加载 E2E 对账锁（M-E Task 4；M-F Task 2 两相驱动升级）：instances60-v1
// 全树前缀加载——驱动 selectTiles 至静默后，DumpTileFetcher 侧记录的全部请求
// 键与 manifest 键集合逐一对账。
//
// 【M-F Task 2 取证结论（实测驱动，替代 M-E 的"派生链差异"假设）】
// 本 dump 的 manifest 键域是仓外 sweep.js **合成 BFS 采集面**的产物，不是
// 任何单一视口驱动的请求面（README/provenance 在案：generateTileContent
// 鸭子类型逐键请求 + 页内 computeChildTileProps 派生子瓦 + 放大 cap=×8 +
// BFS 未完成前缀）。两个结构性事实决定视口面对本域的连接上限：
//  (a) 放大子与父**同范围**（TileMetadata.ts:785-799）——父 TooCoarse 时
//      放大子同尺寸也 TooCoarse、永不 Visible；而 hasSizeMultiplier 使
//      isNotReady 为假（IModelTile.ts:272）不耗跳级预算——放大脊在视口
//      面是"无请求的canSkip穿透"（参考同构）。故采集域的 depth-0 放大键
//      "-b-0-0-0-0-{2,4,8}" 是 BFS 直接请求的产物，**任何视口驱动都不会
//      请求它们**（M-F Task 2 选择轨迹实证：冷启动 fit 泵帧全程只请求
//      根 + 可见子 "-b-1-0-0-0-1"）。
//  (b) 细分子的 mult **继承父键**（TileMetadata.ts:841-846 childSpec 展开
//      parentSpec——DanQing ImdlTileTree.cpp 逐行同构）。冷启动根的首次
//      loadChildren 在内容到达前以 mult=0 细分（两实现同构——参考的
//      computeChildTileProps 在 loadChildren() 调用点同步执行），产
//      mult=1 子；setContent :148 的 disposeChildren 门要求 children>1
//      （唯一幸存子被保留，参考同构）→ 冷 fit 面请求 "-b-1-0-0-0-1"——
//      该键不在采集域（sweep 从未细分 mult=1 父：根 descMult=1 恒走放大）。
// 因此本锁分两相驱动，把**视口面可达的链消费**钉全：
//   相 1（冷 fit 全量装载）：根 "-b-0-0-0-0-1" 链通（Completed），子
//      "-b-1-0-0-0-1" miss（域外，形验记录）。
//   相 2（确定性钻取 kZoom=3，CompatSeed 成熟配方同款）："-b-1-0-0-0-1"
//      转 TooCoarse → canSkip 下潜（:272 预算 0→1）→ 其 mult=1 细分子
//      预算耗尽被请求（:329-331）→ "-b-2-0-0-0-1" 命中采集域（Completed
//      → graphics）——派生子瓦请求→字节→graphics 的 E2E 直接证据。
// ---------------------------------------------------------------------------
// Authored: 见文件头。判据设计（不盲目 ready==manifest.tiles.size——采集域
// 是 BFS 面产物（上），盲目全等会把采集面差异烤成失败）：
// ① 零 Error：请求日志里无文件缺失/byteLength 不符/畸形 url（dump 资产
//    自洽性破口必红）；
// ② 零重复：每键至多一条请求（TileAdmin 非 NotLoaded 不重建请求的门）；
// ③ 链通：Completed 键 ⊆ manifest 键集，且根键 "-b-0-0-0-0-1" 在其中
//    （IModelTileTree.ts:398 rootContentId 覆写——V4 形根键命中采集域）；
//    M-F(2) 新增："-b-2-0-0-0-1" ∈ Completed（相 2 钻取消费的非根派生键——
//    采集域内 depth-2 唯一 mult=1 键）；
// ④ 差异形：requested \ manifest 的每个键都是 "-b-<d>-<i>-<j>-<k>-<mult>"
//    形（V4 派生形；mult 段为 hex——细分继承父 mult、放大为 2 的幂，参考
//    TileMetadata.ts:633-636/:841-846）；已知域外键两枚实钉（见下）；
// ⑤ 树侧终态全消费：树内每个瓦——contentId ∈ manifest 命中集 → hasGraphics()；
//    否则（派生 miss）→ loadStatus == NotFound。无 Loading/Queued/NotLoaded
//    残留（泵至静默的"全部就绪"语义：在途清零）；
// ⑥ 计数对账：|Completed| == 树侧 graphics 瓦数（每个完成的请求都被消费）；
// ⑦ 覆盖率报告（printf 非断言）：命中/3587——首绿实测 1/3587；M-F(2) 两相
//    实测 2/3587（相 2 的 "-b-2-0-0-0-1" 命中）。其余 3585 键的域内可达性
//    受采集面 (a)(b) 限制（见锁头取证结论），非引擎通路缺口；
// ⑧ 像素锚：消费的瓦上屏（内容 + 绿上下文 ≥ 首绿实测下界，见断言处）；
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

    // 泵至静默的公共形态：请求日志尺寸连续 6 次迭代不变 + 投递队列排空
    // （每键至多请求一次——日志稳定即"想要的全要了"；有界 20s/相）。
    // M-G(2) 增补：每迭代强制 InvalidateController——NotFound 交付不触发
    // 失效级联（无 graphic），突发帧停后 :299-301 回退（子 NotFound → 请求
    // 父——maxInitialTilesToSkip=3 载体接线（M-G(2)）后根本相 1 即经此回退
    // 被请求）等"需下一选择帧"的机制会饥饿（本锁相 1 实测：log={-b-1} 停
    // 20s——取证见 M-G(2) 任务报告）；强制选择帧把请求面推到不动点（请求键
    // 由视图×瓦状态决定，强制帧只提高求值频率，键级幂等不改变请求面）。
    uint32_t const dispatchedBefore =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests;
    long readyTiles = 0, totalTiles = 0;
    auto pumpToQuiesce = [&]() -> int {
        size_t lastLogSize = 0;
        int stable = 0;
        for (int i = 0; i < 200; ++i) {
            view.getUeViewport()->InvalidateController();
            spin(100);
            readyTiles = 0;
            totalTiles = 0;
            for (auto& t : mount->trees)
                countGraphicsReady(t->getRootTile(), readyTiles, totalTiles);
            size_t const logSize = mount->fetcher->requestLog().size();
            if (readyTiles >= 1 && !mount->fetcher->requestLog().empty()
                && logSize == lastLogSize
                && mount->fetcher->getActiveCount() == 0) {
                if (++stable >= 6)
                    return i;
            } else {
                stable = 0;
            }
            lastLogSize = logSize;
        }
        return -1;
    };

    // 相 1：冷 fit 全量装载泵至静默（选择轨迹实证面：根 TooCoarse 请求 +
    // 其唯一细分子 Visible 请求；见锁头取证结论）。
    int quiesceIter = pumpToQuiesce();
    ASSERT_GE(quiesceIter, 0)
        << "phase-1 tile load never quiesced — in-flight requests remain (log="
        << mount->fetcher->requestLog().size() << " ready=" << readyTiles << ")";

    // 相 2：确定性钻取（CompatSeed 成熟配方同款——不跑 StandardViewTool，
    // LookAtVolume+synch 即刻 settle）：fit 体积向心缩 3× → "-b-1-0-0-0-1"
    // 由 Visible 转 TooCoarse（fit 下其 screenSize ≤ 2048，3× 后 ∈
    // (3072,6144] 必超阈值）→ canSkip 下潜 → 其 mult=1 细分子预算耗尽被
    // 请求（:329-331）→ "-b-2-0-0-0-1" 入链通集（采集域内 depth-2 唯一
    // mult=1 键）。后续波次（"-b-2" 就绪后再下潜 depth-3）由泵至静默兜住，
    // 实测钉值见下。
    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        dqGeom::Range3d const fit = dta::mountDumpFitVolume(mount->fitRange,
                                                            /*zEps=*/1.0);
        auto const center = fit.Center();
        double constexpr kZoom = 3.0;
        dqGeom::Range3d const zoomed = dqGeom::Range3d::CreateXYZXYZ(
            center.x - (center.x - fit.low.x) / kZoom,
            center.y - (center.y - fit.low.y) / kZoom,
            center.z - (center.z - fit.low.z) / kZoom,
            center.x + (fit.high.x - center.x) / kZoom,
            center.y + (fit.high.y - center.y) / kZoom,
            center.z + (fit.high.z - center.z) / kZoom);
        view3d->LookAtVolume(zoomed);
        view.getUeViewport()->InvalidateController();
        view.getUeViewport()->synchWithView(
            dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    }
    int const quiesceIter2 = pumpToQuiesce();
    ASSERT_GE(quiesceIter2, 0)
        << "phase-2 drill load never quiesced — in-flight requests remain (log="
        << mount->fetcher->requestLog().size() << " ready=" << readyTiles << ")";
    view.getUeViewport()->RenderFrame();
    uint32_t const dispatched =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests
        - dispatchedBefore;

    // --- 对账：请求日志 vs manifest 键集合（两相累计） ---
    std::set<std::string> manifestKeys;
    for (auto const& t : mount->manifest.tiles)
        manifestKeys.insert(t.treeId + "/" + t.contentId);

    auto const& log = mount->fetcher->requestLog();
    std::set<std::string> requestedKeys;
    size_t numCompleted = 0, numNotFound = 0, numError = 0;
    std::vector<std::string> misses;  // requested \ manifest（contentId）
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
    printf("[RPC-RENDER] full-load reconcile (2-phase): requested=%zu "
           "(completed=%zu notFound=%zu error=%zu) manifest=%zu "
           "coverage=%zu/%zu quiesceIter=%d/%d ready=%ld/%ld dispatched=%u\n",
           log.size(), numCompleted, numNotFound, numError, manifestKeys.size(),
           requestedKeys.size() - misses.size(), manifestKeys.size(),
           quiesceIter, quiesceIter2, readyTiles, totalTiles, dispatched);

    // ① 零 Error（dump 资产自洽性破口）。
    EXPECT_EQ(0u, numError) << "dump asset integrity break (missing file / "
                               "byteLength mismatch / malformed url)";
    // ② 零重复请求（每键至多一条）。
    EXPECT_EQ(requestedKeys.size(), log.size())
        << "duplicate requests for the same key — re-request thrash";
    // ③ 链通：Completed 键 ⊆ manifest，根键 + 相 2 钻取键在其中。
    for (auto const& rec : log) {
        if (rec.outcome != dqApp::DumpTileFetcher::DumpFetchOutcome::Completed)
            continue;
        EXPECT_TRUE(manifestKeys.count(rec.treeId + "/" + rec.contentId) > 0)
            << "Completed key not in manifest: " << rec.contentId;
    }
    EXPECT_GT(numCompleted, 0u) << "no manifest key was ever requested";
    // M-F(2) 实测钉值（两相驱动实测）：链通集 = {"-b-0-0-0-0-1"（相 1 根键）,
    // "-b-2-0-0-0-1"（相 2 钻取消费的派生子键——mult=1 细分继承形，采集域内
    // depth-2 唯一）}。采集域的放大键（-b-0-0-0-0-{2,4,8} 等）是 sweep BFS
    // 面产物，视口驱动不请求（锁头取证结论 (a)）——不钉入链通集，钉了即烤
    // 采集面差异进判据。
    EXPECT_EQ(2u, numCompleted) << "unexpected chain-connected set — reconcile "
                                   "the new consumption face consciously";
    {
        bool rootConnected = false, drillConnected = false;
        for (auto const& rec : log) {
            if (rec.outcome != dqApp::DumpTileFetcher::DumpFetchOutcome::Completed)
                continue;
            if (rec.contentId == "-b-0-0-0-0-1")
                rootConnected = true;
            if (rec.contentId == "-b-2-0-0-0-1")
                drillConnected = true;
        }
        EXPECT_TRUE(rootConnected) << "root key never connected";
        EXPECT_TRUE(drillConnected)
            << "phase-2 drill key -b-2-0-0-0-1 (in-manifest mult-1 derivation) "
               "never connected — derived-child request chain broken";
    }
    // ④ 差异形：每个 miss 都是 "-b-<d>-<i>-<j>-<k>-<mult>" 形（V4 派生形，
    // mult 段 hex——细分继承父 mult / 放大 2 的幂）。
    for (auto const& m : misses) {
        bool derivedForm = false;
        if (m.rfind("-b-", 0) == 0) {
            // 段：['', 'b', d, i, j, k, mult] —— mult 为 1-2 位 hex、d ≥ 1。
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
            derivedForm = segs.size() == 7 && segs[1] == "b"
                          && segs[6].size() >= 1 && segs[6].size() <= 2
                          && segs[6].find_first_not_of("0123456789abcdef")
                             == std::string::npos
                          && segs[2] != "0";
        }
        EXPECT_TRUE(derivedForm)
            << "miss key is not a V4 derivation shape: " << m
            << " (collection-domain shape leaked or malformed)";
    }
    // 差异键实钉（M-F(2) 两相实测）：域外 miss 恰两枚——
    //  "-b-1-0-0-0-1"（相 1：冷启动根的首个 loadChildren 在内容到达前细分
    //    （两实现同构）、唯一幸存子被 :148 ≤1 门保留、fit 下 Visible 被请求；
    //    sweep 从未细分 mult=1 父 → 域外）；
    //  "-b-3-0-0-0-1"（相 2 后波："-b-2-0-0-0-1" 就绪后再下潜 depth-3，
    //    mult=1 形；采集域 depth-3 无 mult=1 键 → 域外）。
    // 若实测变化（键集合/数量），先核锁头取证结论再更新钉值。
    ASSERT_EQ(2u, misses.size());
    EXPECT_EQ("-b-1-0-0-0-1", misses[0]);
    EXPECT_EQ("-b-3-0-0-0-1", misses[1]);

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

    // ⑧ 像素锚：相 2 钻取视图的消费内容上屏。首绿实测（M-F(2) 两相驱动）：
    // content=2624167 px = 帧（2000×1400）的 93.7%——钻取体积落入模型内容
    // octant，整帧近全覆盖（绿上下文墙在钻取体积外，green=0 是相 2 取景的
    // 预期形态，不作锚）。M-G(2) 复测（maxInitialTilesToSkip=3 载体接线 +
    // 强制选择帧泵——链通集/差异集/绘制键全部不变）content=2748939（98.2%，
    // 阈值带内）。阈值 = 首绿实测的 0.38×（内容存活退化——如瓦内容整体消
    // 失/取景链断——必红）；WHERE = 内容质心在中央带（钻取向心取 fit 中
    // 心，质心偏移 = 变换链缺陷）。
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    dumpBmp(frame, w, h,
            DANQING_TILE_ASSETS_DIR "/../../build/rpc-dump-instances60-full.bmp");
    long count = 0;
    double cx = 0, cy = 0;
    uint32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
    ASSERT_TRUE(contentStats(frame, w, h, count, cx, cy, minX, maxX, minY, maxY));
    printf("[RPC-RENDER] content=%ld px (%.3f%% of %ux%u) bbox=(%u,%u)-(%u,%u) "
           "centroid=(%.0f,%.0f)\n",
           count, 100.0 * count / (static_cast<double>(w) * h), w, h,
           minX, minY, maxX, maxY, cx, cy);
    EXPECT_GE(count, 1000000l)
        << "consumed drill-view content barely visible — threshold is a pinned "
           "fraction of the measured GREEN baseline, not a fudge factor";
    EXPECT_LT(std::abs(cx - w / 2.0), w * 0.2) << "content centroid x off-center";
    EXPECT_LT(std::abs(cy - h / 2.0), h * 0.2) << "content centroid y off-center";
    // ⑨ dispatch ≥ 1。
    EXPECT_GE(dispatched, 1u);

    view.getUeViewport()->DropTiledGraphicsProvider(&mount->provider);
    view.close();
    spin(200);
}

// ---------------------------------------------------------------------------
// drill 回放 E2E 同构锁（M-G(2)）：instances60-drill-v1（**真实视口请求面**
// 采集——zoom 泵放大链）的全部瓦键都必须进入链通/消费集。"视口请求面 = 回放
// 可达面"的同构证据：采集侧真实视口请求的每一张瓦，回放侧同配方下潜必须
// 同样派生、请求、字节回放、消费成 graphics。
//
// 【采集 provenance（README 在案，回放侧只读 §11.11）】
// instances60-drill-v1：1 树（25_1d-E:6_0x1c，props=files/3.json）+ 5 瓦——
//   -b-2-0-0-0-{1,2,4,8,10}：depth=2、(i,j,k)=(0,0,0) 的**放大链**（V4 键全
//   字段 hex——mult 段 "10"=0x10=16，ImdlTileTree.cpp:118-172 specFromId/join
//   权威解析；dump README 的"×10/非 2 幂"表述是把 hex 段按十进制读的松散
//   记法，链实为 ×1,×2,×4,×8,×16 逐倍放大）。
// dump 不含根瓦 -b-0-0-0-0-1 与中间瓦 -b-1-0-0-0-1（采集侧 wrapper 安装竞态
// + 参考跳过语义——maxInitialTilesToSkip=3 下根/d1 恒 canSkip，视口本不强制
// 请求；已登记为 dump 已知限制）。
//
// 【参考机制锚点（判据设计依据——全部核读）】
//  ①该模型每张瓦的 imdl header 解码 sizeMultiplier=1.0
//    （decodeImdlContentDescriptionHeaderOnly，ImdlDocument.cpp:87-140；
//    参考 TileMetadata.ts:880-940；采集侧 python 复算六张瓦全部 1.0）。
//  ②computeChildTileProps（TileMetadata.ts:777-853；DanQing 移植
//    ImdlTileTree.cpp:293-357 逐行同构）：parent.sizeMultiplier 已设 →
//    **放大分支**（独子、同体积、mult×2、depth 不变——:785-799）；未设 →
//    **细分分支**（8 分、depth+1、ijk×2+octant、mult 继承、model-range
//    拒空——:801-848）。
//  ③树 props（drill files/3.json）：rootTile 无 sizeMultiplier、有
//    contentRange、maxInitialTilesToSkip=3、maxTilesToSkip=1、
//    tileScreenSize=2048。
//  ④IModelTile.selectTiles（IModelTile.ts:205-334）：:282 loadChildren 在
//    too coarse 时无条件调用，不等内容到达；setContent（:134-148，M-F(1)
//    已移植）：sizeMultiplier 升门控 + contentId 覆写 + children>1 才
//    disposeChildren。
//  ⑤活体派生路径（采集取证 + 参考机制推出，全部事实自洽）：root（props
//    元数据，无 sizeMult）→ 细分 → -b-1-0-0-0-1（model-range 拒 7 个
//    octant，独子）→ 细分 → -b-2-0-0-0-1（默认视口 Visible，被请求）→
//    setContent 置 sizeMult=1.0 → zoom 泵逐级 too coarse → 放大链
//    -b-2-0-0-0-{2,4,8,10}。
//  ⑤b 根键请求的参考机制（首绿实测——回放侧根键在相 1 被请求）：-b-1 转
//    NotFound 终态后，根的选择走 :296-307 子代循环——独子 -b-1 Visible 但
//    未就绪返回 Yes、其 loadStatus==NotFound 命中 :299-301（"a child we
//    want to draw failed to load — draw parent instead"）→ canSkip 翻假 →
//    :329-331 insertMissing(根)。参考同构（非预算缺口残留）；drill dump
//    无根字节 → 根键入 miss 集（采集 provenance 登记）。
//  ⑥maxInitialTilesToSkip 的参考载体：TileProps.ts:63（IModelTileTreeProps
//    字段）→ iModelTileTreeParamsFromJSON（IModelTileTree.ts:51/:76）→
//    构造器 :390 `params.maxInitialTilesToSkip ?? 0`。本锁建立前的实测
//    （RED 记录见下）暴露 DanQing 该载体缺失（m_maxInitialTilesToSkip
//    硬编码 0——ImdlTileTree.cpp 原 :901 登记"props 载体归后续里程碑"），
//    根 NotFound 时 :265/:269 的 canSkip 恒假 → 子代永不遍历 → 树被根
//    阻断（链={根 NotFound}，0/5 键）。参考语义下（预算=3）根/d1/d2 恒
//    canSkip，根 NotFound 不阻下潜——**移植缺口**，按 §11.8 修引擎
//    （maxInitialTilesToSkip props 载体接线，真实 Ported from: 锚点如上）
//    而非退 merged-mount 备案。
//
// 【判据（首绿实测钉值见各断言处）】
//  ①零 Error；②零重复请求；
//  ③**链通完备（本锁主判据——同构证据）**：drill manifest 全部 5 键
//    ∈ requestLog 且 outcome=Completed（目标 100%）；
//  ④最深键实钉：-b-2-0-0-0-10 ∈ Completed；
//  ⑤miss 集形验 + 首绿钉死清单：每个 miss 都是 V4 派生形
//    "-b-<hex>-<hex>-<hex>-<hex>-<hex>"；
//  ⑥树侧终态：Completed 键 → hasGraphics（计数对账 ==）；NotFound 键 →
//    loadStatus==NotFound；无在途残留；
//  ⑦像素锚：泵停后固定 kZoom=8 锚定视图（链上某就绪瓦必 Visible 被绘制）
//    的内容存活 + WHERE（首绿实测钉死）；
//  ⑧dispatch ≥ 5（每 drill 键至少一次投递）。
// Authored: no reference test exists in itwinjs-core/imodel-native for
// dump-replay viewport-chain isomorphism（§5(f)——渲染像素回归授权 §5(g)：
// 复现配方=本锁的确定性 zoom 泵；证据链=requestLog 对账 + readPixels 锚）。
TEST(RpcDumpRender, Instances60DrillReplaysViewportChain)
{
    std::string dumpRoot = std::string(DANQING_TILE_ASSETS_DIR)
                           + "/rpc-dumps/instances60-drill-v1";
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

    // 多树全量装载（drill-only 挂载——本 dump 1 树 5 瓦；root/-b-1 缺字节
    // 是采集实态，参考跳过语义（maxInitialTilesToSkip=3）下不阻下潜——
    // 本锁即该语义的实锤面。iModelInfo 缺省——fit 回退树 contentRange）。
    auto mount = dta::mountDump(*view.getUeViewport(), dumpRoot);
    ASSERT_TRUE(mount.has_value()) << "mount failed: " << dumpRoot;
    ASSERT_EQ(1u, mount->manifest.trees.size());  // instances60-drill-v1：1 树
    ASSERT_EQ(5u, mount->manifest.tiles.size());  // 放大链 5 瓦
    ASSERT_NE(nullptr, mount->fetcher);
    std::string const treeId = mount->manifest.trees[0].treeId;
    auto props = mount->props->byTreeId(treeId);
    ASSERT_TRUE(props.has_value()) << "byTreeId failed: " << treeId;
    ASSERT_EQ(2424832u, props->metadata.formatVersion);  // 37.0（major<<16）

    view.getUeViewport()->AddTiledGraphicsProvider(&mount->provider);
    ASSERT_TRUE(view.getUeViewport()->HasTiledGraphicsProvider(&mount->provider));

    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        // 取景 = fitRange（树 contentRange 并集——单树即其域；30% 外扩 +
        // zEps=1.0 同 M-E/M-F 成熟配方——不跑 StandardViewTool）。
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

    // 泵至静默：请求日志尺寸连续 6 次迭代不变 + 投递队列排空（有界 20s/级）。
    // 与 M-F(2) 的差异：①不设 readyTiles≥1 门——fit 级静默的合法形态是"唯一
    // 请求键（-b-1，采集域外 miss）零就绪"，该门会把合法静默误判为未达；
    // ②每迭代强制 InvalidateController——NotFound 交付不触发失效级联（无
    // graphic），突发帧停后 :299-301 回退（子 NotFound → 请求父）等"需下一
    // 选择帧"的机制会饥饿（M-F(2) 相 1 实测：log={-b-1} 停 20s）；强制选择
    // 帧把请求面推到不动点——请求键由（视图 × 瓦状态）决定，强制帧只提高
    // 求值频率，不改变请求面（键级幂等）。
    long readyTiles = 0, totalTiles = 0;
    auto pumpToQuiesce = [&]() -> int {
        size_t lastLogSize = 0;
        int stable = 0;
        for (int i = 0; i < 200; ++i) {
            view.getUeViewport()->InvalidateController();
            spin(100);
            readyTiles = 0;
            totalTiles = 0;
            for (auto& t : mount->trees)
                countGraphicsReady(t->getRootTile(), readyTiles, totalTiles);
            size_t const logSize = mount->fetcher->requestLog().size();
            if (!mount->fetcher->requestLog().empty()
                && logSize == lastLogSize
                && mount->fetcher->getActiveCount() == 0) {
                if (++stable >= 6)
                    return i;
            } else {
                stable = 0;
            }
            lastLogSize = logSize;
        }
        return -1;
    };

    // 相 1：冷 fit 泵至静默（根细分 -b-1——采集域外 miss；不阻下潜是本锁
    // 实锤面之一）。
    int quiesceIter = pumpToQuiesce();
    ASSERT_GE(quiesceIter, 0)
        << "phase-1 tile load never quiesced (log="
        << mount->fetcher->requestLog().size() << " ready=" << readyTiles << ")";

    // 相 2：迭代 zoom 泵（CompatSeed/M-F(2) 确定性钻取的循环形——kZoom=2^n
    // 绕 fit 中心缩 LookAtVolume + synch + 泵至静默，n=1..8 上限）。每级把
    // 当前最深瓦推到 too coarse → loadChildren 派生下一级放大子（机制锚
    // ②④⑤）。停止规则：drill 5 键全部 Completed 后再跑一级（采过冲键/
    // 终态形），或级数耗尽。
    std::set<std::string> const drillKeys = {
        "-b-2-0-0-0-1", "-b-2-0-0-0-2", "-b-2-0-0-0-4",
        "-b-2-0-0-0-8", "-b-2-0-0-0-10",
    };
    auto connectedDrillCount = [&]() -> size_t {
        size_t n = 0;
        for (auto const& rec : mount->fetcher->requestLog())
            if (rec.outcome == dqApp::DumpTileFetcher::DumpFetchOutcome::Completed
                && drillKeys.count(rec.contentId) > 0)
                ++n;
        return n;
    };
    dqGeom::Range3d const fitVol = dta::mountDumpFitVolume(mount->fitRange,
                                                           /*zEps=*/1.0);
    int firstFullLevel = -1, levelsRun = 0;
    for (int n = 1; n <= 8; ++n) {
        double const kZoom = static_cast<double>(1u << n);  // 2,4,8,...,256
        auto const center = fitVol.Center();
        dqGeom::Range3d const zoomed = dqGeom::Range3d::CreateXYZXYZ(
            center.x - (center.x - fitVol.low.x) / kZoom,
            center.y - (center.y - fitVol.low.y) / kZoom,
            center.z - (center.z - fitVol.low.z) / kZoom,
            center.x + (fitVol.high.x - center.x) / kZoom,
            center.y + (fitVol.high.y - center.y) / kZoom,
            center.z + (fitVol.high.z - center.z) / kZoom);
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        view3d->LookAtVolume(zoomed);
        view.getUeViewport()->InvalidateController();
        view.getUeViewport()->synchWithView(
            dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
        int const q = pumpToQuiesce();
        ASSERT_GE(q, 0) << "zoom level " << n << " never quiesced (log="
                        << mount->fetcher->requestLog().size() << ")";
        levelsRun = n;
        printf("[RPC-RENDER] drill level %d (kZoom=%.0f): log=%zu connected=%zu "
               "ready=%ld/%ld\n",
               n, kZoom, mount->fetcher->requestLog().size(),
               connectedDrillCount(), readyTiles, totalTiles);
        if (connectedDrillCount() == drillKeys.size()) {
            if (firstFullLevel < 0)
                firstFullLevel = n;
            if (n > firstFullLevel)
                break;  // 全连通后的过冲/终态级已采——停
        }
    }

    // --- 对账：请求日志 vs drill manifest 键集合（泵全程累计） ---
    std::set<std::string> manifestKeys;
    for (auto const& t : mount->manifest.tiles)
        manifestKeys.insert(t.treeId + "/" + t.contentId);

    auto const& log = mount->fetcher->requestLog();
    std::set<std::string> requestedKeys;
    size_t numCompleted = 0, numNotFound = 0, numError = 0;
    std::vector<std::string> misses;      // requested \ manifest（contentId）
    std::set<std::string> completedIds;   // Completed 的 contentId 集
    for (auto const& rec : log) {
        std::string const key = rec.treeId + "/" + rec.contentId;
        requestedKeys.insert(key);
        // 同树前缀（判据⑤的另一半——全部请求都落在本 dump 唯一树域内）。
        EXPECT_EQ(treeId, rec.treeId)
            << "request key escaped the drill tree domain: " << key;
        switch (rec.outcome) {
        case dqApp::DumpTileFetcher::DumpFetchOutcome::Completed:
            ++numCompleted;
            completedIds.insert(rec.contentId);
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
    printf("[RPC-RENDER] drill reconcile: requested=%zu (completed=%zu "
           "notFound=%zu error=%zu) manifest=%zu levels=%d (full@%d) "
           "ready=%ld/%ld\n",
           log.size(), numCompleted, numNotFound, numError, manifestKeys.size(),
           levelsRun, firstFullLevel, readyTiles, totalTiles);
    for (auto const& rec : log)
        printf("[RPC-RENDER]   log %s %s\n",
               rec.outcome == dqApp::DumpTileFetcher::DumpFetchOutcome::Completed
                   ? "Completed"
                   : (rec.outcome == dqApp::DumpTileFetcher::DumpFetchOutcome::NotFound
                          ? "NotFound"
                          : "Error"),
               rec.contentId.c_str());

    // ① 零 Error（dump 资产自洽性破口）。
    EXPECT_EQ(0u, numError) << "dump asset integrity break (missing file / "
                               "byteLength mismatch / malformed url)";
    // ② 零重复请求（每键至多一条——TileAdmin 非 NotLoaded 不重建请求的门）。
    EXPECT_EQ(requestedKeys.size(), log.size())
        << "duplicate requests for the same key — re-request thrash";
    // ③ 链通完备（主判据——同构证据）：drill manifest 全部 5 键被请求且
    //    Completed。Completed 键必须 ⊆ manifest（链通集不外溢）。
    for (auto const& rec : log) {
        if (rec.outcome != dqApp::DumpTileFetcher::DumpFetchOutcome::Completed)
            continue;
        EXPECT_TRUE(manifestKeys.count(rec.treeId + "/" + rec.contentId) > 0)
            << "Completed key not in manifest: " << rec.contentId;
    }
    for (auto const& key : drillKeys) {
        EXPECT_TRUE(completedIds.count(key) > 0)
            << "drill key never connected (viewport chain not isomorphic to "
               "the captured request face): " << key;
    }
    // ④ 最深键实钉：-b-2-0-0-0-10（mult=0x10——采集侧 SSE 饱和点）在链通集。
    EXPECT_TRUE(completedIds.count("-b-2-0-0-0-10") > 0)
        << "deepest captured key -b-2-0-0-0-10 (mult=0x10) never connected — "
           "zoom pump stopped short of the captured saturation point";
    // ⑤ miss 集形验：每个 miss 都是 V4 请求键形——派生形
    //    "-b-<d≥1>-<i>-<j>-<k>-<mult>"（全段 hex——细分继承父 mult / 放大 2
    //    的幂），或 V4 根形 "-b-0-0-0-0-1"（rootContentId()——根经 :299-301
    //    NotFound 回退被请求（机制锚⑤b），drill dump 无根字节是采集实态，
    //    非畸形键）。
    for (auto const& m : misses) {
        bool derivedForm = false;
        if (m.rfind("-b-", 0) == 0) {
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
            bool allHex = segs.size() == 7;
            if (allHex) {
                for (size_t s = 2; s < 7; ++s) {
                    if (segs[s].empty()
                        || segs[s].find_first_not_of("0123456789abcdef")
                           != std::string::npos) {
                        allHex = false;
                        break;
                    }
                }
            }
            derivedForm = allHex && segs[1] == "b" && segs[6].size() <= 2
                          && (segs[2] != "0" || m == "-b-0-0-0-0-1");
        }
        EXPECT_TRUE(derivedForm)
            << "miss key is not a V4 request-key shape: " << m
            << " (collection-domain shape leaked or malformed)";
    }
    // miss 清单实钉（首绿实测——请求日志序）：
    //  "-b-1-0-0-0-1"（相 1：根的冷启动 loadChildren 以 props 元数据细分、
    //    独子 Visible 被请求；采集域外——wrapper 竞态 + 参考跳过语义，dump
    //    已知限制登记）；
    //  "-b-0-0-0-0-1"（相 1 后波：-b-1 NotFound 终态 → :299-301 回退请求根
    //    ——机制锚⑤b；采集域外同上）；
    //  "-b-2-0-0-0-20"（过冲放大子 mult=0x20：全连通后一级 SSE 越过 ×16 →
    //    m16 too coarse → 派生 m32 → Visible 被请求；采集在 ×16 饱和——dump
    //    域外，机制自洽）。
    ASSERT_EQ(3u, misses.size());
    EXPECT_EQ("-b-1-0-0-0-1", misses[0]);
    EXPECT_EQ("-b-0-0-0-0-1", misses[1]);
    EXPECT_EQ("-b-2-0-0-0-20", misses[2]);
    // Completed 恰为 drill 5 键（链通集 == manifest 键集合——同构的完整表述）。
    EXPECT_EQ(5u, numCompleted) << "chain-connected set != drill manifest keys";
    // 配方形实钉（首绿实测）：m16 于第 5 级（kZoom=32）连通，过冲级第 6 级
    // （kZoom=64）后停泵——级数依赖视口/设备像素几何，harness 变更时按机制
    // 复核重钉。
    EXPECT_EQ(6, levelsRun);
    EXPECT_EQ(5, firstFullLevel);

    // ⑥ 树侧终态全消费：Completed 键 → hasGraphics；NotFound 键 → 终态
    //    NotFound；无在途残留。
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
        bool const completed = completedIds.count(
                                   kv.first.substr(kv.first.find('/') + 1)) > 0
                               && manifestKeys.count(kv.first) > 0;
        if (completed) {
            if (kv.second->hasGraphics())
                ++graphicsTiles;
            else
                ++inFlight;  // Completed 却未消费成 graphics
        } else if (requestedKeys.count(kv.first) > 0) {
            if (kv.second->getLoadStatus() == dqRender::TileLoadStatus::NotFound)
                ++terminalMisses;
            else
                ++inFlight;  // 已请求未达终态
        }
        // 未请求的瓦（根——canSkip 穿透，参考同构的"视口本不请求"形态）
        // 不计入任何桶。
    }
    printf("[RPC-RENDER] drill tree-side: tiles=%zu graphics=%ld terminalMiss=%ld "
           "inFlight=%ld\n",
           tilesByKey.size(), graphicsTiles, terminalMisses, inFlight);
    EXPECT_EQ(0l, inFlight) << "tiles left in non-terminal state at quiescence";
    EXPECT_EQ(static_cast<long>(numCompleted), graphicsTiles)
        << "completed requests without consumed graphics (readContent drop?)";

    // ⑦ 像素锚：固定 kZoom=8 锚定视图（泵停后重设——链上某就绪瓦在此视图
    //    必 Visible 被绘制：放大链 m1..m16 全就绪后，kZoom=8 的 Visible 级
    //    落在链内；视图内容 = 实例簇中心 3.4m 带）。重设视图不得引入新请求
    //    （全部就绪——对账日志尺寸不变的副断言）。
    size_t const logSizeBeforeAnchor = log.size();
    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        auto const center = fitVol.Center();
        double constexpr kAnchor = 8.0;
        dqGeom::Range3d const anchor = dqGeom::Range3d::CreateXYZXYZ(
            center.x - (center.x - fitVol.low.x) / kAnchor,
            center.y - (center.y - fitVol.low.y) / kAnchor,
            center.z - (center.z - fitVol.low.z) / kAnchor,
            center.x + (fitVol.high.x - center.x) / kAnchor,
            center.y + (fitVol.high.y - center.y) / kAnchor,
            center.z + (fitVol.high.z - center.z) / kAnchor);
        view3d->LookAtVolume(anchor);
        view.getUeViewport()->InvalidateController();
        view.getUeViewport()->synchWithView(
            dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    }
    spin(300);  // 失效级联 → 重绘帧（无新请求——不泵全静默，只等重绘）
    view.getUeViewport()->RenderFrame();
    EXPECT_EQ(logSizeBeforeAnchor, mount->fetcher->requestLog().size())
        << "anchor view introduced new requests — chain tiles not all ready";

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    dumpBmp(frame, w, h,
            DANQING_TILE_ASSETS_DIR "/../../build/rpc-dump-instances60-drill.bmp");
    long count = 0;
    double cx = 0, cy = 0;
    uint32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
    ASSERT_TRUE(contentStats(frame, w, h, count, cx, cy, minX, maxX, minY, maxY))
        << "no drill-chain content rendered at the anchor view — chain "
           "consumption broken (see BMP)";
    printf("[RPC-RENDER] drill anchor: content=%ld px (%.3f%% of %ux%u) "
           "bbox=(%u,%u)-(%u,%u) centroid=(%.0f,%.0f) frame center=(%.0f,%.0f)\n",
           count, 100.0 * count / (static_cast<double>(w) * h), w, h,
           minX, minY, maxX, maxY, cx, cy, w / 2.0, h / 2.0);
    // 内容存活：≥ 450000（首绿实测 1197243 px = 帧 42.76% 的 0.38×——内容
    //    整体消失/取景链断必红；阈值是首绿实测的钉死比例，非调参）。
    EXPECT_GE(count, 450000l)
        << "drill-chain content barely visible at the anchor view — threshold "
           "is a pinned fraction of the measured GREEN baseline";
    // WHERE：质心在视口中央带（±20%——首绿实测 (973,700) 对帧心 (1000,700)）。
    EXPECT_LT(std::abs(cx - w / 2.0), w * 0.2) << "content centroid x off-center";
    EXPECT_LT(std::abs(cy - h / 2.0), h * 0.2) << "content centroid y off-center";

    // ⑧ dispatch ≥ 5（每 drill 键至少一次投递）。
    uint32_t const dispatched =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests;
    EXPECT_GE(dispatched, 5u) << "expected at least one dispatch per drill key";

    view.getUeViewport()->DropTiledGraphicsProvider(&mount->provider);
    view.close();
    spin(200);
}

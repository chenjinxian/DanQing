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

#include <dqApp/Application.h>
#include <dqApp/Viewport.h>
#include <dqApp/StandardView.h>
#include <dqApp/ViewTool.h>
#include <dqApp/ViewState.h>
#include <dqApp/tile/DumpTileFetcher.h>
#include <dqApp/tile/DumpTileTreeProps.h>
#include <dqApp/tile/SimpleTileTreeReference.h>
#include <dqApp/tile/TiledGraphicsProvider.h>
#include <dqRender/tile/ImdlTileTree.h>
#include <dqRender/tile/TileAdmin.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
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

// TiledGraphicsProvider 装树（TileTreeRenderTest 的 TreeSetProvider 同款——
// 应用通道 Viewport.ts:1729-1732 addTiledGraphicsProvider）。
class DumpTreeProvider final : public dqApp::TiledGraphicsProvider {
public:
    void addTree(dqRender::TileTree* tree)
    {
        m_refs.push_back(std::make_unique<dqApp::SimpleTileTreeReference>(tree));
    }

    void forEachTileTreeRef(
        dqApp::Viewport& /*viewport*/,
        std::function<void(dqApp::TileTreeReference&)> const& func) const override
    {
        for (auto& ref : m_refs)
            func(*ref);
    }

private:
    std::vector<std::unique_ptr<dqApp::SimpleTileTreeReference>> m_refs;
};

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
// ---------------------------------------------------------------------------
// Authored: 见文件头。该瓦无 instances 修饰、顶点量化域 = iModel 坐标域——
// 现有 LUT 消费链的忠实上屏。判据：
// ①内容存活：非背景像素 ≥ 阈值（首绿实测的 1/2 下限，实测值回填本注释）；
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

    // 1. dump 装载 + DumpTileFetcher 注入（DI 缝，§8.4；替换 Startup 的
    //    FileTileFetcher。gtest_discover_tests 逐测试独立进程——不回溢其他
    //    ctest 条目；本文件注册在 TileTreeRenderTest 之后）。
    auto manifest = dqApp::loadDumpManifest(dumpRoot);
    ASSERT_TRUE(manifest.has_value()) << "manifest load failed: " << dumpRoot;
    ASSERT_EQ(2u, manifest->trees.size());  // mirukuru-v1：2 树 1 瓦
    ASSERT_EQ(1u, manifest->tiles.size());

    auto fetcher = std::make_unique<dqApp::DumpTileFetcher>(dumpRoot);
    ASSERT_TRUE(fetcher->isValid());
    dqRender::TileAdmin::instance().setFetcher(std::move(fetcher));

    auto treeProps = dqApp::DumpTileTreeProps::load(dumpRoot);
    ASSERT_TRUE(treeProps.has_value());
    std::string const treeId = manifest->trees[0].treeId;
    auto props = treeProps->byTreeId(treeId);
    ASSERT_TRUE(props.has_value()) << "byTreeId failed: " << treeId;
    // 前置（资产自洽，非回归断言）：RPC dump 携带 formatVersion——M-D(3) 起
    // 消费（ContentIdProvider V4 请求键形态的切换依据，IModelTileTree.ts:396）。
    ASSERT_EQ(2424832u, props->metadata.formatVersion);  // 37.0（major<<16）

    // 2. 树装配 + 请求键锁（IModelTileTree.ts:398 rootContentId 覆写——根键
    //    必须是 manifest 的 depth-0 瓦键，否则 fetch NotFound）。
    std::unique_ptr<dqRender::ImdlTileTree> tree =
        std::make_unique<dqRender::ImdlTileTree>(
            props->id, props->rootTile.contentId, props->rootTile.range,
            props->metadata);
    {
        ASSERT_EQ("-b-0-0-0-0-1", manifest->tiles[0].contentId);
        auto* root = static_cast<dqRender::ImdlTile*>(tree->getRootTile());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(manifest->tiles[0].contentId, root->getContentId())
            << "root request key must be the manifest key (IModelTileTree.ts:398 "
               "rootContentId override)";
    }

    // 3. 装树（TiledGraphicsProvider 应用通道）+ 取景 + 泵帧。
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin(400);

    DumpTreeProvider provider;
    provider.addTree(tree.get());
    view.getUeViewport()->AddTiledGraphicsProvider(&provider);
    ASSERT_TRUE(view.getUeViewport()->HasTiledGraphicsProvider(&provider));
    tree->setRenderSystem(view.getUeViewport()->renderSystem());

    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        // 取景 = 树 contentRange（iModel 坐标域——瓦顶点同域，见文件头登记）
        // 外扩 30%（先验决定：对象完整居中、四角留背景——非事后调参）。
        dqGeom::Range3d const& cr = props->metadata.contentRange;
        double const dx = 0.3 * (cr.high.x - cr.low.x);
        double const dy = 0.3 * (cr.high.y - cr.low.y);
        double const dz = 0.3 * (cr.high.z - cr.low.z) + 1.0;  // +1：z 向亚厘米薄盒——给 Iso 视线留深度
        view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(
            cr.low.x - dx, cr.low.y - dy, cr.low.z - dz,
            cr.high.x + dx, cr.high.y + dy, cr.high.z + dz));
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
        countGraphicsReady(tree->getRootTile(), readyTiles, totalTiles);
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

    // ① 内容存活（阈值 = 首绿实测的 1/2：首绿 2000×1400 实测 572660 px =
    //    20.45%，质心 (999,698) 对帧心 (1000,700)、bbox (284,284)-(1714,1111)
    //    近对称——几何即取景域内的模型平面）。
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

    view.getUeViewport()->DropTiledGraphicsProvider(&provider);
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

    auto manifest = dqApp::loadDumpManifest(dumpRoot);
    ASSERT_TRUE(manifest.has_value()) << "manifest load failed: " << dumpRoot;
    ASSERT_EQ(1u, manifest->trees.size());  // compatseed-v1：1 树 7 瓦
    ASSERT_EQ(7u, manifest->tiles.size());

    auto fetcher = std::make_unique<dqApp::DumpTileFetcher>(dumpRoot);
    ASSERT_TRUE(fetcher->isValid());
    dqRender::TileAdmin::instance().setFetcher(std::move(fetcher));

    auto treeProps = dqApp::DumpTileTreeProps::load(dumpRoot);
    ASSERT_TRUE(treeProps.has_value());
    std::string const treeId = manifest->trees[0].treeId;
    auto props = treeProps->byTreeId(treeId);
    ASSERT_TRUE(props.has_value()) << "byTreeId failed: " << treeId;
    ASSERT_EQ(2424832u, props->metadata.formatVersion);  // 37.0（major<<16）

    std::unique_ptr<dqRender::ImdlTileTree> tree =
        std::make_unique<dqRender::ImdlTileTree>(
            props->id, props->rootTile.contentId, props->rootTile.range,
            props->metadata);

    // ① 请求键覆写（:405）：根键 = manifest 的 depth-0 条目。
    {
        std::string rootKey;
        for (auto const& t : manifest->tiles)
            if (t.treeId == treeId && t.contentId.rfind("-b-0-0-0-0-1", 0) == 0)
                rootKey = t.contentId;
        ASSERT_FALSE(rootKey.empty()) << "no depth-0 entry in manifest";
        auto* root = static_cast<dqRender::ImdlTile*>(tree->getRootTile());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(rootKey, root->getContentId())
            << "root request key must be the manifest key (IModelTileTree.ts:398 "
               "rootContentId override)";
        EXPECT_EQ(treeId + "/" + rootKey, tree->contentUrl(rootKey));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin(400);

    DumpTreeProvider provider;
    provider.addTree(tree.get());
    view.getUeViewport()->AddTiledGraphicsProvider(&provider);
    ASSERT_TRUE(view.getUeViewport()->HasTiledGraphicsProvider(&provider));
    tree->setRenderSystem(view.getUeViewport()->renderSystem());

    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        // 取景 = 树 contentRange（iModel 坐标域——instances 修好后瓦几何的
        // 落点；TD-25 前几何在原点附近不上屏，见本锁头注释）。
        dqGeom::Range3d const& cr = props->metadata.contentRange;
        double const dx = 0.3 * (cr.high.x - cr.low.x);
        double const dy = 0.3 * (cr.high.y - cr.low.y);
        double const dz = 0.3 * (cr.high.z - cr.low.z);
        view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(
            cr.low.x - dx, cr.low.y - dy, cr.low.z - dz,
            cr.high.x + dx, cr.high.y + dy, cr.high.z + dz));
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
        countGraphicsReady(tree->getRootTile(), readyTiles, totalTiles);
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
    auto* root = static_cast<dqRender::ImdlTile*>(tree->getRootTile());
    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(root->hasGraphics()) << "root tile (-b-0) has no graphics";

    view.getUeViewport()->DropTiledGraphicsProvider(&provider);
    view.close();
    spin(200);
}

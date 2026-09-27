// DumpTileFetcherTest — RPC-dump 本地回放取数契约测试（零网络 §8.2）。
//
// Authored: no reference test exists in itwinjs-core for RPC-dump replay（dump
//           资产为仓外 danqing-rpc-tools collector 从真实后端采集——阶段1 M-D
//           Task 1，§11.11 采集后只读）；本测试钉死的参考侧语义是
//           IModelTileTreeProps/IModelTileProps 的字段映射（core/common/src/tile/
//           TileProps.ts:23-70，经 iModelTileTreeParamsFromJSON
//           core/frontend/src/internal/tile/IModelTileTree.ts:49-82 消费——
//           PrimaryTileTree.createTileTree PrimaryTileTree.ts:63-80 的对齐面）
//           与 manifest 瓦键契约（contentId 原样串 = 前端请求键
//           getTileRequestProps TileAdmin.ts:694-706，Task 1 评审钉死①）。
//
// 完整性降级登记（brief Step 3 决策点）：manifest.sha256 由采集侧 collector 已算
// （Task 1 入库前全量复算一致）；dqBase 仅有 imodel-native 移植的 SHA1/MD5、
// 无 SHA256（自实现属 §7 自创算法——两参考仓均无对应物），故回放侧完整性 =
// byteLength 精确一致 + 文件存在性；字节同一性由③的磁盘直读逐字节比对钉死
// （强于哈希相等断言）。
#include <gtest/gtest.h>

#include "dqApp/tile/DumpTileFetcher.h"
#include "dqApp/tile/DumpTileTreeProps.h"

#include <dqCommon/FeatureTable.h>  // TileContent::featureTable unique_ptr 析构需完整类型
#include <dqRender/RenderGraphic.h>  // TileContent::graphic unique_ptr 同上
#include <dqRender/tile/Tile.h>
#include <dqRender/tile/TileTree.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef DANQING_TEST_ASSET_ROOT
#define DANQING_TEST_ASSET_ROOT "."
#endif

namespace {

std::string const kDumpRoot = std::string(DANQING_TEST_ASSET_ROOT)
    + "/third_party/tile-sample-assets/rpc-dumps";
std::string const kCompatTreeId = "25_1d-E:6_0x1c";

// --- 桩树/桩瓦（TileTreeRegistryTest 的 NullRangeTile 同款最小桩——fetch 回调
//     只要求一个可传递的 Tile&，不触 GL/TileAdmin）---
class StubTree : public dqRender::TileTree {
public:
    StubTree()
        : TileTree(nullptr)
    {
    }

    dqRender::TileVisibility computeVisibility(dqRender::TileDrawArgs&,
                                               dqRender::Tile*) override
    {
        return dqRender::TileVisibility::Visible;
    }
};

class StubTile : public dqRender::Tile {
public:
    explicit StubTile(dqRender::TileTree& tree)
        : Tile(tree, nullptr, dqGeom::Range3d())
    {
    }
    bool requestContent() override { return false; }
    dqRender::TileContent readContent(uint8_t const*, size_t) override { return {}; }
    void loadChildren() override {}
};

std::vector<uint8_t> readAllBytes(std::string const& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
        return {};
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// fetch 回调捕获（ITileFetcher 的 std::function 面）。
struct FetchResult {
    dqRender::Tile* tile = nullptr;
    bool completed = false;
    bool errored = false;
    std::vector<uint8_t> data;
    std::string error;
};

void fetchAndWait(dqApp::DumpTileFetcher& fetcher, std::string const& url,
                  dqRender::Tile& tile, FetchResult& out)
{
    fetcher.fetch(
        url, tile,
        [&out](dqRender::Tile& t, std::vector<uint8_t> const& data) {
            out.tile = &t;
            out.completed = true;
            out.data = data;
        },
        [&out](dqRender::Tile& t, std::string const& error) {
            out.tile = &t;
            out.errored = true;
            out.error = error;
        });
    fetcher.processCompleted();  // 轮询契约：投递只在 processCompleted（FileTileFetcher 同款）
}

}  // namespace

// ---------------------------------------------------------------------------
// 主契约：manifest 加载 → 树 props 映射 → 字节回放 → NotFound 语义。
// ---------------------------------------------------------------------------
TEST(DumpTileFetcherTest, ManifestLoadsAndServesTreePropsAndTiles)
{
    std::string const dumpRoot = kDumpRoot + "/compatseed-v1";

    // ① load：trees/tiles 计数与 manifest 一致（manifest.json stats：1 树/7 瓦）。
    auto props = dqApp::DumpTileTreeProps::load(dumpRoot);
    ASSERT_TRUE(props.has_value()) << "manifest load failed: " << dumpRoot;
    EXPECT_EQ(1u, props->getTreeCount());
    EXPECT_EQ(7u, props->getTileCount());

    // ② byTreeId：树 props JSON（files/5.json）钉死值——TileProps.ts 字段映射。
    auto tree = props->byTreeId(kCompatTreeId);
    ASSERT_TRUE(tree.has_value()) << "byTreeId failed: " << kCompatTreeId;

    // 树级（IModelTileTreeProps → ImdlTreeMetadata）：
    //   tileScreenSize = props.tileScreenSize ?? 512（IModelTileTree.ts:52）。
    EXPECT_EQ(2048u, tree->metadata.tileScreenSize);
    //   contentRange（:54-56 仅在字段存在时置值——compatseed 有）。
    EXPECT_DOUBLE_EQ(-100.00500000000001, tree->metadata.contentRange.low.x);
    EXPECT_DOUBLE_EQ(-100.00500000000001, tree->metadata.contentRange.low.y);
    EXPECT_DOUBLE_EQ(-100.00500000000001, tree->metadata.contentRange.low.z);
    EXPECT_DOUBLE_EQ(-97.50500000000001, tree->metadata.contentRange.high.x);
    EXPECT_DOUBLE_EQ(-97.50500000000001, tree->metadata.contentRange.high.y);
    EXPECT_DOUBLE_EQ(-97.50500000000001, tree->metadata.contentRange.high.z);
    //   is2d 不是树 props 字段（参考侧来自视图 is3d——PrimaryTileTree.ts:68
    //   options.is3d）；离线 dump 全为空间树 → 缺省 false。
    EXPECT_FALSE(tree->metadata.is2d);
    //   id（TileTreeProps.id 原文——不做强校验，参考 requestTileTreeProps 同样
    //   信任返回值；此处钉死解析所见）。
    EXPECT_EQ(kCompatTreeId, tree->id);

    // 根瓦 props（TileProps → ImdlTileMetadata + rootMaximumSize 载体）。
    //   注意键形态区分（Task 1 评审钉死①）：props.rootTile.contentId 是
    //   "0/0/0/0/1" 形态；manifest 瓦键是前端请求键 "-b-0-0-0-0-1" 形态。
    EXPECT_EQ("0/0/0/0/1", tree->rootTile.contentId);
    EXPECT_DOUBLE_EQ(2048.0, tree->rootMaximumSize);  // TileProps.maximumSize
    EXPECT_DOUBLE_EQ(-100.01499999999999, tree->rootTile.range.low.x);
    EXPECT_DOUBLE_EQ(-100.01499999999999, tree->rootTile.range.low.y);
    EXPECT_DOUBLE_EQ(-100.01499999999999, tree->rootTile.range.low.z);
    EXPECT_DOUBLE_EQ(100.01499999999997, tree->rootTile.range.high.x);
    EXPECT_DOUBLE_EQ(100.01499999999997, tree->rootTile.range.high.y);
    EXPECT_DOUBLE_EQ(100.01499999999997, tree->rootTile.range.high.z);
    // optional 缺省路径（Task 1 评审钉死：该 dump 的 rootTile 无
    // contentRange/sizeMultiplier/emptySubRangeMask——只锻炼默认值）：
    //   isLeaf 缺省 false（TileProps.ts:34-35 "Defaults to false"）；
    //   sizeMultiplier/emptySubRangeMask 缺省 = 未设（DanQing 0 约定，
    //   ImdlTileMetadata）；contentRange 缺省 null（IModelTileTree.ts:54-56）。
    EXPECT_FALSE(tree->rootTile.isLeaf);
    EXPECT_TRUE(tree->rootTile.contentRange.isNull());
    EXPECT_EQ(0.0, tree->rootTile.sizeMultiplier);
    EXPECT_EQ(0u, tree->rootTile.emptySubRangeMask);

    // ③ fetch(root 瓦)：manifest 根条目 = "-b-0-0-0-0-1" → files/12.imdl
    //    （byteLength 4304）。url 形态 A：<dumpRoot>/<treeId>/<contentId>。
    dqApp::DumpTileFetcher fetcher(dumpRoot);
    ASSERT_TRUE(fetcher.isValid());
    EXPECT_EQ(1u, fetcher.getTreeCount());
    EXPECT_EQ(7u, fetcher.getTileCount());

    StubTree stubTree;
    StubTile tile(stubTree);
    FetchResult got;
    fetchAndWait(fetcher, dumpRoot + "/" + kCompatTreeId + "/-b-0-0-0-0-1",
                 tile, got);
    ASSERT_TRUE(got.completed) << got.error;
    EXPECT_FALSE(got.errored);
    EXPECT_EQ(&tile, got.tile);  // 同一 Tile 传回（ITileFetcher 契约）
    EXPECT_EQ(4304u, got.data.size());
    // 完整性：byteLength（manifest 4304）已由 fetcher 校验（错误则走 onError）；
    // 字节同一性：与磁盘文件直读逐字节相等（强于哈希相等——见文件头降级登记）。
    auto disk = readAllBytes(dumpRoot + "/files/12.imdl");
    ASSERT_EQ(4304u, disk.size()) << "asset files/12.imdl missing/truncated";
    EXPECT_EQ(disk, got.data);

    // url 形态 B：ImdlTileTree::contentUrl 既有组合约定（<treeId>/<contentId>，
    // 无 dumpRoot 前缀）——同末两段键解析，同字节。
    StubTile tile2(stubTree);
    FetchResult got2;
    fetchAndWait(fetcher, kCompatTreeId + "/-b-0-0-0-0-1", tile2, got2);
    ASSERT_TRUE(got2.completed) << got2.error;
    EXPECT_EQ(disk, got2.data);

    // ④ 未知 contentId：onError（NotFound 语义），onComplete 不触。
    StubTile tile3(stubTree);
    FetchResult miss;
    fetchAndWait(fetcher, dumpRoot + "/" + kCompatTreeId + "/-b-9-9-9-9-9",
                 tile3, miss);
    EXPECT_FALSE(miss.completed);
    EXPECT_TRUE(miss.errored);
    EXPECT_EQ(&tile3, miss.tile);
    EXPECT_FALSE(miss.error.empty());

    // props.rootTile.contentId（"0/0/0/0/1"）不是瓦键（Task 1 评审钉死①）——
    // 以它组 url 同样 NotFound（末两段 "0"/"1" 落空）。
    StubTile tile4(stubTree);
    FetchResult miss2;
    fetchAndWait(fetcher, kCompatTreeId + "/0/0/0/0/1", tile4, miss2);
    EXPECT_FALSE(miss2.completed);
    EXPECT_TRUE(miss2.errored);
}

// ---------------------------------------------------------------------------
// 多树全量装载 + iModel 元数据消费（instances60-v1 驱动——阶段1 M-E Task 2）。
// ---------------------------------------------------------------------------
// Authored: no reference test exists in itwinjs-core for dump loading（dump
// 为本仓采集资产；多树迭代对齐参考侧 PrimaryTreeSupplier 对
// requestTileTreeProps 返回树的逐树 createTileTree 消费面
// PrimaryTileTree.ts:63-80；iModelInfo 的范围语义对齐 iModel 项目范围经
// fit 视图消费的宿主侧用途）。instances60-v1 实测 1 树（如实断言——trees()
// 迭代面由 DisplayTestApp 挂载入口的多树装载消费）。
TEST(DumpTileFetcherTest, Instances60LoadsAllTreesWithIModelInfo)
{
    std::string const dumpRoot = kDumpRoot + "/instances60-v1";

    // ① load：trees/tiles 计数与 manifest 一致（instances60-v1 实态：
    //    stats 1 树 / 3587 瓦——BFS 最大完整前缀，§11.11 只读资产）。
    auto props = dqApp::DumpTileTreeProps::load(dumpRoot);
    ASSERT_TRUE(props.has_value()) << "manifest load failed: " << dumpRoot;
    EXPECT_EQ(1u, props->getTreeCount());
    EXPECT_EQ(3587u, props->getTileCount());

    // trees() 迭代 == manifest trees[]（多树全量装载的驱动面——与
    // getTreeCount() 同域，逐条可达）。
    auto const& trees = props->trees();
    ASSERT_EQ(props->getTreeCount(), trees.size());
    ASSERT_FALSE(trees.empty());
    EXPECT_EQ("25_1d-E:6_0x1c", trees[0].treeId);
    EXPECT_EQ("dfac2750-4c3e-41de-afe7-5f4d97375006", trees[0].iModelId);
    EXPECT_EQ(2424832u, trees[0].formatVersion);  // 37.0（major<<16）
    EXPECT_EQ("files/6.json", trees[0].propsFile);

    // ② iModelInfo()：instances60-v1 采集时 provenance.iModel 缺口（sweep
    //    的 extents 取空）——优雅返回空（nullopt）。登记：iModel 级元数据
    //    载体待采集工具补齐后落地（挂载入口回退树 contentRange 并集 fit）。
    EXPECT_FALSE(props->iModelInfo().has_value());

    // ③ 每树 byTreeId 均可达 root props（instances60 树 props 钉死值——
    // TileProps.ts 字段映射面）。
    for (auto const& entry : trees) {
        auto tree = props->byTreeId(entry.treeId);
        ASSERT_TRUE(tree.has_value()) << "byTreeId failed: " << entry.treeId;
        EXPECT_FALSE(tree->rootTile.contentId.empty());
        EXPECT_FALSE(tree->rootTile.range.isNull());
    }
    auto tree = props->byTreeId(trees[0].treeId);
    ASSERT_TRUE(tree.has_value());
    EXPECT_EQ(2048u, tree->metadata.tileScreenSize);
    EXPECT_EQ(2424832u, tree->metadata.formatVersion);
    EXPECT_DOUBLE_EQ(-88.05017416331312, tree->metadata.contentRange.low.x);
    EXPECT_DOUBLE_EQ(-41.523859496768054, tree->metadata.contentRange.low.y);
    EXPECT_DOUBLE_EQ(-26.906429488189204, tree->metadata.contentRange.low.z);
    EXPECT_DOUBLE_EQ(-69.89486114832899, tree->metadata.contentRange.high.x);
    EXPECT_DOUBLE_EQ(-32.961938138659534, tree->metadata.contentRange.high.y);
    EXPECT_DOUBLE_EQ(-21.358517128951533, tree->metadata.contentRange.high.z);
    EXPECT_EQ(2048.0, tree->rootMaximumSize);
    EXPECT_EQ("0/0/0/0/1", tree->rootTile.contentId);
    EXPECT_FALSE(tree->rootTile.isLeaf);
}

// ---------------------------------------------------------------------------
// iModelInfo() 的 provenance.iModel 解析契约 + 缺省优雅回退。
// 合成最小 manifest（写系统临时目录——不动只读资产 §11.11；两形态：
// 有 iModel 字段（解析钉死值）/ 无 iModel 字段（nullopt，既有三 dump 实态））。
// ---------------------------------------------------------------------------
TEST(DumpTileFetcherTest, IModelInfoParsesProvenanceWithGracefulFallback)
{
    namespace fs = std::filesystem;
    fs::path const dir =
        fs::temp_directory_path() / "danqing-dump-imodelinfo-test";
    fs::remove_all(dir);
    fs::create_directories(dir / "files");
    {
        std::ofstream out(dir / "files" / "0.json");
        out << R"({"id":"tree-0","rootTile":{"contentId":"0/0/0/0/1","range":{"low":[0,0,0],"high":[1,1,1]}}})";
    }
    {
        std::ofstream out(dir / "manifest.json");
        out << R"({"provenance":{"iModel":{"name":"Synthetic.ibim","extents":{"low":[-1.5,-2.5,-3.5],"high":[1.5,2.5,3.5]}}},"stats":{"trees":1,"tiles":0},"trees":[{"treeId":"tree-0","propsFile":"files/0.json"}],"tiles":[]})";
    }
    auto props = dqApp::DumpTileTreeProps::load(dir.string());
    ASSERT_TRUE(props.has_value());
    auto info = props->iModelInfo();
    ASSERT_TRUE(info.has_value()) << "provenance.iModel must be consumed when present";
    EXPECT_EQ("Synthetic.ibim", info->name);
    EXPECT_DOUBLE_EQ(-1.5, info->extents.low.x);
    EXPECT_DOUBLE_EQ(-2.5, info->extents.low.y);
    EXPECT_DOUBLE_EQ(-3.5, info->extents.low.z);
    EXPECT_DOUBLE_EQ(1.5, info->extents.high.x);
    EXPECT_DOUBLE_EQ(2.5, info->extents.high.y);
    EXPECT_DOUBLE_EQ(3.5, info->extents.high.z);

    // 缺省面：provenance.iModel 缺失（compatseed/mirukuru/instances60 三
    // dump 的实态）→ nullopt，装载其余面不受影响。
    {
        std::ofstream out(dir / "manifest.json");
        out << R"({"provenance":{"seed":"x.ibim"},"stats":{"trees":1,"tiles":0},"trees":[{"treeId":"tree-0","propsFile":"files/0.json"}],"tiles":[]})";
    }
    auto noInfo = dqApp::DumpTileTreeProps::load(dir.string());
    ASSERT_TRUE(noInfo.has_value());
    EXPECT_FALSE(noInfo->iModelInfo().has_value());
    fs::remove_all(dir);
}

// ---------------------------------------------------------------------------
// byTreeId 多键分发（mirukuru-v1：2 树/1 瓦）+ optional 字段缺省路径。
// ---------------------------------------------------------------------------
TEST(DumpTileFetcherTest, MultiTreeDispatchByTreeId)
{
    std::string const dumpRoot = kDumpRoot + "/mirukuru-v1";
    auto props = dqApp::DumpTileTreeProps::load(dumpRoot);
    ASSERT_TRUE(props.has_value()) << "manifest load failed: " << dumpRoot;
    EXPECT_EQ(2u, props->getTreeCount());
    EXPECT_EQ(1u, props->getTileCount());

    // 树 A（0x1c）：contentRange 存在、根 maximumSize 2048、非叶。
    auto a = props->byTreeId("25_1d-E:6_0x1c");
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ("25_1d-E:6_0x1c", a->id);
    EXPECT_DOUBLE_EQ(-918.9385374040576, a->metadata.contentRange.low.x);
    EXPECT_DOUBLE_EQ(-632.1885374040576, a->metadata.contentRange.high.x);
    EXPECT_DOUBLE_EQ(2048.0, a->rootMaximumSize);
    EXPECT_FALSE(a->rootTile.isLeaf);

    // 树 B（0x28）：空树形态——无 contentRange 字段（→ null）、根 maximumSize 0
    // （undisplayable 语义）、isLeaf true、range 为 ±1.79e308 反转域（fromJSON
    // 不重排——Range3d(low, high) 直构，isNull() 可见）。
    auto b = props->byTreeId("25_1d-E:6_0x28");
    ASSERT_TRUE(b.has_value());
    EXPECT_TRUE(b->metadata.contentRange.isNull());
    EXPECT_EQ(0.0, b->rootMaximumSize);
    EXPECT_TRUE(b->rootTile.isLeaf);
    EXPECT_TRUE(b->rootTile.range.isNull());

    // 未知 treeId → nullopt（NotFound 语义）。
    EXPECT_FALSE(props->byTreeId("no-such-tree").has_value());

    // 字节回放按 (treeId, contentId) 双键分发：0x1c 的瓦命中、0x28 的同键
    // contentId NotFound（该树无瓦条目）。
    dqApp::DumpTileFetcher fetcher(dumpRoot);
    ASSERT_TRUE(fetcher.isValid());
    StubTree stubTree;

    StubTile tileA(stubTree);
    FetchResult gotA;
    fetchAndWait(fetcher, "25_1d-E:6_0x1c/-b-0-0-0-0-1", tileA, gotA);
    ASSERT_TRUE(gotA.completed) << gotA.error;
    EXPECT_EQ(1652u, gotA.data.size());  // manifest byteLength（files/6.imdl）
    auto disk = readAllBytes(dumpRoot + "/files/6.imdl");
    ASSERT_EQ(1652u, disk.size());
    EXPECT_EQ(disk, gotA.data);

    StubTile tileB(stubTree);
    FetchResult missB;
    fetchAndWait(fetcher, "25_1d-E:6_0x28/-b-0-0-0-0-1", tileB, missB);
    EXPECT_FALSE(missB.completed);
    EXPECT_TRUE(missB.errored);
}

// ---------------------------------------------------------------------------
// 错误路径：dump 缺失/坏 → load 失败、fetcher 失效、fetch 走 onError（不崩）。
// ---------------------------------------------------------------------------
TEST(DumpTileFetcherTest, MissingDumpFailsLoadGracefully)
{
    std::string const badRoot = kDumpRoot + "/no-such-dump";
    EXPECT_FALSE(dqApp::DumpTileTreeProps::load(badRoot).has_value());

    dqApp::DumpTileFetcher fetcher(badRoot);
    EXPECT_FALSE(fetcher.isValid());
    EXPECT_EQ(0u, fetcher.getTreeCount());
    EXPECT_EQ(0u, fetcher.getTileCount());

    StubTree stubTree;
    StubTile tile(stubTree);
    FetchResult out;
    fetchAndWait(fetcher, "some/tree", tile, out);
    EXPECT_FALSE(out.completed);
    EXPECT_TRUE(out.errored);
}

// ---------------------------------------------------------------------------
// 零网络结构锁（§8.2 + TD-24 清退回归锁）。
//
// Authored: no reference test exists（参考的取数在宿主 TS 侧）；本锁钉的是
// 用户 2026-09-27 零网络指令的 DoD 3（docs/阶段1-MD-RPC真实数据回放-实现计划
// -2026-09-27.md「完成定义」3：全仓零网络原语 grep 零命中）。扫描域 = 六个
// 代码目录（含本测试自身所在仓面）；docs/CLAUDE.md 的登记文本不在扫描域。
// 令牌在运行时由两半拼接——本文件源码不得出现任何完整令牌字面量（否则自锁）。
// ---------------------------------------------------------------------------
TEST(DumpTileFetcherTest, ZeroNetworkByConstruction)
{
    namespace fs = std::filesystem;
    std::string const root = DANQING_TEST_ASSET_ROOT;

    // 令牌两半（防自锁：拼接在运行时发生）。覆盖 Qt 网络类/WinHTTP/WinINet/
    // curl/Winsock 名解析/CMake 网络组件。
    struct Token { char const* a; char const* b; };
    Token const tokens[] = {
        {"QNet", "work"},            // Qt 网络类族（access manager/reply/request）
        {"<QtNet", "work"},          // Qt 网络头 include
        {"win", "http"},             // WinHTTP（小写）
        {"Win", "Http"},             // WinHTTP API 族
        {"Internet", "Open"},        // WinINet
        {"URLDownload", "ToFile"},   // WinINet 下载
        {"lib", "curl"},             // curl 库名
        {"curl", "_easy"},           // curl C API
        {"CURL", "OPT"},             // curl 选项宏
        {"WSA", "Startup"},          // Winsock
        {"getaddr", "info"},         // 名解析
        {"Qt6::", "Network"},        // CMake 网络组件链接
        {"Qt::", "Network"},         // CMake 网络组件链接（旧式）
    };

    char const* dirs[] = {"dqBase", "dqCommon", "dqGeom", "dqRender", "dqApp",
                          "samples"};
    char const* skipNames[] = {"build", "third_party", ".git", "node_modules",
                               "assets"};

    auto isSkipped = [&skipNames](fs::path const& p) {
        for (auto const& part : p) {
            for (auto const* skip : skipNames)
                if (part == skip)
                    return true;
        }
        return false;
    };

    auto isScanned = [](fs::path const& p) {
        std::string const name = p.filename().string();
        if (name == "CMakeLists.txt")
            return true;
        std::string const ext = p.extension().string();
        return ext == ".h" || ext == ".hpp" || ext == ".cpp" || ext == ".cc"
            || ext == ".c" || ext == ".mm";
    };

    long scanned = 0;
    for (auto const* dir : dirs) {
        fs::path const base = fs::path(root) / dir;
        if (!fs::exists(base))
            continue;
        for (auto const& entry : fs::recursive_directory_iterator(base)) {
            if (!entry.is_regular_file() || isSkipped(entry.path())
                || !isScanned(entry.path()))
                continue;
            std::ifstream in(entry.path(), std::ios::binary);
            ASSERT_TRUE(in.is_open()) << entry.path().string();
            std::string text{std::istreambuf_iterator<char>(in),
                             std::istreambuf_iterator<char>()};
            ++scanned;
            for (auto const& tok : tokens) {
                std::string const needle = std::string(tok.a) + tok.b;
                size_t const pos = text.find(needle);
                EXPECT_EQ(std::string::npos, pos)
                    << "zero-network violation: " << needle << " in "
                    << entry.path().string();
            }
        }
    }
    // 仪器自检（§11.11）：扫描必须真的覆盖了代码面——文件数为 0 = 扫描域配错
    // （路径失效等），锁空转。
    EXPECT_GT(scanned, 100) << "zero-network scan covered nothing; check "
                               "DANQING_TEST_ASSET_ROOT";
}

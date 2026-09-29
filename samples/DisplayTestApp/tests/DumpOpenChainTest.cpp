// DumpOpenChainTest — 已存 iModel 打开链的流程同构锁（M-H Task 3 判据核心）。
//
// 判据 = **请求序列同构**：DanQing 打开已存数据包（DumpOpenHelper——
// 连接 → 默认视图装载 → changeView → 按 modelSelector 逐 model 树引用 →
// 视口选择驱动逐 TileID）时的数据请求序列，与采集期 wrapper 记录的 DTA
// 实际序列同构（同键同序——启动竞态造成的采集缺口[drill 根/-b-1 字节]
// 按登记面处理，见各锁判据）。
//
// Authored: no reference test exists in itwinjs-core/imodel-native for
//           open-chain dump-replay sequence isomorphism（§5(f)——参考的打开
//           链覆盖在 DTA 集成测试，无离线回放对应物；渲染像素回归授权
//           §5(g)：复现配方 = 打开链 + 确定性 zoom 泵；证据链 = fetcher
//           requestLog 对账 + readPixels 锚）。dump 资产只读（§11.11），
//           钉值 = 首绿实测（阈值注释在各断言处）。
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

std::string const kDumpRoot = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";

struct QtEnvOpenChain {
    QtEnvOpenChain()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvOpenChain s_qtOpenChain;

void spin(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// "内容像素"（与背景差异显著）计数 + 包围盒 + 质心——与
// RpcDumpRenderTest.cpp:99-128 contentStats 同源同构（判别阈值 90 先验
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

// 角落探针——RpcDumpRenderTest.cpp:131-147 同源。
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

// 树内 graphics 就绪瓦计数——RpcDumpRenderTest.cpp:173-182 同源。
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
    printf("[OPEN-CHAIN] frame dumped to %s\n", path);
}

// 泵至静默（M-G(2) 强制选择帧形——NotFound 交付不触发失效级联，:299-301
// 回退等"需下一选择帧"的机制在突发帧停后饥饿；强制帧把请求面推到不动点，
// 键级幂等不改变请求面——RpcDumpRenderTest.cpp 泵注同源）。
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

// 绕世界点 center 的 kZoom 缩放取景（M-G drill 泵配方同族——LookAtVolume
// 向心缩（体积中心 = 当前视域中心点）≈ 参考 viewport.zoom 的绕视口中心
// 缩放语义；旋转保持——RpcDumpRenderTest.cpp 相 2 钻取配方同源）。
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

dqGeom::Point3d viewCenterWorld(dqApp::ViewState3d const* view3d)
{
    return view3d->getCenter();
}

}  // namespace

// ---------------------------------------------------------------------------
// 锁 1：instances60 打开链同构（单 model ⇒ 单树；saved 视图非 fit）。
// ---------------------------------------------------------------------------
// 判据（首绿实测钉值）：
// ① 初始视图参数 = saved ViewState（origin[16.459991045301607,
//   -6.777938783499056, -8.142933015137151] / extents[19.32567417568155,
//   10.797911681847634, 19.557384678145382] 逐项 double 相等、cameraOn=false、
//   rotation = saved angles 的 YPR 矩阵逐项——**非 fit**；装载面（fix 前
//   clone）与 viewport 面（参考窗口 aspect fix 后）两面分钉）；
// ② 树装载 = modelSelector 驱动：1 model（0x1c）→ 1 树
//   "25_1d-E:6_0x1c"（treeLoadLog 实钉——请求序 = 树 props → 逐 TileID）；
// ③ **请求序列同构（主判据）**：saved 视图请求面 = 恰 1 枚
//   "-b-2-0-0-0-1"（Completed——== instances60-drill-v1 manifest tiles[0]
//   采集首键，同键同字节；根/-b-1 零请求 = maxInitialTilesToSkip=3 的
//   canSkip 穿透[IModelTile.ts:265 参考跳过语义]，无 NotFound 子故无
//   :299-301 回退）；miss=0；ready=1（Completed→graphics 对账）；
// ④ 像素锚：saved 视图下 60 实例上屏（location 消费后内容落
//   projectExtents 域 = saved 视域）——首绿实测 content=410953 px（帧
//   14.68%）、bbox (149,365)-(1968,896)、质心 (1097,655) 对帧心 (1000,700)、
//   四角探针 ≤1%；阈值 = 首绿 0.2×（82000）。
TEST(DumpOpenChain, OpensInstances60WithReferenceIsomorphicSequence)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "DumpOpenChain";
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

    // ② 树装载 = modelSelector 驱动（1 model → 1 树；treeLoadLog 实钉）。
    ASSERT_EQ(1u, opened->treeLoadLog.size());
    EXPECT_EQ("25_1d-E:6_0x1c", opened->treeLoadLog[0]);
    ASSERT_EQ(1u, opened->trees.size());

    // ① 初始视图参数 = saved ViewState——两个面：
    //   (a) 视图装载产物逐项 = saved（**非 fit、非 aspect fix**）——从
    //       ViewList 重取 clone（缓存的 load 原样产物，ViewPicker.ts:49-50；
    //       opened->viewState 与 viewport 同实例、已被 SetupFromView 的
    //       aspect fix 原地改写——ViewState::SetExtents 直写成员）；
    //   (b) viewport 当前视图 = saved + 参考的窗口 aspect fix
    //       （Viewport.setupFromView → doSetupFromView Viewport.ts:2041-2042 调
    //       fixAspectRatio ViewState.ts:868-880——extents.y 按窗口 aspect
    //       调整、origin 随视域中心平移；x/z/rotation/cameraOn 不受 fix
    //       影响——钉这四项锁"非 fit"）。
    {
        dqApp::ViewList reloadViews = dqApp::ViewList::create(opened->connection.Get());
        auto reloaded = reloadViews.getDefaultView(opened->connection.Get());
        ASSERT_TRUE(reloaded.IsValid());
        auto const* loaded = reloaded->AsViewState3d();
        ASSERT_NE(loaded, nullptr);
        auto const lorg = loaded->GetOrigin();
        EXPECT_NEAR(16.459991045301607, lorg.x, 1.0e-9);
        EXPECT_NEAR(-6.777938783499056, lorg.y, 1.0e-9);
        EXPECT_NEAR(-8.142933015137151, lorg.z, 1.0e-9);
        auto const lext = loaded->GetExtents();
        EXPECT_NEAR(19.32567417568155, lext.x, 1.0e-9);
        EXPECT_NEAR(10.797911681847634, lext.y, 1.0e-9);
        EXPECT_NEAR(19.557384678145382, lext.z, 1.0e-9);
        EXPECT_FALSE(loaded->IsCameraOn());
    }
    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        auto const ext = view3d->GetExtents();
        EXPECT_NEAR(19.32567417568155, ext.x, 1.0e-6);
        EXPECT_NEAR(19.557384678145382, ext.z, 1.0e-6);
        // aspect fix 面：extents.y = x / windowAspect（1000/700）。
        EXPECT_NEAR(ext.x * 700.0 / 1000.0, ext.y, 1.0e-6);
        EXPECT_FALSE(view3d->IsCameraOn());
        double const kExpectedRot[9] = {
            -0.94054349819232985, -0.33967326654909852, 0.0,
            0.096136201592062182, -0.26619780905027829, 0.95911237985977538,
            -0.32578483505464967, 0.90208691291288379, 0.28302551616368093,
        };
        for (int i = 0; i < 9; ++i)
            EXPECT_NEAR(kExpectedRot[i], view3d->getRotation().coffs[static_cast<size_t>(i)],
                        1.0e-9)
                << "rotation differs at coffs[" << i << "]";
    }

    // ③ 泵至静默（saved 视图冷启动驱动——请求面 = 视口自然请求）。
    PumpContext ctx{&view, &*opened};
    int const quiesceIter = pumpToQuiesce(ctx);
    ASSERT_GE(quiesceIter, 0)
        << "open-chain tile load never quiesced (log="
        << opened->fetcher->requestLog().size() << " ready=" << ctx.readyTiles << ")";
    view.getUeViewport()->RenderFrame();

    // --- 对账：请求日志 vs manifest 联合域（sweep ∪ drill——多根 fetcher） ---
    std::set<std::string> manifestKeys;
    for (auto const& t : opened->props->trees())
        (void)t;
    {
        // 联合域 = 主根 manifest + drill manifest（fetcher 的多根查找域）。
        auto drillManifest = dqApp::loadDumpManifest(kDumpRoot + "/instances60-drill-v1");
        ASSERT_TRUE(drillManifest.has_value());
        for (auto const& t : opened->props->trees())
            (void)t;
        for (auto const& t : dqApp::loadDumpManifest(kDumpRoot + "/instances60-v1")->tiles)
            manifestKeys.insert(t.treeId + "/" + t.contentId);
        for (auto const& t : drillManifest->tiles)
            manifestKeys.insert(t.treeId + "/" + t.contentId);
    }
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
    printf("[OPEN-CHAIN] instances60 reconcile: requested=%zu (completed=%zu "
           "notFound=%zu error=%zu) ready=%ld/%ld\n",
           log.size(), numCompleted, numNotFound, numError, ctx.readyTiles,
           ctx.totalTiles);
    for (auto const& rec : log)
        printf("[OPEN-CHAIN]   log %d %s\n", static_cast<int>(rec.outcome),
               rec.contentId.c_str());

    // ③a 零 Error（dump 资产自洽性破口）+ 零重复请求。
    EXPECT_EQ(0u, numError) << "dump asset integrity break";
    EXPECT_EQ(requestedKeys.size(), log.size()) << "duplicate tile requests";
    // ③b Completed ⊆ 联合域（请求键覆写命中采集键域——同键同字节）。
    for (auto const& rec : log) {
        if (rec.outcome != dqApp::DumpTileFetcher::DumpFetchOutcome::Completed)
            continue;
        EXPECT_TRUE(manifestKeys.count(rec.treeId + "/" + rec.contentId) > 0)
            << "Completed key not in the union manifest domain: " << rec.contentId;
    }
    EXPECT_GT(numCompleted, 0u) << "no manifest key was ever requested";
    // ③c-1 **请求序列同构实钉（本锁主判据——首绿实测）**：saved 视图请求
    //    面 = 恰 1 枚请求 "-b-2-0-0-0-1"（Completed）。采集侧同构证据 =
    //    instances60-drill-v1 manifest tiles[0].contentId（wrapper 在默认
    //    视图安装后的首捕获键）——同键（同字节——sweep/drill 双根同一
    //    后端产物）。根/-b-1 零请求：saved 视图下根/d1 TooCoarse 且
    //    depth<maxInitialTilesToSkip=3 恒 canSkip 穿透（IModelTile.ts:265
    //    ——M-G 登记的参考跳过语义），d2 是首个 Visible 瓦被请求
    //    （:214-217 insertMissing）；无 NotFound 子 → 无 :299-301 回退 →
    //    根键不出现（与 M-G 锁的"根经回退 miss"形态分属两视图——fit 下
    //    d1 Visible 才触发该路径）。
    ASSERT_EQ(1u, log.size()) << "saved-view request face drifted from the "
                                 "captured default-view face";
    EXPECT_EQ("-b-2-0-0-0-1", log[0].contentId);
    EXPECT_EQ(dqApp::DumpTileFetcher::DumpFetchOutcome::Completed, log[0].outcome);
    // ③c-2 miss 集实钉：空（saved 视图下无域外派生请求——无细分 miss、
    //    无回退、无过冲）。
    EXPECT_EQ(0u, numNotFound) << "unexpected misses at the saved view";
    // ③d 树侧终态：-b-2 就绪（Completed→graphics 对账）；根/-b-1 NotLoaded
    //    （canSkip 穿透——视口本不请求的形态，参考同构）。
    EXPECT_EQ(1l, ctx.readyTiles);
    EXPECT_TRUE(completedIds.count("-b-2-0-0-0-1") > 0);
    // ③c-3 miss 集形态（V4 派生形/根形——M-G 锁同款形验；实测为空集，
    //    形验保留作回归门）。
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
        EXPECT_TRUE(derivedForm) << "miss key is not a V4 request-key shape: " << m;
    }

    // ④ 像素锚（saved 视图——location 消费后 60 实例落 projectExtents 域）。
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    dumpBmp(frame, w, h,
            DANQING_TILE_ASSETS_DIR "/../../build/open-chain-instances60.bmp");
    long count = 0;
    double cx = 0, cy = 0;
    uint32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
    ASSERT_TRUE(contentStats(frame, w, h, count, cx, cy, minX, maxX, minY, maxY))
        << "no content rendered at the saved view — location consumption or the "
           "open-chain view application broken (see BMP)";
    printf("[OPEN-CHAIN] instances60 saved-view: content=%ld px (%.3f%% of %ux%u) "
           "bbox=(%u,%u)-(%u,%u) centroid=(%.0f,%.0f) frame center=(%.0f,%.0f)\n",
           count, 100.0 * count / (static_cast<double>(w) * h), w, h,
           minX, minY, maxX, maxY, cx, cy, w / 2.0, h / 2.0);
    // 内容存活阈值 = 首绿实测 410953 px（帧 14.68%）的 0.2×——内容整体
    //    消失/取景链断/location 包裹失效必红；首绿 bbox (149,365)-(1968,896)
    //    展布满幅、质心 (1097,655) 对帧心 (1000,700) 偏移 (97,-45)——
    //    实例群 world x 分布偏内容域右半的屏幕投影（WHERE 语义）。
    EXPECT_GE(count, 82000l)
        << "saved-view content barely visible — location wrap or saved-view "
           "application broken (threshold pinned at 0.2x of the first GREEN "
           "measurement 410953)";
    // WHERE：质心中央带（±20%——首绿偏移 4.9%/3.2% 的 4× 余量）+ 四角背景
    //（§11.11 位置断言——首绿四角探针全 ≤1%）。
    EXPECT_LT(std::abs(cx - w / 2.0), w * 0.2) << "content centroid x off-center";
    EXPECT_LT(std::abs(cy - h / 2.0), h * 0.2) << "content centroid y off-center";
    uint32_t const box = std::max(1u, std::min(w, h) * 6 / 100);
    EXPECT_LE(cornerContentRatio(frame, w, h, 0, 0, box), 0.01);
    EXPECT_LE(cornerContentRatio(frame, w, h, w - box, 0, box), 0.01);
    EXPECT_LE(cornerContentRatio(frame, w, h, 0, h - box, box), 0.01);
    EXPECT_LE(cornerContentRatio(frame, w, h, w - box, h - box, box), 0.01);

    view.getUeViewport()->DropTiledGraphicsProvider(opened->provider.get());
    view.close();
    spin(200);
}

// ---------------------------------------------------------------------------
// 锁 2：joeshouse 打开链同构（10 model ⇔ 10 树；saved 等轴测视图；zoom 泵
// 复现 joeshouse-drill-v1 采集配方——判据 = 瓦键请求序与 drill 15 键序同构）。
// ---------------------------------------------------------------------------
// 判据（首绿实测钉值见各断言处）：
// ① 初始视图 = saved ViewState（等轴测 rotation 逐项——
//   YawPitchRollAnglesTest.JoesHouseSavedAnglesProduceIsometricRotation 同
//   钉值；origin/extents 装载面逐项）；
// ② 10 model ⇔ 10 树逐一对账（treeLoadLog = modelSelector 派生 treeId 全
//   集——model⇔tree 对账表与 IModelTileTreeIdTest.
//   CapturedTreeIdsMatchModelSelectorDerivation 同域）；
// ③ **瓦键请求序同构（主判据）**：saved 视图 + zoom 泵（kZoom=2^n 绕内容
//   质心 [7.302,4.264,4.496]——= saved 视口中心的世界点，drill 采集的
//   viewport.zoom(undefined,0.5) 绕视口中心语义）复现 drill 配方：
//   drill 15 键全部 Completed（3 键[-b-2-0-0-0-{10,10,20}]在 sweep 域
//   外——经 fallback drill 根命中，Task 2 多根合并面）+ **每树内部请求
//   子序与 drill 序该树子序同构**（0x3f: 1→2→4→8→10→20；0x4d:
//   1→2→4→8→10——跨树交错序是 DTA 异步 RPC 完成序，回放侧单线程同步
//   fetch 序由选择帧遍历决定，序差如实登记不烤进判据）+ 回放特有键
//   实钉（4 叶根树的根键——drill 采集的 wrapper 竞态缺口[打开请求先于
//   wrapper 安装]，sweep 域内 Completed）；
// ④ 像素锚：saved 视图内容上屏（location 消费后 10 树落 projectExtents
//   域——首绿实测钉死；§11.11 位置断言）。
TEST(DumpOpenChain, OpensJoesHouseWithTenModelTrees)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "DumpOpenChain";
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

    // ② 10 model ⇔ 10 树逐一对账（treeLoadLog——modelSelector 驱动序）。
    {
        char const* const kExpectedTrees[] = {
            "25_1d-E:6_0x26", "25_1d-E:6_0x3d", "25_1d-E:6_0x3f",
            "25_1d-E:6_0x41", "25_1d-E:6_0x43", "25_1d-E:6_0x45",
            "25_1d-E:6_0x47", "25_1d-E:6_0x49", "25_1d-E:6_0x4b",
            "25_1d-E:6_0x4d",
        };
        ASSERT_EQ(10u, opened->treeLoadLog.size());
        ASSERT_EQ(10u, opened->trees.size());
        for (size_t i = 0; i < 10; ++i)
            EXPECT_EQ(kExpectedTrees[i], opened->treeLoadLog[i]) << "tree[" << i << "]";
    }

    // ① 初始视图 = saved（等轴测 rotation 逐项 + 装载面 origin/extents）。
    {
        dqApp::ViewList reloadViews = dqApp::ViewList::create(opened->connection.Get());
        auto reloaded = reloadViews.getDefaultView(opened->connection.Get());
        ASSERT_TRUE(reloaded.IsValid());
        auto const* loaded = reloaded->AsViewState3d();
        ASSERT_NE(loaded, nullptr);
        auto const lorg = loaded->GetOrigin();
        EXPECT_NEAR(-0.1890538726091618, lorg.x, 1.0e-9);
        EXPECT_NEAR(23.970020102289883, lorg.y, 1.0e-9);
        EXPECT_NEAR(-13.95823848302306, lorg.z, 1.0e-9);
        auto const lext = loaded->GetExtents();
        EXPECT_NEAR(38.46216332077429, lext.x, 1.0e-9);
        EXPECT_NEAR(20.16090372071284, lext.y, 1.0e-9);
        EXPECT_NEAR(35.41381746319353, lext.z, 1.0e-9);
        EXPECT_FALSE(loaded->IsCameraOn());

        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        double const kExpectedRot[9] = {
            0.70710678118654779, -0.70710678118654724, -1.1102230246251565e-16,
            0.40824829046386207, 0.40824829046386224, 0.81649658092772692,
            -0.57735026918962606, -0.57735026918962662, 0.57735026918962451,
        };
        for (int i = 0; i < 9; ++i)
            EXPECT_NEAR(kExpectedRot[i], view3d->getRotation().coffs[static_cast<size_t>(i)],
                        1.0e-9)
                << "rotation differs at coffs[" << i << "]";
    }

    // ③ 相 1：saved 视图泵至静默（打开面请求——各树 depth-2 跳级键 + 4 叶
    //    树根键[回放特有——wrapper 竞态缺口的采集面登记]）。
    PumpContext ctx{&view, &*opened};
    int const quiesceIter = pumpToQuiesce(ctx);
    ASSERT_GE(quiesceIter, 0)
        << "open-chain tile load never quiesced (log="
        << opened->fetcher->requestLog().size() << " ready=" << ctx.readyTiles << ")";
    view.getUeViewport()->RenderFrame();
    size_t const openFaceLogSize = opened->fetcher->requestLog().size();
    printf("[OPEN-CHAIN] joeshouse open face: requested=%zu ready=%ld/%ld\n",
           openFaceLogSize, ctx.readyTiles, ctx.totalTiles);

    // ③-open **打开面实钉（首绿实测）**：saved 视图请求面 = 恰 10 键——
    //    4 叶根树的根键（0x26/0x3d/0x43/0x4b 的 "-b-0-0-0-0-1"——叶根
    //    Visible 被请求[IModelTile.ts:214-217]；**回放特有**——drill 采集
    //    的 wrapper 竞态缺口[打开请求先于 wrapper 安装]，sweep 域内
    //    Completed）+ 6 棵非叶树的 depth-2 跳级键（"-b-2-0-0-0-1"——
    //    根/d1 canSkip 穿透[maxInitialTilesToSkip=3]后首个 Visible 瓦；
    //    == drill 采集首波 6 键同集[tiles[0..5]在案]）。
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
            << "saved-view open face drifted from the captured first wave";
    }

    // ③ 相 2：zoom 泵（kZoom=2^n 绕内容质心——drill 采集配方）复现放大链。
    //    停止规则 = drill 15 键全 Completed 后再一级（过冲/终态形）。
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
    auto const* view3dNow = view.getUeViewport()->GetView()->AsViewState3d();
    ASSERT_NE(view3dNow, nullptr);
    dqGeom::Vector3d const baseDiag = view3dNow->GetExtents();  // aspect fix 后 saved
    dqGeom::Point3d const kContentCentroid =
        dqGeom::Point3d::From(7.302, 4.264, 4.496);  // drill targets[0]
    int firstFullLevel = -1, levelsRun = 0;
    for (int n = 1; n <= 10; ++n) {
        double const kZoom = static_cast<double>(1u << n);
        zoomViewAbout(view, kContentCentroid, baseDiag, kZoom);
        int const q = pumpToQuiesce(ctx);
        ASSERT_GE(q, 0) << "zoom level " << n << " never quiesced (log="
                        << opened->fetcher->requestLog().size() << ")";
        levelsRun = n;
        printf("[OPEN-CHAIN] joeshouse drill level %d (kZoom=%.0f): log=%zu "
               "connected=%zu ready=%ld/%ld\n",
               n, kZoom, opened->fetcher->requestLog().size(),
               connectedDrillCount(), ctx.readyTiles, ctx.totalTiles);
        if (connectedDrillCount() == drillKeys.size()) {
            if (firstFullLevel < 0)
                firstFullLevel = n;
            if (n > firstFullLevel)
                break;
        }
    }
    view.getUeViewport()->RenderFrame();

    // --- 对账：请求日志 vs drill 键域 + 联合 manifest 域 ---
    std::set<std::string> unionKeys;
    {
        auto sweepManifest = dqApp::loadDumpManifest(kDumpRoot + "/joeshouse-v1");
        auto drillManifest = dqApp::loadDumpManifest(kDumpRoot + "/joeshouse-drill-v1");
        ASSERT_TRUE(sweepManifest.has_value());
        ASSERT_TRUE(drillManifest.has_value());
        for (auto const& t : sweepManifest->tiles)
            unionKeys.insert(t.treeId + "/" + t.contentId);
        for (auto const& t : drillManifest->tiles)
            unionKeys.insert(t.treeId + "/" + t.contentId);
    }
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
    printf("[OPEN-CHAIN] joeshouse reconcile: requested=%zu (completed=%zu "
           "notFound=%zu error=%zu) levels=%d (full@%d) ready=%ld/%ld\n",
           log.size(), numCompleted, numNotFound, numError, levelsRun,
           firstFullLevel, ctx.readyTiles, ctx.totalTiles);
    for (auto const& rec : log)
        printf("[OPEN-CHAIN]   log %d %s/%s\n", static_cast<int>(rec.outcome),
               rec.treeId.c_str(), rec.contentId.c_str());

    EXPECT_EQ(0u, numError) << "dump asset integrity break";
    EXPECT_EQ(requestedKeys.size(), log.size()) << "duplicate tile requests";
    // ③a **drill 15 键全部 Completed（同构主判据）**——视口请求面 = 回放
    //    可达面（3 枚 sweep 域外键经 fallback drill 根命中）。
    for (auto const& key : drillKeys) {
        EXPECT_TRUE(completedKeys.count(key) > 0)
            << "drill key never connected (viewport chain not isomorphic to "
               "the captured request face): " << key;
    }
    // ③b **每树内部请求子序 = drill 序该树子序（同键同序）**：两大树放大
    //    链严格逐倍（采集序实测：0x3f 索引 0,7,8,10,12,14 = mult 1,2,4,8,10,
    //    20；0x4d 索引 1,6,9,11,13 = mult 1,2,4,8,10——drill manifest tiles[]
    //    在案）。
    auto assertTreeSubsequence = [&](std::string const& treeId,
                                     std::vector<std::string> const& expectedChain) {
        std::vector<std::string> actualChain;
        for (auto const& rec : log) {
            if (rec.treeId == treeId
                && rec.outcome == dqApp::DumpTileFetcher::DumpFetchOutcome::Completed)
                actualChain.push_back(rec.contentId);
        }
        // 回放侧该树 Completed 序（首次请求序——每键至多一条）必须包含
        // drill 子序为前缀（同键同序；放大过冲键[-b-2-0-0-0-20+]若出现为
        // 采集饱和点后的合法延伸——0x3f 的 -20 在 drill 域内已在链内）。
        std::set<std::string> seen;
        std::vector<std::string> dedup;
        for (auto const& id : actualChain) {
            if (seen.insert(id).second)
                dedup.push_back(id);
        }
        ASSERT_GE(dedup.size(), expectedChain.size())
            << treeId << " chain shorter than the captured chain";
        for (size_t i = 0; i < expectedChain.size(); ++i)
            EXPECT_EQ(expectedChain[i], dedup[i])
                << treeId << " chain diverges at position " << i;
    };
    assertTreeSubsequence("25_1d-E:6_0x3f",
                          {"-b-2-0-0-0-1", "-b-2-0-0-0-2", "-b-2-0-0-0-4",
                           "-b-2-0-0-0-8", "-b-2-0-0-0-10", "-b-2-0-0-0-20"});
    assertTreeSubsequence("25_1d-E:6_0x4d",
                          {"-b-2-0-0-0-1", "-b-2-0-0-0-2", "-b-2-0-0-0-4",
                           "-b-2-0-0-0-8", "-b-2-0-0-0-10"});

    // ③c **Completed 集实钉（首绿实测）** = 19 键 = drill 15 键 + 4 叶根
    //    树打开面根键；requested=22（零重复——每键至多一条）。
    EXPECT_EQ(19u, numCompleted) << "chain-connected set drifted";
    EXPECT_EQ(22u, log.size());
    // ③d **过冲 miss 实钉（首绿实测——采集饱和点后的合法延伸）**：
    //    "25_1d-E:6_0x4d/-b-2-0-0-0-20"（0x4d ×32——采集 0x4d 饱和在
    //    ×16[drill README 在案]，回放 kZoom=64 级把 m16 推过 SSE 派生
    //    m32——域外）；"25_1d-E:6_0x3f/-b-2-0-0-0-40" 与
    //    "25_1d-E:6_0x4d/-b-2-0-0-0-40"（×64——kZoom=128 过冲级）。放大
    //    链在视口面可无限延伸（M-G drill 的 ×20 过冲同族）——采集域外
    //    是采集 cap/饱和实态，非回放缺陷。miss 全为 V4 放大派生形
    //    （mult 段 hex——20/40 = ×32/×64）。
    ASSERT_EQ(3u, misses.size());
    EXPECT_EQ("25_1d-E:6_0x4d/-b-2-0-0-0-20", misses[0]);
    EXPECT_EQ("25_1d-E:6_0x3f/-b-2-0-0-0-40", misses[1]);
    EXPECT_EQ("25_1d-E:6_0x4d/-b-2-0-0-0-40", misses[2]);
    // ③e 泵形实钉（首绿实测——级数依赖视口/设备像素几何[采集
    //    1556×844 vs 回放 2000×1400]，harness 变更时按机制复核重钉——
    //    M-G drill 锁同款登记）：m32(0x3f) 于第 6 级（kZoom=64）连通
    //    （drill 15 键全），过冲级第 7 级后停泵。
    EXPECT_EQ(7, levelsRun);
    EXPECT_EQ(6, firstFullLevel);

    // ④ 像素锚（saved 视图——重设回 saved 取景读帧；location 消费后 10 树
    //    内容落 projectExtents 域）。重 load 的 clone 同样经 EMPTY refs 窗
    //    （占位 refs 会在锚定帧发 "<modelId>/0/0/0/0" NotFound 噪音——
    //    与打开链 ④b 同机制；不影响已定格的对账快照，但锚定帧应保持与
    //    打开链一致的 refs 形态）。
    {
        dqApp::SpatialTileTreeReferences::setCreateOverride(
            [](dqApp::SpatialViewState&) {
                return std::make_unique<dta::DumpOpenEmptyTileTreeReferences>();
            });
        dqApp::ViewList reloadViews2 = dqApp::ViewList::create(opened->connection.Get());
        auto reloaded2 = reloadViews2.getDefaultView(opened->connection.Get());
        dqApp::SpatialTileTreeReferences::clearCreateOverride();
        ASSERT_TRUE(reloaded2.IsValid());
        view.getUeViewport()->ChangeView(reloaded2);
        view.getUeViewport()->InvalidateController();
        spin(400);
        view.getUeViewport()->RenderFrame();
    }
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    dumpBmp(frame, w, h,
            DANQING_TILE_ASSETS_DIR "/../../build/open-chain-joeshouse.bmp");
    long count = 0;
    double cx = 0, cy = 0;
    uint32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
    ASSERT_TRUE(contentStats(frame, w, h, count, cx, cy, minX, maxX, minY, maxY))
        << "no joeshouse content rendered at the saved view (see BMP)";
    printf("[OPEN-CHAIN] joeshouse saved-view: content=%ld px (%.3f%% of %ux%u) "
           "bbox=(%u,%u)-(%u,%u) centroid=(%.0f,%.0f) frame center=(%.0f,%.0f)\n",
           count, 100.0 * count / (static_cast<double>(w) * h), w, h,
           minX, minY, maxX, maxY, cx, cy, w / 2.0, h / 2.0);
    // 内容存活阈值 = 首绿实测 1798847 px（帧 64.24%——saved 等轴测视图
    //    为框住房屋设计、10 树 location 平移后全内容上屏满幅）的 0.2×；
    //    WHERE = 质心中央带（首绿 (912,715) 对帧心 (1000,700) 偏移
    //    (-88,+15)——±20% 带 4× 余量）。bbox 满幅是 saved 取景的合法形态
    //    （不作四角背景断言——内容本就铺满）。
    EXPECT_GE(count, 360000l)
        << "saved-view content barely visible — threshold pinned at 0.2x of "
           "the first GREEN measurement 1798847";
    EXPECT_LT(std::abs(cx - w / 2.0), w * 0.2) << "content centroid x off-center";
    EXPECT_LT(std::abs(cy - h / 2.0), h * 0.2) << "content centroid y off-center";

    view.getUeViewport()->DropTiledGraphicsProvider(opened->provider.get());
    view.close();
    spin(200);
}
// ---------------------------------------------------------------------------
// 锁 3：instances60 实例球实心性（M-J(1) 透明回归锁）。
// ---------------------------------------------------------------------------
// 用户报告（M-J(1)）：instances60 打开链 saved 视图下 60 实例球全部呈
// 透明（此前 M-H 实心——build/mh4-inst-initial.png 同视图取证）。根因
// （取证链见 commit）：RenderCommands::addBatch 无条件置
// m_opaqueOverrides=m_translucentOverrides=true（只要 batch 带 LUT）——
// 偏离参考 RenderCommands.ts:668-677 的
// `viewFlags.transparency || overrides.anyViewIndependentTranslucent` 门 +
// overrides.anyOpaque/anyTranslucent 逐 feature 透明度覆盖语义（本资产
// instances.symbologyOverrides 全 60 实例 flags=0x02 仅 Rgb 位、alpha=255
// ——参考侧 anyOpaque=anyTranslucent=false → 不产生 Translucent pass 副本
// 绘制）。DanQing 的副本进了 Translucent pass 后撞上第二事实：surface
// 变体的 OIT 双输出（Translucency.ts addTranslucency assignFragData）未
// 移植，单输出预乘色写进 accum 附件 +
// blendFuncSeparate(One,Zero,One,OneMinusSrcAlpha) 把 accum.a 清成 0 →
// Composite 把不透明球像素替换成多片元累加色 = 洗白透明。
//
// Authored: no reference test exists in itwinjs-core/imodel-native for
//           dump-replay solid-rendering fidelity（§5(f)；渲染像素回归授权
//           §5(g) + §11.11 判据有效性：主判据 = 洗白淡彩像素计数（透明
//           失败模式的自由度 = 全带替换，淡彩判据直接钉住）；WHERE =
//           饱和色球内容展布 bbox（单球/局部盒不可能满足））。
// 复现配方 = 打开链（双根 fetcher）→ saved 视图泵至静默 → readPixels。
// RED-GREEN 双向验证：修复前 pastel=2035（stride2 采样，下同）RED /
// 修复后 55 GREEN（同一构建双向实测）。
//
// 既有锚登记（本任务取证）：本视图既有 content 计数锚（首绿 410953 /
// 阈值 82000）对透明不敏感——洗白态实测 410953 vs 实心态 231842 双双
// 过阈（洗白把像素推离黑背景反而抬高计数）。实心性维度由本锁补齐。
TEST(DumpOpenChain, Instances60SavedViewSpheresRenderSolid)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "DumpOpenChain";
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

    // 泵至静默（saved 视图请求面 = 视口自然请求——锁 1 同配方）。
    PumpContext ctx{&view, &*opened};
    int const quiesceIter = pumpToQuiesce(ctx);
    ASSERT_GE(quiesceIter, 0)
        << "open-chain tile load never quiesced (log="
        << opened->fetcher->requestLog().size() << " ready=" << ctx.readyTiles << ")";
    view.getUeViewport()->RenderFrame();

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    dumpBmp(frame, w, h,
            DANQING_TILE_ASSETS_DIR "/../../build/open-chain-instances60.bmp");

    // 像素分类（stride 2 采样——2000x1400 帧实测口径）：
    //   洗白淡彩 = max通道 ≥ 150 且通道散 ≤ 40（OIT 累加把重叠球像素推成
    //     低饱和亮色——实心态唯一来源是 ACS 三轴装饰的灰白轴，首绿 55）；
    //   饱和球色 = max ≥ 180 且散 ≥ 100（实例 symbologyOverrides 六色 +
    //     context 绿，首绿 9532）。
    long pastel = 0, saturated = 0;
    uint32_t minXS = w, maxXS = 0, minYS = h, maxYS = 0;
    for (uint32_t y = 0; y < h; y += 2) {
        for (uint32_t x = 0; x < w; x += 2) {
            uint8_t const* p = &frame[(static_cast<size_t>(y) * w + x) * 4];
            int const r = p[0], g = p[1], b = p[2];
            int const mx = std::max(r, std::max(g, b));
            int const mn = std::min(r, std::min(g, b));
            if (mx >= 150 && mx - mn <= 40)
                ++pastel;
            if (mx >= 180 && mx - mn >= 100) {
                ++saturated;
                if (x < minXS) minXS = x;
                if (x > maxXS) maxXS = x;
                if (y < minYS) minYS = y;
                if (y > maxYS) maxYS = y;
            }
        }
    }
    printf("[OPEN-CHAIN] instances60 solidity: pastel=%ld saturated=%ld "
           "satBbox=(%u,%u)-(%u,%u) frame=%ux%u\n",
           pastel, saturated, minXS, minYS, maxXS, maxYS, w, h);

    // ① 实心性（主判据）：洗白淡彩 ≤ 500——首绿 55 的 9× 余量 / 修复前
    //    实测 2035 的 0.25×（RED→GREEN 双向钉死的分离带中点）。
    EXPECT_LE(pastel, 500l)
        << "spheres rendered translucent/washed — OIT composite replaced "
           "opaque pixels (M-J(1) regression signature)";
    // ② 内容存活 + WHERE 展布：饱和球色内容 ≥ 4000（首绿 9532 的 0.42×
    //    ——0 实例消费恒 0）且 bbox 显著展布（首绿 1772x516——单球 ~100px
    //    不可能满足）。
    EXPECT_GE(saturated, 4000l)
        << "no saturated instance-sphere content rendered";
    if (saturated > 0) {
        EXPECT_GE(maxXS - minXS, 1000u) << "sphere content not spread in x";
        EXPECT_GE(maxYS - minYS, 300u) << "sphere content not spread in y";
    }

    view.getUeViewport()->DropTiledGraphicsProvider(opened->provider.get());
    view.close();
    spin(200);
}

// TiledGraphicsTest — M-O(2) I11 第二 iModel 瓦叠加锁（TiledGraphics.ts 的
// proof-of-concept 对应判据）。
//
// 锚定（真实读过的参考行号）：
//   - TiledGraphics.ts:95-121 toggleExternalTiledGraphicsProvider（per-viewport
//     注册表 + add/drop + close）；
//   - :62-66 Provider.forEachTileTreeRef（Viewport.addToScene :2048 的被调面
//     ——Viewport.ts:1729-1732 addTiledGraphicsProvider 应用通道）；
//   - :81-93 computeTransformFromSecondaryIModel（ecef 缺席恒等回退——
//     rpc-dumps 数据面实态）；
//   - EQUIVALENCE（TiledGraphics.h 文件头）：单 fetcher 追加根 vs 两连接后端。
//
// 判据（§11.11）：并域取景（主+副世界域并集 LookAtVolume——两包内容同入
// 视域）下：基线（仅主）→ 叠加后**新增内容像素差集 > 阈值**（副包上屏）+
// WHERE（差集簇与主内容簇显著不重叠——副内容落在主内容之外的世界域投影）→
// toggle off 回落（帧回到基线）+ 多根命中（副包键经追加根 Completed +
// hitRoot == 追加根下标 +1）。
//
// Authored: no reference test exists in itwinjs-core for TiledGraphics.ts
//           （display-test-app 无前端测试；行为锚定如上）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"
#include "Gui/TiledGraphics.h"

#include "DumpOpenHelper.h"

#include <dqApp/Application.h>
#include <dqApp/Viewport.h>
#include <dqApp/tile/DumpTileFetcher.h>
#include <dqApp/tile/DumpTileTreeProps.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {

struct QtEnvTG {
    QtEnvTG()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvTG s_qtTG;

std::string const kDumpRootTG = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";

void spinTG(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

struct RgbTG {
    int r, g, b;
};
RgbTG pixelAtTG(std::vector<uint8_t> const& f, uint32_t w, uint32_t x, uint32_t y)
{
    uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
    return {p[0], p[1], p[2]};
}

// 内容像素统计（DumpPickSceneTest contentStatsPDS 同源——阈值 90 先验）。
bool contentStatsTG(std::vector<uint8_t> const& f, uint32_t w, uint32_t h,
                    long& count, uint32_t& minX, uint32_t& maxX,
                    uint32_t& minY, uint32_t& maxY)
{
    uint8_t const* bg = &f[0];
    count = 0;
    minX = w; maxX = 0; minY = h; maxY = 0;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
            if (std::abs(p[0] - bg[0]) + std::abs(p[1] - bg[1])
                    + std::abs(p[2] - bg[2]) > 90) {
                ++count;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    return count > 0;
}

// 两帧差集（delta ≥ 阈值的像素）簇统计。
std::tuple<long, uint32_t, uint32_t, uint32_t, uint32_t> diffCluster(
    std::vector<uint8_t> const& before, std::vector<uint8_t> const& after,
    uint32_t w, uint32_t h)
{
    long count = 0;
    uint32_t minX = w, maxX = 0, minY = h, maxY = 0;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            RgbTG const a = pixelAtTG(before, w, x, y);
            RgbTG const b = pixelAtTG(after, w, x, y);
            if (std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b)
                > 40) {
                ++count;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    return {count, minX, minY, maxX, maxY};
}

// 新增内容簇（after 为内容 ∧ before 为背景的像素）——副包上屏的净新增面
//（§11.11 WHERE 的干净形态：主簇自身的 LOD/AA 帧差不计入）。
std::tuple<long, uint32_t, uint32_t, uint32_t, uint32_t> newContentCluster(
    std::vector<uint8_t> const& before, std::vector<uint8_t> const& after,
    uint32_t w, uint32_t h)
{
    uint8_t const* bg = &before[0];
    uint8_t const* bgA = &after[0];
    long count = 0;
    uint32_t minX = w, maxX = 0, minY = h, maxY = 0;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t const* pb = &before[(static_cast<size_t>(y) * w + x) * 4];
            uint8_t const* pa = &after[(static_cast<size_t>(y) * w + x) * 4];
            bool const contentBefore =
                std::abs(pb[0] - bg[0]) + std::abs(pb[1] - bg[1])
                    + std::abs(pb[2] - bg[2]) > 90;
            bool const contentAfter =
                std::abs(pa[0] - bgA[0]) + std::abs(pa[1] - bgA[1])
                    + std::abs(pa[2] - bgA[2]) > 90;
            if (contentAfter && !contentBefore) {
                ++count;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    return {count, minX, minY, maxX, maxY};
}

// 夹具：instances60 主包打开 + 泵至静默 + 帧稳定（DumpPick 同源减配——
// 不需要拾取点，只取基线帧）。
struct TiledGraphicsFixture {
    Gui::View3DInventor view{nullptr, nullptr, nullptr};
    dqApp::Viewport* vp = nullptr;
    std::optional<dta::DumpOpenResult> opened;

    bool setup()
    {
        auto& app = dqApp::Application::Get();
        if (!app.isInitialized()) {
            dqApp::Application::Options opts;
            opts.applicationId = "TiledGraphicsTest";
            opts.applicationVersion = "1.0";
            if (!app.Startup(opts))
                return false;
        }

        view.resize(1000, 700);
        view.show();
        spinTG(400);

        dta::DumpOpenPackage pkg;
        pkg.imodelRoot = kDumpRootTG + "/instances60-imodel-v1";
        pkg.tileRoots = {kDumpRootTG + "/instances60-v1",
                         kDumpRootTG + "/instances60-drill-v1"};
        opened = dta::openDumpIModel(view, pkg);
        if (!opened.has_value())
            return false;
        vp = view.getUeViewport();
        return true;
    }

    void pumpQuiesced()
    {
        size_t lastLogSize = 0;
        int stable = 0;
        for (int i = 0; i < 200; ++i) {
            vp->InvalidateController();
            spinTG(100);
            size_t const logSize = opened->fetcher->requestLog().size();
            if (!opened->fetcher->requestLog().empty()
                && logSize == lastLogSize
                && opened->fetcher->getActiveCount() == 0) {
                if (++stable >= 6)
                    return;
            } else {
                stable = 0;
            }
            lastLogSize = logSize;
        }
    }

    bool readStableFrame(std::vector<uint8_t>& out, uint32_t& w, uint32_t& h)
    {
        std::vector<uint8_t> prev;
        for (int i = 0; i < 40; ++i) {
            spinTG(150);
            vp->RenderFrame();
            std::vector<uint8_t> cur;
            uint32_t cw = 0, ch = 0;
            if (!vp->ReadFrameForTest(cur, cw, ch))
                return false;
            w = cw;
            h = ch;
            if (!prev.empty() && prev == cur) {
                out = std::move(cur);
                return true;
            }
            prev = std::move(cur);
        }
        return false;
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// 叠加主锁：并域取景下基线 → 叠加新增内容（差集 > 阈值 + WHERE 簇分离）→
// 回落基线 + 多根命中（追加根 Completed）。
// ---------------------------------------------------------------------------
TEST(TiledGraphicsTest, SecondaryIModelOverlaysAndTogglesOff)
{
    TiledGraphicsFixture s;
    ASSERT_TRUE(s.setup());

    // 主包世界域并集（provider entries 的 worldRange——打开链已算）。
    dqGeom::Range3d unionRange = dqGeom::Range3d::CreateNull();
    // 空瓦叶树（DumpBrowse 白名单同族）的 rootTile.range 是倒置 null
    //（low=+DBL_MAX/high=−DBL_MAX）——倒置守卫下跳过（并入会污染并集）。
    auto rangeIsUsable = [](dqGeom::Range3d const& r) {
        return r.low.x <= r.high.x && r.low.y <= r.high.y && r.low.z <= r.high.z;
    };
    for (auto const& entry : s.opened->provider->entries()) {
        if (rangeIsUsable(entry.worldRange))
            unionRange.ExtendRange(entry.worldRange);
    }
    ASSERT_FALSE(unionRange.isNull());

    // 副包世界域并入（joeshouse 树 props——props 侧装载一次供装配[attachment
    // 侧再装载共享失败无碍：takeManifest 只共享给 fetcher]；域并集用
    // contentRange × location 的 8 角点式）。joeshouse 单包 10 树——经
    // DumpTileTreeProps 主 manifest 树清单不可枚举（API 面 byTreeId）——用
    // imodel.json defaultViewState 的 models 派生 treeId 逐树取域（与
    // attachment 装配同源）。
    {
        auto conn = dqApp::DumpIModelConnection::open(
            kDumpRootTG + "/joeshouse-v1/imodel.json");
        ASSERT_TRUE(conn.IsValid());
        auto props = dqApp::DumpTileTreeProps::load(kDumpRootTG + "/joeshouse-v1");
        ASSERT_TRUE(props.has_value());
        ASSERT_TRUE(conn->getDefaultViewState().has_value());
        ASSERT_TRUE(conn->getDefaultViewState()->modelSelectorProps.has_value());
        for (auto modelId :
             conn->getDefaultViewState()->modelSelectorProps->models) {
            dqRender::PrimaryTileTreeId treeIdObj;
            treeIdObj.edges = dqRender::TileOptions{}.edgeOptions;
            std::string const treeId = dqRender::iModelTileTreeIdToString(
                modelId.ToString(), treeIdObj, dqRender::TileOptions{});
            auto tree = props->byTreeId(treeId);
            ASSERT_TRUE(tree.has_value()) << treeId;
            dqGeom::Range3d const src =
                tree->metadata.contentRange.isNull()
                    ? tree->rootTile.range
                    : tree->metadata.contentRange;
            dqGeom::Transform const xf =
                tree->hasLocation ? tree->location
                                  : dqGeom::Transform::CreateIdentity();
            // 倒置 null 域（空瓦叶树——同上守卫）跳过。
            if (!(src.low.x <= src.high.x && src.low.y <= src.high.y
                  && src.low.z <= src.high.z))
                continue;
            auto const diag = src.Diagonal();
            for (int c = 0; c < 8; ++c) {
                dqGeom::Point3d const corner(
                    src.low.x + ((c & 1) ? diag.x : 0.0),
                    src.low.y + ((c & 2) ? diag.y : 0.0),
                    src.low.z + ((c & 4) ? diag.z : 0.0));
                unionRange.ExtendPoint(xf.MultiplyPoint3d(corner));
            }
        }
    }

    // 并域取景（坑 24 取景路径同式——两包内容同入视域；LookAtVolume 在
    // ViewState3d 上[打开链同式]）。
    double const aspect = s.vp->viewRect().aspect();
    auto* view3d = s.vp->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    view3d->LookAtVolume(unionRange, &aspect);
    s.vp->InvalidateController();
    s.vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});

    // 基线（仅主包）。
    s.pumpQuiesced();
    std::vector<uint8_t> before;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(s.readStableFrame(before, w, h));
    printf("[TG] union=(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f)\n",
           unionRange.low.x, unionRange.low.y, unionRange.low.z,
           unionRange.high.x, unionRange.high.y, unionRange.high.z);
    fflush(stdout);
    long beforeCount = 0;
    uint32_t bMinX = 0, bMaxX = 0, bMinY = 0, bMaxY = 0;
    ASSERT_TRUE(contentStatsTG(before, w, h, beforeCount, bMinX, bMaxX, bMinY,
                               bMaxY));
    // 并域取景下主包足迹 ~3.5k px（60 实例球簇在 177m 并域里的投影——
    // 首绿实测 3542）；阈值 2000 = 夹具前提断（主包内容在屏）。
    ASSERT_GT(beforeCount, 2000) << "并域取景后主包内容缺失（夹具前提断）";
    size_t const fetcherRootsBefore =
        s.opened->fetcher->fallbackRoots().size();

    // 叠加（:112-116 建并 add）。
    dta::DumpOpenPackage pkg2;
    pkg2.imodelRoot = kDumpRootTG + "/joeshouse-v1";
    pkg2.tileRoots = {kDumpRootTG + "/joeshouse-v1"};
    ASSERT_TRUE(Gui::toggleExternalTiledGraphicsProvider(s.vp, &pkg2));
    auto* attachment = Gui::findTiledGraphicsAttachment(s.vp);
    ASSERT_NE(attachment, nullptr);
    EXPECT_FALSE(attachment->treeLoadLog.empty());

    s.pumpQuiesced();
    std::vector<uint8_t> after;
    uint32_t w2 = 0, h2 = 0;
    ASSERT_TRUE(s.readStableFrame(after, w2, h2));
    ASSERT_EQ(w2, w);
    ASSERT_EQ(h2, h);

    // 差集（副包上屏的可观测面）。
    auto [diffCount, dMinX, dMinY, dMaxX, dMaxY] =
        diffCluster(before, after, w, h);
    (void)dMinX; (void)dMinY; (void)dMaxX; (void)dMaxY;
    long afterCount = 0;
    uint32_t aMinX = 0, aMaxX = 0, aMinY = 0, aMaxY = 0;
    ASSERT_TRUE(
        contentStatsTG(after, w, h, afterCount, aMinX, aMaxX, aMinY, aMaxY));
    auto [newCount, nMinX, nMinY, nMaxX, nMaxY] =
        newContentCluster(before, after, w, h);
    printf("[TG] before=%ld after=%ld diff=%ld newContent=%ld "
           "newBBox=(%u,%u)-(%u,%u) beforeBBox=(%u,%u)-(%u,%u)\n",
           beforeCount, afterCount, diffCount, newCount, nMinX, nMinY, nMaxX,
           nMaxY, bMinX, bMinY, bMaxX, bMaxY);
    EXPECT_GT(diffCount, 2000) << "叠加后无新增内容（副包未上屏）";

    // WHERE（§11.11）：副内容世界域[~120,123]与主包[~-7..18]不重叠——其
    // 屏幕投影主体落在主内容 bbox 之外（saved 视角下两簇屏幕域部分交叠，
    // 首绿实测新增簇 bbox 69% 面积在主 bbox 外[左伸 1586<1740 / 下伸
    // 608>546]；判据 = 新增簇 bbox 在主 bbox 外的面积 > 50%）。
    {
        long const interW = std::min(nMaxX, bMaxX) - std::max(nMinX, bMinX);
        long const interH = std::min(nMaxY, bMaxY) - std::max(nMinY, bMinY);
        long const interArea =
            interW > 0 && interH > 0 ? interW * interH : 0;
        long const newArea =
            static_cast<long>(nMaxX - nMinX + 1) * (nMaxY - nMinY + 1);
        EXPECT_GT(newArea - interArea, newArea / 2)
            << "新增内容簇主体落在主内容 bbox 之内（未体现副包独立投影域）";
    }

    // 多根命中（EQUIVALENCE 验证面）：追加根下标 = fetcherRootsBefore（0 起）
    // + 1 = hitRoot；副包键经该根 Completed ≥1。
    {
        size_t const appendedRoot = fetcherRootsBefore + 1;  // hitRoot = i+1
        EXPECT_EQ(fetcherRootsBefore + 1,
                  s.opened->fetcher->fallbackRoots().size());
        size_t completedOnAppended = 0;
        for (auto const& rec : s.opened->fetcher->requestLog())
            if (rec.outcome == dqApp::DumpTileFetcher::DumpFetchOutcome::Completed
                && rec.hitRoot == appendedRoot)
                ++completedOnAppended;
        EXPECT_GT(completedOnAppended, 0u) << "副包键未在追加根命中";
        printf("[TG] appendedRoot=%zu completedOnAppended=%zu logSize=%zu\n",
               appendedRoot, completedOnAppended,
               s.opened->fetcher->requestLog().size());
    }

    // 回落（:99-105 drop + 关连接）→ 帧回基线。
    ASSERT_FALSE(Gui::toggleExternalTiledGraphicsProvider(s.vp, &pkg2));
    EXPECT_EQ(nullptr, Gui::findTiledGraphicsAttachment(s.vp));
    s.pumpQuiesced();
    std::vector<uint8_t> afterOff;
    uint32_t w3 = 0, h3 = 0;
    ASSERT_TRUE(s.readStableFrame(afterOff, w3, h3));
    auto [offDiff, oMinX, oMinY, oMaxX, oMaxY] =
        diffCluster(before, afterOff, w, h);
    (void)oMinX; (void)oMinY; (void)oMaxX; (void)oMaxY;
    EXPECT_LE(offDiff, 200) << "toggle off 后帧未回落基线";
}

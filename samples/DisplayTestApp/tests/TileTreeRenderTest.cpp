// TileTreeRenderTest — 离线 3D Tiles tileset 加载渲染像素回归（路径 A 的 RED 锁）。
//
// 2026-09-19 tiles 全链分析（docs/itwinjs-tiles-全链分析与DanQing加载验证方案-2026-09-19.md）
// 定位五断点：①树进不了视口（AddTileTree 零调用者）②missing tiles 进不了 TileAdmin
// ③TileRequestChannel dispatch 后丢弃（从不调 tile.requestContent）④readContent 走
// RenderSystem::get() 空桩 ⑤无 onTileLoad 失效级联。本测试是修复前的 RED 锁：
// 照 GltfStandardViewTest 模式驱动真窗口，加载最小 tileset（红/绿/蓝三 box，
// third_party/tile-sample-assets/minimal/，不对称色块钉 X 轴朝向——参考 tile 夹具
// 自身惯用法 TileIO.data.ts "triangles"），断言内容上屏且红在画面左半、蓝在右半。
// 五断点修复完成后转 GREEN。
//
// Authored: no reference test exists in itwinjs-core for offline 3D Tiles consumption
//           (frontend unit tests use inline Uint8Array fixtures; real tilesets come
//           only from the mesh-export cloud service). Authorized by CLAUDE.md §5(g)
//           (pixel-level rendering regression) + §11.11 (position assertion with
//           asymmetrical markers; dedicated new asset, never mutated in place).
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
#include <dqRender/tile/RealityTileTree.h>
#include <dqApp/tile/SimpleTileTreeReference.h>
#include <dqApp/tile/TiledGraphicsProvider.h>
#include <dqRender/tile/TileAdmin.h>

#include <algorithm>
#include <functional>
#include <cstdio>
#include <fstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
// SEH 崩溃符号化探针（PickHiliteSelectionTest 同款；DANQING_CRASH_STACK=1 门控）。
static LONG WINAPI tileCrashPrinter(EXCEPTION_POINTERS* ep)
{
    static HANDLE s_process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
    SymInitialize(s_process, nullptr, TRUE);
    printf("[CRASH] code=0x%08x addr=%p\n",
           ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
    void* frames[32] = {};
    WORD const n = CaptureStackBackTrace(0, 32, frames, nullptr);
    for (WORD i = 0; i < n; ++i) {
        char buf[sizeof(SYMBOL_INFO) + 256] = {};
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(buf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        DWORD64 disp = 0;
        if (SymFromAddr(s_process, reinterpret_cast<DWORD64>(frames[i]), &disp, sym)) {
            printf("[CRASH] #%u %s+0x%llx\n", i, sym->Name,
                   static_cast<unsigned long long>(disp));
        } else {
            printf("[CRASH] #%u %p\n", i, frames[i]);
        }
    }
    fflush(stdout);
    return EXCEPTION_CONTINUE_SEARCH;
}
static bool const s_tileCrashHook = [] {
    if (std::getenv("DANQING_CRASH_STACK"))
        SetUnhandledExceptionFilter(tileCrashPrinter);
    return true;
}();
#endif

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace { struct QtEnvR4 { QtEnvR4() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvR4 s_qtR4;

namespace {
void spin3(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 画面中"内容像素"（与背景差异显著）的包围盒；返回是否找到。
bool contentBBox(std::vector<uint8_t> const& f, uint32_t w, uint32_t h,
                 uint32_t& minX, uint32_t& maxX, uint32_t& minY, uint32_t& maxY)
{
    uint8_t const* bg = &f[0];
    minX = w; maxX = 0; minY = h; maxY = 0;
    bool any = false;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
            int const dr = std::abs(p[0] - bg[0]), dg = std::abs(p[1] - bg[1]), db = std::abs(p[2] - bg[2]);
            if (dr + dg + db > 60) {
                any = true;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }
    return any;
}

// 主色 blob 质心（红/绿/蓝通道占优像素）。返回像素数。
long colorCentroid(std::vector<uint8_t> const& f, uint32_t w,
                   uint32_t minX, uint32_t maxX, uint32_t minY, uint32_t maxY,
                   int which, double& cxOut)
{
    double sx = 0; long n = 0;
    for (uint32_t y = minY; y <= maxY; ++y)
        for (uint32_t x = minX; x <= maxX; ++x) {
            uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
            int const r = p[0], g = p[1], b = p[2];
            bool hit = false;
            if (which == 0)      hit = r > 120 && r > b + 60 && r > g + 60;  // red
            else if (which == 1) hit = g > 120 && g > r + 60 && g > b + 60;  // green
            else                 hit = b > 120 && b > r + 60 && b > g + 60;  // blue
            if (hit) { sx += x; ++n; }
        }
    if (n > 0) cxOut = sx / n;
    return n;
}
}  // namespace

TEST(TileTreeRender, MinimalTilesetRendersColoredBoxes)
{
    std::string const tilesetPath = DANQING_TILE_ASSETS_DIR "/minimal/tileset.json";

    // Startup injects the application-layer tile fetcher (FileTileFetcher →
    // QtTileRequestFetcher) into TileAdmin — without it the NullTileFetcher
    // fails every request synchronously (CursorStateTest 同款幂等模式).
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "TileTreeRender";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    // 树先声明（后析构）：AddTileTree 存裸指针、树归调用者所有——树必须活得
    // 比 viewport 久（Shutdown 的 freeContents 会遍历树；LOD 版曾因栈序反置
    // 在断言全过后 SEH 0xc0000005）。
    std::unique_ptr<dqRender::RealityTileTree> tree;

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin3(400);

    // 1. 读 tileset.json 并建树（前置：资产解析成功——失败=setup 问题，非回归断言）。
    {
        std::ifstream in(tilesetPath, std::ios::binary);
        ASSERT_TRUE(in.good()) << "cannot open " << tilesetPath;
        std::vector<uint8_t> jsonBytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        tree = dqRender::RealityTileTree::loadTileset(tilesetPath, jsonBytes.data(), jsonBytes.size());
    }
    ASSERT_NE(tree, nullptr) << "loadTileset failed to parse " << tilesetPath;

    // 2. 树进视口（断点①修复后此调用生效；现状 m_tileTrees 恒空）。
    view.getUeViewport()->AddTileTree(tree.get());

    // 3. 取景到内容（GltfImport.cpp:43-49 同款：LookAtVolume + InvalidateController）。
    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-3, -1.2, -1.2, 3, 1.2, 1.2));
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
    spin3(1600);  // 泵帧：GREEN 世界里 fetch→process→失效级联→重绘在此窗口完成
    view.getUeViewport()->RenderFrame();

    // 4. 像素断言：内容上屏（RED 在此失败：五断点链路无内容）。
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    uint32_t minX, maxX, minY, maxY;
    ASSERT_TRUE(contentBBox(frame, w, h, minX, maxX, minY, maxY))
        << "no tile content rendered — tile chain broken (see docs/itwinjs-tiles-*.md §3.2 breakpoints)";

    // 5. 位置断言（§11.11）：红 box（世界 x=-1.5）在画面左半、蓝 box（x=+1.5）在
    //    右半——交换 = 坐标系/变换缺陷暴露。
    // 存 BMP 供人工目检（GltfStandardViewTest 同款；行序 = glReadPixels 语义）。
    {
        FILE* f = fopen("build/tiletree-render.bmp", "wb");
        if (f) {
            uint32_t const rowBytes = w * 4;
            uint32_t const dataSize = rowBytes * h;
            unsigned char head[54] = {};
            head[0] = 'B'; head[1] = 'M';
            *reinterpret_cast<uint32_t*>(&head[2]) = 54 + dataSize;
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
            printf("[TILE] frame saved to build/tiletree-render.bmp\n");
        }
    }

    double rCx = 0, bCx = 0, gCx = 0;
    // 诊断：三个 box 预期位置的中心采样（物理像素，DPR 已含在 w/h）。
    {
        auto sampleAvg = [&](uint32_t cx, uint32_t cy) {
            double sr = 0, sg = 0, sb = 0; long n = 0;
            for (uint32_t y = cy - 5; y <= cy + 5; ++y)
                for (uint32_t x = cx - 5; x <= cx + 5; ++x) {
                    if (x >= w || y >= h) continue;
                    uint8_t const* p = &frame[(static_cast<size_t>(y) * w + x) * 4];
                    sr += p[0]; sg += p[1]; sb += p[2]; ++n;
                }
            printf("[TILE] sample(%u,%u) avg RGB=(%.0f,%.0f,%.0f)\n", cx, cy, sr / n, sg / n, sb / n);
        };
        uint32_t const cy = (minY + maxY) / 2;
        sampleAvg(static_cast<uint32_t>((rCx ? rCx : w * 0.25)), cy);  // 占位：先采预期位
        sampleAvg(w / 2, cy);
        sampleAvg(static_cast<uint32_t>(w * 0.75), cy);
    }
    long const rn = colorCentroid(frame, w, minX, maxX, minY, maxY, 0, rCx);
    long const gn = colorCentroid(frame, w, minX, maxX, minY, maxY, 1, gCx);
    long const bn = colorCentroid(frame, w, minX, maxX, minY, maxY, 2, bCx);
    printf("[TILE] content bbox=(%u,%u)-(%u,%u) red=%ld@%.0f green=%ld@%.0f blue=%ld@%.0f frameCx=%.0f\n",
           minX, minY, maxX, maxY, rn, rCx, gn, gCx, bn, bCx, w / 2.0);
    ASSERT_GT(rn, 30) << "red box not rendered";
    ASSERT_GT(gn, 30) << "green box not rendered";
    ASSERT_GT(bn, 30) << "blue box not rendered";
    EXPECT_LT(rCx, w / 2.0) << "red box should be in left half (world x=-1.5)";
    EXPECT_GT(bCx, w / 2.0) << "blue box should be in right half (world x=+1.5)";
    EXPECT_LT(std::abs(gCx - w / 2.0), w * 0.2) << "green box should be near center (world x=0)";

    view.close();
    spin3(200);
}

// 两层 LOD 树（2026-09-21 P7）：root 无 content（合法 3D Tiles LOD 中间层）+
// 2 children（红 @x=-1.5 / 蓝 @x=+1.5）。锁"无 content 中间层下钻"机制——
// 参考 RealityTile.selectRealityTiles（RealityTile.ts:340-390）：无 content 的
// tile 不参与显示但继续 descend children；DanQing 现状 selectTile 无 content 分支
// 直接 return 且基类递归以 isDisplayable 门控 → 整棵树不显示（RED 锁）。
//
// Authored: no reference test exists in itwinjs-core for offline 3D Tiles
//           consumption (§5(g) + §11.11；资产 third_party/tile-sample-assets/minimal-lod/)。
TEST(TileTreeRender, LodTilesetRendersChildren)
{
    std::string const tilesetPath = DANQING_TILE_ASSETS_DIR "/minimal-lod/tileset.json";

    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "TileTreeRender";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    // 树先声明（后析构）：AddTileTree 存裸指针、树归调用者所有——树必须活得
    // 比 viewport 久（LOD 版曾因栈序反置在断言全过后 SEH 0xc0000005：
    // tree 先析构，view 收尾渲染触碰悬空指针）。
    std::unique_ptr<dqRender::RealityTileTree> tree;

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin3(400);

    {
        std::ifstream in(tilesetPath, std::ios::binary);
        ASSERT_TRUE(in.good()) << "cannot open " << tilesetPath;
        std::vector<uint8_t> jsonBytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        tree = dqRender::RealityTileTree::loadTileset(tilesetPath, jsonBytes.data(), jsonBytes.size());
    }
    ASSERT_NE(tree, nullptr) << "loadTileset failed to parse " << tilesetPath;
    ASSERT_NE(tree->getRootTile(), nullptr);
    // 前置：树结构如资产所声明（root 无 content、2 children）——失败=setup 问题。
    auto const* root = tree->getRootTile();
    ASSERT_EQ(root->getChildren().size(), 2u) << "children not created from tileset.json";

    view.getUeViewport()->AddTileTree(tree.get());

    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-3, -1.2, -1.2, 3, 1.2, 1.2));
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
    spin3(1600);
    view.getUeViewport()->RenderFrame();

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    uint32_t minX, maxX, minY, maxY;
    ASSERT_TRUE(contentBBox(frame, w, h, minX, maxX, minY, maxY))
        << "no tile content rendered — contentless intermediate tile does not descend";

    double rCx = 0, bCx = 0;
    long const rn = colorCentroid(frame, w, minX, maxX, minY, maxY, 0, rCx);
    long const bn = colorCentroid(frame, w, minX, maxX, minY, maxY, 2, bCx);
    printf("[TILE-LOD] bbox=(%u,%u)-(%u,%u) red=%ld@%.0f blue=%ld@%.0f frameCx=%.0f\n",
           minX, minY, maxX, maxY, rn, rCx, bn, bCx, w / 2.0);
    ASSERT_GT(rn, 30) << "red child not rendered";
    ASSERT_GT(bn, 30) << "blue child not rendered";
    EXPECT_LT(rCx, w / 2.0) << "red child should be in left half (world x=-1.5)";
    EXPECT_GT(bCx, w / 2.0) << "blue child should be in right half (world x=+1.5)";

    view.close();
    spin3(200);
}

// 视锥裁剪（2026-09-21 P7c）：复用 minimal-lod 资产，取景只覆盖红 child 一侧
// （视锥 x∈[-3.2,0.2]，红 box 在 [-2,-1]、蓝 box 在 [1,2]）。判据 = 请求计数差：
// 裁剪正确时只有 child-red 被 dispatch（差=1）；现状无裁剪 → 蓝 child 也被请求
// （差=2，RED）。参考：RealityTile.computeVisibilityFactor（RealityTile.ts:516-545）
// ——isFrustumCulled（range 八角 + FrustumPlanes.computeContainment，core/common
// FrustumPlanes.ts）→ -1 剪掉，不请求不显示。
//
// Authored: no reference test exists in itwinjs-core for offline 3D Tiles
//           consumption (§5(g)；dispatch 计数判据是请求级直接证据，不依赖像素)。
TEST(TileTreeRender, FrustumCullsOffscreenChildren)
{
    std::string const tilesetPath = DANQING_TILE_ASSETS_DIR "/minimal-lod/tileset.json";

    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "TileTreeRender";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    std::unique_ptr<dqRender::RealityTileTree> tree;

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin3(400);

    {
        std::ifstream in(tilesetPath, std::ios::binary);
        ASSERT_TRUE(in.good()) << "cannot open " << tilesetPath;
        std::vector<uint8_t> jsonBytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        tree = dqRender::RealityTileTree::loadTileset(tilesetPath, jsonBytes.data(), jsonBytes.size());
    }
    ASSERT_NE(tree, nullptr);
    view.getUeViewport()->AddTileTree(tree.get());

    // 取景只含红半边：视锥 x∈[-3.2, 0.2]。
    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-3.2, -1.4, -1.4, 0.2, 1.4, 1.4));
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

    // 泵帧到红 child 就绪（内容上屏），统计全程 dispatch 数。
    uint32_t const dispatchedBefore =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests;
    spin3(1600);
    view.getUeViewport()->RenderFrame();
    uint32_t const dispatchedAfter =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests;
    uint32_t const dispatched = dispatchedAfter - dispatchedBefore;
    printf("[TILE-CULL] dispatched=%u (before=%u after=%u)\n",
           dispatched, dispatchedBefore, dispatchedAfter);

    // 像素面：红 child 上屏（视锥内）。
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    uint32_t minX, maxX, minY, maxY;
    ASSERT_TRUE(contentBBox(frame, w, h, minX, maxX, minY, maxY));
    double rCx = 0;
    long const rn = colorCentroid(frame, w, minX, maxX, minY, maxY, 0, rCx);
    ASSERT_GT(rn, 30) << "red child (inside frustum) not rendered";

    // 裁剪判据：视锥外的蓝 child 必须从未被请求（root 无 content 不计 dispatch；
    // 无裁剪现状=2：红+蓝都被请求）。
    EXPECT_EQ(dispatched, 1u)
        << "frustum culling missing: out-of-frustum blue child was dispatched";

    view.close();
    spin3(200);
}

// REPLACE 细化（2026-09-21 P7d）：三层资产 minimal-replace/——root 有 content
// （绿色大板 GE=0.1 粗层）+ 红/蓝 children（GE=0.01 细层）。两阶段断言：
//   远景（视高 10）：root SSE≈7≤16 → 只显示绿板（红蓝不请求，dispatch=1）；
//   近景（视高 2.2）：root SSE≈32>16 → refine → 显示红蓝 children，父隐藏
//   （REPLACE——参考 RealityTile.selectRealityTiles/BatchedTile.selectTiles
//   :87-110 的 closestDisplayableAncestor 顶替 + 子就绪后父被替代）。
// RED 预期双缺口：①visible 分支未加载 tile 的 requestedTiles 无人请求
// （CreateScene 只报 readyTiles）→ 远景绿板永不加载；②refine 分支 children
// 未就绪时的显示空窗。
//
// Authored: no reference test exists in itwinjs-core for offline 3D Tiles
//           consumption (§5(g)；两阶段像素 + dispatch 计数判据)。
TEST(TileTreeRender, ReplaceRefinementSwapsParentForChildren)
{
    std::string const tilesetPath = DANQING_TILE_ASSETS_DIR "/minimal-replace/tileset.json";

    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "TileTreeRender";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    std::unique_ptr<dqRender::RealityTileTree> tree;

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin3(400);

    {
        std::ifstream in(tilesetPath, std::ios::binary);
        ASSERT_TRUE(in.good()) << "cannot open " << tilesetPath;
        std::vector<uint8_t> jsonBytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        tree = dqRender::RealityTileTree::loadTileset(tilesetPath, jsonBytes.data(), jsonBytes.size());
    }
    ASSERT_NE(tree, nullptr);
    view.getUeViewport()->AddTileTree(tree.get());

    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }

    // ---- 阶段 1：远景（视高 10）——root 够细，只显示绿板 ----
    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-7.5, -5, -5, 7.5, 5, 5));
        view.getUeViewport()->InvalidateController();
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    uint32_t const dispatchedBefore =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests;
    spin3(1600);
    view.getUeViewport()->RenderFrame();

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    uint32_t minX, maxX, minY, maxY;
    ASSERT_TRUE(contentBBox(frame, w, h, minX, maxX, minY, maxY)) << "far view: no content";
    double gCx = 0, rCx = 0, bCx = 0;
    long const gn = colorCentroid(frame, w, minX, maxX, minY, maxY, 1, gCx);
    long const rn = colorCentroid(frame, w, minX, maxX, minY, maxY, 0, rCx);
    long const bn = colorCentroid(frame, w, minX, maxX, minY, maxY, 2, bCx);
    uint32_t const farDispatched =
        dqRender::TileAdmin::instance().statistics().totalDispatchedRequests - dispatchedBefore;
    printf("[TILE-REPL] far green=%ld@%.0f red=%ld blue=%ld dispatched=%u\n",
           gn, gCx, rn, bn, farDispatched);
    ASSERT_GT(gn, 30) << "far view: parent (green) not rendered";
    EXPECT_EQ(rn, 0) << "far view: children should not render (root meets SSE)";
    EXPECT_EQ(bn, 0) << "far view: children should not render (root meets SSE)";
    EXPECT_EQ(farDispatched, 1u) << "far view: only the parent should be dispatched";

    // ---- 阶段 2：近景（视高 2.2）——refine：红蓝显示，绿板隐藏（REPLACE）----
    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-2.6, -1.1, -1.1, 2.6, 1.1, 1.1));
        view.getUeViewport()->InvalidateController();
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spin3(1600);
    view.getUeViewport()->RenderFrame();

    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    ASSERT_TRUE(contentBBox(frame, w, h, minX, maxX, minY, maxY)) << "near view: no content";
    double gCx2 = 0;
    long const gn2 = colorCentroid(frame, w, minX, maxX, minY, maxY, 1, gCx2);
    long const rn2 = colorCentroid(frame, w, minX, maxX, minY, maxY, 0, rCx);
    long const bn2 = colorCentroid(frame, w, minX, maxX, minY, maxY, 2, bCx);
    printf("[TILE-REPL] near green=%ld red=%ld@%.0f blue=%ld@%.0f\n",
           gn2, rn2, rCx, bn2, bCx);
    ASSERT_GT(rn2, 30) << "near view: red child not rendered";
    ASSERT_GT(bn2, 30) << "near view: blue child not rendered";
    EXPECT_EQ(gn2, 0) << "REPLACE violated: parent (green) still rendered after refinement";

    view.close();
    spin3(200);
}

// Provider 通道（忠实路径 F6）：TiledGraphicsProvider 注入（应用通道，
// Viewport.ts:1729-1732 addTiledGraphicsProvider）——与 AddTileTree 脚手架
// 同一资产/断言，验证 Viewport → providers → addToScene 链路。
//
// Authored: no reference test exists in itwinjs-core for offline 3D Tiles
//           consumption (§5(g)；frontend 注入通道的窗口级端到端锁)。
namespace {

class TreeSetProvider final : public dqApp::TiledGraphicsProvider {
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

}  // namespace

TEST(TileTreeRender, ProviderChannelRendersTileset)
{
    std::string const tilesetPath = DANQING_TILE_ASSETS_DIR "/minimal/tileset.json";

    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "TileTreeRender";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    std::unique_ptr<dqRender::RealityTileTree> tree;

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin3(400);

    {
        std::ifstream in(tilesetPath, std::ios::binary);
        ASSERT_TRUE(in.good()) << "cannot open " << tilesetPath;
        std::vector<uint8_t> jsonBytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        tree = dqRender::RealityTileTree::loadTileset(tilesetPath, jsonBytes.data(), jsonBytes.size());
    }
    ASSERT_NE(tree, nullptr);

    TreeSetProvider provider;
    provider.addTree(tree.get());
    view.getUeViewport()->AddTiledGraphicsProvider(&provider);
    EXPECT_TRUE(view.getUeViewport()->HasTiledGraphicsProvider(&provider));
    {
        // RenderSystem 注入：provider 通道不经 AddTileTree——手动注入
        //（树归调用者，渲染系统归视口——同一约定）。
        tree->setRenderSystem(view.getUeViewport()->renderSystem());
    }

    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-3, -1.2, -1.2, 3, 1.2, 1.2));
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
    spin3(1600);
    view.getUeViewport()->RenderFrame();

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    uint32_t minX, maxX, minY, maxY;
    ASSERT_TRUE(contentBBox(frame, w, h, minX, maxX, minY, maxY))
        << "no tile content via provider channel";

    double rCx = 0, bCx = 0;
    long const rn = colorCentroid(frame, w, minX, maxX, minY, maxY, 0, rCx);
    long const bn = colorCentroid(frame, w, minX, maxX, minY, maxY, 2, bCx);
    printf("[TILE-PROV] red=%ld@%.0f blue=%ld@%.0f frameCx=%.0f\n", rn, rCx, bn, bCx, w / 2.0);
    ASSERT_GT(rn, 30) << "red box not rendered via provider channel";
    ASSERT_GT(bn, 30) << "blue box not rendered via provider channel";
    EXPECT_LT(rCx, w / 2.0);
    EXPECT_GT(bCx, w / 2.0);

    view.getUeViewport()->DropTiledGraphicsProvider(&provider);
    view.close();
    spin3(200);
}

// 透视 SSE（2026-09-23 G-A）：相机开启下 LOD 判定按最近点像素尺寸——近处
// tile 应 refine（红蓝 children）、远处不 refine（绿板）。资产同
// minimal-replace。参考：TileDrawArgs.computePixelSizeInMetersAtClosestPoint
// （TileDrawArgs.ts:190-206）。
//
// Authored: no reference test exists in itwinjs-core for offline 3D Tiles
//           consumption (§5(g)；透视 LOD 的窗口级锁)。
TEST(TileTreeRender, PerspectiveCameraSseRefines)
{
    std::string const tilesetPath = DANQING_TILE_ASSETS_DIR "/minimal-replace/tileset.json";

    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "TileTreeRender";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    std::unique_ptr<dqRender::RealityTileTree> tree;

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin3(400);

    {
        std::ifstream in(tilesetPath, std::ios::binary);
        ASSERT_TRUE(in.good()) << "cannot open " << tilesetPath;
        std::vector<uint8_t> jsonBytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        tree = dqRender::RealityTileTree::loadTileset(tilesetPath, jsonBytes.data(), jsonBytes.size());
    }
    ASSERT_NE(tree, nullptr);
    view.getUeViewport()->AddTileTree(tree.get());
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }

    // 近相机：eye @ (0,0,4) 看向原点，lens 90°——root 最近点 ≈ 4-2.7 ≈ 1.3，
    // pixelSize ≈ 1.3*2*tan(45°)/700 ≈ 0.0037 → root SSE ≈ 27 > 16 → refine。
    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        dqApp::LookAtArgs args;
        args.eyePoint = dqGeom::Point3d::From(0, 0, 4);
        args.targetPoint = dqGeom::Point3d::From(0, 0, 0);
        args.upVector = dqGeom::Vector3d::From(0, 1, 0);
        args.lensAngleRadians = M_PI / 2.0;
        ASSERT_TRUE(view3d->lookAt(args) == dqApp::ViewStatus::Success);
        view.getUeViewport()->InvalidateController();
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spin3(1600);
    view.getUeViewport()->RenderFrame();

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    uint32_t minX, maxX, minY, maxY;
    ASSERT_TRUE(contentBBox(frame, w, h, minX, maxX, minY, maxY)) << "near camera: no content";
    double rCx = 0, bCx = 0, gCx = 0;
    long const rn = colorCentroid(frame, w, minX, maxX, minY, maxY, 0, rCx);
    long const bn = colorCentroid(frame, w, minX, maxX, minY, maxY, 2, bCx);
    long const gn = colorCentroid(frame, w, minX, maxX, minY, maxY, 1, gCx);
    printf("[TILE-CAM] near red=%ld blue=%ld green=%ld\n", rn, bn, gn);
    ASSERT_GT(rn, 30) << "near camera: red child should render (refine)";
    ASSERT_GT(bn, 30) << "near camera: blue child should render (refine)";
    EXPECT_EQ(gn, 0) << "near camera: REPLACE — parent should be hidden";

    // 远相机：eye @ (0,0,16)——root 最近点 ≈ 13.3，pixelSize ≈ 0.0157 →
    // root SSE ≈ 6.4 ≤ 16 → 绿板（children 不渲染）。
    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        dqApp::LookAtArgs args;
        args.eyePoint = dqGeom::Point3d::From(0, 0, 16);
        args.targetPoint = dqGeom::Point3d::From(0, 0, 0);
        args.upVector = dqGeom::Vector3d::From(0, 1, 0);
        args.lensAngleRadians = M_PI / 2.0;
        ASSERT_TRUE(view3d->lookAt(args) == dqApp::ViewStatus::Success);
        view.getUeViewport()->InvalidateController();
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spin3(1600);
    view.getUeViewport()->RenderFrame();

    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    ASSERT_TRUE(contentBBox(frame, w, h, minX, maxX, minY, maxY)) << "far camera: no content";
    long const gn2 = colorCentroid(frame, w, minX, maxX, minY, maxY, 1, gCx);
    long const rn2 = colorCentroid(frame, w, minX, maxX, minY, maxY, 0, rCx);
    long const bn2 = colorCentroid(frame, w, minX, maxX, minY, maxY, 2, bCx);
    printf("[TILE-CAM] far green=%ld red=%ld blue=%ld\n", gn2, rn2, bn2);
    ASSERT_GT(gn2, 30) << "far camera: parent (green) should render (no refine)";
    EXPECT_EQ(rn2, 0) << "far camera: children should not render";
    EXPECT_EQ(bn2, 0) << "far camera: children should not render";

    view.close();
    spin3(200);
}

// 无纹理纯色路径（TD-17 修复锁，2026-09-23）：纯 baseColorFactor 资产
// （minimal-solid/，无 UV/无纹理）——三个 box 应按材质色渲染（红/绿/蓝），
// 不再通道错位（绿→品红/蓝→黄/alpha 错位透明→黑）。纹理路径不受影响
// （纹理供色；既有 6 测试回归保障）。
//
// Authored: no reference test exists for DanQing's untextured solid-color
//           path (§5(g)；TD-17 字节序解包错位的像素锁)。
TEST(TileTreeRender, SolidBaseColorFactorRendersMaterialColors)
{
    std::string const tilesetPath = DANQING_TILE_ASSETS_DIR "/minimal-solid/tileset.json";

    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "TileTreeRender";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    std::unique_ptr<dqRender::RealityTileTree> tree;

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin3(400);

    {
        std::ifstream in(tilesetPath, std::ios::binary);
        ASSERT_TRUE(in.good()) << "cannot open " << tilesetPath;
        std::vector<uint8_t> jsonBytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        tree = dqRender::RealityTileTree::loadTileset(tilesetPath, jsonBytes.data(), jsonBytes.size());
    }
    ASSERT_NE(tree, nullptr);
    view.getUeViewport()->AddTileTree(tree.get());

    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-3, -1.2, -1.2, 3, 1.2, 1.2));
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
    spin3(1600);
    view.getUeViewport()->RenderFrame();

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    uint32_t minX, maxX, minY, maxY;
    ASSERT_TRUE(contentBBox(frame, w, h, minX, maxX, minY, maxY))
        << "solid-color path rendered nothing";
    double rCx = 0, gCx = 0, bCx = 0;
    long const rn = colorCentroid(frame, w, minX, maxX, minY, maxY, 0, rCx);
    long const gn = colorCentroid(frame, w, minX, maxX, minY, maxY, 1, gCx);
    long const bn = colorCentroid(frame, w, minX, maxX, minY, maxY, 2, bCx);
    printf("[TILE-SOLID] red=%ld@%.0f green=%ld@%.0f blue=%ld@%.0f\n",
           rn, rCx, gn, gCx, bn, bCx);
    ASSERT_GT(rn, 30) << "red box (baseColorFactor) not rendered in its color";
    ASSERT_GT(gn, 30) << "green box rendered wrong (channel-shifted? TD-17)";
    ASSERT_GT(bn, 30) << "blue box rendered wrong (channel-shifted? TD-17)";

    view.close();
    spin3(200);
}

// imdl 端到端（H-4）：imdl tileset（夹具录制的真后端 rectangle 字节）经
// tile 链加载→量化解码→图形上屏。断言绿色内容出现（fixture 材质 fillColor
// 65280=绿，TileIO.data.ts:22 "a green rectangle"）。
// Ported from: TileIO.test.ts processRectangle :148（result.graphic !==
// undefined + MeshGraphic 断言）的窗口级等价。
// 资产：minimal-imdl/（tileset.json + root.imdl = TileIO.data.1.1.ts
// rectangle 场景字节，Authored 组装，§5(g)）。
TEST(TileTreeRender, ImdlTilesetRendersRecordedFixture)
{
    std::string const tilesetPath = DANQING_TILE_ASSETS_DIR "/minimal-imdl/tileset.json";

    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "TileTreeRender";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    std::unique_ptr<dqRender::RealityTileTree> tree;

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin3(400);

    {
        std::ifstream in(tilesetPath, std::ios::binary);
        ASSERT_TRUE(in.good()) << "cannot open " << tilesetPath;
        std::vector<uint8_t> jsonBytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        tree = dqRender::RealityTileTree::loadTileset(tilesetPath, jsonBytes.data(), jsonBytes.size());
    }
    ASSERT_NE(tree, nullptr);
    view.getUeViewport()->AddTileTree(tree.get());

    {
        auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
        ASSERT_NE(view3d, nullptr);
        // imdl 内容中心化（±2.5/±5）——取景覆盖。
        view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-3, -5.5, -1.2, 3, 5.5, 1.2));
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
    // imdl 矩形是 z≈0 的平面薄片——Top 视图（+z 往下看）正对平面（默认
    // Front 沿 -y 看视线与平面平行，侧棱亚像素不可见）。
    {
        auto* tool = new dqApp::StandardViewTool(view.getUeViewport(), dqApp::StandardViewId::Iso);
        if (!tool->run())
            delete tool;
    }
    spin3(1500);
    view.getUeViewport()->RenderFrame();

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    {
        FILE* f = fopen("build/tiletree-imdl.bmp", "wb");
        if (f) {
            uint32_t const rowBytes = w * 4;
            uint32_t const dataSize = rowBytes * h;
            unsigned char head[54] = {};
            head[0] = 'B'; head[1] = 'M';
            *reinterpret_cast<uint32_t*>(&head[2]) = 54 + dataSize;
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
            printf("[TILE-IMDL] frame dumped to build/tiletree-imdl.bmp\n");
        }
    }
    uint32_t minX, maxX, minY, maxY;
    ASSERT_TRUE(contentBBox(frame, w, h, minX, maxX, minY, maxY))
        << "imdl content rendered nothing";
    double gCx = 0;
    long const gn = colorCentroid(frame, w, minX, maxX, minY, maxY, 1, gCx);
    printf("[TILE-IMDL] bbox=(%u,%u)-(%u,%u) green=%ld@%.0f\n",
           minX, minY, maxX, maxY, gn, gCx);
    ASSERT_GT(gn, 30) << "green rectangle (imdl fixture fillColor 65280) not rendered";

    view.close();
    spin3(200);
}


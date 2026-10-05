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
#include <dqApp/ClipViewTool.h>  // M-P P-F：ViewClipDecoration E2E 像素锁
#include <dqApp/Viewport.h>
#include <dqApp/StandardView.h>
#include <dqApp/ViewTool.h>
#include <dqApp/ViewState.h>
#include <dqCommon/DisplayStyleSettings.h>
#include <dqGeom/ClipPlane.h>
#include <dqGeom/ClipPrimitive.h>
#include <dqGeom/ClipVector.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqRender/tile/RealityTileTree.h>
#include <dqApp/tile/SimpleTileTreeReference.h>
#include <dqApp/tile/TiledGraphicsProvider.h>
#include <dqRender/tile/TileAdmin.h>
#include "tile-sample-assets/imdl-fixtures/TileIOFixtures.h"  // cylinder 录制夹具（U11(3) silhouette 像素锁派生资产源）

#include <algorithm>
#include <filesystem>
#include <functional>
#include <cstdio>
#include <fstream>
#include <system_error>
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

using namespace dqRender::fixtures;  // V1_1::cylinderBytes（TileIOFixtures.h）

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

    // Startup injects the application-layer tile fetcher (FileTileFetcher —
    // local files only, zero network §8.2) into TileAdmin — without it the
    // NullTileFetcher fails every request synchronously (CursorStateTest
    // 同款幂等模式).
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

// View clip 端到端（M-P P-D 全链锁）：setViewClip 平面剖切 → OnClipVectorChanged
// → InvalidateRenderPlan → RenderPlan.clip → TargetImpl.updateViewClip →
// ClipStack（纹理上传 + startIndex/endIndex）→ TechniqueFlags.numClipPlanes →
// Surface clip 变体（addClipping 片元 discard）。锚定：Clipping.ts addClipping
//（:136-208）+ ClipViewTool.doClipToPlane 的构造面（单 ConvexClipPlaneSet）。
//
// 断言面（§11.11 WHERE——不对称位置断言）：
//  - 剖切后恰一色塌缩（世界 x=-1 平面、内法向 +X——左盒[世界 x≈-2]被裁），
//    存留色的质心相对塌缩色仍在原侧（方向锚）；
//  - 内容 bbox 从塌缩色一侧回退；
//  - viewFlags.clipVolume=false → 不裁（旗标语义）；
//  - setViewClip(nullptr) → 塌缩色恢复（可逆）。
//
// Authored: no reference test exists for view-clip rendering（参考 ClipStack/
//           ClipVolume webgl 测试为 headless 单元；窗口级全链为 DanQing §5(g) 面）。
TEST(TileTreeRender, ViewClipPlaneDiscardsHalfspace)
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

    auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-3, -1.2, -1.2, 3, 1.2, 1.2));
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

    // --- 基线帧：三色 + bbox ---
    auto readFrame = [&](std::vector<uint8_t>& frame, uint32_t& w, uint32_t& h) {
        view.getUeViewport()->RenderFrame();
        return view.getUeViewport()->ReadFrameForTest(frame, w, h);
    };
    auto colorStats = [](std::vector<uint8_t> const& frame, uint32_t w, uint32_t /*h*/,
                         uint32_t minX, uint32_t maxX, uint32_t minY, uint32_t maxY,
                         long counts[3], double cxs[3]) {
        for (int c = 0; c < 3; ++c)
            counts[c] = colorCentroid(frame, w, minX, maxX, minY, maxY, c, cxs[c]);
    };

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(readFrame(frame, w, h));
    uint32_t bMinX, bMaxX, bMinY, bMaxY;
    ASSERT_TRUE(contentBBox(frame, w, h, bMinX, bMaxX, bMinY, bMaxY));
    long baseCounts[3]; double baseCxs[3];
    colorStats(frame, w, h, bMinX, bMaxX, bMinY, bMaxY, baseCounts, baseCxs);
    ASSERT_GT(baseCounts[0] + baseCounts[1] + baseCounts[2], 200) << "baseline rendered nothing";
    printf("[VIEWCLIP] base r=%ld@%.0f g=%ld@%.0f b=%ld@%.0f bbox=[%u,%u]\n",
           baseCounts[0], baseCxs[0], baseCounts[1], baseCxs[1], baseCounts[2], baseCxs[2],
           bMinX, bMaxX);

    // --- 平面剖切：世界 x=-1、内法向 +X（保 x>-1 半空间——裁掉左盒）---
    // 构造面 1:1 ViewClipTool.doClipToPlane（ClipViewTool.ts:159-177）：
    // ClipPlane → ConvexClipPlaneSet → ClipPrimitive::createCapture → ClipVector。
    {
        auto plane = dqGeom::ClipPlane::createNormalAndPoint(
            dqGeom::Vector3d::From(1.0, 0.0, 0.0), dqGeom::Point3d::From(-1.0, 0.0, 0.0));
        ASSERT_TRUE(plane.has_value());
        dqGeom::ConvexClipPlaneSet const set = dqGeom::ConvexClipPlaneSet::createPlanes({*plane});
        dqGeom::ClipVector::Ptr const clip = dqGeom::ClipVector::createCapture(
            {dqGeom::ClipPrimitive::createCapture(set)});
        view3d->setViewClip(clip);
    }
    // 重绘确定性：clip 失效级联（OnClipVectorChanged → InvalidateRenderPlan →
    // … → RequestRedraw）经事件循环异步推进——踢一次失效并等待至内容稳定
    //（连续两帧相同；保全型平面下稳定帧可与基线相同，断言在后续色彩计数）。
    view.getUeViewport()->InvalidateController();
    spin3(400);
    {
        std::vector<uint8_t> prev;
        for (int i = 0; i < 30; ++i) {
            prev = frame;
            spin3(100);
            view.getUeViewport()->InvalidateController();
            ASSERT_TRUE(readFrame(frame, w, h));
            if (i > 0 && frame == prev)
                break;
        }
    }

    // --- 断言：恰一色塌缩 + WHERE（塌缩色在屏幕一侧极端；bbox 同侧回退）---
    uint32_t cMinX, cMaxX, cMinY, cMaxY;
    ASSERT_TRUE(contentBBox(frame, w, h, cMinX, cMaxX, cMinY, cMaxY));
    long clipCounts[3]; double clipCxs[3];
    colorStats(frame, w, h, cMinX, cMaxX, cMinY, cMaxY, clipCounts, clipCxs);
    printf("[VIEWCLIP] clipped r=%ld g=%ld b=%ld bbox=[%u,%u]\n",
           clipCounts[0], clipCounts[1], clipCounts[2], cMinX, cMaxX);

    int collapsed = -1;
    for (int c = 0; c < 3; ++c) {
        if (clipCounts[c] < 5 && baseCounts[c] > 30) {
            ASSERT_EQ(collapsed, -1) << "more than one color collapsed";
            collapsed = c;
        }
    }
    ASSERT_NE(collapsed, -1) << "no color collapsed — clip did not discard";
    // 存留色计数基本保持
    long survivorTotal = clipCounts[0] + clipCounts[1] + clipCounts[2];
    ASSERT_GT(survivorTotal, 100) << "clip discarded everything (wrong normal/plane?)";

    // WHERE：塌缩色基线质心在内容 bbox 的外侧三分之一（极端侧）——剖切是
    // 方向性的，不是全图淡化。
    double const collapsedCx = baseCxs[collapsed];
    double const bboxMid = 0.5 * (bMinX + bMaxX);
    bool const collapsedOnLeft = (collapsedCx < bboxMid);
    double const third = (bMaxX - bMinX) / 3.0;
    if (collapsedOnLeft)
        ASSERT_LT(collapsedCx, bMinX + third) << "collapsed color not on the extreme left";
    else
        ASSERT_GT(collapsedCx, bMaxX - third) << "collapsed color not on the extreme right";
    // bbox 从塌缩侧回退
    if (collapsedOnLeft)
        ASSERT_GT(cMinX, bMinX + 5) << "content bbox did not retreat from the clipped side";
    else
        ASSERT_LT(cMaxX, bMaxX - 5) << "content bbox did not retreat from the clipped side";

    // --- 旗标语义：viewFlags.clipVolume=false → 不裁 ---
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.clipVolume = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    view.getUeViewport()->InvalidateController();  // 旗标经 render plan 生效
    spin3(400);
    ASSERT_TRUE(readFrame(frame, w, h));
    {
        uint32_t fMinX, fMaxX, fMinY, fMaxY;
        ASSERT_TRUE(contentBBox(frame, w, h, fMinX, fMaxX, fMinY, fMaxY));
        long flagCounts[3]; double flagCxs[3];
        colorStats(frame, w, h, fMinX, fMaxX, fMinY, fMaxY, flagCounts, flagCxs);
        printf("[VIEWCLIP] flag-off r=%ld g=%ld b=%ld\n", flagCounts[0], flagCounts[1], flagCounts[2]);
        ASSERT_GT(flagCounts[collapsed], 30) << "clipVolume=false should disable clipping";
    }
    // 恢复旗标
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.clipVolume = true;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    view.getUeViewport()->InvalidateController();
    spin3(400);

    // --- 可逆：setViewClip(nullptr) → 塌缩色恢复 ---
    view3d->setViewClip(nullptr);
    spin3(400);
    ASSERT_TRUE(readFrame(frame, w, h));
    {
        uint32_t r2MinX, r2MaxX, r2MinY, r2MaxY;
        ASSERT_TRUE(contentBBox(frame, w, h, r2MinX, r2MaxX, r2MinY, r2MaxY));
        long restoreCounts[3]; double restoreCxs[3];
        colorStats(frame, w, h, r2MinX, r2MaxX, r2MinY, r2MaxY, restoreCounts, restoreCxs);
        printf("[VIEWCLIP] restored r=%ld g=%ld b=%ld bbox=[%u,%u]\n",
               restoreCounts[0], restoreCounts[1], restoreCounts[2], r2MinX, r2MaxX);
        ASSERT_GT(restoreCounts[collapsed], 30) << "clearing the clip should restore the color";
        // bbox 回到基线侧
        if (collapsedOnLeft)
            ASSERT_LT(r2MinX, cMinX) << "bbox did not extend back after clear";
        else
            ASSERT_GT(r2MaxX, cMaxX) << "bbox did not extend back after clear";
    }

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
// M-P P-F：ViewClipDecoration 装饰轮廓像素锁 + Provider 默认右键 negate 的
// 像素双向锁（§11.11 WHERE 断言）。
// Authored: no reference test exists in itwinjs-core for ViewClipDecoration
//           （无 .test.ts；行为锚 = ClipViewTool.ts decorate :1854-1964 的
//           WorldDecoration 白线轮廓 + doClipPlaneNegate :1587-1605）。
// WHERE：剖切面轮廓 = 竖直窄带（面 x=-1 过视域中心，LookAtVolume 前视），
// 带宽 << 帧宽、带高跨内容 bbox 过半——方向性锚（水平带=错，斜带=错）。
// 双向：negate 后塌缩色翻转（先前塌缩色回归、对侧色塌缩）。
TEST(TileTreeRender, ViewClipDecorationOutlineAndNegatePixelLock)
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

    auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-3, -1.2, -1.2, 3, 1.2, 1.2));
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
        // 装饰判据确定性：关天空球（本 harness 默认开——ISO 视域上下半球渐变
        // 会吃掉填充带的暗青判据）+ 黑背景。
        style.getSettings().toggleSkyBox(false);
        style.getSettings().setBackgroundColor(dqCommon::ColorDef::from(0, 0, 0));
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spin3(1600);
    view.getUeViewport()->RenderFrame();

    auto readFrame = [&](std::vector<uint8_t>& frame, uint32_t& w, uint32_t& h) {
        view.getUeViewport()->RenderFrame();
        return view.getUeViewport()->ReadFrameForTest(frame, w, h);
    };
    auto colorStats = [](std::vector<uint8_t> const& frame, uint32_t w, uint32_t /*h*/,
                         uint32_t minX, uint32_t maxX, uint32_t minY, uint32_t maxY,
                         long counts[3], double cxs[3]) {
        for (int c = 0; c < 3; ++c)
            counts[c] = colorCentroid(frame, w, minX, maxX, minY, maxY, c, cxs[c]);
    };
    // 装饰手柄箭头簇（近白：箭头轮廓 (0,0,0,50) 经 adjustForBackgroundColor
    // 在黑背景翻白——:285-290 语义；边侧看呈水平细条）。轮廓白线与填充 quad
    // hugs viewRange（空连接 extents ±1000/±100——loop 边/巨三角形在本 GL 路径
    // 视锥外丢弃，参考同形几何）；箭头位于 loop 质心（面迹线 x=-1 列、帧高
    // 中带——z=0 → y≈h/2）——装饰上屏的 WHERE 锚。
    auto arrowStripStats = [](std::vector<uint8_t> const& frame, uint32_t w, uint32_t h,
                              uint32_t& minX, uint32_t& maxX, uint32_t& minY, uint32_t& maxY) {
        minX = w; maxX = 0; minY = h; maxY = 0;
        long n = 0;
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                uint8_t const* p = &frame[(static_cast<size_t>(y) * w + x) * 4];
                if (p[0] >= 200 && p[1] >= 200 && p[2] >= 200) {
                    ++n;
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
        return n;
    };
    auto pumpToStable = [&](std::vector<uint8_t>& frame, uint32_t& w, uint32_t& h) {
        std::vector<uint8_t> prev;
        for (int i = 0; i < 30; ++i) {
            prev = frame;
            spin3(100);
            view.getUeViewport()->InvalidateController();
            ASSERT_TRUE(readFrame(frame, w, h));
            if (i > 0 && frame == prev)
                break;
        }
    };

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(readFrame(frame, w, h));
    uint32_t bMinX, bMaxX, bMinY, bMaxY;
    ASSERT_TRUE(contentBBox(frame, w, h, bMinX, bMaxX, bMinY, bMaxY));
    long baseCounts[3]; double baseCxs[3];
    colorStats(frame, w, h, bMinX, bMaxX, bMinY, bMaxY, baseCounts, baseCxs);
    ASSERT_GT(baseCounts[0] + baseCounts[1] + baseCounts[2], 200) << "baseline rendered nothing";
    // 基线无近白簇（内容为饱和三色——near-white 应近零）
    uint32_t wbMinX, wbMaxX, wbMinY, wbMaxY;
    long const baseArrow = arrowStripStats(frame, w, h, wbMinX, wbMaxX, wbMinY, wbMaxY);
    printf("[CLIPDECO] base r=%ld g=%ld b=%ld arrow=%ld\n",
           baseCounts[0], baseCounts[1], baseCounts[2], baseArrow);
    ASSERT_LT(baseArrow, 50) << "baseline already has near-white pixels - arrow assert would be blind";

    // --- 平面剖切（x=-1、内法向 +X——裁掉左盒）+ 装饰激活 ---
    {
        auto plane = dqGeom::ClipPlane::createNormalAndPoint(
            dqGeom::Vector3d::From(1.0, 0.0, 0.0), dqGeom::Point3d::From(-1.0, 0.0, 0.0));
        ASSERT_TRUE(plane.has_value());
        dqGeom::ConvexClipPlaneSet const set = dqGeom::ConvexClipPlaneSet::createPlanes({*plane});
        dqGeom::ClipVector::Ptr const clip = dqGeom::ClipVector::createCapture(
            {dqGeom::ClipPrimitive::createCapture(set)});
        view3d->setViewClip(clip);
    }
    dqApp::ViewClipDecorationProvider& provider = dqApp::ViewClipDecorationProvider::create();
    provider.onNewClipPlane(*view.getUeViewport());
    ASSERT_TRUE(provider.isDecorationActive(*view.getUeViewport()));
    view.getUeViewport()->InvalidateController();
    pumpToStable(frame, w, h);

    // --- 断言 1：恰一色塌缩（P-D 同判据）---
    uint32_t cMinX, cMaxX, cMinY, cMaxY;
    ASSERT_TRUE(contentBBox(frame, w, h, cMinX, cMaxX, cMinY, cMaxY));
    long clipCounts[3]; double clipCxs[3];
    colorStats(frame, w, h, cMinX, cMaxX, cMinY, cMaxY, clipCounts, clipCxs);
    printf("[CLIPDECO] clipped r=%ld g=%ld b=%ld\n", clipCounts[0], clipCounts[1], clipCounts[2]);
    int collapsed = -1;
    for (int c = 0; c < 3; ++c) {
        if (clipCounts[c] < 5 && baseCounts[c] > 30) {
            ASSERT_EQ(collapsed, -1) << "more than one color collapsed";
            collapsed = c;
        }
    }
    ASSERT_NE(collapsed, -1) << "no color collapsed - clip did not discard";
    ASSERT_GT(clipCounts[0] + clipCounts[1] + clipCounts[2], 100);

    // --- 断言 2：手柄箭头簇上屏 + WHERE（水平细条，贴面迹线列、帧高中带）---
    uint32_t oMinX, oMaxX, oMinY, oMaxY;
    long const arrow = arrowStripStats(frame, w, h, oMinX, oMaxX, oMinY, oMaxY);
    printf("[CLIPDECO] arrow px=%ld bbox=[%u,%u]x[%u,%u]\n",
           arrow, oMinX, oMaxX, oMinY, oMaxY);
    ASSERT_GT(arrow, 100) << "clip handle arrow cluster not rendered";
    double const arrowH = static_cast<double>(oMaxY - oMinY);
    ASSERT_LE(arrowH, 12.0) << "arrow cluster should be a thin edge-on strip";
    // WHERE-1：簇右缘 = 面迹线列（x=-1 于 [-3,3] 域 ≈ 左 1/4——非内容边缘）
    ASSERT_GT(oMaxX, cMinX + (cMaxX - cMinX) / 8.0);
    ASSERT_LT(oMaxX, cMinX + (cMaxX - cMinX) / 2.0);
    // WHERE-2：loop 质心 z=0 → 簇位于帧高中带（±15%）
    double const arrowCy = 0.5 * (oMinY + oMaxY);
    ASSERT_GT(arrowCy, 0.35 * h);
    ASSERT_LT(arrowCy, 0.65 * h);

    // --- 断言 3：Provider 默认右键（无监听）→ negate → 塌缩色翻转 ---
    dqApp::ViewClipDecoration* deco = dqApp::ViewClipDecoration::get(*view.getUeViewport());
    ASSERT_NE(deco, nullptr);
    ASSERT_EQ(deco->controls().size(), 1u);  // 单面 → loop 质心单手柄
    dqApp::BeButtonEvent ev;
    ev.viewport = view.getUeViewport();
    ASSERT_TRUE(provider.onRightClick(deco->controlIds()[0], ev));

    view.getUeViewport()->InvalidateController();
    pumpToStable(frame, w, h);
    uint32_t nMinX, nMaxX, nMinY, nMaxY;
    ASSERT_TRUE(contentBBox(frame, w, h, nMinX, nMaxX, nMinY, nMaxY));
    long negCounts[3]; double negCxs[3];
    colorStats(frame, w, h, nMinX, nMaxX, nMinY, nMaxY, negCounts, negCxs);
    printf("[CLIPDECO] negated r=%ld g=%ld b=%ld\n", negCounts[0], negCounts[1], negCounts[2]);
    // 先前塌缩色回归（双向）
    ASSERT_GT(negCounts[collapsed], 30) << "negate should restore the previously discarded half";
    // 对侧出现新的塌缩（内法向翻转 -X → 保 x<-1——面在盒交界处，g+b 双塌缩）
    int numCollapsed2 = 0;
    for (int c = 0; c < 3; ++c) {
        if (negCounts[c] < 5 && baseCounts[c] > 30) {
            ASSERT_NE(c, collapsed) << "previously collapsed color collapsed again";
            ++numCollapsed2;
        }
    }
    ASSERT_GE(numCollapsed2, 1) << "negate did not discard the opposite half";
    // 箭头簇仍在（面位置不变，法向翻转）
    long const arrow2 = arrowStripStats(frame, w, h, oMinX, oMaxX, oMinY, oMaxY);
    ASSERT_GT(arrow2, 100) << "arrow cluster should persist across negate";

    // --- 清理（单例跨测试污染防）---
    dqApp::ViewClipDecorationProvider::clearProvider();
    view3d->setViewClip(nullptr);
    spin3(200);
    view.close();
    spin3(200);
}

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

// ---------------------------------------------------------------------------
// M-O(1) I1：monochrome 渲染像素锁（ViewAttributes.ts addMonochrome 的渲染面）。
// 通道链：viewFlags.monochrome 位 + settings.monochromeColor/monochromeMode →
// RenderPlan（RenderPlan.ts:114 monochromeMode 从 settings）→ StyleUniforms
// (u_monoRgb) + Common.ts:57 位 0（currentViewFlags.monochrome &&
// geometry.wantMonochrome）→ Monochrome.ts applySurfaceMonochromeColor 的
// u_mixMonoColor=0（Flat）分支 = vec4(u_monoRgb, a)——内容色整体替换。
// 修复前状态：kShaderBit_Monochrome 恒 0（u_shaderFlags 位 0 从不置位）+
// u_monoRgb 无喂数方——monochrome 位打开内容仍全绿（red 计数 0）。
//
// Authored: no reference test exists in display-test-app for monochrome
//           rendering (§5(g) 像素级回归；§11.11 位置断言用既有录制件
//           minimal-imdl 的绿色矩形——Flat+红 后绿系清零/红系占满内容)。
// ---------------------------------------------------------------------------
TEST(TileTreeRender, MonochromeFlatReplacesContentColor)
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
        view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-3, -5.5, -1.2, 3, 5.5, 1.2));
        view.getUeViewport()->InvalidateController();
    }
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        p.monochrome = true;   // addMonochrome 的 viewFlag 位
        style.setViewFlags(dqCommon::ViewFlags(p));
        // ViewAttributes.ts:393-396（Color=红）+ :400-402（Scaled 不勾=Flat）。
        style.setMonochromeColor(0xFF0000FFu);   // tbgr 红
        style.setMonochromeMode(dqCommon::MonochromeMode::Flat);
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
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
    uint32_t minX, maxX, minY, maxY;
    ASSERT_TRUE(contentBBox(frame, w, h, minX, maxX, minY, maxY))
        << "monochrome frame rendered nothing";
    double rCx = 0, gCx2 = 0;
    long const rn = colorCentroid(frame, w, minX, maxX, minY, maxY, 0, rCx);
    long const gn2 = colorCentroid(frame, w, minX, maxX, minY, maxY, 1, gCx2);
    printf("[TILE-MONO] bbox=(%u,%u)-(%u,%u) red=%ld@%.0f green=%ld@%.0f\n",
           minX, minY, maxX, maxY, rn, rCx, gn2, gCx2);
    // Flat+红：内容整体替换为红——红系大量存在、绿系清零（imdl fixture 原色绿）。
    ASSERT_GT(rn, 30) << "monochrome Flat did not replace content color with red";
    ASSERT_EQ(gn2, 0) << "green content survived monochrome Flat replacement";

    view.close();
    spin3(200);
}


// ---------------------------------------------------------------------------
// U11(2)：imdl segments 边缘上屏像素锁（SolidFill 对比色环）。
//
// Authored: no reference test exists in itwinjs-core for imdl edges rendering
//           (参考在浏览器 WebGL 中无 edges 像素回归)。Authorized by CLAUDE.md
//           §5(g)（渲染像素回归）+ §11.11（位置断言；资产为既有录制件
//           minimal-imdl，不原地突变）。判据链：Edge.ts adjustContrast
//           （SolidFill 时 bgi=背景亮度 → 绿色边缘对比为白）+
//           EdgeSettings.wantContrastingColor（!overridden && SolidFill）+
//           MeshGeometry.computeEdgePass（SolidFill 非 SmoothShade → 边缘画）。
//           断言三维：绿色填充存活（面无回归）+ 环带白像素总量（边缘上屏）
//           + 环带四侧分布（位置断言——边缘包围填充）。
// ---------------------------------------------------------------------------
TEST(TileTreeRender, ImdlEdgesRenderContrastingRingInSolidFill)
{
    // 派生测试资产（不改动录制原件，§11.11）：root.imdl 拷贝到 build 目录，
    // 材质 lineWidth 1→5（等长字节补丁）——边缘权重 5 使对比环带 ~2.5px，
    // weight=1 时环带亚像素不可稳定断言。
    std::string const srcTileset = DANQING_TILE_ASSETS_DIR "/minimal-imdl/tileset.json";
    std::string const srcImdl = DANQING_TILE_ASSETS_DIR "/minimal-imdl/root.imdl";
    std::string const workDir = "build/imdl-w5";
    std::string const tilesetPath = workDir + "/tileset.json";
    {
        std::error_code ec;
        std::filesystem::create_directories(workDir, ec);
        std::ifstream in(srcImdl, std::ios::binary);
        ASSERT_TRUE(in.good()) << "cannot open " << srcImdl;
        std::vector<uint8_t> imdl((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
        // "lineWidth" 后的第一个数字 1→5（等长补丁，头部长度/JSON 长度不变）。
        static const uint8_t key[] = "\"lineWidth\"";
        auto pos = std::search(imdl.begin(), imdl.end(), std::begin(key), std::end(key) - 1);
        ASSERT_NE(pos, imdl.end()) << "lineWidth key not found in imdl JSON";
        auto digit = pos + static_cast<long>(std::end(key) - std::begin(key) - 1);
        while (digit != imdl.end() && (*digit == ' ' || *digit == ':'))
            ++digit;
        ASSERT_NE(digit, imdl.end());
        ASSERT_EQ(*digit, '1') << "unexpected lineWidth value";
        *digit = '5';
        std::ofstream out(workDir + "/root.imdl", std::ios::binary);
        ASSERT_TRUE(out.good());
        out.write(reinterpret_cast<char const*>(imdl.data()),
                  static_cast<std::streamsize>(imdl.size()));
        // tileset.json 原样复制。
        std::ifstream tin(srcTileset, std::ios::binary);
        ASSERT_TRUE(tin.good());
        std::ofstream tout(tilesetPath, std::ios::binary);
        tout << tin.rdbuf();
    }

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
        // 取景 2 倍于 ImdlTilesetRendersRecordedFixture 的体盒——矩形（±2.5/±5）
        // 边界必须完整落在视口内，否则边缘环在屏外不可断言。
        view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-9, -16.5, -3.6, 9, 16.5, 3.6));
        view.getUeViewport()->InvalidateController();
    }
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        p.renderMode = dqCommon::RenderMode::SolidFill;  // 对比色通道开关
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
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
        FILE* f = fopen("build/tiletree-imdl-edges.bmp", "wb");
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
            printf("[TILE-IMDL-EDGE] frame dumped to build/tiletree-imdl-edges.bmp\n");
        }
    }

    // (1) 绿色填充存活（fillColor 65280——surface 链无回归）。绿 bbox 自算
    //（背景是暖色渐变 + 绿填充可能不达画幅边缘，contentBBox 不适用）。
    auto isGreen = [](uint8_t const* p) {
        return p[1] > 150 && p[0] < 100 && p[2] < 100;
    };
    uint32_t minX = w, maxX = 0, minY = h, maxY = 0;
    long gn = 0;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            if (isGreen(&frame[(static_cast<size_t>(y) * w + x) * 4])) {
                ++gn;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    printf("[TILE-IMDL-EDGE] green bbox=(%u,%u)-(%u,%u) green=%ld\n",
           minX, minY, maxX, maxY, gn);
    ASSERT_GT(gn, 30) << "green rectangle fill not rendered in SolidFill";
    ASSERT_LT(maxX - minX, w * 9 / 10) << "fill must sit inside the viewport for ring asserts";
    ASSERT_LT(maxY - minY, h * 9 / 10) << "fill must sit inside the viewport for ring asserts";

    // (2) 环带暗像素：绿色经 adjustContrast 在 SolidFill 下对比为黑
    //（bgi=背景亮度≈1.0 → s=0 → vec4(0,0,0)）。判别器 = 明显暗于暖白背景
    // (255,205,142) 与绿填充 (0,192,0) 的过渡色；weight=5 → 环带 ~2.5px。
    auto isDarkRing = [](uint8_t const* p) {
        return p[0] < 170 && p[1] < 150 && p[2] < 130;
    };
    // 环带 = 绿 bbox 各向外扩 6px（边缘四边形外半幅 + AA 弥散落在这里）。
    uint32_t const band = 6;
    long ringTotal = 0, leftSide = 0, rightSide = 0, topSide = 0, bottomSide = 0;
    uint32_t const x0 = minX > band ? minX - band : 0;
    uint32_t const x1 = maxX + band < w ? maxX + band : w - 1;
    uint32_t const y0 = minY > band ? minY - band : 0;
    uint32_t const y1 = maxY + band < h ? maxY + band : h - 1;
    for (uint32_t y = y0; y <= y1; ++y) {
        for (uint32_t x = x0; x <= x1; ++x) {
            bool const inGreen = x >= minX && x <= maxX && y >= minY && y <= maxY;
            if (inGreen) continue;  // 环带只看填充外侧（内侧被覆盖）
            if (isDarkRing(&frame[(static_cast<size_t>(y) * w + x) * 4])) {
                ++ringTotal;
                if (x < minX) ++leftSide;
                else if (x > maxX) ++rightSide;
                else if (y < minY) ++topSide;
                else if (y > maxY) ++bottomSide;
            }
        }
    }
    printf("[TILE-IMDL-EDGE] ring dark total=%ld L=%ld R=%ld T=%ld B=%ld\n",
           ringTotal, leftSide, rightSide, topSide, bottomSide);

    // (3) 总量 + 四侧位置断言（§11.11：边缘包围填充，四侧都要有）。
    ASSERT_GT(ringTotal, 60) << "no contrasting edge ring around the imdl fill";
    EXPECT_GE(leftSide, 5) << "edge ring missing on the left side";
    EXPECT_GE(rightSide, 5) << "edge ring missing on the right side";
    EXPECT_GE(topSide, 5) << "edge ring missing on the top side";
    EXPECT_GE(bottomSide, 3) << "edge ring missing on the bottom side";

    view.close();
    spin3(200);
}

// ---------------------------------------------------------------------------
// U11(3)/Task 5 评审裁定：silhouette 渲染路径的窗口级像素锁（朝向剔除机制）。
//
// Authored: no reference test exists in itwinjs-core for silhouette edge
//           rendering pixels（参考在浏览器 WebGL 中无 edges 像素回归）。
//           Authorized by CLAUDE.md §5(g)（渲染像素回归）+ §11.11（位置断言；
//           cylinder 录制件只读——派生资产 = build 目录拷贝 + lineWidth 1→5
//           等长字节补丁，照 ImdlEdgesRenderContrastingRingInSolidFill 的
//           imdl-w5 先例；tileset.json 由内容包围盒在测试内生成）。
//
// 判据链（checkForSilhouetteDiscard，Edge.ts:107-145——DanQing 移植于
// EdgeShaderBuilder.cpp kCheckForSilhouetteDiscard）：silhouette 边仅在两侧
// 面法线相对视线异侧（dot0*dot1 ≤ perpTol）时绘制。36 棱柱圆柱侧面观测：
//   - 丢弃位形（机制正确）：仅左右切线附近的 1-2 条竖直 silhouette 线绘制，
//     落在内容包围盒左右边缘带；面内部（中央带）无边。
//   - 全画位形（剔除失效）：36 条竖直边全部绘制，中央带被边线淹没。
// 断言 = 绿填充存活 + 左右边缘带边缘像素存在（位置断言）+ 中央带（扣除
// 顶/底盖圆弧的竖直中段）边缘像素趋零（剔除机制的方向性对比）。
// ---------------------------------------------------------------------------
TEST(TileTreeRender, ImdlSilhouetteEdgesRenderAtExtremesAndCulledOnFace)
{
    // 派生资产：cylinder 录制夹具（V1_1，36 棱柱 + 上下盖；segments=盖圆、
    // silhouettes=36 竖直棱边+normalPairs）拷入 build 目录，lineWidth 1→5
    // 等长补丁（环带 ~2.5px 可测，§11.11 不突变原件）。
    std::string const workDir = "build/imdl-cyl-w5";
    std::string const tilesetPath = workDir + "/tileset.json";
    {
        std::error_code ec;
        std::filesystem::create_directories(workDir, ec);
        std::vector<uint8_t> imdl(V1_1::cylinderBytes,
                                  V1_1::cylinderBytes + V1_1::cylinderSize);
        static const uint8_t key[] = "\"lineWidth\"";
        auto pos = std::search(imdl.begin(), imdl.end(), std::begin(key), std::end(key) - 1);
        ASSERT_NE(pos, imdl.end()) << "lineWidth key not found in imdl JSON";
        auto digit = pos + static_cast<long>(std::end(key) - std::begin(key) - 1);
        while (digit != imdl.end() && (*digit == ' ' || *digit == ':'))
            ++digit;
        ASSERT_NE(digit, imdl.end());
        ASSERT_EQ(*digit, '1') << "unexpected lineWidth value";
        *digit = '5';
        std::ofstream out(workDir + "/root.imdl", std::ios::binary);
        ASSERT_TRUE(out.good());
        out.write(reinterpret_cast<char const*>(imdl.data()),
                  static_cast<std::streamsize>(imdl.size()));
        // tileset.json：包围盒取自录制件的 decodedMin/Max（±2, ±2, ±3.00045
        // ——Z 轴圆柱），5% 余量。
        double const r = 2.0004 * 1.05, hz = 3.0005 * 1.05;
        std::ofstream tout(tilesetPath);
        ASSERT_TRUE(tout.good());
        tout << "{\n  \"asset\": {\"version\": \"1.1\"},\n"
             << "  \"geometricError\": 200,\n  \"root\": {\n"
             << "    \"boundingVolume\": {\"box\": [0,0,0, " << r << ",0,0, 0," << r
             << ",0, 0,0," << hz << "]},\n"
             << "    \"geometricError\": 0.01,\n    \"refine\": \"REPLACE\",\n"
             << "    \"content\": {\"uri\": \"root.imdl\"}\n  }\n}\n";
    }

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
        // 取景 1.5 倍于圆柱内容盒（±2/±2/±3）——柱体完整在屏内。
        view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-6, -6, -6, 6, 6, 6));
        view.getUeViewport()->InvalidateController();
    }
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        p.renderMode = dqCommon::RenderMode::SolidFill;  // 对比色通道开关
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
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
        FILE* f = fopen("build/tiletree-imdl-cyl-sil.bmp", "wb");
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
            printf("[TILE-CYL-SIL] frame dumped to build/tiletree-imdl-cyl-sil.bmp\n");
        }
    }

    // 内容包围盒（contentBBox 以首像素为背景基准）。
    uint32_t minX = w, maxX = 0, minY = h, maxY = 0;
    ASSERT_TRUE(contentBBox(frame, w, h, minX, maxX, minY, maxY))
        << "cylinder content rendered nothing";
    printf("[TILE-CYL-SIL] bbox=(%u,%u)-(%u,%u)\n", minX, minY, maxX, maxY);

    // (1) 绿填充存活（fillColor 65280——surface 链无回归）。填充族含受光照
    // 阴影的暗绿（实测 (0,83..129,0) 连续渐变）——判别按绿族（r/g/b 结构）
    // 而非亮度阈值。
    auto isFill = [](uint8_t const* p) {
        return p[1] > 60 && p[0] < 30 && p[2] < 30;
    };
    long fillCount = 0;
    for (uint32_t y = minY; y <= maxY; ++y)
        for (uint32_t x = minX; x <= maxX; ++x)
            if (isFill(&frame[(static_cast<size_t>(y) * w + x) * 4]))
                ++fillCount;
    printf("[TILE-CYL-SIL] fill=%ld\n", fillCount);
    ASSERT_GT(fillCount, 1000) << "green cylinder fill not rendered";

    // 边缘判别器：SolidFill 对比通道把边画成黑（实测 weight-5 silhouette 线 =
    // (0,0,0) 3px；填充族为绿、背景暖白——近黑像素唯一来源是对比边）。
    auto isEdge = [](uint8_t const* p) {
        return p[0] < 40 && p[1] < 40 && p[2] < 40;
    };

    // 竖直中段（扣除顶/底盖圆弧）：y ∈ [minY+35%, maxY-35%]。
    uint32_t const bh = maxY - minY, bw = maxX - minX;
    uint32_t const y0 = minY + bh * 35 / 100;
    uint32_t const y1 = maxY - bh * 35 / 100;
    ASSERT_GT(y1, y0) << "content too small for band analysis";
    // 左右边缘带 = [minX, minX+30%w] / [maxX-30%w, maxX]；中央带 = 其余中段。
    uint32_t const leftX1 = minX + bw * 30 / 100;
    uint32_t const rightX0 = maxX - bw * 30 / 100;
    long leftEdges = 0, rightEdges = 0, centerEdges = 0;
    for (uint32_t y = y0; y <= y1; ++y) {
        for (uint32_t x = minX; x <= maxX; ++x) {
            if (!isEdge(&frame[(static_cast<size_t>(y) * w + x) * 4]))
                continue;
            if (x <= leftX1) ++leftEdges;
            else if (x >= rightX0) ++rightEdges;
            else ++centerEdges;
        }
    }
    printf("[TILE-CYL-SIL] midband edges: L=%ld R=%ld center=%ld (y %u..%u)\n",
           leftEdges, rightEdges, centerEdges, y0, y1);

    // (2) 位置断言：silhouette 线在左右切线处绘制（各 ≥ 一条竖线的量；
    // 实测 ~3px × 中段高 ≈ 1000px/侧）。
    EXPECT_GE(leftEdges, 60) << "no silhouette edge pixels at the left tangent";
    EXPECT_GE(rightEdges, 60) << "no silhouette edge pixels at the right tangent";

    // (3) 朝向剔除机制断言：中央带（柱面内部投影区）无边线——丢弃位形。
    // 全画位形（checkForSilhouetteDiscard 失效）会让 36 条竖直棱边全绘，
    // 中央带像素以千计。
    EXPECT_LT(centerEdges, 50) << "face-interior silhouette edges drawn — "
                                  "checkForSilhouetteDiscard is not culling";

    view.close();
    spin3(200);
}

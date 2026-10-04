// PickDumpSceneTest — dump 场景拾取/点选/高亮贯通像素锁（M-I Task 2）。
//
// 断点（侦察实锤，docs/阶段1-MI-打开性能渲染对齐与拾取-实现计划-2026-09-29.md
// Task 2 / 侦察结论 2）：pick 视图的数据源从未接收场景——
// OpenGLRenderTarget::drawFrame 只把 Scene 合并进正常帧通路
//（m_impl->setScene），Scene 从不进 TargetImpl::m_targetGraphics
//（TargetGraphics::setScene 全仓零生产调用方——TargetImpl.cpp:161-164
// ported-but-uncalled）→ pick 链 TargetImpl::readPixels →
// RenderCommands::initForReadPixels(m_targetGraphics) 的 foreground 恒空 →
// pick 命令 0 → Viewport::PickAtPoint 恒 0 → SelectionTool::onDataButtonUp
// 恒 processMiss（装饰能选、瓦不能选——装饰经 setDecorations 照常入列）。
//
// 修复 = pick 入口把当前 Scene 喂入容器（OpenGLRenderTarget::readPickData /
// readPickDepth → TargetImpl::setSceneContainer(m_scene)）——参考语义：
// Target.readPixelsFromFbo → beginReadPixels → RenderCommands.initForReadPixels
// (this.graphics)（Target.ts:917，:933 drawForReadPixels）每次拾取重画**当前
// 场景**；DanQing 等价接入点取 pick 入口喂入（EQUIVALENCE 登记见
// OpenGLRenderTarget::readPickData 注释——changeScene 时喂会把生产绘制主路径
// 改道到 populateCommandsFromScene 的容器分支 TargetImpl.cpp:340，非最小面）。
//
// 判据（§11.11 位置断言制度）：instances60 saved 视图（DumpOpenChain 锁同款
// 打开链 + 泵至静默 + 帧稳定等待）——
//   ①内容像素（bbox 中心出发的首个内容像素）PickAtPoint > 0、背景角点 = 0；
//   ②真实 SelectionTool::onDataButtonUp（GL 拾取路径）后 SelectionSet 非空
//     且含命中 elementId，HiliteSet 同步含它；
//   ③选中后命中 element 的可视像素簇向 hilite 色 0x23bbfc 移动（色距严格
//     变小 + 位移可观测）、簇被内容 bbox 包住且邻近拾取点、背景角点不变；
//     点背景清选后簇消失（还原）。
//
// 已登记非阻断（M-I(2) commit message 供收口任务入 TD）：同 element 的多
// batch 拷贝中，仅持 feature 表的 batch 实例参与高亮（无表 batch 经
// getOrCreateFeatureOverrideLUT 空表建 LUT 对象 → hasFeatureOverrides() true
// → Overrides 变体采样到 unit 7 上别的 batch 的 LUT——首版取证曾令 17 个
// 实例错染且清选不复原；pick 作用域容器修正后仅表现為"无表实例不染"）。
//
// Authored: no reference test exists in itwinjs-core for tile pick/hilite
//           pixel behavior（浏览器 WebGL 无对应窗口测试；行为锚定
//           Target.ts:768-825 readPixels、:879-918 beginReadPixels（:917
//           initForReadPixels(this.graphics)）、:933 readPixelsFromFbo 的
//           drawForReadPixels、SelectTool.ts:441-469 onDataButtonUp →
//           :408-426 processHit、Hilite.ts:53 默认色 0x23bbfc——真实读过的
//           参考行号）。dump 资产只读（§11.11）；钉值 = 首绿实测。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

#include "View3DInventor.h"
#include "Gui/FeatureOverridesPanel.h"  // M-O(2) I10——Provider 引擎通道

#include "DumpOpenHelper.h"

#include <dqApp/Application.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqRender/tile/ImdlTileTree.h>
#include <dqRender/tile/TileAdmin.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <tuple>
#include <vector>

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {

struct QtEnvPDS {
    QtEnvPDS()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvPDS s_qtPDS;

std::string const kDumpRoot = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";

void spinPDS(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// "内容像素"（与背景差异显著）计数 + 包围盒——DumpOpenChainTest.cpp:74-103
// contentStats 同源（判别阈值 90 先验同款：背景天空渐变最大通道差和
// 72 < 90 < 瓦几何最小 126）。
bool contentStatsPDS(std::vector<uint8_t> const& f, uint32_t w, uint32_t h,
                     long& count, uint32_t& minX, uint32_t& maxX,
                     uint32_t& minY, uint32_t& maxY)
{
    uint8_t const* bg = &f[0];
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
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    return count > 0;
}

// bbox 中心出发的环形扫描取首个内容像素（§11.11——拾取点必须实证落在内容上，
// 不推算：bbox 中心可能落在实例间隙的空洞里）。
bool firstContentPixelNear(std::vector<uint8_t> const& f, uint32_t w, uint32_t /*h*/,
                           uint32_t cx, uint32_t cy, uint32_t minX, uint32_t maxX,
                           uint32_t minY, uint32_t maxY, uint32_t& outX, uint32_t& outY)
{
    uint8_t const* bg = &f[0];
    auto isContent = [&](int x, int y) {
        uint8_t const* p = &f[(static_cast<size_t>(y) * w + static_cast<size_t>(x)) * 4];
        return std::abs(p[0] - bg[0]) + std::abs(p[1] - bg[1]) + std::abs(p[2] - bg[2]) > 90;
    };
    if (isContent(static_cast<int>(cx), static_cast<int>(cy))) {
        outX = cx; outY = cy;
        return true;
    }
    int const maxR = static_cast<int>(std::max(maxX - minX, maxY - minY));
    for (int r = 2; r <= maxR; r += 2) {
        for (int a = 0; a < 360; a += 15) {
            double const rad = a * 3.14159265358979323846 / 180.0;
            int const x = static_cast<int>(cx) + static_cast<int>(r * std::cos(rad));
            int const y = static_cast<int>(cy) + static_cast<int>(r * std::sin(rad));
            if (x < static_cast<int>(minX) || x > static_cast<int>(maxX)
                || y < static_cast<int>(minY) || y > static_cast<int>(maxY))
                continue;
            if (isContent(x, y)) {
                outX = static_cast<uint32_t>(x);
                outY = static_cast<uint32_t>(y);
                return true;
            }
        }
    }
    return false;
}

// 像素取色（帧为 RGBA 设备像素，ReadFrameForTest 全 FBO 回读）。
struct RgbPDS {
    int r, g, b;
};
RgbPDS pixelAtPDS(std::vector<uint8_t> const& f, uint32_t w, uint32_t x, uint32_t y)
{
    uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
    return {p[0], p[1], p[2]};
}
int distToPDS(RgbPDS const& p, RgbPDS const& h)
{
    return std::abs(p.r - h.r) + std::abs(p.g - h.g) + std::abs(p.b - h.b);
}

// 树内 graphics 就绪瓦计数——DumpOpenChainTest.cpp:125-134 同源。
void countGraphicsReadyPDS(dqRender::Tile* tile, long& ready, long& total)
{
    if (!tile)
        return;
    ++total;
    if (tile->hasGraphics())
        ++ready;
    for (dqRender::Tile* child : tile->getChildren())
        countGraphicsReadyPDS(child, ready, total);
}

// 经注册表构造 SelectionTool（PickHiliteSelectionTest makeSelectionTool 同款
// 工厂路径——EventDispatchTest 同源）。
dqApp::PrimitiveTool* makeSelectionToolPDS()
{
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();
    return static_cast<dqApp::PrimitiveTool*>(
        admin.GetRegistry().Create("Select"));
}

// 共享夹具：instances60 打开链（DumpOpenChain 锁同款包定义 + 泵至静默）→
// saved 视图帧 → 内容 bbox → 命中像素（bbox 中心最近内容像素）+ 背景探针。
struct DumpPick {
    Gui::View3DInventor view{nullptr, nullptr, nullptr};
    dqApp::Viewport* vp = nullptr;
    std::optional<dta::DumpOpenResult> opened;
    std::vector<uint8_t> frame;
    uint32_t fw = 0, fh = 0;
    uint32_t px = 0, py = 0;    // 命中像素（设备像素——实证的内容像素）
    uint32_t bgx = 0, bgy = 0;  // 背景探针（设备像素，画面角落、非内容）
    double dpr = 1.0;
    long contentPx = 0;
    uint32_t cMinX = 0, cMaxX = 0, cMinY = 0, cMaxY = 0;  // 内容 bbox（WHERE 域）

    bool setup()
    {
        auto& app = dqApp::Application::Get();
        if (!app.isInitialized()) {
            dqApp::Application::Options opts;
            opts.applicationId = "PickDumpScene";
            opts.applicationVersion = "1.0";
            if (!app.Startup(opts))
                return false;
        }

        view.resize(1000, 700);
        view.show();
        spinPDS(400);

        dta::DumpOpenPackage pkg;
        pkg.imodelRoot = kDumpRoot + "/instances60-imodel-v1";
        pkg.tileRoots = {kDumpRoot + "/instances60-v1", kDumpRoot + "/instances60-drill-v1"};
        opened = dta::openDumpIModel(view, pkg);
        if (!opened.has_value())
            return false;

        // 泵至静默（DumpOpenChainTest pumpToQuiesce 同款强制选择帧形——NotFound
        // 交付不触发失效级联，回退路径在突发帧停后饥饿；saved 视图请求面 = 恰
        // 1 枚 "-b-2-0-0-0-1"，泵形幂等不改请求面）。
        size_t lastLogSize = 0;
        int stable = 0;
        bool quiesced = false;
        for (int i = 0; i < 200 && !quiesced; ++i) {
            view.getUeViewport()->InvalidateController();
            spinPDS(100);
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
        long ready = 0, total = 0;
        for (auto& t : opened->trees)
            countGraphicsReadyPDS(t->getRootTile(), ready, total);
        if (ready <= 0)
            return false;

        vp = view.getUeViewport();
        vp->RenderFrame();

        // 帧稳定等待（LookAtVolume/视图动画收敛——PickHiliteSelectionTest
        // PickScene 的 spinPHS(1600) 等价物，泛化为"连续两帧逐像素相等"，
        // dump 视图的动画时长不假设）：拾取点与高亮簇判据都要求前后帧同
        // 取景，视图漂移会制造整帧伪移动（首跑实证：未等收敛时高亮簇 bbox
        // 超出内容 bbox 右/下边 ~80px）。稳定后再取基准帧。
        {
            std::vector<uint8_t> prev;
            bool frameStable = false;
            int iters = 0;
            for (int i = 0; i < 40 && !frameStable; ++i) {
                spinPDS(150);
                vp->RenderFrame();
                std::vector<uint8_t> cur;
                uint32_t cw = 0, ch = 0;
                if (!vp->ReadFrameForTest(cur, cw, ch))
                    return false;
                fw = cw;
                fh = ch;
                if (!prev.empty() && prev == cur)
                    frameStable = true;
                prev = std::move(cur);
                iters = i + 1;
            }
            printf("[PDS] frame-stable iters=%d stable=%d\n", iters,
                   frameStable ? 1 : 0);
            if (!frameStable)
                return false;
            frame = std::move(prev);
        }

        uint32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
        if (!contentStatsPDS(frame, fw, fh, contentPx, minX, maxX, minY, maxY)) {
            printf("[PDS] setup: no content after stabilization (px0=%d %d %d)\n",
                   frame[0], frame[1], frame[2]);
            return false;
        }
        cMinX = minX; cMaxX = maxX; cMinY = minY; cMaxY = maxY;
        uint32_t const cx = (minX + maxX) / 2;
        uint32_t const cy = (minY + maxY) / 2;
        if (!firstContentPixelNear(frame, fw, fh, cx, cy, minX, maxX, minY, maxY, px, py)) {
            printf("[PDS] setup: no content pixel near bbox center\n");
            return false;
        }

        // 背景探针：左上角内侧；若落在内容 bbox 内则取右下角内侧，且必须
        // 实证为非内容像素（位置断言的"不变"半边要求该点本无内容）。
        bgx = 8; bgy = 8;
        if (bgx >= minX && bgx <= maxX && bgy >= minY && bgy <= maxY) {
            bgx = fw - 8;
            bgy = fh - 8;
        }
        {
            uint8_t const* bg = &frame[0];
            uint8_t const* p = &frame[(static_cast<size_t>(bgy) * fw + bgx) * 4];
            int const d = std::abs(p[0] - bg[0]) + std::abs(p[1] - bg[1])
                          + std::abs(p[2] - bg[2]);
            if (d > 90) {
                printf("[PDS] setup: bg probe is content (d=%d)\n", d);
                return false;
            }
        }

        dpr = fw / static_cast<double>(vp->width());

        // 选中态清零（夹具隔离——PickHiliteSelectionTest PickScene.setup 同款）。
        auto* imodel = vp->GetIModel();
        if (!imodel) {
            printf("[PDS] setup: no iModel on viewport\n");
            return false;
        }
        imodel->GetSelectionSet().EmptyAll();
        imodel->GetHiliteSet().clear();
        vp->RenderFrame();
        return true;
    }

    // CSS（Qt/事件）坐标——PickAtPoint/onDataButtonUp 的 viewPoint 语义。
    int cssX(uint32_t devX) const { return static_cast<int>(std::lround(devX / dpr)); }
    int cssY(uint32_t devY) const { return static_cast<int>(std::lround(devY / dpr)); }
};

}  // namespace

// ---------------------------------------------------------------------------
// ① 拾取链：瓦内容像素 PickAtPoint > 0、背景角点 = 0。
// 锚定：Target.ts:768-825 readPixels（pick 视图重画当前场景后回读 featureId →
//       BatchState.getElementId 反查 elementId）；Viewport.ts:2753-2784。
// 修复前 RED：TargetGraphics foreground 恒空 → pick 命令 0 → 恒 0。
// ---------------------------------------------------------------------------
TEST(PickDumpScene, PickAtTileContentPixelReturnsElementId)
{
    DumpPick s;
    ASSERT_TRUE(s.setup());

    uint32_t const hit = s.vp->PickAtPoint(s.cssX(s.px), s.cssY(s.py));
    uint32_t const miss = s.vp->PickAtPoint(s.cssX(s.bgx), s.cssY(s.bgy));
    printf("[PDS] pick(content dev=(%u,%u) css=(%d,%d) dpr=%.2f content=%ldpx) "
           "-> 0x%08x; pick(bg) -> 0x%08x\n",
           s.px, s.py, s.cssX(s.px), s.cssY(s.py), s.dpr, s.contentPx, hit, miss);
    EXPECT_GT(hit, 0u) << "瓦内容像素拾取为 0——pick 视图未接收场景源";
    EXPECT_EQ(miss, 0u) << "背景角点被拾取（pick 视图混入非场景内容）";
}

// ---------------------------------------------------------------------------
// ② 点选链：真实 SelectionTool::onDataButtonUp（GL 拾取）→ SelectionSet/
//    HiliteSet 含命中 elementId。
// 锚定：SelectTool.ts:441-469 onDataButtonUp → doLocate → :408-426 processHit
//       → :251-274 updateSelection → selectionSet.replace。
// ---------------------------------------------------------------------------
TEST(PickDumpScene, ClickSelectsTileFeatureIntoSelectionSet)
{
    DumpPick s;
    ASSERT_TRUE(s.setup());

    uint32_t const hit = s.vp->PickAtPoint(s.cssX(s.px), s.cssY(s.py));
    ASSERT_GT(hit, 0u) << "前置拾取失败（拾取链断）——点选链无输入";

    dqApp::Application::Get().GetViewManager().SetSelectedViewport(s.vp);
    auto* tool = makeSelectionToolPDS();
    ASSERT_NE(tool, nullptr);
    tool->onPostInstall();

    dqApp::BeButtonEvent ev;
    ev.button = dqApp::BeButton::Data;
    ev.isDown = false;  // Up transition（参考在 release 拾取，SelectTool.ts:441）
    ev.viewport = s.vp;
    ev.viewPoint = dqGeom::Point3d::From(
        static_cast<double>(s.cssX(s.px)), static_cast<double>(s.cssY(s.py)), 0.0);

    auto const result = tool->onDataButtonUp(ev);
    auto* imodel = s.vp->GetIModel();
    printf("[PDS] click(content) -> %d; selSet.size=%d selContains=%d "
           "hiliteContains=%d\n",
           static_cast<int>(result), imodel->GetSelectionSet().size(),
           imodel->GetSelectionSet().Contains(hit) ? 1 : 0,
           imodel->GetHiliteSet().Contains(hit) ? 1 : 0);

    EXPECT_EQ(result, dqApp::EventHandled::Yes);
    EXPECT_GT(imodel->GetSelectionSet().size(), 0)
        << "点选后 SelectionSet 为空——拾取恒 0 时 processMiss 清空";
    EXPECT_TRUE(imodel->GetSelectionSet().Contains(hit))
        << "SelectionSet 不含命中 elementId";
    EXPECT_TRUE(imodel->GetHiliteSet().Contains(hit))
        << "HiliteSet 未同步选中集（SyncWith 断）";

    dqApp::Application::Get().GetToolAdmin().setPrimitiveTool(nullptr);
    delete tool;
}

// ---------------------------------------------------------------------------
// ③ 高亮链：选中 → 命中 element 的可视像素簇向 hilite 色 0x23bbfc 移动
//    （色距严格变小——Surface-Overrides 变体 kOvrBit_Hilited → mix(base,
//    u_hiliteColor, 0.25) 的确定性几何后果，对任意底色/光照缩放成立）、
//    簇被内容 bbox 包住且覆盖拾取点、背景角点不变；点背景清选 → 簇消失。
// 锚定：Viewport.ts:2613-2617（_selectionSetDirty → target.setHiliteSet）；
//       FeatureOverrides.ts:216-224/412-441（updateHilite → LUT Hilited 位）；
//       RenderCommands.ts:681-695（addBatch hilite 路由）；Hilite.ts:53。
// ---------------------------------------------------------------------------
TEST(PickDumpScene, SelectionHilitesTilePixelsAndUnselectRestores)
{
    DumpPick s;
    ASSERT_TRUE(s.setup());

    RgbPDS const kHilite{0x23, 0xbb, 0xfc};  // Hilite.ts:53 默认色
    RgbPDS const bgBefore = pixelAtPDS(s.frame, s.fw, s.bgx, s.bgy);

    dqApp::Application::Get().GetViewManager().SetSelectedViewport(s.vp);
    auto* tool = makeSelectionToolPDS();
    ASSERT_NE(tool, nullptr);
    tool->onPostInstall();

    dqApp::BeButtonEvent ev;
    ev.button = dqApp::BeButton::Data;
    ev.isDown = false;
    ev.viewport = s.vp;
    ev.viewPoint = dqGeom::Point3d::From(
        static_cast<double>(s.cssX(s.px)), static_cast<double>(s.cssY(s.py)), 0.0);
    ASSERT_EQ(tool->onDataButtonUp(ev), dqApp::EventHandled::Yes);
    ASSERT_TRUE(s.vp->GetIModel()->GetSelectionSet().Contains(
        s.vp->PickAtPoint(s.cssX(s.px), s.cssY(s.py))));

    spinPDS(200);
    s.vp->RenderFrame();

    std::vector<uint8_t> frameSel;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(s.vp->ReadFrameForTest(frameSel, w, h));

    // 簇级判据（§11.11 位置断言的单点选址修正）：高亮的可观测面 = 命中
    // element 的可视像素簇，非单点——拾取像素的所有权在重叠几何（同
    // elementId 的多 batch 拷贝）深度并列时随绘制序漂移（取证：同一像素
    // 两次 pick 分属 batch 435/1，LUT 行均已按 elementId 对齐），单像素
    // "变色"断言选址脆弱。判据 = 位移可观测（通道和 ≥15）且向 hilite 色
    // 0x23bbfc 移动（色距严格变小——Surface-Overrides 变体 kOvrBit_Hilited
    // → mix(base, u_hiliteColor, 0.25) 的确定性几何后果，对任意底色/光照
    // 缩放成立）的像素簇。
    auto movedTowardHiliteCluster = [&](std::vector<uint8_t> const& post) {
        long moved = 0;
        uint32_t mMinX = s.fw, mMaxX = 0, mMinY = s.fh, mMaxY = 0;
        for (uint32_t y = 0; y < s.fh; y += 2)
            for (uint32_t x = 0; x < s.fw; x += 2) {
                RgbPDS const p2 = pixelAtPDS(post, s.fw, x, y);
                RgbPDS const p1 = pixelAtPDS(s.frame, s.fw, x, y);
                int const delta = std::abs(p2.r - p1.r) + std::abs(p2.g - p1.g)
                                  + std::abs(p2.b - p1.b);
                if (delta < 15)
                    continue;
                if (distToPDS(p2, kHilite) + 5 < distToPDS(p1, kHilite)) {
                    ++moved;
                    if (x < mMinX) mMinX = x;
                    if (x > mMaxX) mMaxX = x;
                    if (y < mMinY) mMinY = y;
                    if (y > mMaxY) mMaxY = y;
                }
            }
        return std::make_tuple(moved, mMinX, mMinY, mMaxX, mMaxY);
    };
    long moved = 0, mMinX = 0, mMinY = 0, mMaxX = 0, mMaxY = 0;
    std::tie(moved, mMinX, mMinY, mMaxX, mMaxY) = movedTowardHiliteCluster(frameSel);
    RgbPDS const bgSel = pixelAtPDS(frameSel, s.fw, s.bgx, s.bgy);
    int const bgDelta = std::abs(bgSel.r - bgBefore.r) + std::abs(bgSel.g - bgBefore.g)
                        + std::abs(bgSel.b - bgBefore.b);
    printf("[PDS] movedTowardHilite(2px grid)=%ld clusterBBox=(%ld,%ld)-(%ld,%ld) "
           "contentBBox=(%u,%u)-(%u,%u) pick=(%u,%u); bg (%d,%d,%d)->(%d,%d,%d) delta=%d\n",
           moved, mMinX, mMinY, mMaxX, mMaxY,
           s.cMinX, s.cMinY, s.cMaxX, s.cMaxY, s.px, s.py,
           bgBefore.r, bgBefore.g, bgBefore.b, bgSel.r, bgSel.g, bgSel.b, bgDelta);

    // 断言 1（可观测面）：命中 element 的可视像素簇向 hilite 色移动——
    // 首绿实测 2307（2px 网格 = 命中 element 的一个可视实例足迹
    // bbox (996,698)-(1104,806)）；阈值 = 0.2×。同 element 的其余可视拷贝
    // 所在 batch 无 feature 表（见文件头"已登记非阻断"），不参与高亮——
    // 足迹以持有表的 batch 实例为准。
    EXPECT_GE(moved, 500)
        << "选中后无向 hilite 色 0x23bbfc 移动的像素簇（高亮未上屏）";

    // WHERE 断言 2（域）：移动簇被内容 bbox 包住——高亮只落在场景内容上，
    // 天空/背景永不参与（排除整帧污染/全局色调变化）。
    EXPECT_GE(mMinX, s.cMinX);
    EXPECT_GE(mMinY, s.cMinY);
    EXPECT_LE(mMaxX, s.cMaxX);
    EXPECT_LE(mMaxY, s.cMaxY);

    // WHERE 断言 3（位置）：移动簇邻近拾取点（簇 bbox 外扩 200 设备像素须
    // 含拾取点）——高亮的就是所点 element 的可视足迹。不取"簇覆盖拾取点"：
    // 拾取像素的所有权在重叠几何（同 element 的多 batch 拷贝）深度并列时
    // 随绘制序漂移（取证：同一像素两次 pick 分属 batch 435/1），可见高亮
    // 落在持有 feature 表的 batch 实例上——首绿实测簇-拾取点 y 向偏移 68。
    uint32_t const kProximity = 200;
    EXPECT_LE(mMinX, s.px + kProximity);
    EXPECT_GE(mMaxX, s.px > kProximity ? s.px - kProximity : 0u);
    EXPECT_LE(mMinY, s.py + kProximity);
    EXPECT_GE(mMaxY, s.py > kProximity ? s.py - kProximity : 0u);

    // WHERE 断言 4：背景角点不变。
    EXPECT_LE(bgDelta, 10) << "背景像素变化（污染整帧——非命中高亮）";

    // 点背景 → processMiss 清选 → 移动簇消失（还原）。
    dqApp::BeButtonEvent miss;
    miss.button = dqApp::BeButton::Data;
    miss.isDown = false;
    miss.viewport = s.vp;
    miss.viewPoint = dqGeom::Point3d::From(
        static_cast<double>(s.cssX(s.bgx)), static_cast<double>(s.cssY(s.bgy)), 0.0);
    ASSERT_EQ(tool->onDataButtonUp(miss), dqApp::EventHandled::Yes);
    EXPECT_TRUE(s.vp->GetIModel()->GetSelectionSet().isEmpty());

    spinPDS(200);
    s.vp->RenderFrame();
    std::vector<uint8_t> frameAfter;
    ASSERT_TRUE(s.vp->ReadFrameForTest(frameAfter, w, h));
    long movedAfter = 0;
    std::tie(movedAfter, std::ignore, std::ignore, std::ignore, std::ignore) =
        movedTowardHiliteCluster(frameAfter);
    printf("[PDS] movedTowardHilite after-unselect=%ld\n", movedAfter);
    EXPECT_LE(movedAfter, moved / 10) << "清选后高亮簇未消失";

    dqApp::Application::Get().GetToolAdmin().setPrimitiveTool(nullptr);
    delete tool;
}

// ---------------------------------------------------------------------------
// M-O(2) I10 — FeatureOverrides 面板引擎通道（选择集 appearance 覆盖上屏）。
// 锚定：FeatureOverrides.ts:14-97（Provider.addFeatureOverrides → ovrs.override
//       (elementId, appearance)）+ Viewport.ts:2619-2645（overridesNeeded =
//       changeFlags.areFeatureOverridesDirty → rebuild）+ Viewport.ts:1637-1640
//       （setFeatureOverrideProviderChanged 置位）。
// 修复前 RED：Step 9 的 m_featureOverridesDirty 只在首帧为 true——provider
// 变更永不触发重建 → override 后像素不变。
// ---------------------------------------------------------------------------
TEST(PickDumpScene, FeatureOverrideProviderRestylesHitElement)
{
    DumpPick s;
    ASSERT_TRUE(s.setup());

    // 命中元素 id（不进选择集——避免 hilite wash 混淆 override 判据；面板的
    // overrideElements = 同一 overrideElement 循环[选择集驱动]，通道同构）。
    uint32_t const hit = s.vp->PickAtPoint(s.cssX(s.px), s.cssY(s.py));
    ASSERT_GT(hit, 0u);

    s.vp->RenderFrame();
    std::vector<uint8_t> before;
    uint32_t bw = 0, bh = 0;
    ASSERT_TRUE(s.vp->ReadFrameForTest(before, bw, bh));

    // 命中簇基准色（(px,py) 邻域内容像素均值——实例球单色行，均值稳定）。
    auto clusterMean = [&](std::vector<uint8_t> const& f) -> RgbPDS {
        long r = 0, g = 0, b = 0;
        int n = 0;
        uint8_t const* bg = &f[0];
        for (int dy = -24; dy <= 24; dy += 2) {
            for (int dx = -24; dx <= 24; dx += 2) {
                int const x = static_cast<int>(s.px) + dx;
                int const y = static_cast<int>(s.py) + dy;
                if (x < 0 || y < 0 || x >= static_cast<int>(bw) || y >= static_cast<int>(bh))
                    continue;
                uint8_t const* p = &f[(static_cast<size_t>(y) * bw + x) * 4];
                if (std::abs(p[0] - bg[0]) + std::abs(p[1] - bg[1]) + std::abs(p[2] - bg[2]) <= 90)
                    continue;  // 非内容
                r += p[0]; g += p[1]; b += p[2]; ++n;
            }
        }
        return n > 0 ? RgbPDS{static_cast<int>(r / n), static_cast<int>(g / n), static_cast<int>(b / n)}
                     : RgbPDS{0, 0, 0};
    };
    RgbPDS const beforeMean = clusterMean(before);
    ASSERT_GT(beforeMean.r + beforeMean.g + beforeMean.b, 0) << "命中簇无内容像素";

    // 自适应覆盖色：取命中色最弱通道 → 置 255 其余 0（红/绿/蓝三选——与任意
    // 实例行色的主导通道都翻转；instances60 行色 green/purple/blue/yellow/
    // orange/red）。
    int or_, og, ob;
    if (beforeMean.r <= beforeMean.g && beforeMean.r <= beforeMean.b) {
        or_ = 255; og = 0; ob = 0;  // 原色弱红 → 覆盖红
    } else if (beforeMean.g <= beforeMean.b) {
        or_ = 0; og = 255; ob = 0;  // 弱绿 → 绿
    } else {
        or_ = 0; og = 0; ob = 255;  // 弱蓝 → 蓝
    }

    // Provider（面板引擎通道同款——Gui::FeatureOverridesProvider::override
    // ElementsByArray 的直接形态，I9 recall 消费面）。
    auto* provider = Gui::FeatureOverridesProvider::getOrCreate(s.vp);
    ASSERT_NE(provider, nullptr);
    std::vector<Gui::FeatureOverridesProvider::ElementOverride> ovrs;
    Gui::FeatureOverridesProvider::ElementOverride eo;
    eo.id = std::to_string(hit);
    eo.fsaJson = QString::fromUtf8(QJsonDocument(QJsonObject{
        {"rgb", QJsonObject{{"r", or_}, {"g", og}, {"b", ob}}},
    }).toJson(QJsonDocument::Compact)).toStdString();
    ovrs.push_back(eo);
    provider->overrideElementsByArray(ovrs);

    s.vp->RenderFrame();
    std::vector<uint8_t> after;
    uint32_t aw = 0, ah = 0;
    ASSERT_TRUE(s.vp->ReadFrameForTest(after, aw, ah));
    ASSERT_EQ(aw, bw);
    ASSERT_EQ(ah, bh);

    RgbPDS const kOvr{or_, og, ob};
    // 簇级判据（同 movedTowardHiliteCluster 形态）：覆盖的可观测面 = 命中
    // element 的可视像素簇（全帧差分扫描），非拾取点邻域——同 element 的多
    // batch 拷贝中仅持 feature 表的 batch 实例参与覆盖（文件头"已登记非阻断"），
    // 可视覆盖实例可偏离拾取点（高亮锁首绿实测 y 向偏移 68）。判据 = 位移
    // 可观测（通道和 ≥15）且向覆盖色移动（色距严格变小——Surface-Overrides
    // 变体 kOvrBit_Rgb → baseColor.rgb = 第 2 texel rgb 的确定性几何后果）。
    auto movedTowardOverrideCluster = [&](std::vector<uint8_t> const& post) {
        long moved = 0;
        uint32_t mMinX = s.fw, mMaxX = 0, mMinY = s.fh, mMaxY = 0;
        for (uint32_t y = 0; y < s.fh; y += 2)
            for (uint32_t x = 0; x < s.fw; x += 2) {
                RgbPDS const p2 = pixelAtPDS(post, s.fw, x, y);
                RgbPDS const p1 = pixelAtPDS(before, s.fw, x, y);
                int const delta = std::abs(p2.r - p1.r) + std::abs(p2.g - p1.g)
                                  + std::abs(p2.b - p1.b);
                if (delta < 15)
                    continue;
                if (distToPDS(p2, kOvr) + 5 < distToPDS(p1, kOvr)) {
                    ++moved;
                    if (x < mMinX) mMinX = x;
                    if (x > mMaxX) mMaxX = x;
                    if (y < mMinY) mMinY = y;
                    if (y > mMaxY) mMaxY = y;
                }
            }
        return std::make_tuple(moved, mMinX, mMinY, mMaxX, mMaxY);
    };
    long moved = 0, mMinX = 0, mMinY = 0, mMaxX = 0, mMaxY = 0;
    std::tie(moved, mMinX, mMinY, mMaxX, mMaxY) = movedTowardOverrideCluster(after);
    printf("[FOVR] hit=0x%08x before=(%d,%d,%d) ovr=(%d,%d,%d) "
           "movedTowardOvr(2px grid)=%ld clusterBBox=(%ld,%ld)-(%ld,%ld) "
           "contentBBox=(%u,%u)-(%u,%u) pick=(%u,%u)\n",
           hit, beforeMean.r, beforeMean.g, beforeMean.b, or_, og, ob, moved,
           mMinX, mMinY, mMaxX, mMaxY,
           s.cMinX, s.cMinY, s.cMaxX, s.cMaxY, s.px, s.py);

    // WHERE ①（可观测面）：命中 element 的可视像素簇向覆盖色移动——一个
    // 实例球足迹（高亮锁同域首绿 2307，2px 网格）；阈值 = 0.2×。
    EXPECT_GE(moved, 500) << "覆盖后无向覆盖色移动的像素簇（override 未上屏）";

    // WHERE ②（域）：移动簇被内容 bbox 包住——覆盖只落在场景内容上，
    // 天空/背景永不参与（排除整帧污染/全局色调变化）。
    EXPECT_GE(mMinX, s.cMinX);
    EXPECT_GE(mMinY, s.cMinY);
    EXPECT_LE(mMaxX, s.cMaxX);
    EXPECT_LE(mMaxY, s.cMaxY);

    // WHERE ③（位置）：移动簇邻近拾取点（簇 bbox 外扩 200 设备像素须含
    // 拾取点——覆盖的就是所点 element 的可视足迹；不取"簇覆盖拾取点"：
    // 拾取像素的所有权在重叠几何随绘制序漂移，可见覆盖落在持有 feature
    // 表的 batch 实例上——高亮锁首绿实测簇-拾取点 y 向偏移 68）。
    uint32_t const kProximity = 200;
    EXPECT_LE(mMinX, s.px + kProximity);
    EXPECT_GE(mMaxX, s.px > kProximity ? s.px - kProximity : 0u);
    EXPECT_LE(mMinY, s.py + kProximity);
    EXPECT_GE(mMaxY, s.py > kProximity ? s.py - kProximity : 0u);

    // WHERE ④：远端内容（内容 bbox 左上角最近内容像素——另一实例球）逐通道
    // 不变（override 只作用于命中元素）。
    {
        uint32_t dx = 0, dy = 0;
        ASSERT_TRUE(firstContentPixelNear(after, aw, ah,
                                          (s.cMinX + s.cMaxX) / 4, (s.cMinY + s.cMaxY) / 4,
                                          s.cMinX, s.cMaxX, s.cMinY, s.cMaxY, dx, dy));
        // 距命中点足够远（不同实例——instances60 多球布局）。
        ASSERT_GT(std::abs(static_cast<int>(dx) - static_cast<int>(s.px))
                      + std::abs(static_cast<int>(dy) - static_cast<int>(s.py)),
                 60);
        RgbPDS const beforeFar = pixelAtPDS(before, bw, dx, dy);
        RgbPDS const afterFar = pixelAtPDS(after, aw, dx, dy);
        printf("[FOVR] far=(%u,%u) before=(%d,%d,%d) after=(%d,%d,%d)\n",
               dx, dy, beforeFar.r, beforeFar.g, beforeFar.b, afterFar.r, afterFar.g,
               afterFar.b);
        EXPECT_LE(distToPDS(beforeFar, afterFar), 24) << "非命中元素的像素被改动";
    }

    // 清面（provider 撤下——夹具隔离；宿主所有权角色 delete）。
    Gui::FeatureOverridesProvider::remove(s.vp);
    delete provider;
}

// PickHiliteSelectionTest — 拾取/高亮/点选三链像素级回归（2026-09-16）。
//
// 断点背景（原 docs/audits/2026-09-16 拾取/高亮/点选三链审计，已随 2026-09-24
// 历史文档清理删除；断点清单如下）：
//   B1 PickAtPoint 读颜色缓冲当 featureId；B3 pick 附件 RGBA8≠uint 输出；
//   B4 drawPass 不消费 isReadPixelsInProgress；B5 装饰 isPickable=false 被过滤；
//   B7/B8 target 级 hilite LUT 死对象 + drawPass 后置 flags 变异。
// 修复前这些测试为 RED（锁定），修复后转 GREEN——比真实 app 手点可靠一个数量级
// （§12.9 黑方块 saga 教训 6）。
//
// 判据设计（§11.11 位置断言制度）：
//   拾取——装饰面心（contentBBox 中心）PickAtPoint == 装饰 pickableId；背景角点 == 0。
//   高亮——选中后面心像素变色（朝参考默认 hilite 色 0x23bbfc 方向移动）、背景角点
//          像素不变（WHERE：装饰在中央、背景在角落——两个采样点钉住位置自由度）。
//   点选——真实 SelectionTool::onDataButtonUp（GL 拾取路径，非 SetPickResultForTest 桩）
//          后 SelectionSet 含 pickableId；点背景后清空且面心像素还原。
//
// Authored: no reference test exists in itwinjs-core for decoration pick/hilite
//           pixel behavior（浏览器 WebGL 无对应窗口测试；行为锚定
//           ElementLocateManager.ts:199-291 doPick、Viewport.ts:2613-2617
//           setHiliteSet、SelectTool.ts:441-469 onDataButtonUp、Hilite.ts:53
//           默认色 0x23bbfc/visibleRatio 0.25——真实读过的参考行号）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"

#include <dqApp/Application.h>
#include <dqApp/GltfDecoration.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewState.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#ifndef DANQING_GLTF_ASSETS_DIR
#define DANQING_GLTF_ASSETS_DIR "."
#endif

namespace {
struct QtEnvPHS {
    QtEnvPHS() {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
static QtEnvPHS s_qtPHS;

// TEMP-DIAG（拾取 saga，env 门控）：未处理异常时符号化打印调用栈——套内
// SEH 崩塌定位用（gtest 的 Stack trace 为空）。
// Authored: forensic affordance，无参考等价物。
#include <windows.h>
#include <dbghelp.h>
static LONG WINAPI crashPrinter(EXCEPTION_POINTERS* ep)
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
static bool const s_crashHook = [] {
    if (getenv("DANQING_CRASH_STACK"))
        SetUnhandledExceptionFilter(crashPrinter);
    return true;
}();

void spinPHS(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 画面中"内容像素"（与背景差异显著）的包围盒（同 GltfStandardViewTest 的取证判据）。
bool contentBBoxPHS(std::vector<uint8_t> const& f, uint32_t w, uint32_t h,
                    uint32_t& minX, uint32_t& maxX, uint32_t& minY, uint32_t& maxY)
{
    uint8_t const* bg = &f[0];
    minX = w; maxX = 0; minY = h; maxY = 0;
    bool any = false;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
            int const dr = std::abs(p[0] - bg[0]), dg = std::abs(p[1] - bg[1]),
                      db = std::abs(p[2] - bg[2]);
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

// 像素取色（帧为 RGBA 设备像素，ReadFrameForTest 全 FBO 回读）。
struct Rgb {
    int r, g, b;
};
Rgb pixelAt(std::vector<uint8_t> const& f, uint32_t w, uint32_t x, uint32_t y)
{
    uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
    return {p[0], p[1], p[2]};
}

// 共享场景夹具：真实 View3DInventor + BoxTextured 装饰 + 关 Grid/ACS（内容 bbox
// 即装饰包围盒），返回帧与内容 bbox。dpr = FBO 宽 / 视口 CSS 宽（PickAtPoint 收
// CSS 坐标——参考 viewPoint 语义，Viewport.ts cssPixelsToDevicePixels 的逆运算在
// 拾取实现内部处理）。
struct PickScene {
    // View3DInventor(parent, parentWidget, doc) — GltfStandardViewTest 同款三空参。
    Gui::View3DInventor view{nullptr, nullptr, nullptr};
    dqApp::Viewport* vp = nullptr;
    dqApp::GltfDecoration* deco = nullptr;
    uint32_t pickableId = 0;
    std::vector<uint8_t> frame;
    uint32_t fw = 0, fh = 0;
    uint32_t cx = 0, cy = 0;      // 装饰面心（设备像素）
    uint32_t bgx = 0, bgy = 0;    // 背景点（设备像素，画面角落）
    double dpr = 1.0;

    bool setup()
    {
        view.resize(1000, 700);
        view.show();
        spinPHS(400);
        if (!view.loadGltf(DANQING_GLTF_ASSETS_DIR "/BoxTextured/BoxTextured.gltf"))
            return false;
        spinPHS(300);
        vp = view.getUeViewport();
        if (!vp) return false;
        deco = view.gltfDecoration();
        if (!deco) return false;
        pickableId = deco->GetPickableId();
        {
            auto& style = vp->GetView()->GetDisplayStyle();
            auto p = style.getViewFlags().Properties();
            p.grid = false;
            p.acsTriad = false;
            style.setViewFlags(dqCommon::ViewFlags(p));
        }
        vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
        spinPHS(300);
        spinPHS(1600);  // LookAtVolume 动画收敛（同 GltfStandardViewTest 的节奏）
        vp->RenderFrame();
        if (!vp->ReadFrameForTest(frame, fw, fh)) return false;
        uint32_t minX, maxX, minY, maxY;
        if (!contentBBoxPHS(frame, fw, fh, minX, maxX, minY, maxY)) return false;
        cx = (minX + maxX) / 2;
        cy = (minY + maxY) / 2;
        // 背景点：左上角内侧（远离内容 bbox——内容若占据左上角则取右下角）。
        bgx = 8;
        bgy = 8;
        if (bgx >= minX && bgx <= maxX && bgy >= minY && bgy <= maxY) {
            bgx = fw - 8;
            bgy = fh - 8;
        }
        dpr = fw / static_cast<double>(vp->width());
        // 选中态清零（夹具隔离）。
        auto* imodel = vp->GetIModel();
        if (imodel) {
            imodel->GetSelectionSet().EmptyAll();
            imodel->GetHiliteSet().clear();
        }
        vp->RenderFrame();
        return true;
    }

    // CSS（Qt/事件）坐标——PickAtPoint/onDataButtonUp 的 viewPoint 语义。
    int cssX(uint32_t devX) const { return static_cast<int>(std::lround(devX / dpr)); }
    int cssY(uint32_t devY) const { return static_cast<int>(std::lround(devY / dpr)); }
};

// 经注册表构造 SelectionTool（同 EventDispatchTest 的工厂路径）。
dqApp::PrimitiveTool* makeSelectionTool()
{
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();
    return static_cast<dqApp::PrimitiveTool*>(
        admin.GetRegistry().Create("Select"));
}
}  // namespace

// ---------------------------------------------------------------------------
// 拾取链：pick buffer 写入 + 回读。
// 断言：装饰面心 PickAtPoint == pickableId；背景角点 == 0。
// 锚定：ElementLocateManager.ts:199-291（doPick 像素扫描语义）；Target.ts:768-825
//       （readPixels 渲染 pick 视图后回读 featureId）。
// ---------------------------------------------------------------------------
TEST(PickHiliteSelection, PickAtDecorationCenterReturnsPickableId)
{
    PickScene s;
    ASSERT_TRUE(s.setup());

    uint32_t const hit = s.vp->PickAtPoint(s.cssX(s.cx), s.cssY(s.cy));
    printf("[PHS] pick(center css=(%d,%d) dev=(%u,%u) dpr=%.2f) -> 0x%08x (want 0x%08x)\n",
           s.cssX(s.cx), s.cssY(s.cy), s.cx, s.cy, s.dpr, hit, s.pickableId);
    EXPECT_EQ(hit, s.pickableId);
}

TEST(PickHiliteSelection, PickAtBackgroundReturnsZero)
{
    PickScene s;
    ASSERT_TRUE(s.setup());

    uint32_t const miss = s.vp->PickAtPoint(s.cssX(s.bgx), s.cssY(s.bgy));
    printf("[PHS] pick(bg css=(%d,%d)) -> 0x%08x (want 0)\n",
           s.cssX(s.bgx), s.cssY(s.bgy), miss);
    EXPECT_EQ(miss, 0u);
}

// ---------------------------------------------------------------------------
// 点选链：真实 onDataButtonUp（GL 拾取）→ SelectionSet。
// 锚定：SelectTool.ts:441-469 onDataButtonUp → processHit(:408-426) →
//       updateSelection(:251-274) → selectionSet.replace。
// ---------------------------------------------------------------------------
TEST(PickHiliteSelection, RealClickSelectsDecorationIntoSelectionSet)
{
    PickScene s;
    ASSERT_TRUE(s.setup());

    dqApp::Application::Get().GetViewManager().SetSelectedViewport(s.vp);
    auto* tool = makeSelectionTool();
    ASSERT_NE(tool, nullptr);
    tool->onPostInstall();

    dqApp::BeButtonEvent ev;
    ev.button = dqApp::BeButton::Data;
    ev.isDown = false;  // Up transition（参考在 release 拾取，SelectTool.ts:441）
    ev.viewport = s.vp;
    ev.viewPoint = dqGeom::Point3d::From(
        static_cast<double>(s.cssX(s.cx)), static_cast<double>(s.cssY(s.cy)), 0.0);

    auto const result = tool->onDataButtonUp(ev);
    printf("[PHS] click(center) -> %d; selSet.size=%u\n",
           static_cast<int>(result),
           static_cast<unsigned>(s.vp->GetIModel()->GetSelectionSet().size()));

    EXPECT_EQ(result, dqApp::EventHandled::Yes);
    EXPECT_TRUE(s.vp->GetIModel()->GetSelectionSet().Contains(s.pickableId));

    dqApp::Application::Get().GetToolAdmin().setPrimitiveTool(nullptr);
    delete tool;
}

// ---------------------------------------------------------------------------
// 高亮链：选中 → 装饰面心像素变色 + 背景角点像素不变（位置断言：中央变、角落
// 不变，钉住 WHERE 自由度）。清选后还原。
// 锚定：Viewport.ts:2613-2617（_selectionSetDirty → target.setHiliteSet）；
//       FeatureOverrides.ts:412-441（update → updateHilite → LUT Hilited 位）；
//       Hilite.ts:53（默认色 0x23bbfc、visibleRatio 0.25）。
// ---------------------------------------------------------------------------
TEST(PickHiliteSelection, SelectionHilitesDecorationPixelsAndUnselectRestores)
{
    PickScene s;
    ASSERT_TRUE(s.setup());

    Rgb const faceBefore = pixelAt(s.frame, s.fw, s.cx, s.cy);
    Rgb const bgBefore = pixelAt(s.frame, s.fw, s.bgx, s.bgy);

    dqApp::Application::Get().GetViewManager().SetSelectedViewport(s.vp);
    auto* tool = makeSelectionTool();
    ASSERT_NE(tool, nullptr);
    tool->onPostInstall();

    dqApp::BeButtonEvent ev;
    ev.button = dqApp::BeButton::Data;
    ev.isDown = false;
    ev.viewport = s.vp;
    ev.viewPoint = dqGeom::Point3d::From(
        static_cast<double>(s.cssX(s.cx)), static_cast<double>(s.cssY(s.cy)), 0.0);
    ASSERT_EQ(tool->onDataButtonUp(ev), dqApp::EventHandled::Yes);
    ASSERT_TRUE(s.vp->GetIModel()->GetSelectionSet().Contains(s.pickableId));

    spinPHS(200);
    s.vp->RenderFrame();

    std::vector<uint8_t> frameSel;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(s.vp->ReadFrameForTest(frameSel, w, h));
    Rgb const faceSel = pixelAt(frameSel, w, s.cx, s.cy);
    Rgb const bgSel = pixelAt(frameSel, w, s.bgx, s.bgy);

    printf("[PHS] face before=(%d,%d,%d) selected=(%d,%d,%d); bg before=(%d,%d,%d) selected=(%d,%d,%d)\n",
           faceBefore.r, faceBefore.g, faceBefore.b, faceSel.r, faceSel.g, faceSel.b,
           bgBefore.r, bgBefore.g, bgBefore.b, bgSel.r, bgSel.g, bgSel.b);

    // WHERE 断言 1：装饰面心变色（朝参考默认 hilite 0x23bbfc 移动——色距显著）。
    int const faceDelta = std::abs(faceSel.r - faceBefore.r) +
                          std::abs(faceSel.g - faceBefore.g) +
                          std::abs(faceSel.b - faceBefore.b);
    EXPECT_GE(faceDelta, 30) << "选中后装饰面心像素未变化（高亮未上屏）";

    // WHERE 断言 2：背景角点不变（排除整帧污染/全局色调变化）。
    int const bgDelta = std::abs(bgSel.r - bgBefore.r) +
                        std::abs(bgSel.g - bgBefore.g) +
                        std::abs(bgSel.b - bgBefore.b);
    EXPECT_LE(bgDelta, 10) << "背景像素变化（污染整帧——非装饰高亮）";

    // 点背景 → processMiss 清选 → 像素还原。
    dqApp::BeButtonEvent miss;
    miss.button = dqApp::BeButton::Data;
    miss.isDown = false;
    miss.viewport = s.vp;
    miss.viewPoint = dqGeom::Point3d::From(
        static_cast<double>(s.cssX(s.bgx)), static_cast<double>(s.cssY(s.bgy)), 0.0);
    ASSERT_EQ(tool->onDataButtonUp(miss), dqApp::EventHandled::Yes);
    EXPECT_TRUE(s.vp->GetIModel()->GetSelectionSet().isEmpty());

    spinPHS(200);
    s.vp->RenderFrame();
    std::vector<uint8_t> frameAfter;
    ASSERT_TRUE(s.vp->ReadFrameForTest(frameAfter, w, h));
    Rgb const faceAfter = pixelAt(frameAfter, w, s.cx, s.cy);
    int const restoreDelta = std::abs(faceAfter.r - faceBefore.r) +
                             std::abs(faceAfter.g - faceBefore.g) +
                             std::abs(faceAfter.b - faceBefore.b);
    printf("[PHS] face after-unselect=(%d,%d,%d) restoreDelta=%d\n",
           faceAfter.r, faceAfter.g, faceAfter.b, restoreDelta);
    EXPECT_LE(restoreDelta, 10) << "清选后装饰面心像素未还原";

    dqApp::Application::Get().GetToolAdmin().setPrimitiveTool(nullptr);
    delete tool;
}

// ---------------------------------------------------------------------------
// Hover flash 链（flash = 参考的 hover 高亮机制）。
// 锚定：AccuSnap.onMotion→HitDetail ctor 设 vp.flashedId（AccuSnap.ts:1095、
//       HitDetail.ts:333）；Viewport.processFlash 强度 0→maxIntensity 随
//       duration 爬升（Viewport.ts:2521-2543，默认 0.25s/1.0，
//       FlashSettings.ts:69-85）；renderFrame step（:2682-2687
//       target.setFlashed）；FeatureOverrides.updateFlashed 置 LUT Flashed 位
//       （:338-375）；doApplyFlash 默认 Brighten：rgb += intensity×0.2
//       （glsl/FeatureSymbology.ts:685-707，maxBrighten=0.2）。
// 判据：flash 满强度时面心 r/g/b 各增 ≈0.2×255≈51（方向为增亮——区别于
//       选中高亮的向 0x23bbfc 混色）；背景角点不变；清 flash 还原。
// ---------------------------------------------------------------------------

// Flash 渲染链：直接驱动 SetFlashedId（隔离 motion 插桩，锁 flash 本体）。
TEST(PickHiliteSelection, HoverFlashBrightensDecorationFace)
{
    PickScene s;
    ASSERT_TRUE(s.setup());

    Rgb const faceBefore = pixelAt(s.frame, s.fw, s.cx, s.cy);
    Rgb const bgBefore = pixelAt(s.frame, s.fw, s.bgx, s.bgy);

    s.vp->SetFlashedId(s.pickableId);
    spinPHS(350);  // ≥ flashDuration 0.25s：强度爬满（参考默认 FlashSettings）
    s.vp->RenderFrame();

    std::vector<uint8_t> frameFlashed;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(s.vp->ReadFrameForTest(frameFlashed, w, h));
    Rgb const faceFlashed = pixelAt(frameFlashed, w, s.cx, s.cy);
    Rgb const bgFlashed = pixelAt(frameFlashed, w, s.bgx, s.bgy);

    printf("[PHS] face before=(%d,%d,%d) flashed=(%d,%d,%d); bg=(%d,%d,%d)->(%d,%d,%d)\n",
           faceBefore.r, faceBefore.g, faceBefore.b,
           faceFlashed.r, faceFlashed.g, faceFlashed.b,
           bgBefore.r, bgBefore.g, bgBefore.b, bgFlashed.r, bgFlashed.g, bgFlashed.b);

    // WHERE 断言 1：装饰面心增亮（Brighten：各通道 +≈51，方向为正）。
    EXPECT_GE(faceFlashed.r - faceBefore.r, 20) << "flash 后面心未增亮（Brighten 语义）";
    EXPECT_GE(faceFlashed.g - faceBefore.g, 20);
    EXPECT_GE(faceFlashed.b - faceBefore.b, 20);

    // WHERE 断言 2：背景角点不变。
    int const bgDelta = std::abs(bgFlashed.r - bgBefore.r) +
                        std::abs(bgFlashed.g - bgBefore.g) +
                        std::abs(bgFlashed.b - bgBefore.b);
    EXPECT_LE(bgDelta, 10) << "背景像素变化（flash 污染整帧）";

    // 清 flash → 还原。
    s.vp->SetFlashedId(0);
    spinPHS(120);
    s.vp->RenderFrame();
    std::vector<uint8_t> frameAfter;
    ASSERT_TRUE(s.vp->ReadFrameForTest(frameAfter, w, h));
    Rgb const faceAfter = pixelAt(frameAfter, w, s.cx, s.cy);
    int const restoreDelta = std::abs(faceAfter.r - faceBefore.r) +
                             std::abs(faceAfter.g - faceBefore.g) +
                             std::abs(faceAfter.b - faceBefore.b);
    printf("[PHS] face after-unflash=(%d,%d,%d) restoreDelta=%d\n",
           faceAfter.r, faceAfter.g, faceAfter.b, restoreDelta);
    EXPECT_LE(restoreDelta, 10) << "清 flash 后面心未还原";
}

// Motion 插桩链：合成 QMouseMove 于装饰面心 → hover locate → flash。
// 锚定参考事件驱动（EventController mousemove → ToolAdmin → AccuSnap.onMotion）；
// DanQing 等价：Qt mouseMoveEvent 记录 hover 点，renderFrame 帧合并 locate
// （Chromium 每帧合并的等价物——EQUIVALENCE 见 Viewport.cpp hover 注释）。
TEST(PickHiliteSelection, HoverMotionFlashesDecoration)
{
    PickScene s;
    ASSERT_TRUE(s.setup());

    Rgb const faceBefore = pixelAt(s.frame, s.fw, s.cx, s.cy);

    // 合成 MouseMove（Qt 真事件路径：viewport 是 QWidget）。
    QPoint const center(s.cssX(s.cx), s.cssY(s.cy));
    QMouseEvent move(QEvent::MouseMove, center, s.vp->mapToGlobal(center),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(s.vp, &move);
    spinPHS(350);
    s.vp->RenderFrame();

    std::vector<uint8_t> frameFlashed;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(s.vp->ReadFrameForTest(frameFlashed, w, h));
    Rgb const faceFlashed = pixelAt(frameFlashed, w, s.cx, s.cy);
    printf("[PHS] motion face before=(%d,%d,%d) flashed=(%d,%d,%d) flashedId=0x%08x\n",
           faceBefore.r, faceBefore.g, faceBefore.b,
           faceFlashed.r, faceFlashed.g, faceFlashed.b,
           s.vp->GetFlashedId());

    EXPECT_EQ(s.vp->GetFlashedId(), s.pickableId)
        << "motion 未定位到装饰（hover locate 链断）";
    EXPECT_GE(faceFlashed.r - faceBefore.r, 20) << "motion hover 后面心未增亮";
    EXPECT_GE(faceFlashed.g - faceBefore.g, 20);
    EXPECT_GE(faceFlashed.b - faceBefore.b, 20);

    // 移到背景 → flash 清除 → 还原。
    QPoint const corner(s.cssX(s.bgx), s.cssY(s.bgy));
    QMouseEvent moveAway(QEvent::MouseMove, corner, s.vp->mapToGlobal(corner),
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(s.vp, &moveAway);
    spinPHS(200);
    s.vp->RenderFrame();
    std::vector<uint8_t> frameAfter;
    ASSERT_TRUE(s.vp->ReadFrameForTest(frameAfter, w, h));
    Rgb const faceAfter = pixelAt(frameAfter, w, s.cx, s.cy);
    int const restoreDelta = std::abs(faceAfter.r - faceBefore.r) +
                             std::abs(faceAfter.g - faceBefore.g) +
                             std::abs(faceAfter.b - faceBefore.b);
    printf("[PHS] motion face after-move-away=(%d,%d,%d) restoreDelta=%d flashedId=0x%08x\n",
           faceAfter.r, faceAfter.g, faceAfter.b, restoreDelta, s.vp->GetFlashedId());
    EXPECT_EQ(s.vp->GetFlashedId(), 0u) << "移开后 flash 未清除";
    EXPECT_LE(restoreDelta, 10) << "移开后面心未还原";
}

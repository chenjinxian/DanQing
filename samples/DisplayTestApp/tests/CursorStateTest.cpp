// CursorStateTest — 光标/光圈/snap 状态机回归（2026-09-18 用户报四条 Deco 示例
// 与 DTA 不一致：hover 多 snap 十字 / Fit 后卡十字 / Rotate 光圈泄漏 / 离开不消）。
// Authored: no reference test exists（状态机级回归；行为锚定 ToolAdmin.ts
// startViewTool:1815-1841 / exitViewTool:1795-1812 / SuspendedToolState:107-141 /
// onMouseLeave:952-960 + SelectTool.ts:239 enableSnap=false + AccuSnap.ts:1315-1331
// 默认关 + ViewTool.ts:1183-1190 rotate 手柄恒命中 + :146 focusIn→getHandleCursor）。
//
// 判据全部取自同一真视口的状态面（viewManager.cursor() / toolState.locateCircleOn /
// accuSnap.toolState），与 DTA 实测对照（CDP 读 canvas.style.cursor + 截图）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>
#include <cstdio>

#ifdef _WIN32
// SEH 崩塌定位用（gtest 的 Stack trace 为空）。Authored: forensic affordance。
#include <windows.h>
#include <dbghelp.h>
static LONG WINAPI crashPrinterCS(EXCEPTION_POINTERS* ep)
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
        if (SymFromAddr(s_process, reinterpret_cast<DWORD64>(frames[i]), &disp, sym))
            printf("[CRASH] #%u %s+0x%llx\n", i, sym->Name, static_cast<unsigned long long>(disp));
        else
            printf("[CRASH] #%u %p\n", i, frames[i]);
    }
    fflush(stdout);
    return EXCEPTION_CONTINUE_SEARCH;
}
static bool const s_crashHookCS = [] {
    if (getenv("DANQING_CRASH_STACK")) {
        // Vectored handler：先于 gtest 的 SEH __except 帧触发（SetUnhandledExceptionFilter
        // 会被 gtest 捕获抢先吞掉）。
        AddVectoredExceptionHandler(1, crashPrinterCS);
    }
    return true;
}();
#endif

#include "View3DInventor.h"
#include "DecorationGeometryExample.h"

#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewTool.h>
#include <dqApp/Viewport.h>
#include <dqApp/AccuSnap.h>

namespace { struct QtEnvCS { QtEnvCS() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvCS s_qtCS;

namespace {
void spinCS(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}
// TEMP-DIAG：DANQING_DUMP_FRAME=path → FBO RGBA 落盘 PPM（bottom-up 翻转），
// 亲眼检查 canvas 十字/光圈的渲染形状（2026-09-20 #1 十字形状取证）。
void dumpFramePpm(std::vector<uint8_t> const& rgba, uint32_t w, uint32_t h, char const* path)
{
    if (FILE* f = fopen(path, "wb")) {
        fprintf(f, "P6\n%u %u\n255\n", w, h);
        for (int row = static_cast<int>(h) - 1; row >= 0; --row)
            for (uint32_t col = 0; col < w; ++col)
                fwrite(&rgba[(static_cast<size_t>(row) * w + col) * 4], 1, 3, f);
        fclose(f);
    }
}
}  // namespace

TEST(CursorState, SelectDefaultsRotateSwitchFitRestoreLeaveClears)
{
    // idleTool/Select 注册在 Application::Startup（main.cpp:59 同款；幂等）。
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "CursorState";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinCS(400);

    auto* vp = view.getUeViewport();
    ASSERT_NE(vp, nullptr);
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    auto& viewManager = dqApp::Application::Get().GetViewManager();
    auto& accuSnap = dqApp::Application::Get().GetAccuSnap();

    // 装 Selection 工具（CursorPositionTest.cpp:46-56 同款 harness 补装）。
    auto* selTool = admin.GetRegistry().Create("Select");
    ASSERT_NE(selTool, nullptr);
    selTool->onPostInstall();   // initSelectTool → initLocateElements(true, false, "default")

    // ---- 默认态（Select）：cursor="default" + 光圈开 + snap 关 ----
    EXPECT_EQ(viewManager.cursor(), "default")
        << "Select 工具的参考光标是 default（SelectTool.ts:239）";
    EXPECT_TRUE(admin.isLocateCircleOn()) << "locate 光圈应开（initLocateElements true）";
    EXPECT_FALSE(accuSnap.isSnapEnabled())
        << "Select 工具 enableSnap(false)——snap 十字不应出现（changeLocateState 吞参则破）";

    // ---- Rotate 激活：安装期 crosshair + 光圈关；首次 motion 后 rotate 光标 ----
    auto* rotate = new dqApp::RotateViewTool(vp, /*oneShot=*/false);
    ASSERT_TRUE(rotate->run());
    EXPECT_EQ(viewManager.cursor(), "crosshair")
        << "startViewTool 的安装期默认（ToolAdmin.ts:1837）";
    EXPECT_FALSE(admin.isLocateCircleOn())
        << "startViewTool 关光圈（ToolAdmin.ts:1833）";

    // 真事件链 motion → focusHitHandle → focusIn → rotate 光标。
    // 注意避开视图中心：ViewTargetCenter 在目标点 0.15in 内命中且 priority High
    // 压过 Rotate（ViewTool.ts:950-968）——中心区 hover 的参考光标是 default。
    QMouseEvent move(QEvent::MouseMove, QPointF(700, 250), vp->mapToGlobal(QPoint(700, 250)),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(vp, &move);
    spinCS(300);
    EXPECT_EQ(viewManager.cursor(), "rotate")
        << "rotate 手柄恒命中 + focusIn（ViewTool.ts:1187-1190 + :146）";

    // 中心点（目标十字的隐形命中窗）→ TargetCenter 焦点 → default 光标。
    QMouseEvent moveCenter(QEvent::MouseMove, QPointF(500, 350), vp->mapToGlobal(QPoint(500, 350)),
                           Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(vp, &moveCenter);
    spinCS(300);
    EXPECT_EQ(viewManager.cursor(), "default")
        << "目标点 0.15in 内 TargetCenter 命中（High>Medium），光标回落 default";

    // ---- Fit（one-shot）：退出后恢复挂起前的 Select 态 ----
    auto* fit = new dqApp::FitViewTool(vp, /*oneShot=*/true);
    ASSERT_TRUE(fit->run());
    spinCS(1500);   // fit 动画 1s
    EXPECT_EQ(viewManager.cursor(), "default")
        << "exitViewTool 应恢复 SuspendedToolState 快照的 default 光标";
    EXPECT_TRUE(admin.isLocateCircleOn())
        << "快照应恢复光圈开（SuspendedToolState.stop 恢复 toolState）";

    // ---- 离开视口：光圈停画（cursorView 清空） + snap 十字灭 + flash 清 ----
    admin.setLocateCircleOn(true);
    ASSERT_TRUE(admin.isLocateCircleOn());
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(vp, &leave);
    EXPECT_EQ(admin.cursorView(), nullptr)
        << "onMouseLeave 应清 currentInputState.viewport（ToolAdmin.ts:958）";
    EXPECT_FALSE(accuSnap.cross.isActive())
        << "onMouseLeave 后 snap 十字应消（accuSnap.clear 的 DanQing 视觉落点）";

    view.close();
    spinCS(200);
}

// ---------------------------------------------------------------------------
// 真实光标离开取证（2026-09-19 用户报 #4：真实 app 中离开渲染窗口光圈消失
// "不灵敏"）。合成 QEvent::Leave 的上一测试只验证 handler 语义；本测试移动
// **物理光标**（QCursor::setPos → OS WM_MOUSEMOVE/WM_MOUSELEAVE 真实投递链）
// 验证 Qt enter/leave 投递 + onMouseLeave + 帧重绘全链。
// Authored: no reference test exists（Qt 投递链取证；锚定 ToolAdmin.ts:841/952-960）。
// 环境敏感（需前台窗口+物理光标），DANQING_REAL_CURSOR=1 门控，默认跳过。
// ---------------------------------------------------------------------------
TEST(CursorState, RealCursorLeaveClearsLocateCircle)
{
    if (!getenv("DANQING_REAL_CURSOR"))
        GTEST_SKIP() << "真实光标取证，DANQING_REAL_CURSOR=1 时运行";

    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "CursorStateRealLeave";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    view.raise();
    view.activateWindow();
    spinCS(600);

#ifdef _WIN32
    HWND const fore = GetForegroundWindow();
    HWND const ours = reinterpret_cast<HWND>(view.winId());
    printf("[REALLEAVE] foreground=%p ours=%p match=%d\n",
           (void*)fore, (void*)ours, fore == ours ? 1 : 0);
    if (fore != ours)
        GTEST_SKIP() << "窗口未前台（无人值守/被遮挡）——物理光标取证无效";
#endif

    auto* vp = view.getUeViewport();
    ASSERT_NE(vp, nullptr);
    auto& admin = dqApp::Application::Get().GetToolAdmin();

    auto* selTool = admin.GetRegistry().Create("Select");
    ASSERT_NE(selTool, nullptr);
    selTool->onPostInstall();
    ASSERT_TRUE(admin.isLocateCircleOn());

    QPoint const savedCursor = QCursor::pos();

    // 物理光标移入视口中心 → OS 投递 mousemove → hover 记录 + 光圈出现。
    QPoint const inside = vp->mapToGlobal(QPoint(vp->width() / 2, vp->height() / 2));
    QCursor::setPos(inside);
    spinCS(600);
    printf("[REALLEAVE] after move-in: cursorView=%p\n", (void*)admin.cursorView());
    ASSERT_EQ(admin.cursorView(), vp) << "物理移入应建立 cursorView（投递链前置条件）";

    // 物理光标移出窗口（屏幕左上角 (5,5)，远离窗口）→ OS 应投递 leave。
    QCursor::setPos(QPoint(5, 5));
    spinCS(600);
    printf("[REALLEAVE] after move-out: cursorView=%p\n", (void*)admin.cursorView());

    // 帧泵：离开后光圈应从帧内容消失（WHERE：光圈最后位置不再有白圈）。
    for (int i = 0; i < 4; ++i) { vp->RenderFrame(); spinCS(50); }

    QCursor::setPos(savedCursor);   // 恢复用户光标位置

    EXPECT_EQ(admin.cursorView(), nullptr)
        << "物理离开后 cursorView 应清空（onMouseLeave，ToolAdmin.ts:958）——"
           "若仍非空，Qt 未投递 Leave，即'不灵敏'根因";

    view.close();
    spinCS(200);
}
// Authored: no reference test exists（渲染输出级回归；行为锚定
// Viewport.ts:3435-3503 pickDepthPoint + ViewTool.ts:341-376 previewDepthPoint +
// ToolAdmin.ts:2330-2348 doZoom 目标三来源）。
// 复现配方：Deco 示例 + Rotate 工具 + 悬停砖纹盒 + 滚轮。
// ---------------------------------------------------------------------------
TEST(CursorState, DepthPreviewFollowsMouseAndWheelAnchorsAtGeometry)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "CursorStateDepth";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinCS(400);

    Gui::openDecorationGeometryExample(view);
    spinCS(2500);

    auto* vp = view.getUeViewport();
    ASSERT_NE(vp, nullptr);

    // 砖纹盒（行 y=0、列 x=3：世界 (3.5, 0.5, 0.5)，顶面 z∈[0,1]）。
    dqGeom::Point3d const boxCenter = dqGeom::Point3d::From(3.5, 0.5, 0.5);
    dqGeom::Point3d const cssPt = vp->WorldToView(boxCenter);
    printf("[DEPTH] boxCenter css=(%.1f, %.1f)\n", cssPt.x, cssPt.y);

    // ---- pickDepthPoint：盒上命中 → Geometry 源 + 命中点在盒面附近 ----
    {
        auto const hit = vp->pickDepthPoint(boxCenter, 15.0);
        printf("[DEPTH] pick over box: source=%d origin=(%.2f, %.2f, %.2f) id=%u\n",
               static_cast<int>(hit.source), hit.origin.x, hit.origin.y, hit.origin.z, hit.sourceId);
        EXPECT_EQ(hit.source, dqApp::DepthPointSource::Geometry)
            << "盒上应命中几何（featureId + depthAndOrder 回读链）";
        EXPECT_NE(hit.sourceId, 0u) << "几何命中应带 elementId（hitDetail.sourceId 对应物）";
        // 命中点应在盒的包围范围内（x∈[3,4]、y∈[0,1]、z∈[0,1]，允许边缘容差）。
        // 校准：本视图 far=1538（backgroundMap 深视锥）下 24-bit 深度的世界残差
        // 达 ~0.5 单位（参考同为 RGBA8 24-bit 打包——分辨率上限一致）。
        EXPECT_GT(hit.origin.x, 2.5); EXPECT_LT(hit.origin.x, 4.5);
        EXPECT_GT(hit.origin.y, -0.6); EXPECT_LT(hit.origin.y, 1.6);
        EXPECT_GT(hit.origin.z, -0.2); EXPECT_LT(hit.origin.z, 1.3);
    }

    // ---- pickDepthPoint：天空处 → 背景图求交回退（DTA 实测 src=2 BackgroundMap） ----
    {
        dqGeom::Point3d const skyPoint = dqGeom::Point3d::From(3.5, 20.0, 0.5);   // 几何区外的 z=0 面外一点
        auto const hit = vp->pickDepthPoint(skyPoint, 15.0);
        printf("[DEPTH] pick over sky: source=%d origin=(%.2f, %.2f, %.2f)\n",
               static_cast<int>(hit.source), hit.origin.x, hit.origin.y, hit.origin.z);
        // 参考链（Viewport.ts:3475-3484）：blank connection 有 Exton ecefLocation
        // （Surface.ts:185 默认）→ backgroundMapGeometry 椭球求交在 cartesianRange
        // 内修正到 z=0 cartesian 平面 → BackgroundMap 源。DTA 实测 deco 示例空处
        // 悬停 src=2(BackgroundMap)、isDefaultDepth=false（2026-09-20 CDP 取证）。
        EXPECT_EQ(hit.source, dqApp::DepthPointSource::BackgroundMap)
            << "空处应命中背景图（椭球求交→cartesian 平面修正）——DTA 实测 src=2";
        EXPECT_NEAR(hit.origin.z, 0.0, 0.1) << "cartesian 平面修正后 z=0";
        EXPECT_NEAR(hit.normal.z, 1.0, 1.0e-6) << "法线为平面法线 (0,0,1)";
    }

    // ---- Rotate 悬停 → 预览圆出现在悬停点（帧像素对比） ----
    auto* rotate = new dqApp::RotateViewTool(vp, /*oneShot=*/false);
    ASSERT_TRUE(rotate->run());

    vp->RenderFrame();
    std::vector<uint8_t> frame0;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(vp->ReadFrameForTest(frame0, w, h));

    QPoint const hover(static_cast<int>(std::lround(cssPt.x)), static_cast<int>(std::lround(cssPt.y)));
    QMouseEvent move(QEvent::MouseMove, QPointF(hover), vp->mapToGlobal(hover),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(vp, &move);
    spinCS(400);
    for (int i = 0; i < 3; ++i) { vp->RenderFrame(); spinCS(50); }

    std::vector<uint8_t> frame1;
    ASSERT_TRUE(vp->ReadFrameForTest(frame1, w, h));
    if (auto const* dumpPath = getenv("DANQING_DUMP_FRAME"))
        dumpFramePpm(frame1, w, h, dumpPath);

    // 悬停点附近的 60×60 设备像素区内应有变化像素（预览圆/十字）。
    double const dpr = static_cast<double>(w) / vp->width();
    int const cx = static_cast<int>(cssPt.x * dpr);
    int const cy = static_cast<int>(cssPt.y * dpr);
    size_t changed = 0;
    for (int y = cy - 30; y < cy + 30; ++y) {
        for (int x = cx - 30; x < cx + 30; ++x) {
            if (x < 0 || y < 0 || x >= static_cast<int>(w) || y >= static_cast<int>(h)) continue;
            // FBO 回读为 GL bottom-up
            size_t const px = (static_cast<size_t>(h - 1 - y) * w + x) * 4;
            int const d = std::abs(static_cast<int>(frame1[px]) - static_cast<int>(frame0[px]))
                        + std::abs(static_cast<int>(frame1[px + 1]) - static_cast<int>(frame0[px + 1]))
                        + std::abs(static_cast<int>(frame1[px + 2]) - static_cast<int>(frame0[px + 2]));
            if (d > 12) ++changed;
        }
    }
    printf("[DEPTH] preview-circle changed px near hover = %zu\n", changed);
    EXPECT_GT(changed, 30u) << "Rotate 悬停应有深度预览圆+十字（ViewTool.ts:341-376）";

    // ---- 滚轮：锚点应钉在悬停的几何（而非视图目标点） ----
    {
        auto* view3d = vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr;
        ASSERT_NE(view3d, nullptr);
        dqGeom::Point3d const targetBefore = view3d->GetTargetPoint();
        for (int i = 0; i < 5; ++i) {
            QWheelEvent we(QPointF(hover), vp->mapToGlobal(hover),
                           QPoint(0, 0), QPoint(0, 120 * 5),
                           Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(vp, &we);
        }
        spinCS(800);
        dqGeom::Point3d const targetAfter = view3d->GetTargetPoint();
        double const drift = std::sqrt((targetAfter.x - boxCenter.x) * (targetAfter.x - boxCenter.x)
                                     + (targetAfter.y - boxCenter.y) * (targetAfter.y - boxCenter.y)
                                     + (targetAfter.z - boxCenter.z) * (targetAfter.z - boxCenter.z));
        printf("[DEPTH] wheel anchor: before=(%.2f,%.2f,%.2f) after=(%.2f,%.2f,%.2f) drift-to-box=%.2f\n",
               targetBefore.x, targetBefore.y, targetBefore.z,
               targetAfter.x, targetAfter.y, targetAfter.z, drift);
        // 目标点应逼近盒心（pickNearestVisibleGeometry 命中盒面）——
        // 不是视图目标点（GetTargetPoint 回退）或任意漂移。
        EXPECT_LT(drift, 2.5) << "滚轮锚点应钉在悬停几何（pickNearestVisibleGeometry）";
    }

    view.close();
    spinCS(200);
}

// ---------------------------------------------------------------------------
// Rotate 锚定态（2026-09-19 用户报 #3 残留：单击确认旋转轴后 DTA 表现为
// "十字固定不动 + 无圈"，再次点击恢复初始）。根因 = ViewTargetCenter 手柄未移植
// （锚定态固定十字的唯一绘制者，ViewTool.ts:1001-1021 inHandleModify 分支）。
// Authored: no reference test exists（渲染输出级回归；行为锚定
// ViewTool.ts:464-489 onDataButtonDown + :740-753 processFirstPoint +
// :1193-1214 ViewRotate.firstPoint + :1001-1021 ViewTargetCenter.drawHandle）。
// 位置断言（§11.11）：圆环区（光圈）必须消失、锚点核区（十字）必须留存、
// 恢复后圆环必须出现在新悬停点 B——三个 WHERE 判据钉住锚定/恢复两个迁移。
// ---------------------------------------------------------------------------
TEST(CursorState, RotateAnchorFixesCrossAndClearsPreview)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "CursorStateAnchor";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinCS(400);

    Gui::openDecorationGeometryExample(view);
    spinCS(2500);

    auto* vp = view.getUeViewport();
    ASSERT_NE(vp, nullptr);

    // 砖纹盒（列 x=3.5,行 y=0.5）——须远离视图中心，否则 testHit 会被
    // ViewTargetCenter 的 0.15in 命中窗截获（priority High > Rotate Medium，
    // ViewTool.ts:950-968），点的就不是旋转手柄而是目标十字本身。
    dqGeom::Point3d const boxCenter = dqGeom::Point3d::From(3.5, 0.5, 0.5);
    dqGeom::Point3d const cssA = vp->WorldToView(boxCenter);
    auto* view3d0 = vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr;
    ASSERT_NE(view3d0, nullptr);
    dqGeom::Point3d const viewCenter = vp->WorldToView(view3d0->GetTargetPoint());
    double const offCenter = std::hypot(cssA.x - 500.0, cssA.y - 350.0);
    printf("[ANCHOR] cssA=(%.1f,%.1f) viewCenter=(%.1f,%.1f) offCenter=%.1f\n",
           cssA.x, cssA.y, viewCenter.x, viewCenter.y, offCenter);
    ASSERT_GT(offCenter, 60.0) << "锚定点须避开目标十字命中窗（0.15in≈14px）";

    auto* rotate = new dqApp::RotateViewTool(vp, /*oneShot=*/false);
    ASSERT_TRUE(rotate->run());
    vp->RenderFrame();
    spinCS(100);
    std::vector<uint8_t> frame0;
    uint32_t w0 = 0, h0 = 0;
    ASSERT_TRUE(vp->ReadFrameForTest(frame0, w0, h0));   // 基线帧（无装饰）

    auto sendMove = [&](QPoint p) {
        QMouseEvent mv(QEvent::MouseMove, QPointF(p), vp->mapToGlobal(p),
                       Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &mv);
    };
    auto sendClick = [&](QPoint p) {
        QMouseEvent dn(QEvent::MouseButtonPress, QPointF(p), vp->mapToGlobal(p),
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &dn);
        QMouseEvent up(QEvent::MouseButtonRelease, QPointF(p), vp->mapToGlobal(p),
                       Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &up);
    };
    // 装饰面探针：世界 overlay（预览圆）与画布十字（drawCross）的计数+位置。
    // 注意：不能用像素环判据——hover 的 flash 高亮把盒面整片打亮，环区变化
    // 像素被 flash 淹没（2026-09-19 取证：woCmds=0 而环区 diff 不变实锤）。
    auto countOverlay = [&]() { return vp->GetDecorations().worldOverlay.size(); };
    auto crossNear = [&](dqGeom::Point2d const& css, double tol) -> int {
        int found = 0;
        for (auto const& dec : vp->GetDecorations().canvasDecorations) {
            if (dec.position.has_value()
                && std::abs(dec.position->x - css.x) <= tol && std::abs(dec.position->y - css.y) <= tol)
                ++found;
        }
        return found;
    };
    auto pumpFrames = [&]() { for (int i = 0; i < 3; ++i) { vp->RenderFrame(); spinCS(50); } };

    // ---- 悬停 A：预览圆（worldOverlay）+ 十字跟手在 A ----
    QPoint const A(static_cast<int>(std::lround(cssA.x)), static_cast<int>(std::lround(cssA.y)));
    dqGeom::Point2d const ptA = dqGeom::Point2d::From(cssA.x, cssA.y);
    sendMove(A);
    spinCS(400);
    pumpFrames();
    size_t const overlayHover = countOverlay();
    int const crossHover = crossNear(ptA, 8.0);
    printf("[ANCHOR] hover: worldOverlay=%zu crossNearA=%d\n", overlayHover, crossHover);
    ASSERT_GT(overlayHover, 0u) << "悬停应有预览圆（WorldOverlay 椭圆，ViewTool.ts:369-373）";
    ASSERT_GT(crossHover, 0) << "悬停应有预览十字跟手（ViewTool.ts:375 drawCross）";

    // ---- 单击 A：锚定——预览圆消失 + 固定十字留在锚点 ----
    sendClick(A);
    spinCS(400);
    EXPECT_TRUE(rotate->inDynamicUpdate) << "firstPoint 应 beginDynamicUpdate（ViewTool.ts:1209）";
    EXPECT_TRUE(rotate->inHandleModify) << "processFirstPoint 应置 inHandleModify（ViewTool.ts:745）";
    EXPECT_EQ(rotate->nPts, 1);
    pumpFrames();
    size_t const overlayAnchored = countOverlay();
    int const crossAnchored = crossNear(ptA, 8.0);
    printf("[ANCHOR] anchored: worldOverlay=%zu crossNearA=%d targetCenter=(%.2f,%.2f,%.2f)\n",
           overlayAnchored, crossAnchored,
           rotate->targetCenterWorld.x, rotate->targetCenterWorld.y, rotate->targetCenterWorld.z);
    EXPECT_EQ(overlayAnchored, 0u)
        << "锚定态预览圆应消失（pickDepthPoint 非预览清 + inDynamicUpdate 门，ViewTool.ts:396-399）";
    EXPECT_GT(crossAnchored, 0)
        << "锚点应留固定十字（ViewTargetCenter.drawHandle inHandleModify 小十字，ViewTool.ts:1011-1017）";

    // ---- 十字形状像素锁（2026-09-20 用户实测：右上斜线伪影） ----
    // 十字 = 两条独立臂（moveTo 断子路径，HTML canvas 语义）；GLCanvasContext
    // 曾把全 beginPath 扫成一条折线 → 右臂端点 (outline,0) 与上臂端点 (0,-outline)
    // 被连出右上斜线。判据（§11.11 位置断言）：锚点右上象限（dx∈[4,12],
    // dy∈[-12,-4]，避开轴上的臂本身）内与基线帧的差异必须为零——有斜线则破。
    // 判据前置：先让 hover flash 强度完全衰减（SetFlashedId(0) 后 ramp-down 的
    // 帧会把盒面染色——那不是斜线）。
    spinCS(1200);
    for (int i = 0; i < 6; ++i) { vp->RenderFrame(); spinCS(80); }
    {
        std::vector<uint8_t> frame2;
        uint32_t w = 0, h = 0;
        ASSERT_TRUE(vp->ReadFrameForTest(frame2, w, h));
        if (auto const* dumpPath = getenv("DANQING_DUMP_FRAME"))
            dumpFramePpm(frame2, w, h, dumpPath);   // 锚定帧（十字形状亲眼核对）
        // 判据原点取**离点击点最近的画布十字**（装饰列表可能含多个十字——
        // 预览十字/锚定十字共存时取锚定者；臂本身只占 |d|≤3 的轴带，象限判据
        // 须以真实中心为原点）。
        std::optional<dqGeom::Point2d> crossPos;
        double best = 1.0e9;
        for (auto const& dec : vp->GetDecorations().canvasDecorations) {
            if (!dec.position.has_value()) continue;
            double const d = std::hypot(dec.position->x - cssA.x, dec.position->y - cssA.y);
            if (d < best) { best = d; crossPos = dec.position; }
        }
        ASSERT_TRUE(crossPos.has_value());
        printf("[ANCHOR] canvas decs=%zu chosen cross=(%.1f,%.1f) cssA=(%.1f,%.1f)\n",
               vp->GetDecorations().canvasDecorations.size(),
               crossPos->x, crossPos->y, cssA.x, cssA.y);
        for (auto const& dec : vp->GetDecorations().canvasDecorations) {
            if (dec.position.has_value())
                printf("[ANCHOR]   dec pos=(%.1f,%.1f)\n", dec.position->x, dec.position->y);
        }
        double const dpr = static_cast<double>(w) / vp->width();
        int const cx = static_cast<int>(crossPos->x * dpr);
        int const cy = static_cast<int>(crossPos->y * dpr);
        size_t stray = 0;
        for (int dy = -12; dy <= -4; ++dy) {
            for (int dx = 4; dx <= 12; ++dx) {
                int const x = cx + static_cast<int>(dx * dpr);
                int const y = cy + static_cast<int>(dy * dpr);
                if (x < 0 || y < 0 || x >= static_cast<int>(w) || y >= static_cast<int>(h)) continue;
                size_t const px = (static_cast<size_t>(h - 1 - y) * w + x) * 4;  // GL bottom-up
                // 判据：f2 显著**变暗**（斜线 = 黑色描边笔画）。不用绝对差——
                // 锚定态 hover flash 会把盒面整体打亮（变白）淹没判据（2026-09-20
                // 取证：flash 使绝对差 81/81 全满格，与斜线无关）。
                int const darkened = (static_cast<int>(frame0[px]) - static_cast<int>(frame2[px]))
                                   + (static_cast<int>(frame0[px + 1]) - static_cast<int>(frame2[px + 1]))
                                   + (static_cast<int>(frame0[px + 2]) - static_cast<int>(frame2[px + 2]));
                if (darkened > 24) ++stray;
            }
        }
        printf("[ANCHOR] darkened (diagonal-stroke) pixels in upper-right quadrant = %zu\n", stray);
        EXPECT_EQ(stray, 0u) << "十字两臂必须独立——右上象限不应有黑色斜线（moveTo 断子路径）";
    }

    // ---- 再击 A：完成并恢复初始态；移动到新点 B 预览圆回归 ----
    sendClick(A);
    spinCS(400);
    EXPECT_EQ(rotate->nPts, 0) << "oneShot=false → onReinitialize 归零（ViewTool.ts:484-485/451）";
    EXPECT_FALSE(rotate->inHandleModify);

    QPoint const B(700, 200);   // 天空区（远离几何与锚点）
    dqGeom::Point2d const ptB = dqGeom::Point2d::From(700.0, 200.0);
    sendMove(B);
    spinCS(400);
    pumpFrames();
    size_t const overlayRestored = countOverlay();
    int const crossAtB = crossNear(ptB, 8.0);
    printf("[ANCHOR] restored: worldOverlay=%zu crossNearB=%d\n", overlayRestored, crossAtB);
    EXPECT_GT(overlayRestored, 0u) << "恢复初始态后预览圆应回归（蓝圈跟手）";
    EXPECT_GT(crossAtB, 0) << "恢复初始态后十字应跟手到 B";

    view.close();
    spinCS(200);
}

// ---------------------------------------------------------------------------
// 锚定拖动旋转场景存活回归（2026-09-20 用户报：Deco 示例 + Rotate + 单击左键 →
// 视口错乱；真实 app 取证复现：锚定后移动 → 内容整屏消失）。harness 复现定位：
// 锚定 + 拖动后 FBO 内容必须仍在、视锥必须仍然健康（extents 不 NaN/不爆炸）。
// Authored: no reference test exists（渲染输出级回归；行为锚定 ViewTool.ts
// onMouseMotion:545-557 processPoint(inDynamics) → ViewRotate.perform:1216-1286）。
// ---------------------------------------------------------------------------
TEST(CursorState, RotateAnchorDragKeepsSceneAlive)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "CursorStateDrag";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinCS(400);

    Gui::openDecorationGeometryExample(view);
    spinCS(2500);

    auto* vp = view.getUeViewport();
    ASSERT_NE(vp, nullptr);
    auto* view3d = vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr;
    ASSERT_NE(view3d, nullptr);

    // 基线：内容像素量（中央区非背景像素数）+ 视图四元数
    auto countContentPx = [&](std::vector<uint8_t> const& f, uint32_t w, uint32_t h) {
        size_t n = 0;
        // 中央 60% 区域采样：与浅蓝天空背景差异显著的即内容
        for (uint32_t y = h / 5; y < h * 4 / 5; y += 4) {
            for (uint32_t x = w / 5; x < w * 4 / 5; x += 4) {
                size_t const px = (static_cast<size_t>(y) * w + x) * 4;
                // 天空背景浅蓝≈(235,242,248)；内容（红/绿/蓝盒/砖纹）饱和度更高
                int const mx = std::max({(int)f[px], (int)f[px+1], (int)f[px+2]});
                int const mn = std::min({(int)f[px], (int)f[px+1], (int)f[px+2]});
                if (mx - mn > 40) ++n;
            }
        }
        return n;
    };

    std::vector<uint8_t> frame0;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(vp->ReadFrameForTest(frame0, w, h));
    size_t const content0 = countContentPx(frame0, w, h);
    printf("[DRAG] baseline content px = %zu\n", content0);
    ASSERT_GT(content0, 100u) << "基线应有内容（Deco 示例已开）";

    dqGeom::Point3d const boxCenter = dqGeom::Point3d::From(3.5, 0.5, 0.5);
    dqGeom::Point3d const cssA = vp->WorldToView(boxCenter);
    QPoint const A(static_cast<int>(std::lround(cssA.x)), static_cast<int>(std::lround(cssA.y)));

    auto sendClick = [&](QPoint p) {
        QMouseEvent dn(QEvent::MouseButtonPress, QPointF(p), vp->mapToGlobal(p),
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &dn);
        QMouseEvent up(QEvent::MouseButtonRelease, QPointF(p), vp->mapToGlobal(p),
                       Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &up);
    };
    auto sendMove = [&](QPoint p) {
        QMouseEvent mv(QEvent::MouseMove, QPointF(p), vp->mapToGlobal(p),
                       Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &mv);
    };

    auto* rotate = new dqApp::RotateViewTool(vp, /*oneShot=*/false);
    ASSERT_TRUE(rotate->run());
    spinCS(500);   // 工具安装触发的失效级联先重建完再拾取（pick 读的是重建后的缓冲）
    {
        // 视锥结构取证：前/后面宽比 + 眼距（对照 DTA 同项：rear 40.40 front 17.31
        // extents 23.73 focusDist 11.87——参考链 extents 在拖动全程恒定）。
        dqCommon::Frustum const wf = vp->getWorldFrustum();
        auto d = [&](int a, int b) {
            double const dx = wf.points[a].x - wf.points[b].x;
            double const dy = wf.points[a].y - wf.points[b].y;
            double const dz = wf.points[a].z - wf.points[b].z;
            return std::sqrt(dx*dx + dy*dy + dz*dz);
        };
        printf("[DRAG] worldFrustum rearX=%.2f frontX=%.2f zEdge=%.2f frustFraction=%.5g zClipAdj=%d\n",
               d(1, 0), d(5, 4), d(4, 0), vp->GetViewingSpace().getFrustFraction(),
               vp->GetViewingSpace().zClipAdjusted() ? 1 : 0);
    }

    // 锚定 + 分步拖动（参考：锚定后移动即实时旋转——inDynamicUpdate 下
    // onMouseMotion → processPoint(ev, true) → ViewRotate.perform）
    {
        // 双测：run() 前/后各拾一次——区分"Rotate 激活改变拾取"与"始终如此"。
        auto const probe = vp->pickDepthPoint(boxCenter, 15.0);
        printf("[DRAG] pre-click pick at box: source=%d origin=(%.2f,%.2f,%.2f)\n",
               static_cast<int>(probe.source), probe.origin.x, probe.origin.y, probe.origin.z);
    }
    sendClick(A);
    spinCS(300);
    printf("[DRAG] after click: targetCenter=(%.2f,%.2f,%.2f) inDyn=%d\n",
           rotate->targetCenterWorld.x, rotate->targetCenterWorld.y, rotate->targetCenterWorld.z,
           rotate->inDynamicUpdate ? 1 : 0);
    ASSERT_TRUE(rotate->inDynamicUpdate) << "单击应进入锚定动态";
    dqGeom::Vector3d const extentsBefore = view3d->GetExtents();
    dqGeom::Point3d const eyeBefore = view3d->getEyePoint();

    for (int i = 1; i <= 6; ++i) {
        sendMove(QPoint(A.x() + i * 12, A.y() + i * 5));
        spinCS(120);
        vp->RenderFrame();
    }
    spinCS(300);

    dqGeom::Vector3d const extentsAfter = view3d->GetExtents();
    dqGeom::Point3d const eyeAfter = view3d->getEyePoint();
    printf("[DRAG] extents before=(%.2f,%.2f,%.2f) after=(%.2f,%.2f,%.2f)\n",
           extentsBefore.x, extentsBefore.y, extentsBefore.z,
           extentsAfter.x, extentsAfter.y, extentsAfter.z);
    printf("[DRAG] eye before=(%.2f,%.2f,%.2f) after=(%.2f,%.2f,%.2f)\n",
           eyeBefore.x, eyeBefore.y, eyeBefore.z, eyeAfter.x, eyeAfter.y, eyeAfter.z);

    // 视锥健康：extents 有限、非 NaN、且**拖动全程恒定**（DTA 实测 [23.73]×3 不变；
    // 参考 perform 经 SetupFromFrustum override 把后平面尺度收回焦平面，
    // ViewState.ts:1766-1811——未移植时 extents 12→35 爆炸）。
    EXPECT_FALSE(std::isnan(extentsAfter.x) || std::isnan(extentsAfter.y) || std::isnan(extentsAfter.z))
        << "拖动后 extents NaN = 视锥被旋转数学打爆";
    EXPECT_GT(extentsAfter.x, 1e-6);
    EXPECT_NEAR(extentsAfter.x, extentsBefore.x, extentsBefore.x * 0.01)
        << "旋转不改变 extents（参考 perform 往返恒定；爆炸=错乱根因）";
    EXPECT_NEAR(extentsAfter.y, extentsBefore.y, extentsBefore.y * 0.01);

    // 内容存活：FBO 中央区内容像素量不得崩到零
    std::vector<uint8_t> frame1;
    ASSERT_TRUE(vp->ReadFrameForTest(frame1, w, h));
    if (auto const* dumpPath = getenv("DANQING_DUMP_FRAME"))
        dumpFramePpm(frame1, w, h, dumpPath);   // 拖动中帧（内容消失与否亲眼核对）
    size_t const content1 = countContentPx(frame1, w, h);
    printf("[DRAG] after anchored drag content px = %zu (baseline %zu)\n", content1, content0);
    EXPECT_GT(content1, content0 / 4)
        << "锚定拖动后内容消失 = 用户所报错乱（场景不应被清空）";

    // 第二击完成旋转——内容仍须在
    sendClick(A);
    spinCS(400);
    vp->RenderFrame();
    std::vector<uint8_t> frame2;
    ASSERT_TRUE(vp->ReadFrameForTest(frame2, w, h));
    size_t const content2 = countContentPx(frame2, w, h);
    printf("[DRAG] after second click content px = %zu\n", content2);
    EXPECT_GT(content2, content0 / 4) << "第二击完成后内容应仍在";

    view.close();
    spinCS(200);
}
// ---------------------------------------------------------------------------
// 平移（Pan）手势光标回归——中键拖拽：grab/grabbing 手掌（ViewPan.getHandleCursor，
// ViewTool.ts:1109-1111；IdleTool 中键→View.Pan，IdleTool.ts:45-54）。
// Authored: no reference test exists（状态机级回归）。
// ---------------------------------------------------------------------------
TEST(CursorState, MiddleDragPanShowsGrabbingHand)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "CursorStatePan";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinCS(400);

    auto* vp = view.getUeViewport();
    ASSERT_NE(vp, nullptr);
    auto& viewManager = dqApp::Application::Get().GetViewManager();
    auto& admin = dqApp::Application::Get().GetToolAdmin();

    // Selection 默认态（参照基线）
    auto* selTool = admin.GetRegistry().Create("Select");
    ASSERT_NE(selTool, nullptr);
    selTool->onPostInstall();
    ASSERT_EQ(viewManager.cursor(), "default");

    // 中键按下 + 拖拽（超过 startDrag 阈值）
    QMouseEvent down(QEvent::MouseButtonPress, QPointF(500, 350), vp->mapToGlobal(QPoint(500, 350)),
                     Qt::MiddleButton, Qt::MiddleButton, Qt::NoModifier);
    QApplication::sendEvent(vp, &down);
    for (int i = 1; i <= 6; ++i) {
        QMouseEvent mv(QEvent::MouseMove, QPointF(500 + i * 8, 350), vp->mapToGlobal(QPoint(500 + i * 8, 350)),
                       Qt::NoButton, Qt::MiddleButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &mv);
        spinCS(80);
    }
    spinCS(300);

    printf("[PAN] cursor during middle-drag: %s\n", viewManager.cursor().c_str());
    EXPECT_EQ(viewManager.cursor(), "grabbing")
        << "中键平移拖拽中应为 grabbing 手掌（ViewPan inHandleModify 分支）";
    printf("[PAN] viewTool during drag: %s\n", admin.GetViewTool() ? "pan" : "null");

    // 松开 → oneShot 平移工具退出 → 恢复挂起前的 Select 态
    QMouseEvent up(QEvent::MouseButtonRelease, QPointF(548, 350), vp->mapToGlobal(QPoint(548, 350)),
                   Qt::MiddleButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(vp, &up);
    spinCS(400);
    printf("[PAN] cursor after release: %s viewTool=%s\n",
           viewManager.cursor().c_str(), admin.GetViewTool() ? "pan" : "null");
    EXPECT_EQ(viewManager.cursor(), "default")
        << "平移结束应恢复（exitViewTool 恢复 SuspendedToolState 快照）";

    view.close();
    spinCS(200);
}

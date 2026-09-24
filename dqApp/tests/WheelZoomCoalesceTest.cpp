// DanQing dqApp — wheel-zoom per-frame coalescing regression.
//
// Authored: no reference test exists in itwinjs-core/imodel-native ——滚轮合并
// 是浏览器（Chromium）层行为：对齐 rAF、每帧每元素至多派发一个 WheelEvent、
// delta 累积（itwinjs 的 ToolAdmin.tryReplace 只合并 mousemove/touchmove，
// ToolAdmin.ts:795-806——itwinjs 源码不含滚轮合并）。DanQing 的 Qt 桥每个 OS
// 通知都产生一个 QWheelEvent（实测高分辨率滚轮一帧可达 3+ 个事件：
// dy=-72,-11,-11,…），ToolAdmin::coalesceWheelEvents 在每帧 processEvent 前
// 把同视口 Wheel 合并为一个（delta 求和），复现参考运行时契约"每帧每视口
// 至多一次滚轮缩放"。本测试锁定该契约。
#include <QApplication>
#include <QDateTime>
#include <QThread>
#ifdef _WIN32
#include <windows.h>
#endif
#include <gtest/gtest.h>
#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqCommon/Frustum.h>
#include <cmath>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
static LONG WINAPI wheelCrashPrinter(EXCEPTION_POINTERS* ep)
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
            printf("[CRASH] #%u %s+0x%llx\n", i, sym->Name,
                   static_cast<unsigned long long>(disp));
    }
    fflush(stdout);
    return EXCEPTION_CONTINUE_SEARCH;
}
static bool const s_wheelCrashHook = [] {
    if (std::getenv("DANQING_CRASH_STACK"))
        SetUnhandledExceptionFilter(wheelCrashPrinter);
    return true;
}();
#endif

namespace { struct QtEnv4 { QtEnv4() { if (!qApp) { static int argc=1; static char n[]="t"; static char* av[]={n,nullptr}; new QApplication(argc,av);} } }; }
static QtEnv4 s_qt4;

namespace {
struct WheelZoomEnv {
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    dqApp::Viewport* vp = nullptr;

    WheelZoomEnv()
    {
        // idleTool 在 Application::Startup（OnInitialized）创建 —— 滚轮经
        // activeTool(空)/idleTool 派发，未 Startup 则两者皆空、事件被静默丢弃。
        auto& app = dqApp::Application::Get();
        if (!app.isInitialized()) {
            dqApp::Application::Options opts;
            opts.applicationId = "WheelZoomCoalesceTest";
            opts.applicationVersion = "1.0";
            opts.noRender = true;
            EXPECT_TRUE(app.Startup(opts));
        }
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(
            dqGeom::Point3d::From(-1000, -1000, -100), dqGeom::Point3d::From(1000, 1000, 100));
        imodel = dqApp::BlankConnection::create(props);
        view = dqApp::SpatialViewState::CreateBlank(
            imodel.Get(), dqGeom::Point3d::From(0, 0, 0), dqGeom::Vector3d::From(200, 200, 200));
        vp = dqApp::Viewport::Create(nullptr, view);
        vp->resize(1001, 844);
        vp->move(0, 0);
        vp->show();  // 真窗口：RenderFrame 需有效 swapchain（否则早退、动画器不 tick）
        // ViewManager 注册（启动事件循环 —— addEvent 的门槛，ToolAdmin.ts:794-795）。
        dqApp::Application::Get().GetViewManager().AddViewport(vp);
        dqCommon::Frustum f;
        view->AsViewState3d()->GetFrustum(f);
        vp->setupViewFromFrustum(f);
        for (int i = 0; i < 20; ++i) { QCoreApplication::processEvents(); QThread::msleep(10); }
        // 跨测试隔离：静态 ToolAdmin 队列可能残留上一测试的 wheel 事件（携带
        // 已销毁视口指针）——清空（DANQING_TESTING 专用缝，ToolAdmin.h 注释）。
        dqApp::ToolAdmin::clearQueue();
    }
    ~WheelZoomEnv()
    {
        dqApp::Application::Get().GetViewManager().DropViewport(vp);
        delete vp;
        dqApp::ToolAdmin::clearQueue();
    }
};

void queueWheel(dqApp::Viewport* vp, float deltaY)
{
    dqApp::ToolEvent te;
    te.type = dqApp::ToolEventType::Wheel;
    te.vp = vp;
    te.posx = 500.0f;
    te.posy = 400.0f;
    te.wheelDeltaY = deltaY;
    dqApp::ToolAdmin::addEvent(te);
}
}  // namespace

// 一帧内的爆发（高分辨率滚轮：-72,-11,-11,-11,-9 累计 -114）经
// coalesceWheelEvents + 单次 processEvent 后，只应用一次 ×(2/3)。
TEST(WheelZoomCoalesceTest, BurstWithinFrameAppliesSingleZoom)
{
    WheelZoomEnv env;
    ASSERT_TRUE(dqApp::Application::Get().IsEventLoopStarted());
    auto* v3 = env.view->AsViewState3d();
    auto const before = v3->GetExtents();

    for (float dy : {-72.0f, -11.0f, -11.0f, -11.0f, -9.0f})
        queueWheel(env.vp, dy);
    ASSERT_EQ(dqApp::ToolAdmin::pendingEventCount(), 5u);

    auto& toolAdmin = dqApp::Application::Get().GetToolAdmin();
    toolAdmin.coalesceWheelEvents();            // Application::EventLoop 每帧先合并
    ASSERT_EQ(dqApp::ToolAdmin::pendingEventCount(), 1u);  // 5 → 1（delta 求和）
    toolAdmin.processEvent();

    // doZoom 带 500ms 动画：processEvent 后 extents 是动画目标值（×1.5），
    // FrustumAnimator 在后续帧回写插值中间态——断言须在动画收敛后取值。
    auto spin = [&](int spinMs) {
        qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
        while (QDateTime::currentMSecsSinceEpoch() - t0 < spinMs)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    };
    spin(700);  // wheel 动画 500ms + 余量

    auto const after = v3->GetExtents();
    // 单次应用：x/y/z ×(2/3)（wheelDelta 合计 -114 < 0 → doZoom ratio=1/1.5... 参考符号：
    // wheelDelta>0 → 1/1.5；Qt 上滚 dy>0。此处合计 -114 → zoomRatio=1.5 → 放大视域=缩小
    // extents？doZoom: wheelDelta>0 → ratio=1/1.5（zoom in，extents×2/3）。
    // 合计 -114 → ratio=1.5 → extents ×1.5。
    EXPECT_NEAR(after.x / before.x, 1.5, 1e-9);
    EXPECT_NEAR(after.y / before.y, 1.5, 1e-9);
}

// 跨帧：第一帧的合并事件处理后，第二帧新到的事件独立应用（≤每帧一次）。
// doZoom 带 500ms 动画（ToolAdmin.ts:2313-2321）：processEvent 后 extents 是
// 动画目标值，FrustumAnimator 在后续帧回写插值中间态——断言须在动画收敛后取值。
TEST(WheelZoomCoalesceTest, DistinctFramesApplyIndependently)
{
    WheelZoomEnv env;
    auto* v3 = env.view->AsViewState3d();
    auto& toolAdmin = dqApp::Application::Get().GetToolAdmin();

    auto spin = [&](int spinMs) {
        qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
        while (QDateTime::currentMSecsSinceEpoch() - t0 < spinMs)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    };
    auto settle = [&]() { spin(700); };  // wheel 动画 500ms + 余量

    auto const e0 = v3->GetExtents();
    queueWheel(env.vp, -30.0f);
    toolAdmin.coalesceWheelEvents();
    toolAdmin.processEvent();
    settle();
    auto const e1 = v3->GetExtents();

    queueWheel(env.vp, -40.0f);
    toolAdmin.coalesceWheelEvents();
    toolAdmin.processEvent();
    settle();
    auto const e2 = v3->GetExtents();

    EXPECT_NEAR(e1.x / e0.x, 1.5, 1e-9);
    EXPECT_NEAR(e2.x / e1.x, 1.5, 1e-9);  // 两次共 (1.5)^2
}

// ---------------------------------------------------------------------------
// 连续滚轮 + 动画 + 真实帧循环交织（复现"多次缩放后 ACS/Grid/Fit 失效"）。
// Authored: no reference test exists —— 参考对 wheel 连滚 × FrustumAnimator 的
// 组合行为无单测；本测试锁定"视图始终健康（extents 有限且 > 最小限、Fit 可
// 恢复）+ 动画收敛后净缩放等于每帧一次 ×1.5 的复利"。
// ---------------------------------------------------------------------------
TEST(WheelZoomCoalesceTest, RapidZoomWithAnimationKeepsViewHealthy)
{
    WheelZoomEnv env;
    auto* v3 = env.view->AsViewState3d();
    auto& toolAdmin = dqApp::Application::Get().GetToolAdmin();

    // 自驱动模式：Qt 事件泵让 Application 自身的 16ms m_animTimer 驱动
    // EventLoop（合并→派发→RenderLoop），与真实应用同路径（含
    // RequestNextAnimation 门控）。spinMs = 泵墙钟时长。
    auto spin = [&](int spinMs) {
        qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
        while (QDateTime::currentMSecsSinceEpoch() - t0 < spinMs)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    };

    // 等待动画收敛（500ms wheel 时长 + 余量）。
    auto settle = [&]() { spin(900); };

    auto const eStart = v3->GetExtents();

    // 8 事件 @25ms 节拍（DTA 金标准实测净比 0.498383，build/dta-cdp-zoomburst/
    // 手动 renderFrame 泵）。
    double prev = eStart.x;
    for (int i = 0; i < 8; ++i) {
        queueWheel(env.vp, +120.0f);
        spin(25);
        double const cur = v3->GetExtents().x;
        printf("[ZOOMTRACE] ev%d ext=%.6g perEvent=%.6f\n", i, cur, cur / prev);
        prev = cur;
    }
    settle();
    auto const eMid = v3->GetExtents();
    printf("[ZOOMHEALTH] 8x25ms ratio=%.6f (DTA=0.498383, pure=%.6g) settledFromLast=%.6f\n",
           eMid.x / eStart.x, std::pow(2.0 / 3.0, 8), eMid.x / prev);

    // 快速连滚 zoom-out（爆炸方向）。
    for (int i = 0; i < 8; ++i) {
        queueWheel(env.vp, -120.0f);
        spin(25);
    }
    settle();

    auto const& e = v3->GetExtents();
    printf("[ZOOMHEALTH] extents=(%.6g, %.6g, %.6g) origin=(%.6g,%.6g,%.6g)\n",
           e.x, e.y, e.z, v3->GetOrigin().x, v3->GetOrigin().y, v3->GetOrigin().z);

    // 健康判据 1：extents 有限、为正。
    EXPECT_TRUE(std::isfinite(e.x) && std::isfinite(e.y) && std::isfinite(e.z));
    EXPECT_GT(e.x, 0.0);
    EXPECT_GT(e.y, 0.0);
    EXPECT_GT(e.z, 0.0);

    // 健康判据 2：Fit（LookAtVolume 项目范围）能把视图恢复到项目尺度。
    auto fitRange = env.imodel->GetProjectExtents();
    fitRange.scaleAboutCenterInPlace(1.0001);
    fitRange.ensureMinLengths(1.0);
    v3->LookAtVolume(fitRange, nullptr, nullptr);
    toolAdmin.coalesceWheelEvents();
    toolAdmin.processEvent();
    env.vp->RenderFrame();
    auto const& eFit = v3->GetExtents();
    printf("[ZOOMHEALTH] after-fit extents=(%.6g, %.6g, %.6g)\n", eFit.x, eFit.y, eFit.z);
    EXPECT_TRUE(std::isfinite(eFit.x));
    EXPECT_NEAR(eFit.x, fitRange.high.x - fitRange.low.x, 1.05 * (fitRange.high.x - fitRange.low.x));
}

// ---------------------------------------------------------------------------
// 深度缩放后 Grid/ACS 装饰仍被收集（复现"多次缩放后 ACS 和 Grid 都看不到了"）。
// Authored: no reference test exists —— 参考对装饰 × 深度缩放组合无单测；本测试
// 锁定装饰收集契约：每次失效级联后重收集的 Decorations 必须包含 Grid（world，
// GraphicType::WorldDecoration）与 ACS（worldOverlay，ViewContext.ts:293）。
// ---------------------------------------------------------------------------
TEST(WheelZoomCoalesceTest, DeepZoomKeepsDecorationsAlive)
{
    WheelZoomEnv env;
    auto* v3 = env.view->AsViewState3d();

    // DTA 运行时状态：grid + acsTriad 显示（CreateBlank 默认两者关——装饰 gating
    // 在 viewFlags，GridDecorator :185 / AcsTriadDecorator :268）。
    dqCommon::ViewFlagsProperties on;
    on.grid = true;
    on.acsTriad = true;
    env.view->GetDisplayStyle().setViewFlags(env.view->getViewFlags().override(on));

    auto spin = [&](int spinMs) {
        qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
        while (QDateTime::currentMSecsSinceEpoch() - t0 < spinMs)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    };

    // 首帧渲染（装饰收集在 RenderFrame Step 12）。
    spin(400);
    EXPECT_FALSE(env.vp->GetDecorations().world.empty()) << "baseline: grid decoration missing";
    EXPECT_FALSE(env.vp->GetDecorations().worldOverlay.empty()) << "baseline: ACS decoration missing";

    // 深度缩放三阶段：in → out → in（模拟用户连续滚轮操作，每格 25ms 节拍带动画）。
    for (int phase = 0; phase < 3; ++phase) {
        for (int i = 0; i < 40; ++i) {
            queueWheel(env.vp, phase == 1 ? -120.0f : 120.0f);
            spin(25);
        }
        spin(800);  // 动画收敛（500ms + 余量）

        auto const& e = v3->GetExtents();
        printf("[DECODIAG] phase=%d ext=(%.6g,%.6g,%.6g) world=%zu overlay=%zu\n",
               phase, e.x, e.y, e.z,
               env.vp->GetDecorations().world.size(),
               env.vp->GetDecorations().worldOverlay.size());
        ASSERT_TRUE(std::isfinite(e.x) && e.x > 0.0) << "phase " << phase;
        ASSERT_FALSE(env.vp->GetDecorations().world.empty())
            << "phase " << phase << ": grid decoration lost after deep zoom";
        ASSERT_FALSE(env.vp->GetDecorations().worldOverlay.empty())
            << "phase " << phase << ": ACS decoration lost after deep zoom";
    }

    // Fit 恢复（LookAtVolume 项目范围）后装饰仍在。
    auto fitRange = env.imodel->GetProjectExtents();
    v3->LookAtVolume(fitRange, nullptr, nullptr);
    spin(100);
    EXPECT_FALSE(env.vp->GetDecorations().world.empty()) << "grid lost after Fit";
    EXPECT_FALSE(env.vp->GetDecorations().worldOverlay.empty()) << "ACS lost after Fit";
}

// ---------------------------------------------------------------------------
// 深度 zoom-in 触底（extentLimits.min=0.001 clamp）后装饰存活+Fit 恢复。
// Authored: no reference test exists —— 参考对 extentLimits clamp × 装饰组合
// 无单测；复现用户报告"不停放大后 ACS 和 Grid 都不见了，点击 Fit 也没用"。
// 根因：DanQing doZoom 直接 SetExtents(extents*factor) 无 adjustViewDelta clamp，
// zoom-in 到 extents<0.001 后退化（网格/ACS 消失、Fit 失效）。修复后 doZoom
// 调 view->adjustViewDelta（Viewport.ts:2217 vp.zoom 正交分支同层）。
// ---------------------------------------------------------------------------
TEST(WheelZoomCoalesceTest, DeepZoomInClampKeepsDecorationsAndFit)
{
    WheelZoomEnv env;
    auto* v3 = env.view->AsViewState3d();

    // grid + acsTriad 显示（同 DeepZoomKeepsDecorationsAlive）。
    dqCommon::ViewFlagsProperties on;
    on.grid = true;
    on.acsTriad = true;
    env.view->GetDisplayStyle().setViewFlags(env.view->getViewFlags().override(on));

    auto spin = [&](int spinMs) {
        qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
        while (QDateTime::currentMSecsSinceEpoch() - t0 < spinMs)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    };

    auto& toolAdmin = dqApp::Application::Get().GetToolAdmin();

    // 首帧渲染（装饰收集）。
    spin(400);
    EXPECT_FALSE(env.vp->GetDecorations().world.empty());

    // 直接 doZoom（慢滚形态——每格间动画收敛，用户实测 DTA 约 29-30 次到底）：
    // 35 格 ×(2/3)，每格 spin(600) 让 500ms 动画完成 → 200×(2/3)^30 ≈ 0.0005，
    // extentLimits.min=0.001 clamp 应触底并挡住后续。
    dqApp::BeWheelEvent ev;
    ev.viewport = env.vp;
    ev.wheelDelta = 120.0;
    ev.rawPoint = dqGeom::Point3d::From(0, 0, 0);
    for (int i = 0; i < 35; ++i) {
        dqApp::WheelEventProcessor::process(ev, false);
        spin(600);  // 动画收敛（500ms + 余量）
    }
    spin(400);
    auto const e0 = v3->GetExtents();
    printf("[CLAMP] after 35 slow doZoom: ext=(%.6g,%.6g,%.6g)\n", e0.x, e0.y, e0.z);
    EXPECT_GE(e0.x, 0.0009) << "clamp min=0.001 not enforced";
    EXPECT_LE(e0.x, 0.002)  << "clamp min=0.001 not reached after 35 slow doZoom";
    EXPECT_FALSE(env.vp->GetDecorations().worldOverlay.empty());

    // 深度 zoom-in：连续滚轮手势（每帧合并 1 次 ×(2/3)，持续 500 帧——
    // 用户"不停放大"的实际形态；动画器打断（cancelOnAbort 推进 6% 后丢弃）
    // 使净缩放 ≈0.94/帧，500 帧净比 ≈0.94^500≈3e-14 → extents 触底
    // extentLimits.min=0.001）。
    for (int frame = 0; frame < 500; ++frame) {
        queueWheel(env.vp, 120.0f);
        toolAdmin.coalesceWheelEvents();
        toolAdmin.processEvent();
        spin(16);  // 1 帧 @60fps
    }
    spin(800);  // 动画收敛

    auto const& e = v3->GetExtents();
    printf("[DEEPIN] ext=(%.6g,%.6g,%.6g) world=%zu overlay=%zu\n",
           e.x, e.y, e.z,
           env.vp->GetDecorations().world.size(),
           env.vp->GetDecorations().worldOverlay.size());

    // 健康判据 1：extents 深度缩小且为正（clamp 链工作——坐标系修复后
    // 500 帧净比 ≈0.985/帧，200→1.17；无 clamp 时原点漂移会导致装饰丢失）。
    EXPECT_TRUE(std::isfinite(e.x) && std::isfinite(e.y) && std::isfinite(e.z));
    EXPECT_GT(e.x, 0.0);
    EXPECT_GT(e.y, 0.0);
    EXPECT_GT(e.z, 0.0);
    EXPECT_LT(e.x, 5.0);   // 确实深度缩过（200 → <5）
    EXPECT_LT(e.y, 5.0);

    // 健康判据 2：装饰仍在（grid + ACS 不消失）。
    EXPECT_FALSE(env.vp->GetDecorations().world.empty()) << "grid lost after deep zoom-in";
    EXPECT_FALSE(env.vp->GetDecorations().worldOverlay.empty()) << "ACS lost after deep zoom-in";

    // 健康判据 3：Fit 恢复（LookAtVolume 项目范围→ extents 回到 2080 量级）。
    auto fitRange = env.imodel->GetProjectExtents();
    v3->LookAtVolume(fitRange, nullptr, nullptr);
    spin(100);
    auto const& eFit = v3->GetExtents();
    printf("[DEEPIN] after-fit ext=(%.6g,%.6g,%.6g)\n", eFit.x, eFit.y, eFit.z);
    EXPECT_TRUE(std::isfinite(eFit.x));
    EXPECT_NEAR(eFit.x, fitRange.high.x - fitRange.low.x, 1.05 * (fitRange.high.x - fitRange.low.x));
    EXPECT_FALSE(env.vp->GetDecorations().world.empty()) << "grid lost after Fit";
    EXPECT_FALSE(env.vp->GetDecorations().worldOverlay.empty()) << "ACS lost after Fit";
}

#ifdef DANQING_TESTING
// ---------------------------------------------------------------------------
// ACS Z 轴圆盘（浅蓝填充椭圆）随深度放大的像素级存活。
// Authored: no reference test exists —— 参考（浏览器）无对应测试；复现用户报告
// "放大约 20 次时 Z 轴浅蓝色大圆填充一点点消失"（2026-09-14）。判据：中心锚定
// 慢滚放大每步后，readPixels 的视口中心环上存在圆盘蓝色像素（b 明显大于 r/g）。
// env 照 GridAppDiag.AcsTriadOnFrameContent：getDefaultView 视图（天空背景，
// 像素判据有参照）+ noRender Startup + 显式 RenderFrame。
// ---------------------------------------------------------------------------
TEST(WheelZoomCoalesceTest, AcsDiscSurvivesDeepZoomPixels)
{
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(-1000, -1000, -100, 1000, 1000, 100);
    auto conn = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
    auto* vp = dqApp::Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);
    vp->resize(1280, 677);  // 真实 app 视口逻辑尺寸（同几何复刻）
    vp->move(0, 0);
    vp->show();
    // 用户消失场景均为【最大化】窗口（t 系列/用户实测皆最大化态复现；浮动窗
    // +OITDUMP 轮不消失）——最大化作为变量引入。
    vp->showMaximized();
    {
        auto& app = dqApp::Application::Get();
        if (!app.isInitialized()) {
            dqApp::Application::Options opts;
            opts.applicationId = "AcsDiscZoom";
            opts.applicationVersion = "1.0";
            opts.noRender = true;
            ASSERT_TRUE(app.Startup(opts));
        }
        app.GetViewManager().AddViewport(vp);
        dqApp::ToolAdmin::clearQueue();
    }
    auto* v3 = view->AsViewState3d();

    auto spin = [&](int spinMs) {
        qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
        while (QDateTime::currentMSecsSinceEpoch() - t0 < spinMs)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    };
    spin(600);

    {
        auto& style = view->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = true;
        p.acsTriad = true;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spin(600);

    // 圆盘判据修正二（2026-09-14 夜，b-g 判别子）：旧判据 b>r+40 && b>g+40 对
    // 【天空色本身】(142,205,255) 恒真（b-g=50）——旧 40-90px 环采样的大多是天空，
    // 计数>0 不代表盘活着（测量全盲；g<185 变体又被网格着色背景 (120,175,220)
    // 污染）。像素剖面标定：盘填充（蓝 alpha≈0.216 叠背景）b-g≈83-124；盘死后
    // 中心亮区 (128,184,229) b-g=45；背景 b-g=35-50。判别子【b-g>60 && b>r+30】。
    // 采样半径 4-14 物理 px（盘半径 ~21 物理，避开中心交点与箭头杆），8 向偏 7.5°。
    // 采样锚点=triad 实际屏幕位置（WorldToView(世界原点)×DPR）——fast-400 阶段的
    // 非中心锚定会让视图漂移、triad 偏离 FBO 中心（固定中心采样在 out 段全 0 的
    // 测量伪象，2026-09-14 实测）。
    auto discBluePixels = [&](uint32_t w, uint32_t h, std::vector<uint8_t> const& px) {
        dqGeom::Point3d const o = vp->WorldToView(dqGeom::Point3d::From(0, 0, 0));
        double const dpr = vp->devicePixelRatioF();
        int const cx = static_cast<int>(o.x * dpr), cy = static_cast<int>(o.y * dpr);
        int blues = 0;
        for (uint32_t dy = 4; dy <= 14; dy += 2) {
            for (int a = 0; a < 8; ++a) {
                double const ang = (a + 0.5) * 3.14159265 / 4;
                int const x = cx + static_cast<int>(dy * std::cos(ang));
                int const y = cy + static_cast<int>(dy * std::sin(ang));  // readPixels 行序与 view 坐标同为 y-up
                if (x < 0 || y < 0 || x >= static_cast<int>(w) || y >= static_cast<int>(h)) continue;
                size_t const idx = (static_cast<size_t>(y) * w + x) * 4;
                uint8_t const r = px[idx], g = px[idx + 1], b = px[idx + 2];
                if (b - g > 60 && b > r + 30) ++blues;
            }
        }
        return blues;
    };

    dqApp::BeWheelEvent ev;
    ev.viewport = vp;
    ev.wheelDelta = 120.0;
    ev.rawPoint = dqGeom::Point3d::From(0, 0, 0);  // 锚定世界原点：圆盘恒在中心

#ifdef _WIN32
    // 屏幕抓取（screen-vs-FBO 对比：FBO 有圆盘而屏幕没有 ⇒ 呈现层断裂）。
    // 测试窗口是桌面上的真窗口（vp->show()）——GDI BitBlt 抓其物理像素区域。
    // 返回 -1 = 窗口不在前台（被遮挡/激活权旁落），本次测量无效——§12.5
    // 真实窗口取证三件套①：无前台守卫的 BitBlt 抓到的可能是别的窗口内容
    // （此前全量运行 stderr 重定向/trace 开销让前台权时序漂移 → screenBlues
    // 假 0 → PRESENTATION SPLIT 误报，2026-09-23 flaky 定案）。
    auto screenBlues = [&]() -> int {
        HWND const self = reinterpret_cast<HWND>(vp->winId());
        if (!self || GetForegroundWindow() != self)
            return -1;
        double const dpr = vp->devicePixelRatioF();
        QPoint const g = vp->mapToGlobal(QPoint(0, 0));
        int const px = static_cast<int>(g.x() * dpr), py = static_cast<int>(g.y() * dpr);
        int const pw = static_cast<int>(vp->width() * dpr), ph = static_cast<int>(vp->height() * dpr);
        HDC screenDC = GetDC(nullptr);
        HDC memDC = CreateCompatibleDC(screenDC);
        HBITMAP bmp = CreateCompatibleBitmap(screenDC, pw, ph);
        HGDIOBJ old = SelectObject(memDC, bmp);
        BitBlt(memDC, 0, 0, pw, ph, screenDC, px, py, SRCCOPY | CAPTUREBLT);
        // 锚点=triad 屏幕位置（view y-up → GDI 顶朝下翻转）。
        dqGeom::Point3d const o = vp->WorldToView(dqGeom::Point3d::From(0, 0, 0));
        int const cx = static_cast<int>(o.x * dpr), cyTop = static_cast<int>(ph - o.y * dpr);
        int blues = 0;
        for (uint32_t dy = 4; dy <= 14; dy += 2) {
            for (int a = 0; a < 8; ++a) {
                double const ang = (a + 0.5) * 3.14159265 / 4;
                int const x = cx + static_cast<int>(dy * std::cos(ang));
                int const y = cyTop - static_cast<int>(dy * std::sin(ang));
                if (x < 0 || y < 0 || x >= pw || y >= ph) continue;
                COLORREF c = GetPixel(memDC, x, y);
                uint8_t r = GetRValue(c), g2 = GetGValue(c), b = GetBValue(c);
                if (b - g2 > 60 && b > r + 30) ++blues;
            }
        }
        SelectObject(memDC, old);
        DeleteObject(bmp);
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return blues;
    };
#endif  // _WIN32

    int firstLostStep = -1;
    // 用户形态复刻：匀速滚轮（~200ms 间隔）——500ms 动画未完成即被下一格打断
    // （cancelOnAbort），非完整收敛。40 格覆盖消失区间（用户 ext≈0.1-0.2 消失）。
    for (int i = 0; i <= 40; ++i) {
        if (i > 0) {
            dqApp::WheelEventProcessor::process(ev, false);
            spin(200);  // 匀速打断式：动画进行中（500ms 未满）
        }
        vp->RenderFrame();  // noRender env：显式绘制一帧
        std::vector<uint8_t> px;
        uint32_t w = 0, h = 0;
        if (!vp->ReadFrameForTest(px, w, h) || px.empty()) {
            printf("[DISC] step %d: readback FAILED\n", i);
            firstLostStep = i;
            break;
        }
        int const blues = discBluePixels(w, h, px);
        auto const& e = v3->GetExtents();
#ifdef _WIN32
        int const sblues = screenBlues();
        if (sblues >= 0) {
            printf("[DISC] step %2d ext=(%.6g,%.6g) fboBlues=%d screenBlues=%d\n", i, e.x, e.y, blues, sblues);
            if (i > 0 && blues > 0 && sblues == 0 && firstLostStep < 0) {
                firstLostStep = i;
                printf("[DISC] ^^ PRESENTATION SPLIT: disc in FBO but not on screen\n");
            }
        } else {
            printf("[DISC] step %2d ext=(%.6g,%.6g) fboBlues=%d screenBlues=SKIPPED(not-foreground)\n", i, e.x, e.y, blues);
        }
#else
        printf("[DISC] step %2d ext=(%.6g,%.6g) fboBlues=%d\n", i, e.x, e.y, blues);
#endif
        if (i > 0 && blues == 0 && firstLostStep < 0)
            firstLostStep = i;
    }
    // 第二阶段：连续快滚（真实手势形态——每帧合并 1 次 ×(2/3)，动画被后续事件
    // 打断 cancelOnAbort）。2000·(2/3)^N 到 min=0.001 需 N≈24 格，快滚净比
    // ≈0.985/帧（DeepZoomInClamp 实测）→ 500 帧深度触底。期间圆盘必须存活。
    auto& toolAdmin = dqApp::Application::Get().GetToolAdmin();
    for (int frame = 0; frame < 400; ++frame) {
        // 锚定真实视口中心（queueWheel 硬编码 (500,400) 在最大化 1560×970 下非中心
        // → fast-400 把 triad 漂出中心采样区——out 段全 0 的测量伪象，2026-09-14）。
        dqApp::ToolEvent te;
        te.type = dqApp::ToolEventType::Wheel;
        te.vp = vp;
        te.posx = static_cast<float>(vp->width() / 2);
        te.posy = static_cast<float>(vp->height() / 2);
        te.wheelDeltaY = 120.0f;
        dqApp::ToolAdmin::addEvent(te);
        toolAdmin.coalesceWheelEvents();
        toolAdmin.processEvent();
        spin(16);
    }
    spin(800);
    vp->RenderFrame();
    {
        std::vector<uint8_t> px;
        uint32_t w = 0, h = 0;
        ASSERT_TRUE(vp->ReadFrameForTest(px, w, h));
        auto const& e = v3->GetExtents();
        int const blues = discBluePixels(w, h, px);
        dqGeom::Point3d const ov = vp->WorldToView(dqGeom::Point3d::From(0, 0, 0));
        printf("[DISC] after fast-400 ext=(%.6g,%.6g) blues=%d originView=(%.0f,%.0f) widget=%dx%d dpr=%.2f fbo=%ux%u\n",
               e.x, e.y, blues, ov.x, ov.y, vp->width(), vp->height(), vp->devicePixelRatioF(), w, h);
        EXPECT_GT(blues, 0) << "ACS disc lost after continuous fast zoom";
    }

    // 第三阶段：zoom-out（wheelDelta<0，ext ×1.5/格）到 MaxWindow。真实 app 实测
    // 消失在 zoom-out 穿越 ext≈5 时复现（mouse_event(-120) 即此方向——t 系列）。
    ev.wheelDelta = -120.0;
    for (int i = 0; i < 45; ++i) {
        dqApp::WheelEventProcessor::process(ev, false);
        spin(200);
        vp->RenderFrame();
        std::vector<uint8_t> px;
        uint32_t w = 0, h = 0;
        if (!vp->ReadFrameForTest(px, w, h) || px.empty()) break;
        int const blues = discBluePixels(w, h, px);
        auto const& e = v3->GetExtents();
        dqGeom::Point3d const ov = vp->WorldToView(dqGeom::Point3d::From(0, 0, 0));
        printf("[DISC] out-step %2d ext=(%.6g,%.6g) fboBlues=%d originView=(%.0f,%.0f)\n",
               i, e.x, e.y, blues, ov.x, ov.y);
        if (blues == 0 && firstLostStep < 0)
            firstLostStep = 1000 + i;  // zoom-out 丢失步号空间（1000+）与 zoom-in 区分
    }

    dqApp::Application::Get().GetViewManager().DropViewport(vp);
    dqApp::ToolAdmin::clearQueue();
    vp->Shutdown();
    delete vp;
    EXPECT_EQ(firstLostStep, -1) << "ACS disc lost at step " << firstLostStep;
}
#endif  // DANQING_TESTING

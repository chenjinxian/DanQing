// Authored: no reference test exists in itwinjs-core for real-window sky-gradient
//           behavior across standard-view rotations。真窗口回归：切换标准视图后
//           天空渐变分布（4 色渐变 + backgroundMapOn 的 ground/nadir→skyColor 替换，
//           glsl/SkySphere.ts:145-180 + SkyBox.ts:139-142 默认色板 + globe-mode 天空
//           四边形，CachedGeometry.ts:597-651 + 椭球深度拟合
//           BackgroundMapGeometry.ts:314-419）。
//
// 行为定案（itwinjs DTA 同源码 blank connection（Exton ECEF, lon=-75.686694,
//           lat=40.065757，globe mode Ellipsoid）逐视图截图与像素采样；本测试用同一
//           location/extents/视口几何（1001×844 CSS px）与同序视图链复现该场景）：
//   Top/Bottom           — 均匀 (142,205,255)（skyColor 浅蓝；upVector≈竖直 →
//                          (projRt,0,projUp)≈0 → globe 分支退化均匀）。
//   Left/Right/Front/Back— 微弱渐变：顶缘 ~1° 仰角 → 天顶混入 ~5%：
//                          顶 (138,201,255) vs 底 (142,205,255)。椭球深度拟合给出
//                          地平线尺度 camDist（数十 km），而非旧平面分支的 ~2.2km
//                          （旧行为天顶混入 ~60% ≈ (89,152,255)——本测试即回归它）。
//   Iso/RightIso         — 渐变：顶 (82,145,255) vs 底 (142,205,255)（天顶混入
//                          ~29%；伪相机 camDist=(zScale-0.5)·depth，zScale=
//                          diag/(2·atan(22.5°))/|delta|，CachedGeometry.ts:619-626）。
// 【基准修正 2026-09-13】Iso/RightIso 旧基准 (99,162,255) 出自上一会话的非规范
// 链态截图。本会话以 CDP 在 DTA 电子版内重开 blank connection、按本测试同序
// （Top→Bottom→Left→Right→Front→Back→Iso→RightIso，每步 1.7 s 静置）截图并在
// canvas 中心列采样（build/dta-cdp-pixels.js），实测 Iso/RightIso 顶=(82,145,255)、
// Front/Back 顶=(138,201,255)、Left/Right 顶=(139,202,255)、Top/Bottom 全屏
// (142,205,255)——与本实现逐步吻合至 ±1。同会话另证：本实现的
// BackgroundMapGeometry.getFrustumIntersectionDepthRange 与 DTA 在同一视锥上
// 数值一致（build/dta-cdp-crossfeed.js：喂入本实现 Iso 视锥 8 角点，DTA 返回
// [-173061.599, -170774.160] vs 本实现 [-173061.598, -170774.159]）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QThread>
#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/StandardView.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqCommon/ViewFlags.h>
#include <cmath>
#include <cstdio>
#include <vector>
namespace { struct QtEnv2 { QtEnv2() { if (!qApp) { static int argc=1; static char n[]="t"; static char* av[]={n,nullptr}; new QApplication(argc,av);} } }; }
static QtEnv2 s_qt2;

namespace {
struct SkySample { int r, g, b; };

// 中心列上 3 采样：GL 底部（屏幕底）、中部、顶部。
void sampleSkyColumn(dqApp::Viewport* vp, SkySample& bottom, SkySample& mid, SkySample& top)
{
    std::vector<uint8_t> frame;
    uint32_t fw = 0, fh = 0;
    vp->RenderFrame();
    if (!vp->ReadFrameForTest(frame, fw, fh)) { bottom = mid = top = {-1, -1, -1}; return; }
    auto at = [&](uint32_t y) {
        uint8_t const* p = &frame[((size_t)y * fw + fw / 2) * 4];
        return SkySample{p[0], p[1], p[2]};
    };
    bottom = at(0);
    mid = at(fh / 2);
    top = at(fh - 1);
}

bool nearColor(SkySample const& s, int r, int g, int b, int tol = 12)
{
    return std::abs(s.r - r) <= tol && std::abs(s.g - g) <= tol && std::abs(s.b - b) <= tol;
}
}  // namespace

TEST(SkyRotateDiag, SkyGradientFollowsStandardRotation)
{
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(-1000,-1000,-100, 1000,1000,100);
    // DTA blank connection 的 ECEF 位置（Surface.ts:185，Exton PA）——
    // globeMode=Ellipsoid 深度拟合的前提（getIsBackgroundMapVisible 要求
    // ecefLocation defined，DisplayStyleState.ts:734-742）。
    props.locationIsCartographic = true;
    props.cartographicLocation = dqCommon::Cartographic::fromDegrees(-75.686694, 40.065757, 0.0);
    auto conn = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
    auto* vp = dqApp::Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);
    // 与 itwinjs DTA 截图窗口同几何（vpRect 1001×844 CSS px，devicePixelRatio 2）——
    // 深度拟合沿链式视锥传递，窗口纵横比必须一致才能逐步对齐。
    vp->resize(1001, 844);
    vp->move(0, 0);
    vp->show();
    {
        auto& app = dqApp::Application::Get();
        if (!app.isInitialized()) {
            dqApp::Application::Options opts;
            opts.applicationId = "SkyRotateDiag";
            opts.applicationVersion = "1.0";
            opts.noRender = true;
            ASSERT_TRUE(app.Startup(opts));
        }
        app.GetViewManager().AddViewport(vp);
    }
    for (int i = 0; i < 20; ++i) { QCoreApplication::processEvents(); QThread::msleep(20); }

    auto* v3 = vp->GetView()->AsViewState3d();
    ASSERT_NE(v3, nullptr);

    // StandardViewTool.onPostInstall 同路径（ViewTool.ts:3508-3524，无动画）。
    auto switchTo = [&](dqApp::StandardViewId id) {
        auto const rMatrix = dqApp::StandardView::GetStandardRotation(id);
        dqGeom::Matrix3d inverse;
        ASSERT_TRUE(rMatrix.Inverse(inverse));
        auto const targetMatrix = inverse.MultiplyMatrix(v3->getRotation());
        // ViewManip.getDefaultTargetPointWorld (ViewTool.ts:787-795)：
        // target = vp.view.getTargetPoint()（camera off → getEarthFocalPoint，
        // ViewState.ts:1877-1880），再把它沿视锥深度 NPC z 夹取到 [0,1]。
        dqGeom::Point3d targetPoint = v3->GetTargetPoint();
        auto targetPointNpc = vp->WorldToNpc(targetPoint);
        if (targetPointNpc.z < 0.0 || targetPointNpc.z > 1.0) {
            targetPointNpc.z = 0.5;
            targetPoint = vp->NpcToWorld(targetPointNpc);
        }
        auto const rotateTransform = dqGeom::Transform::CreateFixedPointAndMatrix(
            targetPoint, targetMatrix);
        auto newFrustum = vp->getFrustum();
        newFrustum.multiply(rotateTransform);
        v3->SetupFromFrustum(newFrustum);
        vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
        // DTA 每步静置 1.7 s（dta-cdp-views.js sleep(1700)）——渲染循环每帧重建
        // ViewingSpace 并重跑深度拟合（ViewingSpace.ts:271-372 + Viewport.ts 24 步
        // 管线）；显式驱动 RenderFrame 迭代至不动点（Iso 链对迭代盆敏感）。
        for (int i = 0; i < 30; ++i) {
            vp->RenderFrame();
            QCoreApplication::processEvents();
            QThread::msleep(10);
        }
    };

    SkySample bot, mid, top;

    auto dumpView = [&](char const* name) {
        sampleSkyColumn(vp, bot, mid, top);
        auto f = vp->getFrustum();
        auto const& lbr = f.getCorner(0);
        auto const& lbf = f.getCorner(4);
        double const ddx = lbf.x - lbr.x, ddy = lbf.y - lbr.y, ddz = lbf.z - lbr.z;
        double const deltaMag = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz);
        printf("[SKYDIAG] %s bot=(%d,%d,%d) mid=(%d,%d,%d) top=(%d,%d,%d) |delta|=%.1f\n", name,
               bot.r, bot.g, bot.b, mid.r, mid.g, mid.b, top.r, top.g, top.b, deltaMag);
    };

    // DTA 基准容差：同算法同场景应逐通道个位数吻合（渐变由 frustum/伪相机公式
    // 决定；残留差仅来自窗口纵横比（300×200 vs DTA 全屏）对视锥对角线的影响）。
    constexpr int kTol = 8;

    // 视图切换顺序与 itwinjs DTA 截图脚本（build/dta-cdp-views.js 的 VIEWS）逐步
    // 对齐：Top, Bottom, Left, Right, Front, Back, Iso, RightIso——深度拟合沿
    // 链式视锥传递，顺序一致才能逐步对照。

    // Top：DTA 均匀 (142,205,255)。
    switchTo(dqApp::StandardViewId::Top);
    dumpView("Top");
    EXPECT_TRUE(nearColor(bot, 142, 205, 255, kTol)) << "Top bottom uniform skyColor";
    EXPECT_TRUE(nearColor(top, 142, 205, 255, kTol)) << "Top top uniform skyColor";

    // Bottom：DTA 均匀 (142,205,255)（实测值——替代旧注释的"均匀 zenith"推测）。
    switchTo(dqApp::StandardViewId::Bottom);
    dumpView("Bottom");
    EXPECT_TRUE(nearColor(bot, 142, 205, 255, kTol)) << "Bottom bottom uniform skyColor";
    EXPECT_TRUE(nearColor(top, 142, 205, 255, kTol)) << "Bottom top uniform skyColor";

    // Left/Right（DTA 顺序第 3/4 个）：水平视线——底 (142,205,255)、顶 (138,201,255)。
    switchTo(dqApp::StandardViewId::Left);
    dumpView("Left");
    EXPECT_TRUE(nearColor(bot, 142, 205, 255, kTol)) << "Left bottom skyColor";
    EXPECT_TRUE(nearColor(top, 138, 201, 255, kTol)) << "Left top ~5% zenith mix";
    switchTo(dqApp::StandardViewId::Right);
    dumpView("Right");
    EXPECT_TRUE(nearColor(bot, 142, 205, 255, kTol)) << "Right bottom skyColor";
    EXPECT_TRUE(nearColor(top, 138, 201, 255, kTol)) << "Right top ~5% zenith mix";

    // Front/Back：同构水平视线。
    switchTo(dqApp::StandardViewId::Front);
    dumpView("Front");
    EXPECT_TRUE(nearColor(bot, 142, 205, 255, kTol)) << "Front bottom skyColor";
    EXPECT_TRUE(nearColor(top, 138, 201, 255, kTol)) << "Front top ~5% zenith mix";
    switchTo(dqApp::StandardViewId::Back);
    dumpView("Back");
    EXPECT_TRUE(nearColor(bot, 142, 205, 255, kTol)) << "Back bottom skyColor";
    EXPECT_TRUE(nearColor(top, 138, 201, 255, kTol)) << "Back top ~5% zenith mix";

    // Iso：俯视角（DTA 顺序第 7 个，接在 Back 之后）——底 (142,205,255)、
    // 顶 (82,145,255)（天顶混入 ~29%）。基准为 DTA 规范链实测（见文件头
    // 【基准修正】注：旧 (99,162,255) 出自非规范链态）。
    switchTo(dqApp::StandardViewId::Iso);
    dumpView("Iso");
    EXPECT_TRUE(nearColor(bot, 142, 205, 255, kTol)) << "Iso bottom skyColor";
    EXPECT_TRUE(nearColor(top, 82, 145, 255, kTol)) << "Iso top zenith mix (DTA canonical chain measured)";

    // RightIso：同 Iso（DTA 实测顶=(82,145,255)）。
    switchTo(dqApp::StandardViewId::RightIso);
    dumpView("RightIso");
    EXPECT_TRUE(nearColor(bot, 142, 205, 255, kTol)) << "RightIso bottom skyColor";
    EXPECT_TRUE(nearColor(top, 82, 145, 255, kTol)) << "RightIso top zenith mix (DTA canonical chain measured)";

    // 视锥/伪相机证据（delta(rear→front)、zScale、worldEye）——终态(Back)打印一次。
    {
        auto f = vp->getFrustum();
        auto const& lbr = f.getCorner(0);
        auto const& rtr = f.getCorner(3);
        auto const& lbf = f.getCorner(4);
        double const dx = lbf.x - lbr.x, dy = lbf.y - lbr.y, dz = lbf.z - lbr.z;
        double const deltaMag = std::sqrt(dx*dx + dy*dy + dz*dz);
        double const diagonal = std::sqrt((rtr.x-lbr.x)*(rtr.x-lbr.x) + (rtr.y-lbr.y)*(rtr.y-lbr.y) + (rtr.z-lbr.z)*(rtr.z-lbr.z));
        double const focalLength = diagonal / (2.0 * std::atan(22.5 * 3.14159265358979323846 / 180.0));
        double zScale = (deltaMag > 1e-12) ? focalLength / deltaMag : 1.000001;
        if (zScale < 1.000001) zScale = 1.000001;
        printf("[SKYDIAG] final |delta|=%.1f diag=%.1f zScale=%.3f worldEye=(%.0f,%.0f,%.0f)\n",
               deltaMag, diagonal, zScale,
               (lbr.x+rtr.x)*0.5 + dx*zScale, (lbr.y+rtr.y)*0.5 + dy*zScale, (lbr.z+rtr.z)*0.5 + dz*zScale);
    }

    dqApp::Application::Get().GetViewManager().DropViewport(vp);
    delete vp;
}

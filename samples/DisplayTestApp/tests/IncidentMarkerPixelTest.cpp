// IncidentMarkerPixelTest — M-X 主会话集成件：IncidentMarker 真窗像素锁。
//
// Authored: no reference test exists in itwinjs-core for IncidentMarkerDemo
//           pixels（DTA 装饰经 DOM/canvas 层，无像素级测试；行为锚定
//           IncidentMarkerDemo.ts:150-251 + core/frontend/src/Marker.ts
//           drawDecoration :195-223）。本锁验证 B 道无头锁覆盖不到的
//           XE15 GPU 腿：SVG 解码 → MarkerImage 纹理上传 → canvas
//           decoration drawImage → 屏幕像素（活体上屏）。聚类合成/悬停
//           tooltip/点击路由已由无头锁覆盖（MarkerTest 21/21 +
//           IncidentMarkerDemoLock 8/8——含 MouseMoveOpensToolTipWithTitle），
//           真窗悬停注入不重复（§11.9 合成驱动状态与真实流程不等价）。
//
// 判据（§11.11）：量值（帧差显著）+ 位置（500 marker 布满取景域 →
// 四象限带差分均在场）+ 可逆（toggle off 帧差回落基线）。随机源经
// rand01 注入固定种子（XE16 确定性；ctor nullptr = 参考非确定语义）。
// 首跑钉值（2026-10-22 主会话收口，Debug 1000×700 逻辑/dpr2）：diffOn=260,541
// /四象限 39872/45548/36974/37598/diffOff=0——阈值保守下界余量 ~130×/~400×/逐位。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "Gui/IncidentMarkerDemo.h"
#include "View3DInventor.h"

#include <dqApp/Application.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace { struct QtEnvIM { QtEnvIM() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvIM s_qtIM;

namespace {

void spinIM(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 全帧差计数（逐字节容差 8——AA 抖动带；PathDecorationTestLock.cpp:302 同款）。
size_t countFrameDiffIM(std::vector<uint8_t> const& a, std::vector<uint8_t> const& b)
{
    size_t n = 0;
    size_t const count = std::min(a.size(), b.size());
    for (size_t i = 0; i < count; i += 4)
        if (std::abs(int(a[i]) - int(b[i])) > 8 ||
            std::abs(int(a[i + 1]) - int(b[i + 1])) > 8 ||
            std::abs(int(a[i + 2]) - int(b[i + 2])) > 8)
            ++n;
    return n;
}

// 子矩形帧差（WHERE 判据——设备像素坐标，FBO 回读行序已由 ReadFrameForTest
// 归一为 top-down？否——bottom-up（GL 惯例）；子矩形按比例取带，行序对
// "带内差分计数"判据不敏感[带高≫marker 尺寸]，不做逐行翻转）。
size_t countRegionDiffIM(std::vector<uint8_t> const& a, std::vector<uint8_t> const& b,
                         uint32_t w, uint32_t h,
                         double fx0, double fy0, double fx1, double fy1)
{
    uint32_t const x0 = static_cast<uint32_t>(fx0 * w);
    uint32_t const x1 = static_cast<uint32_t>(fx1 * w);
    uint32_t const y0 = static_cast<uint32_t>(fy0 * h);
    uint32_t const y1 = static_cast<uint32_t>(fy1 * h);
    size_t n = 0;
    for (uint32_t y = y0; y < y1 && y < h; ++y)
        for (uint32_t x = x0; x < x1 && x < w; ++x) {
            size_t const px = (static_cast<size_t>(y) * w + x) * 4;
            if (std::abs(int(a[px]) - int(b[px])) > 8 ||
                std::abs(int(a[px + 1]) - int(b[px + 1])) > 8 ||
                std::abs(int(a[px + 2]) - int(b[px + 2])) > 8)
                ++n;
        }
    return n;
}

}  // namespace

// Authored（§5(g)——参考无像素对应物；XE15 GPU 腿活体验证）。
TEST(IncidentMarkerPixel, ToggleOnRastersMarkerIconsAndRestores)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinIM(400);

    // 500 incidents 布点域（IncidentMarkerDemo ctor 形参；DTA 用真实
    // projectExtents——blank connection 下取同量级大域使取景后 marker
    // 铺满视口而非全聚成一簇）。
    dqGeom::Range3d const extents(0.0, 0.0, 0.0, 2000.0, 2000.0, 200.0);
    view.resetBlankConnection(extents, "Incident Marker Demo");
    spinIM(400);

    auto* vp = view.getUeViewport();
    ASSERT_NE(vp, nullptr);
    if (auto* view3d = vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr)
        view3d->LookAtVolume(vp->GetIModel()->GetProjectExtents());
    spinIM(300);

    vp->RenderFrame();
    std::vector<uint8_t> baseline;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(vp->ReadFrameForTest(baseline, w, h));

    // 确定性随机源（XE16——seed 42；start() 的 nullptr 形态留给生产）。
    auto rng = [e = std::mt19937(42u), d = std::uniform_real_distribution<double>(0.0, 1.0)]() mutable {
        return d(e);
    };
    auto deco = std::make_unique<Gui::IncidentMarkerDemo>(*vp, extents, rng);
    dqApp::Application::Get().GetViewManager().AddDecorator(deco.get());

    // 装载门（XE9：同步解码 + singleShot(0) 完成回调——spin 泵事件循环）。
    for (int i = 0; i < 150 && deco->isLoading(); ++i)
        spinIM(100);
    ASSERT_FALSE(deco->isLoading()) << "marker image loading never completed";

    vp->InvalidateDecorations();
    vp->InvalidateScene();
    spinIM(300);
    vp->RenderFrame();
    std::vector<uint8_t> decorated;
    ASSERT_TRUE(vp->ReadFrameForTest(decorated, w, h));

    // 判据 1（量值）：500 marker/聚类上屏 → 帧差显著（阈值保守下界；
    // 首跑 printf 钉值）。
    size_t const diffOn = countFrameDiffIM(baseline, decorated);
    printf("[INCIDENT] diffOn=%zu w=%u h=%u\n", diffOn, w, h);
    EXPECT_GT(diffOn, 2000u) << "incident markers produced no visible change";

    // 判据 2（WHERE）：四象限带（10% 边距内带）差分均在场——marker 布满
    // 取景域的形态断言（单一角落塌缩/全屏单一簇都抓得住）。
    size_t const q[4] = {
        countRegionDiffIM(baseline, decorated, w, h, 0.10, 0.10, 0.50, 0.50),
        countRegionDiffIM(baseline, decorated, w, h, 0.50, 0.10, 0.90, 0.50),
        countRegionDiffIM(baseline, decorated, w, h, 0.10, 0.50, 0.50, 0.90),
        countRegionDiffIM(baseline, decorated, w, h, 0.50, 0.50, 0.90, 0.90),
    };
    printf("[INCIDENT] quadrants=%zu/%zu/%zu/%zu\n", q[0], q[1], q[2], q[3]);
    for (int i = 0; i < 4; ++i)
        EXPECT_GT(q[i], 100u) << "quadrant " << i << " shows no marker content";

    // 拆装饰（stop 语义镜像——DropDecorator + reset）→ 帧回落基线。
    dqApp::Application::Get().GetViewManager().DropDecorator(deco.get());
    deco.reset();
    vp->InvalidateDecorations();
    vp->InvalidateScene();
    spinIM(300);
    vp->RenderFrame();
    std::vector<uint8_t> restored;
    ASSERT_TRUE(vp->ReadFrameForTest(restored, w, h));
    size_t const diffOff = countFrameDiffIM(baseline, restored);
    printf("[INCIDENT] diffOff=%zu (baseline-vs-restored)\n", diffOff);
    EXPECT_LT(diffOff, diffOn / 20 + 200u)
        << "frame did not restore after decorator removal";

    view.close();
}

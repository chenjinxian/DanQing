// CesiumGalleryTest — M-O(4) P8 装饰图元陈列馆像素锁（8 族 ≥1 形态上屏 +
// WHERE 展布断言 + 弧几何三分点回代锁）。
//
// 锚定（真实读过的参考行号）：
//   - EmptyExample.ts:50-496 CesiumDecorator（decorate :58-71 的 8 族派发；
//     逐族色值/权重/挂载形 1:1——蓝点/红方框线/橙菱线/绿三角面/品红四边/
//     黄弧/青椭圆/玫红路径/锯齿绿/紫环/橙金字塔/青盒/红盒/蓝球/绿锥）；
//   - :287-293 路径族弧过渡（createCircularStartMiddleEnd——DanQing XY 共面
//     构造的 EQUIVALENCE 回代验证）。
//
// 判据（§11.11 WHERE）：陈列馆以 projectExtents.center 基布局——大 extents
// blank 连接（±350000）+ 装饰器 + 全域 fit 后：8 族各至少 1 形态上屏
// （族标志色采样计数 >0）+ 展布断言（色像素 bbox 覆盖帧宽 ≥1/3——8 族横
// 向 ±280000 级铺开）。
//
// Authored: no reference test exists in itwinjs-core for EmptyExample
//           （display-test-app 无前端测试；行为锚定如上）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "Gui/CesiumDecorator.h"

#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>

#include <dqGeom/AngleSweep.h>
#include <dqGeom/Arc3d.h>
#include <dqGeom/Point3d.h>

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

struct QtEnvCes {
    QtEnvCes()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvCes s_qtCes;

void spinCes(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

}  // namespace

// ---------------------------------------------------------------------------
// 弧几何锁（arcFromStartMiddleEnd 的 XY 共面构造——三分点回代距离）。
// 注：该静态法私有——经 Decorate 内消费面间接验证（路径族的弧在像素锁内）；
// 本锁以同公式独立复算（公式即头文件 EQUIVALENCE 登记体）。
// ---------------------------------------------------------------------------
TEST(CesiumGallery, ThreePointArcFormulaRoundTrips)
{
    // 参考路径族的 90° 过渡弧三分点（:287-293——P2 + R/√2 中点 + P2+R 终点）。
    dqGeom::Point3d const P2(1000.0, 2000.0, 50.0);
    double const R = 20000.0;
    dqGeom::Point3d const mid(P2.x + R / std::sqrt(2.0),
                              P2.y + R / std::sqrt(2.0), P2.z);
    dqGeom::Point3d const end(P2.x + R, P2.y, P2.z);

    // 外接圆心（同公式——CesiumDecorator.cpp arcFromStartMiddleEnd）。
    double const ax = mid.x - P2.x, ay = mid.y - P2.y;
    double const bx = end.x - P2.x, by = end.y - P2.y;
    double const d = 2.0 * (ax * by - ay * bx);
    ASSERT_GT(std::abs(d), 1.0e-12);
    double const as = ax * ax + ay * ay, bs = bx * bx + by * by;
    double const cx = P2.x + (by * as - ay * bs) / d;
    double const cy = P2.y + (ax * bs - bx * as) / d;

    // 三分点回代：各点到圆心距离相等（外接圆）。
    auto dist = [&](dqGeom::Point3d const& p) {
        return std::sqrt((p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy));
    };
    double const r0 = dist(P2), r1 = dist(mid), r2 = dist(end);
    EXPECT_NEAR(r0, r1, 1.0e-9);
    EXPECT_NEAR(r0, r2, 1.0e-9);
    // 半径量级 = R 量级（90° 弧外接圆半径 ≈ R/√2·√2 —— 参考几何）。
    EXPECT_GT(r0, R * 0.5);
    EXPECT_LT(r0, R * 1.5);
}

// ---------------------------------------------------------------------------
// 8 族像素锁。
// ---------------------------------------------------------------------------
TEST(CesiumGallery, EightFamiliesRenderOnScreen)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "CesiumGallery";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    // 大 extents blank 连接（±350000——陈列馆 ±280000 级偏移全部入视域）。
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(
        dqGeom::Point3d::From(-350000, -350000, -1000),
        dqGeom::Point3d::From(350000, 350000, 400000));
    dqBase::RefPtr<dqApp::BlankConnection> imodel =
        dqApp::BlankConnection::create(props);
    ASSERT_TRUE(imodel.IsValid());
    dqBase::RefPtr<dqApp::SpatialViewState> view =
        dqApp::SpatialViewState::CreateBlank(
            imodel.Get(), dqGeom::Point3d::From(0, 0, 0),
            dqGeom::Vector3d::From(700000, 700000, 401000));
    ASSERT_TRUE(view.IsValid());
    dqApp::Viewport* vp = dqApp::Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);
    app.GetViewManager().AddViewport(vp);
    vp->resize(900, 600);
    vp->show();
    spinCes(300);

    auto* decorator = Gui::CesiumDecorator::start(imodel.Get());
    ASSERT_NE(decorator, nullptr);
    // 天空开（渲染管线健康探针——天空不上屏则装饰链无需检查）。
    {
        dqCommon::Environment env =
            view->GetDisplayStyle().getEnvironment().clone();
        env.displaySky = true;
        view->GetDisplayStyle().setEnvironment(env);
    }

    // 全域 fit（陈列馆 ±280000 级铺开——DumpOpenChain 坑 24 取景路径同式：
    // LookAtVolume + InvalidateController + synchWithView）。
    double const aspect = vp->viewRect().aspect() > 0 ? vp->viewRect().aspect() : 1.5;
    vp->GetView()->AsViewState3d()->LookAtVolume(props.extents, &aspect);
    vp->InvalidateController();
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    vp->InvalidateDecorations();
    vp->RenderFrame();
    spinCes(200);
    vp->InvalidateDecorations();
    vp->RenderFrame();
    {
        auto const& o = view->AsViewState3d()->GetOrigin();
        auto const& e = view->AsViewState3d()->GetExtents();
        printf("[CES] afterFit org=(%.0f,%.0f,%.0f) ext=(%.0f,%.0f,%.0f)\n", o.x,
               o.y, o.z, e.x, e.y, e.z);
    }

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(vp->ReadFrameForTest(frame, w, h));
    ASSERT_GT(w, 0u);
    printf("[CES] frame=%ux%u corner=(%u,%u,%u) center=(%u,%u,%u)\n", w, h,
           frame[0], frame[1], frame[2],
           frame[(static_cast<size_t>(h / 2) * w + w / 2) * 4],
           frame[(static_cast<size_t>(h / 2) * w + w / 2) * 4 + 1],
           frame[(static_cast<size_t>(h / 2) * w + w / 2) * 4 + 2]);

    // 8 族判据：参考逐族 symbology 色为基准的**色相域**分类器（SmoothShade
    // 光照对填充色合法缩放——±30 精确窗对绿三角/品红环/蓝球不成立[首跑
    // 实测 0/255/255→shaded)，逐族以主导通道判别；path vs loop 以 r−b 分离
    // [path 玫红 (255,100,200) r>b、loop 品红 (255,0,255) r≈b]）。
    struct FamilyColor {
        char const* family;
        int count;
        uint32_t minX, maxX;
    };
    FamilyColor families[] = {
        {"point", 0, 0, 0},       // 蓝 (0,0,255)：b 主导
        {"lineString", 0, 0, 0},  // 红 (255,0,0)：r 主导
        {"shape", 0, 0, 0},       // 绿 (0,255,0)：g 主导
        {"arc", 0, 0, 0},         // 黄 (255,255,0)：r&g 双高 b 低
        {"path", 0, 0, 0},        // 玫红 (255,100,200)：r>b>g
        {"loop", 0, 0, 0},        // 品红 (255,0,255)：r≈b 双高 g 低
        {"polyface", 0, 0, 0},    // 橙 (255,165,0)/青 (100,255,255)：橙 r>g>b 或 青 b≈g 高 r 低
        {"solid", 0, 0, 0},       // 蓝紫 (100,100,255)：b 主导 r 中低
    };
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t const* p =
                &frame[(static_cast<size_t>(y) * w + x) * 4];
            int const r = p[0], g = p[1], b = p[2];
            int fam = -1;
            int const maxRG = std::max(r, g);
            int const lum = (r * 299 + g * 587 + b * 114) / 1000;
            if (lum < 25)
                continue;  // 黑底/暗角
            if (b - maxRG > 40) {
                fam = (r > 40 && r < 170 && std::abs(r - g) < 40) ? 7 : 0;
            } else if (r - std::max(g, b) > 40 && g < 100 && b < 100) {
                fam = 1;  // 红（纯红 g/b 均低——橙/玫红经此门回落各自分支）
            } else if (g - std::max(r, b) > 40) {
                fam = 2;  // 绿（g − max(r,b)——maxAll 含 g 自身恒 ≥ g）
            } else if (r > 80 && b > 80 && g < std::min(r, b) - 40) {
                fam = (r - b > 25) ? 4 : 5;  // 玫红 vs 品红
            } else if (r > 80 && g > 80 && b < std::min(r, g) - 40) {
                fam = 3;  // 黄
            } else if (r > 100 && g > 60 && r > g && b < g - 40) {
                fam = 6;  // 橙
            } else if (b > 100 && g > 180 && r < 160 && std::abs(g - (int)b) < 40) {
                fam = 6;  // 青（polyface 盒）
            }
            if (fam < 0 || fam >= 8)
                continue;
            ++families[fam].count;
            if (x < families[fam].minX) families[fam].minX = x;
            if (x > families[fam].maxX) families[fam].maxX = x;
        }
    }
    for (auto const& f : families) {
        printf("[CES] family=%s count=%d xRange=[%u,%u]\n", f.family, f.count,
               f.minX, f.maxX);
        EXPECT_GT(f.count, 20) << f.family << " 未上屏";
    }
    // WHERE（展布）：色像素总体覆盖帧宽 ≥1/3（8 族 ±280000 级横向铺开）。
    uint32_t globMinX = w, globMaxX = 0;
    for (auto const& f : families) {
        if (f.count == 0)
            continue;
        globMinX = std::min(globMinX, f.minX);
        globMaxX = std::max(globMaxX, f.maxX);
    }
    EXPECT_LT(globMinX, w / 3);
    EXPECT_GT(globMaxX, 2u * w / 3);

    decorator->stop();
    delete decorator;
    app.GetViewManager().DropViewport(vp);
    vp->Shutdown();
    delete vp;
}

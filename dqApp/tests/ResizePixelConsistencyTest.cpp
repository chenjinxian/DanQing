// ResizePixelConsistencyTest — resize 前后渲染输出的像素级一致性（§12.8 教训 3：
// 渲染管线资源生命周期改动前先锁住视觉行为，"不崩"测试不防"画错"——resize 三次
// 视觉回归全靠用户肉眼发现的教训）。
//

// Authored: no reference test exists —— itwinjs-core 无 readPixels 级 resize 回归
//（浏览器 canvas 尺寸变化由浏览器自理）；本测试锁 DanQing 侧契约：同一视图在
// resize 重建渲染资源后，Grid 线与 ACS 轴的像素采样点必须仍然命中（非纯背景）。
#include <QApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QScreen>
#include <QThread>
#include <gtest/gtest.h>
#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqCommon/Frustum.h>
#include <cmath>
#include <cstdio>
#include <vector>

namespace { struct QtEnvR2 { QtEnvR2() { if (!qApp) { static int argc=1; static char n[]="t"; static char* av[]={n,nullptr}; new QApplication(argc,av);} } }; }
static QtEnvR2 s_qtR2;

namespace {

void spin(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 帧采样：视野中心带（Grid 主线区）与中心点（ACS 所在）的非背景像素计数。
// 背景=天空渐变的纯色近似；Grid 线=getContrastToBackgroundColor（深色）。
struct FrameProbe {
    std::vector<uint8_t> rgba;
    uint32_t w = 0, h = 0;
    bool capture(dqApp::Viewport* vp)
    {
        if (!vp->ReadFrameForTest(rgba, w, h))
            return false;
        return w > 0 && h > 0 && rgba.size() == static_cast<size_t>(w) * h * 4;
    }
    // 中心水平带里与背景差异显著的像素（Grid 线/ACS 轴）计数。
    // 背景色取帧角落（视野边缘的纯背景区）；Grid 线 = contrastToBackground。
    int countContrastPixelsInCenterBand() const
    {
        uint8_t const* bg = &rgba[(static_cast<size_t>(h - 4) * w + 4) * 4];  // 左下角
        int count = 0;
        uint32_t const y0 = h / 2 - h / 20, y1 = h / 2 + h / 20;
        for (uint32_t y = y0; y < y1; ++y)
            for (uint32_t x = 0; x < w; x += 2) {
                uint8_t const* p = &rgba[(static_cast<size_t>(y) * w + x) * 4];
                int const dr = std::abs(static_cast<int>(p[0]) - bg[0]);
                int const dg = std::abs(static_cast<int>(p[1]) - bg[1]);
                int const db = std::abs(static_cast<int>(p[2]) - bg[2]);
                if (dr + dg + db > 150)  // 与背景的显著差异（线/轴/盘）
                    ++count;
            }
        return count;
    }
};

}  // namespace

TEST(ResizePixelConsistency, GridAndAcsSurviveRenderTargetRebuild)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "ResizePixelConsistency";
        opts.applicationVersion = "1.0";
        opts.noRender = true;
        ASSERT_TRUE(app.Startup(opts));
    }
    // GridAppDiag 同款 harness（已验证可读到合成内容）：ViewList::getDefaultView
    //（= manufactureSpatialView：白背景 + sky on + Iso）。
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(
        dqGeom::Point3d::From(-1000, -1000, -100), dqGeom::Point3d::From(1000, 1000, 100));
    auto imodel = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(imodel.Get()).getDefaultView(imodel.Get());
    auto* vp = dqApp::Viewport::Create(nullptr, view);
    vp->resize(1001, 844);
    vp->show();
    app.GetViewManager().AddViewport(vp);
    spin(400);

    // DTA 运行态 viewFlags：grid + acsTriad 开（GridAppDiag 的 synch 路径）。
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = true;
        p.acsTriad = true;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spin(400);
    vp->RenderFrame();
    dqApp::ToolAdmin::clearQueue();

    // 基线帧：resize 前的 Grid 线密度。
    FrameProbe before;
    ASSERT_TRUE(before.capture(vp));
    int const darkBefore = before.countContrastPixelsInCenterBand();
    printf("[PIXEL] before resize: %ux%u contrastBand=%d\n", before.w, before.h, darkBefore);
    ASSERT_GT(darkBefore, 40) << "基线帧中心带无 Grid 线（测试环境异常）";

    // resize（触发渲染资源重建）+ 收敛。
    vp->resize(1300, 900);
    spin(600);

    // 最大化形态：大幅尺寸 + 连续中间态序列（showMaximized 的 SetWindowPos
    // 会连发多个 resizeEvent——复现"最大化后 Grid 消失"）。
    auto const screen = QGuiApplication::primaryScreen()->availableGeometry();
    int const targetW = screen.width(), targetH = screen.height() - 200;
    for (int i = 1; i <= 8; ++i) {
        vp->resize(1300 + (targetW - 1300) * i / 8,
                   900 + (targetH - 900) * i / 8);
        spin(30);
    }
    vp->resize(targetW, targetH);
    spin(600);

    FrameProbe after;
    ASSERT_TRUE(after.capture(vp));
    int const darkAfter = after.countContrastPixelsInCenterBand();
    printf("[PIXEL] after resize: %ux%u contrastBand=%d\n", after.w, after.h, darkAfter);
    // Grid 在 resize 后仍存在（参考 PlanarGrid 是 GPU 程序化网格，按视锥自适应
    // 线距倍率——视口变大选择更稀的倍率层级，密度比例不守恒是参考行为；
    // 量化 UV 后线 alpha 含 fwidth 归一（PlanarGrid.ts drawGridLine 的
    // (1-line)/max(1,|deriv|)），更宽视口 deriv 更大线更淡——断言存在性
    // + 绝对下限，不锁比例）。
    EXPECT_GT(darkAfter, 300) << "resize 后 Grid 消失（渲染资源重建画错）";

    app.GetViewManager().DropViewport(vp);
    delete vp;
    dqApp::ToolAdmin::clearQueue();
}

// ZoomBlackBoxTest — 放大视口时出现"黑色大方块"的复现与二分（用户报告
// 2026-09-14：不是简单丢失，不停放大视口会发现黑色大方块）。
// Authored: TEMP-DIAG 诊断工具（无对应参考测试）。
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QThread>
#include <gtest/gtest.h>
#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqCommon/Frustum.h>
#include <dqCommon/ViewFlags.h>
#include <cmath>
#include <cstdio>
#include <vector>

namespace { struct QtEnvZ { QtEnvZ() { if (!qApp) { static int argc=1; static char n[]="t"; static char* av[]={n,nullptr}; new QApplication(argc,av);} } }; }
static QtEnvZ s_qtZ;

namespace {

void spin(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 黑色像素计数（RGB 全 < 40 = 黑，区别于天空 (99,143,178) 与白背景）。
int countBlack(std::vector<uint8_t> const& f, uint32_t /*w*/, uint32_t /*h*/)
{
    int count = 0;
    for (size_t i = 0; i < f.size() / 4; ++i) {
        uint8_t const* p = &f[i * 4];
        if (p[0] < 40 && p[1] < 40 && p[2] < 40) ++count;
    }
    return count;
}

void dumpPpm(std::vector<uint8_t> const& f, uint32_t w, uint32_t h, char const* tag)
{
    char path[160];
    std::snprintf(path, sizeof(path), "D:\\Github\\DanQing\\build\\zoombox-%s.ppm", tag);
    if (FILE* dbg = std::fopen(path, "wb")) {
        std::fprintf(dbg, "P6\n%u %u\n255\n", w, h);
        for (size_t i = 0; i < f.size() / 4; ++i)
            std::fwrite(&f[i * 4], 1, 3, dbg);
        std::fclose(dbg);
    }
}

}  // namespace

TEST(ZoomBlackBox, ZoomScanForBlackBox)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "ZoomBlackBox";
        opts.applicationVersion = "1.0";
        opts.noRender = true;
        ASSERT_TRUE(app.Startup(opts));
    }
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(
        dqGeom::Point3d::From(-1000, -1000, -100), dqGeom::Point3d::From(1000, 1000, 100));
    auto imodel = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(imodel.Get()).getDefaultView(imodel.Get());
    auto* vp = dqApp::Viewport::Create(nullptr, view);
    vp->resize(1200, 900);  // 2400x1800 物理（DPR2）——跨 NestedViewportDiag 的 2160 阈值
    vp->show();
    app.GetViewManager().AddViewport(vp);
    spin(400);

    {   // 勾 Grid
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = true;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spin(400);
    dqApp::ToolAdmin::clearQueue();

    // 快速连滚放大（真实操作形态：动画未收敛叠加 + 滚轮位置漂移）。
    dqApp::BeWheelEvent ev;
    ev.viewport = vp;
    ev.wheelDelta = 120.0;
    double angle = 0.0;
    for (int step = 0; step < 120; ++step) {
        // 位置漂移（模拟手抖）。
        angle += 0.7;
        ev.rawPoint = dqGeom::Point3d::From(200.0 * std::cos(angle), 150.0 * std::sin(angle), 0);
        dqApp::WheelEventProcessor::process(ev, false);
        spin(25);  // 快速连滚节拍（动画中断叠加）
        if (step % 6 == 5) {
            vp->RenderFrame();
            std::vector<uint8_t> frame;
            uint32_t w = 0, h = 0;
            if (!vp->ReadFrameForTest(frame, w, h)) { printf("[ZB] step=%d readback FAIL\n", step); break; }
            int const black = countBlack(frame, w, h);
            double const extX = view->AsViewState3d()->GetExtents().x;
            printf("[ZB] step=%3d ext=%.6g black=%d (%.1f%%)\n",
                   step, extX, black, 100.0 * black / (w * h));
            if (black > static_cast<int>(w * h / 100)) {
                char tag[32];
                std::snprintf(tag, sizeof(tag), "fast%d", step);
                dumpPpm(frame, w, h, tag);
                printf("[ZB] BLACK BOX at fast step %d — dumped\n", step);
                break;
            }
        }
    }
    // 收敛后再看一次。
    spin(800);
    vp->RenderFrame();
    {
        std::vector<uint8_t> frame;
        uint32_t w = 0, h = 0;
        if (vp->ReadFrameForTest(frame, w, h)) {
            int const black = countBlack(frame, w, h);
            printf("[ZB] settled ext=%.6g black=%d (%.1f%%)\n",
                   view->AsViewState3d()->GetExtents().x, black, 100.0 * black / (w * h));
            if (black > static_cast<int>(w * h / 100)) dumpPpm(frame, w, h, "settled");
        }
    }

    app.GetViewManager().DropViewport(vp);
    delete vp;
    dqApp::ToolAdmin::clearQueue();
}

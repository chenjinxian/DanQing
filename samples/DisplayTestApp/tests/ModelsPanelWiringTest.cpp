// ModelsPanelWiringTest — M-L(3) Models 选择器演示版的机制锁（provider 通道
// 逐模型可见性 → 场景 → 像素）。
//
// Authored: no reference test exists in itwinjs-core for per-model display
//           toggling on the offline replay path（参考的 changeModelDisplay 在
//           浏览器 RPC 面上；DanQing 的对应物 = DumpOpenTreeProvider 过滤 +
//           InvalidateScene——DumpOpenHelper.h 注释）。像素判据按 §11.11：
//           隔离后内容像素显著少于全集（joeshouse 10 树——单树隔离只画一棵），
//           恢复全集回到基线量级。dump 资产只读。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"

#include "DumpOpenHelper.h"

#include <dqApp/Application.h>
#include <dqApp/Viewport.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {
struct QtEnvMpw {
    QtEnvMpw()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvMpw s_qtMpw;

std::string const kDumpRootMpw = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";

void spinMpw(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 背景色 (25,25,25) 之外的像素数（JoesHouseColor/Edge 同款背景）。
long contentPixels(std::vector<uint8_t> const& f, uint32_t w, uint32_t h)
{
    long n = 0;
    for (size_t i = 0; i + 3 < f.size(); i += 4)
        if (std::abs(int(f[i]) - 25) > 12 || std::abs(int(f[i + 1]) - 25) > 12
            || std::abs(int(f[i + 2]) - 25) > 12)
            ++n;
    (void)w;
    (void)h;
    return n;
}
}  // namespace

TEST(ModelsPanelWiring, ProviderIsolateChangesRenderedContent)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "ModelsPanelWiring";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(900, 640);
    view.show();
    spinMpw(300);

    dta::DumpOpenPackage pkg;
    pkg.imodelRoot = kDumpRootMpw + "/joeshouse-v1";
    pkg.tileRoots = {kDumpRootMpw + "/joeshouse-v1", kDumpRootMpw + "/joeshouse-drill-v1",
                     kDumpRootMpw + "/joeshouse-drill-v2"};
    auto opened = dta::openDumpIModel(view, pkg);
    ASSERT_TRUE(opened.has_value()) << "open chain failed: " << pkg.imodelRoot;

    // 泵至静默（JoesHouseEdge 同款强制选择帧形）。
    size_t lastLogSize = 0;
    int stable = 0;
    bool quiesced = false;
    for (int i = 0; i < 200 && !quiesced; ++i) {
        view.getUeViewport()->InvalidateController();
        spinMpw(100);
        size_t const logSize = opened->fetcher->requestLog().size();
        if (!opened->fetcher->requestLog().empty() && logSize == lastLogSize
            && opened->fetcher->getActiveCount() == 0) {
            if (++stable >= 6)
                quiesced = true;
        } else {
            stable = 0;
        }
        lastLogSize = logSize;
    }
    ASSERT_TRUE(quiesced) << "saved view never quiesced";

    auto* vp = view.getUeViewport();
    vp->RenderFrame();
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(vp->ReadFrameForTest(frame, w, h));
    long const baseline = contentPixels(frame, w, h);
    printf("[MPW] baseline content pixels=%ld (10 trees)\n", baseline);
    ASSERT_GT(baseline, 10000l) << "joeshouse did not render";

    // 单树隔离（ModelPicker stepToIndex(0) 语义：全部隐藏 + 只启用第 0 个）。
    auto* provider = opened->provider.get();
    ASSERT_EQ(provider->visibleCount(), opened->treeLoadLog.size());
    provider->setAllVisible(false);
    provider->setModelVisible(0, true);
    ASSERT_EQ(provider->visibleCount(), 1u);
    vp->InvalidateScene();
    spinMpw(400);
    vp->RenderFrame();
    spinMpw(200);
    vp->RenderFrame();
    ASSERT_TRUE(vp->ReadFrameForTest(frame, w, h));
    long const isolated = contentPixels(frame, w, h);
    printf("[MPW] isolated(1 tree) content pixels=%ld\n", isolated);

    // 隔离后内容像素显著少于全集（单树只画一棵——10 树房子的其余 9 棵不再进场）。
    EXPECT_LT(isolated, baseline / 2)
        << "isolating one model did not reduce rendered content — provider "
           "filtering not consumed by the scene";

    // 恢复全集（Show All）→ 回到基线量级。
    provider->setAllVisible(true);
    vp->InvalidateScene();
    spinMpw(400);
    vp->RenderFrame();
    spinMpw(200);
    vp->RenderFrame();
    ASSERT_TRUE(vp->ReadFrameForTest(frame, w, h));
    long const restored = contentPixels(frame, w, h);
    printf("[MPW] restored content pixels=%ld\n", restored);
    EXPECT_GT(restored, baseline * 3 / 4)
        << "Show All did not restore the full model set";
}

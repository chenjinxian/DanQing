// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — async open-chain tests (P-1b P-b4)
//
// Ported from: itwinjs-core 的打开链 async 面（Snapshot/Briefcase openFile
//   await RPC 数据面 → views 装载；Viewer.openUi 的完成续体）。
//
// Authored: no reference test exists in <project> for the C++ async-open
//   seam (the reference's openFile coverage rides browser RPC integration);
//   the scenarios pin the E5 EQUIVALENCE face — 异步完成态与同步态终态对账
//   （views/树数）、提交不阻塞主线程（manifest 解析离手——M-I(1) 热点）、
//   完成回调在主线程——with the instance60/joeshouse dump packages the
//   DumpOpenChain suite already locks.
//
// 真窗族规程：本文件随 DisplayTestAppDtaTest 目标注册（主会话收口跑）；
// 代理侧只验证编译。

#include "DumpOpenHelper.h"
#include "DumpDataGate.h"  // P-4：大模型 dump 数据门禁（缺席→GTEST_SKIP）

#include "Gui/View3DInventor.h"

#include <dqApp/Application.h>
#include <dqRender/WorkerProxy.h>
#include <dqRender/tile/TileAdmin.h>

#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {

std::string const kDumpRoot = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";

struct QtEnvAsyncOpen {
    QtEnvAsyncOpen()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvAsyncOpen s_qtAsyncOpen;

void spin(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// Pump the frame seam that drains the worker's completions
// (TileAdmin::process step 0 — the production drain point) until the async
// open callback ran.
bool pumpUntilOpen(std::atomic<bool>& done, int budgetMs = 120000)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (!done.load()
           && QDateTime::currentMSecsSinceEpoch() - t0 < budgetMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        dqRender::TileAdmin::instance().process();
        std::this_thread::yield();
    }
    return done.load();
}

dta::DumpOpenPackage instances60Package()
{
    dta::DumpOpenPackage pkg;
    pkg.imodelRoot = kDumpRoot + "/instances60-imodel-v1";
    pkg.tileRoots = {kDumpRoot + "/instances60-v1", kDumpRoot + "/instances60-drill-v1",
                     kDumpRoot + "/instances60-mag-missing-v1"};
    return pkg;
}

}  // namespace

// E5 对账腿：异步打开完成态与同步态终态对账（views/树数/请求面载荷）。
// Ported from: itwinjs-core openFile await vs sync-shaped consumption of the
// same RPC data faces — the terminal state must not diverge.
// Authored: no reference test (see file head).
TEST(AsyncOpenChain, AsyncOpenReconcilesSyncTerminalState)
{
    // P-4 数据门禁：instances60 族已外置（scripts/data/download_dumps.ps1 回灌）。
    DTA_REQUIRE_DUMP_ROOT(kDumpRoot + "/instances60-imodel-v1");
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "AsyncOpenChain";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    dta::DumpOpenPackage const pkg = instances60Package();

    // Sync face (the pre-P-1b entry — stage1+stage2 inline).
    Gui::View3DInventor syncView(nullptr, nullptr, nullptr);
    syncView.resize(1000, 700);
    syncView.show();
    spin(200);
    auto synced = dta::openDumpIModel(syncView, pkg);
    ASSERT_TRUE(synced.has_value()) << "sync open failed";
    ASSERT_FALSE(synced->treeLoadLog.empty());

    // Async face (fresh view; the fetcher replacement semantics of a second
    // open are the documented "后开者替换前者" face).
    Gui::View3DInventor asyncView(nullptr, nullptr, nullptr);
    asyncView.resize(1000, 700);
    asyncView.show();
    spin(200);

    std::atomic<bool> done{false};
    std::thread::id callbackThreadId;
    std::optional<dta::DumpOpenResult> opened;
    dta::openDumpIModelAsync(asyncView, pkg,
                             [&](std::optional<dta::DumpOpenResult>&& result) {
                                 opened = std::move(result);
                                 callbackThreadId = std::this_thread::get_id();
                                 done = true;
                             });
    ASSERT_TRUE(pumpUntilOpen(done));
    ASSERT_TRUE(opened.has_value()) << "async open failed";

    // 终态对账：树装载序逐项一致（modelSelector 驱动序）+ 视图面非空。
    EXPECT_EQ(opened->treeLoadLog, synced->treeLoadLog);
    EXPECT_EQ(opened->trees.size(), synced->trees.size());
    EXPECT_EQ(opened->provider->entries().size(), synced->provider->entries().size());
    EXPECT_TRUE(opened->viewState.IsValid());
    EXPECT_TRUE(opened->connection.IsValid());
    // 完成回调在主线程（E5 的续体语义面）。
    EXPECT_EQ(callbackThreadId, std::this_thread::get_id());
}

// 主线程阻塞计时锁（宽上界）：joeshouse（58MB manifest——M-I(1) 实测同步
// 打开 2.87s Debug）提交后 submit 立即返回（解析离手），完成经 drain 回来。
// Authored: no reference test (see file head).
TEST(AsyncOpenChain, SubmitDoesNotBlockMainThreadOnManifestParse)
{
    // P-4 数据门禁：joeshouse 族已外置（scripts/data/download_dumps.ps1 回灌）。
    DTA_REQUIRE_DUMP_ROOT(kDumpRoot + "/joeshouse-v1");
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "AsyncOpenChain";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    dta::DumpOpenPackage pkg;
    pkg.imodelRoot = kDumpRoot + "/joeshouse-v1";
    pkg.tileRoots = {kDumpRoot + "/joeshouse-v1", kDumpRoot + "/joeshouse-drill-v1",
                     kDumpRoot + "/joeshouse-drill-v2",
                     kDumpRoot + "/joeshouse-mag-missing-v1"};

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin(200);

    std::atomic<bool> done{false};
    size_t treeCount = 0;
    auto const tSubmit0 = std::chrono::steady_clock::now();
    dta::openDumpIModelAsync(view, pkg,
                             [&](std::optional<dta::DumpOpenResult>&& result) {
                                 if (result.has_value())
                                     treeCount = result->treeLoadLog.size();
                                 done = true;
                             });
    auto const submitMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - tSubmit0)
            .count();
    // 宽上界：submit = 队列投递（解析已离手——同步旧路此处要 2.87s Debug）。
    EXPECT_LT(submitMs, 1000)
        << "openDumpIModelAsync blocked the calling thread for " << submitMs
        << "ms — the manifest parse did not leave the main thread";

    ASSERT_TRUE(pumpUntilOpen(done));
    EXPECT_GT(treeCount, 0u);
}

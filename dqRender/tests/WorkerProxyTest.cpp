// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — WorkerProxy carrier tests (P-1B P-b1)
//
// Ported from: itwinjs-core core/frontend/src/common/WorkerProxy.ts
//   (createWorkerProxy :93-140 — promise-RPC over a web worker: msgId
//   correlation, main-thread onmessage continuation) and core/frontend/src/
//   internal/tile/ImdlParser.ts:49-58 (the defaultParser singleton worker) /
//   :39-47 (the noWorker main-thread branch).
//
// Authored: no reference test exists in <project> for the WorkerProxy carrier
//   itself (WorkerProxy has no dedicated unit test in itwinjs-core — its
//   contract is exercised through ImdlParser/integration); the scenarios here
//   pin the E1/E2/E3 EQUIVALENCE seams of the C++ carrier (thread identity /
//   completion FIFO / no execution before drain / noWorker inline pass-through)
//   with assertion values derived from the reference source's branch structure.

#include "dqRender/WorkerProxy.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace dqRender;

namespace {

// Spin-wait helper with a deadline — the worker is asynchronous; tests must
// wait for job execution WITHOUT draining (the no-execution-before-drain
// contract needs the job to have run while its completion is still queued).
template <typename Pred>
bool waitFor(Pred&& pred, std::chrono::milliseconds budget
                             = std::chrono::milliseconds(10000))
{
    auto const deadline = std::chrono::steady_clock::now() + budget;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred())
            return true;
        std::this_thread::yield();
    }
    return pred();
}

}  // namespace

// E1: the job executes on the worker thread, never on the submitting
// (main) thread — the reference's worker-side execution face.
// Ported from: WorkerProxy.ts:132-137 (worker.postMessage → worker-side run).
// Authored: no reference unit test for the carrier (see file head).
TEST(WorkerProxy, JobsRunOnWorkerThreadNotMainThread)
{
    WorkerProxy proxy(WorkerProxy::Args{});

    std::thread::id jobThreadId;
    std::atomic<bool> ran{false};
    proxy.submit([&ran, &jobThreadId]() {
        jobThreadId = std::this_thread::get_id();
        ran = true;
    }, []() {});

    ASSERT_TRUE(waitFor([&ran] { return ran.load(); }));
    std::thread::id const mainThreadId = std::this_thread::get_id();
    EXPECT_NE(jobThreadId, mainThreadId);
    EXPECT_EQ(jobThreadId, proxy.workerThreadId());
}

// E1: completions resolve on the main thread in submission (FIFO) order —
// the reference's msgId correlation + onmessage ordering
// (WorkerProxy.ts:94/:108-121 — tasks resolved as responses arrive; the
// single-worker task queue preserves submission order).
// Authored: no reference unit test for the carrier (see file head).
TEST(WorkerProxy, CompletionsRunInFifoOrderOnDrain)
{
    WorkerProxy proxy(WorkerProxy::Args{});

    constexpr int kCount = 8;
    std::vector<int> order;
    std::vector<std::thread::id> completionThreads;
    std::atomic<int> ran{0};
    std::atomic<int> jobsRan{0};
    for (int i = 0; i < kCount; ++i) {
        proxy.submit([&jobsRan]() { ++jobsRan; },
                     [&order, &completionThreads, &ran, i]() {
                         order.push_back(i);
                         completionThreads.push_back(std::this_thread::get_id());
                         ++ran;
                     });
    }

    // Wait for every job to EXECUTE (completion still queued) so the FIFO
    // assertion covers all eight, not whichever subset raced in.
    ASSERT_TRUE(waitFor([&jobsRan] { return jobsRan.load() == kCount; }));

    EXPECT_EQ(ran.load(), 0);  // drain 前零执行（合同锁）
    proxy.drainCompleted();
    ASSERT_EQ(order.size(), static_cast<size_t>(kCount));
    for (int i = 0; i < kCount; ++i)
        EXPECT_EQ(order[static_cast<size_t>(i)], i);
    // Completions run on the calling (main) thread — the drain seam.
    std::thread::id const mainThreadId = std::this_thread::get_id();
    for (auto const& tid : completionThreads)
        EXPECT_EQ(tid, mainThreadId);
}

// E1: the completion never runs before drainCompleted — the completion is the
// main-thread continuation (the reference's promise resolution), queued until
// the main thread pumps it.
// Authored: no reference unit test for the carrier (see file head).
TEST(WorkerProxy, CompletionDoesNotRunBeforeDrain)
{
    WorkerProxy proxy(WorkerProxy::Args{});

    std::atomic<bool> jobRan{false};
    int completions = 0;
    proxy.submit([&jobRan]() { jobRan = true; }, [&completions]() { ++completions; });

    // The job may take a moment to be picked up; once it HAS run, the
    // completion must still be pending.
    ASSERT_TRUE(waitFor([&jobRan] { return jobRan.load(); }));
    EXPECT_EQ(completions, 0);
    EXPECT_EQ(proxy.pendingCompletionCount(), 1u);

    proxy.drainCompleted();
    EXPECT_EQ(completions, 1);
    EXPECT_EQ(proxy.pendingCompletionCount(), 0u);
}

// E3: the noWorker branch passes through on the calling thread — job and
// completion both run inline inside submit, no drain needed.
// Ported from: ImdlParser.ts:39-47 (acquireImdlParser's noWorker branch —
// `parse: async (options) => parseImdlDocument(...)` runs on the main thread).
// Authored: no reference unit test for the carrier (see file head).
TEST(WorkerProxy, NoWorkerRunsJobAndCompletionInlineOnCallingThread)
{
    WorkerProxy proxy(WorkerProxy::Args{/*noWorker=*/true});
    ASSERT_TRUE(proxy.isNoWorker());

    std::thread::id jobThreadId;
    std::thread::id completionThreadId;
    bool completionRan = false;
    proxy.submit([&jobThreadId]() { jobThreadId = std::this_thread::get_id(); },
                 [&completionThreadId, &completionRan]() {
                     completionThreadId = std::this_thread::get_id();
                     completionRan = true;
                 });

    std::thread::id const mainThreadId = std::this_thread::get_id();
    // Inline: both ran during submit, on the calling thread, no drain.
    EXPECT_EQ(jobThreadId, mainThreadId);
    EXPECT_EQ(completionThreadId, mainThreadId);
    EXPECT_TRUE(completionRan);
    EXPECT_EQ(proxy.pendingCompletionCount(), 0u);
    proxy.drainCompleted();  // no-op — nothing queued
    EXPECT_EQ(completionThreadId, mainThreadId);
}

// Ported from: WorkerProxy.ts:82-87 (terminate/isTerminated).
// Authored: no reference unit test for the carrier (see file head).
TEST(WorkerProxy, TerminateMarksTerminatedAndStopsSubmission)
{
    WorkerProxy proxy(WorkerProxy::Args{});
    ASSERT_FALSE(proxy.isTerminated());

    proxy.terminate();
    EXPECT_TRUE(proxy.isTerminated());

    // Post-terminate submission is a no-op (the reference's worker.postMessage
    // throws after terminate — WorkerProxy.ts:135; the C++ carrier refuses
    // silently with a 0 handle).
    std::atomic<bool> ran{false};
    uint64_t const handle = proxy.submit([&ran]() { ran = true; }, []() {});
    EXPECT_EQ(handle, 0u);
    proxy.drainCompleted();
    EXPECT_FALSE(ran.load());
}

// E2: the default parser's singleton — one shared worker thread
// (ImdlParser.ts:49-58: `if (!defaultParser) { const worker = createWorkerProxy
// (...); defaultParser = {...} }`).
// Authored: no reference unit test for the carrier (see file head).
TEST(WorkerProxy, SingletonIsOneSharedInstance)
{
    WorkerProxy& a = WorkerProxy::instance();
    WorkerProxy& b = WorkerProxy::instance();
    EXPECT_EQ(&a, &b);
}

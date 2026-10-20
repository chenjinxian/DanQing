// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — async job carrier over a single worker thread (P-1B P-b1)
// Ported from: itwinjs-core core/frontend/src/common/WorkerProxy.ts
//              (createWorkerProxy :93-140 — promise-RPC over a web worker;
//              terminate/isTerminated :82-87)
//              core/frontend/src/internal/tile/ImdlParser.ts:39-58
//              (the noWorker main-thread branch :39-47 + the defaultParser
//              singleton worker :49-58)
#pragma once

#include "Export.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// WorkerProxy — asynchronous execution carrier over ONE persistent worker
// thread (the DanQing stand-in for a web worker; dqRender is Qt-free §8.2, so
// the carrier is std::thread-based).
//
// EQUIVALENCE E1 (§11.10): 参考 = promise-RPC——createWorkerProxy 返回的代理把
// 每个方法调用 postMessage 到 worker（msgId 关联，WorkerProxy.ts:132-137），
// worker 完成后 onmessage 在主线程 resolve/reject 对应 promise（:108-121）；
// transferable 数组第二参零拷贝移交 buffer。C++ 无 promise/structured-clone——
// 等价承载 = 任务队列（worker 线程跑 job）+ 完成队列（drainCompleted 在主线程
// 跑 completion——promise 续体只落主线程的语义合同）；transferable→移动语义
// （bytes 随 job move 进任务——零拷贝移交同面）。验证法 = WorkerProxyTest
// （线程 id 断言 / 完成 FIFO / drain 前零执行）。
//
// EQUIVALENCE E2: 参考 defaultParser 单例 worker（ImdlParser.ts:49-58——lazy 建
// `createWorkerProxy(publicPath + "scripts/parse-imdl-worker.js")`）——
// instance() 单例、单 worker 线程。TODO(P-1B 后续，锚 WorkerProxy.ts): 参考
// WorkerProxy 本身不排池；如未来需要多 worker，按 ParserWithTimeline 的
// per-timeline worker 形态（ImdlParser.ts:61-67）扩。
//
// EQUIVALENCE E3: 参考 noWorker 分支（ImdlParser.ts:39-47——`parse: async
// (options) => parseImdlDocument(...)` 主线程同步解析）——Args.noWorker（或
// env DANQING_NO_WORKER=1 的单例）下 submit 直通调用线程执行（job 与
// completion 都在 submit 调用内完成，无需 drain）。
//
// 线程生命周期（M-P teardown join 死锁 saga 的教训在案——join 必须有策略并
// 成文）：**本载体不 join**。worker 线程持 State 的 shared_ptr（任务队列/
// 完成队列/停止位/线程 id），detach-at-construction——析构只置停止位+唤醒后
// 放手，状态块由线程侧 shared_ptr 最后释放（跑完剩余任务后自退出）。零等待
// = M-P 类 join 死锁（DqEventScope 悬垂退订挂死）结构性不可达；进程退出时
// 未跑完的任务随线程自然终止（job 契约 = 纯 CPU 零 GL 零 DqEvent——
// ImdlParser 解析段）。trade-off 登记：析构后 queued 任务仍会执行（无人消费
// 其 completion——闭包随状态块释放，无泄漏）。分离线程不捕获 this——全部经
// State 的 shared_ptr/weak_ptr 触达。
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT WorkerProxy {
public:
    // Ported from: AcquireImdlParserArgs.noWorker (ImdlParser.ts:30-33 — the
    // timeline field is not ported; see ImdlParser.h).
    struct Args {
        bool noWorker = false;
    };

    explicit WorkerProxy(Args args);
    ~WorkerProxy();

    WorkerProxy(WorkerProxy const&) = delete;
    WorkerProxy& operator=(WorkerProxy const&) = delete;

    // The default-parser singleton (E2). Its mode comes from
    // noWorkerEnvRequested() at first use.
    static WorkerProxy& instance();

    // The env degradation face (P-1B P-b3/P-b4): DANQING_NO_WORKER=1 → 全同步
    // 旧路（参考 noWorker 分支的部署面开关——env 形为 DanQing 约定，参考无
    // 对应物；Authored: test/deploy affordance）。
    static bool noWorkerEnvRequested();

    // Test seam for the noWorker gate (the singleton's mode is frozen at first
    // use; the override lets a test flip the degradation face process-wide).
    // Authored: test affordance — same pattern as TileAdmin::setNowOverride
    // ForTest; no reference counterpart.
    static void setNoWorkerOverrideForTest(std::optional<bool> noWorker);

    using Job = std::function<void()>;
    using Completion = std::function<void()>;

    // Submit a job (runs on the worker thread) with its main-thread completion
    // (runs in drainCompleted). Returns the job handle (the reference's msgId
    // correlation — WorkerProxy.ts:133), or 0 if terminated (the reference's
    // post-terminate postMessage throws — the C++ carrier refuses with a 0
    // handle; see WorkerProxyTest.TerminateMarksTerminatedAndStopsSubmission).
    // Ported from: WorkerProxy.get(operation) (WorkerProxy.ts:132-137).
    uint64_t submit(Job job, Completion onCompleted);

    // The main-thread seam: run the queued completions in FIFO order (the
    // reference's onmessage continuation face). Caller must be the owning
    // (main) thread.
    void drainCompleted();

    // Ported from: WorkerProxy.terminate (WorkerProxy.ts:82-87) — stops
    // accepting submissions; the worker drains its remaining queued jobs and
    // exits (the reference's worker.terminate() aborts immediately; the C++
    // carrier's graceful drain is the shared-state lifetime policy above).
    void terminate();

    // Ported from: WorkerProxy.isTerminated (WorkerProxy.ts:85-87).
    bool isTerminated() const noexcept { return m_terminated.load(); }

    bool isNoWorker() const noexcept
    {
        if (s_noWorkerOverride.has_value())
            return *s_noWorkerOverride;
        return m_noWorker;
    }

    // The worker thread's id (recorded by the thread itself at entry; default
    // until the thread started). Test/diagnostic face.
    std::thread::id workerThreadId() const;

    // Queued-but-not-yet-drained completions (contract-lock face).
    size_t pendingCompletionCount() const;

private:
    struct State;
    // The detached thread's loop — a static member so the private State stays
    // unnameable from namespace-scope code (the thread holds the shared_ptr).
    static void workerLoop(std::shared_ptr<State> state);

    static std::optional<bool> s_noWorkerOverride;  // definition in .cpp

    std::shared_ptr<State> m_state;
    std::thread::id m_ownerThreadId;  // drain/submit 的合同线程（构造者）
    bool const m_noWorker;
    std::atomic<bool> m_terminated{false};
    std::atomic<uint64_t> m_nextJobId{0};
};

END_DQ_RENDER_NAMESPACE

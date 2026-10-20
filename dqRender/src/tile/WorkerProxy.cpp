// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — WorkerProxy implementation (P-1B P-b1)
// Ported from: itwinjs-core core/frontend/src/common/WorkerProxy.ts
//              (createWorkerProxy :93-140 — the promise-RPC machinery; the
//              C++ carrier's EQUIVALENCE registration lives on the header)
#include "dqRender/WorkerProxy.h"

#include <cassert>
#include <cstdlib>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// WorkerProxy::State — the shared block the worker thread owns alongside the
// proxy. Lifetime: the proxy holds one reference, the detached thread another
// — the state outlives the proxy whenever the thread is still draining (the
// no-join teardown policy, header note). The detached thread NEVER touches
// the proxy object — everything goes through State.
// ---------------------------------------------------------------------------
struct WorkerProxy::State {
    struct Task {
        uint64_t id = 0;
        Job job;
        Completion completion;
    };
    struct Completed {
        uint64_t id = 0;
        Completion completion;
    };

    std::mutex mutex;
    std::condition_variable jobCv;
    std::deque<Task> jobs;
    std::deque<Completed> completed;
    std::thread::id workerThreadId;  // written by the thread at entry
    bool threadReady = false;
    bool stopping = false;
};

WorkerProxy::WorkerProxy(Args args)
    : m_state(std::make_shared<State>())
    , m_ownerThreadId(std::this_thread::get_id())
    , m_noWorker(s_noWorkerOverride.has_value() ? *s_noWorkerOverride : args.noWorker)
{
    if (m_noWorker)
        return;  // E3 降级——不孵化线程（参考 noWorker 分支不建 worker，
                 // ImdlParser.ts:39-47）

    // Detach-at-construction: the thread owns a shared_ptr<State> copy and
    // winds down by itself (header note — the no-join teardown policy).
    std::weak_ptr<State> weak = m_state;
    std::thread([weak]() {
        // Record the worker thread id before taking any job (the contract
        // lock reads it once the thread is up).
        if (std::shared_ptr<State> locked = weak.lock()) {
            std::lock_guard<std::mutex> guard(locked->mutex);
            locked->workerThreadId = std::this_thread::get_id();
            locked->threadReady = true;
        }
        if (std::shared_ptr<State> locked = weak.lock())
            WorkerProxy::workerLoop(locked);
    }).detach();
}

void WorkerProxy::workerLoop(std::shared_ptr<State> state)
{
    std::unique_lock<std::mutex> lock(state->mutex);
    for (;;) {
        state->jobCv.wait(lock, [&state] {
            return !state->jobs.empty() || state->stopping;
        });
        if (state->jobs.empty())
            break;  // stopping && drained

        State::Task task = std::move(state->jobs.front());
        state->jobs.pop_front();
        lock.unlock();

        if (task.job)
            task.job();  // worker thread — the reference's worker-side run

        lock.lock();
        state->completed.push_back(
            State::Completed{task.id, std::move(task.completion)});
    }
}

WorkerProxy::~WorkerProxy()
{
    terminate();
    // 放手即析构（no-join 策略——header note）：m_state 释放引用，线程侧
    // shared_ptr 继续持有状态块直至跑完退出。
}

WorkerProxy& WorkerProxy::instance()
{
    // E2: the default parser's singleton (ImdlParser.ts:49-58). The mode is
    // captured at first use from the env degradation face.
    static WorkerProxy sDefault{Args{noWorkerEnvRequested()}};
    return sDefault;
}

bool WorkerProxy::noWorkerEnvRequested()
{
    // Authored: deploy/test affordance — the reference has no env face (the
    // noWorker degradation is an argument, ImdlParser.ts:39). DanQing carries
    // the same semantics on an env switch for the P-1B delivery path.
    char const* env = std::getenv("DANQING_NO_WORKER");
    return env != nullptr && env[0] == '1';
}

std::optional<bool> WorkerProxy::s_noWorkerOverride = std::nullopt;

void WorkerProxy::setNoWorkerOverrideForTest(std::optional<bool> noWorker)
{
    // Authored: test affordance (see the declaration). instance()'s mode was
    // frozen at first use — the override steers isNoWorker() readers instead;
    // a pre-existing worker thread simply idles unused under the override.
    s_noWorkerOverride = noWorker;
}

uint64_t WorkerProxy::submit(Job job, Completion onCompleted)
{
    assert(std::this_thread::get_id() == m_ownerThreadId);
    if (m_terminated.load())
        return 0;  // reference: postMessage after terminate throws
    if (m_noWorker) {
        // E3: 直通调用线程——job 与 completion 都在 submit 内完成（参考
        // noWorker 分支的主线程同步 parse）。
        if (job)
            job();
        if (onCompleted)
            onCompleted();
        return ++m_nextJobId;
    }

    uint64_t const id = ++m_nextJobId;
    {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        m_state->jobs.push_back(
            State::Task{id, std::move(job), std::move(onCompleted)});
    }
    m_state->jobCv.notify_one();
    return id;
}

void WorkerProxy::drainCompleted()
{
    assert(std::this_thread::get_id() == m_ownerThreadId);
    if (m_noWorker)
        return;  // 直通面无排队完成项

    std::deque<State::Completed> ready;
    {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        ready.swap(m_state->completed);
    }
    // FIFO — the msgId-correlated resolution order (WorkerProxy.ts:108-121).
    while (!ready.empty()) {
        State::Completed task = std::move(ready.front());
        ready.pop_front();
        if (task.completion)
            task.completion();
    }
}

void WorkerProxy::terminate()
{
    if (m_terminated.exchange(true))
        return;
    if (m_noWorker || !m_state)
        return;
    {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        m_state->stopping = true;
    }
    m_state->jobCv.notify_all();
}

std::thread::id WorkerProxy::workerThreadId() const
{
    if (m_noWorker || !m_state)
        return std::thread::id{};
    std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->workerThreadId;
}

size_t WorkerProxy::pendingCompletionCount() const
{
    if (m_noWorker || !m_state)
        return 0;
    std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->completed.size();
}

END_DQ_RENDER_NAMESPACE

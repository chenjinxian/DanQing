// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GPU hardware timer queries (GLTimer)
//
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/GLTimer.ts
//
// Records GPU time (independent of CPU) for frame operations as GLTimerResult
// trees. The reference wraps the WebGL EXT_disjoint_timer_query extension;
// the desktop-GL equivalent is the GL_TIME_ELAPSED query object (core since
// GL 3.3 — DanQing targets 4.1 core). Like the reference, only one query may be
// active on the context at a time, so this wrapper keeps an internal stack to
// make nesting work (push/pop with sibling queries, GLTimer.ts:187-214).
//
// EQUIVALENCE（didDisjointEventHappen）：参考的 GPU_DISJOINT_EXT 检查是 WebGL
// 独占概念（浏览器上下文丢失事件）；桌面 WGL 上下文丢失走 DanQing 的 swapchain
// 表面自愈族（[SURFHEAL]/[SC] 重建），与计时查询正交——DanQing 恒返回 false，
// 发散=极端情况下（上下文丢失发生在两帧之间）该帧结果不可用，当前 LRU 式
// 轮询会在结果永远不可用时停在队头（与参考的无限 setTimeout 重试同形）。
#pragma once

#include "dqRender/RenderSystem.h"  // GLTimerResult / GLTimerResultCallback / RenderSystemDebugControl

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Backend adapter for hardware timer queries.
// Ported from: itwinjs-core DisjointTimerExtension (GLTimer.ts:11-38).
// The production adapter is the GL_TIME_ELAPSED query object; tests inject
// a stub. Reference semantics: one active query per context at a time.
class TimerQueryExtension {
public:
    virtual ~TimerQueryExtension() = default;

    // Extension present? (GLTimer.ts:20 isSupported)
    virtual bool isSupported() const { return false; }
    // GPU disjoint event happened? WebGL-only concept — always false on
    // desktop (see the file header note).
    virtual bool didDisjointEventHappen() const { return false; }

    virtual uint32_t createQuery() { return 0; }
    virtual void deleteQuery(uint32_t /*q*/) {}
    virtual void beginQuery(uint32_t /*q*/) {}
    virtual void endQuery() {}
    virtual bool isResultAvailable(uint32_t /*q*/) const { return false; }
    virtual uint64_t getResult(uint32_t /*q*/) const { return 0; }
};

// Record GPU hardware queries to profile independent of CPU.
// Ported from: itwinjs-core GLTimer (GLTimer.ts:61-215).
class GLTimer {
public:
    GLTimer();  // production adapter (GL_TIME_ELAPSED query objects)
    explicit GLTimer(std::unique_ptr<TimerQueryExtension> extension);  // test seam

    GLTimer(GLTimer const&) = delete;
    GLTimer& operator=(GLTimer const&) = delete;

    // GLTimer.ts:78 isSupported — the backend exposes timer queries.
    bool isSupported() const noexcept;

    // Access the RenderSystemDebugControl surface (isGLTimerSupported +
    // resultsCallback) owned by this timer — RenderSystem::debugControl
    // forwards here (System.ts:967-970 equivalent).
    RenderSystemDebugControl& debugControl() noexcept { return m_ctrl; }

    // GLTimer.ts:87-92 beginOperation(label).
    void beginOperation(std::string const& label);
    // GLTimer.ts:94-101 endOperation().
    void endOperation();

    // GLTimer.ts:103-112 beginFrame.
    void beginFrame();
    // GLTimer.ts:114-172 endFrame — the frame's result enters the delivery
    // queue; pending results are polled and delivered in order (the
    // reference's setTimeout-retry equivalent, delivered ≥1 frame late).
    void endFrame();

private:
    // GLTimer.ts:40-45 QueryEntry.
    struct QueryEntry {
        std::string label;
        uint32_t query = 0;                         // query object id
        std::vector<uint32_t> siblingQueries;       // main query split by child queries
        std::vector<std::unique_ptr<QueryEntry>> children;
    };

    // GLTimer.ts:187-201 pushQuery.
    void pushQuery(std::string const& label);
    // GLTimer.ts:203-214 popQuery.
    void popQuery();
    // GLTimer.ts:142-167 processQueryEntry — read + free a result tree
    // (inclusive nanoseconds: parent += children).
    GLTimerResult processQueryEntry(QueryEntry& entry);
    // Poll the front of the pending queue; deliver when the root result is
    // available (called from endFrame).
    void pollPendingResults();
    // GLTimer.ts:174-185 cleanupAfterDisjointEvent.
    void cleanupAfterDisjointEvent(QueryEntry& entry);

    std::unique_ptr<TimerQueryExtension> m_extension;
    RenderSystemDebugControl m_ctrl;   // isGLTimerSupported + resultsCallback
    // 栈持有非拥有裸指针；所有权在父的 children（unique_ptr）与 m_frameRoot。
    std::vector<QueryEntry*> m_queryStack;
    std::unique_ptr<QueryEntry> m_frameRoot;   // 当前帧根节点（beginFrame 所建）
    // Completed frames awaiting result availability (delivery order).
    std::deque<std::unique_ptr<QueryEntry>> m_pendingResults;
};

END_DQ_RENDER_NAMESPACE

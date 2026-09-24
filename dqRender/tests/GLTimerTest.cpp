// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GLTimer tests (stubbed TimerQueryExtension)
//
// Authored: no reference test exists in itwinjs-core for GLTimer (the class
// is exercised through the display-test-app GpuProfiler widget only). The
// nesting/delivery semantics are ported 1:1 from GLTimer.ts:61-215 and locked
// here against a stubbed query backend.
//
// Covers (2026-09-21 Profile GPU 任务):
//   no-callback / no-support → no-op paths
//   nested operations → sibling queries + children tree, inclusive ns
//   async delivery: results deliver ≥1 endFrame late, in order

#include "render/GLTimer.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace dqRender;

namespace {

// Stub backend mirroring the TimerQueryExtension contract with controllable
// result availability + elapsed times.
class StubTimerExtension : public TimerQueryExtension {
public:
    struct Query {
        uint32_t id = 0;
        bool available = false;
        uint64_t ns = 0;
        bool deleted = false;
    };

    bool supported = true;
    bool disjoint = false;
    uint32_t nextId = 1;
    std::map<uint32_t, Query> queries;

    bool isSupported() const override { return supported; }
    bool didDisjointEventHappen() const override { return disjoint; }

    uint32_t createQuery() override
    {
        uint32_t const id = nextId++;
        queries[id] = Query{ id, false, 0, false };
        return id;
    }
    void deleteQuery(uint32_t q) override { queries[q].deleted = true; }
    void beginQuery(uint32_t) override {}
    void endQuery() override {}
    bool isResultAvailable(uint32_t q) const override
    {
        auto it = queries.find(q);
        return it != queries.end() && it->second.available;
    }
    uint64_t getResult(uint32_t q) const override
    {
        auto it = queries.find(q);
        return it != queries.end() ? it->second.ns : 0;
    }

    // Test driver: complete a query with the given elapsed time.
    void complete(uint32_t q, uint64_t ns)
    {
        queries[q].available = true;
        queries[q].ns = ns;
    }
};

// Find a child result by label.
GLTimerResult const* findChild(GLTimerResult const& parent, std::string const& label)
{
    for (auto const& child : parent.children)
        if (child.label == label)
            return &child;
    return nullptr;
}

}  // namespace

// Authored: 见文件头 — without a callback the whole chain no-ops (zero cost).
TEST(GLTimerTest, NoCallbackRecordsNothing)
{
    auto ext = std::make_unique<StubTimerExtension>();
    auto* stub = ext.get();
    GLTimer timer(std::move(ext));
    EXPECT_TRUE(timer.isSupported());

    timer.beginFrame();
    timer.beginOperation("A");
    timer.endOperation();
    timer.endFrame();
    EXPECT_EQ(stub->queries.size(), 0u);  // no query was even created
}

// Authored: 见文件头 — unsupported backend: no recording, no crash.
TEST(GLTimerTest, NoSupportIsNoop)
{
    auto ext = std::make_unique<StubTimerExtension>();
    auto* stub = ext.get();
    stub->supported = false;
    GLTimer timer(std::move(ext));
    EXPECT_FALSE(timer.isSupported());
    EXPECT_FALSE(timer.debugControl().isGLTimerSupported);

    int calls = 0;
    timer.debugControl().resultsCallback = [&calls](GLTimerResult const&) { ++calls; };
    timer.beginFrame();
    timer.beginOperation("A");
    timer.endOperation();
    timer.endFrame();
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(stub->queries.size(), 0u);
}

// Authored: 见文件头 — the reference's nesting semantics (GLTimer.ts:187-214):
// a nested operation splits the parent query into sibling segments; results
// form a label tree with inclusive nanoseconds (parent = own + siblings +
// children), delivered on a LATER endFrame.
TEST(GLTimerTest, NestedOperationsFormInclusiveTree)
{
    auto ext = std::make_unique<StubTimerExtension>();
    auto* stub = ext.get();
    GLTimer timer(std::move(ext));

    std::vector<GLTimerResult> delivered;
    timer.debugControl().resultsCallback =
        [&delivered](GLTimerResult const& result) { delivered.push_back(result); };

    // Frame 1: Total(q1) → A(q2) → B(q3) → end B (A sibling q4) → end A (Total sibling q5) → endFrame.
    timer.beginFrame();
    timer.beginOperation("A");
    timer.beginOperation("B");
    timer.endOperation();   // pop B
    timer.endOperation();   // pop A
    timer.endFrame();
    EXPECT_TRUE(delivered.empty());  // nothing available yet

    // Complete the queries: Total q1=10, A q2=20, B q3=5, A-sibling q4=8, Total-sibling q5=3.
    stub->complete(1, 10);
    stub->complete(2, 20);
    stub->complete(3, 5);
    stub->complete(4, 8);
    stub->complete(5, 3);

    // Delivered at the NEXT endFrame (≥1 frame late).
    timer.beginFrame();
    timer.endFrame();
    ASSERT_EQ(delivered.size(), 1u);

    auto const& total = delivered[0];
    EXPECT_EQ(total.label, "Total");
    // Total = q1(10) + q5(3) + children(A)
    // A     = q2(20) + q4(8) + children(B:5) = 33
    // Total = 13 + 33 = 46
    EXPECT_EQ(total.nanoseconds, 46u);
    auto const* a = findChild(total, "A");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->nanoseconds, 33u);
    auto const* b = findChild(*a, "B");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->nanoseconds, 5u);
}

// Authored: 见文件头 — pending frames deliver in order (oldest first).
TEST(GLTimerTest, DeliversInOrderAcrossFrames)
{
    auto ext = std::make_unique<StubTimerExtension>();
    auto* stub = ext.get();
    GLTimer timer(std::move(ext));

    std::vector<uint64_t> deliveredNs;
    timer.debugControl().resultsCallback =
        [&deliveredNs](GLTimerResult const& result) { deliveredNs.push_back(result.nanoseconds); };

    // Frame 1 (q1), Frame 2 (q2), both incomplete.
    timer.beginFrame();
    timer.endFrame();
    timer.beginFrame();
    timer.endFrame();
    EXPECT_TRUE(deliveredNs.empty());

    // Complete frame 1 only → next endFrame delivers frame 1, keeps frame 2 pending.
    stub->complete(1, 100);
    timer.beginFrame();
    timer.endFrame();
    ASSERT_EQ(deliveredNs.size(), 1u);
    EXPECT_EQ(deliveredNs[0], 100u);

    // Complete frame 2 → delivered at the following endFrame, after frame 1.
    stub->complete(2, 200);
    timer.beginFrame();
    timer.endFrame();
    ASSERT_EQ(deliveredNs.size(), 2u);
    EXPECT_EQ(deliveredNs[1], 200u);
}

// Authored: 见文件头 — disjoint event drops the frame's results
// (GLTimer.ts:127-131 cleanupAfterDisjointEvent).
TEST(GLTimerTest, DisjointEventDropsFrameResults)
{
    auto ext = std::make_unique<StubTimerExtension>();
    auto* stub = ext.get();
    stub->disjoint = true;
    GLTimer timer(std::move(ext));

    int calls = 0;
    timer.debugControl().resultsCallback = [&calls](GLTimerResult const&) { ++calls; };

    timer.beginFrame();
    timer.endFrame();
    stub->complete(1, 42);
    timer.beginFrame();
    timer.endFrame();
    EXPECT_EQ(calls, 0);  // frame dropped, no delivery
}

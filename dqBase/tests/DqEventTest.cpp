// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/test/BeEvent.test.ts
// dqBase tests — DqEvent / DqEventScope 行为验证（TDD：先写测试）
#include <gtest/gtest.h>

#include <dqBase/DqEvent.h>

#include <vector>

// --- 基础触发 ---

// Authored: no reference test exists in itwinjs-core for this specific test scenario
TEST(DqEventTest, RaiseInvokesListener) {
    dqBase::DqEvent<int> e;
    int received = 0;
    e.AddListener([&](int v) {
        received = v;
    });
    e.Raise(42);
    EXPECT_EQ(received, 42);
}

// Authored: no reference test exists in itwinjs-core for this specific test scenario
TEST(DqEventTest, MultipleListenersAllInvoked) {
    dqBase::DqEvent<> e;
    int count = 0;
    e.AddListener([&] {
        count++;
    });
    e.AddListener([&] {
        count += 10;
    });
    e.Raise();
    EXPECT_EQ(count, 11);
}

// --- 退订 ---

// Authored: no reference test exists in itwinjs-core for this specific test scenario
TEST(DqEventTest, DisconnectTokenRemovesListener) {
    dqBase::DqEvent<int> e;
    int received = 0;
    auto disconnect = e.AddListener([&](int v) {
        received = v;
    });
    e.Raise(1);
    EXPECT_EQ(received, 1);
    disconnect(); // 退订
    e.Raise(2);
    EXPECT_EQ(received, 1); // 未再触发
}

// Authored: no reference test exists in itwinjs-core for this specific test scenario
TEST(DqEventTest, ListenerCountReflectsAddRemove) {
    dqBase::DqEvent<int> e;
    EXPECT_EQ(e.ListenerCount(), 0u);
    auto d1 = e.AddListener([](int) {});
    auto d2 = e.AddListener([](int) {});
    EXPECT_EQ(e.ListenerCount(), 2u);
    d1();
    EXPECT_EQ(e.ListenerCount(), 1u);
    d2();
    EXPECT_EQ(e.ListenerCount(), 0u);
}

// --- DqEventScope RAII 批量退订 ---

// Authored: no reference test exists in itwinjs-core for this specific test scenario
TEST(DqEventScopeTest, DestructorDisconnectsAll) {
    dqBase::DqEvent<int> e;
    int received = 0;
    {
        dqBase::DqEventScope scope;
        scope.add(e.AddListener([&](int v) {
            received += v;
        }));
        scope.add(e.AddListener([&](int v) {
            received += v * 2;
        }));
        e.Raise(10);
        EXPECT_EQ(received, 30); // 10 + 20
    } // scope 析构退订全部
    e.Raise(100);
    EXPECT_EQ(received, 30); // 未再触发
}

// --- 多参数 / 引用传递 ---

// Authored: no reference test exists in itwinjs-core for this specific test scenario
TEST(DqEventTest, ForwardsMultipleArgs) {
    dqBase::DqEvent<int, const std::string&> e;
    int gotInt = 0;
    std::string gotStr;
    e.AddListener([&](int i, const std::string& s) {
        gotInt = i;
        gotStr = s;
    });
    e.Raise(7, std::string("hello"));
    EXPECT_EQ(gotInt, 7);
    EXPECT_EQ(gotStr, "hello");
}

// --- 触发顺序按注册顺序 ---

// Authored: no reference test exists in itwinjs-core for this specific test scenario
TEST(DqEventTest, ListenersInvokedInRegistrationOrder) {
    dqBase::DqEvent<> e;
    std::vector<int> order;
    e.AddListener([&] {
        order.push_back(1);
    });
    e.AddListener([&] {
        order.push_back(2);
    });
    e.AddListener([&] {
        order.push_back(3);
    });
    e.Raise();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeEvent_Test.cpp
//              TEST_F(BeEvents, Once) —— AddOnce 触发一次后自动移除。
TEST(DqEventTest, AddOnceFiresOnlyOnce) {
    dqBase::DqEvent<int> e;
    int persistent = 0;
    int once = 0;
    e.AddListener([&](int v) {
        persistent = v;
    });
    e.AddOnce([&](int v) {
        once = v;
    });
    EXPECT_EQ(e.ListenerCount(), 2u);
    e.Raise(10); // 两者都触发，once 自动移除
    EXPECT_EQ(persistent, 10);
    EXPECT_EQ(once, 10);
    EXPECT_EQ(e.ListenerCount(), 1u); // once 已移除
    e.Raise(20);                      // 仅 persistent 触发
    EXPECT_EQ(persistent, 20);
    EXPECT_EQ(once, 10); // once 不再触发
}

// Ported from: itwinjs-core core/bentley/src/test/BeEvent.test.ts
//              it("clear subscriptions.")
TEST(DqEventTest, ClearRemovesAllListeners) {
    dqBase::DqEvent<int> e;
    int count = 0;
    e.AddListener([&](int) {
        count++;
    });
    e.AddListener([&](int) {
        count++;
    });
    EXPECT_EQ(e.ListenerCount(), 2u);
    e.clear();
    EXPECT_EQ(e.ListenerCount(), 0u);
    e.Raise(1);
    EXPECT_EQ(count, 0); // 无触发
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeEvent_Test.cpp
//              TEST_F(BeEvents, Simple) —— Count() 与 cancel 回调语义。
TEST(DqEventTest, CancelTokenRemovesOnlyOneListener) {
    dqBase::DqEvent<int> e;
    int a = 0, b = 0, c = 0;
    auto da = e.AddListener([&](int v) {
        a = v;
    });
    auto db = e.AddListener([&](int v) {
        b = v;
    });
    auto dc = e.AddListener([&](int v) {
        c = v;
    });
    EXPECT_EQ(e.ListenerCount(), 3u);
    e.Raise(1);
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 1);
    EXPECT_EQ(c, 1);
    db(); // 仅退订 b
    EXPECT_EQ(e.ListenerCount(), 2u);
    e.Raise(2);
    EXPECT_EQ(a, 2);
    EXPECT_EQ(b, 1); // 未变
    EXPECT_EQ(c, 2);
}

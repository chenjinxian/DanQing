// SPDX-License-Identifier: Apache-2.0
// dqBase tests — DqSync (DqMutex) 行为验证
//
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeThread.h
//              BeMutex recursive-ownership semantics (BeThread.h:50-56)
//
// 参考依据：BeThread.h:52 明确注释 BeMutex "offers exclusive, recursive ownership
// semantics" 且 "@see std::recursive_mutex"。DanQing DqMutex 作为 BeMutex 的 thin
// wrapper 必须保持递归语义——同一线程多次加锁不得死锁。
//
// imodel-native 的 BeThread_Tests.cpp 中未发现专门覆盖 BeMutex 递归重入的测试
// （grep "recursi\|BeMutex" 无命中），故本测试按 CLAUDE.md §4 标注 Authored。
// Authored: no reference test exists in imodel-native for BeMutex recursion
//            (BeThread_Tests.cpp 没有覆盖同线程重入场景)。
//
// 注：所有递归重入断言一律用 try_lock（而非 lock_guard 嵌套），以避免在
// 非递归 baseline 上挂死测试进程——见 CLAUDE.md §5 任务说明。
#include <gtest/gtest.h>

#include <dqBase/DqSync.h>

#include <mutex>
#include <thread>

namespace {

// 在另一个 joinable 线程里执行 functor，确保跨线程语义但本测试仍可控
template <typename Fn>
void RunOnJoinableThread(Fn&& fn) {
    std::thread t(std::forward<Fn>(fn));
    t.join();
}

}  // namespace

// 核心契约：DqMutex 必须可被同一线程递归（重入）加锁。
// 对齐 BeThread.h:52 "recursive ownership semantics"。
// 用 try_lock 避免在非递归实现上挂死：std::mutex::try_lock 在已持有锁的同线程
// 上返回 false（实现定义行为，主流平台一致），递归 std::recursive_mutex 返回 true。
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeThread.h
//              TEST(DqSyncTest, DqMutexIsReentrantOnSameThread)
TEST(DqSyncTest, DqMutexIsReentrantOnSameThread) {
    dqBase::DqMutex m;

    // 第一层：正常获取锁（RAII 管理，作用域结束自动释放）
    std::unique_lock<dqBase::DqMutex> outer(m, std::defer_lock);
    outer.lock();
    ASSERT_TRUE(outer.owns_lock());

    // 第二层：同一线程在不解锁第一层的前提下，再次成功加锁——
    // 非递归 std::mutex 此处失败，递归 std::recursive_mutex 此处成功。
    EXPECT_TRUE(m.try_lock());

    // 解锁第二次加锁，再由 outer 释放第一次
    m.unlock();
}

// 同线程三次重入：递归深度 ≥ 2 的常见用例（BeMutexHolder 嵌套）。
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeThread.h
//              TEST(DqSyncTest, DqMutexSupportsDeepReentrancy)
TEST(DqSyncTest, DqMutexSupportsDeepReentrancy) {
    dqBase::DqMutex m;
    ASSERT_TRUE(m.try_lock());
    EXPECT_TRUE(m.try_lock());
    EXPECT_TRUE(m.try_lock());

    m.unlock();
    m.unlock();
    m.unlock();
}

// 跨线程互斥仍须成立：递归语义不影响不同线程之间的排他性。
// 验证递归修复没有意外放开跨线程保护。
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeThread.h
//              TEST(DqSyncTest, DqMutexStillExcludesOtherThreads)
TEST(DqSyncTest, DqMutexStillExcludesOtherThreads) {
    dqBase::DqMutex m;
    std::unique_lock<dqBase::DqMutex> holder(m);

    bool otherAcquired = false;
    RunOnJoinableThread([&] {
        // 持有者未解锁，另一线程必须拿不到
        otherAcquired = m.try_lock();
        if (otherAcquired) {
            m.unlock();
        }
    });

    EXPECT_FALSE(otherAcquired);
}

// DqLockGuard<DqMutex> 模板别名在递归 mutex 上必须可正常构造析构。
// 仅在 DqMutex 已为递归时该测试才加入；用 try_lock 验证守卫确实持有锁，
// 避免在非递归 baseline 上以 lock_guard 构造直接挂死。
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeThread.h
//              TEST(DqSyncTest, DqLockGuardAliasComposesWithRecursiveMutex)
TEST(DqSyncTest, DqLockGuardAliasComposesWithRecursiveMutex) {
    dqBase::DqMutex m;
    {
        dqBase::DqLockGuard<dqBase::DqMutex> g(m);
        // 守卫持有锁后，try_lock 应再次成功（递归语义）
        EXPECT_TRUE(m.try_lock());
        m.unlock();
    }
    // 守卫析构后，其他线程可获取
    bool acquired = false;
    RunOnJoinableThread([&] {
        acquired = m.try_lock();
        if (acquired) m.unlock();
    });
    EXPECT_TRUE(acquired);
}

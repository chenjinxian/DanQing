// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeThread_Tests.cpp
//              imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeThread.h
//              imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeSharedMutex.h
// dqBase tests — BeMutex/BeMutexHolder/BeConditionVariable/BeThreadUtilities/BeSystemMutexHolder 行为验证

#include <gtest/gtest.h>

#include <dqBase/BeThread.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// BeSharedMutex tests (existing — kept)
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeSharedMutex.h contract — exclusive lock
TEST(BeSharedMutexTest, ExclusiveLockPreventsConcurrentAccess) {
    dqBase::BeSharedMutex mtx;
    std::atomic<int> counter{0};

    mtx.lock();
    // 启动一个线程尝试获取锁
    std::thread t([&]() {
        // try_lock 应该失败
        EXPECT_FALSE(mtx.try_lock());
        counter++;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    mtx.unlock();
    t.join();
    EXPECT_EQ(counter.load(), 1);
}

// Ported from: imodel-native BeSharedMutex.h contract — shared lock allows concurrent readers
TEST(BeSharedMutexTest, SharedLockAllowsConcurrentReaders) {
    dqBase::BeSharedMutex mtx;
    std::atomic<int> readerCount{0};

    mtx.lock_shared();
    std::thread t([&]() {
        // 另一个共享锁应该成功
        EXPECT_TRUE(mtx.try_lock_shared());
        readerCount++;
        mtx.unlock_shared();
    });

    t.join();
    mtx.unlock_shared();
    EXPECT_EQ(readerCount.load(), 1);
}

// Ported from: imodel-native BeSharedMutex.h contract — exclusive blocks shared
TEST(BeSharedMutexTest, ExclusiveBlocksShared) {
    dqBase::BeSharedMutex mtx;
    std::atomic<bool> reached{false};

    mtx.lock();
    std::thread t([&]() {
        // try_lock_shared 应该失败
        EXPECT_FALSE(mtx.try_lock_shared());
        reached = true;
    });

    t.join();
    mtx.unlock();
    EXPECT_TRUE(reached.load());
}

// Ported from: imodel-native BeSharedMutex.h contract — BeSharedMutexHolder
TEST(BeSharedMutexTest, BeSharedMutexHolderRAII) {
    dqBase::BeSharedMutex mtx;
    {
        dqBase::BeSharedMutexHolder holder(mtx);
        // 持有共享锁时，独占锁应失败
        EXPECT_FALSE(mtx.try_lock());
    }
    // 释放后独占锁应成功
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

// Ported from: imodel-native BeSharedMutex.h contract — unlock_and_lock_shared
TEST(BeSharedMutexTest, UnlockAndLockShared) {
    dqBase::BeSharedMutex mtx;
    mtx.lock();
    mtx.unlock_and_lock_shared();
    // 现在是共享锁，另一个共享锁应该成功
    EXPECT_TRUE(mtx.try_lock_shared());
    mtx.unlock_shared();
    mtx.unlock_shared();
}

// ---------------------------------------------------------------------------
// BeMutex / BeMutexHolder tests
// Ported from: imodel-native BeThread.h BeMutex (ref:45-123) contract
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeThread.h BeMutex — recursive reentry
// BeMutex "offers exclusive, recursive ownership semantics" (ref:53)
TEST(BeMutexTest, RecursiveReentrySameThreadNoDeadlock) {
    dqBase::BeMutex mtx;
    mtx.lock();
    mtx.lock();  // 同线程重入，不应死锁
    mtx.unlock();
    mtx.unlock();
}

// Ported from: imodel-native BeThread.h BeMutex — Enter()/Leave() aliases (ref:76,80)
TEST(BeMutexTest, EnterLeaveAliasesForLockUnlock) {
    dqBase::BeMutex mtx;
    mtx.Enter();
    mtx.Enter();  // 递归
    mtx.Leave();
    mtx.Leave();
}

// Ported from: imodel-native BeThread.h BeMutexHolder (ref:88-123) RAII
// ref BeMutex has no try_lock; verify holder owns_lock() transitions and same-thread
// recursive reentry (recursive_mutex allows same-thread reentry, NOT cross-thread).
TEST(BeMutexHolderTest, RaiiLocksAndUnlocks) {
    dqBase::BeMutex mtx;
    {
        dqBase::BeMutexHolder holder(mtx);
        EXPECT_TRUE(holder.owns_lock());
        // 同线程重入（recursive_mutex 允许）：嵌套 holder 不死锁
        dqBase::BeMutexHolder inner(mtx);
        EXPECT_TRUE(inner.owns_lock());
    }
    // 离开作用域后锁已释放；新线程应能取到
    std::atomic<bool> got{false};
    std::thread t2([&]() {
        dqBase::BeMutexHolder h(mtx);
        got = h.owns_lock();
    });
    t2.join();
    EXPECT_TRUE(got.load());
}

// Ported from: imodel-native BeThread.h BeMutexHolder — Lock::No defer
TEST(BeMutexHolderTest, DeferLockNo) {
    dqBase::BeMutex mtx;
    dqBase::BeMutexHolder holder(mtx, dqBase::BeMutexHolder::Lock::No);
    EXPECT_FALSE(holder.owns_lock());
    holder.lock();
    EXPECT_TRUE(holder.owns_lock());
}

// Ported from: imodel-native BeThread.h BeMutexHolder — GetMutex
TEST(BeMutexHolderTest, GetMutexReturnsHeldMutex) {
    dqBase::BeMutex mtx;
    dqBase::BeMutexHolder holder(mtx);
    EXPECT_EQ(holder.GetMutex(), &mtx);
}

// ---------------------------------------------------------------------------
// BeConditionVariable tests (ref IConditionVariablePredicate API, ref:129-203)
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeThread.h BeConditionVariable::WaitOnCondition (ref:193-197)
// Uses ConditionVariablePredicate<T> wrapper to satisfy IConditionVariablePredicate.
TEST(BeConditionVariableTest, WaitOnConditionWakesOnNotify) {
    dqBase::BeConditionVariable cv;
    std::atomic<bool> ready{false};

    std::thread producer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        dqBase::BeMutexHolder holder(cv.GetMutex());
        ready = true;
        cv.notify_one();
    });

    // WaitOnCondition enters its own BeMutexHolder internally (ref:194-196)
    dqBase::ConditionVariablePredicate<std::function<bool(dqBase::BeConditionVariable&)>> pred(
        [&](dqBase::BeConditionVariable&) { return ready.load(); });
    bool result = cv.WaitOnCondition(&pred, dqBase::BeConditionVariable::Infinite);
    EXPECT_TRUE(result);
    EXPECT_TRUE(ready.load());
    producer.join();
}

// Ported from: imodel-native BeThread.h BeConditionVariable::ProtectedWaitOnCondition (ref:177)
TEST(BeConditionVariableTest, ProtectedWaitOnConditionTimeoutReturnsFalse) {
    dqBase::BeConditionVariable cv;
    dqBase::BeMutexHolder holder(cv.GetMutex(), dqBase::BeMutexHolder::Lock::Yes);
    dqBase::ConditionVariablePredicate<std::function<bool(dqBase::BeConditionVariable&)>> pred(
        [](dqBase::BeConditionVariable&) { return false; });
    bool result = cv.ProtectedWaitOnCondition(holder, &pred, 30);
    EXPECT_FALSE(result);
}

// Ported from: imodel-native BeThread.h BeConditionVariable::ProtectedWaitOnCondition
// nullptr predicate → Infinite returns true on first wake per ref:192-202
TEST(BeConditionVariableTest, ProtectedWaitOnConditionNullPredicateWakes) {
    dqBase::BeConditionVariable cv;
    std::thread producer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        cv.notify_one();
    });
    dqBase::BeMutexHolder holder(cv.GetMutex());
    bool result = cv.ProtectedWaitOnCondition(holder, nullptr, dqBase::BeConditionVariable::Infinite);
    EXPECT_TRUE(result);
    producer.join();
}

// Ported from: imodel-native BeThread.h BeConditionVariable — InfiniteWait/notify_one (ref:164,200)
TEST(BeConditionVariableTest, InfiniteWaitWakesOnNotifyAll) {
    dqBase::BeConditionVariable cv;
    std::atomic<int> count{0};

    auto waiter = [&]() {
        dqBase::BeMutexHolder holder(cv.GetMutex());
        cv.InfiniteWait(holder);
        count++;
    };

    std::thread t1(waiter);
    std::thread t2(waiter);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    cv.notify_all();
    t1.join();
    t2.join();
    EXPECT_EQ(count.load(), 2);
}

// ---------------------------------------------------------------------------
// BeThreadUtilities tests (ref:219-252)
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeThread.h BeThreadUtilities::GetHardwareConcurrency (ref:248)
TEST(BeThreadUtilitiesTest, GetHardwareConcurrencyReturnsUint32) {
    uint32_t concurrency = dqBase::BeThreadUtilities::GetHardwareConcurrency();
    EXPECT_GT(concurrency, 0u);
}

// Ported from: imodel-native BeThread.h BeThreadUtilities::GetCurrentThreadId (ref:245)
// ref return type is intptr_t (NOT std::thread::id)
TEST(BeThreadUtilitiesTest, GetCurrentThreadIdReturnsIntptr) {
    intptr_t id = dqBase::BeThreadUtilities::GetCurrentThreadId();
    // 不同线程的 id 应不同
    std::atomic<intptr_t> otherId{0};
    std::thread t([&]() { otherId = dqBase::BeThreadUtilities::GetCurrentThreadId(); });
    t.join();
    EXPECT_NE(id, (intptr_t)0);
    EXPECT_NE(otherId.load(), (intptr_t)0);
    EXPECT_NE(id, otherId.load());
}

// Ported from: imodel-native BeThread.h BeThreadUtilities::GetCurrentProcessId (ref:251)
TEST(BeThreadUtilitiesTest, GetCurrentProcessId) {
    uint64_t pid = dqBase::BeThreadUtilities::GetCurrentProcessId();
    EXPECT_GT(pid, 0u);
}

// Ported from: imodel-native BeThread.h BeThreadUtilities::GetDefaultStackSize (ref:226)
TEST(BeThreadUtilitiesTest, GetDefaultStackSizePositive) {
    int sz = dqBase::BeThreadUtilities::GetDefaultStackSize();
    EXPECT_GT(sz, 0);
}

// Ported from: imodel-native BeThread.h BeThreadUtilities::BeSleep(BeDuration) (ref:237)
TEST(BeThreadUtilitiesTest, BeSleepDurationDoesNotCrash) {
    dqBase::BeThreadUtilities::BeSleep(dqBase::BeDuration::FromMilliseconds(1));
}

// Ported from: imodel-native BeThread.h BeThreadUtilities::BeSleep(uint32_t) (ref:242)
TEST(BeThreadUtilitiesTest, BeSleepMillisDoesNotCrash) {
    dqBase::BeThreadUtilities::BeSleep((uint32_t)1);
}

// Ported from: imodel-native BeThread.h BeThreadUtilities::StartNewThread (ref:232)
// 线程入口签名用 THREAD_MAIN_IMPL 宏（ref:206-213 的跨平台形式：
// POSIX = void*(*)(void*)，Win32 = unsigned __stdcall(*)(void*)）
static THREAD_MAIN_IMPL TestThreadStart(void* arg) {
    auto* pCounter = reinterpret_cast<std::atomic<int>*>(arg);
    pCounter->fetch_add(1);
    return 0; // THREAD_MAIN_IMPL：Win32 分支返回 unsigned（ref:206-213）
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeThread_Tests.cpp
//              TEST(BeThreadUtilitiesTest, StartNewThreadRunsFunction)
TEST(BeThreadUtilitiesTest, StartNewThreadRunsFunction) {
    std::atomic<int> counter{0};
    dqBase::DqStatus status = dqBase::BeThreadUtilities::StartNewThread(
        TestThreadStart, &counter, dqBase::BeThreadUtilities::GetDefaultStackSize());
    EXPECT_EQ(status, dqBase::DqStatus::Success);
    // 等待线程完成（StartNewThread 在 ref 中分离线程）
    while (counter.load() == 0)
        std::this_thread::yield();
    EXPECT_EQ(counter.load(), 1);
}

// Ported from: imodel-native BeThread.h BeThreadUtilities::SetCurrentThreadName (ref:223)
TEST(BeThreadUtilitiesTest, SetCurrentThreadNameDoesNotCrash) {
    dqBase::BeThreadUtilities::SetCurrentThreadName("DanQing-test-thread");
}

// ---------------------------------------------------------------------------
// BeSystemMutexHolder tests (ref:259-269)
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeThread.h BeSystemMutexHolder — process-global mutex (ref:259-269)
TEST(BeSystemMutexHolderTest, AcquireAndReleaseSystemMutex) {
    {
        dqBase::BeSystemMutexHolder holder;
        EXPECT_TRUE(holder.owns_lock());
    }
    // 释放后可重新获取
    dqBase::BeSystemMutexHolder holder2;
    EXPECT_TRUE(holder2.owns_lock());
}

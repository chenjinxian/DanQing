// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeThreadLocalStorage.h
//              (no standalone test file; test scenarios derived from header contract)
// dqBase tests — BeThreadLocalStorage 行为验证

#include <gtest/gtest.h>

#include <dqBase/BeThreadLocalStorage.h>

#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// 基础功能测试
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeThreadLocalStorage.h contract — Create/Delete
TEST(BeThreadLocalStorageTest, CreateAndDelete) {
    void* key = dqBase::BeThreadLocalStorage::Create();
    EXPECT_NE(key, nullptr);
    dqBase::BeThreadLocalStorage::Delete(key);
}

// Ported from: imodel-native BeThreadLocalStorage.h contract — SetValue/GetValue by key
TEST(BeThreadLocalStorageTest, SetValueGetValueByKey) {
    void* key = dqBase::BeThreadLocalStorage::Create();
    int value = 42;
    dqBase::BeThreadLocalStorage::SetValue(key, &value);
    EXPECT_EQ(dqBase::BeThreadLocalStorage::GetValue(key), &value);
    dqBase::BeThreadLocalStorage::Delete(key);
}

// Ported from: imodel-native BeThreadLocalStorage.h contract — instance methods
TEST(BeThreadLocalStorageTest, InstanceMethods) {
    dqBase::BeThreadLocalStorage tls;
    int value = 99;
    tls.SetValueAsPointer(&value);
    EXPECT_EQ(tls.GetValueAsPointer(), &value);
}

// Ported from: imodel-native BeThreadLocalStorage.h contract — integer storage
TEST(BeThreadLocalStorageTest, IntegerStorage) {
    dqBase::BeThreadLocalStorage tls;
    tls.SetValueAsInteger(12345);
    EXPECT_EQ(tls.GetValueAsInteger(), 12345);
}

// Ported from: imodel-native BeThreadLocalStorage.h contract — thread isolation
TEST(BeThreadLocalStorageTest, ThreadIsolation) {
    dqBase::BeThreadLocalStorage tls;
    tls.SetValueAsInteger(1);

    std::thread t([&]() {
        // 新线程中应返回 nullptr/0
        EXPECT_EQ(tls.GetValueAsPointer(), nullptr);
        tls.SetValueAsInteger(2);
        EXPECT_EQ(tls.GetValueAsInteger(), 2);
    });
    t.join();

    // 主线程的值不变
    EXPECT_EQ(tls.GetValueAsInteger(), 1);
}

// Ported from: imodel-native BeThreadLocalStorage.h contract — multiple threads
TEST(BeThreadLocalStorageTest, MultipleThreads) {
    dqBase::BeThreadLocalStorage tls;
    constexpr int NUM_THREADS = 8;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&tls, i, &successCount]() {
            tls.SetValueAsInteger(i * 100);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (tls.GetValueAsInteger() == i * 100)
                successCount++;
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(successCount.load(), NUM_THREADS);
}

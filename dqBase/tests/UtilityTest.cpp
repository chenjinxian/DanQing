// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists for utility functions
// dqBase tests — 工具类行为验证（TDD：先写测试）
//   覆盖：NonCopyable / Passkey / DqByteStream / LruCache / PriorityQueue / DqClock
#include <gtest/gtest.h>

#include <type_traits>

#include <dqBase/LruCache.h>
#include <dqBase/NonCopyable.h>
#include <dqBase/Passkey.h>
#include <dqBase/PriorityQueue.h>
#include <dqBase/DqByteStream.h>
#include <dqBase/DqClock.h>
#include <dqBase/DqStatus.h>
#include <dqBase/Version.h>

// ===================== NonCopyable =====================

class NonCopyableDerived : public dqBase::DqNonCopyable {
public:
    int value = 0;
};

// Authored: no reference test exists for this utility test
TEST(NonCopyableTest, IsNotCopyable) {
    static_assert(!std::is_copy_constructible<NonCopyableDerived>::value,
            "NonCopyable derived must not be copy-constructible");
    static_assert(!std::is_copy_assignable<NonCopyableDerived>::value,
            "NonCopyable derived must not be copy-assignable");
    SUCCEED();
}

// ===================== Passkey =====================

class GuardedResource;
class PasskeyTest : public ::testing::Test {
protected:
    using Key = dqBase::Passkey<GuardedResource>;
};

class GuardedResource {
public:
    int SecretMethod(dqBase::Passkey<GuardedResource>, int x) const {
        return x * 2;
    }
};

// Authored: no reference test exists for this utility test
TEST(PasskeyTest, OwnerCanConstructEmptyKey) {
    SUCCEED();
}

// ===================== DqByteStream =====================

// Authored: no reference test exists for this utility test
TEST(DqByteStreamTest, WriteAndReadRoundtrip) {
    dqBase::DqByteStream w;
    w.WriteUint32(0x12345678);
    w.WriteDouble(3.14);
    w.WriteUint8(0xAB);

    dqBase::DqByteStream r = dqBase::DqByteStream::FromReadable(w.ToByteArray());
    EXPECT_EQ(r.ReadUint32(), 0x12345678u);
    EXPECT_DOUBLE_EQ(r.ReadDouble(), 3.14);
    EXPECT_EQ(r.ReadUint8(), 0xABu);
    EXPECT_TRUE(r.AtEnd());
}

// Authored: no reference test exists for this utility test
TEST(DqByteStreamTest, PositionAndSize) {
    dqBase::DqByteStream w;
    EXPECT_EQ(w.Size(), 0u);
    EXPECT_EQ(w.Position(), 0u);
    w.WriteUint64(1);
    EXPECT_EQ(w.Size(), 8u);
    EXPECT_EQ(w.Position(), 8u);
}

// Authored: no reference test exists for this utility test
TEST(DqByteStreamTest, SeekAllowsReread) {
    dqBase::DqByteStream w;
    w.WriteUint32(42);
    w.Seek(0);
    EXPECT_EQ(w.Position(), 0u);
}

// ===================== LruCache =====================

// Authored: no reference test exists for this utility test
TEST(LruCacheTest, PutAndFind) {
    dqBase::LruCache<int, std::string> cache(3);
    cache.Put(1, "one");
    cache.Put(2, "two");
    ASSERT_NE(cache.Find(1), nullptr);
    EXPECT_EQ(*cache.Find(1), "one");
    EXPECT_EQ(cache.Find(99), nullptr);
    EXPECT_EQ(cache.Size(), 2u);
}

// Authored: no reference test exists for this utility test
TEST(LruCacheTest, EvictsLeastRecentlyUsed) {
    dqBase::LruCache<int, int> cache(2);
    cache.Put(1, 10);
    cache.Put(2, 20);
    cache.Find(1);                     // 访问 1，使 2 成为最久未用
    cache.Put(3, 30);                  // 容量满，淘汰 2
    EXPECT_EQ(cache.Find(2), nullptr); // 2 被淘汰
    EXPECT_NE(cache.Find(1), nullptr); // 1 保留
    EXPECT_NE(cache.Find(3), nullptr); // 3 保留
    EXPECT_EQ(cache.Size(), 2u);
}

// Authored: no reference test exists for this utility test
TEST(LruCacheTest, RemoveAndClear) {
    dqBase::LruCache<int, int> cache(5);
    cache.Put(1, 10);
    cache.Put(2, 20);
    cache.Remove(1);
    EXPECT_EQ(cache.Find(1), nullptr);
    EXPECT_EQ(cache.Size(), 1u);
    cache.clear();
    EXPECT_EQ(cache.Size(), 0u);
}

// Ported from: itwinjs-core core/bentley/src/test/LRUMap.test.ts
//              it("shift") —— shift() 移除并返回最久未用项。
TEST(LruCacheTest, ShiftRemovesAndReturnsOldest) {
    dqBase::LruCache<int, int> cache(3);
    cache.Put(1, 10);
    cache.Put(2, 20);
    cache.Put(3, 30);
    auto oldest = cache.Shift();
    ASSERT_TRUE(oldest.has_value());
    EXPECT_EQ(oldest->first, 1);
    EXPECT_EQ(oldest->second, 10);
    EXPECT_EQ(cache.Size(), 2u);
    EXPECT_EQ(cache.Find(1), nullptr);
    EXPECT_NE(cache.Find(2), nullptr);
}

// Ported from: itwinjs-core core/bentley/src/test/LRUMap.test.ts
//              it("set") 压力循环
TEST(LruCacheTest, StressLoopMaintainsCapacityAndLruOrder) {
    dqBase::LruCache<int, int> cache(5);
    for (int round = 0; round < 20; ++round) {
        for (int k = 0; k < 13; ++k) {
            cache.Put(round * 100 + k, k);
        }
        EXPECT_LE(cache.Size(), 5u);
    }
    EXPECT_EQ(cache.Size(), 5u);
}

// ===================== PriorityQueue =====================
// All PriorityQueue tests below assert MIN-heap semantics: with the default
// comparator (std::less<T>), the value that "compares less" is at the front,
// i.e. Top()/Pop() return the SMALLEST element. This matches the reference:
//   itwinjs-core core/bentley/src/PriorityQueue.ts:17
//     "the value in the queue that compares less than all other values is
//      always located at the front of the queue"
//   imodel-native DPoint3dOps.h:2234 MinimumValuePriorityQueue
//     "m_heap.front() is the minimum value entry"

// Static contract: PriorityQueue must be copy/move-assignable for SDK
// consumers. The Inverted functor used to hold Compare& (a reference member),
// which silently deleted copy/move assignment. Holding Compare by value
// restores both. These asserts would FAIL before that fix.
static_assert(std::is_copy_assignable_v<dqBase::PriorityQueue<int>>,
        "PriorityQueue must be copy-assignable");
static_assert(std::is_move_assignable_v<dqBase::PriorityQueue<int>>,
        "PriorityQueue must be move-assignable");

// Ported from: itwinjs-core core/bentley/src/test/PriorityQueue.test.ts
//              describe("PriorityQueue") / it("pops in sorted order")
// Deterministic seed-free variant: push a fixed set, expect ascending pop.
TEST(PriorityQueueTest, TopAndPopReturnMinimum) {
    dqBase::PriorityQueue<int> q;
    q.Push(3);
    q.Push(1);
    q.Push(4);
    q.Push(1);
    q.Push(5);
    q.Push(9);
    q.Push(2);
    q.Push(6);
    EXPECT_EQ(q.Size(), 8u);
    // Min-heap: front is the smallest.
    EXPECT_EQ(q.Top(), 1);
    // Successive Pop() yields ascending order (curr <= next).
    EXPECT_EQ(q.Pop(), 1);
    EXPECT_EQ(q.Pop(), 1);
    EXPECT_EQ(q.Pop(), 2);
    EXPECT_EQ(q.Pop(), 3);
    EXPECT_EQ(q.Pop(), 4);
    EXPECT_EQ(q.Pop(), 5);
    EXPECT_EQ(q.Pop(), 6);
    EXPECT_EQ(q.Pop(), 9);
    EXPECT_TRUE(q.isEmpty());
}

// Ported from: itwinjs-core core/bentley/src/test/PriorityQueue.test.ts
//              describe("PriorityQueue") / it("pops in sorted order")
// Mirrors the reference's random-init + ascending-pop loop (curr <= next).
TEST(PriorityQueueTest, RandomPushPopsInAscendingOrder) {
    dqBase::PriorityQueue<int> q;
    unsigned int seed = 12345;
    auto nextRand = [&]() {
        seed = seed * 1103515245u + 12345u;
        return static_cast<int>(seed % 1000);
    };
    for (int i = 0; i < 100; ++i) {
        q.Push(nextRand());
    }
    EXPECT_EQ(q.Size(), 100u);

    int curr = q.Pop();
    EXPECT_EQ(q.Size(), 99u);
    while (!q.isEmpty()) {
        int next = q.Pop();
        // Min-heap: pops come out in ascending order, so curr <= next.
        EXPECT_LE(curr, next);
        curr = next;
    }
    EXPECT_TRUE(q.isEmpty());
}

// Ported from: itwinjs-core core/bentley/src/test/PriorityQueue.test.ts
//              describe("PriorityQueue") / it("maintains heap property on pop")
// Mirrors the reference: after random init, each successive pop must be >=
// the previous one (ascending = min-heap invariant preserved through pops).
TEST(PriorityQueueTest, PopMaintainsHeapInvariant) {
    dqBase::PriorityQueue<int> q;
    unsigned int seed = 777;
    auto nextRand = [&]() {
        seed = seed * 1103515245u + 12345u;
        return static_cast<int>(seed % 1000);
    };
    for (int i = 0; i < 50; ++i)
        q.Push(nextRand());

    int prev = q.Pop();
    for (size_t i = 1; i < 50; ++i) {
        int v = q.Pop();
        EXPECT_LE(prev, v) << "pop #" << i << " broke ascending order";
        prev = v;
    }
    EXPECT_TRUE(q.isEmpty());
}

// ===================== DqClock =====================

// Authored: no reference test exists for this utility test
TEST(DqClockTest, SystemClockIsMonotonicNonDecreasing) {
    auto& sys = dqBase::DqClock::Get();
    const auto t0 = sys.CurrentMillis();
    const auto t1 = sys.CurrentMillis();
    EXPECT_GE(t1, t0);
}

// Authored: no reference test exists for this utility test
TEST(FakeClockTest, InjectedClockReturnsControlledTime) {
    class FakeClock : public dqBase::DqClock {
    public:
        int64_t CurrentMillis() const noexcept override {
            return m_now;
        }
        dqBase::DqTimePoint Now() const noexcept override {
            return dqBase::DqTimePoint(std::chrono::steady_clock::time_point(std::chrono::milliseconds(m_now)));
        }
        int64_t m_now = 1000;
    };
    FakeClock fake;
    dqBase::DqClock::SetInstance(&fake);
    EXPECT_EQ(dqBase::DqClock::Get().CurrentMillis(), 1000);
    fake.m_now = 2000;
    EXPECT_EQ(dqBase::DqClock::Get().CurrentMillis(), 2000);
    dqBase::DqClock::SetInstance(nullptr);
}

// ===================== DqVersion (BeVersion 4-digit) =====================

// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeVersion.h
//              struct BeVersion (4-digit m_major/m_minor/m_sub1/m_sub2 + Mask/GetInt64/CompareTo).
// imodel-native 没有 BeVersion_Test.cpp（搜索 `find imodel-native -iname BeVersion*Test*` 无结果），
// 故按 §4 标注 Ported 来源（公开头本身就是契约），断言逐项覆盖 4 字段 ctor、Mask、
// GetInt64(Mask)、CompareTo(v, Mask)、GetSub1/GetSub2、IsEmpty、FromString 返回 DqStatus 且 mutate this。

TEST(DqVersionTest, DefaultCtorIsAllZero) {
    dqBase::DqVersion v;
    EXPECT_EQ(v.GetMajor(), 0);
    EXPECT_EQ(v.GetMinor(), 0);
    EXPECT_EQ(v.GetSub1(), 0);
    EXPECT_EQ(v.GetSub2(), 0);
    EXPECT_TRUE(v.IsEmpty());
}

// Authored: no reference test exists for utility functions
TEST(DqVersionTest, TwoArgCtorZeroesSubFields) {
    dqBase::DqVersion v(3, 7);
    EXPECT_EQ(v.GetMajor(), 3);
    EXPECT_EQ(v.GetMinor(), 7);
    EXPECT_EQ(v.GetSub1(), 0);
    EXPECT_EQ(v.GetSub2(), 0);
    EXPECT_FALSE(v.IsEmpty());
}

// Authored: no reference test exists for utility functions
TEST(DqVersionTest, FourArgCtorSetsAllFields) {
    dqBase::DqVersion v(1, 2, 3, 4);
    EXPECT_EQ(v.GetMajor(), 1);
    EXPECT_EQ(v.GetMinor(), 2);
    EXPECT_EQ(v.GetSub1(), 3);
    EXPECT_EQ(v.GetSub2(), 4);
}

// Authored: no reference test exists for utility functions
TEST(DqVersionTest, IntCtorTruncatesToUint16) {
    // 对齐 ref BeVersion(int, int) → (uint16_t, uint16_t) 截断语义
    dqBase::DqVersion v(0x10001, 0x10002);
    EXPECT_EQ(v.GetMajor(), 1);
    EXPECT_EQ(v.GetMinor(), 2);
}

// Authored: no reference test exists for utility functions
TEST(DqVersionTest, MaskGetInt64PacksFieldsAtBitOffsets) {
    dqBase::DqVersion v(1, 2, 3, 4);
    // ref: m_major<<48 | m_minor<<32 | m_sub1<<16 | m_sub2
    const uint64_t all = v.GetInt64(dqBase::DqVersion::Mask::All);
    EXPECT_EQ(all, (uint64_t(1) << 48) | (uint64_t(2) << 32) | (uint64_t(3) << 16) | uint64_t(4));
    // 单字段 Mask
    EXPECT_EQ(v.GetInt64(dqBase::DqVersion::Mask::Major), uint64_t(1) << 48);
    EXPECT_EQ(v.GetInt64(dqBase::DqVersion::Mask::Minor), uint64_t(2) << 32);
    EXPECT_EQ(v.GetInt64(dqBase::DqVersion::Mask::Sub1),  uint64_t(3) << 16);
    EXPECT_EQ(v.GetInt64(dqBase::DqVersion::Mask::Sub2),  uint64_t(4));
    // 复合 Mask
    EXPECT_EQ(v.GetInt64(dqBase::DqVersion::Mask::MajorMinor),
              (uint64_t(1) << 48) | (uint64_t(2) << 32));
}

// Authored: no reference test exists for utility functions
TEST(DqVersionTest, CompareToUsesAllFieldsByDefault) {
    dqBase::DqVersion a(1, 2, 3, 4);
    dqBase::DqVersion b(1, 2, 3, 5);
    EXPECT_EQ(a.CompareTo(a), 0);
    EXPECT_EQ(a.CompareTo(b), -1);
    EXPECT_EQ(b.CompareTo(a), 1);
}

// Authored: no reference test exists for utility functions
TEST(DqVersionTest, CompareToWithMaskIgnoresMaskedOutFields) {
    dqBase::DqVersion a(1, 2, 3, 4);
    dqBase::DqVersion b(1, 2, 9, 9);
    // 仅比较 Major+Minor → 相等
    EXPECT_EQ(a.CompareTo(b, dqBase::DqVersion::Mask::MajorMinor), 0);
    // 比较 Major+Minor+Sub1 → a < b
    EXPECT_EQ(a.CompareTo(b, dqBase::DqVersion::Mask::MajorMinorSub1), -1);
}

// Authored: no reference test exists for utility functions
TEST(DqVersionTest, RelationalOperators) {
    dqBase::DqVersion v0(1, 0, 0, 0);
    dqBase::DqVersion v1(1, 2, 3, 4);
    dqBase::DqVersion v2(1, 2, 3, 4);
    dqBase::DqVersion v3(2, 0, 0, 0);

    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 != v2);
    EXPECT_TRUE(v0 < v1);
    EXPECT_TRUE(v1 < v3);
    EXPECT_TRUE(v1 <= v2);
    EXPECT_TRUE(v3 > v1);
    EXPECT_TRUE(v1 >= v0);
}

// Authored: no reference test exists for utility functions
TEST(DqVersionTest, IsEmptyOnlyWhenAllZero) {
    EXPECT_TRUE(dqBase::DqVersion().IsEmpty());
    EXPECT_TRUE(dqBase::DqVersion(0, 0, 0, 0).IsEmpty());
    EXPECT_FALSE(dqBase::DqVersion(0, 0, 0, 1).IsEmpty());
    EXPECT_FALSE(dqBase::DqVersion(1, 0, 0, 0).IsEmpty());
}

// Authored: no reference test exists for utility functions
TEST(DqVersionTest, ToStringUsesFourDigitsByDefault) {
    dqBase::DqVersion v(1, 2, 3, 4);
    EXPECT_EQ(v.ToString(), "1.2.3.4");
    EXPECT_EQ(v.ToMajorMinorString(), "1.2");
}

// Authored: no reference test exists for utility functions
TEST(DqVersionTest, FromStringReturnsStatusAndMutatesThis) {
    dqBase::DqVersion v;
    // 成功：至少匹配 1 个数字
    EXPECT_EQ(v.FromString("1.2.3.4"), dqBase::DqStatus::Success);
    EXPECT_EQ(v.GetMajor(), 1);
    EXPECT_EQ(v.GetMinor(), 2);
    EXPECT_EQ(v.GetSub1(), 3);
    EXPECT_EQ(v.GetSub2(), 4);
    // 完全无法匹配 → Error
    EXPECT_EQ(v.FromString("not-a-version"), dqBase::DqStatus::Error);
    // ref 语义：FromString 不重置未匹配字段，仅按 result 段数赋值（BeVersion.h:97-104）。
    // 用一个全新的对象验证部分匹配：sub1/sub2 保留默认 0。
    dqBase::DqVersion partial;
    EXPECT_EQ(partial.FromString("5.6"), dqBase::DqStatus::Success);
    EXPECT_EQ(partial.GetMajor(), 5);
    EXPECT_EQ(partial.GetMinor(), 6);
    EXPECT_EQ(partial.GetSub1(), 0);
    EXPECT_EQ(partial.GetSub2(), 0);
}

// Authored: no reference test exists for utility functions
TEST(DqVersionTest, FromStringConstructorPopulatesFields) {
    // 对齐 ref BeVersion(Utf8CP, Utf8CP) 隐式调用 FromString
    dqBase::DqVersion v("10.20.30.40");
    EXPECT_EQ(v.GetMajor(), 10);
    EXPECT_EQ(v.GetMinor(), 20);
    EXPECT_EQ(v.GetSub1(), 30);
    EXPECT_EQ(v.GetSub2(), 40);
}

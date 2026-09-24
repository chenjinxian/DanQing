// SPDX-License-Identifier: Apache-2.0
// dqBase tests — OrderedSet<T>
// Ported from: itwinjs-core core/bentley/src/OrderedSet.ts
//
// No OrderedSet.test.ts exists in itwinjs-core (verified:
// core/bentley/src/test/ has only SortedArray.test.ts and ObservableSet.test.ts).
// Test assertions below mirror the invariants of the reference OrderedSet
// (comparator-required, SortedArray-backed, iteration honors injected
// comparator) — authored per CLAUDE.md §4.
#include <gtest/gtest.h>

#include <dqBase/OrderedSet.h>

#include <string>
#include <vector>

namespace {

// Ascending comparator (default-like).
int compareIntsAsc(const int& a, const int& b) { return a - b; }

// DESCENDING comparator — the load-bearing assertion: OrderedSet MUST honor
// the injected comparator, not a hard-coded operator<.
int compareIntsDesc(const int& a, const int& b) { return b - a; }

} // namespace

// Authored: no reference test exists in itwinjs-core for OrderedSet
//              (core/bentley/src/test/ contains no OrderedSet.test.ts).
// Asserts: ctor REQUIRES an OrderedComparator<T>; default construction must
// not compile (the previous bug took no ctor args and silently used operator<).
TEST(OrderedSetTest, CtorRequiresComparator) {
    dqBase::OrderedSet<int> set(compareIntsAsc);
    EXPECT_EQ(set.length(), 0);
    EXPECT_TRUE(set.isEmpty());
}

// Authored: no reference test exists in itwinjs-core for OrderedSet.
// Asserts: a DESCENDING comparator is honored — iteration yields {3,2,1}.
// This is the regression that the old std::set<T>-backed implementation with
// no ctor args would FAIL (it would always yield ascending {1,2,3}).
TEST(OrderedSetTest, DescendingComparatorHonored) {
    dqBase::OrderedSet<int> set(compareIntsDesc);
    set.add(1);
    set.add(2);
    set.add(3);

    EXPECT_EQ(set.length(), 3);

    std::vector<int> collected;
    for (const auto& v : set) {
        collected.push_back(v);
    }
    ASSERT_EQ(collected.size(), 3u);
    EXPECT_EQ(collected[0], 3);
    EXPECT_EQ(collected[1], 2);
    EXPECT_EQ(collected[2], 1);
}

// Authored: no reference test exists in itwinjs-core for OrderedSet.
// Ascending baseline — proves the comparator injection works in both directions.
TEST(OrderedSetTest, AscendingComparatorHonored) {
    dqBase::OrderedSet<int> set(compareIntsAsc);
    set.add(3);
    set.add(1);
    set.add(2);

    std::vector<int> collected;
    for (const auto& v : set) {
        collected.push_back(v);
    }
    ASSERT_EQ(collected.size(), 3u);
    EXPECT_EQ(collected[0], 1);
    EXPECT_EQ(collected[1], 2);
    EXPECT_EQ(collected[2], 3);
}

// Authored: no reference test exists in itwinjs-core for OrderedSet.
// Mirrors ref OrderedSet semantics: add is idempotent for duplicates (Retain),
// has reports membership, delete returns presence and removes.
TEST(OrderedSetTest, HasAddDeleteClear) {
    dqBase::OrderedSet<int> set(compareIntsAsc);

    EXPECT_FALSE(set.has(1));
    EXPECT_TRUE(set.add(1));
    EXPECT_TRUE(set.has(1));

    // Duplicate add is a no-op (SortedArray Retain policy).
    EXPECT_FALSE(set.add(1));
    EXPECT_EQ(set.length(), 1);

    EXPECT_TRUE(set.add(2));
    EXPECT_TRUE(set.add(3));
    EXPECT_EQ(set.length(), 3);

    EXPECT_TRUE(set.delete_(2));
    EXPECT_FALSE(set.has(2));
    EXPECT_EQ(set.length(), 2);

    EXPECT_FALSE(set.delete_(99));  // Not present
    EXPECT_EQ(set.length(), 2);

    set.clear();
    EXPECT_EQ(set.length(), 0);
    EXPECT_TRUE(set.isEmpty());
}

// Authored: no reference test exists in itwinjs-core for OrderedSet.
// String values exercise a non-trivial T and verify the comparator plumbing
// is generic.
TEST(OrderedSetTest, StringValuesDescending) {
    dqBase::OrderedSet<std::string> set(
        [](const std::string& a, const std::string& b) { return b.compare(a); });
    set.add("apple");
    set.add("cherry");
    set.add("banana");

    std::vector<std::string> collected;
    for (const auto& v : set) {
        collected.push_back(v);
    }
    ASSERT_EQ(collected.size(), 3u);
    EXPECT_EQ(collected[0], "cherry");
    EXPECT_EQ(collected[1], "banana");
    EXPECT_EQ(collected[2], "apple");
}

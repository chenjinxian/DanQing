// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/test/ObservableSet.test.ts
// dqBase tests — ObservableSet batch events + clear guard (TDD: test first)
//
// Ports the assertions from ObservableSet.test.ts:
//   - "should raise events only when contents change" (incl. clear on empty raises nothing)
//   - "addAll should raise onBatchAdded only once" (returns count, no per-item onAdded)
//   - "addAll should not raise any event for empty iterable"
//   - "addAll should not raise event when all items already exist"
//   - "addAll should count only newly added items"
//   - "deleteAll should raise onBatchDeleted only once" (returns count, no per-item onDeleted)
//   - "deleteAll should not raise any event for empty iterable"
//   - "deleteAll should not raise event when no items exist in set"
//   - "deleteAll should count only actually deleted items"
//
// NOTE: Reference uses Iterable<T> (TS). C++ port uses iterator range [first, last).
#include <gtest/gtest.h>

#include <dqBase/ObservableSet.h>

#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

using namespace dqBase;

namespace {
// Listener equivalent of ObservableSet.test.ts Listener class.
// Tracks per-item counts (added/deleted), boolean flags, and batch counts.
struct Listener {
    bool added = false;
    bool deleted = false;
    bool cleared = false;
    int addCount = 0;
    int deleteCount = 0;
    int batchAddCount = 0;
    int batchDeleteCount = 0;
    std::vector<DqEventDisconnect> tokens;

    explicit Listener(ObservableSet<std::string>& set) {
        tokens.push_back(set.onAdded.AddListener([&](const std::string&) {
            added = true;
            ++addCount;
        }));
        tokens.push_back(set.onDeleted.AddListener([&](const std::string&) {
            deleted = true;
            ++deleteCount;
        }));
        tokens.push_back(set.onCleared.AddListener([&] {
            cleared = true;
        }));
        tokens.push_back(set.onBatchAdded.AddListener([&] {
            ++batchAddCount;
        }));
        tokens.push_back(set.onBatchDeleted.AddListener([&] {
            ++batchDeleteCount;
        }));
    }

    void reset() {
        added = deleted = cleared = false;
        addCount = deleteCount = batchAddCount = batchDeleteCount = 0;
    }

    // Equivalent to Listener.expect(added, deleted, cleared, fn)
    void expect(bool a, bool d, bool c, const std::function<void()>& fn) {
        reset();
        fn();
        EXPECT_EQ(added, a);
        EXPECT_EQ(deleted, d);
        EXPECT_EQ(cleared, c);
        reset();
    }

    // Equivalent to Listener.expectBatch(batchAdd, batchDelete, fn) — also asserts
    // NO per-item onAdded/onDeleted were raised (ref semantics: batch ops never raise per-item).
    void expectBatch(int bAdd, int bDel, const std::function<void()>& fn) {
        reset();
        fn();
        EXPECT_EQ(batchAddCount, bAdd);
        EXPECT_EQ(batchDeleteCount, bDel);
        EXPECT_EQ(addCount, 0);
        EXPECT_EQ(deleteCount, 0);
        reset();
    }

    void expectNone(const std::function<void()>& fn) { expect(false, false, false, fn); }
    void expectAdd(const std::function<void()>& fn) { expect(true, false, false, fn); }
    void expectDelete(const std::function<void()>& fn) { expect(false, true, false, fn); }
    void expectClear(const std::function<void()>& fn) { expect(false, false, true, fn); }
};
}  // namespace

// Ported from: itwinjs-core core/bentley/src/test/ObservableSet.test.ts
//              describe("ObservableSet") -> it("should raise events only when contents change")
TEST(ObservableSetTest, RaisesEventsOnlyWhenContentsChange) {
    ObservableSet<std::string> set;
    Listener listener(set);

    // clear() on empty set must NOT raise onCleared (ref lines 57-62).
    listener.expectNone([&] {
        set.clear();
        set.Delete("abc");
    });
    listener.expectAdd([&] { set.add("abc"); });
    listener.expectAdd([&] { set.add("def"); });
    listener.expectNone([&] { set.add("abc"); });  // already present
    listener.expectDelete([&] { set.Delete("def"); });
    listener.expectClear([&] { set.clear(); });
}

// Ported from: itwinjs-core core/bentley/src/test/ObservableSet.test.ts
//              it("addAll should raise onBatchAdded only once")
TEST(ObservableSetTest, AddAllRaisesBatchEventOnceAndReturnsCount) {
    ObservableSet<std::string> set;
    Listener listener(set);

    std::vector<std::string> items = {"a", "b", "c"};
    listener.expectBatch(1, 0, [&] {
        size_t count = set.AddAll(items.begin(), items.end());
        EXPECT_EQ(count, 3u);
    });
    EXPECT_EQ(set.Size(), 3u);
}

// Ported from: itwinjs-core core/bentley/src/test/ObservableSet.test.ts
//              it("addAll should not raise any event for empty iterable")
TEST(ObservableSetTest, AddAllEmptyIterableRaisesNothing) {
    ObservableSet<std::string> set;
    Listener listener(set);

    std::vector<std::string> empty;
    listener.expectBatch(0, 0, [&] {
        size_t count = set.AddAll(empty.begin(), empty.end());
        EXPECT_EQ(count, 0u);
    });
}

// Ported from: itwinjs-core core/bentley/src/test/ObservableSet.test.ts
//              it("addAll should not raise event when all items already exist")
TEST(ObservableSetTest, AddAllAllPresentRaisesNothing) {
    std::vector<std::string> initial = {"a", "b"};
    ObservableSet<std::string> set;
    set.AddAll(initial.begin(), initial.end());
    Listener listener(set);

    std::vector<std::string> dup = {"a", "b"};
    listener.expectBatch(0, 0, [&] {
        size_t count = set.AddAll(dup.begin(), dup.end());
        EXPECT_EQ(count, 0u);
    });
    EXPECT_EQ(set.Size(), 2u);
}

// Ported from: itwinjs-core core/bentley/src/test/ObservableSet.test.ts
//              it("addAll should count only newly added items")
TEST(ObservableSetTest, AddAllCountsOnlyNewlyAdded) {
    ObservableSet<std::string> set;
    set.add("a");
    Listener listener(set);

    std::vector<std::string> items = {"a", "b", "c"};
    listener.expectBatch(1, 0, [&] {
        size_t count = set.AddAll(items.begin(), items.end());
        EXPECT_EQ(count, 2u);  // only b and c are new
    });
    EXPECT_EQ(set.Size(), 3u);
}

// Ported from: itwinjs-core core/bentley/src/test/ObservableSet.test.ts
//              it("deleteAll should raise onBatchDeleted only once")
TEST(ObservableSetTest, DeleteAllRaisesBatchEventOnceAndReturnsCount) {
    ObservableSet<std::string> set;
    std::vector<std::string> initial = {"a", "b", "c"};
    set.AddAll(initial.begin(), initial.end());
    Listener listener(set);

    std::vector<std::string> items = {"a", "b", "c"};
    listener.expectBatch(0, 1, [&] {
        size_t count = set.DeleteAll(items.begin(), items.end());
        EXPECT_EQ(count, 3u);
    });
    EXPECT_EQ(set.Size(), 0u);
}

// Ported from: itwinjs-core core/bentley/src/test/ObservableSet.test.ts
//              it("deleteAll should not raise any event for empty iterable")
TEST(ObservableSetTest, DeleteAllEmptyIterableRaisesNothing) {
    ObservableSet<std::string> set;
    set.add("a");
    Listener listener(set);

    std::vector<std::string> empty;
    listener.expectBatch(0, 0, [&] {
        size_t count = set.DeleteAll(empty.begin(), empty.end());
        EXPECT_EQ(count, 0u);
    });
    EXPECT_EQ(set.Size(), 1u);
}

// Ported from: itwinjs-core core/bentley/src/test/ObservableSet.test.ts
//              it("deleteAll should not raise event when no items exist in set")
TEST(ObservableSetTest, DeleteAllNonePresentRaisesNothing) {
    ObservableSet<std::string> set;
    set.add("a");
    Listener listener(set);

    std::vector<std::string> items = {"x", "y"};
    listener.expectBatch(0, 0, [&] {
        size_t count = set.DeleteAll(items.begin(), items.end());
        EXPECT_EQ(count, 0u);
    });
    EXPECT_EQ(set.Size(), 1u);
}

// Ported from: itwinjs-core core/bentley/src/test/ObservableSet.test.ts
//              it("deleteAll should count only actually deleted items")
TEST(ObservableSetTest, DeleteAllCountsOnlyActuallyDeleted) {
    ObservableSet<std::string> set;
    std::vector<std::string> initial = {"a", "b"};
    set.AddAll(initial.begin(), initial.end());
    Listener listener(set);

    std::vector<std::string> items = {"a", "x"};
    listener.expectBatch(0, 1, [&] {
        size_t count = set.DeleteAll(items.begin(), items.end());
        EXPECT_EQ(count, 1u);  // only "a" was present
    });
    EXPECT_EQ(set.Size(), 1u);
    EXPECT_TRUE(set.Contains("b"));
}

// Ported from: itwinjs-core core/bentley/src/test/ObservableSet.test.ts
//              it("should construct from iterable")
// (Reference's Set constructor invokes add() — the bug being tested is that
// events are undefined during construction. C++ port has no such ctor, but we
// verify the initializer-list batch construction path leaves a correct set.)
TEST(ObservableSetTest, BatchConstructionYieldsCorrectContents) {
    ObservableSet<std::string> set;
    std::vector<std::string> elems = {"a", "b", "c"};
    set.AddAll(elems.begin(), elems.end());
    EXPECT_EQ(set.Size(), 3u);
    EXPECT_TRUE(set.Contains("a"));
    EXPECT_TRUE(set.Contains("b"));
    EXPECT_TRUE(set.Contains("c"));
}

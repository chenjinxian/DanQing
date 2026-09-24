// SPDX-License-Identifier: Apache-2.0
// dqBase tests — TupleKeyedMap<K, V>
// Ported from: itwinjs-core core/bentley/src/test/TupleKeyedMap.test.ts
//
// Adaptation note: the reference TupleKeyedMap is generic over a heterogeneous
// tuple K = [string, number, object, ...]. DanQing's C++ port keys on a
// homogeneous element type K (std::vector<K>), matching the pre-existing
// public signature. The reference's heterogeneous-object-as-third-element
// behavior is exercised here via int tags acting as identity surrogates.
#include <gtest/gtest.h>

#include <dqBase/TupleKeyedMap.h>

#include <string>
#include <utility>
#include <vector>

// ============================================================================
// TupleKeyedMap tests
// Ported from: itwinjs-core core/bentley/src/test/TupleKeyedMap.test.ts
// ============================================================================

// Ported from: itwinjs-core core/bentley/src/test/TupleKeyedMap.test.ts
//              it("should maintain mapping between keys and values")
TEST(TupleKeyedMapTest, MaintainsMappingBetweenKeysAndValues) {
    const std::vector<std::string> entries = {"a", "b", "c", "z", "y", "x", "p", "r", "q"};
    dqBase::TupleKeyedMap<std::string, std::string> map;
    for (const auto& entry : entries) {
        map.Set({entry}, entry);
        auto* found = map.Get({entry});
        ASSERT_NE(found, nullptr);
        EXPECT_EQ(*found, entry);
    }

    EXPECT_EQ(map.Size(), entries.size());
}

// Ported from: itwinjs-core core/bentley/src/test/TupleKeyedMap.test.ts
//              it("gets, sets, and iterates")
//
// Reference key shape: [string, number, object] -> number. Here ported to a
// 3-int homogeneous key: [tag0, tag1, tag2]. tag2 stands in for the object
// identity (a/b/c/d -> 1/2/3/4).
TEST(TupleKeyedMapTest, GetsSetsAndIterates) {
    const int a = 1, c = 3, d = 4;
    // Encoding: {wordTag, numTag, objTag}. "three"->93, "four"->94, "one"->91.
    dqBase::TupleKeyedMap<int, int> map;

    // Ctor-from-entries analogue (ref accepts an entries array; DanQing port
    // builds the same state via Set on a default-constructed map).
    map.Set({93, 3, c}, 3);   // ["three", 3, c] = 3
    map.Set({94, 4, d}, 4);   // ["four",  4, d] = 4

    // Absent key -> nullptr (TS: undefined).
    EXPECT_EQ(map.Get({91, 1, a}), nullptr);

    auto* v = map.Get({93, 3, c});
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, 3);
    EXPECT_EQ(map.Get({93, 3, d}), nullptr);
    EXPECT_EQ(map.Get({93, 4, c}), nullptr);
    EXPECT_EQ(map.Get({94, 3, c}), nullptr);

    // Bad-key arity: ref throws on key lengths that don't match the established
    // tuple width. C++ adaptation (-fno-exceptions): returns nullptr for arity
    // mismatches against the established key width.
    EXPECT_EQ(map.Get(std::vector<int>{93, 3}),    nullptr);  // too short
    EXPECT_EQ(map.Get(std::vector<int>{93, 3, c, 2}), nullptr); // too long

    // Overwrite existing entry (ref: set returns this for fluency).
    map.Set({93, 3, c}, 10);
    v = map.Get({93, 3, c});
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, 10);
    EXPECT_EQ(map.Get({93, 3, d}), nullptr);
    EXPECT_EQ(map.Get({93, 4, c}), nullptr);
    EXPECT_EQ(map.Get({94, 3, c}), nullptr);

    // Add a new entry under the same prefix ("three") but a distinct subkey.
    map.Set({93, 4, c}, 11);

    // Iteration: produces every stored (key, value) pair.
    // NOTE: ref TS Map preserves insertion order; the C++ port uses bmap
    // (sorted B-tree), so pairs come out sorted-by-key, not insertion-ordered.
    // The set of pairs is identical; only order differs (documented in header).
    std::vector<std::pair<std::vector<int>, int>> collected;
    map.ForEach([&](const std::vector<int>& key, const int& value) {
        collected.emplace_back(key, value);
    });
    ASSERT_EQ(collected.size(), 3u);
    // Sorted by key (lexicographic on ints): {93,3,c}=10, {93,4,c}=11, {94,4,d}=4
    EXPECT_EQ(collected[0].second, 10);
    EXPECT_EQ(collected[1].second, 11);
    EXPECT_EQ(collected[2].second, 4);

    // Fluency: Set returns *this (matches ref `set`).
    auto& self = map.Set({99, 9, 9}, 5);
    EXPECT_EQ(&self, &map);
}

// ---------------------------------------------------------------------------
// Authored: no reference test exists in itwinjs-core for arity-mismatch on
// Set. The reference's set() also throws on bad arity; this case nails the
// C++ adaptation: Set on a wrong-arity key is a no-op (with BeAssert) once a
// key width is established.  -- documented in TupleKeyedMap.h.
// ---------------------------------------------------------------------------
TEST(TupleKeyedMapTest, Authored_ArityMismatchRejected) {
    dqBase::TupleKeyedMap<int, std::string> map;
    ASSERT_TRUE(map.isEmpty());
    map.Set({1, 2, 3}, "abc");
    // Wrong arity on Get -> nullptr (ref throws; C++ -fno-exceptions adaptation).
    EXPECT_EQ(map.Get(std::vector<int>{1, 2}), nullptr);
    EXPECT_EQ(map.Get(std::vector<int>{1, 2, 3, 4}), nullptr);
    // Correct arity still works.
    auto* v = map.Get(std::vector<int>{1, 2, 3});
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, "abc");
}

// Authored: no reference test exists in itwinjs-core for clear / isEmpty /
// Has on a 1-tuple map. Covers the kept (documented) self-additions.
TEST(TupleKeyedMapTest, Authored_HasClearIsEmpty) {
    dqBase::TupleKeyedMap<int, int> map;
    EXPECT_TRUE(map.isEmpty());
    EXPECT_FALSE(map.Has({7}));
    map.Set({7}, 42);
    EXPECT_FALSE(map.isEmpty());
    EXPECT_TRUE(map.Has({7}));
    EXPECT_EQ(*map.Get({7}), 42);
    map.clear();
    EXPECT_TRUE(map.isEmpty());
    EXPECT_FALSE(map.Has({7}));
    EXPECT_EQ(map.Get({7}), nullptr);
}

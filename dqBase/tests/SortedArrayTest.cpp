// SPDX-License-Identifier: Apache-2.0
// dqBase tests — SortedArray<T>, Dictionary<K,V>, IndexMap<T>
// Ported from: itwinjs-core core/bentley/src/test/SortedArray.test.ts
//              itwinjs-core core/bentley/src/test/Dictionary.test.ts
//              itwinjs-core core/bentley/src/test/IndexMap.test.ts
#include <gtest/gtest.h>

#include <dqBase/SortedArray.h>
#include <dqBase/Dictionary.h>
#include <dqBase/IndexMap.h>

#include <string>
#include <vector>

namespace {

// Comparator for ints
int compareInts(const int& a, const int& b) { return a - b; }

// Comparator for strings
int compareStrings(const std::string& a, const std::string& b) {
    return a.compare(b);
}

} // namespace

// ============================================================================
// SortedArray tests
// Ported from: itwinjs-core core/bentley/src/test/SortedArray.test.ts
// ============================================================================

// Ported from: itwinjs-core core/bentley/src/test/SortedArray.test.ts
TEST(SortedArrayTest, InsertAndFind) {
    dqBase::SortedArray<int> arr(compareInts);
    EXPECT_TRUE(arr.isEmpty());

    arr.insert(5);
    arr.insert(3);
    arr.insert(7);
    arr.insert(1);

    EXPECT_EQ(arr.length(), 4);
    EXPECT_TRUE(arr.contains(3));
    EXPECT_TRUE(arr.contains(5));
    EXPECT_TRUE(arr.contains(7));
    EXPECT_TRUE(arr.contains(1));
    EXPECT_FALSE(arr.contains(4));
}

// Ported from: itwinjs-core core/bentley/src/test/SortedArray.test.ts
TEST(SortedArrayTest, SortedOrder) {
    dqBase::SortedArray<int> arr(compareInts);
    arr.insert(5);
    arr.insert(3);
    arr.insert(7);
    arr.insert(1);

    // Should be in sorted order: 1, 3, 5, 7
    EXPECT_EQ(*arr.get(0), 1);
    EXPECT_EQ(*arr.get(1), 3);
    EXPECT_EQ(*arr.get(2), 5);
    EXPECT_EQ(*arr.get(3), 7);
}

// Ported from: itwinjs-core core/bentley/src/test/SortedArray.test.ts
TEST(SortedArrayTest, RetainDuplicatePolicy) {
    dqBase::SortedArray<int> arr(compareInts, dqBase::DuplicatePolicy::Retain);
    arr.insert(5);
    int idx = arr.insert(5);  // Duplicate — should be retained
    EXPECT_EQ(idx, 0);  // Returns index of existing
    EXPECT_EQ(arr.length(), 1);
}

// Ported from: itwinjs-core core/bentley/src/test/SortedArray.test.ts
TEST(SortedArrayTest, ReplaceDuplicatePolicy) {
    dqBase::SortedArray<int> arr(compareInts, dqBase::DuplicatePolicy::Replace);
    arr.insert(5);
    int idx = arr.insert(5);  // Duplicate — should replace (same value)
    EXPECT_EQ(idx, 0);
    EXPECT_EQ(arr.length(), 1);
    EXPECT_EQ(*arr.get(0), 5);

    // Different values are not duplicates
    arr.insert(10);
    EXPECT_EQ(arr.length(), 2);
}

// Ported from: itwinjs-core core/bentley/src/test/SortedArray.test.ts
TEST(SortedArrayTest, AllowDuplicatePolicy) {
    dqBase::SortedArray<int> arr(compareInts, dqBase::DuplicatePolicy::Allow);
    arr.insert(5);
    arr.insert(5);
    arr.insert(5);
    EXPECT_EQ(arr.length(), 3);
}

// Ported from: itwinjs-core core/bentley/src/test/SortedArray.test.ts
TEST(SortedArrayTest, Remove) {
    dqBase::SortedArray<int> arr(compareInts);
    arr.insert(1);
    arr.insert(3);
    arr.insert(5);

    int idx = arr.remove(3);
    EXPECT_EQ(idx, 1);
    EXPECT_EQ(arr.length(), 2);
    EXPECT_FALSE(arr.contains(3));

    idx = arr.remove(99);  // Not found
    EXPECT_EQ(idx, -1);
}

// Ported from: itwinjs-core core/bentley/src/test/SortedArray.test.ts
TEST(SortedArrayTest, IndexOf) {
    dqBase::SortedArray<int> arr(compareInts);
    arr.insert(10);
    arr.insert(20);
    arr.insert(30);

    EXPECT_EQ(arr.indexOf(10), 0);
    EXPECT_EQ(arr.indexOf(20), 1);
    EXPECT_EQ(arr.indexOf(30), 2);
    EXPECT_EQ(arr.indexOf(15), -1);
}

// Ported from: itwinjs-core core/bentley/src/test/SortedArray.test.ts
TEST(SortedArrayTest, FindEqual) {
    dqBase::SortedArray<int> arr(compareInts);
    arr.insert(42);

    const int* found = arr.findEqual(42);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(*found, 42);

    const int* notFound = arr.findEqual(99);
    EXPECT_EQ(notFound, nullptr);
}

// Ported from: itwinjs-core core/bentley/src/test/SortedArray.test.ts
TEST(SortedArrayTest, ForEach) {
    dqBase::SortedArray<int> arr(compareInts);
    arr.insert(3);
    arr.insert(1);
    arr.insert(2);

    std::vector<int> collected;
    arr.forEach([&collected](const int& v) { collected.push_back(v); });

    EXPECT_EQ(collected.size(), 3u);
    EXPECT_EQ(collected[0], 1);
    EXPECT_EQ(collected[1], 2);
    EXPECT_EQ(collected[2], 3);
}

// Ported from: itwinjs-core core/bentley/src/test/SortedArray.test.ts
TEST(SortedArrayTest, ExtractArray) {
    dqBase::SortedArray<int> arr(compareInts);
    arr.insert(3);
    arr.insert(1);
    arr.insert(2);

    auto extracted = arr.extractArray();
    EXPECT_EQ(extracted.size(), 3u);
    EXPECT_EQ(extracted[0], 1);
    EXPECT_EQ(extracted[1], 2);
    EXPECT_EQ(extracted[2], 3);

    EXPECT_TRUE(arr.isEmpty());
}

// Ported from: itwinjs-core core/bentley/src/test/SortedArray.test.ts
TEST(SortedArrayTest, StringValues) {
    dqBase::SortedArray<std::string> arr(compareStrings);
    arr.insert("banana");
    arr.insert("apple");
    arr.insert("cherry");

    EXPECT_EQ(arr.length(), 3);
    EXPECT_EQ(*arr.get(0), "apple");
    EXPECT_EQ(*arr.get(1), "banana");
    EXPECT_EQ(*arr.get(2), "cherry");
}

// ============================================================================
// Dictionary tests
// Ported from: itwinjs-core core/bentley/src/test/Dictionary.test.ts
// ============================================================================

// Ported from: itwinjs-core core/bentley/src/test/Dictionary.test.ts
TEST(DictionaryTest, InsertAndLookup) {
    dqBase::Dictionary<int, std::string> dict(compareInts);

    dict.insert(1, "one");
    dict.insert(2, "two");
    dict.insert(3, "three");

    EXPECT_EQ(dict.size(), 3);

    auto* val = dict.get(2);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "two");

    auto* notFound = dict.get(99);
    EXPECT_EQ(notFound, nullptr);
}

// Ported from: itwinjs-core core/bentley/src/test/Dictionary.test.ts
TEST(DictionaryTest, HasAndErase) {
    dqBase::Dictionary<int, std::string> dict(compareInts);
    dict.insert(1, "one");

    EXPECT_TRUE(dict.has(1));
    EXPECT_FALSE(dict.has(2));

    bool erased = dict.erase(1);
    EXPECT_TRUE(erased);
    EXPECT_FALSE(dict.has(1));
    EXPECT_EQ(dict.size(), 0);

    erased = dict.erase(99);  // Not found
    EXPECT_FALSE(erased);
}

// Ported from: itwinjs-core core/bentley/src/test/Dictionary.test.ts
TEST(DictionaryTest, SetOverwrites) {
    dqBase::Dictionary<int, std::string> dict(compareInts);

    dict.set(1, "one");
    EXPECT_EQ(*dict.get(1), "one");

    dict.set(1, "ONE");
    EXPECT_EQ(*dict.get(1), "ONE");
    EXPECT_EQ(dict.size(), 1);
}

// Ported from: itwinjs-core core/bentley/src/test/Dictionary.test.ts
TEST(DictionaryTest, InsertDoesNotOverwrite) {
    dqBase::Dictionary<int, std::string> dict(compareInts);

    bool inserted = dict.insert(1, "one");
    EXPECT_TRUE(inserted);

    inserted = dict.insert(1, "ONE");
    EXPECT_FALSE(inserted);
    EXPECT_EQ(*dict.get(1), "one");
}

// Ported from: itwinjs-core core/bentley/src/test/Dictionary.test.ts
TEST(DictionaryTest, FindOrInsert) {
    dqBase::Dictionary<int, std::string> dict(compareInts);

    auto [val1, inserted1] = dict.findOrInsert(1, "one");
    EXPECT_TRUE(inserted1);
    EXPECT_EQ(val1, "one");

    auto [val2, inserted2] = dict.findOrInsert(1, "ONE");
    EXPECT_FALSE(inserted2);
    EXPECT_EQ(val2, "one");  // Original value
}

// Ported from: itwinjs-core core/bentley/src/test/Dictionary.test.ts
TEST(DictionaryTest, ForEach) {
    dqBase::Dictionary<int, std::string> dict(compareInts);
    dict.insert(3, "three");
    dict.insert(1, "one");
    dict.insert(2, "two");

    std::vector<int> keys;
    dict.forEach([&keys](const int& k, const std::string&) {
        keys.push_back(k);
    });

    // Should be in sorted order
    EXPECT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], 1);
    EXPECT_EQ(keys[1], 2);
    EXPECT_EQ(keys[2], 3);
}

// Ported from: itwinjs-core core/bentley/src/test/Dictionary.test.ts
TEST(DictionaryTest, ExtractPairs) {
    dqBase::Dictionary<int, std::string> dict(compareInts);
    dict.insert(2, "two");
    dict.insert(1, "one");

    auto pairs = dict.extractPairs();
    EXPECT_EQ(pairs.size(), 2u);
    EXPECT_EQ(pairs[0].key, 1);
    EXPECT_EQ(pairs[0].value, "one");
    EXPECT_EQ(pairs[1].key, 2);
    EXPECT_EQ(pairs[1].value, "two");

    EXPECT_TRUE(dict.isEmpty());
}

// ============================================================================
// IndexMap tests
// Ported from: itwinjs-core core/bentley/src/test/IndexMap.test.ts
// ============================================================================

// Ported from: itwinjs-core core/bentley/src/test/IndexMap.test.ts
TEST(IndexMapTest, InsertAndIndexOf) {
    dqBase::IndexMap<int> map(compareInts);

    int idx1 = map.insert(10);
    int idx2 = map.insert(20);
    int idx3 = map.insert(30);

    EXPECT_EQ(idx1, 0);
    EXPECT_EQ(idx2, 1);
    EXPECT_EQ(idx3, 2);

    EXPECT_EQ(map.indexOf(10), 0);
    EXPECT_EQ(map.indexOf(20), 1);
    EXPECT_EQ(map.indexOf(30), 2);
    EXPECT_EQ(map.indexOf(99), -1);
}

// Ported from: itwinjs-core core/bentley/src/test/IndexMap.test.ts
TEST(IndexMapTest, DuplicateReturnsExistingIndex) {
    dqBase::IndexMap<int> map(compareInts);

    int idx1 = map.insert(10);
    int idx2 = map.insert(10);  // Duplicate

    EXPECT_EQ(idx1, idx2);
    EXPECT_EQ(map.length(), 1);
}

// Ported from: itwinjs-core core/bentley/src/test/IndexMap.test.ts
TEST(IndexMapTest, MaximumSize) {
    dqBase::IndexMap<int> map(compareInts, 2);

    int idx1 = map.insert(10);
    int idx2 = map.insert(20);
    int idx3 = map.insert(30);  // Should fail — map is full

    EXPECT_EQ(idx1, 0);
    EXPECT_EQ(idx2, 1);
    EXPECT_EQ(idx3, -1);
    EXPECT_TRUE(map.isFull());
}

// Ported from: itwinjs-core core/bentley/src/test/IndexMap.test.ts
TEST(IndexMapTest, ToArray) {
    dqBase::IndexMap<int> map(compareInts);

    map.insert(30);  // index 0
    map.insert(10);  // index 1
    map.insert(20);  // index 2

    auto arr = map.toArray();
    EXPECT_EQ(arr.size(), 3u);
    EXPECT_EQ(arr[0], 30);
    EXPECT_EQ(arr[1], 10);
    EXPECT_EQ(arr[2], 20);
}

// Ported from: itwinjs-core core/bentley/src/test/IndexMap.test.ts
TEST(IndexMapTest, SortedOrder) {
    dqBase::IndexMap<std::string> map(compareStrings);

    map.insert("banana");  // index 0
    map.insert("apple");   // index 1
    map.insert("cherry");  // index 2

    // indexOf finds by value, returns insertion index
    EXPECT_EQ(map.indexOf("apple"), 1);
    EXPECT_EQ(map.indexOf("banana"), 0);
    EXPECT_EQ(map.indexOf("cherry"), 2);
}

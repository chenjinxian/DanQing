// SPDX-License-Identifier: Apache-2.0
// dqBase tests — OrderedId64Iterable behavior verification
//
// 参考来源：
//   - itwinjs-core core/bentley/src/OrderedId64Iterable.ts (compare ref:32-42,
//     union/intersection/differenceIterator ref:141-255, areEqualSets ref:59-79,
//     uniqueIterator ref:102-115)
//   - itwinjs-core core/bentley/src/test/Id.test.ts
//     describe("OrderedId64Iterable", ...) ref:422-555
//
// Ported from: itwinjs-core core/bentley/src/test/Id.test.ts
//              describe("OrderedId64Iterable")
#include <gtest/gtest.h>

#include <dqBase/OrderedId64Iterable.h>
#include <dqBase/DqId.h>

#include <string>
#include <vector>

namespace {

// 将字符串 ID 列表转换为 DqIdVector（按 OrderedId64Iterable 的顺序约束保持原序）
dqBase::DqIdVector ToIdVec(std::initializer_list<const char*> strs) {
    dqBase::DqIdVector out;
    for (auto* s : strs) {
        auto id = dqBase::DqId::FromString(s);
        out.push_back(id);
    }
    return out;
}

// 将 DqIdVector 转换为 hex 字符串列表（用于断言顺序）
std::vector<std::string> ToHexStrings(const dqBase::DqIdVector& ids) {
    std::vector<std::string> out;
    for (auto id : ids) out.push_back(id.ToString());
    return out;
}

} // namespace

// ===========================================================================
// compare — length-then-lexicographic on the hex string form
// Ported from: itwinjs-core OrderedId64Iterable.ts compare (ref:32-42)
//              itwinjs-core core/bentley/src/test/Id.test.ts (implicitly via
//              union/intersection/difference ordering assertions)
// ===========================================================================
TEST(OrderedId64IterableTest, CompareStringForm_LengthFirstThenLexicographic) {
    // Ported from: itwinjs-core OrderedId64Iterable.ts:32-42
    //              export function compare(lhs, rhs): number
    //   if (lhs.length !== rhs.length)
    //     return lhs.length < rhs.length ? -1 : 1;
    //   if (lhs !== rhs)
    //     return lhs < rhs ? -1 : 1;
    //   return 0;

    using O = dqBase::OrderedId64Iterable;

    // --- 等长情况：纯字典序（hex 字符顺序 == 数值顺序，对等长 canonical 输入） ---
    EXPECT_EQ(0, O::CompareString("0x1", "0x1"));
    EXPECT_EQ(-1, O::CompareString("0x1", "0x2"));
    EXPECT_EQ(1, O::CompareString("0x2", "0x1"));
    EXPECT_EQ(-1, O::CompareString("0xa", "0xf"));
    EXPECT_EQ(1, O::CompareString("0xf", "0xa"));

    // --- 不等长情况：短串 < 长串（length 优先） ---
    EXPECT_EQ(-1, O::CompareString("0xa", "0xff"));   // length 1 vs 2
    EXPECT_EQ(1, O::CompareString("0xff", "0xa"));
    EXPECT_EQ(-1, O::CompareString("0xf", "0x1a"));   // length 1 vs 2 (diverges from
                                                       //   raw lex: "f" > "1...", but
                                                       //   length-first short wins)
    EXPECT_EQ(1, O::CompareString("0x1a", "0xf"));

    // 关键非规范输入（leading-zero / mixed-length）—— 这是 P1 #10 的真实分歧面。
    // ref compare 对字符串长度敏感，而非对数值敏感。
    EXPECT_EQ(-1, O::CompareString("0x9", "0x1a"));   // 9 vs 26: len 1 < len 2 → -1
    EXPECT_EQ(1, O::CompareString("0x1a", "0x9"));    // 反向 → 1
    // 与"裸数值 <"的分歧示例：字符串 "0x0a" (len 4) vs "0xff" (len 4) 等长按字典序；
    // 字符串 "0xa" (len 3) vs "0x0a" (len 4) → 长度优先：3 < 4 → -1（即使数值 0xa == 0x0a == 10）
    EXPECT_EQ(-1, O::CompareString("0xa", "0x0a"));   // length-first: 3 < 4
    EXPECT_EQ(1, O::CompareString("0x0a", "0xa"));
}

// DqId 重载：对 canonical 形式，length-then-lex == 数值序（验证向后兼容）
// Ported from: itwinjs-core core/bentley/src/test/Id.test.ts
//              TEST(OrderedId64IterableTest, CompareUeId_CanonicalMatchesNumeric)
TEST(OrderedId64IterableTest, CompareUeId_CanonicalMatchesNumeric) {
    using O = dqBase::OrderedId64Iterable;
    EXPECT_EQ(0, O::Compare(dqBase::DqId{1}, dqBase::DqId{1}));
    EXPECT_EQ(-1, O::Compare(dqBase::DqId{1}, dqBase::DqId{2}));
    EXPECT_EQ(1, O::Compare(dqBase::DqId{2}, dqBase::DqId{1}));
    // 数值上 0xff(255) > 0xa(10)，长度也 1 < 2，两算法一致 → 1
    EXPECT_EQ(-1, O::Compare(dqBase::DqId{0xa}, dqBase::DqId{0xff}));
    EXPECT_EQ(1, O::Compare(dqBase::DqId{0xff}, dqBase::DqId{0xa}));
    // 数值上 0xf(15) > 0x1a(26)? 不，0xf=15 < 0x1a=26；长度 1 < 2 也一致 → -1
    EXPECT_EQ(-1, O::Compare(dqBase::DqId{0xf}, dqBase::DqId{0x1a}));
}

// ===========================================================================
// sortArray — 按数值序（canonical 等价于 length-then-lex）
// Ported from: itwinjs-core OrderedId64Iterable.ts sortArray (ref:51-54)
// ===========================================================================
TEST(OrderedId64IterableTest, SortArray_NumericOrdering) {
    using O = dqBase::OrderedId64Iterable;
    dqBase::DqIdVector ids = ToIdVec({"0xa", "0x1", "0xff", "0x2", "0x1a"});
    O::SortArray(ids);
    // 预期顺序：1, 2, a, 1a, ff（数值序；canonical 长度序也一致）
    auto strs = ToHexStrings(ids);
    ASSERT_EQ(strs.size(), 5u);
    EXPECT_EQ(strs[0], "0x1");
    EXPECT_EQ(strs[1], "0x2");
    EXPECT_EQ(strs[2], "0xa");
    EXPECT_EQ(strs[3], "0x1a");
    EXPECT_EQ(strs[4], "0xff");
}

// ===========================================================================
// areEqualSets — 含重复元素时也应判定集合相等
// Ported from: itwinjs-core core/bentley/src/test/Id.test.ts
//              it("should determine set equality") ref:523-538
// ===========================================================================
TEST(OrderedId64IterableTest, AreEqualSets_DedupSemantics) {
    using O = dqBase::OrderedId64Iterable;
    // Ported from: itwinjs-core Id.test.ts:529
    //   [["1", "1", "1"], ["1"], true]
    EXPECT_TRUE(O::AreEqualSets(ToIdVec({"0x1", "0x1", "0x1"}),
                                 ToIdVec({"0x1"})));
    // Ported from: itwinjs-core Id.test.ts:530
    //   [["1", "2", "2", "3", "3", "3", "4"], ["1", "1", "1", "2", "2", "3", "4"], true]
    EXPECT_TRUE(O::AreEqualSets(
        ToIdVec({"0x1", "0x2", "0x2", "0x3", "0x3", "0x3", "0x4"}),
        ToIdVec({"0x1", "0x1", "0x1", "0x2", "0x2", "0x3", "0x4"})));
    // Ported from: itwinjs-core Id.test.ts:531
    //   [["1", "2", "2", "3", "3", "3", "4"], ["1", "1", "1", "2", "2", "4"], false]
    EXPECT_FALSE(O::AreEqualSets(
        ToIdVec({"0x1", "0x2", "0x2", "0x3", "0x3", "0x3", "0x4"}),
        ToIdVec({"0x1", "0x1", "0x1", "0x2", "0x2", "0x4"})));
    // 单元素不等
    EXPECT_FALSE(O::AreEqualSets(ToIdVec({"0x1"}), ToIdVec({"0x2"})));
    // 空 vs 空
    EXPECT_TRUE(O::AreEqualSets(ToIdVec({}), ToIdVec({})));
}

// ===========================================================================
// isEmptySet
// Ported from: itwinjs-core core/bentley/src/test/Id.test.ts ref:507-521
// ===========================================================================
TEST(OrderedId64IterableTest, IsEmptySet) {
    using O = dqBase::OrderedId64Iterable;
    EXPECT_TRUE(O::IsEmptySet(ToIdVec({})));
    EXPECT_FALSE(O::IsEmptySet(ToIdVec({"0x1"})));
}

// ===========================================================================
// unique — 去重
// Ported from: itwinjs-core core/bentley/src/test/Id.test.ts
//              it("should iterate unique Ids") ref:540-555
// ===========================================================================
TEST(OrderedId64IterableTest, Unique_DeduplicatesContiguous) {
    using O = dqBase::OrderedId64Iterable;
    // Ported from: itwinjs-core Id.test.ts:550
    //   [["1", "2", "2", "3", "3", "3", "4"], ["1", "2", "3", "4"]]
    auto out = O::Unique(ToIdVec({"0x1", "0x2", "0x2", "0x3", "0x3", "0x3", "0x4"}));
    auto strs = ToHexStrings(out);
    ASSERT_EQ(strs.size(), 4u);
    EXPECT_EQ(strs[0], "0x1");
    EXPECT_EQ(strs[1], "0x2");
    EXPECT_EQ(strs[2], "0x3");
    EXPECT_EQ(strs[3], "0x4");
}

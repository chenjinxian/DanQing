// SPDX-License-Identifier: Apache-2.0
// dqBase tests — CompressedId64Set / MutableCompressedId64Set 行为验证
//
// 参考来源：
//   - itwinjs-core core/bentley/src/CompressedId64Set.ts
//     compressIds/compactRange 编码 (ref:37-132) + MutableCompressedId64Set 懒缓冲
//     _inserted/_deleted 设计 (ref:375-498) + set ops computeUnion/Intersection/Difference
//     (ref:432-457) + lazy updateIds 合并算法 (ref:487-497)
//   - itwinjs-core core/bentley/src/OrderedId64Iterable.ts
//     unionIterator/intersectionIterator/differenceIterator (ref:141-255)
//
// Authored: no reference test exists in itwinjs-core for CompressedId64Set — assertions
//           derived directly from the reference source's encoding algorithm (compressIds)
//           and the lazy _inserted/_deleted merge semantics (updateIds). The TS source is
//           treated as spec per CLAUDE.md §2/§5.
#include <gtest/gtest.h>

#include <dqBase/CompressedId64Set.h>
#include <dqBase/DqId.h>

#include <string>

namespace {

// 将 DqId 数值直接喂进 CompressSet，便于紧凑断言
dqBase::DqString CompressValues(std::initializer_list<uint64_t> vals) {
    dqBase::DqIdSet set;
    for (auto v : vals) set.insert(dqBase::DqId{v});
    return dqBase::CompressedId64Set::CompressSet(set);
}

// 解压缩后转 DqIdVector（按值排序）
dqBase::DqIdVector DecompressValues(const dqBase::DqString& s) {
    auto set = dqBase::CompressedId64Set::DecompressSet(s);
    return dqBase::DqIdVector(set.begin(), set.end());
}

} // namespace

// ===========================================================================
// compressIds 编码 — ported from itwinjs-core CompressedId64Set.ts compressIds (ref:89-132)
// ===========================================================================

// Authored: no reference test exists — derived from compressIds/compactRange algorithm
// 单 ID: compactRange(inc, 1) 退化为 "+inc"（length<=1 不输出 *LEN）
TEST(CompressedId64SetTest, CompressSingleIdEmitsIncrementOnly) {
    EXPECT_EQ(CompressValues({0x1}), "+1");
    EXPECT_EQ(CompressValues({0x10}), "+10");
    EXPECT_EQ(CompressValues({0xff}), "+FF");
}

// Authored: no reference test exists — compressIds 连续相同增量构成 range
//   prev 初值 0；inc = curId − prev。
//   {1,2,3}: 全部 inc=1 → "+1*3"
//   {2,4,6}: 首个 inc=2（2−0），随后 inc=2, inc=2 → "+2*3"
//   {1,3,5}: 首个 inc=1（1−0），随后 inc=2, inc=2 → "+1+2*2"
//   {1,2,4}: 首个 inc=1，随后 inc=1, inc=2 → "+1*2+2"
TEST(CompressedId64SetTest, CompressContiguousRunEmitsIncrementTimesLength) {
    EXPECT_EQ(CompressValues({0x1, 0x2, 0x3}), "+1*3");
    EXPECT_EQ(CompressValues({0x2, 0x4, 0x6}), "+2*3");
    EXPECT_EQ(CompressValues({0x1, 0x3, 0x5}), "+1+2*2");
    EXPECT_EQ(CompressValues({0x1, 0x2, 0x4}), "+1*2+2");
}

// Authored: no reference test exists — compressIds 第一个 ID 的 inc 等于 ID 自身（prevId 初值 0）
// {5,6,7}: 首个 inc=5（从 0 到 5），随后两次 inc=1 → "+5+1*2"
TEST(CompressedId64SetTest, CompressFirstIncrementIsIdValueFromZero) {
    EXPECT_EQ(CompressValues({0x5, 0x6, 0x7}), "+5+1*2");
}

// Authored: no reference test exists — compressIds 忽略无效 ID（0）并按数值排序去重
// DqIdSet 已是排序去重的 bset，非法值 0 不会出现（默认构造无效）
TEST(CompressedId64SetTest, CompressIgnoresDuplicates) {
    dqBase::DqIdSet s;
    s.insert(dqBase::DqId{0x1});
    s.insert(dqBase::DqId{0x1}); // 重复
    s.insert(dqBase::DqId{0x2});
    EXPECT_EQ(dqBase::CompressedId64Set::CompressSet(s), "+1*2");
}

// ===========================================================================
// decompress / recompress 往返 — compressIds 与 iterator 互为逆运算 (ref:215-305, 325-349)
// ===========================================================================

// Authored: no reference test exists — round-trip
TEST(CompressedId64SetTest, DecompressRecompressIsIdentity) {
    const auto original = CompressValues({0x1, 0x2, 0x3, 0x10, 0x14});
    const auto vec = DecompressValues(original);
    dqBase::DqIdSet roundTripped(vec.begin(), vec.end());
    EXPECT_EQ(dqBase::CompressedId64Set::CompressSet(roundTripped), original);
}

// Authored: no reference test exists — empty set compresses to empty string
TEST(CompressedId64SetTest, EmptySetCompressesToEmptyString) {
    dqBase::DqIdSet empty;
    EXPECT_EQ(dqBase::CompressedId64Set::CompressSet(empty), "");
    EXPECT_TRUE(dqBase::CompressedId64Set::DecompressSet("").empty());
}

// ===========================================================================
// MutableCompressedId64Set 懒 _inserted/_deleted 缓冲 — ported from itwinjs-core
// CompressedId64Set.ts MutableCompressedId64Set (ref:375-498)
//
// 关键不变量（ref:391-400, 487-497）：
//   - add(id): 从 _deleted 移除 id，向 _inserted 插入 id（不立即重算 _ids 字符串）
//   - delete(id): 从 _inserted 移除 id，向 _deleted 插入 id（不立即重算）
//   - updateIds(): _ids = compressIds(union(decompress(_ids) - _deleted, _inserted))
//   - 所有公开方法（add/delete 除外）都触发 updateIds()
// ===========================================================================

// Authored: no reference test exists — derived from MutableCompressedId64Set.add/updateIds
// 构造空集合，添加单个 ID 后 GetIds() 触发懒合并，结果等于 compressIds({id})
TEST(MutableCompressedId64SetTest, AddThenGetIdsProducesCompressedSingle) {
    dqBase::MutableCompressedId64Set s;
    s.add(dqBase::DqId{0x1});
    EXPECT_EQ(s.GetIds(), "+1");
}

// Authored: no reference test exists — 添加多个连续 ID，updateIds 合并后是单条 range
TEST(MutableCompressedId64SetTest, AddContiguousRunProducesOneRange) {
    dqBase::MutableCompressedId64Set s;
    s.add(dqBase::DqId{0x1});
    s.add(dqBase::DqId{0x2});
    s.add(dqBase::DqId{0x3});
    EXPECT_EQ(s.GetIds(), "+1*3");
}

// Authored: no reference test exists — 初始化压缩集合 + 添加新 ID，结果等于整体 compressIds
TEST(MutableCompressedId64SetTest, AddToNonEmptyBaseMergesToCompressedForm) {
    // base = compressIds({1,2,3}) = "+1*3"
    dqBase::MutableCompressedId64Set s{"+1*3"};
    // 添加 0x4 → updateIds: decompress(base) ∪ {0x4} = {1,2,3,4} → "+1*4"
    s.add(dqBase::DqId{0x4});
    EXPECT_EQ(s.GetIds(), "+1*4");
    // 添加 0x10（间断）→ {1,2,3,4,16} → "+1*4+C"
    s.add(dqBase::DqId{0x10});
    EXPECT_EQ(s.GetIds(), "+1*4+C");
}

// Authored: no reference test exists — delete(id) 在 updateIds 时从 base 移除
TEST(MutableCompressedId64SetTest, DeleteFromBaseRemovesId) {
    // base = compressIds({1,2,3}) = "+1*3"
    dqBase::MutableCompressedId64Set s{"+1*3"};
    s.Delete(dqBase::DqId{0x2});
    // {1,3} → inc 1 (一次), inc 2 (一次) → "+1+2"
    EXPECT_EQ(s.GetIds(), "+1+2");
}

// Authored: no reference test exists — re-add a deleted id（懒缓冲的关键）
// add(2) → delete(2) → add(2)：最终 updateIds 应再次包含 2
TEST(MutableCompressedId64SetTest, ReAddingDeletedIdRestoresIt) {
    dqBase::MutableCompressedId64Set s{"+1*3"};  // {1,2,3}
    s.Delete(dqBase::DqId{0x2});                  // → {1,3}
    s.add(dqBase::DqId{0x2});                     // → {1,2,3}
    EXPECT_EQ(s.GetIds(), "+1*3");
}

// Authored: no reference test exists — delete a non-present id 是 no-op
TEST(MutableCompressedId64SetTest, DeletingNonPresentIdIsNoop) {
    dqBase::MutableCompressedId64Set s{"+1*3"};  // {1,2,3}
    s.Delete(dqBase::DqId{0xff});                 // 不在集合中
    EXPECT_EQ(s.GetIds(), "+1*3");
}

// Authored: no reference test exists — add a present id 是 no-op（去重）
TEST(MutableCompressedId64SetTest, AddingPresentIdIsNoop) {
    dqBase::MutableCompressedId64Set s{"+1*3"};  // {1,2,3}
    s.add(dqBase::DqId{0x2});                     // 已存在
    EXPECT_EQ(s.GetIds(), "+1*3");
}

// Authored: no reference test exists — add/delete/add cycles
// 多轮 add/delete 后 updateIds 合并结果必须等于直接 compressIds 的结果
TEST(MutableCompressedId64SetTest, AddDeleteCyclesProduceCanonicalForm) {
    dqBase::MutableCompressedId64Set s;  // 空
    s.add(dqBase::DqId{0x1});
    s.add(dqBase::DqId{0x2});
    s.add(dqBase::DqId{0x3});
    s.Delete(dqBase::DqId{0x2});
    s.add(dqBase::DqId{0x5});
    s.Delete(dqBase::DqId{0x1});
    s.add(dqBase::DqId{0x2});
    // 最终集合 = {2, 3, 5} → compressIds = "+2+1+2"?
    //   prev=0; id=2: inc=2 (rangeLen=1)
    //   id=3: inc=1 ≠ 2 → flush "+2", new range inc=1 (rangeLen=1)
    //   id=5: inc=2 ≠ 1 → flush "+1", new range inc=2 (rangeLen=1)
    //   end: flush "+2"
    //   = "+2+1+2"
    EXPECT_EQ(s.GetIds(), "+2+1+2");
}

// Authored: no reference test exists — clear() 清空 base + inserted + deleted
TEST(MutableCompressedId64SetTest, ClearEmptiesEverything) {
    dqBase::MutableCompressedId64Set s{"+1*3"};
    s.add(dqBase::DqId{0x10});
    s.Delete(dqBase::DqId{0x2});
    s.clear();
    EXPECT_EQ(s.GetIds(), "");
    EXPECT_TRUE(s.isEmpty());
}

// Authored: no reference test exists — reset() 清空 + 重新初始化
TEST(MutableCompressedId64SetTest, ResetReinitializes) {
    dqBase::MutableCompressedId64Set s{"+1*3"};
    s.add(dqBase::DqId{0xff});
    s.Reset("+2*2");  // compressIds({2,4}) = "+2*2"
    EXPECT_EQ(s.GetIds(), "+2*2");
}

// ===========================================================================
// set ops — ported from MutableCompressedId64Set.computeUnion/Intersection/Difference
// (ref:432-457). DanQing currently exposes Compute* on the CompressedId64Set static class
// taking (a, b); this test exercises that surface.
// ===========================================================================

// Authored: no reference test exists — computeUnion of two compressed sets
TEST(CompressedId64SetTest, ComputeUnion) {
    // a = {1,2,3}, b = {3,4,5} → union = {1,2,3,4,5} → "+1*5"
    dqBase::DqString a = CompressValues({0x1, 0x2, 0x3});
    dqBase::DqString b = CompressValues({0x3, 0x4, 0x5});
    EXPECT_EQ(dqBase::CompressedId64Set::ComputeUnion(a, b), "+1*5");
}

// Authored: no reference test exists — computeIntersection
TEST(CompressedId64SetTest, ComputeIntersection) {
    dqBase::DqString a = CompressValues({0x1, 0x2, 0x3, 0x4});
    dqBase::DqString b = CompressValues({0x2, 0x4, 0x6});
    // {2, 4} → "+2*2"
    EXPECT_EQ(dqBase::CompressedId64Set::ComputeIntersection(a, b), "+2*2");
}

// Authored: no reference test exists — computeDifference (a − b)
TEST(CompressedId64SetTest, ComputeDifference) {
    dqBase::DqString a = CompressValues({0x1, 0x2, 0x3, 0x4});
    dqBase::DqString b = CompressValues({0x2, 0x4, 0x6});
    // {1, 3} → "+1+2"
    EXPECT_EQ(dqBase::CompressedId64Set::ComputeDifference(a, b), "+1+2");
}

// Authored: no reference test exists — isEmpty
TEST(CompressedId64SetTest, isEmpty) {
    EXPECT_TRUE(dqBase::CompressedId64Set::isEmpty(""));
    EXPECT_FALSE(dqBase::CompressedId64Set::isEmpty("+1*3"));
}

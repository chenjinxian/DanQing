// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/CompressedId64Set.ts
// DanQing dqBase — 压缩 ID 集合
//
// 1:1 对齐 itwinjs-core CompressedId64Set namespace + MutableCompressedId64Set class.
//   - compressIds/compactRange: run-length 编码 "+inc*LEN..."（ref:37-132）
//   - iterator/iterable: 逆向解析压缩串（ref:215-314）
//   - MutableCompressedId64Set: 懒 _inserted/_deleted 缓冲（ref:375-498）
//     add(id) → 从 _deleted 移除、向 _inserted 插入；delete(id) 反之
//     updateIds() → _ids = compressIds(union(decompress(_ids) − _deleted, _inserted))
#pragma once

#include "Export.h"
#include "DqId.h"
#include "DqTypes.h"

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// CompressedId64Set — 压缩 ID 集合（字符串形式，free-function 接口）
// Ported from: itwinjs-core CompressedId64Set.ts (namespace, ref:26-350)
//
// 编码格式（ref:37-45 compactRange, ref:89-132 compressIds）：
//   "<+inc[*LEN]><+inc[*LEN]>..."
//   - 首个 inc 从 0 算起，因此首段 inc 等于第一个 ID 的数值
//   - inc 为大写十六进制（不含前导零）
//   - 当且仅当某段 inc 连续重复 >= 2 次时输出 "*LEN"（LEN 大写十六进制）
//   - 单元素集合: "+inc"
//   - 空集合: ""
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT CompressedId64Set {
public:
    /// 压缩 ID 集合为字符串（ref: compressSet/compressArray/compressIds）
    static DqString CompressSet(const DqIdSet& ids);

    /// 排序并压缩（与 CompressSet 等价，DqIdSet 已是排序 bset）
    static DqString SortAndCompress(const DqIdSet& ids);

    /// 解压缩为 ID 集合（ref: decompressSet）
    static DqIdSet DecompressSet(const DqString& compressed);

    /// 解压缩为 ID 数组（ref: decompressArray）
    static DqIdVector DecompressArray(const DqString& compressed);

    /// 检查压缩集合是否为空（ref: isEmptySet 对压缩串的特化）
    static bool isEmpty(const DqString& compressed);

    /// 计算两个压缩集合的并集（ref: unionIterator + compressIds）
    static DqString ComputeUnion(const DqString& a, const DqString& b);

    /// 计算两个压缩集合的交集（ref: intersectionIterator + compressIds）
    static DqString ComputeIntersection(const DqString& a, const DqString& b);

    /// 计算两个压缩集合的差集 a − b（ref: differenceIterator + compressIds）
    static DqString ComputeDifference(const DqString& a, const DqString& b);
};

// ---------------------------------------------------------------------------
// MutableCompressedId64Set — 可变压缩 ID 集合
// Ported from: itwinjs-core CompressedId64Set.ts MutableCompressedId64Set (ref:375-498)
//
// 设计：内部维护三个缓冲
//   - m_ids      : 当前已合并的压缩串
//   - m_inserted : 待合并的插入集合（OrderedId64Array 等价，bset<DqId> 排序）
//   - m_deleted  : 待合并的删除集合
//
// add(id)    → m_deleted.erase(id); m_inserted.insert(id)
// delete(id) → m_inserted.erase(id); m_deleted.insert(id)
// updateIds()（懒合并，由 add/delete 之外的所有公开方法触发）:
//   m_ids = compressIds(union(decompress(m_ids) − m_deleted, m_inserted))
//   m_inserted.clear(); m_deleted.clear();
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT MutableCompressedId64Set {
public:
    MutableCompressedId64Set() = default;
    explicit MutableCompressedId64Set(const DqString& compressed);

    /// 获取压缩字符串（触发懒合并）
    /// Ported from: itwinjs-core MutableCompressedId64Set.get ids (ref:386-389)
    DqString GetIds();

    /// 获取压缩字符串（const 视图，触发可变的懒合并）
    DqString GetIds() const;

    /// 是否为空（ref:386-389 + updateIds + OrderedId64Iterable.isEmptySet）
    bool isEmpty();

    /// 添加 ID（ref:391-400）
    void add(DqId id);

    /// 删除 ID（ref:402-411）
    void Delete(DqId id);

    /// 清空（ref:413-418）
    void clear();

    /// 重置为指定压缩集合（ref:421-424）
    void Reset(const DqString& compressed);

    /// 与另一压缩集合的并集，写入本对象（ref:432-439 computeUnion）
    void ComputeUnion(const DqString& other);

    /// 与另一压缩集合的交集，写入本对象（ref:442-449 computeIntersection）
    void ComputeIntersection(const DqString& other);

    /// 与另一压缩集合的差集，写入本对象（ref:452-457 computeDifference）
    void ComputeDifference(const DqString& other);

    /// 比较（ref:467-481 equals）
    bool equals(const DqString& other);

private:
    // 懒合并：触发后 m_inserted/m_deleted 清空，m_ids 重算（ref:487-497 updateIds）
    void UpdateIds();

    DqString m_ids;                // 已合并的压缩串
    DqIdSet  m_inserted;           // 待合并插入缓冲（bset，等价 OrderedId64Array）
    DqIdSet  m_deleted;            // 待合并删除缓冲
};

END_DQ_BASE_NAMESPACE

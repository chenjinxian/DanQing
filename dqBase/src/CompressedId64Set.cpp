// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/CompressedId64Set.ts
// DanQing dqBase — 压缩 ID 集合实现
//
// 1:1 对齐 itwinjs-core：
//   - compactRange (ref:37-45): "+inc" 或 "+inc*LEN"
//   - compressIds  (ref:89-132): run-length 编码，首段 inc 从 0 起算
//   - iterator     (ref:215-305): 逆向解析
//   - MutableCompressedId64Set.add/delete/updateIds (ref:391-497)
//   - OrderedId64Iterable.union/intersection/differenceIterator (ref:141-255)
#include "dqBase/CompressedId64Set.h"
#include "dqBase/BeAssert.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

BEGIN_DQ_BASE_NAMESPACE

namespace {

// 大写十六进制转字符串（无前导零），ref 等价: Uint64.toString (ref:197-205)
DqString ToUpperHex(uint64_t v) {
    if (v == 0) return "0";
    char buf[17];
    int pos = 16;
    buf[16] = '\0';
    while (v > 0 && pos > 0) {
        const int nibble = static_cast<int>(v & 0xF);
        buf[--pos] = static_cast<char>(nibble < 10 ? '0' + nibble : 'A' + (nibble - 10));
        v >>= 4;
    }
    return DqString(buf + pos);
}

// Ported from: itwinjs-core CompressedId64Set.ts compactRange (ref:37-45)
//   length <= 1  → "+inc"
//   length >= 2  → "+inc*LEN" (LEN 大写十六进制)
DqString CompactRange(uint64_t increment, size_t length) {
    DqAssert(length > 0);
    DqString inc = "+" + ToUpperHex(increment);
    if (length <= 1) return inc;
    return inc + "*" + ToUpperHex(static_cast<uint64_t>(length));
}

// 解析大写十六进制数字字符 [0-9A-F]，返回是否为合法十六进制
// Ported from: itwinjs-core CompressedId64Set.ts isHexDigit (ref:27-35) — 注 ref 用大写 [A..F]
bool IsHexDigit(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F');
}

// 从 16 进制字符解析回数值（ref:235: ch >= 65 ? ch - 65 + 10 : ch - 48）
uint32_t ParseHexChar(char ch) {
    if (ch >= 'A' && ch <= 'F') return static_cast<uint32_t>(ch - 'A' + 10);
    return static_cast<uint32_t>(ch - '0');
}

} // namespace

// ---------------------------------------------------------------------------
// compressIds — 内部辅助：传入**已排序、去重**的 DqIdVector，输出压缩串。
// Ported from: itwinjs-core CompressedId64Set.ts compressIds (ref:89-132)
// ---------------------------------------------------------------------------
// 注：ref 用 Uint64 比较 prevId 与 curId 维护排序不变量；这里 m_value 是 uint64_t，
// 严格递增检查（cmp > 0 表示 cur < prev → 未排序 → DqAssert）。
static DqString CompressIds(const DqIdVector& ids) {
    DqString str;
    uint64_t prevId = 0;
    uint64_t rangeIncrement = 0;
    size_t rangeLen = 0;

    for (const auto& idEntry : ids) {
        const uint64_t curId = idEntry.GetValue();
        // ref:102-103: 忽略无效 Id（"0"）。DqId.isValid() == (m_value != 0)
        if (!idEntry.isValid()) continue;

        // ref:108-112: 严格递增校验
        if (curId == prevId) {
            continue; // ref:109-110: 忽略重复
        }
        DqAssert(curId > prevId && "CompressIds requires a sorted, deduplicated array");
        if (curId < prevId) continue; // 防御性：no-exceptions 模式下不破坏不变量

        const uint64_t curIncrement = curId - prevId; // ref:106 curIncrement = cur - prev
        prevId = curId;

        if (rangeLen == 0) {
            // ref:116-118: 首段
            rangeIncrement = curIncrement;
            rangeLen = 1;
        } else if (curIncrement == rangeIncrement) {
            // ref:119-120: 续接当前 range
            ++rangeLen;
        } else {
            // ref:121-124: flush 旧 range，开新 range
            str += CompactRange(rangeIncrement, rangeLen);
            rangeIncrement = curIncrement;
            rangeLen = 1;
        }
    }

    if (rangeLen > 0) { // ref:128-129
        str += CompactRange(rangeIncrement, rangeLen);
    }
    return str;
}

// ---------------------------------------------------------------------------
// iterator — 逆向解析压缩串为 DqIdVector
// Ported from: itwinjs-core CompressedId64Set.ts iterator (ref:215-305)
// ---------------------------------------------------------------------------
static DqIdVector IterateIds(const DqString& ids) {
    DqIdVector result;
    if (ids.empty()) return result; // ref:216-217

    // ref:219-220: 必须以 '+' 开头
    DqAssert(!ids.empty() && ids[0] == '+');
    if (ids.empty() || ids[0] != '+') return result;

    size_t curIndex = 1; // ref:222 跳过前导 '+'
    uint64_t curId = 0;  // ref:223 curId 初值 0

    // ref:275-304: 主循环 — 解析 increment + optional *multiplier，逐步累加 curId
    while (curIndex < ids.size()) {
        // ref:225-241 parseUint32: 读取最多 8 个十六进制字符
        auto parseUint32 = [&]() -> uint32_t {
            uint32_t value = 0;
            int nChars = 0;
            while (curIndex < ids.size() && nChars < 8) {
                ++nChars;
                char ch = ids[curIndex];
                if (!IsHexDigit(ch)) break;
                value <<= 4;
                value |= ParseHexChar(ch);
                ++curIndex;
            }
            return value;
        };

        // ref:243-273 parseUint64: ref 用低/高 32 位拆分绕过 JS Number 精度问题。
        // C++ 原生 uint64_t，直接读最多 16 个 nibble 拼成 uint64_t（语义等价）。
        auto parseUint64 = [&]() -> uint64_t {
            size_t start = curIndex;
            uint32_t first = parseUint32(); // 先读最多 8 位
            size_t nFirst = curIndex - start;
            DqAssert(nFirst <= 8);

            if (nFirst == 8 && curIndex < ids.size() && IsHexDigit(ids[curIndex])) {
                // ref:254-266: 还有更多位可读 — 直接左移 first 后补上后续 nibbles
                uint64_t value = static_cast<uint64_t>(first);
                int extra = 0;
                while (curIndex < ids.size() && extra < 8 && IsHexDigit(ids[curIndex])) {
                    value = (value << 4) | ParseHexChar(ids[curIndex]);
                    ++curIndex;
                    ++extra;
                }
                return value;
            }
            return static_cast<uint64_t>(first);
        };

        // ref:276-298: parse increment + optional *multiplier
        uint64_t increment = parseUint64();
        DqAssert(increment != 0); // ref:279-280: 0 增量非法
        if (increment == 0) return result;

        size_t multiplier = 1;
        if (curIndex < ids.size()) {
            char sep = ids[curIndex++];
            if (sep == '*') {
                // ref:284-287: 解析 multiplier
                uint32_t m = parseUint32();
                DqAssert(m != 0); // ref:286-287
                if (m == 0) return result;
                multiplier = static_cast<size_t>(m);
                // ref:289-290: multiplier 后必须紧跟 '+'（或串尾）
                if (curIndex != ids.size() && ids[curIndex++] != '+') {
                    return result;
                }
            } else if (sep != '+') {
                // ref:296: 非法分隔符
                DqAssert(false && "Invalid CompressedId64Set separator");
                return result;
            }
            // sep == '+': ref:293-294 继续下一段
        }

        // ref:300-303: 累加 increment，产出 multiplier 个 ID
        for (size_t i = 0; i < multiplier; ++i) {
            curId += increment;
            result.push_back(DqId{curId});
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// OrderedId64Iterable — union/intersection/differenceIterator
// Ported from: itwinjs-core OrderedId64Iterable.ts (ref:141-255)
// 输入输出均为已排序去重向量
// ---------------------------------------------------------------------------
static DqIdVector UnionIds(const DqIdVector& a, const DqIdVector& b) {
    DqIdVector result;
    size_t i = 0, j = 0;
    DqId prev{0};
    bool havePrev = false;
    while (i < a.size() || j < b.size()) {
        DqId next{0};
        bool have;
        if (i >= a.size()) {
            next = b[j++]; have = true;
        } else if (j >= b.size()) {
            next = a[i++]; have = true;
        } else {
            const uint64_t l = a[i].GetValue();
            const uint64_t r = b[j].GetValue();
            if (l <= r) {
                next = a[i++];
                if (l == r) ++j; // ref:169-170: 同时前进去重
            } else {
                next = b[j++];
            }
            have = true;
        }
        if (!have) break;
        if (havePrev && prev == next) continue; // ref:177-178
        prev = next; havePrev = true;
        result.push_back(next);
    }
    return result;
}

static DqIdVector IntersectIds(const DqIdVector& a, const DqIdVector& b) {
    DqIdVector result;
    size_t i = 0, j = 0;
    DqId prev{0};
    bool havePrev = false;
    while (i < a.size() && j < b.size()) {
        DqId left = a[i++];
        if (havePrev && left == prev) continue; // ref:198-199
        prev = left; havePrev = true;
        uint64_t r = b[j].GetValue();
        while (left.GetValue() > r) {
            ++j;
            if (j >= b.size()) return result;
            r = b[j].GetValue();
        }
        if (left.GetValue() == r) result.push_back(left); // ref:214-215
    }
    return result;
}

static DqIdVector DifferenceIds(const DqIdVector& a, const DqIdVector& b) {
    DqIdVector result;
    size_t i = 0, j = 0;
    DqId prev{0};
    bool havePrev = false;
    while (i < a.size()) {
        DqId left = a[i++];
        if (havePrev && left == prev) continue; // ref:232-233
        if (j >= b.size()) {
            prev = left; havePrev = true;
            result.push_back(left); // ref:234-236
            continue;
        }
        uint64_t r = b[j].GetValue();
        while (left.GetValue() > r && j < b.size()) {
            ++j;
            if (j >= b.size()) {
                prev = left; havePrev = true;
                result.push_back(left); // ref:243-245
                break;
            }
            r = b[j].GetValue();
        }
        if (j >= b.size()) continue;
        if (left.GetValue() < r) {
            result.push_back(left); // ref:252-253
        }
        prev = left; havePrev = true;
    }
    return result;
}

// ===========================================================================
// CompressedId64Set 公开静态方法
// ===========================================================================

DqString CompressedId64Set::CompressSet(const DqIdSet& ids) {
    // ref:53-55 compressSet → sortAndCompress；DqIdSet 已是排序 bset
    DqIdVector sorted(ids.begin(), ids.end());
    // bset 迭代器已按 < 排序，但显式 sort 保证与 ref sortArray 一致
    std::sort(sorted.begin(), sorted.end());
    return CompressIds(sorted);
}

DqString CompressedId64Set::SortAndCompress(const DqIdSet& ids) {
    return CompressSet(ids);
}

DqIdSet CompressedId64Set::DecompressSet(const DqString& compressed) {
    // ref:325-331 decompressSet
    DqIdSet set;
    for (const auto& id : IterateIds(compressed)) {
        set.insert(id);
    }
    return set;
}

DqIdVector CompressedId64Set::DecompressArray(const DqString& compressed) {
    // ref:343-349 decompressArray
    return IterateIds(compressed);
}

bool CompressedId64Set::isEmpty(const DqString& compressed) {
    // ref:85-90 isEmptySet 对 string 的特化
    return compressed.empty();
}

DqString CompressedId64Set::ComputeUnion(const DqString& a, const DqString& b) {
    // ref: unionIterator(a, b) → compressIds
    DqIdVector av = IterateIds(a);
    DqIdVector bv = IterateIds(b);
    return CompressIds(UnionIds(av, bv));
}

DqString CompressedId64Set::ComputeIntersection(const DqString& a, const DqString& b) {
    DqIdVector av = IterateIds(a);
    DqIdVector bv = IterateIds(b);
    return CompressIds(IntersectIds(av, bv));
}

DqString CompressedId64Set::ComputeDifference(const DqString& a, const DqString& b) {
    DqIdVector av = IterateIds(a);
    DqIdVector bv = IterateIds(b);
    return CompressIds(DifferenceIds(av, bv));
}

// ===========================================================================
// MutableCompressedId64Set — 懒 _inserted/_deleted 缓冲
// Ported from: itwinjs-core CompressedId64Set.ts MutableCompressedId64Set (ref:375-498)
// ===========================================================================

MutableCompressedId64Set::MutableCompressedId64Set(const DqString& compressed)
    : m_ids(compressed) {} // ref:381-383

void MutableCompressedId64Set::UpdateIds() {
    // ref:487-497 updateIds
    if (m_inserted.empty() && m_deleted.empty()) return; // ref:483-485, 488-489

    // ref:491: difference = decompress(m_ids) − m_deleted
    DqIdVector base = IterateIds(m_ids);
    DqIdVector del(m_deleted.begin(), m_deleted.end());
    DqIdVector diff = DifferenceIds(base, del);

    // ref:492-493: union(diff, m_inserted)
    DqIdVector ins(m_inserted.begin(), m_inserted.end());
    DqIdVector merged = UnionIds(diff, ins);

    // ref:493: m_ids = compressIds(union)
    m_ids = CompressIds(merged);

    // ref:495-496
    m_inserted.clear();
    m_deleted.clear();
}

DqString MutableCompressedId64Set::GetIds() {
    UpdateIds();
    return m_ids;
}

DqString MutableCompressedId64Set::GetIds() const {
    // const 视图：const_cast 委派给非 const 版本（懒合并不影响逻辑 const 性）
    return const_cast<MutableCompressedId64Set*>(this)->GetIds();
}

bool MutableCompressedId64Set::isEmpty() {
    return GetIds().empty(); // ref:460-462 isEmpty
}

void MutableCompressedId64Set::add(DqId id) {
    // ref:394-400
    if (!id.isValid()) {
        DqAssert(false && "MutableCompressedId64Set.add: invalid Id");
        return;
    }
    m_deleted.erase(id);
    m_inserted.insert(id);
}

void MutableCompressedId64Set::Delete(DqId id) {
    // ref:405-411
    if (!id.isValid()) {
        DqAssert(false && "MutableCompressedId64Set.Delete: invalid Id");
        return;
    }
    m_inserted.erase(id);
    m_deleted.insert(id);
}

void MutableCompressedId64Set::clear() {
    // ref:414-418
    m_ids.clear();
    m_inserted.clear();
    m_deleted.clear();
}

void MutableCompressedId64Set::Reset(const DqString& compressed) {
    // ref:421-424 reset
    clear();
    m_ids = compressed;
}

void MutableCompressedId64Set::ComputeUnion(const DqString& other) {
    // ref:432-439: updateIds then merge
    UpdateIds();
    m_ids = CompressedId64Set::ComputeUnion(m_ids, other);
}

void MutableCompressedId64Set::ComputeIntersection(const DqString& other) {
    // ref:442-449
    UpdateIds();
    m_ids = CompressedId64Set::ComputeIntersection(m_ids, other);
}

void MutableCompressedId64Set::ComputeDifference(const DqString& other) {
    // ref:452-457
    UpdateIds();
    m_ids = CompressedId64Set::ComputeDifference(m_ids, other);
}

bool MutableCompressedId64Set::equals(const DqString& other) {
    // ref:467-481
    UpdateIds();
    return m_ids == other;
}

END_DQ_BASE_NAMESPACE

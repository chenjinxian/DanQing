// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/Primitives.ts
// DanQing dqRender — ToleranceRatio / GeometryOptions / Triangle / TriangleList / TriangleKey / TriangleSet
//
// 保真依据：逐类型移植 Primitives.ts。TriangleSet 参考 extends SortedArray<TriangleKey>（core-bentley），
// DanQing 用自维护有序 vector + 二分查找复刻 insertKey 语义（按 TriangleKey::compare 排序、去重、新插入时
// 回调）。@internal，仅 dqRender 内部消费（MeshBuilder triangle 去重，PR E）。
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 ToleranceRatio (Primitives.ts:12-15).
namespace ToleranceRatio {
inline constexpr double vertex = 0.1;
inline constexpr double facetArea = 0.1;
}  // namespace ToleranceRatio

// 1:1 GeometryOptions (Primitives.ts:18-21).
struct GeometryOptions {
    bool wantEdges = false;
    bool preserveOrder = false;
};

// 1:1 Triangle (Primitives.ts:23-48).
class Triangle {
public:
    explicit Triangle(bool singleSided = true) : m_singleSided(singleSided) {}

    void setIndices(uint32_t a, uint32_t b, uint32_t c) {
        m_indices[0] = a; m_indices[1] = b; m_indices[2] = c;
    }
    void setEdgeVisibility(bool a, bool b, bool c) {
        m_visible[0] = a; m_visible[1] = b; m_visible[2] = c;
    }
    bool isEdgeVisible(size_t index) const { return m_visible[index]; }
    bool isDegenerate() const {
        return m_indices[0] == m_indices[1] || m_indices[0] == m_indices[2] || m_indices[1] == m_indices[2];
    }

    uint32_t index(size_t i) const { return m_indices[i]; }
    bool singleSided() const { return m_singleSided; }
    void setSingleSided(bool v) { m_singleSided = v; }

private:
    uint32_t m_indices[3] = {0, 0, 0};
    bool m_visible[3] = {true, true, true};
    bool m_singleSided;
};

// 1:1 TriangleList (Primitives.ts:51-98) — packs triangle indices (3 per triangle) + a per-triangle
// visibility/singleSided flag byte (bit 0 = singleSided; bits 1..3 = edge visibility).
class TriangleList {
public:
    size_t length() const { return m_flags.size(); }
    bool isEmpty() const { return m_flags.empty(); }
    const std::vector<uint32_t>& indices() const { return m_indices; }
    const std::vector<uint16_t>& flags() const { return m_flags; }

    void addTriangle(const Triangle& triangle) {
        uint16_t flags = triangle.singleSided() ? 1 : 0;
        for (size_t i = 0; i < 3; ++i) {
            if (triangle.isEdgeVisible(i))
                flags |= static_cast<uint16_t>(0x0002 << i);
            m_indices.push_back(triangle.index(i));
        }
        m_flags.push_back(flags);
    }

    void addFromTypedArray(const uint32_t* indices, size_t count, uint16_t flags = 0) {
        for (size_t i = 0; i < count;) {
            m_indices.push_back(indices[i++]);
            m_indices.push_back(indices[i++]);
            m_indices.push_back(indices[i++]);
            m_flags.push_back(flags);
        }
    }

    Triangle getTriangle(size_t index) const {
        Triangle t;
        const uint16_t flags = m_flags[index];
        t.setSingleSided(0 != (flags & 0x0001));
        const size_t baseIndex = index * 3;
        t.setIndices(m_indices[baseIndex], m_indices[baseIndex + 1], m_indices[baseIndex + 2]);
        t.setEdgeVisibility(0 != (flags & 0x0002), 0 != (flags & 0x0004), 0 != (flags & 0x0008));
        return t;
    }

private:
    std::vector<uint16_t> m_flags;
    std::vector<uint32_t> m_indices;
};

// 1:1 TriangleKey (Primitives.ts:101-154) — the triangle's 3 indices sorted ascending, for dedup.
class TriangleKey {
public:
    explicit TriangleKey(const Triangle& triangle) {
        const uint32_t i0 = triangle.index(0), i1 = triangle.index(1), i2 = triangle.index(2);
        if (i0 < i1) {
            if (i0 < i2) { m_sorted[0] = i0; sortPair(i1, i2); }
            else { m_sorted[0] = i2; m_sorted[1] = i0; m_sorted[2] = i1; }
        } else {
            if (i1 < i2) { m_sorted[0] = i1; sortPair(i0, i2); }
            else { m_sorted[0] = i2; m_sorted[1] = i1; m_sorted[2] = i0; }
        }
    }

    int compare(const TriangleKey& rhs) const {
        for (size_t i = 0; i < 3; ++i) {
            const int diff = static_cast<int>(m_sorted[i]) - static_cast<int>(rhs.m_sorted[i]);
            if (0 != diff) return diff;
        }
        return 0;
    }

private:
    void sortPair(uint32_t a, uint32_t b) {
        if (a < b) { m_sorted[1] = a; m_sorted[2] = b; }
        else { m_sorted[1] = b; m_sorted[2] = a; }
    }
    uint32_t m_sorted[3] = {0, 0, 0};
};

// 1:1 TriangleSet (Primitives.ts:157-164) — sorted, de-duplicated TriangleKey set.
// insertKey builds a TriangleKey from a Triangle, inserts if new (calling onInsert), returns its index.
class TriangleSet {
public:
    // Insert a triangle's key. Returns the index of the (existing or newly inserted) key; calls
    // onInsert with the new key only when it was not already present. (1:1 SortedArray.insertKey.)
    size_t insertKey(const Triangle& triangle, const std::function<void(const TriangleKey&)>& onInsert = nullptr) {
        TriangleKey key(triangle);
        auto it = std::lower_bound(m_keys.begin(), m_keys.end(), key,
            [](const TriangleKey& a, const TriangleKey& b) { return a.compare(b) < 0; });
        if (it != m_keys.end() && it->compare(key) == 0)
            return static_cast<size_t>(it - m_keys.begin());
        const size_t index = static_cast<size_t>(it - m_keys.begin());
        it = m_keys.insert(it, key);
        if (onInsert) onInsert(*it);
        return index;
    }
    size_t size() const { return m_keys.size(); }
    bool isEmpty() const { return m_keys.empty(); }

private:
    std::vector<TriangleKey> m_keys;
};

END_DQ_RENDER_NAMESPACE

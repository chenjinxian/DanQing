// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/ColorMap.ts
// DanQing dqRender — ColorMap (IndexMap<number> tracking mesh color table + transparency; @internal)
//
// 保真依据：逐方法移植 ColorMap.ts。extends IndexMap<number>（DanQing IndexMap<uint32_t>，tbgr），
// ctor compareNumbers + maximumSize 0xffff。insert 覆写（name-hiding，DanQing IndexMap::insert 非虚）追踪
// _hasTransparency（首个颜色的透明性）。hasColor/hasTransparency/isUniform/toColorIndex。
// toColorIndex 用 ColorIndex（dqCommon FeatureIndex.h）的 reset/initUniform/initNonUniform。
// @internal；仅 dqRender Mesh 消费（Mesh.colorMap）。
#pragma once

#include <dqBase/IndexMap.h>
#include <dqCommon/ColorDef.h>
#include <dqCommon/FeatureIndex.h>

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 ColorMap (ColorMap.ts:13-52) — color table used to build a ColorIndex for a mesh.
class ColorMap : public dqBase::IndexMap<uint32_t> {
public:
    // 1:1 ColorMap constructor: super(compareNumbers, 0xffff).
    ColorMap()
        : dqBase::IndexMap<uint32_t>(compareNumbers, 0xffff)
    {
    }

    // 1:1 ColorMap.hasColor.
    bool hasColor(uint32_t color) const { return indexOf(color) != -1; }

    // 1:1 ColorMap.insert — track transparency, then super.insert. Name-hides the non-virtual
    // IndexMap::insert (called as colorMap.insert(fillColor) from Mesh.addVertex).
    int insert(uint32_t color)
    {
        // The table should never contain a mix of opaque and translucent colors (1:1 assert).
        if (isEmpty())
            m_hasTransparency = isTranslucent(color);
        return dqBase::IndexMap<uint32_t>::insert(color);
    }

    // 1:1 ColorMap.hasTransparency / isUniform getters.
    bool hasTransparency() const noexcept { return m_hasTransparency; }
    bool isUniform() const noexcept { return 1 == length(); }

    // 1:1 ColorMap.toColorIndex — populate a ColorIndex from this table + the mesh's per-vertex
    // color indices. `indices` are the mesh.colors entries (insertion-order color indices).
    void toColorIndex(dqCommon::ColorIndex& index, const std::vector<uint32_t>& indices) const;

private:
    // 1:1 ColorMap.isTranslucent — tbgr is translucent when not opaque.
    static bool isTranslucent(uint32_t tbgr) { return !dqCommon::ColorDef::isOpaque(tbgr); }

    // 1:1 compareNumbers (core-bentley): ascending numeric order.
    static int compareNumbers(uint32_t lhs, uint32_t rhs) noexcept
    {
        if (lhs < rhs) return -1;
        if (lhs > rhs) return 1;
        return 0;
    }

    bool m_hasTransparency = false;
};

END_DQ_RENDER_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/GeometryList.ts
// DanQing dqRender — GeometryList (container of Geometry records consumed by GeometryAccumulator)
//
// 保真依据：逐方法移植 GeometryList.ts。内部 vector<unique_ptr<Geometry>>；first/isEmpty/length/
// push/append/clear/computeRange（遍历 tileRange 扩展）。computeQuantizationParams 留 PR E（需 QParams3d）。
// @internal。
#pragma once

#include "GeometryPrimitives.h"

#include <dqGeom/Range3d.h>

#include <iterator>
#include <memory>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 GeometryList (GeometryList.ts:14-44) — a list of Geometry records.
class GeometryList {
public:
    Geometry* first() const { return m_list.empty() ? nullptr : m_list.front().get(); }
    bool isEmpty() const noexcept { return m_list.empty(); }
    size_t length() const noexcept { return m_list.size(); }

    void push(std::unique_ptr<Geometry> geom) { m_list.push_back(std::move(geom)); }
    // Reference append(src) shares Geometry objects (TS GC refs); C++ unique_ptr ownership moves src's
    // records into this list (src is left empty). Adaptation noted; faithful for the accumulator pipeline
    // (the only consumer pushes, never shares).
    void append(GeometryList& src) {
        m_list.insert(m_list.end(), std::make_move_iterator(src.m_list.begin()),
                      std::make_move_iterator(src.m_list.end()));
        src.m_list.clear();
    }
    void clear() { m_list.clear(); }

    // 1:1 GeometryList.computeRange — union of all records' tileRanges.
    dqGeom::Range3d computeRange() const {
        dqGeom::Range3d range = dqGeom::Range3d::CreateNull();
        for (const auto& geom : m_list)
            range.ExtendRange(geom->tileRange());
        return range;
    }

    // 1:1 iterator support.
    auto begin() { return m_list.begin(); }
    auto end() { return m_list.end(); }
    auto begin() const { return m_list.begin(); }
    auto end() const { return m_list.end(); }

private:
    std::vector<std::unique_ptr<Geometry>> m_list;
};

END_DQ_RENDER_NAMESPACE

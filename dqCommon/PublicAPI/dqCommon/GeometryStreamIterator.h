// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Geometry stream iterator
// Ported from: itwinjs-core core/common/src/geometry/GeometryStream.ts
//
// Iterates over geometry stream entries, providing typed access to each primitive.
// Uses opcode-based type discrimination (no RTTI required).
#pragma once

#include "Export.h"
#include "GeometryStream.h"
#include "DqCommon.h"

#include <dqGeom/GeometryQuery.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>

#include <cstdint>
#include <optional>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Represents a single entry when iterating a geometry stream.
// Ported from: itwinjs-core GeometryStreamIteratorEntry
struct GeometryStreamIteratorEntry {
    ElementGeometryOpcode opcode = ElementGeometryOpcode::PointPrimitive;
    const GeometryAppearanceProps* appearance = nullptr;
    const AreaFillProps* fill = nullptr;
    const GeometryMaterialProps* material = nullptr;
    const BRepDataProps* brep = nullptr;
    const GeometryPartInstanceProps* partRef = nullptr;
    const dqGeom::Range3d* subRange = nullptr;
    dqGeom::GeometryQueryPtr geometry;  // non-null for geometry entries
};

// Iterates over a geometry stream, yielding typed entries.
// Ported from: itwinjs-core GeometryStreamIterator
class DQ_COMMON_EXPORT GeometryStreamIterator {
public:
    // Construct from entries and optional parallel geometry array.
    GeometryStreamIterator(const std::vector<GeometryStreamEntry>& entries,
                           const std::vector<dqGeom::GeometryQueryPtr>* geometry = nullptr)
        : m_entries(&entries), m_geometry(geometry)
    {
    }

    // ── Position ──────────────────────────────────────────────────

    // Current index.
    size_t getIndex() const noexcept { return m_index; }

    // Number of entries.
    size_t getSize() const noexcept { return m_entries ? m_entries->size() : 0; }

    // Check if more entries remain.
    bool hasNext() const noexcept { return m_index < getSize(); }

    // Reset to beginning.
    void reset() noexcept { m_index = 0; }

    // ── Iteration ─────────────────────────────────────────────────

    // Get the current entry without advancing.
    GeometryStreamIteratorEntry getCurrent() const
    {
        GeometryStreamIteratorEntry result;
        if (!m_entries || m_index >= m_entries->size())
            return result;

        const auto& entry = (*m_entries)[m_index];
        result.opcode = entry.opcode;
        result.appearance = entry.appearance.has_value() ? &*entry.appearance : nullptr;
        result.fill = entry.fill.has_value() ? &*entry.fill : nullptr;
        result.material = entry.material.has_value() ? &*entry.material : nullptr;
        result.brep = entry.brep.has_value() ? &*entry.brep : nullptr;
        result.partRef = entry.partRef.has_value() ? &*entry.partRef : nullptr;
        result.subRange = entry.subRange.has_value() ? &*entry.subRange : nullptr;

        // Attach geometry if available
        if (m_geometry && m_index < m_geometry->size())
            result.geometry = (*m_geometry)[m_index];

        return result;
    }

    // Advance to next entry and return it.
    // Returns nullopt if no more entries.
    std::optional<GeometryStreamIteratorEntry> next()
    {
        if (!hasNext())
            return std::nullopt;
        auto result = getCurrent();
        ++m_index;
        return result;
    }

    // ── Queries ───────────────────────────────────────────────────

    // Check if the stream has view-independent flag.
    bool isViewIndependent() const noexcept
    {
        if (!m_entries || m_entries->empty()) return false;
        const auto& first = (*m_entries)[0];
        return first.header.has_value() &&
               first.header->flags == GeometryStreamFlags::ViewIndependent;
    }

    // Get the sub-range (placement bounding box) if the first entry is SubGraphicRange.
    std::optional<dqGeom::Range3d> getLocalRange() const
    {
        if (!m_entries || m_entries->empty()) return std::nullopt;
        const auto& first = (*m_entries)[0];
        if (first.opcode == ElementGeometryOpcode::SubGraphicRange && first.subRange.has_value())
            return first.subRange;
        return std::nullopt;
    }

    // Find the next entry with a specific opcode.
    std::optional<GeometryStreamIteratorEntry> findNext(ElementGeometryOpcode opcode)
    {
        while (hasNext()) {
            auto entry = getCurrent();
            ++m_index;
            if (entry.opcode == opcode)
                return entry;
        }
        return std::nullopt;
    }

private:
    const std::vector<GeometryStreamEntry>* m_entries = nullptr;
    const std::vector<dqGeom::GeometryQueryPtr>* m_geometry = nullptr;
    size_t m_index = 0;
};

END_DQ_COMMON_NAMESPACE

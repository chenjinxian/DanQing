// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Geometry stream builder
// Ported from: itwinjs-core core/common/src/geometry/GeometryStream.ts
//
// Builder for constructing geometry streams entry by entry.
// Produces a std::vector<GeometryStreamEntry> suitable for serialization.
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

// Builds a geometry stream by appending entries.
// Ported from: itwinjs-core GeometryStreamBuilder
class DQ_COMMON_EXPORT GeometryStreamBuilder {
public:
    GeometryStreamBuilder() = default;

    // ── Header ────────────────────────────────────────────────────

    // Set the stream header flags.
    // Ported from: itwinjs-core GeometryStreamBuilder.obtainHeader()
    void setViewIndependent(bool viewIndependent)
    {
        m_header.flags = viewIndependent ? GeometryStreamFlags::ViewIndependent : GeometryStreamFlags::None;
        m_headerSet = true;
        // Emit or update header entry as first entry
        if (m_entries.empty() || !m_entries[0].header.has_value()) {
            GeometryStreamEntry entry;
            entry.header = m_header;
            m_entries.insert(m_entries.begin(), entry);
        } else {
            m_entries[0].header->flags = m_header.flags;
        }
    }

    bool isViewIndependent() const noexcept
    {
        return m_header.flags == GeometryStreamFlags::ViewIndependent;
    }

    // ── Placement ─────────────────────────────────────────────────

    // Supply optional local-to-world transform. Used to transform world-coordinate input relative
    // to element placement. Calling with identity (or default-constructed) Transform clears the
    // stored world-to-local, resuming local-coordinate appending.
    // Ported from: itwinjs-core GeometryStreamBuilder.setLocalToWorld(Transform?) (ref GeometryStream.ts:202)
    void setLocalToWorld(const dqGeom::Transform& localToWorld)
    {
        if (localToWorld.IsIdentity()) {
            m_worldToLocal.reset();
            return;
        }
        dqGeom::Transform inverse;
        if (localToWorld.Inverse(inverse))
            m_worldToLocal = inverse;
        else
            m_worldToLocal.reset();
    }

    // clear any stored local-to-world transform (resume appending in local coordinates).
    // Ported from: itwinjs-core GeometryStreamBuilder.setLocalToWorld(undefined) (ref GeometryStream.ts:202)
    void setLocalToWorld()
    {
        m_worldToLocal.reset();
    }

    // Whether a non-identity local-to-world transform is currently active.
    bool hasLocalToWorld() const noexcept { return m_worldToLocal.has_value(); }

    // Set local-to-world transform from a 3d placement.
    // Ported from: itwinjs-core GeometryStreamBuilder.setLocalToWorld3d() (ref GeometryStream.ts:213)
    // NOTE: the ref signature is (origin: Point3d, angles: YawPitchRollAngles). DanQing has no
    // YawPitchRollAngles type, so this overload takes a Placement3d (origin + yaw/pitch/roll + bbox)
    // and emits the SubGraphicRange entry that the existing placement-based code path expects.
    void setLocalToWorld3d(const Placement3d& placement)
    {
        // Emit a SubGraphicRange entry with the placement's bounding box
        GeometryStreamEntry entry;
        entry.opcode = ElementGeometryOpcode::SubGraphicRange;
        entry.subRange = placement.bbox;
        m_entries.push_back(entry);
    }

    // Set local-to-world transform from a 2d placement.
    // Ported from: itwinjs-core GeometryStreamBuilder.setLocalToWorld2d() (ref GeometryStream.ts:223)
    void setLocalToWorld2d(const Placement2d& placement)
    {
        GeometryStreamEntry entry;
        entry.opcode = ElementGeometryOpcode::SubGraphicRange;
        entry.subRange = placement.bbox;
        m_entries.push_back(entry);
    }

    // TODO: blocked on missing PlacementProps discriminated-union type
    //       (ref GeometryStream.ts:233 setLocalToWorldFromPlacement(props: PlacementProps) — DanQing
    //        has Placement2d/Placement3d but no unified PlacementProps variant that switches on
    //        isPlacement2dProps). Port once PlacementProps is added to dqCommon.

    // ── Range bookkeeping ─────────────────────────────────────────

    // Store a local range marker in the GeometryStream for all subsequent geometry appended.
    // Improves range-testing performance for elements with multiple GeometryQuery differentiated
    // by range. Ignored when defining a GeometryPart (parts store their own range).
    // Ported from: itwinjs-core GeometryStreamBuilder.appendGeometryRanges() (ref GeometryStream.ts:285)
    void appendGeometryRanges()
    {
        GeometryStreamEntry entry;
        entry.opcode = ElementGeometryOpcode::SubGraphicRange;
        entry.subRange = dqGeom::Range3d::CreateNull();
        m_entries.push_back(entry);
    }

    // ── Appearance ────────────────────────────────────────────────

    // Change subcategory or appearance.
    // Ported from: itwinjs-core GeometryStreamBuilder.appendSubCategoryChange()
    void appendSubCategoryChange(const GeometryAppearanceProps& appearance)
    {
        GeometryStreamEntry entry;
        entry.opcode = ElementGeometryOpcode::BasicSymbology;
        entry.appearance = appearance;
        m_entries.push_back(entry);
    }

    // Change geometry params (appearance + fill + material).
    // Ported from: itwinjs-core GeometryStreamBuilder.appendGeometryParamsChange()
    void appendGeometryParamsChange(const GeometryAppearanceProps& appearance,
                                     const AreaFillProps* fill = nullptr,
                                     const GeometryMaterialProps* material = nullptr)
    {
        GeometryStreamEntry entry;
        entry.opcode = ElementGeometryOpcode::BasicSymbology;
        entry.appearance = appearance;
        if (fill) entry.fill = *fill;
        if (material) entry.material = *material;
        m_entries.push_back(entry);
    }

    // ── Geometry ──────────────────────────────────────────────────

    // Append a geometry query (curve, polyface, solid, etc.).
    // Ported from: itwinjs-core GeometryStreamBuilder.appendGeometry()
    void appendGeometry(dqGeom::GeometryQueryPtr geometry, ElementGeometryOpcode opcode = ElementGeometryOpcode::CurvePrimitive)
    {
        GeometryStreamEntry entry;
        entry.opcode = opcode;
        m_geometry.push_back(std::move(geometry));
        m_entries.push_back(entry);
    }

    // Append BRep data.
    // Ported from: itwinjs-core GeometryStreamBuilder.appendBRepData()
    void appendBRepData(const BRepDataProps& brep)
    {
        GeometryStreamEntry entry;
        entry.opcode = ElementGeometryOpcode::BRep;
        entry.brep = brep;
        m_entries.push_back(entry);
    }

    // Append a geometry part reference (3d instance).
    // Ported from: itwinjs-core GeometryStreamBuilder.appendGeometryPart3d() (ref GeometryStream.ts:316)
    void appendGeometryPart3d(const GeometryPartInstanceProps& partRef)
    {
        GeometryStreamEntry entry;
        entry.opcode = ElementGeometryOpcode::PartReference;
        entry.partRef = partRef;
        m_entries.push_back(entry);
    }

    // Append a geometry part reference (2d instance). Convenience wrapper around appendGeometryPart3d
    // that fills a GeometryPartInstanceProps from a 2d origin + rotation + scale.
    // Ported from: itwinjs-core GeometryStreamBuilder.appendGeometryPart2d() (ref GeometryStream.ts:320)
    //               (ref signature: (partId, instanceOrigin?: Point2d, instanceRotation?: Angle,
    //                instanceScale?: number); DanQing has no Point2d/Angle-in-dqGeom pair here, so the
    //                2d origin and rotation are passed as doubles matching GeometryPartInstanceProps.)
    void appendGeometryPart2d(dqBase::DqId partId,
                              double instanceOriginX = 0.0,
                              double instanceOriginY = 0.0,
                              double instanceRotationDegrees = 0.0,
                              double instanceScale = 1.0)
    {
        GeometryPartInstanceProps part;
        part.partId = partId;
        part.originX = instanceOriginX;
        part.originY = instanceOriginY;
        part.originZ = 0.0;  // 2d parts live in the z=0 plane
        part.yawDegrees = instanceRotationDegrees;  // 2d rotation is a yaw about +Z
        part.scale = instanceScale;
        appendGeometryPart3d(part);
    }

    // TODO: blocked on missing TextString type
    //       (ref GeometryStream.ts:341 appendTextString(textString: TextString) — DanQing has no
    //        TextString type in dqCommon). Port once TextString is added.
    // TODO: blocked on missing TextBlockGeometryProps type
    //       (ref GeometryStream.ts:358 appendTextBlock(block: TextBlockGeometryProps) — DanQing has
    //        no TextBlockGeometryProps). Port once TextBlockGeometryProps is added.
    // TODO: blocked on missing ImageGraphic type
    //       (ref GeometryStream.ts:376 appendImage(image: ImageGraphic) — DanQing has no ImageGraphic
    //        type). Port once ImageGraphic is added.

    // Append fill properties.
    void appendFill(const AreaFillProps& fill)
    {
        GeometryStreamEntry entry;
        entry.opcode = ElementGeometryOpcode::Fill;
        entry.fill = fill;
        m_entries.push_back(entry);
    }

    // Append material properties.
    void appendMaterial(const GeometryMaterialProps& material)
    {
        GeometryStreamEntry entry;
        entry.opcode = ElementGeometryOpcode::Material;
        entry.material = material;
        m_entries.push_back(entry);
    }

    // ── Output ────────────────────────────────────────────────────

    // Get the built entries.
    const std::vector<GeometryStreamEntry>& getEntries() const noexcept { return m_entries; }
    std::vector<GeometryStreamEntry>& getEntries() noexcept { return m_entries; }

    // Get the entries as a movable vector.
    std::vector<GeometryStreamEntry> takeEntries() noexcept { return std::move(m_entries); }

    // Number of entries.
    size_t getSize() const noexcept { return m_entries.size(); }

    // Check if empty.
    bool isEmpty() const noexcept { return m_entries.empty(); }

    // clear all entries.
    void clear()
    {
        m_entries.clear();
        m_geometry.clear();
        m_headerSet = false;
    }

private:
    std::vector<GeometryStreamEntry> m_entries;
    std::vector<dqGeom::GeometryQueryPtr> m_geometry;  // keeps geometry alive
    // Inverse of the user-supplied local-to-world transform; used to convert world-coordinate
    // geometry input to be placement-relative. Ported from: GeometryStreamBuilder._worldToLocal.
    std::optional<dqGeom::Transform> m_worldToLocal;
    GeometryStreamHeaderProps m_header;
    bool m_headerSet = false;
};

END_DQ_COMMON_NAMESPACE

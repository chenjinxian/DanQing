// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/GeometryAccumulator.ts
// DanQing dqRender — GeometryAccumulator (accumulates Geometry records → MeshList; @internal)
//
// 保真依据：逐字段/方法移植 GeometryAccumulator.ts。持 _transform/_surfacesOnly/tileRange/
// geometries(GeometryList)/currentFeature?。addLoop/addLineString/addPointString/addPath/addPolyface/
// addSolidPrimitive/addGeometry/clear 各自：getPrimitiveRange → calculateTransform（haveTransform 时
// 左乘 _transform，再 MultiplyRange 作用到 range）→ Geometry::createFromXxx → geometries.push。
// toMeshBuilderMap/toMeshes：geometries.computeRange + isAlmostZeroZ → MeshBuilderMap。
//
// 命名：dqRender/src/render 内部全 camelCase（§3.3 + 与 PR B/C/D′ 既有约定一致）。
//
// DEFERRED：analysisStyleDisplacement（PolyfaceData::auxData + AuxDataChannel::computeDisplacementRange）
// → Phase-N；addPolyface 的位移分支省略，range 直接取 getPrimitiveRange。@internal；PrimitiveBuilder 持有。
#pragma once

#include "GeometryList.h"
#include "GeometryPrimitives.h"
#include "MeshBuilderMap.h"
#include "MeshPrimitives.h"
#include "Primitives.h"

#include <dqCommon/FeatureTable.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Path.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/SolidPrimitive.h>
#include <dqGeom/Transform.h>

#include <memory>
#include <optional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 GeometryAccumulator ctor options (GeometryAccumulator.ts:33-45). analysisStyleDisplacement omitted
// (Phase-N — analysis displacement / auxData deferred).
struct GeometryAccumulatorOptions {
    bool surfacesOnly = false;
    dqGeom::Transform transform = dqGeom::Transform::CreateIdentity();
    dqGeom::Range3d tileRange;  // default null (Range3d default ctor is null)
    std::optional<dqCommon::Feature> feature;
};

// 1:1 GeometryAccumulator (GeometryAccumulator.ts:19-164).
class GeometryAccumulator {
public:
    explicit GeometryAccumulator(const GeometryAccumulatorOptions& options = GeometryAccumulatorOptions{});

    // --- Accessors (1:1 getters) ---
    bool surfacesOnly() const noexcept { return m_surfacesOnly; }
    const dqGeom::Transform& transform() const noexcept { return m_transform; }
    bool isEmpty() const { return m_geometries.isEmpty(); }
    bool haveTransform() const { return !m_transform.IsIdentity(); }

    const dqGeom::Range3d& tileRange() const noexcept { return m_tileRange; }
    GeometryList& geometries() noexcept { return m_geometries; }
    const GeometryList& geometries() const noexcept { return m_geometries; }

    // 1:1 currentFeature (public mutable field).
    std::optional<dqCommon::Feature>& currentFeature() noexcept { return m_currentFeature; }
    const std::optional<dqCommon::Feature>& currentFeature() const noexcept { return m_currentFeature; }

    // 1:1 GeometryAccumulator.addLoop (GeometryAccumulator.ts:61-68).
    bool addLoop(const dqBase::RefPtr<dqGeom::Loop>& loop, const DisplayParams& displayParams,
                 const dqGeom::Transform& transform, bool disjoint);
    // 1:1 addLineString (GeometryAccumulator.ts:70-79).
    bool addLineString(const std::vector<dqGeom::Point3d>& pts, const DisplayParams& displayParams,
                       const dqGeom::Transform& transform);
    // 1:1 addPointString (GeometryAccumulator.ts:81-90).
    bool addPointString(const std::vector<dqGeom::Point3d>& pts, const DisplayParams& displayParams,
                        const dqGeom::Transform& transform);
    // 1:1 addPath (GeometryAccumulator.ts:92-99).
    bool addPath(const dqBase::RefPtr<dqGeom::Path>& path, const DisplayParams& displayParams,
                 const dqGeom::Transform& transform, bool disjoint);
    // 1:1 addPolyface (GeometryAccumulator.ts:101-124). Analysis-displacement branch is Phase-N.
    bool addPolyface(const dqBase::RefPtr<dqGeom::IndexedPolyface>& pf, const DisplayParams& displayParams,
                     const dqGeom::Transform& transform);
    // 1:1 addSolidPrimitive (GeometryAccumulator.ts:126-133).
    bool addSolidPrimitive(const dqBase::RefPtr<dqGeom::SolidPrimitive>& primitive,
                           const DisplayParams& displayParams, const dqGeom::Transform& transform);

    // 1:1 addGeometry (GeometryAccumulator.ts:135-138).
    bool addGeometry(std::unique_ptr<Geometry> geom)
    {
        m_geometries.push(std::move(geom));
        return true;
    }

    // 1:1 clear (GeometryAccumulator.ts:140).
    void clear() { m_geometries.clear(); }

    // 1:1 toMeshBuilderMap (GeometryAccumulator.ts:148-155). `tolerance` derives from viewport pixel size.
    std::unique_ptr<MeshBuilderMap> toMeshBuilderMap(const GeometryOptions& options, double tolerance,
            const std::optional<MeshBuilderMapPickable>& pickable);

    // 1:1 toMeshes (GeometryAccumulator.ts:157-163).
    MeshList toMeshes(const GeometryOptions& options, double tolerance,
                      const std::optional<MeshBuilderMapPickable>& pickable);

private:
    // 1:1 getPrimitiveRange (GeometryAccumulator.ts:47-51) — geom.Range() if non-null. Templated over the
    // GeometryQuery subtype (Loop/Path/IndexedPolyface/SolidPrimitive), all carrying Range().
    template<typename Geom>
    std::optional<dqGeom::Range3d> getPrimitiveRange(const dqBase::RefPtr<Geom>& geom) const
    {
        const dqGeom::Range3d range = geom->Range();
        if (range.isNull())
            return std::nullopt;
        return range;
    }

    // 1:1 calculateTransform (GeometryAccumulator.ts:53-59) — left-multiply _transform if present, then
    // apply transform to range in place (range = transform(range)).
    dqGeom::Transform calculateTransform(dqGeom::Transform transform, dqGeom::Range3d& range) const
    {
        if (haveTransform())
            transform = m_transform.MultiplyTransform(transform);
        transform.MultiplyRange(range, range);  // 1:1 transform.multiplyRange(range, range)
        return transform;
    }

    dqGeom::Transform m_transform;
    bool m_surfacesOnly = false;
    dqGeom::Range3d m_tileRange;
    GeometryList m_geometries;
    std::optional<dqCommon::Feature> m_currentFeature;
};

END_DQ_RENDER_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/GeometryAccumulator.ts
// DanQing dqRender — GeometryAccumulator implementation
#include "GeometryAccumulator.h"

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 constructor (GeometryAccumulator.ts:33-45).
GeometryAccumulator::GeometryAccumulator(const GeometryAccumulatorOptions& options)
    : m_transform(options.transform)
    , m_surfacesOnly(options.surfacesOnly)
    , m_tileRange(options.tileRange)
    , m_currentFeature(options.feature)
{
}

// 1:1 addLoop (GeometryAccumulator.ts:61-68).
bool GeometryAccumulator::addLoop(const dqBase::RefPtr<dqGeom::Loop>& loop, const DisplayParams& displayParams,
                                  const dqGeom::Transform& transform, bool disjoint)
{
    auto rangeOpt = getPrimitiveRange(loop);
    if (!rangeOpt)
        return false;
    dqGeom::Range3d range = *rangeOpt;
    const dqGeom::Transform xform = calculateTransform(transform, range);
    return addGeometry(Geometry::createFromLoop(loop, xform, range, displayParams, disjoint, m_currentFeature));
}

// 1:1 addLineString (GeometryAccumulator.ts:70-79).
bool GeometryAccumulator::addLineString(const std::vector<dqGeom::Point3d>& pts, const DisplayParams& displayParams,
                                        const dqGeom::Transform& transform)
{
    // Do getPrimitiveRange() manually — no need to create a PointString3d just to find the range.
    dqGeom::Range3d range;
    range.extendArray(pts);
    if (range.isNull())
        return false;

    const dqGeom::Transform xform = calculateTransform(transform, range);
    return addGeometry(Geometry::createFromLineString(pts, xform, range, displayParams, m_currentFeature));
}

// 1:1 addPointString (GeometryAccumulator.ts:81-90).
bool GeometryAccumulator::addPointString(const std::vector<dqGeom::Point3d>& pts, const DisplayParams& displayParams,
                                         const dqGeom::Transform& transform)
{
    dqGeom::Range3d range;
    range.extendArray(pts);
    if (range.isNull())
        return false;

    const dqGeom::Transform xform = calculateTransform(transform, range);
    return addGeometry(Geometry::createFromPointString(pts, xform, range, displayParams, m_currentFeature));
}

// 1:1 addPath (GeometryAccumulator.ts:92-99).
bool GeometryAccumulator::addPath(const dqBase::RefPtr<dqGeom::Path>& path, const DisplayParams& displayParams,
                                  const dqGeom::Transform& transform, bool disjoint)
{
    auto rangeOpt = getPrimitiveRange(path);
    if (!rangeOpt)
        return false;
    dqGeom::Range3d range = *rangeOpt;
    const dqGeom::Transform xform = calculateTransform(transform, range);
    return addGeometry(Geometry::createFromPath(path, xform, range, displayParams, disjoint, m_currentFeature));
}

// 1:1 addPolyface (GeometryAccumulator.ts:101-124). Analysis-displacement branch is Phase-N (omitted);
// range is taken directly from getPrimitiveRange.
bool GeometryAccumulator::addPolyface(const dqBase::RefPtr<dqGeom::IndexedPolyface>& pf,
                                      const DisplayParams& displayParams, const dqGeom::Transform& transform)
{
    auto rangeOpt = getPrimitiveRange(pf);
    if (!rangeOpt)
        return false;
    dqGeom::Range3d range = *rangeOpt;
    const dqGeom::Transform xform = calculateTransform(transform, range);
    return addGeometry(Geometry::createFromPolyface(pf, xform, range, displayParams, m_currentFeature));
}

// 1:1 addSolidPrimitive (GeometryAccumulator.ts:126-133).
bool GeometryAccumulator::addSolidPrimitive(const dqBase::RefPtr<dqGeom::SolidPrimitive>& primitive,
                                            const DisplayParams& displayParams, const dqGeom::Transform& transform)
{
    auto rangeOpt = getPrimitiveRange(primitive);
    if (!rangeOpt)
        return false;
    dqGeom::Range3d range = *rangeOpt;
    const dqGeom::Transform xform = calculateTransform(transform, range);
    return addGeometry(
        Geometry::createFromSolidPrimitive(primitive, xform, range, displayParams, m_currentFeature));
}

// 1:1 toMeshBuilderMap (GeometryAccumulator.ts:148-155).
std::unique_ptr<MeshBuilderMap> GeometryAccumulator::toMeshBuilderMap(const GeometryOptions& options,
        double tolerance, const std::optional<MeshBuilderMapPickable>& pickable)
{
    const dqGeom::Range3d range = m_geometries.computeRange();
    const bool is2d = !range.isNull() && range.isAlmostZeroZ();
    return MeshBuilderMap::createFromGeometries(m_geometries, tolerance, range, is2d, options, pickable);
}

// 1:1 toMeshes (GeometryAccumulator.ts:157-163).
MeshList GeometryAccumulator::toMeshes(const GeometryOptions& options, double tolerance,
                                       const std::optional<MeshBuilderMapPickable>& pickable)
{
    if (m_geometries.isEmpty())
        return MeshList();

    auto builderMap = toMeshBuilderMap(options, tolerance, pickable);
    return builderMap->toMeshes();
}

END_DQ_RENDER_NAMESPACE

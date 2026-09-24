// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/MeshBuilderMap.ts
// DanQing dqRender — MeshBuilderMap implementation
#include "MeshBuilderMap.h"

BEGIN_DQ_RENDER_NAMESPACE

namespace {

// Authored: 1:1 core-bentley compareBooleans (false < true) and compareNumbers (ascending).
int compareBooleans(bool a, bool b) noexcept { return (a ? 1 : 0) - (b ? 1 : 0); }
int compareNumbers(int a, int b) noexcept { return (a < b) ? -1 : (a > b ? 1 : 0); }

}  // namespace

// 1:1 MeshBuilderMap constructor (MeshBuilderMap.ts:34-46).
MeshBuilderMap::MeshBuilderMap(double tolerance, const dqGeom::Range3d& range, bool is2d,
                               GeometryOptions options, const std::optional<MeshBuilderMapPickable>& pickable)
    : m_range(range)
    , m_tolerance(tolerance)
    , m_vertexTolerance(tolerance * ToleranceRatio::vertex)
    , m_facetAreaTolerance(tolerance * ToleranceRatio::facetArea)
    , m_is2d(is2d)
    , m_options(options)
    , m_isVolumeClassifier(pickable ? pickable->isVolumeClassifier : false)
{
    if (pickable)
        m_features.emplace(2048 * 1024, pickable->modelId);
}

// 1:1 MeshBuilderMap.createFromGeometries (MeshBuilderMap.ts:48-55).
std::unique_ptr<MeshBuilderMap> MeshBuilderMap::createFromGeometries(GeometryList& geometries,
        double tolerance, const dqGeom::Range3d& range, bool is2d, GeometryOptions options,
        const std::optional<MeshBuilderMapPickable>& pickable)
{
    auto map = std::make_unique<MeshBuilderMap>(tolerance, range, is2d, options, pickable);
    for (auto& geom : geometries)
        map->loadGeometry(*geom);
    return map;
}

// 1:1 MeshBuilderMap.toMeshes (MeshBuilderMap.ts:57-64).
MeshList MeshBuilderMap::toMeshes()
{
    MeshList meshes(m_features, m_range);
    for (auto& kv : m_map) {
        MeshBuilder& builder = *kv.second;
        if (builder.mesh().points().length() > 0)
            meshes.push(builder.releaseMesh());
    }
    return meshes;
}

// 1:1 MeshBuilderMap.loadGeometry (MeshBuilderMap.ts:70-73).
void MeshBuilderMap::loadGeometry(Geometry& geom)
{
    loadPolyfacePrimitiveList(geom);
    loadStrokePrimitiveList(geom);
}

// 1:1 MeshBuilderMap.loadPolyfacePrimitiveList (MeshBuilderMap.ts:79-85).
void MeshBuilderMap::loadPolyfacePrimitiveList(Geometry& geom)
{
    const auto polyfaces = geom.getPolyfaces(m_tolerance);
    if (polyfaces)
        for (const auto& polyface : *polyfaces)
            loadIndexedPolyface(polyface, geom.feature());
}

// 1:1 MeshBuilderMap.loadIndexedPolyface (MeshBuilderMap.ts:91-103).
void MeshBuilderMap::loadIndexedPolyface(const PolyfacePrimitive& polyface,
                                         const std::optional<dqCommon::Feature>& feature)
{
    const auto& indexedPolyface = polyface.indexedPolyface();
    const DisplayParams& displayParams = polyface.displayParams;
    const bool isPlanar = polyface.isPlanar;
    const size_t pointCount = indexedPolyface->Data().PointCount();
    const size_t normalCount = indexedPolyface->Data().NormalCount();
    const uint32_t fillColor = displayParams.fillColor().getTbgr();
    const bool isTextured = displayParams.isTextured();
    const std::optional<dqCommon::TextureMapping>& textureMapping = displayParams.textureMapping();

    if (pointCount == 0)
        return;

    MeshBuilder& builder = getBuilder(displayParams, MeshPrimitiveType::Mesh, normalCount > 0, isPlanar);
    const MeshEdgeCreationType edgeType =
        (polyface.displayEdges && m_options.wantEdges) ? MeshEdgeCreationType::DefaultEdges : MeshEdgeCreationType::NoEdges;
    MeshBuilder::PolyfaceOptions props;
    props.includeParams = isTextured;
    props.fillColor = fillColor;
    props.mappedTexture = textureMapping;
    props.edgeOptions = MeshEdgeCreationOptions(edgeType);
    builder.addFromPolyface(*indexedPolyface, props, feature);
}

// 1:1 MeshBuilderMap.loadStrokePrimitiveList (MeshBuilderMap.ts:109-115).
void MeshBuilderMap::loadStrokePrimitiveList(Geometry& geom)
{
    const auto strokes = geom.getStrokes(m_tolerance);
    if (strokes)
        for (const auto& stroke : *strokes)
            loadStrokesPrimitive(stroke, geom.feature());
}

// 1:1 MeshBuilderMap.loadStrokesPrimitive (MeshBuilderMap.ts:121-127).
void MeshBuilderMap::loadStrokesPrimitive(const StrokesPrimitive& strokePrimitive,
                                          const std::optional<dqCommon::Feature>& feature)
{
    const DisplayParams& displayParams = strokePrimitive.displayParams;
    const bool isDisjoint = strokePrimitive.isDisjoint;
    const bool isPlanar = strokePrimitive.isPlanar;
    const StrokesPrimitivePointLists& strokes = strokePrimitive.strokes;

    const MeshPrimitiveType type = isDisjoint ? MeshPrimitiveType::Point : MeshPrimitiveType::Polyline;
    MeshBuilder& builder = getBuilder(displayParams, type, false, isPlanar);
    builder.addStrokePointLists(strokes, isDisjoint, displayParams.lineColor().getTbgr(), feature);
}

// 1:1 MeshBuilderMap.getBuilder (MeshBuilderMap.ts:129-146).
MeshBuilder& MeshBuilderMap::getBuilder(const DisplayParams& displayParams, MeshPrimitiveType type,
                                        bool hasNormals, bool isPlanar)
{
    const Key key = getKey(displayParams, type, hasNormals, isPlanar);

    MeshBuilder::Props props;
    props.displayParams = displayParams;
    props.type = type;
    props.range = m_range;
    props.quantizePositions = false;  // ###TODO should this be configurable? (reference: always false here)
    props.is2d = m_is2d;
    props.isPlanar = isPlanar;
    props.tolerance = m_tolerance;
    props.areaTolerance = m_facetAreaTolerance;
    props.features = m_features;  // D′: nullopt (no pickable). Pickable path copies — see header TODO.
    props.isVolumeClassifier = m_isVolumeClassifier;
    return getBuilderFromKey(key, std::move(props));
}

// 1:1 MeshBuilderMap.getKey (MeshBuilderMap.ts:148-155).
MeshBuilderMap::Key MeshBuilderMap::getKey(const DisplayParams& displayParams, MeshPrimitiveType type,
                                           bool hasNormals, bool isPlanar)
{
    Key key(displayParams, type, hasNormals, isPlanar);
    if (m_options.preserveOrder)
        key.setOrder(++m_keyOrder);
    return key;
}

// 1:1 MeshBuilderMap.getBuilderFromKey (MeshBuilderMap.ts:163-170).
MeshBuilder& MeshBuilderMap::getBuilderFromKey(const Key& key, MeshBuilder::Props props)
{
    auto it = m_map.find(key);
    if (it == m_map.end()) {
        auto builder = MeshBuilder::create(std::move(props));
        MeshBuilder* ptr = builder.get();
        // Emplace copies `key` into the map node (preserving its order, set by getKey when preserveOrder).
        m_map.emplace(key, std::move(builder));
        return *ptr;
    }
    return *it->second;
}

// 1:1 MeshBuilderMap.Key.compare (MeshBuilderMap.ts:193-209).
int MeshBuilderMap::Key::compare(const Key& rhs) const
{
    int diff = compareNumbers(m_order, rhs.m_order);
    if (0 == diff) {
        diff = compareNumbers(static_cast<int>(m_type), static_cast<int>(rhs.m_type));
        if (0 == diff) {
            diff = compareBooleans(m_isPlanar, rhs.m_isPlanar);
            if (0 == diff) {
                diff = compareBooleans(m_hasNormals, rhs.m_hasNormals);
                if (0 == diff)
                    diff = m_params.compareForMerge(rhs.m_params);
            }
        }
    }
    return diff;
}

END_DQ_RENDER_NAMESPACE

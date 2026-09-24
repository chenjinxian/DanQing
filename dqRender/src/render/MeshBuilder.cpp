// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/MeshBuilder.ts
// DanQing dqRender — MeshBuilder implementation (tessellation engine)
#include "MeshBuilder.h"

#include <array>
#include <functional>
#include <utility>

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 MeshBuilder private constructor (MeshBuilder.ts:42-58).
MeshBuilder::MeshBuilder(std::unique_ptr<Mesh> mesh, double tolerance, double areaTolerance,
                         const dqGeom::Range3d& tileRange)
    : m_mesh(std::move(mesh))
    , m_vertexMap(dqGeom::Point3d::From(tolerance, tolerance, tolerance))
    , m_tolerance(tolerance)
    , m_areaTolerance(areaTolerance)
    , m_tileRange(tileRange)
{
    // Non-quantized path (always — MeshBuilderMap sets quantizePositions false): vertexTolerance is a
    // uniform (tolerance, tolerance, tolerance) vector. (QPoint3dList path is Phase-N.)
}

// 1:1 MeshBuilder.addStrokePointLists (MeshBuilder.ts:73-80).
void MeshBuilder::addStrokePointLists(const StrokesPrimitivePointLists& strokes, bool isDisjoint,
                                      uint32_t fillColor, const std::optional<dqCommon::Feature>& feature)
{
    for (const auto& strokePoints : strokes) {
        if (isDisjoint)
            addPointString(strokePoints.points, fillColor, feature);
        else
            addPolyline(strokePoints.points, fillColor, feature);
    }
}

// 1:1 MeshBuilder.addFromPolyface (MeshBuilder.ts:87-96).
void MeshBuilder::addFromPolyface(const dqGeom::IndexedPolyface& polyface, const PolyfaceOptions& props,
                                  const std::optional<dqCommon::Feature>& feature)
{
    beginPolyface(polyface, props.edgeOptions);
    auto visitor = polyface.CreateVisitor(0);  // numWrap = 0
    while (visitor->MoveToNextFacet())
        addFromPolyfaceVisitor(*visitor, props, feature);
    endPolyface();
}

// 1:1 MeshBuilder.addFromPolyfaceVisitor (MeshBuilder.ts:102-125).
void MeshBuilder::addFromPolyfaceVisitor(dqGeom::PolyfaceVisitor& visitor, const PolyfaceOptions& options,
                                         const std::optional<dqCommon::Feature>& feature)
{
    const size_t pointCount = visitor.PointCount();
    const size_t normalCount = visitor.NormalCount();
    const size_t paramCount = visitor.ParamCount();
    const bool requireNormals = visitor.RequireNormals();

    // TFS#790263: degenerate triangle has no normals.
    const bool isDegenerate = requireNormals && normalCount < pointCount;
    if (pointCount < 3 || isDegenerate)
        return;

    const bool haveParam = options.includeParams && paramCount > 0;
    const size_t triangleCount = pointCount - 2;

    // Fan-triangulate the (convex) facet.
    PolyfaceVisitorOptions vopts;
    static_cast<PolyfaceOptions&>(vopts) = options;
    vopts.triangleCount = triangleCount;
    vopts.haveParam = haveParam;

    for (size_t triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex) {
        auto triangle = createTriangle(triangleIndex, visitor, vopts, feature);
        if (triangle)
            addTriangle(*triangle);
    }
}

// 1:1 MeshBuilder.createTriangleVertices (MeshBuilder.ts:127-160).
std::optional<std::array<VertexKeyProps, 3>> MeshBuilder::createTriangleVertices(
    size_t triangleIndex, dqGeom::PolyfaceVisitor& visitor, const PolyfaceVisitorOptions& options,
    const std::optional<dqCommon::Feature>& feature) const
{
    const bool requireNormals = visitor.RequireNormals();
    const uint32_t fillColor = options.fillColor;

    // UV params computed from mappedTexture (MeshBuilder.ts:131-140) — Phase-N; always absent in D′.
    std::array<VertexKeyProps, 3> vertices{};

    for (size_t i = 0; i < 3; ++i) {
        const size_t vertexIndex = (0 == i) ? 0 : triangleIndex + i;
        VertexKeyProps props;
        props.position = visitor.GetPoint(vertexIndex);
        props.fillColor = fillColor;
        if (requireNormals)
            props.normal = dqCommon::OctEncodedNormal::fromVector(visitor.GetNormal(vertexIndex));
        // uvParam: nullopt (Phase-N — computeUVParams deferred).
        props.feature = feature;
        vertices[i] = props;
    }

    // 1:1 degenerate guard (MeshBuilder.ts:154-157) — detect before adding vertices.
    if (m_vertexMap.arePositionsAlmostEqual(vertices[0], vertices[1]) ||
        m_vertexMap.arePositionsAlmostEqual(vertices[0], vertices[2]) ||
        m_vertexMap.arePositionsAlmostEqual(vertices[1], vertices[2]))
        return std::nullopt;

    return vertices;
}

// 1:1 MeshBuilder.createTriangle (MeshBuilder.ts:162-199).
std::optional<Triangle> MeshBuilder::createTriangle(size_t triangleIndex, dqGeom::PolyfaceVisitor& visitor,
                                                    const PolyfaceVisitorOptions& options,
                                                    const std::optional<dqCommon::Feature>& feature)
{
    auto vertices = createTriangleVertices(triangleIndex, visitor, options, feature);
    if (!vertices)
        return std::nullopt;

    Triangle triangle;
    triangle.setEdgeVisibility(
        0 == triangleIndex ? visitor.EdgeVisible(0) : false,
        visitor.EdgeVisible(triangleIndex + 1),
        triangleIndex == options.triangleCount - 1 ? visitor.EdgeVisible(triangleIndex + 2) : false);

    // 1:1 vertices.forEach: assign each triangle index to the vertex-key index from the map.
    std::array<uint32_t, 3> indices{};
    for (size_t i = 0; i < 3; ++i) {
        // 1:1: auxData branch (no dedup) is Phase-N; D′ always takes the addVertex dedup path.
        const int vertexKeyIndex = addVertex((*vertices)[i]);
        indices[i] = (vertexKeyIndex < 0) ? 0 : static_cast<uint32_t>(vertexKeyIndex);

        // 1:1: if currentPolyface, map vertexKeyIndex → visitor.clientPointIndex(sourceIndex).
        // D′ never builds a currentPolyface (wantEdges=false); kept faithful for the Phase-N edge path.
        if (m_currentPolyface) {
            const size_t sourceIndex = (0 == i) ? 0 : triangleIndex + i;
            m_currentPolyface->vertexIndexMap()[indices[i]] =
                static_cast<uint32_t>(visitor.ClientPointIndex(sourceIndex));
        }
    }
    triangle.setIndices(indices[0], indices[1], indices[2]);

    return triangle;
}

// 1:1 MeshBuilder.addPolyline (MeshBuilder.ts:202-210).
void MeshBuilder::addPolyline(const std::vector<dqGeom::Point3d>& points, uint32_t fillColor,
                              const std::optional<dqCommon::Feature>& feature)
{
    MeshPolyline poly;
    for (const auto& position : points) {
        VertexKeyProps props;
        props.position = position;
        props.fillColor = fillColor;
        props.feature = feature;
        poly.addIndex(static_cast<uint32_t>(addVertex(props)));
    }
    m_mesh->addPolyline(poly);
}

// 1:1 MeshBuilder.addPointString (MeshBuilder.ts:213-221).
void MeshBuilder::addPointString(const std::vector<dqGeom::Point3d>& points, uint32_t fillColor,
                                 const std::optional<dqCommon::Feature>& feature)
{
    MeshPolyline poly;
    for (const auto& position : points) {
        VertexKeyProps props;
        props.position = position;
        props.fillColor = fillColor;
        props.feature = feature;
        poly.addIndex(static_cast<uint32_t>(addVertex(props)));
    }
    m_mesh->addPolyline(poly);
}

// 1:1 MeshBuilder.beginPolyface (MeshBuilder.ts:223-228).
void MeshBuilder::beginPolyface(const dqGeom::Polyface& polyface, const MeshEdgeCreationOptions& options)
{
    if (!options.generateNoEdges()) {
        TriangleList* tris = m_mesh->triangles();
        const size_t base = tris ? tris->length() : 0;
        m_currentPolyface = std::make_unique<MeshBuilderPolyface>(polyface, options, base);
    }
}

// 1:1 MeshBuilder.addVertex (MeshBuilder.ts:239-243).
int MeshBuilder::addVertex(const VertexKeyProps& vertex, bool addToMeshOnInsert)
{
    std::function<void(const VertexKey&)> onInsert;
    if (addToMeshOnInsert) {
        onInsert = [this](const VertexKey& vk) { m_mesh->addVertex(vk.props()); };
    }
    return m_vertexMap.insertKey(vertex, onInsert);
}

// 1:1 MeshBuilder.addTriangle (MeshBuilder.ts:245-253).
void MeshBuilder::addTriangle(const Triangle& triangle)
{
    if (triangle.isDegenerate())
        return;
    std::function<void(const TriangleKey&)> onInsert =
        [this, &triangle](const TriangleKey&) { m_mesh->addTriangle(triangle); };
    triangleSet().insertKey(triangle, onInsert);
}

END_DQ_RENDER_NAMESPACE

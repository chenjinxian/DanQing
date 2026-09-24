// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/MeshPrimitives.ts
// DanQing dqRender — Mesh / Mesh::Features implementation
#include "MeshPrimitives.h"


BEGIN_DQ_RENDER_NAMESPACE

// 1:1 Mesh private constructor (MeshPrimitives.ts:151-173).
Mesh::Mesh(Props props)
    : m_displayParams(std::move(props.displayParams))
    , m_type(props.type)
    , m_is2d(props.is2d)
    , m_isPlanar(props.isPlanar)
    , m_hasBakedLighting(props.hasBakedLighting)
    , m_isVolumeClassifier(props.isVolumeClassifier)
{
    // _data = MeshPrimitiveType.Mesh === type ? new TriangleList() : [] (MeshPolylineList).
    if (MeshPrimitiveType::Mesh == m_type)
        m_triangles = std::make_unique<TriangleList>();
    else
        m_polylines = std::make_unique<MeshPolylineList>();

    if (props.features)
        m_features.emplace(std::move(*props.features));

    // 1:1 quantizePositions branch (always false in the accumulator pipeline — MeshBuilderMap sets it
    // false). Non-quantized: points.range = range; center = range.center; add → push(pt - center).
    m_points.range = props.range;
    m_points.center = props.range.Center();
    // (quantizePositions=true path — QPoint3dList(QParams3d::fromRange(range)) — is Phase-N, unused.)
}

// 1:1 Mesh.addPolyline (MeshPrimitives.ts:235-246).
void Mesh::addPolyline(const MeshPolyline& poly)
{
    // assert(Polyline || Point); polylines != undefined.
    if (MeshPrimitiveType::Polyline == m_type && poly.indices.size() < 2)
        return;
    if (m_polylines)
        m_polylines->push_back(poly);
}

// 1:1 Mesh.addTriangle (MeshPrimitives.ts:248-256).
void Mesh::addTriangle(const Triangle& triangle)
{
    // assert(Mesh); triangles != undefined.
    if (m_triangles)
        m_triangles->addTriangle(triangle);
}

// 1:1 Mesh.addVertex (MeshPrimitives.ts:258-289).
size_t Mesh::addVertex(const VertexKeyProps& props)
{
    m_points.add(props.position);

    if (props.normal)
        m_normals.push_back(*props.normal);

    if (props.uvParam)
        m_uvParams.push_back(*props.uvParam);

    if (props.feature) {
        // assert(features != undefined).
        if (m_features)
            m_features->add(*props.feature, m_points.length());
    }

    // Don't allocate color indices until we have non-uniform colors.
    if (0 == m_colorMap.length()) {
        m_colorMap.insert(props.fillColor);
    } else if (!m_colorMap.isUniform() || !m_colorMap.hasColor(props.fillColor)) {
        // Back-fill uniform value (index=0) for existing vertices if previously uniform.
        if (m_colors.empty())
            m_colors.resize(m_points.length() - 1, 0);  // 1:1 colors.length = points.length - 1 (→ zeros)
        m_colors.push_back(m_colorMap.insert(props.fillColor));
    }

    return m_points.length() - 1;
}

// ---------------------------------------------------------------------------
// Mesh::Features
// ---------------------------------------------------------------------------

// 1:1 Mesh.Features.add (MeshPrimitives.ts:301-317).
void Mesh::Features::add(const dqCommon::Feature& feat, size_t numVerts)
{
    const int index = m_table.insert(feat);
    if (!m_initialized) {
        // First feature — uniform.
        m_uniform = static_cast<uint32_t>(index);
        m_initialized = true;
    } else if (!m_indices.empty()) {
        // Already non-uniform.
        m_indices.push_back(static_cast<uint32_t>(index));
    } else {
        // Second feature — back-fill uniform for existing verts.
        while (m_indices.size() < numVerts - 1)
            m_indices.push_back(m_uniform);
        m_indices.push_back(static_cast<uint32_t>(index));
    }
}

// 1:1 Mesh.Features.setIndices (MeshPrimitives.ts:319-329).
void Mesh::Features::setIndices(const std::vector<uint32_t>& indices)
{
    m_indices.clear();
    m_uniform = 0;
    m_initialized = !indices.empty();

    // assert(0 < indices.length).
    if (indices.size() == 1)
        m_uniform = indices[0];
    else if (indices.size() > 1)
        m_indices = indices;
}

// 1:1 Mesh.Features.toFeatureIndex (MeshPrimitives.ts:331-344).
void Mesh::Features::toFeatureIndex(dqCommon::FeatureIndex& index) const
{
    using namespace dqCommon;
    if (!m_initialized) {
        index.type = FeatureIndexType::Empty;
    } else if (m_indices.empty()) {
        index.type = FeatureIndexType::Uniform;
        index.featureID = m_uniform;
    } else {
        index.type = FeatureIndexType::NonUniform;
        index.featureIDs = m_indices;
    }
}

END_DQ_RENDER_NAMESPACE

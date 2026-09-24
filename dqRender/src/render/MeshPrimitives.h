// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/MeshPrimitives.ts
// DanQing dqRender — Mesh / Mesh::Features / Mesh::Props / MeshList / MeshPolyline (@internal)
//
// 保真依据：逐类型/方法移植 MeshPrimitives.ts。Mesh 持 _data(TriangleList | MeshPolylineList)、
// points(Point3dList，ctor 设 center=range.Center 使 add 重心化)、normals/uvParams/colorMap/colors、
// features?、displayParams、type/is2d/isPlanar/hasBakedLighting/isVolumeClassifier。private ctor +
// static create。addVertex（颜色 uniform→non-uniform 回填，1:1）、addTriangle、addPolyline、
// toFeatureIndex。Mesh.Features(add/setIndices/toFeatureIndex)。MeshList(features?,range?)。
//
// DEFERRED：
//  - createMeshArgs/createPolylineArgs/MeshArgsEdges → PR F（MeshArgs/PolylineArgs GPU bridge；
//    toColorIndex 已就绪，但 MeshArgs.edges 依赖的 MeshEdges/EdgeArgs 类型未 port）。
//  - mesh.edges / buildMeshEdges / auxChannels → Phase-N（边生成、分析位移）。
//
// MeshPolyline：1:1 core-common RenderMesh.ts:15-29（dqCommon 暂未 port；dqRender-internal 别名，
// 待 dqCommon::MeshPolyline 落地后一行 using 替换）。@internal；仅 Mesh/MeshBuilder/createPolylineArgs 消费。
#pragma once

#include "ColorMap.h"
#include "DisplayParams.h"
#include "MeshPrimitive.h"
#include "Primitives.h"
#include "VertexKey.h"

#include <dqCommon/FeatureIndex.h>
#include <dqCommon/FeatureTable.h>
#include <dqCommon/OctEncodedNormal.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Range3d.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 MeshPolyline (core-common RenderMesh.ts:15-29). dqRender-internal until dqCommon ports it.
class MeshPolyline {
public:
    MeshPolyline() = default;
    explicit MeshPolyline(const std::vector<uint32_t>& idx) : indices(idx) {}

    // 1:1 MeshPolyline.addIndex — append unless identical to the last (collapse runs).
    void addIndex(uint32_t index)
    {
        if (indices.empty() || indices.back() != index)
            indices.push_back(index);
    }

    // 1:1 MeshPolyline.clear.
    void clear() { indices.clear(); }

    std::vector<uint32_t> indices;
};

// 1:1 MeshPolylineList (Array<MeshPolyline>).
using MeshPolylineList = std::vector<MeshPolyline>;

// 1:1 Mesh (MeshPrimitives.ts:133-290).
class Mesh {
public:
    // 1:1 Mesh.Features (MeshPrimitives.ts:293-345).
    class Features {
    public:
        // 1:1 Mesh.Features constructor(table).
        explicit Features(dqCommon::FeatureTable table) : m_table(std::move(table)) {}

        // 1:1 Mesh.Features.add(feat, numVerts).
        void add(const dqCommon::Feature& feat, size_t numVerts);

        // 1:1 Mesh.Features.setIndices(indices).
        void setIndices(const std::vector<uint32_t>& indices);

        // 1:1 Mesh.Features.toFeatureIndex(output?) — fill `index` (1:1 returns it; DanQing fills by ref).
        void toFeatureIndex(dqCommon::FeatureIndex& index) const;

        const dqCommon::FeatureTable& table() const noexcept { return m_table; }
        const std::vector<uint32_t>& indices() const noexcept { return m_indices; }
        uint32_t uniform() const noexcept { return m_uniform; }
        bool initialized() const noexcept { return m_initialized; }

    private:
        dqCommon::FeatureTable m_table;
        std::vector<uint32_t> m_indices;
        uint32_t m_uniform = 0;
        bool m_initialized = false;
    };

    // 1:1 Mesh.Props (MeshPrimitives.ts:347-357).
    struct Props {
        DisplayParams displayParams;
        std::optional<dqCommon::FeatureTable> features;
        MeshPrimitiveType type = MeshPrimitiveType::Mesh;
        dqGeom::Range3d range;
        bool quantizePositions = false;
        bool is2d = false;
        bool isPlanar = false;
        bool hasBakedLighting = false;
        bool isVolumeClassifier = false;
    };

    // 1:1 Mesh.create(props).
    static std::unique_ptr<Mesh> create(Props props) { return std::unique_ptr<Mesh>(new Mesh(std::move(props))); }

    // --- Accessors (1:1 readonly fields) ---
    MeshPrimitiveType type() const noexcept { return m_type; }
    bool is2d() const noexcept { return m_is2d; }
    bool isPlanar() const noexcept { return m_isPlanar; }
    bool hasBakedLighting() const noexcept { return m_hasBakedLighting; }
    bool isVolumeClassifier() const noexcept { return m_isVolumeClassifier; }

    const DisplayParams& displayParams() const noexcept { return m_displayParams; }
    DisplayParams& displayParams() noexcept { return m_displayParams; }

    const Point3dList& points() const noexcept { return m_points; }
    Point3dList& points() noexcept { return m_points; }

    const std::vector<dqCommon::OctEncodedNormal>& normals() const noexcept { return m_normals; }
    std::vector<dqCommon::OctEncodedNormal>& normals() noexcept { return m_normals; }

    const std::vector<dqGeom::Point2d>& uvParams() const noexcept { return m_uvParams; }
    std::vector<dqGeom::Point2d>& uvParams() noexcept { return m_uvParams; }

    const ColorMap& colorMap() const noexcept { return m_colorMap; }
    ColorMap& colorMap() noexcept { return m_colorMap; }

    const std::vector<uint32_t>& colors() const noexcept { return m_colors; }
    std::vector<uint32_t>& colors() noexcept { return m_colors; }

    // 1:1 Mesh.triangles getter — undefined unless type === Mesh.
    const TriangleList* triangles() const noexcept
    {
        return MeshPrimitiveType::Mesh == m_type ? m_triangles.get() : nullptr;
    }
    TriangleList* triangles() noexcept
    {
        return MeshPrimitiveType::Mesh == m_type ? m_triangles.get() : nullptr;
    }

    // 1:1 Mesh.polylines getter — undefined unless type !== Mesh.
    const MeshPolylineList* polylines() const noexcept
    {
        return MeshPrimitiveType::Mesh != m_type ? m_polylines.get() : nullptr;
    }
    MeshPolylineList* polylines() noexcept
    {
        return MeshPrimitiveType::Mesh != m_type ? m_polylines.get() : nullptr;
    }

    const std::optional<Features>& features() const noexcept { return m_features; }

    // 1:1 Mesh.toFeatureIndex — delegate to features if present.
    void toFeatureIndex(dqCommon::FeatureIndex& index) const
    {
        if (m_features)
            m_features->toFeatureIndex(index);
    }

    // --- Mutation ---
    // 1:1 Mesh.addPolyline(poly).
    void addPolyline(const MeshPolyline& poly);
    // 1:1 Mesh.addTriangle(triangle).
    void addTriangle(const Triangle& triangle);
    // 1:1 Mesh.addVertex(props) — append vertex data with uniform→non-uniform color back-fill.
    size_t addVertex(const VertexKeyProps& props);

private:
    explicit Mesh(Props props);

    // _data: TriangleList (Mesh) | MeshPolylineList (Polyline/Point). Only one is non-null.
    std::unique_ptr<TriangleList> m_triangles;
    std::unique_ptr<MeshPolylineList> m_polylines;

    Point3dList m_points;
    std::vector<dqCommon::OctEncodedNormal> m_normals;
    std::vector<dqGeom::Point2d> m_uvParams;
    ColorMap m_colorMap;  // 1:1 colorMap (was "ColorTable")
    std::vector<uint32_t> m_colors;
    std::optional<Features> m_features;
    DisplayParams m_displayParams;
    MeshPrimitiveType m_type = MeshPrimitiveType::Mesh;
    bool m_is2d = false;
    bool m_isPlanar = false;
    bool m_hasBakedLighting = false;
    bool m_isVolumeClassifier = false;
};

// 1:1 MeshList (MeshPrimitives.ts:360-368) — list of Meshes + optional shared features/range.
// Composition (mirrors GeometryList) rather than `extends Array<Mesh>`; unique_ptr<Mesh> ownership
// since Mesh is non-copyable (unique_ptr<TriangleList> member).
class MeshList {
public:
    MeshList() = default;
    // 1:1 MeshList constructor(features?, range?).
    MeshList(std::optional<dqCommon::FeatureTable> features, std::optional<dqGeom::Range3d> range)
        : m_features(std::move(features)), m_range(range)
    {
    }

    bool isEmpty() const noexcept { return m_meshes.empty(); }
    size_t length() const noexcept { return m_meshes.size(); }

    // 1:1 MeshList.push(mesh).
    void push(std::unique_ptr<Mesh> mesh) { m_meshes.push_back(std::move(mesh)); }
    Mesh* first() const { return m_meshes.empty() ? nullptr : m_meshes.front().get(); }
    Mesh* operator[](size_t i) const { return m_meshes.at(i).get(); }

    const std::optional<dqCommon::FeatureTable>& features() const noexcept { return m_features; }
    const std::optional<dqGeom::Range3d>& range() const noexcept { return m_range; }
    std::optional<dqGeom::Range3d>& rangeMut() noexcept { return m_range; }

    auto begin() noexcept { return m_meshes.begin(); }
    auto end() noexcept { return m_meshes.end(); }
    auto begin() const noexcept { return m_meshes.begin(); }
    auto end() const noexcept { return m_meshes.end(); }

private:
    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::optional<dqCommon::FeatureTable> m_features;
    std::optional<dqGeom::Range3d> m_range;
};

END_DQ_RENDER_NAMESPACE

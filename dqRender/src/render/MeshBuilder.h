// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/MeshBuilder.ts
// DanQing dqRender — MeshBuilder (tessellates polyfaces/strokes into a Mesh; @internal)
//
// 保真依据：逐方法移植 MeshBuilder.ts。MeshBuilder 持 mesh(unique_ptr<Mesh>)/tolerance/areaTolerance/
// tileRange/vertexMap(VertexMap，ctor 用 tolerance 建位置容差)/_triangleSet(lazy)/_currentPolyface。
// create/addFromPolyface/addFromPolyfaceVisitor/createTriangle/createTriangleVertices(退化三角形预先剔除)/
// addPolyline/addPointString/addStrokePointLists/addVertex(VertexMap 去重 + onInsert→mesh.addVertex)/
// addTriangle(TriangleSet 去重 + onInsert→mesh.addTriangle)/beginPolyface/endPolyface。
//
// 命名：dqRender/src/render 内部全 camelCase（与 DisplayParams.h/GeometryPrimitives.h/MeshPrimitives.h
// 既有约定一致；§3.3 TS 方法/访问器 → camelCase）。跨模块调用 dqGeom PascalCase（visitor.GetPoint 等）。
//
// DEFERRED：buildMeshEdges/EdgeInfo/EdgeParams/silhouette → Phase-N（边生成）。endPolyface 因此仅清
// _currentPolyface，不生成边（D′ 中 _currentPolyface 始终为 null —— wantEdges=false 时 beginPolyface 不建）。
// mappedTexture/computeUVParams（UV 纹理坐标）→ Phase-N。
// @internal；仅 MeshBuilderMap 消费。
#pragma once

#include "MeshPrimitives.h"
#include "Primitives.h"
#include "Strokes.h"
#include "VertexKey.h"

#include <dqCommon/TextureMapping.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/PolyfaceVisitor.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 MeshEdgeCreationOptions.Type (MeshBuilder.ts:289-297) — bitmask of which edges to generate.
enum class MeshEdgeCreationType : uint16_t {
    NoEdges = 0x0000,
    CreaseEdges = 0x0001 << 1,   // = 2
    SmoothEdges = 0x0001 << 2,   // = 4
    CreateChains = 0x0001 << 3,  // = 8
    DefaultEdges = CreaseEdges,  // = 2
    AllEdges = CreaseEdges | SmoothEdges,  // = 6
};

// 1:1 MeshEdgeCreationOptions (MeshBuilder.ts:276-285).
class MeshEdgeCreationOptions {
public:
    // 1:1 minCreaseAngle = 20.0 * Angle.radiansPerDegree.
    static constexpr double kRadiansPerDegree = 0.017453292519943295;  // M_PI/180

    explicit MeshEdgeCreationOptions(MeshEdgeCreationType type = MeshEdgeCreationType::NoEdges)
        : m_type(type)
    {
    }

    MeshEdgeCreationType type() const noexcept { return m_type; }
    double minCreaseAngle() const noexcept { return 20.0 * kRadiansPerDegree; }

    // 1:1 MeshEdgeCreationOptions.generateAllEdges / generateNoEdges / generateCreaseEdges / createEdgeChains.
    bool generateAllEdges() const noexcept { return m_type == MeshEdgeCreationType::AllEdges; }
    bool generateNoEdges() const noexcept { return m_type == MeshEdgeCreationType::NoEdges; }
    bool generateCreaseEdges() const noexcept
    {
        return 0 != (static_cast<uint16_t>(m_type) & static_cast<uint16_t>(MeshEdgeCreationType::CreaseEdges));
    }
    bool createEdgeChains() const noexcept
    {
        return 0 != (static_cast<uint16_t>(m_type) & static_cast<uint16_t>(MeshEdgeCreationType::CreateChains));
    }

private:
    MeshEdgeCreationType m_type;
};

// 1:1 MeshBuilderPolyface (MeshBuilder.ts:300-310) — bookkeeping for buildMeshEdges (Phase-N). Vertexed
// here 1:1; D′ never populates it (wantEdges=false), so the vertexIndexMap stays empty.
class MeshBuilderPolyface {
public:
    MeshBuilderPolyface(const dqGeom::Polyface& polyface, const MeshEdgeCreationOptions& edgeOptions,
                        size_t baseTriangleNumber)
        : m_polyface(polyface)
        , m_edgeOptions(edgeOptions)
        , m_baseTriangleNumber(baseTriangleNumber)
    {
    }

    const dqGeom::Polyface& polyface() const noexcept { return m_polyface; }
    const MeshEdgeCreationOptions& edgeOptions() const noexcept { return m_edgeOptions; }
    size_t baseTriangleNumber() const noexcept { return m_baseTriangleNumber; }
    std::map<uint32_t, uint32_t>& vertexIndexMap() noexcept { return m_vertexIndexMap; }

private:
    const dqGeom::Polyface& m_polyface;
    MeshEdgeCreationOptions m_edgeOptions;
    std::map<uint32_t, uint32_t> m_vertexIndexMap;
    size_t m_baseTriangleNumber;
};

// 1:1 MeshBuilder (MeshBuilder.ts:22-254).
class MeshBuilder {
public:
    // 1:1 MeshBuilder.Props (extends Mesh.Props + tolerance + areaTolerance).
    struct Props : Mesh::Props {
        double tolerance = 0.0;
        double areaTolerance = 0.0;
    };

    // 1:1 MeshBuilder.PolyfaceOptions.
    struct PolyfaceOptions {
        bool includeParams = false;
        uint32_t fillColor = 0;
        std::optional<dqCommon::TextureMapping> mappedTexture;  // Phase-N: always nullopt in D′
        MeshEdgeCreationOptions edgeOptions;
    };

    // 1:1 MeshBuilder.PolyfaceVisitorOptions (extends PolyfaceOptions + triangleCount + haveParam).
    struct PolyfaceVisitorOptions : PolyfaceOptions {
        size_t triangleCount = 0;
        bool haveParam = false;
    };

    // 1:1 MeshBuilder.create(props).
    static std::unique_ptr<MeshBuilder> create(Props props)
    {
        const double tolerance = props.tolerance;
        const double areaTolerance = props.areaTolerance;
        dqGeom::Range3d range = props.range;
        auto mesh = Mesh::create(std::move(props));  // Props is-a Mesh::Props
        return std::unique_ptr<MeshBuilder>(new MeshBuilder(std::move(mesh), tolerance, areaTolerance, range));
    }

    // --- Accessors (1:1 readonly fields) ---
    Mesh& mesh() const noexcept { return *m_mesh; }
    std::unique_ptr<Mesh> releaseMesh() noexcept { return std::move(m_mesh); }
    const VertexMap& vertexMap() const noexcept { return m_vertexMap; }
    double tolerance() const noexcept { return m_tolerance; }
    double areaTolerance() const noexcept { return m_areaTolerance; }
    const dqGeom::Range3d& tileRange() const noexcept { return m_tileRange; }
    MeshBuilderPolyface* currentPolyface() const noexcept { return m_currentPolyface.get(); }
    const DisplayParams& displayParams() const noexcept { return m_mesh->displayParams(); }
    void setDisplayParams(const DisplayParams& params) { m_mesh->displayParams() = params; }

    // 1:1 MeshBuilder.triangleSet (lazy).
    TriangleSet& triangleSet()
    {
        if (!m_triangleSet)
            m_triangleSet = std::make_unique<TriangleSet>();
        return *m_triangleSet;
    }

    // 1:1 MeshBuilder.addStrokePointLists (MeshBuilder.ts:73-80).
    void addStrokePointLists(const StrokesPrimitivePointLists& strokes, bool isDisjoint, uint32_t fillColor,
                             const std::optional<dqCommon::Feature>& feature);

    // 1:1 MeshBuilder.addFromPolyface (MeshBuilder.ts:87-96).
    void addFromPolyface(const dqGeom::IndexedPolyface& polyface, const PolyfaceOptions& props,
                         const std::optional<dqCommon::Feature>& feature);

    // 1:1 MeshBuilder.addFromPolyfaceVisitor (MeshBuilder.ts:102-125).
    void addFromPolyfaceVisitor(dqGeom::PolyfaceVisitor& visitor, const PolyfaceOptions& options,
                                const std::optional<dqCommon::Feature>& feature);

    // 1:1 MeshBuilder.createTriangleVertices (MeshBuilder.ts:127-160) — returns nullopt if degenerate.
    std::optional<std::array<VertexKeyProps, 3>> createTriangleVertices(
        size_t triangleIndex, dqGeom::PolyfaceVisitor& visitor, const PolyfaceVisitorOptions& options,
        const std::optional<dqCommon::Feature>& feature) const;

    // 1:1 MeshBuilder.createTriangle (MeshBuilder.ts:162-199) — returns nullopt if degenerate.
    std::optional<Triangle> createTriangle(size_t triangleIndex, dqGeom::PolyfaceVisitor& visitor,
                                           const PolyfaceVisitorOptions& options,
                                           const std::optional<dqCommon::Feature>& feature);

    // 1:1 MeshBuilder.addPolyline (MeshBuilder.ts:202-210).
    void addPolyline(const std::vector<dqGeom::Point3d>& points, uint32_t fillColor,
                     const std::optional<dqCommon::Feature>& feature);
    // 1:1 MeshBuilder.addPointString (MeshBuilder.ts:213-221).
    void addPointString(const std::vector<dqGeom::Point3d>& points, uint32_t fillColor,
                        const std::optional<dqCommon::Feature>& feature);

    // 1:1 MeshBuilder.beginPolyface (MeshBuilder.ts:223-228).
    void beginPolyface(const dqGeom::Polyface& polyface, const MeshEdgeCreationOptions& options);
    // 1:1 MeshBuilder.endPolyface (MeshBuilder.ts:230-237). Edge generation (buildMeshEdges) is Phase-N.
    void endPolyface() { m_currentPolyface.reset(); }

    // 1:1 MeshBuilder.addVertex (MeshBuilder.ts:239-243).
    int addVertex(const VertexKeyProps& vertex, bool addToMeshOnInsert = true);
    // 1:1 MeshBuilder.addTriangle (MeshBuilder.ts:245-253).
    void addTriangle(const Triangle& triangle);

private:
    MeshBuilder(std::unique_ptr<Mesh> mesh, double tolerance, double areaTolerance,
                const dqGeom::Range3d& tileRange);

    std::unique_ptr<Mesh> m_mesh;
    VertexMap m_vertexMap;
    std::unique_ptr<TriangleSet> m_triangleSet;
    std::unique_ptr<MeshBuilderPolyface> m_currentPolyface;
    double m_tolerance = 0.0;
    double m_areaTolerance = 0.0;
    dqGeom::Range3d m_tileRange;
};

END_DQ_RENDER_NAMESPACE

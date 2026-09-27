// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Mesh graphic (concrete mesh render graphic)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Mesh.ts
//
// MeshRenderGeometry + MeshGraphic: the concrete mesh graphic that wraps
// surface, edge, and polyline geometries from a single mesh.
#pragma once

#include "Graphic.h"
#include "MeshData.h"
#include "SurfaceGeometry.h"
#include "IndexedEdgeGeometry.h"
#include "InstancedGeometry.h"
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <memory>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// MeshGraphic — concrete mesh render graphic
// (Ported from: itwinjs-core Mesh.ts MeshGraphic)
// ---------------------------------------------------------------------------
class MeshGraphic : public Graphic {
public:
    explicit MeshGraphic(rhi::Driver& driver) noexcept
        : m_driver(driver) {}
    ~MeshGraphic() override;

    MeshGraphic(MeshGraphic const&) = delete;
    MeshGraphic& operator=(MeshGraphic const&) = delete;

    /// add a surface geometry to this mesh.
    void addSurface(std::unique_ptr<SurfaceGeometry> surface)
    {
        m_surfaceInstanced.push_back(false);
        m_surfaces.push_back(std::move(surface));
    }

    /// add an instanced surface: the base surface geometry and the instance
    /// buffers are owned here; the InstancedGeometry wrapper observes the
    /// base surface (repr not owned — InstancedGeometry.ts:348 _repr shared
    /// with the mesh's other geometries via Primitive.createShared).
    /// Ported from: itwinjs-core Mesh.ts MeshGraphic.create(geometry, buffers)
    /// (:119-121) + Primitive.createShared(geometry, instances)（:123-130——
    /// 同一 _instances 包裹 mesh 的每个 CachedGeometry；DanQing 本期接线
    /// surface 成员——带 instances 的 primitive 其 edges 亦应共享实例缓冲
    /// （TODO 登记：本资产 prim1 边缘 numVisible=0 无边缘几何，未触发））。
    void addInstancedSurface(std::unique_ptr<SurfaceGeometry> surface,
                             InstanceBuffers* buffers)
    {
        m_surfaceInstanced.push_back(true);
        m_surfaces.push_back(std::move(surface));
        auto* repr = m_surfaces.back().get();
        m_instancedSurfaces.push_back(
            std::make_unique<InstancedGeometry>(repr, buffers));
    }

    /// Get instanced surface wrappers（parallel to the instanced entries of
    /// getSurfaces()——第 N 个 true 的 m_surfaceInstanced 下标对应下标 N）。
    std::vector<std::unique_ptr<InstancedGeometry>> const& getInstancedSurfaces()
        const noexcept
    {
        return m_instancedSurfaces;
    }

    /// add an edge geometry to this mesh.
    void addEdge(std::unique_ptr<EdgeGeometry> edge)
    {
        m_edges.push_back(std::move(edge));
    }

    /// add an indexed edge geometry to this mesh.
    /// Ported from: Mesh.ts:36（indexedEdges?: IndexedEdgeGeometry——独立字段）
    /// + :141（addPrimitive(geometry.indexedEdges)——排在 segmentEdges/
    /// silhouetteEdges 之后、polylineEdges 之前）。
    void addIndexedEdge(std::unique_ptr<IndexedEdgeGeometry> edge)
    {
        m_indexedEdges.push_back(std::move(edge));
    }

    /// add a polyline geometry to this mesh.
    void addPolyline(std::unique_ptr<PolylineGeometry> polyline)
    {
        m_polylines.push_back(std::move(polyline));
    }

    /// add a point string geometry to this mesh.
    void addPointString(std::unique_ptr<PointStringGeometry> pointString)
    {
        m_pointStrings.push_back(std::move(pointString));
    }

    /// add a point cloud geometry to this mesh.
    void addPointCloud(std::unique_ptr<PointCloudGeometry> pointCloud)
    {
        m_pointClouds.push_back(std::move(pointCloud));
    }

    /// Get surface geometries.
    std::vector<std::unique_ptr<SurfaceGeometry>> const& getSurfaces() const noexcept
    {
        return m_surfaces;
    }

    /// Get edge geometries.
    std::vector<std::unique_ptr<EdgeGeometry>> const& getEdges() const noexcept
    {
        return m_edges;
    }

    /// Get indexed edge geometries.（Mesh.ts:36 indexedEdges 独立字段。）
    std::vector<std::unique_ptr<IndexedEdgeGeometry>> const& getIndexedEdges() const noexcept
    {
        return m_indexedEdges;
    }

    /// Get polyline geometries.
    std::vector<std::unique_ptr<PolylineGeometry>> const& getPolylines() const noexcept
    {
        return m_polylines;
    }

    /// Get point string geometries.
    std::vector<std::unique_ptr<PointStringGeometry>> const& getPointStrings() const noexcept
    {
        return m_pointStrings;
    }

    /// Get point cloud geometries.
    std::vector<std::unique_ptr<PointCloudGeometry>> const& getPointClouds() const noexcept
    {
        return m_pointClouds;
    }

    /// Check if this mesh has any geometry.
    bool isEmpty() const noexcept
    {
        return m_surfaces.empty() && m_edges.empty() && m_indexedEdges.empty()
            && m_polylines.empty() && m_pointStrings.empty()
            && m_pointClouds.empty();
    }

    /// Wire the shared vertex-side GL resources (vbo + vbih + vbh) created once by
    /// MeshRenderGeometry::create and reused by every geometry in this mesh. The
    /// MeshGraphic dtor releases them; each geometry's dtor releases its own ibh +
    /// render primitive.
    void setSharedVertexResources(rhi::BufferObjectHandle vbo,
                                  rhi::VertexBufferInfoHandle vbih,
                                  rhi::VertexBufferHandle vbh) noexcept
    {
        m_vbo = vbo;
        m_vbih = vbih;
        m_vbh = vbh;
    }

    // --- Graphic interface ---
    void addCommands(RenderCommands& commands) override
    {
        size_t instancedIndex = 0;
        for (size_t i = 0; i < m_surfaces.size(); ++i) {
            auto& surface = m_surfaces[i];
            if (!surface) continue;
            // Mesh.ts:138-143 addPrimitive 顺序——surface 最先；带 instances
            // 的 surface 以其 InstancedGeometry 包裹体入列（参考
            // Primitive.createShared(geometry, this._instances)）。
            if (i < m_surfaceInstanced.size() && m_surfaceInstanced[i])
                commands.addPrimitive(m_instancedSurfaces[instancedIndex++].get());
            else
                commands.addPrimitive(surface.get());
        }
        for (auto& edge : m_edges) {
            if (edge) commands.addPrimitive(edge.get());
        }
        // Mesh.ts:141 addPrimitive(geometry.indexedEdges)——segment/silhouette
        // 之后、polylines 之前。
        for (auto& edge : m_indexedEdges) {
            if (edge) commands.addPrimitive(edge.get());
        }
        for (auto& polyline : m_polylines) {
            if (polyline) commands.addPrimitive(polyline.get());
        }
        for (auto& pointString : m_pointStrings) {
            if (pointString) commands.addPrimitive(pointString.get());
        }
        for (auto& pointCloud : m_pointClouds) {
            if (pointCloud) commands.addPrimitive(pointCloud.get());
        }
    }

private:
    rhi::Driver& m_driver;
    rhi::BufferObjectHandle m_vbo;
    rhi::VertexBufferInfoHandle m_vbih;
    rhi::VertexBufferHandle m_vbh;
    std::vector<std::unique_ptr<SurfaceGeometry>> m_surfaces;
    // TD-25：实例化 surface 包裹体（观察 m_surfaces 对应项，非拥有）+
    // 与 m_surfaces 平行的 instanced 标记（addCommands 的派发选择）。
    // 成员序：m_surfaces 先于 m_instancedSurfaces 声明 → 析构反序 wrapper
    // 先死、surface 后死——wrapper 析构只删 InstanceBuffers（InstancedGeometry
    // dtor 不触 repr），surface 全程存活，无悬空窗口。
    std::vector<std::unique_ptr<InstancedGeometry>> m_instancedSurfaces;
    std::vector<bool> m_surfaceInstanced;
    std::vector<std::unique_ptr<EdgeGeometry>> m_edges;
    std::vector<std::unique_ptr<IndexedEdgeGeometry>> m_indexedEdges;
    std::vector<std::unique_ptr<PolylineGeometry>> m_polylines;
    std::vector<std::unique_ptr<PointStringGeometry>> m_pointStrings;
    std::vector<std::unique_ptr<PointCloudGeometry>> m_pointClouds;
};

// ---------------------------------------------------------------------------
// MeshRenderGeometry — factory for creating MeshGraphic from MeshData
// (Ported from: itwinjs-core Mesh.ts MeshRenderGeometry)
// ---------------------------------------------------------------------------
class MeshRenderGeometry {
public:
    /// Create a MeshGraphic from raw mesh data.
    /// @param driver RHI driver for GPU resource creation.
    /// @param meshData The raw mesh data.
    /// @param defaultColor Default color if mesh has no per-vertex colors.
    /// @return The created MeshGraphic, or nullptr on failure.
    static std::unique_ptr<MeshGraphic> create(rhi::Driver& driver, MeshData const& meshData,
                                                uint32_t defaultColor = 0xFF8080FF);

    /// Create a MeshGraphic directly from an accumulator Mesh (PR F bug-fix path).
    /// Builds a MeshData (quantized positions, oct-normal pairs, resolved colors/features, triangle
    /// indices) then delegates to the MeshData overload above — reusing the verified GPU upload.
    /// @param driver RHI driver for GPU resource creation.
    /// @param mesh The accumulator Mesh (MeshPrimitives).
    /// @param defaultColor Default color (tbgr) if the mesh has no color table.
    /// @return The created MeshGraphic (with addCommands — a Graphic), or nullptr if no triangle data.
    static std::unique_ptr<MeshGraphic> create(rhi::Driver& driver, class Mesh const& mesh,
                                                uint32_t defaultColor = 0xFF8080FF);
};

END_DQ_RENDER_NAMESPACE

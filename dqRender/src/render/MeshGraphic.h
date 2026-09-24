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
        m_surfaces.push_back(std::move(surface));
    }

    /// add an edge geometry to this mesh.
    void addEdge(std::unique_ptr<EdgeGeometry> edge)
    {
        m_edges.push_back(std::move(edge));
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
        return m_surfaces.empty() && m_edges.empty() && m_polylines.empty() && m_pointStrings.empty()
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
        for (auto& surface : m_surfaces) {
            if (surface) commands.addPrimitive(surface.get());
        }
        for (auto& edge : m_edges) {
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
    std::vector<std::unique_ptr<EdgeGeometry>> m_edges;
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

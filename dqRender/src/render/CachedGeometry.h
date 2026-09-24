// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GPU-ready geometry base class hierarchy
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/CachedGeometry.ts
//
// Abstract base for all GPU-ready geometry.  Concrete subclasses know their
// TechniqueId, render pass, and how to issue the actual draw call.
//
// Contains: CachedGeometry, LUTGeometry, IndexedGeometryParams, IndexedGeometry
#pragma once

#include "TechniqueImpl.h"
#include "ColorInfo.h"
#include "Material.h"
#include "dqRender/RenderMemory.h"
#include "gl/RenderFlags.h"
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <dqGeom/Point3d.h>

#include <array>
#include <cstdint>
#include <memory>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Forward declarations for types used in CachedGeometry methods.
class TargetImpl;
class DrawParams;
class ShaderProgramParams;
class VertexLutTexture;
class PolylineBuffers;
class InstancedGeometry;
class SurfaceGeometry;
class MeshGeometry;
class EdgeGeometry;
class IndexedEdgeGeometry;
class SilhouetteEdgeGeometry;
class PointCloudGeometry;
class PlanarGridGraphic;  // defined in PlanarGridGraphic.h, derives from CachedGeometry
class SkySphereViewportQuadGeometry;  // defined in ViewportQuadGeometry.h
class RealityMeshGeometry;

// Bring GL::Pass and GL::RenderOrder into dqRender namespace for convenience.
using GL::Pass;
using GL::RenderOrder;
using GL::CompositeFlags;

// ---------------------------------------------------------------------------
// FlashMode — how to flash/highlight geometry
// Ported from: itwinjs-core FlashSettings.ts FlashMode
// ---------------------------------------------------------------------------
enum class FlashMode : uint8_t {
    None = 0,
    Hilite = 1,
    Brighten = 2,
};

// ---------------------------------------------------------------------------
// BoundaryType — volume classifier boundary type
// Ported from: itwinjs-core CachedGeometry.ts BoundaryType
// ---------------------------------------------------------------------------
enum class BoundaryType : uint8_t {
    Outside = 0,
    Inside = 1,
    Selected = 2,
};

// ---------------------------------------------------------------------------
// BlurType — blur pass type
// Ported from: itwinjs-core CachedGeometry.ts BlurType
// ---------------------------------------------------------------------------
enum class BlurType : uint8_t {
    NoTest = 0,
    TestOrder = 1,
};

// ---------------------------------------------------------------------------
// CachedGeometry — abstract GPU geometry
// (Ported from: itwinjs-core CachedGeometry.ts line 51-185)
// ---------------------------------------------------------------------------
class CachedGeometry {
public:
    virtual ~CachedGeometry() = default;

    // --- Casting accessors ---
    // IMPORTANT: Do NOT use static_cast for downcasting. Use these accessor
    // functions instead. InstancedGeometry wraps a shared base geometry —
    // direct casts will fail, but these accessors forward correctly.
    // Ported from: itwinjs-core CachedGeometry.ts line 59-69
    virtual CachedGeometry* asLUT() { return nullptr; }
    virtual SurfaceGeometry* asSurface() { return nullptr; }
    virtual MeshGeometry* asMesh() { return nullptr; }
    virtual EdgeGeometry* asEdge() { return nullptr; }
    virtual IndexedEdgeGeometry* asIndexedEdge() { return nullptr; }
    virtual RealityMeshGeometry* asRealityMesh() { return nullptr; }
    virtual SilhouetteEdgeGeometry* asSilhouette() { return nullptr; }
    virtual InstancedGeometry* asInstanced() { return nullptr; }
    virtual PointCloudGeometry* asPointCloud() { return nullptr; }
    virtual PlanarGridGraphic* asPlanarGrid() { return nullptr; }
    virtual SkySphereViewportQuadGeometry* asSkySphere() { return nullptr; }

    bool isInstanced() const { return const_cast<CachedGeometry*>(this)->asInstanced() != nullptr; }

    // --- View-independent origin ---
    // Ported from: itwinjs-core CachedGeometry.ts viewIndependentOrigin (line 117)
    // Base geometry has none; subclasses (e.g. RealityMesh) override.
    virtual dqGeom::Point3d const* viewIndependentOrigin() const noexcept { return nullptr; }
    bool isViewIndependent() const noexcept { return nullptr != viewIndependentOrigin(); }

    // --- Render state queries ---
    virtual bool alwaysRenderTranslucent() const { return false; }
    virtual bool allowColorOverride() const { return true; }

    // --- Surface texture (s_texture source for the compositor bind site) ---
    // Ported from: itwinjs-core Surface.ts addTexture (:596-609) — geometry-sourced
    // baseColor texture. asSurface() returns the concrete SurfaceGeometry*, so a
    // second virtual covers non-MeshGeometry surface paths (PolyfaceGraphic).
    virtual rhi::TextureHandle getSurfaceTexture() const { return {}; }

    // --- Normal map (s_normalMap source for the compositor bind site) ---
    // Ported from: itwinjs-core Surface.ts addNormal (:612-620 — surfGeom.normalMap
    // bound when useNormalMap) + MeshData.ts :27/:61-67 (geometry-sourced normal
    // map from the material's textureMapping.normalMapParams). Mirrors the
    // getSurfaceTexture routing above.
    virtual rhi::TextureHandle getNormalMapTexture() const { return {}; }

    // --- Normal map scale (u_normalMapScale source) ---
    // Ported from: itwinjs-core Surface.ts addNormal (:541-550 —
    // normalMapParams.scale ?? 1.0, negated when greenUp). The greenUp
    // negation is applied by the caller that binds the normal map (glTF
    // binds greenUp=true → -1.0, GltfReader.ts :2587).
    virtual float getNormalMapScale() const { return 1.0f; }

    // --- Abstract render interface ---
    virtual TechniqueId getTechniqueId() const = 0;
    virtual Pass getPass() const = 0;
    virtual RenderOrder getRenderOrder() const { return RenderOrder::None; }

    /// Line/point weight (symbology weight). Consumed by the PointString shader
    /// as gl_PointSize — itwinjs PointString.ts:22 `gl_PointSize = lineWeight`,
    /// sourced from PointString.ts:62 `_getLineWeight() { return this.weight }`.
    /// Default 1.0; point strings override with their symbology weight (e.g. the
    /// ACS Z-axis tip renders at weight 6). Ported from: itwinjs-core LUTGeometry._getLineWeight().
    virtual float getLineWeight() const noexcept { return 1.0f; }

    // --- Lit/material queries ---
    virtual bool isLitSurface() const { return false; }
    virtual bool hasBakedLighting() const { return false; }
    virtual bool hasAnimation() const { return false; }

    // --- Quantized positions ---
    // If false, positions are not quantized. qOrigin/qScale can still derive
    // range but won't be passed to the shader.
    // Ported from: itwinjs-core CachedGeometry.ts line 94-102
    virtual bool usesQuantizedPositions() const { return true; }
    virtual float const* getQOrigin() const { return nullptr; }
    virtual float const* getQScale() const { return nullptr; }

    // --- Draw ---
    virtual void draw(rhi::Driver& driver) = 0;

    // --- Bind primitive (for instanced drawing) ---
    // Binds the VAO without issuing a draw call. Used by InstancedGeometry
    // to set up the base geometry's VAO before adding instance attributes.
    virtual void bindPrimitive(rhi::Driver& /*driver*/) {}

    // --- Get index count (for instanced drawing) ---
    // Subclasses that have indexed geometry should override this.
    virtual uint32_t getDrawIndexCount() const { return 0; }

    // --- Material ---
    virtual RenderMaterialInternal const* getMaterialInfo() const { return nullptr; }
    bool hasMaterialAtlas() const {
        auto const* mat = getMaterialInfo();
        return mat != nullptr && mat->isAtlas();
    }

    // --- Polyline buffers ---
    virtual PolylineBuffers const* getPolylineBuffers() const { return nullptr; }

    // --- Feature/animation queries ---
    virtual bool hasFeatures() const { return false; }

    // --- View-independent origin ---
    // Ported from: itwinjs-core CachedGeometry.ts line 117-118
    // Returns the origin for view-independent rendering (e.g., for 2D overlays in 3D views).
    // Subclasses that support view-independent rendering override this.
    // Not stored here — subclasses manage it directly.

    // --- Thematic display ---
    virtual bool supportsThematicDisplay() const { return false; }

    // --- Cumulative distances ---
    virtual bool hasCumulativeDistances() const { return false; }

    // --- Edge detection ---
    // Ported from: itwinjs-core CachedGeometry.ts line 123-133
    bool isEdge() const {
        switch (getRenderOrder()) {
            case RenderOrder::Edge:
            case RenderOrder::Silhouette:
            case RenderOrder::PlanarEdge:
            case RenderOrder::PlanarSilhouette:
                return true;
            default:
                return false;
        }
    }

    // --- Line weight/code ---
    // Ported from: itwinjs-core CachedGeometry.ts line 76-78
    virtual float getLineWeight(TargetImpl const& target) const;
    virtual uint32_t getLineCode(TargetImpl const& target) const;

    // --- WoW reversal ---
    // Ported from: itwinjs-core CachedGeometry.ts line 74, 134-136
    // Returns true if white portions should render as black on white background.
    virtual bool wantWoWReversal(TargetImpl const& /*target*/) const { return false; }

    // --- Flash mode ---
    // Ported from: itwinjs-core CachedGeometry.ts line 152-163
    virtual FlashMode getFlashMode(TargetImpl const& target) const;

    // --- Monochrome ---
    // Ported from: itwinjs-core CachedGeometry.ts line 165-166
    virtual bool wantMixMonochromeColor(TargetImpl const& /*target*/) const { return false; }
    virtual bool wantMonochrome(TargetImpl const& /*target*/) const { return true; }

    // --- Memory statistics ---
    virtual void collectStatistics(RenderMemory::Statistics& stats) const = 0;

    // --- Range computation ---
    // Ported from: itwinjs-core CachedGeometry.ts line 170-184
    // Computes the range from quantized origin and scale.
    // Mutable because it caches the computed range.
    struct Range3d {
        float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
        float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
    };

    Range3d computeRange() const {
        if (!m_rangeValid) {
            float const* origin = getQOrigin();
            float const* scale = getQScale();
            if (origin && scale) {
                m_range.minX = origin[0];
                m_range.minY = origin[1];
                m_range.minZ = origin[2];
                m_range.maxX = 0xFFFF * scale[0] + origin[0];
                m_range.maxY = 0xFFFF * scale[1] + origin[1];
                m_range.maxZ = 0xFFFF * scale[2] + origin[2];
            }
            m_rangeValid = true;
        }
        return m_range;
    }

private:
    mutable Range3d m_range;
    mutable bool m_rangeValid = false;
};

// ---------------------------------------------------------------------------
// LUTGeometry — geometry drawn using vertex look-up texture
// (Ported from: itwinjs-core CachedGeometry.ts line 190-216)
//
// Geometry which is drawn using indices into a look-up texture of vertex data,
// via drawArrays(). The LUT texture contains all vertex attributes.
// ---------------------------------------------------------------------------
class LUTGeometry : public CachedGeometry {
public:
    ~LUTGeometry() override = default;

    // --- LUT access ---
    virtual VertexLutTexture const* getLut() const = 0;

    // --- Casting ---
    CachedGeometry* asLUT() override { return this; }

    // --- View-independent origin ---
    // Ported from: itwinjs-core LUTGeometry.ts line 191
    // Not stored here — subclasses manage it.

    // --- Color ---
    // override if color varies based on the target.
    virtual ColorInfo getColor(TargetImpl const& target) const;

    // --- Quantized positions from LUT ---
    bool usesQuantizedPositions() const override;
    float const* getQOrigin() const override;
    float const* getQScale() const override;
    bool hasAnimation() const override;

    // --- Draw with instancing support ---
    // Ported from: itwinjs-core LUTGeometry.ts line 200-201
    virtual void drawInstanced(uint32_t numInstances, void const* instanceBuffersContainer) = 0;

protected:
    LUTGeometry() = default;
};

// ---------------------------------------------------------------------------
// IndexedGeometryParams — parameters for indexed geometry
// (Ported from: itwinjs-core CachedGeometry.ts line 221-261)
//
// Holds vertex positions (quantized) and an index buffer for gl.drawElements().
// ---------------------------------------------------------------------------
class IndexedGeometryParams {
public:
    IndexedGeometryParams(rhi::RenderPrimitiveHandle primitive,
                          rhi::BufferObjectHandle positions,
                          rhi::IndexBufferHandle indices,
                          uint32_t numIndices)
        : m_primitive(primitive)
        , m_positions(positions)
        , m_indices(indices)
        , m_numIndices(numIndices)
    {
    }

    ~IndexedGeometryParams() = default;

    IndexedGeometryParams(IndexedGeometryParams const&) = delete;
    IndexedGeometryParams& operator=(IndexedGeometryParams const&) = delete;
    IndexedGeometryParams(IndexedGeometryParams&&) noexcept = default;
    IndexedGeometryParams& operator=(IndexedGeometryParams&&) noexcept = default;

    rhi::RenderPrimitiveHandle getPrimitive() const noexcept { return m_primitive; }
    rhi::BufferObjectHandle getPositions() const noexcept { return m_positions; }
    rhi::IndexBufferHandle getIndices() const noexcept { return m_indices; }
    uint32_t getNumIndices() const noexcept { return m_numIndices; }

    bool isValid() const noexcept { return m_primitive != rhi::RenderPrimitiveHandle{}; }

private:
    rhi::RenderPrimitiveHandle m_primitive;
    rhi::BufferObjectHandle m_positions;
    rhi::IndexBufferHandle m_indices;
    uint32_t m_numIndices = 0;
};

// ---------------------------------------------------------------------------
// IndexedGeometry — geometry drawn with gl.drawElements()
// (Ported from: itwinjs-core CachedGeometry.ts line 266-288)
//
// A geometric primitive rendered using drawElements() with one or more vertex
// buffers indexed by an index buffer.
// ---------------------------------------------------------------------------
class IndexedGeometry : public CachedGeometry {
public:
    explicit IndexedGeometry(IndexedGeometryParams params)
        : m_params(std::move(params))
    {
    }

    ~IndexedGeometry() override = default;

    IndexedGeometry(IndexedGeometry const&) = delete;
    IndexedGeometry& operator=(IndexedGeometry const&) = delete;

    // --- CachedGeometry interface ---
    void draw(rhi::Driver& driver) override {
        if (m_params.isValid()) {
            driver.bindRenderPrimitive(m_params.getPrimitive());
            driver.draw2(0, m_params.getNumIndices(), 0);
        }
    }

    void bindPrimitive(rhi::Driver& driver) override {
        if (m_params.isValid()) {
            driver.bindRenderPrimitive(m_params.getPrimitive());
        }
    }

    uint32_t getDrawIndexCount() const override {
        return m_params.getNumIndices();
    }

    float const* getQOrigin() const override { return m_qOrigin.data(); }
    float const* getQScale() const override { return m_qScale.data(); }

    void setQuantization(float originX, float originY, float originZ,
                         float scaleX, float scaleY, float scaleZ) {
        m_qOrigin = {originX, originY, originZ};
        m_qScale = {scaleX, scaleY, scaleZ};
    }

    void setTechniqueId(TechniqueId id) { m_techniqueId = id; }
    void setPass(Pass pass) { m_pass = pass; }
    void setRenderOrder(RenderOrder order) { m_renderOrder = order; }

    TechniqueId getTechniqueId() const override { return m_techniqueId; }
    Pass getPass() const override { return m_pass; }
    RenderOrder getRenderOrder() const override { return m_renderOrder; }

    void collectStatistics(RenderMemory::Statistics& stats) const override {
        stats.addIndexedEdges(
                        static_cast<uint64_t>(m_params.getNumIndices() * 4));
    }

    IndexedGeometryParams const& getParams() const { return m_params; }

protected:
    bool wantWoWReversal(TargetImpl const&) const override { return false; }

private:
    IndexedGeometryParams m_params;
    TechniqueId m_techniqueId = TechniqueId::Surface;
    Pass m_pass = Pass::Opaque;
    RenderOrder m_renderOrder = RenderOrder::None;
    std::array<float, 3> m_qOrigin = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> m_qScale = {1.0f, 1.0f, 1.0f};
};

END_DQ_RENDER_NAMESPACE

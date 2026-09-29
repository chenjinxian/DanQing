// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface geometry
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/SurfaceGeometry.ts
//
// Concrete surface geometry for rendering solid surfaces.
// The primary geometry type for BIM elements.
#pragma once

#include "MeshGeometry.h"
#include "ViewFlags.h"
#include "dqRender/RenderMemory.h"
#include "VertexLutTexture.h"
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <dqCommon/ColorDef.h>

#include <algorithm>  // std::clamp (PolylineGeometry::getLineWeight)

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// SurfaceGeometry — concrete surface geometry
// (Ported from: itwinjs-core SurfaceGeometry.ts)
// ---------------------------------------------------------------------------
class SurfaceGeometry : public MeshGeometry {
public:
    // driver + ibh own this geometry's per-geometry GL resources (ibh is released
    // in the dtor; the render primitive is set via setPrimitive after construction).
    SurfaceGeometry(rhi::Driver& driver, rhi::IndexBufferHandle ibh,
                    uint32_t numIndices, SurfaceType surfaceType, bool isPlanar, bool hasTextures);

    // LUT 形态（量化 imdl 网格）。
    // Ported from: itwinjs-core SurfaceGeometry.ts —— 构造的 BuffersContainer 以
    // a_pos 24-bit UBYTE3 索引 attribute 承载全部顶点引用（:378-387，参考
    // BufferParameters.create(attrPos.location, 3, UnsignedByte, ...)）；
    // _draw 走 gl.drawArrays（无 element index buffer，:150-162）。LUT 持有/
    // 析构对齐 CachedGeometry.ts LUTGeometry 的 lut 字段（:195-210）+ 在仓模板
    // PolylineGeometry。`lutIndexBuffer` 即 a_qPosition 的 24-bit 索引流
    // （UBYTE3，stride 3，由创建点 Task 5 upload 后经此传入）；primitive（VAO，
    // 内绑 a_qPosition → m_lutIndexBuffer）由创建点 setPrimitive 设置。
    // `lutVertexBuffer`/`lutVertexBufferInfo` = a_qPosition VAO 的 RHI 顶点缓冲
    // 句柄（bindRenderPrimitive 每次绑定都从 vbh 重建 attrib 指针，句柄必须与
    // 几何同寿；析构清单对齐 PolylineGeometry::~PolylineGeometry）。
    SurfaceGeometry(rhi::Driver& driver, VertexLutTexture lut,
                    rhi::BufferObjectHandle lutIndexBuffer,
                    uint32_t numIndices, SurfaceType surfaceType,
                    bool isPlanar, bool hasTextures,
                    rhi::VertexBufferHandle lutVertexBuffer = {},
                    rhi::VertexBufferInfoHandle lutVertexBufferInfo = {});
    ~SurfaceGeometry() override;

    // --- Casting accessors (Ported from: itwinjs-core CachedGeometry.ts asSurface/asMesh) ---
    SurfaceGeometry* asSurface() override { return this; }
    MeshGeometry* asMesh() override { return this; }

    SurfaceGeometry(SurfaceGeometry const&) = delete;
    SurfaceGeometry& operator=(SurfaceGeometry const&) = delete;

    // --- CachedGeometry interface ---
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::Surface; }
    Pass getPass() const noexcept override;
    RenderOrder getRenderOrder() const noexcept override;
    void draw(rhi::Driver& driver) override;
    // InstancedGeometry 的 VAO 绑定入口（draw 的 bind 半段——
    // InstancedGeometry.ts draw :453-455 → repr.drawInstanced 的 bufs.bind()）。
    void bindPrimitive(rhi::Driver& driver) override
    {
        if (m_primitive)
            driver.bindRenderPrimitive(m_primitive);
    }
    uint32_t getDrawIndexCount() const override { return m_numIndices; }
    // LUT 形态（量化 imdl 网格）= drawArrays（无 element index buffer，
    // 24-bit 顶点表索引流即 a_qPosition attribute）；VBO 形态 = draw2。
    // Ported from: SurfaceGeometry.ts :150-162（_draw 的 drawArrays）。
    bool usesIndexBuffer() const override { return !m_usesQuantizedPositions; }
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    // LUT 形态为 true，VBO 形态为 false。
    // Ported from: itwinjs-core CachedGeometry.ts:98 + :207（LUTGeometry.
    // usesQuantizedPositions ← this.lut.usesQuantizedPositions）——覆盖
    // MeshGeometry 的 false 约定（PolyfaceGraphic/MeshRenderGeometry 非量化路径）。
    bool usesQuantizedPositions() const noexcept override { return m_usesQuantizedPositions; }

    // 绘制绑定入口（SceneCompositorImpl Surface 分支的 u_vertLUT/u_vertParams 源）。
    // Ported from: itwinjs-core MeshGeometry.ts:40（get lut()）+ glsl/Vertex.ts:229-246
    //（u_vertLUT/u_vertParams 的 GraphicUniform 绑定）。
    // NOTE: 按名隐藏 MeshGeometry::getLut()（非拥有 const* 观察位）——LUT 形态下
    // LUT 由本几何体按值持有（PolylineGeometry 同款）；基类版本当前无调用方。
    VertexLutTexture const& getLut() const noexcept { return m_lut; }
    // a_qPosition 24-bit 索引流句柄（VAO 绑定于创建点 Task 5）。
    rhi::BufferObjectHandle getLutIndexBuffer() const noexcept { return m_lutIndexBuffer; }

    // u_color（量化路径均匀色）：量化 Surface shader 无 a_color attribute，
    // v_color 取自 u_color uniform（Color.ts:51-60 ← lutGeom.getColor(target)）。
    // 对齐 PolylineGeometry::getColor（Polyline.ts:129-131）。
    dqCommon::ColorDef getColor() const noexcept { return m_color; }
    void setColor(dqCommon::ColorDef color) { m_color = color; }

    // 非均匀（色表）顶点色形态位——参考语义：VertexTable.uniformColor
    // undefined → ColorInfo.createFromVertexTable → createNonUniform
    //（ColorInfo.ts:35 ← VertexLUT.ts:97 createFromVertexTable）→ 每 draw
    // setShaderFlags 置 u_shaderFlags[kShaderBit_NonUniformColor]=1
    //（Common.ts:59-73）+ u_color 免绑（Color.ts:56 仅 isUniform 时 bind）。
    // 色表字节无需独立上传：appendColorTable 把色表追加在顶点表数据尾部
    //（VertexTableBuilder.ts:185-193——numVertices*numRgbaPerVertex 之后），
    // LUT 纹理直传时已在内，shader 按 colorTableStart 定位采样（Color.ts:18-23）。
    // 命名注（§3.4 碰撞避让）：几何上已有 getColor() 返回均匀 ColorDef
    //（PolylineGeometry 先例），故本位以 isNonUniformColor 表达参考
    // colorInfo.isNonUniform，不覆盖 getColor 的均匀值语义。
    void setNonUniformColor() noexcept { m_nonUniformColor = true; }
    bool isNonUniformColor() const noexcept { return m_nonUniformColor; }

    /// Check if this surface is lit.
    bool isLit() const noexcept { return getSurfaceType() != SurfaceType::Unknown; }

    /// Set the render primitive (VAO).
    void setPrimitive(rhi::RenderPrimitiveHandle primitive) { m_primitive = primitive; }

    /// Set the number of indices.
    void setNumIndices(uint32_t count) { m_numIndices = count; }

    /// Derive the 12 SurfaceBitIndex flags from geometry state.
    /// Ported from: itwinjs-core MeshGeometry.computeSurfaceFlags()
    /// Returns a pointer to a thread-local 12-int array (matches itwinjs's
    /// module-level surfaceFlagArray Int32Array pattern).
    /// Step 1 fills: HasTexture/ApplyLighting/HasNormals/IgnoreMaterial/
    /// TransparencyThreshold/HasColorAndNormal/HasMaterialAtlas.
    /// Step 2+ will add: BackgroundFill/OverrideRgb/HasNormalMap/ConstantLod*.
    /// Surface texture (s_texture). Ported from: itwinjs SurfaceGeometry.texture
    void setTexture(rhi::TextureHandle texture) { m_texture = texture; }
    rhi::TextureHandle getTexture() const noexcept { return m_texture; }
    rhi::TextureHandle getSurfaceTexture() const noexcept override { return m_texture; }

    /// Ported from: itwinjs-core SurfaceGeometry.computeSurfaceFlags(params, flags)
    /// — the target-derived gates (displayNormalMaps, currentViewFlags) are
    /// passed explicitly instead of via ShaderProgramParams.target (C++ tests
    /// cannot construct a real TargetImpl). Gates consumed per wantNormalMaps
    /// (SurfaceGeometry.ts :416-428): displayNormalMaps (Target.ts :158,
    /// default true) + renderMode == SmoothShade + viewFlags.textures.
    /// ViewFlags here is the pipeline alias (= dqCommon::ViewFlagsProperties,
    /// field access — ViewFlags.h :36).
    static int const* computeSurfaceFlags(CachedGeometry const& geom,
                                          bool displayNormalMaps,
                                          ViewFlags const& viewFlags);

private:
    rhi::Driver& m_driver;
    rhi::IndexBufferHandle m_ibh;
    rhi::RenderPrimitiveHandle m_primitive;
    rhi::TextureHandle m_texture;
    uint32_t m_numIndices = 0;
    // LUT 形态成员（VBO 形态下 m_lut 为空、m_lutIndexBuffer 为 nullid）：
    VertexLutTexture m_lut;                    // 顶点 LUT 纹理（按值持有，仅 LUT 形态非空）
    rhi::BufferObjectHandle m_lutIndexBuffer;  // a_qPosition 24-bit 索引流
    rhi::VertexBufferHandle m_lutVertexBuffer;       // a_qPosition VAO 顶点缓冲句柄
    rhi::VertexBufferInfoHandle m_lutVertexBufferInfo;  // attribute 布局（UBYTE3@0）
    bool m_usesQuantizedPositions = false;
    dqCommon::ColorDef m_color = dqCommon::ColorDef::create();  // u_color 均匀色（默认黑——对齐 ColorDef.create()）
    bool m_nonUniformColor = false;  // 色表形态位（ColorInfo.createNonUniform——见 isNonUniformColor 注）
};

// ---------------------------------------------------------------------------
// EdgeGeometry — concrete edge geometry
// (Ported from: itwinjs-core EdgeGeometry.ts)
//
// 两种形态（与 SurfaceGeometry 的 VBO/LUT 双形态同构）：
//   - VBO 形态：EdgeGeometry(numIndices)（既有，无消费者）；
//   - LUT 形态（U11(2)：量化 imdl 网格的 segment edges）——Ported from:
//     EdgeGeometry.ts create (:42-47) + ctor (:75-88)。a_pos = 24-bit 顶点表
//     索引流（UBYTE3，BO0）、a_endPointAndQuadIndices = 对端点索引+quad 角标
//     （UBYTE4，BO1）；drawArrays(Triangles, 0, numIndices)（:54-60 _draw，
//     无 element index buffer）；technique Edge；顶点 LUT 由 surface 几何持有、
//     本几何经 MeshGeometry::setLut 非拥有观察（参考侧两者共享 MeshData.lut——
//     glsl/Vertex.ts addPositionFromLUT 的 u_vertLUT 对 edge 变体同样生效）。
//     生命周期：MeshGraphic 成员声明序 m_surfaces 先于 m_edges → 析构反序
//     edges 先死，LUT（surface 持有）后死——观察指针无悬空窗口。
// ---------------------------------------------------------------------------
class EdgeGeometry : public MeshGeometry {
public:
    // VBO 形态（既有）。
    EdgeGeometry(uint32_t numIndices);
    // LUT 形态。`lut` 为 surface 几何持有的顶点 LUT（非拥有观察——参考侧
    // surface/segmentEdges/silhouetteEdges 共享同一 MeshData.lut）；
    // `edgeWeight`/`edgeLineCode` = EdgeParams.weight/linePixels
    // （MeshData.ts:93-94）；`color` = mesh 均匀色（u_color 源，
    // ColorInfo.createFromVertexTable 的 uniform 路径）。
    EdgeGeometry(rhi::Driver& driver, rhi::BufferObjectHandle indices,
                 rhi::BufferObjectHandle endPointAndQuadIndices, uint32_t numIndices,
                 VertexLutTexture const& lut, float edgeWeight, uint32_t edgeLineCode,
                 dqCommon::ColorDef color, bool isPlanar);
    ~EdgeGeometry() override;

    // --- Casting accessor (Ported from: itwinjs-core CachedGeometry.ts asEdge) ---
    EdgeGeometry* asEdge() override { return this; }

    // --- CachedGeometry interface ---
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::Edge; }
    Pass getPass() const noexcept override { return Pass::OpaqueLinear; }
    // Ported from: itwinjs-core EdgeGeometry.ts renderOrder (:66)——
    // isPlanar ? PlanarEdge : Edge。
    RenderOrder getRenderOrder() const noexcept override
    {
        return isPlanar() ? RenderOrder::PlanarEdge : RenderOrder::Edge;
    }
    // LUT 形态为量化几何（与 surface 共享 Quantized 边缘变体）；VBO 形态沿用
    // MeshGeometry 的 false。
    bool usesQuantizedPositions() const noexcept override { return m_lutForm; }
    // LUT 形态 = drawArrays（EdgeGeometry.ts:54-60 _draw 无 element index
    // buffer）；VBO 形态 = draw2。InstancedGeometry::draw 的 flavor 选择。
    bool usesIndexBuffer() const override { return !m_lutForm; }
    void draw(rhi::Driver& driver) override;
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    // u_color 源（mesh 均匀色——参考 lutGeom.getColor(target) 的 uniform 路径；
    // EdgeSettings 的颜色覆盖在 dispatch 侧合并，EdgeGeometry.ts:68
    // computeEdgeColor）。
    dqCommon::ColorDef getColor() const noexcept { return m_color; }

    void setPrimitive(rhi::RenderPrimitiveHandle primitive) { m_primitive = primitive; }
    void setNumIndices(uint32_t count) { m_numIndices = count; }
    // a_pos/a_endPointAndQuadIndices VAO 的 RHI 顶点缓冲（析构清单成员——
    // 创建点组装后注入，PolylineGeometry cornerVbh/vbih 同款）。
    void setVbhResources(rhi::VertexBufferHandle vbh, rhi::VertexBufferInfoHandle vbih)
    {
        m_vbh = vbh;
        m_vbih = vbih;
    }

protected:
    // LUT 形态共享资源（driver + BOs + VAO）。派生类 SilhouetteEdgeGeometry
    // 先析构自己的 normalPairs，再由本基类析构共享资源（成员析构反序）。
    rhi::Driver* m_driver = nullptr;  // VBO 形态为 nullptr（既有语义不变）
    rhi::RenderPrimitiveHandle m_primitive;
    rhi::VertexBufferHandle m_vbh;
    rhi::VertexBufferInfoHandle m_vbih;
    rhi::BufferObjectHandle m_indices;
    rhi::BufferObjectHandle m_endPointAndQuadIndices;
    bool m_lutForm = false;
    uint32_t m_numIndices = 0;
    dqCommon::ColorDef m_color = dqCommon::ColorDef::create();
};

// ---------------------------------------------------------------------------
// SilhouetteEdgeGeometry — silhouette edge geometry
// (Ported from: itwinjs-core EdgeGeometry.ts SilhouetteEdgeGeometry :92-133)
//
// segment edge 的第三条流：a_normals = 每顶点 2×16-bit oct-encoded normal 对
// （UBYTE4，BO2——参考 :129-131 addBuffer(normalPairs, [location 2])）；
// technique SilhouetteEdge、order Silhouette/PlanarSilhouette（:110-112）；
// 视锥相关的可见性判定在 shader（Edge.ts checkForSilhouetteDiscard）。
// ---------------------------------------------------------------------------
class SilhouetteEdgeGeometry : public EdgeGeometry {
public:
    SilhouetteEdgeGeometry(rhi::Driver& driver, rhi::BufferObjectHandle indices,
                           rhi::BufferObjectHandle endPointAndQuadIndices,
                           rhi::BufferObjectHandle normalPairs, uint32_t numIndices,
                           VertexLutTexture const& lut, float edgeWeight,
                           uint32_t edgeLineCode, dqCommon::ColorDef color, bool isPlanar);
    ~SilhouetteEdgeGeometry() override;

    SilhouetteEdgeGeometry* asSilhouette() override { return this; }

    // Ported from: itwinjs-core EdgeGeometry.ts :110-112。
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::SilhouetteEdge; }
    RenderOrder getRenderOrder() const noexcept override
    {
        return isPlanar() ? RenderOrder::PlanarSilhouette : RenderOrder::Silhouette;
    }

private:
    rhi::BufferObjectHandle m_normalPairs;
};

// ---------------------------------------------------------------------------
// PolylineGeometry — concrete polyline geometry
// (Ported from: itwinjs-core webgl/Polyline.ts PolylineGeometry)
//
// Faithful thick-line geometry. Owns:
//   - a VertexLutTexture (the per-vertex LUT — positions/transposed SoA layout
//     baked by VertexTableBuilder::buildFromPolylines);
//   - a corner RenderPrimitiveHandle (TRIANGLES, NO element index buffer) built
//     by MeshGraphic from PolylineTesselator's 3 corner arrays (PolylineBuffers,
//     CachedGeometry.ts:1141-1146);
//   - numCorners (triangle-list vertex count — the itwinjs `numIndices` field,
//     Polyline.ts:39);
//   - the line symbology (lineWidth + uniform color).
//
// Draw is glDrawArrays(GL_TRIANGLES, 0, numCorners) (Polyline.ts:133-140 —
// `gl.drawArrays(GL.PrimitiveType.Triangles, 0, this.numIndices)`). The technique
// is TechniqueId::Polyline (Polyline.ts:114) and the VAO must match
// PolylineVariantCompiler's attribute map (a_pos/a_prevIndex/a_nextIndex/a_param
// at locations 0/1/2/3) — flipped in the SAME commit as the VAO change because
// the Surface shader's a_position (float3) is incompatible with the corner
// buffer's a_pos (24-bit LUT index).
// ---------------------------------------------------------------------------
class PolylineGeometry : public CachedGeometry {
public:
    // `driver` owns the GL resources (all destroyed in dtor). Faithful 1:1 with
    // itwinjs Polyline.ts PolylineGeometry constructor + [Symbol.dispose]:
    //   - `lut`                   = VertexLUT (LUT texture + params);
    //   - `primitive`             = PolylineBuffers.buffers (the VAO);
    //   - `cornerVbh`             = VertexBufferHandle that binds the 3 BOs;
    //   - `cornerVbih`            = the 4-attribute VertexBufferInfoHandle;
    //   - `posVbo/prevVbo/nextPropsVbo` = PolylineBuffers.indices/prevIndices/
    //                                     nextIndicesAndParams (CachedGeometry.ts:1148-1150);
    //   - `numCorners`            = numIndices (triangle-list vertex count);
    //   - `lineWidth`             = lineWeight (Polyline.ts:50);
    //   - `color`                 = uniform line color (DisplayParams.lineColor).
    // NullDriver/Mock-based tests pass nullid handles for the GL resources they
    // don't exercise; destroy({}) is a no-op so the dtor is uniform.
    PolylineGeometry(rhi::Driver& driver, VertexLutTexture lut,
                     rhi::RenderPrimitiveHandle primitive,
                     rhi::VertexBufferHandle cornerVbh,
                     rhi::VertexBufferInfoHandle cornerVbih,
                     rhi::BufferObjectHandle posVbo,
                     rhi::BufferObjectHandle prevVbo,
                     rhi::BufferObjectHandle nextPropsVbo,
                     uint32_t numCorners, float lineWidth, dqCommon::ColorDef color);
    ~PolylineGeometry() override;

    PolylineGeometry(PolylineGeometry const&) = delete;
    PolylineGeometry& operator=(PolylineGeometry const&) = delete;

    // --- CachedGeometry interface ---
    // VertexTableBuilder bakes unquantized float positions into the LUT (no
    // QParams3d) → the Polyline technique's Unquantized variant is required.
    bool usesQuantizedPositions() const noexcept override { return false; }
    // Ported from: itwinjs-core Polyline.ts:114 (techniqueId === TechniqueId.Polyline).
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::Polyline; }
    Pass getPass() const noexcept override { return Pass::OpaqueLinear; }
    RenderOrder getRenderOrder() const noexcept override { return RenderOrder::Linear; }
    void draw(rhi::Driver& driver) override;
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    // Per-draw uniform sources (consumed by SceneCompositorImpl Polyline branch).
    // Ported from: itwinjs-core Polyline.ts:123-125 _getLineWeight → this.lineWeight;
    // clamp 1..31 per CachedGeometry.ts:140-150 (prevents 0/inf weight breaking
    // the miter/displacement math). Mirrors PointStringGeometry::getLineWeight
    // (SurfaceGeometry.h:195).
    float getLineWeight() const noexcept override {
        return std::clamp(m_lineWidth, 1.0f, 31.0f);
    }
    // Ported from: itwinjs-core Polyline.ts:129-131 getColor → lut.colorInfo.
    // For the uniform-color path the LUT carries no colorInfo, so the geometry
    // holds the single DisplayParams.lineColor for the dispatch's u_color upload.
    dqCommon::ColorDef getColor() const noexcept { return m_color; }
    // LUT access (u_vertLUT/u_vertParams source for the dispatch).
    VertexLutTexture const& getLut() const noexcept { return m_lut; }

private:
    rhi::Driver& m_driver;
    VertexLutTexture m_lut;
    rhi::RenderPrimitiveHandle m_primitive;
    // Corner buffer GL resources (PolylineBuffers). Owned here because RHI
    // primitives only own the VAO, not the underlying VBOs/VBIH (matches itwinjs
    // PolylineBuffers.[Symbol.dispose] disposing indices/prevIndices/nextIndicesAndParams).
    rhi::VertexBufferHandle m_cornerVbh;
    rhi::VertexBufferInfoHandle m_cornerVbih;
    rhi::BufferObjectHandle m_posVbo;
    rhi::BufferObjectHandle m_prevVbo;
    rhi::BufferObjectHandle m_nextPropsVbo;
    uint32_t m_numCorners = 0;
    float m_lineWidth = 1.0f;
    dqCommon::ColorDef m_color;
};

// ---------------------------------------------------------------------------
// PointCloudGeometry — concrete point cloud geometry
// (Ported from: itwinjs-core PointCloud.ts)
// ---------------------------------------------------------------------------
class PointCloudGeometry : public CachedGeometry {
public:
    PointCloudGeometry(uint32_t vertexCount, float voxelSize);
    ~PointCloudGeometry() override;

    // --- CachedGeometry interface ---
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::PointCloud; }
    Pass getPass() const noexcept override { return Pass::PointClouds; }
    RenderOrder getRenderOrder() const noexcept override { return RenderOrder::Linear; }
    void draw(rhi::Driver& driver) override;
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    uint32_t getVertexCount() const noexcept { return m_vertexCount; }
    float getVoxelSize() const noexcept { return m_voxelSize; }

    void setPrimitive(rhi::RenderPrimitiveHandle primitive) { m_primitive = primitive; }

private:
    rhi::RenderPrimitiveHandle m_primitive;
    uint32_t m_vertexCount = 0;
    float m_voxelSize = 0.0f;
};

// ---------------------------------------------------------------------------
// PointStringGeometry — concrete point string geometry
// (Ported from: itwinjs-core PointString.ts)
// ---------------------------------------------------------------------------
class PointStringGeometry : public CachedGeometry {
public:
    // driver + ibh own this geometry's per-geometry GL resources (ibh is released
    // in the dtor; the render primitive is set via setPrimitive after construction).
    PointStringGeometry(rhi::Driver& driver, rhi::IndexBufferHandle ibh,
                        uint32_t vertexCount, float weight);
    ~PointStringGeometry() override;

    // --- CachedGeometry interface ---
    // MeshRenderGeometry::create uploads raw FLOAT3 positions (no VertexLUT) → must
    // select the Unquantized variant or the triad's point string renders zero-coverage.
    bool usesQuantizedPositions() const noexcept override { return false; }
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::PointString; }
    Pass getPass() const noexcept override { return Pass::OpaqueLinear; }
    RenderOrder getRenderOrder() const noexcept override { return RenderOrder::Linear; }
    void draw(rhi::Driver& driver) override;
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    float getWeight() const noexcept { return m_weight; }

    // Ported from: itwinjs-core PointString.ts:62 (_getLineWeight → this.weight).
    // Drives gl_PointSize in the PointString shader (PolylineShaderBuilder.cpp:367).
    float getLineWeight() const noexcept override { return m_weight; }

    void setPrimitive(rhi::RenderPrimitiveHandle primitive) { m_primitive = primitive; }

private:
    rhi::Driver& m_driver;
    rhi::IndexBufferHandle m_ibh;
    rhi::RenderPrimitiveHandle m_primitive;
    uint32_t m_vertexCount = 0;
    float m_weight = 1.0f;
};

END_DQ_RENDER_NAMESPACE

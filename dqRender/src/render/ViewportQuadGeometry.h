// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Viewport quad geometry hierarchy
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/CachedGeometry.ts
//
// Full-screen quad geometry used for post-processing effects (SSAO, blur,
// EDL, compositing, skybox, etc.).  Renders a single quad that covers the viewport.
//
// Contains: ViewportQuadGeometry, TexturedViewportQuadGeometry,
//           SkySphereViewportQuadGeometry, CompositeGeometry,
//           AmbientOcclusionGeometry, BlurGeometry, EDL geometries,
//           EVSMGeometry, CopyPickBufferGeometry, CombineTexturesGeometry,
//           SingleTexturedViewportQuadGeometry, VolumeClassifierGeometry,
//           ScreenPointsGeometry, SkyBoxQuadsGeometry
#pragma once

#include "CachedGeometry.h"
#include "ColorInfo.h"
#include <dqRender/RenderSkyBoxParams.h>
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <dqCommon/Frustum.h>

#include <array>
#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Forward declarations
class PolylineBuffers;
class TesselatedPolyline;

// ---------------------------------------------------------------------------
// ViewportQuadGeometry — full-screen quad for post-processing
// (Ported from: itwinjs-core CachedGeometry.ts line 503-522)
// ---------------------------------------------------------------------------
class ViewportQuadGeometry : public IndexedGeometry {
public:
    ViewportQuadGeometry(IndexedGeometryParams params, TechniqueId techniqueId);
    ~ViewportQuadGeometry() override = default;

    ViewportQuadGeometry(ViewportQuadGeometry const&) = delete;
    ViewportQuadGeometry& operator=(ViewportQuadGeometry const&) = delete;

    // --- CachedGeometry interface ---
    TechniqueId getTechniqueId() const noexcept override { return m_techniqueId; }
    Pass getPass() const noexcept override { return Pass::Opaque; }
    RenderOrder getRenderOrder() const noexcept override { return RenderOrder::UnlitSurface; }

    void collectStatistics(RenderMemory::Statistics& /*stats*/) const override {
        // NB: These don't really count...
    }

    // --- Factory ---
    // Creates a viewport quad with the given technique.
    // Ported from: itwinjs-core CachedGeometry.ts line 510-513
    static ViewportQuadGeometry* create(TechniqueId techniqueId);

protected:
    TechniqueId m_techniqueId;
};

// ---------------------------------------------------------------------------
// TexturedViewportQuadGeometry — viewport quad with texture sampling
// (Ported from: itwinjs-core CachedGeometry.ts line 527-546)
// ---------------------------------------------------------------------------
class TexturedViewportQuadGeometry : public ViewportQuadGeometry {
public:
    TexturedViewportQuadGeometry(IndexedGeometryParams params,
                                 TechniqueId techniqueId,
                                 std::vector<rhi::TextureHandle> textures);
    ~TexturedViewportQuadGeometry() override = default;

    // --- Texture access ---
    rhi::TextureHandle getTexture(size_t index) const {
        return index < m_textures.size() ? m_textures[index] : rhi::TextureHandle{};
    }
    size_t getTextureCount() const noexcept { return m_textures.size(); }

    // --- Factory ---
    static TexturedViewportQuadGeometry* create(TechniqueId techniqueId,
                                                 std::vector<rhi::TextureHandle> textures);

protected:
    std::vector<rhi::TextureHandle> m_textures;
};

// ---------------------------------------------------------------------------
// ComputeSkySphereWorldPosAndEye — port of itwinjs-core's per-frame sky
// worldPos + worldEye derivation (non-globe path; blank-connection view).
//   worldPos[12] = the 4 mid-depth WORLD corners of the frustum
//                  (LeftBottom, RightBottom, RightTop, LeftTop), each the
//                  midpoint of its Rear and Front corners.
//                  Ported from: itwinjs-core CachedGeometry.ts:583-596.
//   worldEye[3]  = the orthographic pseudo-camera position (behind the rear
//                  plane, 22.5° half-angle focal length), used by the gradient
//                  shader as the eye for eyeToVert = a_worldPos - u_worldEye.
//                  Ported from: itwinjs-core SkySphere.ts:251-264.
// The frustum `f` must be the camera-consistent WORLD frustum (8 corners in
// Npc order). Pure math (no GL) — unit-testable.
// ---------------------------------------------------------------------------
void ComputeSkySphereWorldPosAndEye(dqCommon::Frustum const& f,
                                    float worldPos[12], float worldEye[3]) noexcept;

// ---------------------------------------------------------------------------
// ComputeSkySphereWorldPosAndEye 的 globe-mode 重载：isGlobeMode3D 时走
// CachedGeometry.ts:597-651 的 globe 分支（固定 camDir=(0,1,0) 的假视锥 +
// global-up 投影定向的 4 个天空角点）；否则退化为非 globe 路径（同上）。
// SkySphereGlobeParams 定义见公开头 RenderSkyBoxParams.h（与计划状态同源）。
// ---------------------------------------------------------------------------
void ComputeSkySphereWorldPosAndEye(dqCommon::Frustum const& f,
                                    float worldPos[12], float worldEye[3],
                                    SkySphereGlobeParams const& globe) noexcept;

// ---------------------------------------------------------------------------
// SkySphereViewportQuadGeometry — gradient/texture spherical skybox
// (Ported from: itwinjs-core CachedGeometry.ts line 551-735)
// ---------------------------------------------------------------------------
class SkySphereViewportQuadGeometry : public ViewportQuadGeometry {
public:
    struct Colors {
        std::array<float, 3> zenith = {0.0f, 0.0f, 0.0f};
        std::array<float, 3> sky = {0.0f, 0.0f, 0.0f};
        std::array<float, 3> ground = {0.0f, 0.0f, 0.0f};
        std::array<float, 3> nadir = {0.0f, 0.0f, 0.0f};
    };

    SkySphereViewportQuadGeometry(IndexedGeometryParams params,
                                   RenderSkySphereParams const& skybox);
    SkySphereViewportQuadGeometry(IndexedGeometryParams params,
                                   RenderSkyGradientParams const& skybox);
    ~SkySphereViewportQuadGeometry() override = default;

    // Sky renders in the SkyBox pass (drawn behind all scene geometry).
    // ← itwinjs-core: skyBox graphics are classified into RenderPass.SkyBox.
    Pass getPass() const noexcept override { return Pass::SkyBox; }

    // Type guard so OpenGLRenderSystem::updateSkySphere can downcast safely.
    SkySphereViewportQuadGeometry* asSkySphere() override { return this; }

    // Lazily upload the full-screen quad on first draw. The viewport-quad
    // hierarchy otherwise leaves the GPU primitive null, so the inherited
    // IndexedGeometry::draw early-returns and nothing renders.
    void draw(rhi::Driver& driver) override;

    // --- Accessors ---
    std::array<float, 3> const& getTypeAndExponents() const noexcept { return m_typeAndExponents; }
    float getZOffset() const noexcept { return m_zOffset; }
    float getRotation() const noexcept { return m_rotation; }
    Colors const& getColors() const noexcept { return m_colors; }
    rhi::TextureHandle getSkyTexture() const noexcept { return m_skyTexture; }
    float const* getWorldPos() const noexcept { return m_worldPos.data(); }

    /// world-space eye for the gradient (computeGradientValue: eyeToVert = a_worldPos - u_worldEye).
    /// Set per-frame from ComputeSkySphereWorldPosAndEye.
    float const* getWorldEye() const noexcept { return m_worldEye.data(); }
    /// Per-frame world corners (a_worldPos attribute) + world eye, from the camera frustum.
    /// Ported from: itwinjs-core SkySphereViewportQuadGeometry (worldPos updated each draw).
    void setWorldPosAndEye(float const worldPos[12], float const worldEye[3]) noexcept;

    // --- Factory ---
    static SkySphereViewportQuadGeometry* createGeometry(
        RenderSkySphereParams const& skybox);
    static SkySphereViewportQuadGeometry* createGeometry(
        RenderSkyGradientParams const& skybox);

private:
    std::array<float, 3> m_typeAndExponents = {0.0f, 1.0f, 1.0f};
    float m_zOffset = 0.0f;
    float m_rotation = 0.0f;
    Colors m_colors;
    rhi::TextureHandle m_skyTexture;
    std::array<float, 12> m_worldPos = {};  // 4 corners × 3 floats (a_worldPos)
    std::array<float, 3> m_worldEye = {};   // u_worldEye (gradient eye)

    // Lazy GPU upload state for the full-screen quad.
    bool m_skyUploaded = false;
    rhi::VertexBufferInfoHandle m_skyVbih;
    rhi::VertexBufferHandle m_skyVbh;
    rhi::BufferObjectHandle m_skyVbo;       // buffer 0: a_position (NDC ±1, static)
    rhi::BufferObjectHandle m_skyWorldVbo;  // buffer 1: a_worldPos (world corners, per-frame)
    rhi::IndexBufferHandle m_skyIbh;
    rhi::RenderPrimitiveHandle m_skyPrimitive;
};

// ---------------------------------------------------------------------------
// AmbientOcclusionGeometry — SSAO post-process
// (Ported from: itwinjs-core CachedGeometry.ts line 740-758)
// ---------------------------------------------------------------------------
class AmbientOcclusionGeometry : public TexturedViewportQuadGeometry {
public:
    AmbientOcclusionGeometry(IndexedGeometryParams params,
                              rhi::TextureHandle depthAndOrder,
                              rhi::TextureHandle depth);
    ~AmbientOcclusionGeometry() override = default;

    rhi::TextureHandle getDepthAndOrder() const { return getTexture(1); }
    rhi::TextureHandle getDepth() const { return getTexture(0); }

    static AmbientOcclusionGeometry* createGeometry(
        rhi::TextureHandle depthAndOrder, rhi::TextureHandle depth);
};

// ---------------------------------------------------------------------------
// BlurGeometry — blur post-process
// (Ported from: itwinjs-core CachedGeometry.ts line 767-789)
// ---------------------------------------------------------------------------
class BlurGeometry : public TexturedViewportQuadGeometry {
public:
    BlurGeometry(IndexedGeometryParams params,
                 std::vector<rhi::TextureHandle> textures,
                 float blurDirX, float blurDirY,
                 BlurType blurType);
    ~BlurGeometry() override = default;

    float getBlurDirX() const noexcept { return m_blurDirX; }
    float getBlurDirY() const noexcept { return m_blurDirY; }

    rhi::TextureHandle getTextureToBlur() const { return getTexture(0); }
    rhi::TextureHandle getDepthAndOrder() const { return getTexture(1); }
    rhi::TextureHandle getDepthAndOrderHidden() const {
        return getTextureCount() > 2 ? getTexture(2) : rhi::TextureHandle{};
    }

    static BlurGeometry* createGeometry(
        rhi::TextureHandle texToBlur,
        rhi::TextureHandle depthAndOrder,
        rhi::TextureHandle depthAndOrderHidden,
        float blurDirX, float blurDirY,
        BlurType blurType);

private:
    float m_blurDirX = 0.0f;
    float m_blurDirY = 0.0f;
};

// ---------------------------------------------------------------------------
// EDLCalcBasicGeometry — EDL basic calculation pass
// (Ported from: itwinjs-core CachedGeometry.ts line 792-812)
// ---------------------------------------------------------------------------
class EDLCalcBasicGeometry : public TexturedViewportQuadGeometry {
public:
    EDLCalcBasicGeometry(IndexedGeometryParams params,
                          rhi::TextureHandle colorBuffer,
                          rhi::TextureHandle depthBuffer,
                          float width, float height);
    ~EDLCalcBasicGeometry() override = default;

    rhi::TextureHandle getColorTexture() const { return getTexture(0); }
    rhi::TextureHandle getDepthTexture() const { return getTexture(1); }
    std::array<float, 3> const& getTexInfo() const noexcept { return m_texInfo; }

    static EDLCalcBasicGeometry* createGeometry(
        rhi::TextureHandle colorBuffer, rhi::TextureHandle depthBuffer,
        float width, float height);

private:
    std::array<float, 3> m_texInfo = {};
};

// ---------------------------------------------------------------------------
// EDLCalcFullGeometry — EDL full calculation pass
// (Ported from: itwinjs-core CachedGeometry.ts line 815-835)
// ---------------------------------------------------------------------------
class EDLCalcFullGeometry : public TexturedViewportQuadGeometry {
public:
    EDLCalcFullGeometry(IndexedGeometryParams params,
                         rhi::TextureHandle colorBuffer,
                         rhi::TextureHandle depthBuffer,
                         float scale, float width, float height);
    ~EDLCalcFullGeometry() override = default;

    rhi::TextureHandle getColorTexture() const { return getTexture(0); }
    rhi::TextureHandle getDepthTexture() const { return getTexture(1); }
    std::array<float, 3> const& getTexInfo() const noexcept { return m_texInfo; }

    static EDLCalcFullGeometry* createGeometry(
        rhi::TextureHandle colorBuffer, rhi::TextureHandle depthBuffer,
        float scale, float width, float height);

private:
    std::array<float, 3> m_texInfo = {};
};

// ---------------------------------------------------------------------------
// EDLFilterGeometry — EDL filter pass
// (Ported from: itwinjs-core CachedGeometry.ts line 838-858)
// ---------------------------------------------------------------------------
class EDLFilterGeometry : public TexturedViewportQuadGeometry {
public:
    EDLFilterGeometry(IndexedGeometryParams params,
                       rhi::TextureHandle colorBuffer,
                       rhi::TextureHandle depthBuffer,
                       float scale, float width, float height);
    ~EDLFilterGeometry() override = default;

    rhi::TextureHandle getColorTexture() const { return getTexture(0); }
    rhi::TextureHandle getDepthTexture() const { return getTexture(1); }
    std::array<float, 3> const& getTexInfo() const noexcept { return m_texInfo; }

    static EDLFilterGeometry* createGeometry(
        rhi::TextureHandle colorBuffer, rhi::TextureHandle depthBuffer,
        float scale, float width, float height);

private:
    std::array<float, 3> m_texInfo = {};
};

// ---------------------------------------------------------------------------
// EDLMixGeometry — EDL mix pass
// (Ported from: itwinjs-core CachedGeometry.ts line 861-878)
// ---------------------------------------------------------------------------
class EDLMixGeometry : public TexturedViewportQuadGeometry {
public:
    EDLMixGeometry(IndexedGeometryParams params,
                    rhi::TextureHandle color1,
                    rhi::TextureHandle color2,
                    rhi::TextureHandle color4);
    ~EDLMixGeometry() override = default;

    rhi::TextureHandle getColorTexture1() const { return getTexture(0); }
    rhi::TextureHandle getColorTexture2() const { return getTexture(1); }
    rhi::TextureHandle getColorTexture4() const { return getTexture(2); }

    static EDLMixGeometry* createGeometry(
        rhi::TextureHandle color1,
        rhi::TextureHandle color2,
        rhi::TextureHandle color4);
};

// ---------------------------------------------------------------------------
// EVSMGeometry — EVSM shadow map generation
// (Ported from: itwinjs-core CachedGeometry.ts line 881-899)
// ---------------------------------------------------------------------------
class EVSMGeometry : public TexturedViewportQuadGeometry {
public:
    EVSMGeometry(IndexedGeometryParams params,
                  rhi::TextureHandle depthBuffer,
                  float width, float height);
    ~EVSMGeometry() override = default;

    rhi::TextureHandle getDepthTexture() const { return getTexture(0); }
    std::array<float, 2> const& getStepSize() const noexcept { return m_stepSize; }

    static EVSMGeometry* createGeometry(
        rhi::TextureHandle depthBuffer, float width, float height);

private:
    std::array<float, 2> m_stepSize = {};
};

// ---------------------------------------------------------------------------
// CompositeGeometry — composite pass (transparency + hilite + occlusion)
// (Ported from: itwinjs-core CachedGeometry.ts line 904-940)
// ---------------------------------------------------------------------------
class CompositeGeometry : public TexturedViewportQuadGeometry {
public:
    CompositeGeometry(IndexedGeometryParams params,
                       std::vector<rhi::TextureHandle> textures);
    ~CompositeGeometry() override = default;

    rhi::TextureHandle getOpaque() const { return getTexture(0); }
    rhi::TextureHandle getAccum() const { return getTexture(1); }
    rhi::TextureHandle getReveal() const { return getTexture(2); }
    rhi::TextureHandle getHilite() const { return getTexture(3); }
    rhi::TextureHandle getOcclusion() const {
        return getTextureCount() > 4 ? getTexture(4) : rhi::TextureHandle{};
    }

    // Invoked each frame to determine the appropriate Technique to use.
    void update(CompositeFlags flags);

    static CompositeGeometry* createGeometry(
        rhi::TextureHandle opaque,
        rhi::TextureHandle accum,
        rhi::TextureHandle reveal,
        rhi::TextureHandle hilite);
};

// ---------------------------------------------------------------------------
// CopyPickBufferGeometry — ping-pong pick buffer between opaque passes
// (Ported from: itwinjs-core CachedGeometry.ts line 945-961)
// ---------------------------------------------------------------------------
class CopyPickBufferGeometry : public TexturedViewportQuadGeometry {
public:
    CopyPickBufferGeometry(IndexedGeometryParams params,
                            rhi::TextureHandle featureId,
                            rhi::TextureHandle depthAndOrder);
    ~CopyPickBufferGeometry() override = default;

    rhi::TextureHandle getFeatureId() const { return getTexture(0); }
    rhi::TextureHandle getDepthAndOrder() const { return getTexture(1); }

    static CopyPickBufferGeometry* createGeometry(
        rhi::TextureHandle featureId,
        rhi::TextureHandle depthAndOrder);
};

// ---------------------------------------------------------------------------
// CombineTexturesGeometry — combine two textures
// (Ported from: itwinjs-core CachedGeometry.ts line 962-978)
// ---------------------------------------------------------------------------
class CombineTexturesGeometry : public TexturedViewportQuadGeometry {
public:
    CombineTexturesGeometry(IndexedGeometryParams params,
                             rhi::TextureHandle texture0,
                             rhi::TextureHandle texture1);
    ~CombineTexturesGeometry() override = default;

    rhi::TextureHandle getTexture0() const { return getTexture(0); }
    rhi::TextureHandle getTexture1() const { return getTexture(1); }

    static CombineTexturesGeometry* createGeometry(
        rhi::TextureHandle texture0,
        rhi::TextureHandle texture1);
};

// ---------------------------------------------------------------------------
// Combine3TexturesGeometry — combine three textures
// (Ported from: itwinjs-core CachedGeometry.ts line 980-997)
// ---------------------------------------------------------------------------
class Combine3TexturesGeometry : public TexturedViewportQuadGeometry {
public:
    Combine3TexturesGeometry(IndexedGeometryParams params,
                              rhi::TextureHandle texture0,
                              rhi::TextureHandle texture1,
                              rhi::TextureHandle texture2);
    ~Combine3TexturesGeometry() override = default;

    rhi::TextureHandle getTexture0() const { return getTexture(0); }
    rhi::TextureHandle getTexture1() const { return getTexture(1); }
    rhi::TextureHandle getTexture2() const { return getTexture(2); }

    static Combine3TexturesGeometry* createGeometry(
        rhi::TextureHandle texture0,
        rhi::TextureHandle texture1,
        rhi::TextureHandle texture2);
};

// ---------------------------------------------------------------------------
// SingleTexturedViewportQuadGeometry — viewport quad with one texture
// (Ported from: itwinjs-core CachedGeometry.ts line 1000-1016)
// ---------------------------------------------------------------------------
class SingleTexturedViewportQuadGeometry : public TexturedViewportQuadGeometry {
public:
    SingleTexturedViewportQuadGeometry(IndexedGeometryParams params,
                                        rhi::TextureHandle texture,
                                        TechniqueId techniqueId);
    ~SingleTexturedViewportQuadGeometry() override = default;

    rhi::TextureHandle getTexture() const { return TexturedViewportQuadGeometry::getTexture(0); }
    void setTexture(rhi::TextureHandle texture) { m_textures[0] = texture; }

    static SingleTexturedViewportQuadGeometry* createGeometry(
        rhi::TextureHandle texture, TechniqueId techniqueId);
};

// ---------------------------------------------------------------------------
// VolumeClassifierGeometry — volume classification
// (Ported from: itwinjs-core CachedGeometry.ts line 1026-1039)
// ---------------------------------------------------------------------------
class VolumeClassifierGeometry : public SingleTexturedViewportQuadGeometry {
public:
    VolumeClassifierGeometry(IndexedGeometryParams params,
                              rhi::TextureHandle texture);
    ~VolumeClassifierGeometry() override = default;

    BoundaryType boundaryType = BoundaryType::Inside;

    static VolumeClassifierGeometry* createVCGeometry(rhi::TextureHandle texture);
};

// ---------------------------------------------------------------------------
// ScreenPointsGeometry — screen-space points (for volume classification copy Z)
// (Ported from: itwinjs-core CachedGeometry.ts line 1044-1120)
// ---------------------------------------------------------------------------
class ScreenPointsGeometry : public CachedGeometry {
public:
    ScreenPointsGeometry(std::vector<float> positions,
                          uint32_t numPoints,
                          rhi::TextureHandle zTexture);
    ~ScreenPointsGeometry() override = default;

    // --- CachedGeometry interface ---
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::VolClassCopyZ; }
    Pass getPass() const noexcept override { return Pass::Classification; }
    RenderOrder getRenderOrder() const noexcept override { return RenderOrder::None; }
    void draw(rhi::Driver& driver) override;
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    float const* getQOrigin() const override { return m_origin.data(); }
    float const* getQScale() const override { return m_scale.data(); }

    rhi::TextureHandle getZTexture() const noexcept { return m_zTexture; }

    // --- Factory ---
    static ScreenPointsGeometry* createGeometry(
        uint32_t width, uint32_t height, rhi::TextureHandle depth);

protected:
    bool wantWoWReversal(TargetImpl const&) const override { return false; }

private:
    std::vector<float> m_positions;
    uint32_t m_numPoints = 0;
    rhi::TextureHandle m_zTexture;
    std::array<float, 3> m_origin = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> m_scale = {1.0f, 1.0f, 1.0f};
    rhi::RenderPrimitiveHandle m_primitive;
};

// ---------------------------------------------------------------------------
// SkyBoxQuadsGeometry — cube-mapped skybox
// (Ported from: itwinjs-core CachedGeometry.ts line 410-451)
// ---------------------------------------------------------------------------
class SkyBoxQuadsGeometry : public CachedGeometry {
public:
    SkyBoxQuadsGeometry(IndexedGeometryParams params,
                         rhi::TextureHandle cubeTexture);
    ~SkyBoxQuadsGeometry() override = default;

    // --- CachedGeometry interface ---
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::SkyBox; }
    Pass getPass() const noexcept override { return Pass::SkyBox; }
    RenderOrder getRenderOrder() const noexcept override { return RenderOrder::UnlitSurface; }
    void draw(rhi::Driver& driver) override;
    void collectStatistics(RenderMemory::Statistics& /*stats*/) const override {}

    float const* getQOrigin() const override { return m_qOrigin.data(); }
    float const* getQScale() const override { return m_qScale.data(); }

    rhi::TextureHandle getCubeTexture() const noexcept { return m_cubeTexture; }

    // --- Factory ---
    static SkyBoxQuadsGeometry* create(rhi::TextureHandle cubeTexture);

protected:
    bool wantWoWReversal(TargetImpl const&) const override { return false; }

private:
    IndexedGeometryParams m_params;
    rhi::TextureHandle m_cubeTexture;
    std::array<float, 3> m_qOrigin = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> m_qScale = {1.0f, 1.0f, 1.0f};
};

// ---------------------------------------------------------------------------
// PolylineBuffers — GPU buffers for polyline rendering
// (Ported from: itwinjs-core CachedGeometry.ts line 1123-1177)
//
// Holds three buffers for polyline rendering:
//   - indices: vertex positions (uint8 triples)
//   - prevIndices: previous vertex indices
//   - nextIndicesAndParams: next vertex indices + parameter byte
// ---------------------------------------------------------------------------
class PolylineBuffers {
public:
    PolylineBuffers(rhi::RenderPrimitiveHandle primitive,
                     rhi::BufferObjectHandle indices,
                     rhi::BufferObjectHandle prevIndices,
                     rhi::BufferObjectHandle nextIndicesAndParams,
                     uint32_t numIndices);
    ~PolylineBuffers() = default;

    PolylineBuffers(PolylineBuffers const&) = delete;
    PolylineBuffers& operator=(PolylineBuffers const&) = delete;

    rhi::RenderPrimitiveHandle getPrimitive() const noexcept { return m_primitive; }
    rhi::BufferObjectHandle getIndices() const noexcept { return m_indices; }
    rhi::BufferObjectHandle getPrevIndices() const noexcept { return m_prevIndices; }
    rhi::BufferObjectHandle getNextIndicesAndParams() const noexcept { return m_nextIndicesAndParams; }
    uint32_t getNumIndices() const noexcept { return m_numIndices; }

    void collectStatistics(RenderMemory::Statistics& stats) const;

    bool isValid() const noexcept { return m_primitive != rhi::RenderPrimitiveHandle{}; }

    // --- Factory ---
    // Ported from: itwinjs-core CachedGeometry.ts line 1153-1157
    // Creates polyline buffers from tesselated polyline data.
    static PolylineBuffers* create(
        rhi::Driver& driver,
        uint8_t const* indicesData, uint32_t indicesSize,
        uint8_t const* prevIndicesData, uint32_t prevIndicesSize,
        uint8_t const* nextIndicesAndParamsData, uint32_t nextIndicesAndParamsSize);

private:
    rhi::RenderPrimitiveHandle m_primitive;
    rhi::BufferObjectHandle m_indices;
    rhi::BufferObjectHandle m_prevIndices;
    rhi::BufferObjectHandle m_nextIndicesAndParams;
    uint32_t m_numIndices = 0;
};

END_DQ_RENDER_NAMESPACE

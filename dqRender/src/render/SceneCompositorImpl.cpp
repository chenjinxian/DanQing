// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Scene compositor implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/SceneCompositor.ts
//
// Implements the Compositor class which orchestrates the multi-pass rendering
// pipeline. All render states, method signatures, and control flow match
// the itwinjs-core reference exactly. The only difference is WebGL vs OpenGL
// API calls.
#include "SceneCompositorImpl.h"
#include "rhi/opengl/GlLoader.h"
#include "CachedGeometry.h"
#include "ViewportQuadGeometry.h"  // SkySphereViewportQuadGeometry (sky uniforms)
#include "Material.h"
#include "SurfaceGeometry.h"
#include "ThematicUniforms.h"
#include "DrawParams.h"
#include "TargetImpl.h"
#include "TechniqueImpl.h"
#include "ShaderProgramImpl.h"
#include "shader/OitShaders.h"
#include "shader/CompositeShaders.h"   // compositeHiliteFrag
#include "shader/PostProcessShaders.h" // kFullscreenQuadVert
#include "gl/GL.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// computeNormalMatrixFromMv — normal matrix = transpose(inverse(mat3(mv)))
// Ported from: itwinjs-core VertexShaderModules.h getNormalMatrixComputation()
// mv is a 16-float column-major mat4; outNm is a 9-float column-major mat3.
// Used by the live draw loop to set DrawParams::setNormalMatrix() (u_normalMatrix).
// ---------------------------------------------------------------------------
static void computeNormalMatrixFromMv(float const* mv, float* outNm)
{
    // mat3(mv): M[row][col], mv is column-major (mv[col*4 + row]).
    float const m00 = mv[0], m01 = mv[4], m02 = mv[8];
    float const m10 = mv[1], m11 = mv[5], m12 = mv[9];
    float const m20 = mv[2], m21 = mv[6], m22 = mv[10];

    // Cofactors of M.
    float const c00 =  (m11 * m22 - m12 * m21);
    float const c01 = -(m10 * m22 - m12 * m20);
    float const c02 =  (m10 * m21 - m11 * m20);
    float const c10 = -(m01 * m22 - m02 * m21);
    float const c11 =  (m00 * m22 - m02 * m20);
    float const c12 = -(m00 * m21 - m01 * m20);
    float const c20 =  (m01 * m12 - m02 * m11);
    float const c21 = -(m00 * m12 - m02 * m10);
    float const c22 =  (m00 * m11 - m01 * m10);

    float const det = m00 * c00 + m01 * c01 + m02 * c02;
    if (std::abs(det) < 1e-9f) {
        // Singular — fall back to identity.
        outNm[0] = 1.0f; outNm[1] = 0.0f; outNm[2] = 0.0f;
        outNm[3] = 0.0f; outNm[4] = 1.0f; outNm[5] = 0.0f;
        outNm[6] = 0.0f; outNm[7] = 0.0f; outNm[8] = 1.0f;
        return;
    }
    float const invDet = 1.0f / det;

    // normalMatrix[row][col] = cofactor[row][col] / det; column-major output.
    outNm[0] = c00 * invDet;  // col0 row0
    outNm[1] = c10 * invDet;  // col0 row1
    outNm[2] = c20 * invDet;  // col0 row2
    outNm[3] = c01 * invDet;  // col1 row0
    outNm[4] = c11 * invDet;  // col1 row1
    outNm[5] = c21 * invDet;  // col1 row2
    outNm[6] = c02 * invDet;  // col2 row0
    outNm[7] = c12 * invDet;  // col2 row1
    outNm[8] = c22 * invDet;  // col2 row2
}

// ---------------------------------------------------------------------------
// Constructor — initialize all 7 render states
// Ported from: itwinjs-core Compositor constructor (lines 1257-1286)
// ---------------------------------------------------------------------------
SceneCompositor::SceneCompositor(TargetImpl& target, Techniques& techniques)
    : m_target(target)
    , m_techniques(techniques)
    , m_batchState(target.getBatchState())
{
    // _opaqueRenderState: depthTest = true
    // All other flags at defaults: depthMask=true, blend=false, cull=false
    m_opaqueRenderState.flags.depthTest = true;
    m_opaqueRenderState.depthFunc = GL::DepthFunc::LessOrEqual;

    // _pointCloudRenderState: depthTest = true
    m_pointCloudRenderState.flags.depthTest = true;
    m_pointCloudRenderState.depthFunc = GL::DepthFunc::LessOrEqual;

    // _translucentRenderState:
    //   depthMask = false, blend = true, depthTest = true
    //   blendFunc = (ONE, ZERO, ONE, ONE_MINUS_SRC_ALPHA)
    m_translucentRenderState.flags.depthTest = true;
    m_translucentRenderState.flags.depthMask = false;
    m_translucentRenderState.flags.blend = true;
    m_translucentRenderState.depthFunc = GL::DepthFunc::LessOrEqual;
    // 参考 SceneCompositor.ts:1268 setBlendFuncSeparate(One, Zero, One, OneMinusSrcAlpha)。
    // itwinjs RenderStateBlend.setBlendFuncSeparate 的签名是 (srcRgb, srcAlpha,
    // dstRgb, dstAlpha)（RenderState.ts:159-164），本库签名一致 → 实参 1:1 照搬：
    //   functionSourceRgb   = One                → accum/revealage.rgb 加法累积
    //   functionSourceAlpha = Zero
    //   functionDestRgb     = One
    //   functionDestAlpha   = OneMinusSrcAlpha   → dst.a *= (1-src.a)
    // 效果（apply 发非索引 glBlendFuncSeparate，对双 MRT 同时生效）：
    //   accum.rgb    += Ci·wzi     ；accum.a   *= (1-ai)（清零 alpha=1 起乘 → Π(1-ai)，
    //   即 Composite.ts:112 transparent.a 的 "opaque 剩余份额"）
    //   revealage.r  += ai·wzi     ；revealage.a *= (1-ai·wzi)（合成不读 alpha）
    // （旧版按 GL 原生参数序误读为 (One,One,Zero,OMSA) → RGB 替换 + alpha over，
    //   空 accum 像素合成黑帧的链上因子。）
    m_translucentRenderState.blend.setBlendFuncSeparate(
        GL::BlendFactor::One,              // srcRgb
        GL::BlendFactor::Zero,             // srcAlpha
        GL::BlendFactor::One,              // dstRgb
        GL::BlendFactor::OneMinusSrcAlpha  // dstAlpha
    );

    // _hiliteRenderState:
    //   depthMask = false, blend = true
    //   destRgb = ONE, destAlpha = ONE (additive blending)
    m_hiliteRenderState.flags.depthMask = false;
    m_hiliteRenderState.flags.blend = true;
    m_hiliteRenderState.blend.setBlendFuncSeparate(
        GL::BlendFactor::One,  // srcRgb (default)
        GL::BlendFactor::One,  // srcAlpha (default)
        GL::BlendFactor::One,  // dstRgb
        GL::BlendFactor::One   // dstAlpha
    );

    // _noDepthMaskRenderState: depthMask = false
    m_noDepthMaskRenderState.flags.depthMask = false;

    // _overlayRenderState (WorldOverlay/ViewOverlay decorations):
    //   depthTest = false, depthMask = false, blend = true,
    //   blendFunc = (ONE, ONE_MINUS_SRC_ALPHA)  [premultiplied alpha]
    // Ported from: itwinjs-core Target._overlayRenderState. Overlays must BLEND so
    // translucent decoration fills (e.g. the ACS triad's arrows/disc at alpha ~0.22)
    // composite faintly instead of opaque. The prior code routed these passes to
    // m_noDepthMaskRenderState (blend off) — TargetImpl::drawOverlays set this blend
    // state but the compositor's applyRenderState immediately overwrote it.
    m_overlayRenderState.flags.depthTest = false;
    m_overlayRenderState.flags.depthMask = false;
    m_overlayRenderState.flags.blend = true;
    m_overlayRenderState.blend.setBlendFunc(
        GL::BlendFactor::One,
        GL::BlendFactor::OneMinusSrcAlpha
    );

    // _backgroundMapRenderState:
    //   depthMask = false, blend = true
    //   blendFunc = (ONE, ONE_MINUS_SRC_ALPHA)
    m_backgroundMapRenderState.flags.depthMask = false;
    m_backgroundMapRenderState.flags.blend = true;
    m_backgroundMapRenderState.blend.setBlendFunc(
        GL::BlendFactor::One,
        GL::BlendFactor::OneMinusSrcAlpha
    );

    // _layerRenderState:
    //   depthTest = true, depthFunc = Always
    //   blendFunc = (ONE, ONE_MINUS_SRC_ALPHA)
    m_layerRenderState.flags.depthTest = true;
    m_layerRenderState.depthFunc = GL::DepthFunc::Always;
    m_layerRenderState.blend.setBlendFunc(
        GL::BlendFactor::One,
        GL::BlendFactor::OneMinusSrcAlpha
    );
}

// TEMP-DIAG（DANQING_OIT_DUMP=1）：帧尾回读 OIT accum/revealage/opaque 快照纹理。
// 用途：1100 逻辑宽（≈2200 物理）Grid 消失的层间二分——accum 非零则
// renderTranslucent 已写入（断裂在 composite），accum 全零则断裂在 translucent
// 写入（uniform/几何/program）。Float 回读（RGBA16F 附件用 UBYTE 会得到天文
// 垃圾值，见 OpenGLDriver::readTexture 注释）。
static void dumpOitTexturesForDiag(rhi::Driver& driver,
                                   rhi::TextureHandle tex, uint32_t w, uint32_t h,
                                   char const* name)
{
    if (!tex) return;
    std::vector<float> px(static_cast<size_t>(w) * h * 4, 0.0f);
    rhi::PixelBufferDescriptor pbd(px.data(), px.size(),
        static_cast<GLenum>(GL::Texture::Format::Rgba),
        static_cast<GLenum>(GL::DataType::Float),
        0, 1, 0, 0, w, h);
    driver.readTexture(tex, 0, std::move(pbd));
    size_t nonzeroRgb = 0;
    float maxRgb = 0.0f, minA = 1e30f, maxA = -1e30f;
    float const* center = &px[(static_cast<size_t>(h / 2) * w + w / 2) * 4];
    for (size_t i = 0; i < px.size() / 4; ++i) {
        float const* p = &px[i * 4];
        float const m = std::max(std::abs(p[0]), std::max(std::abs(p[1]), std::abs(p[2])));
        if (m > 0.001f) ++nonzeroRgb;
        maxRgb = std::max(maxRgb, m);
        minA = std::min(minA, p[3]);
        maxA = std::max(maxA, p[3]);
    }
    std::fprintf(stderr, "[OITDUMP] %s %ux%u nonzeroRgb=%zu/%zu (%.1f%%) maxRgb=%.4g "
                         "a=[%.4g,%.4g] center=(%.4g,%.4g,%.4g,%.4g)\n",
                 name, w, h, nonzeroRgb, px.size() / 4,
                 100.0 * static_cast<double>(nonzeroRgb) / (px.size() / 4),
                 maxRgb, minA, maxA, center[0], center[1], center[2], center[3]);
    // TEMP-DIAG：line-类像素（a<0.85）行分布 + 中心带内计数（band 区与 dqApp
    // 测试同式）+ alpha 灰度 PPM（肉眼确认线在哪里）。
    {
        uint32_t rowMin = h, rowMax = 0, inBand = 0, total = 0;
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                float const a = px[(static_cast<size_t>(y) * w + x) * 4 + 3];
                if (a < 0.85f) {
                    ++total;
                    rowMin = std::min(rowMin, y);
                    rowMax = std::max(rowMax, y);
                    if (y >= h / 2 - h / 20 && y < h / 2 + h / 20) ++inBand;
                }
            }
        std::fprintf(stderr, "[OITDUMP] %s linePixels=%u rows=[%u,%u] inBand=%u\n",
                     name, total, rowMin, rowMax, inBand);
        // TEMP-DIAG：accum alpha 的 ASCII 图（~110 列）——线结构的空间分布。
        if (name[0] == 'a') {  // accum only
            uint32_t const cols = 110, rowsOut = 42;
            for (uint32_t ry = 0; ry < rowsOut; ++ry) {
                uint32_t const y = h - 1 - (ry * h + h / 2) / rowsOut;  // 顶朝上
                std::string lineStr;
                for (uint32_t cx = 0; cx < cols; ++cx) {
                    uint32_t const x = (cx * w + w / 2) / cols;
                    float const a = px[(static_cast<size_t>(y) * w + x) * 4 + 3];
                    char c = a >= 0.90f ? '.' : a >= 0.80f ? ':' : a >= 0.70f ? '-'
                           : a >= 0.60f ? '+' : a >= 0.55f ? '*' : '#';
                    lineStr.push_back(c);
                }
                std::fprintf(stderr, "[OITDUMP]|%s|\n", lineStr.c_str());
            }
        }
        char path[160];
        std::snprintf(path, sizeof(path), "D:\\Github\\DanQing\\build\\oitdump-%s.ppm", name);
        if (FILE* dbg = std::fopen(path, "wb")) {
            std::fprintf(dbg, "P6\n%u %u\n255\n", w, h);
            for (size_t i = 0; i < px.size() / 4; ++i) {
                // alpha 越低越暗（Π(1-ai)：线位更低）。
                unsigned char const v = static_cast<unsigned char>(
                    std::max(0.0f, std::min(1.0f, px[i * 4 + 3])) * 255.0f);
                unsigned char const rgb[3] = {v, v, v};
                std::fwrite(rgb, 1, 3, dbg);
            }
            std::fclose(dbg);
        }
    }
}

// TEMP-DIAG 同族：主渲染 RT 回读 + 中心带对比像素计数（与 dqApp 测试的
// contrastBand 同式），定位 composite 之后哪一步丢掉 Grid。
static void dumpMainRtForDiag(rhi::Driver& driver, rhi::RenderTargetHandle rt,
                              uint32_t w, uint32_t h, char const* name)
{
    if (!rt) return;
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4, 0);
    rhi::PixelBufferDescriptor pbd(px.data(), px.size(),
        static_cast<GLenum>(GL::Texture::Format::Rgba),
        static_cast<GLenum>(GL::DataType::UnsignedByte),
        0, 1, 0, 0, w, h);
    driver.readPixels(rt, 0, 0, w, h, std::move(pbd));
    // GL 坐标（左下原点）：bg 取左下角，band 取竖直中部 ±h/20。
    uint8_t const* bg = &px[(static_cast<size_t>(4) * w + 4) * 4];
    int band = 0;
    for (uint32_t y = h / 2 - h / 20; y < h / 2 + h / 20; ++y)
        for (uint32_t x = 0; x < w; x += 2) {
            uint8_t const* p = &px[(static_cast<size_t>(y) * w + x) * 4];
            int const d = std::abs(int(p[0]) - int(bg[0]))
                        + std::abs(int(p[1]) - int(bg[1]))
                        + std::abs(int(p[2]) - int(bg[2]));
            if (d > 150) ++band;
        }
    std::fprintf(stderr, "[OITDUMP] mainRT %s %ux%u band=%d center=(%d,%d,%d) corner=(%d,%d,%d)\n",
                 name, w, h, band,
                 px[(static_cast<size_t>(h / 2) * w + w / 2) * 4],
                 px[(static_cast<size_t>(h / 2) * w + w / 2) * 4 + 1],
                 px[(static_cast<size_t>(h / 2) * w + w / 2) * 4 + 2],
                 bg[0], bg[1], bg[2]);
}

SceneCompositor::~SceneCompositor()
{
    auto& driver = m_target.getDriver();
    // 析构清 program 跟踪（同 draw() 帧尾 deactivateProgram 的防御半边）：
    // ShaderProgram 实例跨 compositor 共享（同 pipeline driver），旧 compositor
    // 析构若留下 in-use program，新建 compositor 首帧 use() 即重入断言。
    deactivateProgram(driver);
    m_frameBuffers.dispose(&driver);
    m_textures.dispose(&driver);
    destroyOitResources(driver);
    // OIT 合成程序归本 compositor 持有（不经 Techniques）——析构时释放 GL 程序
    // 防泄漏（resize 路径已不销毁它，见 destroyOitResources 注释）。
    m_oitCompositeProgram.releaseGlProgram(driver);
}

// ---------------------------------------------------------------------------
// preDraw — viewport change detection, resource allocation
// Ported from: itwinjs-core Compositor.preDraw()
// Must be called before draw() each frame.  Detects viewport resize and
// re-allocates textures/FBOs accordingly.
// ---------------------------------------------------------------------------
void SceneCompositor::preDraw(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    // Skip if viewport hasn't changed and resources are already initialized
    if (m_resourcesInitialized && width == m_lastWidth && height == m_lastHeight)
        return;

    auto& driver = m_target.getDriver();

    // Dispose old resources
    m_frameBuffers.dispose(&driver);
    m_textures.dispose(&driver);
    // OIT 资源一并重置：参考的 translucent/clearTranslucent FBO 在 FrameBuffers.init
    // 里创建（SceneCompositor.ts:290-291），随 preDraw 的尺寸变化重建（:1310-1319
    // → reset :1677-1687 → dispose(_fbos) 含 translucent）。DanQing 的 OIT 是独立的
    // 惰性初始化（m_oitInitialized 一次性）——缺此重置时 resize 后 OIT 纹理保持
    // 旧尺寸（合成错乱的根因），对齐参考在此一并失效。
    destroyOitResources(driver);
    m_oitInitialized = false;

    // Initialize textures
    if (!m_textures.init(driver, width, height))
        return;

    // Initialize FBOs (depth texture is created internally by the driver)
    // Pass MSAA sample count so opaque render targets can be multisampled.
    rhi::TextureHandle depth;  // Will be set by FBO init
    if (!m_frameBuffers.init(driver, m_textures, depth, m_antialiasSamples))
        return;

    m_lastWidth = width;
    m_lastHeight = height;
    m_resourcesInitialized = true;
}

// ---------------------------------------------------------------------------
// OIT resource management
// Ported from: itwinjs-core SceneCompositor.ts _createTranslucentFbos
// ---------------------------------------------------------------------------
void SceneCompositor::initOitResources(rhi::Driver& driver)
{
    if (m_oitInitialized) return;

    auto rect = m_target.getViewRect();
    uint32_t w = rect.width();
    uint32_t h = rect.height();
    if (w == 0 || h == 0) return;

    // Create MRT render target with 2 color attachments (RGBA16F + RGBA16F) + depth
    // Ported from: itwinjs-core SceneCompositor.ts (accumulation + revealage both RGBA16F)
    rhi::TextureFormat colorFormats[2] = {
        rhi::TextureFormat::RGBA16F,  // accumulation
        rhi::TextureFormat::RGBA16F   // revealage (vec4 per itwinjs-core Translucency.ts)
    };
    m_oitRenderTarget = driver.createRenderTargetMRT(
        rhi::TargetBufferFlags::COLOR_ALL | rhi::TargetBufferFlags::DEPTH,
        w, h, 1, 1, 2, colorFormats, rhi::TextureFormat::DEPTH24);

    if (!m_oitRenderTarget) return;

    // Get texture handles for composite pass sampling
    m_oitAccumTexture = driver.getRenderTargetColorAttachment(m_oitRenderTarget, 0);
    m_oitRevealageTexture = driver.getRenderTargetColorAttachment(m_oitRenderTarget, 1);

    // Opaque 快照 RT：composite 前从主 RT blit（参考 computeOpaqueColor 采样场景色）。
    m_opaqueSceneRenderTarget = driver.createRenderTarget(
        rhi::TargetBufferFlags::COLOR_ALL, w, h, 1, 1);
    m_opaqueSceneTexture = driver.getRenderTargetColorAttachment(m_opaqueSceneRenderTarget, 0);

    // compile OIT composite shader
    {
        m_oitCompositeProgram.setSource(kOitCompositeVert, kOitCompositeFrag, "OitComposite");
        CompileStatus status = m_oitCompositeProgram.compile(driver);
        if (status != CompileStatus::Success) {
            destroyOitResources(driver);
            return;
        }
    }

    // Create fullscreen quad geometry
    {
        rhi::AttributeArray attrs = {};
        attrs[0].buffer = 0;
        attrs[0].offset = 0;
        attrs[0].type = rhi::ElementType::FLOAT2;

        m_quadVbih = driver.createVertexBufferInfo(1, 1, attrs);
        m_quadVbh = driver.createVertexBuffer(4, m_quadVbih);

        struct QuadVertex { float x, y; };
        QuadVertex vertices[4] = {
            {-1.0f, -1.0f}, {+1.0f, -1.0f}, {+1.0f, +1.0f}, {-1.0f, +1.0f},
        };

        auto vbo = driver.createBufferObject(
            sizeof(vertices), rhi::BufferObjectBinding::VERTEX, rhi::BufferUsage::STATIC);
        rhi::BufferDescriptor vboData(vertices, sizeof(vertices));
        driver.updateBufferObject(vbo, std::move(vboData), 0);
        driver.setVertexBufferObject(m_quadVbh, 0, vbo);

        uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
        // createIndexBuffer only glGenBuffers (no data store); updateIndexBuffer
        // glBufferData's the IndexBufferHandle compositeOit's draw2(0,6,0) binds as
        // GL_ELEMENT_ARRAY_BUFFER. The prior orphan-ibo pattern (createBufferObject
        // + updateBufferObject on a disconnected handle) left m_quadIbh unbacked ->
        // glDrawElements SIGSEGV. Same bug class as MeshGraphic/PolyfaceGraphic.
        m_quadIbh = driver.createIndexBuffer(rhi::ElementType::USHORT, 6, rhi::BufferUsage::STATIC);
        rhi::BufferDescriptor iboData(indices, sizeof(indices));
        driver.updateIndexBuffer(m_quadIbh, std::move(iboData), 0);

        m_quadPrimitive = driver.createRenderPrimitive(m_quadVbh, m_quadIbh, rhi::PrimitiveType::TRIANGLES);
    }

    m_oitInitialized = true;
}

void SceneCompositor::destroyOitResources(rhi::Driver& driver)
{
    if (!m_oitInitialized) return;

    if (m_quadPrimitive) driver.destroyRenderPrimitive(m_quadPrimitive);
    if (m_quadIbh) driver.destroyIndexBuffer(m_quadIbh);
    if (m_quadVbh) driver.destroyVertexBuffer(m_quadVbh);
    if (m_quadVbih) driver.destroyVertexBufferInfo(m_quadVbih);
    // 程序不在此销毁：对齐参考 SceneCompositor 的 resize 处置（dispose(_fbos)
    // 只销毁 FBO/纹理——SceneCompositor.ts:1677-1687），shader 属 Techniques 随
    // GL 上下文存活。此处销毁程序而不重置 ShaderProgram::m_status 会让 compile()
    // 缓存 Success 早退、m_programHandle 悬空 → use() 时 driver.useProgram 的
    // handle_cast 落空静默跳过 glUseProgram → 合成 quad 被残留 program 绘制
    // （resize 后 Grid 消失的根因，2026-09-14 [COMPDIAG] 实锤）。析构路径的
    // 程序销毁见 ~SceneCompositor。
    if (m_opaqueSceneRenderTarget) {
        driver.destroyRenderTarget(m_opaqueSceneRenderTarget);
        m_opaqueSceneRenderTarget = rhi::RenderTargetHandle{};
        m_opaqueSceneTexture = rhi::TextureHandle{};
    }
    if (m_oitRenderTarget) driver.destroyRenderTarget(m_oitRenderTarget);

    m_oitRenderTarget = rhi::RenderTargetHandle();
    m_oitAccumTexture = rhi::TextureHandle();
    m_oitRevealageTexture = rhi::TextureHandle();
    m_oitInitialized = false;
}

// ---------------------------------------------------------------------------
// compositeOit — resolve OIT accumulation/revealage to main framebuffer
// Ported from: itwinjs-core SceneCompositor.ts composite()
// ---------------------------------------------------------------------------
void SceneCompositor::compositeOit(rhi::Driver& driver)
{
    if (!m_oitInitialized) return;

    auto rect = m_target.getViewRect();

    // 参考 Composite.ts computeOpaqueColor：over 合成需采样"已画 opaque 场景色"。
    // 同一 FBO 不可边读边写——先把主 RT 的 color blit 到 opaque 快照 RT。
    if (m_opaqueSceneRenderTarget) {
        rhi::Viewport full;
        full.left = 0;
        full.bottom = 0;
        full.width = rect.width();
        full.height = rect.height();
        driver.blit(rhi::TargetBufferFlags::COLOR_ALL, m_opaqueSceneRenderTarget,
                    full, m_target.getRenderTarget(), full);
    }

    // Bind main render target (don't clear)
    rhi::RenderPassParams params;
    params.viewport = {static_cast<int32_t>(rect.left), static_cast<int32_t>(rect.top),
                       rect.width(), rect.height()};
    driver.beginRenderPass(m_target.getRenderTarget(), params);

    // 渲染状态已由 composite() 经 RenderState 通道设为 defaults（blend/depthTest/
    // cull 全关，参考 System.applyRenderState(RenderState.defaults)）——不再走
    // RHI bindPipeline（两套状态跟踪器互不知晓，RHI 侧去重会跳过 GL_BLEND 关闭）。

    // Bind composite shader
    {
        ShaderProgramParams shaderParams;
        shaderParams.setInt("u_accumTexture", 0);
        shaderParams.setInt("u_revealTexture", 1);
        shaderParams.setInt("u_opaqueTexture", 2);
        activateProgram(&m_oitCompositeProgram, driver, shaderParams);
        // 名字型 uniform（sampler 单元号）经 uploadUniforms 上传——use() 只跑
        // ProgramUniform 绑定。漏掉时 sampler 全默认 0：u_revealTexture 采到
        // accum 纹理 → t.rgb = accum/accum ≡ 1 → 合成整帧爆白（网格排查
        // 数值探针定位）。
        m_oitCompositeProgram.uploadUniforms(driver, shaderParams);
        driver.bindTexture(0, m_oitAccumTexture);
        driver.bindTexture(1, m_oitRevealageTexture);
        driver.bindTexture(2, m_opaqueSceneTexture);
    }

    // Draw fullscreen quad
    driver.bindRenderPrimitive(m_quadPrimitive);
    driver.draw2(0, 6, 0);

    // deactivateProgram = endUse + 清空跟踪（合成后 m_activeProgram 必须为空，
    // 否则下一帧 activate(同程序) 会跳过 use() 而实际 GL 绑定已被切换）。
    deactivateProgram(driver);
    driver.endRenderPass();
}

// ---------------------------------------------------------------------------
// draw — main rendering pipeline (16 steps)
// Ported from: itwinjs-core Compositor.draw() (lines 1410-1519)
// ---------------------------------------------------------------------------
void SceneCompositor::draw(RenderCommands& commands)
{
    auto& driver = m_target.getDriver();

    // Step -1: Seed the branch-stack base with the target's view/projection.
    // Reference: itwinjs BranchUniforms.update computes u_mv = frustum.viewMatrix·model
    // at draw time (BranchUniforms.ts:215-226); DanQing's dispatch uploads branch-stack
    // matrices directly, so the view enters through the bottom state. Without this,
    // every branch mvp lacks the camera transform (geometry maps as if the camera sat
    // at the origin — scene z collapses onto the near plane and clips away).
    {
        auto const& view = m_target.getUniforms().frustum.getViewMatrix();  // Transform
        // Transform (row-major Matrix3d + origin) → column-major float[16].
        auto const& r = view.matrix.coffs;
        std::array<float, 16> mv = {
            static_cast<float>(r[0]), static_cast<float>(r[3]), static_cast<float>(r[6]), 0.0f,
            static_cast<float>(r[1]), static_cast<float>(r[4]), static_cast<float>(r[7]), 0.0f,
            static_cast<float>(r[2]), static_cast<float>(r[5]), static_cast<float>(r[8]), 0.0f,
            static_cast<float>(view.origin.x), static_cast<float>(view.origin.y),
            static_cast<float>(view.origin.z), 1.0f};
        // mvp = projection (row-major Matrix4d) · view. Row-major 4x4 view:
        Matrix4d const viewRow = Matrix4d::CreateRowValues(
            r[0], r[1], r[2], view.origin.x,
            r[3], r[4], r[5], view.origin.y,
            r[6], r[7], r[8], view.origin.z,
            0.0, 0.0, 0.0, 1.0);
        Matrix4d const mvpRow = m_target.getFrustumUniforms().getProjectionMatrix().MultiplyMatrixMatrix(viewRow);
        // Row-major Matrix4d → column-major float[16]: transpose indexing [c*4+r].
        std::array<float, 16> mvp;
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                mvp[static_cast<size_t>(col * 4 + row)] =
                    static_cast<float>(mvpRow.at(row, col));
        // 仅空栈时 seed：OpenGLRenderTarget::drawFrame 显式 push(m_viewportMv,
        // m_viewportMvp)（真实矩阵）——单层栈下无条件 seed 会用 frustum-uniforms
        // 推导值覆写 push 的 mvp（推导链 changeProjectionMatrix(mvp·mv⁻¹) 在奇异/
        // 未设时给零矩阵）→ branchStack 全零 → 图元塌 clip 原点（accum 恒 0 整帧
        // 黑，GridAppDiag 复现）。参考 BranchUniforms.update 的 draw 时计算等价于
        // drawFrame 的 push；此 seed 仅作无 push 路径兜底。
        if (m_branchStack.getDepth() == 0)
            m_branchStack.seedBaseTransforms(mv, mvp);
    }

    // Step 0: Pre-draw lifecycle (viewport change detection, resource allocation)
    auto rect = m_target.getViewRect();
    preDraw(rect.width(), rect.height());

    // Step 1: Get compositeFlags from commands
    // Ported from: itwinjs-core Compositor.draw() compositeFlags
    auto compositeFlags = static_cast<CompositeFlags>(commands.getCompositeFlags());
    bool needComposite = compositeFlags != CompositeFlags::None;

    // Step 2: Clear opaque buffers
    m_target.beginPerfMetricRecord("Clear Opaque");   // SceneCompositor.ts:1419
    clearOpaque(needComposite);
    m_target.endPerfMetricRecord();

    // Step 3: Render background
    m_target.beginPerfMetricRecord("Render Background");  // :1427
    renderBackground(commands, needComposite);
    m_target.endPerfMetricRecord();

    // Step 4: Render skybox
    m_target.beginPerfMetricRecord("Render Skybox");  // :1432
    renderSkyBox(commands, needComposite);
    m_target.endPerfMetricRecord();

    // Step 5: Render background map
    m_target.beginPerfMetricRecord("Render Background Map");  // :1437
    renderBackgroundMap(commands, needComposite);
    m_target.endPerfMetricRecord();

    // Step 6: Render volume classification
    m_target.beginPerfMetricRecord("Render VolumeClassification");  // :1450
    renderVolumeClassification(commands, compositeFlags, false);
    m_target.endPerfMetricRecord();

    // Step 7: Render opaque layers
    m_target.beginPerfMetricRecord("Render Opaque Layers");  // 参考同名
    renderLayers(commands, needComposite, RenderPass::OpaqueLayers);
    m_target.endPerfMetricRecord();

    // Step 7: Render point clouds
    m_target.beginPerfMetricRecord("Render PointClouds");  // :1474
    renderPointClouds(commands, compositeFlags);
    m_target.endPerfMetricRecord();

    // Step 8: Render opaque geometry
    m_target.beginPerfMetricRecord("Render Opaque");  // 参考同名
    renderOpaque(commands, compositeFlags, false);
    m_target.endPerfMetricRecord();

    // Step 9: Render translucent layers
    m_target.beginPerfMetricRecord("Render Translucent Layers");  // 参考同名
    renderLayers(commands, needComposite, RenderPass::TranslucentLayers);
    m_target.endPerfMetricRecord();

    // Step 10: Render translucent + hilite + composite (if needed)
    if (needComposite) {
        // Initialize OIT composite shader if needed (kept separate from textures/FBOs)
        if (!m_oitInitialized)
            initOitResources(driver);

        // Clear OIT：直接清渲染 FBO（m_oitRenderTarget）。
        // 参考 Compositor.clearTranslucent 用 clearTranslucent **program 画进
        // translucent FBO**（同一 FBO，SceneCompositor.ts:1176-1184），该 program 的
        // assignFragData（ClearTranslucent.ts:14-17）：
        //   FragColor0 = (0,0,0,1)   // accum：rgb=0，**alpha=1**
        //   FragColor1 = (1,0,0,1)   // revealage：r=1，alpha=1
        // accum 清零 alpha=1 是合成公式的恒等元：空像素 transparent.a=accum.a=1 →
        // col=(1-1)·transparent + 1·opaque = opaque（天空/背景原样穿透）。若误清为
        // alpha=0，空像素合成出 (0,0,0,0) 直接黑掉整帧（真窗口路径实测根因）。
        if (m_oitInitialized) {
            rhi::RenderPassParams clearParams;
            clearParams.flags.clear = rhi::TargetBufferFlags::COLOR_ALL
                                    | rhi::TargetBufferFlags::DEPTH;
            clearParams.perAttachmentClearCount = 2;
            clearParams.perAttachmentClearColors[0].f[0] = 0.0f;
            clearParams.perAttachmentClearColors[0].f[1] = 0.0f;
            clearParams.perAttachmentClearColors[0].f[2] = 0.0f;
            clearParams.perAttachmentClearColors[0].f[3] = 1.0f;                   // accum=(0,0,0,1)
            clearParams.perAttachmentClearColors[1].f[0] = 1.0f;
            clearParams.perAttachmentClearColors[1].f[1] = 0.0f;
            clearParams.perAttachmentClearColors[1].f[2] = 0.0f;
            clearParams.perAttachmentClearColors[1].f[3] = 1.0f;                   // revealage=(1,0,0,1)
            auto viewRect = m_target.getViewRect();
            clearParams.viewport = {static_cast<int32_t>(viewRect.left), static_cast<int32_t>(viewRect.top),
                                   viewRect.width(), viewRect.height()};
            driver.beginRenderPass(m_oitRenderTarget, clearParams);
            driver.endRenderPass();
        }

        // Render translucent
        m_target.beginPerfMetricRecord("Render Translucent");  // SceneCompositor.ts:1494
        renderTranslucent(commands);
        m_target.endPerfMetricRecord();

        // TEMP-DIAG：translucent 命令清单——grid 应只有一份（参考 drawStandardGrid
        // 每帧一次）。多份 = 多层 Π(1-ai) 叠乘 → plane 变暗（黑方块）/线相位拍频。
        if (std::getenv("DANQING_OIT_DUMP")) {
            for (auto const& dc : commands.getCommands(RenderPass::Translucent)) {
                auto* cmd = dc.get();
                auto* geo = cmd->getType() == DrawCommandType::Primitive
                              ? static_cast<PrimitiveCommand*>(cmd)->getGeometry()
                              : nullptr;
                if (auto* grid = geo ? geo->asPlanarGrid() : nullptr)
                    std::fprintf(stderr,
                        "[OITDUMP] translucent cmd type=%d grid=%p verts=%u q=(%.4f,%.4f,%.4f,%.4f)\n",
                        static_cast<int>(cmd->getType()), static_cast<void*>(grid),
                        grid->getVertexCount(),
                        grid->getQTexCoordParams()[0], grid->getQTexCoordParams()[1],
                        grid->getQTexCoordParams()[2], grid->getQTexCoordParams()[3]);
                else
                    std::fprintf(stderr, "[OITDUMP] translucent cmd type=%d grid=no\n",
                                 static_cast<int>(cmd->getType()));
            }
        }
        // Render hilite
        m_target.beginPerfMetricRecord("Render Hilite");  // SceneCompositor.ts:1501
        renderHilite(commands);
        m_target.endPerfMetricRecord();

        // TEMP-DIAG：composite 前主 RT 基线（对照 postComposite，判定合成
        // quad 是否真的改写了主 RT）。
        if (std::getenv("DANQING_OIT_DUMP"))
            dumpMainRtForDiag(driver, m_target.getRenderTarget(), rect.width(), rect.height(), "preComposite");

        // Composite
        m_target.beginPerfMetricRecord("Composite");  // 参考同名
        composite(static_cast<uint8_t>(compositeFlags) & static_cast<uint8_t>(GL::CompositeFlags::Translucent));
        m_target.endPerfMetricRecord();

        // TEMP-DIAG（env 开关，零常态开销）：OIT 三纹理帧尾回读。
        static bool const s_dumpOit = std::getenv("DANQING_OIT_DUMP") != nullptr;
        if (s_dumpOit) {
            dumpMainRtForDiag(driver, m_target.getRenderTarget(), rect.width(), rect.height(), "postComposite");
            dumpOitTexturesForDiag(driver, m_oitAccumTexture, rect.width(), rect.height(), "accum");
            dumpOitTexturesForDiag(driver, m_oitRevealageTexture, rect.width(), rect.height(), "revealage");
            dumpOitTexturesForDiag(driver, m_opaqueSceneTexture, rect.width(), rect.height(), "opaqueSnapshot");
        }
    }

    // Step 11: Render overlay layers
    m_target.beginPerfMetricRecord("Render Overlay Layers");  // 参考同名
    renderLayers(commands, false, RenderPass::OverlayLayers);
    m_target.endPerfMetricRecord();

    // TEMP-DIAG：帧尾主 RT 状态（与 postComposite 对照，锁定 composite 之后
    // 是否还有步骤改写主 RT）。
    if (compositeFlags != CompositeFlags::None && std::getenv("DANQING_OIT_DUMP"))
        dumpMainRtForDiag(driver, m_target.getRenderTarget(), rect.width(), rect.height(), "endDraw");

    // 帧尾无条件清 program 跟踪（endUse 当前 active program）。参考 executor 的
    // active program 是 per-WebGL-context 的（target 不可重建），跨帧滞留自洽；
    // DanQing 的 Techniques/ShaderProgram 实例跨 RenderTarget 共享（同 pipeline 的
    // driver/context，Viewport::resizeEvent 重建 target），needComposite=false 的
    // 帧（Composite 段整体跳过，composite 内的 deactivateProgram 不执行）帧尾
    // 滞留 in-use program——target 重建后新 compositor 首帧 use() 同一 program
    // 即重入断言（resize 风暴崩溃，View3DResizeTest 复现）。
    deactivateProgram(driver);
}

// ---------------------------------------------------------------------------
// clearOpaque — clear pick data buffers + depth
// Ported from: itwinjs-core Compositor.clearOpaque() (lines 918-934)
// ---------------------------------------------------------------------------
void SceneCompositor::clearOpaque(bool /*needComposite*/)
{
    auto& driver = m_target.getDriver();
    auto rect = m_target.getViewRect();
    float const* bgColor = m_target.getBackgroundColor();

    rhi::RenderPassParams params;
    params.flags.clear = rhi::TargetBufferFlags::COLOR_ALL | rhi::TargetBufferFlags::DEPTH;
    params.clearColor.f[0] = bgColor[0];
    params.clearColor.f[1] = bgColor[1];
    params.clearColor.f[2] = bgColor[2];
    params.clearColor.f[3] = bgColor[3];
    params.clearDepth = 1.0;
    params.viewport = {static_cast<int32_t>(rect.left), static_cast<int32_t>(rect.top),
                       rect.width(), rect.height()};

    driver.beginRenderPass(m_target.getRenderTarget(), params);
    driver.endRenderPass();
}

// ---------------------------------------------------------------------------
// renderBackground — render background commands
// Ported from: itwinjs-core Compositor.renderBackground() (lines 1758-1769)
// ---------------------------------------------------------------------------
void SceneCompositor::renderBackground(RenderCommands& commands, bool /*needComposite*/)
{
    auto const& cmds = commands.getCommands(RenderPass::Background);
    if (cmds.empty()) return;

    auto& driver = m_target.getDriver();
    auto rect = m_target.getViewRect();

    rhi::RenderPassParams params;
    params.viewport = {static_cast<int32_t>(rect.left), static_cast<int32_t>(rect.top),
                       rect.width(), rect.height()};
    driver.beginRenderPass(m_target.getRenderTarget(), params);

    applyRenderState(RenderPass::Background);
    drawPass(commands, RenderPass::Background);

    driver.endRenderPass();
}

// ---------------------------------------------------------------------------
// renderSkyBox — render skybox commands
// Ported from: itwinjs-core Compositor.renderSkyBox() (lines 1744-1756)
// ---------------------------------------------------------------------------
void SceneCompositor::renderSkyBox(RenderCommands& commands, bool /*needComposite*/)
{
    auto const& cmds = commands.getCommands(RenderPass::SkyBox);
    if (cmds.empty()) return;

    auto& driver = m_target.getDriver();
    auto rect = m_target.getViewRect();

    rhi::RenderPassParams params;
    params.viewport = {static_cast<int32_t>(rect.left), static_cast<int32_t>(rect.top),
                       rect.width(), rect.height()};
    driver.beginRenderPass(m_target.getRenderTarget(), params);

    applyRenderState(RenderPass::SkyBox);
    drawPass(commands, RenderPass::SkyBox);
    driver.endRenderPass();
}

// ---------------------------------------------------------------------------
// renderBackgroundMap — render background map commands
// Ported from: itwinjs-core Compositor.renderBackgroundMap()
// ---------------------------------------------------------------------------
void SceneCompositor::renderBackgroundMap(RenderCommands& commands, bool /*needComposite*/)
{
    auto const& cmds = commands.getCommands(RenderPass::BackgroundMap);
    if (cmds.empty()) return;

    auto& driver = m_target.getDriver();
    auto rect = m_target.getViewRect();

    rhi::RenderPassParams params;
    params.viewport = {static_cast<int32_t>(rect.left), static_cast<int32_t>(rect.top),
                       rect.width(), rect.height()};
    driver.beginRenderPass(m_target.getRenderTarget(), params);

    applyRenderState(RenderPass::BackgroundMap);
    drawPass(commands, RenderPass::BackgroundMap);

    driver.endRenderPass();
}

// ---------------------------------------------------------------------------
// renderOpaque — render opaque geometry (Linear, Planar, General, HiddenEdge)
// Ported from: itwinjs-core Compositor.renderOpaque() (lines 947-982)
// ---------------------------------------------------------------------------
void SceneCompositor::renderOpaque(RenderCommands& commands,
                                   CompositeFlags /*compositeFlags*/,
                                   bool /*renderForReadPixels*/)
{
    auto& driver = m_target.getDriver();

    auto rect = m_target.getViewRect();
    float const* bgColor = m_target.getBackgroundColor();

    rhi::RenderPassParams params;
    // LOAD color, clear DEPTH only. clearOpaque already cleared color (bgColor)
    // and depth; renderSkyBox then drew the sky with depth-write off, so the
    // sky color sits in the framebuffer and depth is still 1.0. Re-clearing
    // color here would wipe the sky. Clearing depth fresh keeps opaque geometry
    // depth-correct (z < 1.0 passes LEQUAL against 1.0).
    params.flags.clear = rhi::TargetBufferFlags::DEPTH;
    params.clearColor.f[0] = bgColor[0];
    params.clearColor.f[1] = bgColor[1];
    params.clearColor.f[2] = bgColor[2];
    params.clearColor.f[3] = bgColor[3];
    params.clearDepth = 1.0;
    params.viewport = {static_cast<int32_t>(rect.left), static_cast<int32_t>(rect.top),
                       rect.width(), rect.height()};

    driver.beginRenderPass(m_target.getRenderTarget(), params);

    // Set frame-constant uniforms once per pass
    m_frameParams = ShaderProgramParams{};
    // Faithful: reference params.target.uniforms.* — lets ProgramUniform binding
    // callbacks (e.g. u_frustum via Common.addFrustum) read the target at use().
    m_frameParams.setTarget(&m_target);
    float const* sunDir = m_target.getSunDir();
    float sunIntensity = m_target.getSunIntensity();
    float const* ambientColor = m_target.getAmbientColor();

    float sunDirNorm = std::sqrt(sunDir[0]*sunDir[0] + sunDir[1]*sunDir[1] + sunDir[2]*sunDir[2]);
    if (sunDirNorm > 0.0f) {
        float normalizedSunDir[3] = {sunDir[0] / sunDirNorm, sunDir[1] / sunDirNorm, sunDir[2] / sunDirNorm};
        m_frameParams.setVec3("u_sunDir", normalizedSunDir);
    }
    m_frameParams.setFloat("u_sunIntensity", sunIntensity);
    m_frameParams.setVec3("u_ambientColor", ambientColor);

    // Lighting (u_sunDir view-space + u_lightSettings[16]) — dispatched by the
    // ProgramUniform bindings at use() time (wireSunDirection/wireLightSettings
    // read target.uniforms via params.getTarget(), set above); the handle's
    // location is resolved at compile and set* dispatches glUniform* directly.
    // Ported from: itwinjs-core Lighting.ts addLighting() uniforms (:120-135) —
    //   u_sunDir       ← target.uniforms.bindSunDirection (SunDirection, view
    //                    space; default (0.272166,0.680414,0.680414) when no
    //                    world sun dir — TargetUniforms.SunDirection)
    //   u_lightSettings← target.uniforms.lights.bind (LightingUniforms 16-float
    //                    packing, updated by TargetUniforms.updateRenderPlan ←
    //                    Target.changeRenderPlan Target.ts:543)
    // The legacy name-value upload of the same values was removed (TD-15);
    // without u_lightSettings the array uploads GL-default zeros: sun 0 +
    // ambient 0 → applyLighting returns black for every lit surface.

    // Draw opaque passes in order
    drawPass(commands, RenderPass::OpaqueLinear);
    drawPass(commands, RenderPass::OpaquePlanar);
    drawPass(commands, RenderPass::OpaqueGeneral);
    drawPass(commands, RenderPass::HiddenEdge);

    driver.endRenderPass();
}

// ---------------------------------------------------------------------------
// renderTranslucent — render translucent geometry to OIT FBO
// Ported from: itwinjs-core Compositor.renderTranslucent() (lines 1186-1190)
// ---------------------------------------------------------------------------
void SceneCompositor::renderTranslucent(RenderCommands& commands)
{
    auto& driver = m_target.getDriver();
    auto rect = m_target.getViewRect();

    if (!m_oitInitialized) {
        // Fallback: direct alpha blend
        rhi::RenderPassParams params;
        params.viewport = {static_cast<int32_t>(rect.left), static_cast<int32_t>(rect.top),
                           rect.width(), rect.height()};
        driver.beginRenderPass(m_target.getRenderTarget(), params);
        applyRenderState(RenderPass::Translucent);
        drawPass(commands, RenderPass::Translucent);
        driver.endRenderPass();
        return;
    }

    // Render to OIT FBO with per-attachment blend
    rhi::RenderPassParams params;
    params.flags.clear = rhi::TargetBufferFlags::DEPTH;
    params.viewport = {static_cast<int32_t>(rect.left), static_cast<int32_t>(rect.top),
                       rect.width(), rect.height()};
    driver.beginRenderPass(m_oitRenderTarget, params);

    // 混合状态由 drawPass → applyRenderState(RenderPass::Translucent) →
    // m_translucentRenderState.apply 施加（参考机制：System.applyRenderState(
    // _translucentRenderState) 的非索引 glBlendFuncSeparate 对双 MRT 同时生效）。
    // 此前的 RHI bindPipeline per-attachment 块是自创重复，且在 drawPass 内被
    // applyRenderState 覆写——已删除（RHI 枚举映射缺陷修复后不再需要绕行）。

    drawPass(commands, RenderPass::Translucent);
    driver.endRenderPass();
}

// ---------------------------------------------------------------------------
// renderHilite — render hilite geometry
// Ported from: itwinjs-core Compositor.renderHilite() (lines 2261-2296)
// ---------------------------------------------------------------------------
void SceneCompositor::renderHilite(RenderCommands& commands)
{
    auto& driver = m_target.getDriver();
    auto rect = m_target.getViewRect();

    rhi::RenderPassParams params;
    params.flags.clear = rhi::TargetBufferFlags::COLOR_ALL;
    params.clearColor.f[0] = 0.0f;
    params.clearColor.f[1] = 0.0f;
    params.clearColor.f[2] = 0.0f;
    params.clearColor.f[3] = 0.0f;
    params.viewport = {static_cast<int32_t>(rect.left), static_cast<int32_t>(rect.top),
                       rect.width(), rect.height()};

    // Draw into the DEDICATED hilite buffer, not the main render target —
    // composite() blends u_hilite over the opaque scene afterward. Clearing the
    // main target here would erase the already-rendered opaque scene.
    // Ported from: itwinjs-core Compositor.renderHilite() (fbos.hilite execute).
    auto hiliteFbo = m_frameBuffers.getHilite();
    driver.beginRenderPass(hiliteFbo ? hiliteFbo : m_target.getRenderTarget(), params);
    applyRenderState(RenderPass::Hilite);
    drawPass(commands, RenderPass::Hilite);
    driver.endRenderPass();
}

// ---------------------------------------------------------------------------
// composite — resolve OIT and hilite to main framebuffer
// Ported from: itwinjs-core Compositor.composite() (lines 2298-2302)
// ---------------------------------------------------------------------------
void SceneCompositor::composite(bool wantTranslucent)
{
    auto& driver = m_target.getDriver();

    // 参考 SceneCompositor.composite()（SceneCompositor.ts:2298-2302）：
    //   System.instance.applyRenderState(RenderState.defaults);
    // RenderState 通道发裸 glEnable/glDisable/glBlendFuncSeparate。合成必须经
    // 同一通道回到 defaults（blend=false）：translucent pass 由
    // m_translucentRenderState.apply 裸启用的 GL_BLEND 加法态不被 RHI 的
    // OpenGLState 跟踪器知晓，走 RHI bindPipeline 关混合会被其去重跳过 →
    // 加法残留 → 合成 quad 输出叠到目标上（天空 142+142 饱和爆白）。
    RenderState::defaults().apply(m_currentRenderState);
    m_currentRenderState = RenderState::defaults();

    // 参考 SceneCompositor.composite()（CompositeFlags 分派变体）：仅当
    // Translucent 参与时走 OIT 合成——纯 Hilite 场景 accum/revealage 为空，
    // OIT 公式 (1-ta)·t + ta·opaque 会输出黑覆盖整帧（hilite 视觉回归）。
    if (wantTranslucent)
        compositeOit(driver);
}

// ---------------------------------------------------------------------------
// compositeHilite — fullscreen blend of the hilite buffer over the opaque scene
// Ported from: itwinjs-core SceneCompositor.ts composite() + Composite.ts
//              compositeHiliteFrag (opaque + hilite textures → applyHilite)
//
// TODO(phase-N): the full hilite composite is not wired — the hilite FBO is
// rendered by renderHilite but never composited back. The current hilite visual
// comes from the Surface override shader's LUT-driven Hilited mix (the pixel
// assertion in RenderSmokeTest.HilitePassShiftsSelectedFeatureColor verifies the
// recolor). Wiring compositeHilite requires the edge-detection hilite settings
// (u_hilite_settings mat3) + the composite fullscreen-quad FBO dance — deferred.
// ---------------------------------------------------------------------------
void SceneCompositor::compositeHilite(rhi::Driver& /*driver*/)
{
    // Intentionally not wired — see the TODO above.
}

// ---------------------------------------------------------------------------
// pingPong — copy pick data between textures
// Ported from: itwinjs-core Compositor.pingPong() (lines 1196-1208)
// ---------------------------------------------------------------------------
void SceneCompositor::pingPong()
{
    // Ported from: itwinjs-core Compositor.pingPong() (lines 1196-1208)
    //
    // Copies pick data (featureId and depthAndOrder) from the opaqueAll FBO
    // to the pingPong FBO so they can be read back for picking.
    //
    // For non-MSAA rendering: the data is already in the correct textures.
    // For MSAA: resolve the multisampled opaqueAll FBO to the pingPong FBO.

    auto& driver = m_target.getDriver();
    auto opaqueAll = m_frameBuffers.getOpaqueAll();
    auto pingPong = m_frameBuffers.getPingPong();

    if (opaqueAll && pingPong) {
        // Extract pick data texture handles from opaqueAll FBO.
        // Attachment 0 = color, 1 = featureId, 2 = depthAndOrder.
        auto featureIdTex = driver.getRenderTargetColorAttachment(opaqueAll, 1);
        auto depthAndOrderTex = driver.getRenderTargetColorAttachment(opaqueAll, 2);

        if (featureIdTex && depthAndOrderTex) {
            // Bind pick data textures for the copy shader.
            // Ported from: itwinjs-core CopyPickBufferGeometry texture binding
            driver.bindTexture(0, featureIdTex);
            driver.bindTexture(1, depthAndOrderTex);

            // Use CopyPickBuffers technique to render fullscreen quad
            // that copies pick data to the pingPong FBO's MRT outputs.
            // The technique shader samples u_pickFeatureId and u_pickDepthAndOrder
            // and writes to FragColor0 and FragColor1.
            auto* technique = m_techniques.getTechnique(TechniqueId::CopyPickBuffers);
            if (technique) {
                auto* shader = technique->getShader({});
                if (shader) {
                    // Bind pingPong FBO as render target.
                    driver.beginRenderPass(pingPong, {});

                    // Set sampler uniforms.
                    ShaderProgramParams params;
                    params.setInt("u_pickFeatureId", 0);
                    params.setInt("u_pickDepthAndOrder", 1);
                    activateProgram(shader, driver, params);

                    // Draw fullscreen quad.
                    driver.drawArrays(0, 3, 1);

                    driver.endRenderPass();
                }
            }
        }
    }

    // Mark that subsequent readPixels should read from the pingPong FBO.
    m_readPickDataFromPingPong = true;
}

// ---------------------------------------------------------------------------
// activateProgram / deactivateProgram — compositor 内就地实现参考 executor 的
// 程序生命周期契约。
// Ported from: itwinjs-core ShaderProgramExecutor.changeProgram (ShaderProgram.ts:724-739)：
//   if (this._program === program) return true;        // 同一程序：不再 use()
//   else if (this._program) this._program.endUse();    // 切换：先 endUse 旧程序
//   this._program = program;
//   if (program && !program.use(this.params)) { this._program = undefined; return false; }
// ---------------------------------------------------------------------------
void SceneCompositor::activateProgram(ShaderProgram* shader, rhi::Driver& driver,
                                      ShaderProgramParams const& params)
{
    if (m_activeProgram == shader) {
        // 同程序（参考 changeProgram 的 return true 分支）：GL 绑定未变，不重复
        // use()；但 DanQing 的 per-primitive 名值 uniform（u_mvp/u_mv 等 params
        // 映射）在 use() 内上传（参考中它们走 draw() 的 GraphicUniform 绑定），
        // 这里必须刷新，否则第二个图元沿用第一个的矩阵。
        if (shader)
            shader->uploadUniforms(driver, params);
        return;
    }
    if (m_activeProgram)
        m_activeProgram->endUse(driver);
    m_activeProgram = (shader && shader->use(driver, params)) ? shader : nullptr;
    // 换程序分支同样要上传名值 params——use() 只绑程序+ProgramUniforms，不触
    // params 名值映射（u_batchId/u_renderPass 等）。此前只有同程序分支刷新，
    // 程序切换后的首绘 u_batchId 从未上传 → pick 输出 uint(0+0.5)=0（拾取
    // saga：深度全写而 R32UI 全零的直接根因）。
    if (m_activeProgram)
        m_activeProgram->uploadUniforms(driver, params);
    // TEMP-DIAG（U 翻转 saga，env 门控）：当前程序的 a_texCoord 实际 location
    // + GL 属性 3 的指针状态——最终接线核对。
    if (m_activeProgram && getenv("DANQING_ATTR_TRACE")) {
        static int n = 0;
        if (n++ < 64) {
            GLint prog = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
            GLint loc = -1;
            if (prog) loc = dqglGetAttribLocation(static_cast<GLuint>(prog), "a_texCoord");
            GLint aStride = 0, aSize = 0, aEnabled = 0;
            void* aPtr = nullptr;
            dqglGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &aStride);
            dqglGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_SIZE, &aSize);
            dqglGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &aEnabled);
            dqglGetVertexAttribPointerv(3, GL_VERTEX_ATTRIB_ARRAY_POINTER, &aPtr);
            printf("[ATTR] shader=%p prog=%u a_texCoord@%d a_pos@%d a_feat@%d | loc3: enabled=%d size=%d stride=%d offset=%ld\n",
                   static_cast<void*>(shader), prog, loc,
                   prog ? dqglGetAttribLocation(static_cast<GLuint>(prog), "a_position") : -1,
                   prog ? dqglGetAttribLocation(static_cast<GLuint>(prog), "a_featureId") : -1,
                   aEnabled, aSize, aStride,
                   static_cast<long>(reinterpret_cast<intptr_t>(aPtr)));
        }
    }
}

void SceneCompositor::deactivateProgram(rhi::Driver& driver)
{
    if (m_activeProgram)
        m_activeProgram->endUse(driver);
    m_activeProgram = nullptr;
}

// ---------------------------------------------------------------------------
// renderLayers — render layer passes
// Ported from: itwinjs-core Compositor.renderLayers()
//
// Layer passes use a special render state:
//   depthTest=true, depthFunc=Always, blendFunc=(ONE, ONE_MINUS_SRC_ALPHA)
// OpaqueLayers: depthMask=true
// TranslucentLayers: depthMask=false
// OverlayLayers: depthMask=true
// ---------------------------------------------------------------------------
void SceneCompositor::renderLayers(RenderCommands& commands, bool /*needComposite*/, RenderPass pass)
{
    auto const& cmds = commands.getCommands(pass);
    if (cmds.empty()) return;

    auto& driver = m_target.getDriver();
    auto rect = m_target.getViewRect();

    rhi::RenderPassParams params;
    params.viewport = {static_cast<int32_t>(rect.left), static_cast<int32_t>(rect.top),
                       rect.width(), rect.height()};
    driver.beginRenderPass(m_target.getRenderTarget(), params);

    applyRenderState(pass);
    drawPass(commands, pass);

    driver.endRenderPass();
}

// ---------------------------------------------------------------------------
// renderPointClouds — render point cloud geometry
// Ported from: itwinjs-core Compositor.renderPointClouds()
//
// point clouds use depthTest=true render state.
// ---------------------------------------------------------------------------
void SceneCompositor::renderPointClouds(RenderCommands& commands, CompositeFlags /*compositeFlags*/)
{
    auto const& cmds = commands.getCommands(RenderPass::PointClouds);
    if (cmds.empty()) return;

    auto& driver = m_target.getDriver();
    auto rect = m_target.getViewRect();

    rhi::RenderPassParams params;
    params.viewport = {static_cast<int32_t>(rect.left), static_cast<int32_t>(rect.top),
                       rect.width(), rect.height()};
    driver.beginRenderPass(m_target.getRenderTarget(), params);

    applyRenderState(RenderPass::PointClouds);
    drawPass(commands, RenderPass::PointClouds);

    driver.endRenderPass();
}

// ---------------------------------------------------------------------------
// renderVolumeClassification — render volume classification
// Ported from: itwinjs-core Compositor.renderVolumeClassification()
//
// Volume classification uses stencil-based rendering with multiple passes.
// Phase 1: simplified implementation that draws classification commands.
// Phase 2: full stencil-based volume classification pipeline.
// ---------------------------------------------------------------------------
void SceneCompositor::renderVolumeClassification(RenderCommands& commands,
                                                  CompositeFlags /*compositeFlags*/,
                                                  bool /*renderForReadPixels*/)
{
    auto const& cmds = commands.getCommands(RenderPass::Classification);
    if (cmds.empty()) return;

    auto& driver = m_target.getDriver();
    auto rect = m_target.getViewRect();

    rhi::RenderPassParams params;
    params.viewport = {static_cast<int32_t>(rect.left), static_cast<int32_t>(rect.top),
                       rect.width(), rect.height()};
    driver.beginRenderPass(m_target.getRenderTarget(), params);

    applyRenderState(RenderPass::Classification);
    drawPass(commands, RenderPass::Classification);

    driver.endRenderPass();
}

// ---------------------------------------------------------------------------
// drawPass — draw commands for a specific render pass
// Ported from: itwinjs-core Compositor.drawPass() (lines 2336-2346)
//
// itwinjs-core implementation (10 lines):
//   const cmds = commands.getCommands(pass);
//   if (0 === cmds.length) return;
//   else if (pingPong) this.pingPong();
//   System.instance.applyRenderState(this.getRenderState(pass));
//   this.target.techniques.execute(this.target, cmds, pass);
//
// DanQing implementation: inlines technique execution because the full
// techniques.execute() pipeline is not yet implemented. The inline
// approach produces equivalent rendering output.
// ---------------------------------------------------------------------------
void SceneCompositor::drawPass(RenderCommands& commands, RenderPass pass,
                               bool pingPong, RenderPass cmdPass)
{
    auto& driver = m_target.getDriver();
    auto& cmds = commands.getCommands(
        RenderPass::None != cmdPass ? cmdPass : pass);

    if (cmds.empty()) return;

    // Sort primitive-runs by render order (surfaces before lines/points). This
    // mirrors itwinjs's GraphicBuilder default (preserveOrder=false, see
    // GraphicBuilder.ts:35-37): surfaces are drawn before lines so outlines/
    // labels/points render "in front of" fills — giving dark, high-contrast
    // label glyphs over the faint fill and a visible Z-axis tip point atop the
    // disc. Non-primitive (state) commands stay in place; each maximal run of
    // consecutive PrimitiveCommands between them is sorted independently, so
    // PushState/PopState scoping is preserved. RenderOrder ascending:
    // BlankingRegion(2) < UnlitSurface(3) < LitSurface(4) < Linear(5) < Edge(6).
    for (size_t i = 0, n = cmds.size(); i < n; ) {
        while (i < n && cmds[i]->getType() != DrawCommandType::Primitive) ++i;
        size_t const runStart = i;
        while (i < n && cmds[i]->getType() == DrawCommandType::Primitive) ++i;
        if (i > runStart + 1) {
            std::stable_sort(cmds.begin() + static_cast<long>(runStart),
                             cmds.begin() + static_cast<long>(i),
                             [](std::unique_ptr<DrawCommand> const& a,
                                std::unique_ptr<DrawCommand> const& b) {
                                 auto* ga = static_cast<PrimitiveCommand const*>(a.get())->getGeometry();
                                 auto* gb = static_cast<PrimitiveCommand const*>(b.get())->getGeometry();
                                 int const oa = ga ? static_cast<int>(ga->getRenderOrder()) : 0;
                                 int const ob = gb ? static_cast<int>(gb->getRenderOrder()) : 0;
                                 return oa < ob;
                             });
        }
    }

    // Desktop GL ignores `gl_PointSize` written in the vertex shader unless
    // GL_PROGRAM_POINT_SIZE is enabled (WebGL always honors it, so itwinjs needs
    // no enable). Without this the PointString shader's `gl_PointSize = u_pointSize`
    // is ignored and every point renders at 1px — the ACS Z-axis tip (weight 6)
    // was effectively invisible. Enable once, globally (only gl_PointSize outputs
    // are affected; point clouds write 1.0). Ported intent: itwinjs PointString.ts.
    [[maybe_unused]] static bool const s_programPointSizeEnabled = [] {
        glEnable(GL_PROGRAM_POINT_SIZE);
        return true;
    }();

    // If pingPong requested, resolve pick data before drawing.
    // Ported from: itwinjs-core Compositor.drawPass() pingPong parameter
    if (pingPong) {
        this->pingPong();
    }

    // Apply render state for this pass
    applyRenderState(pass);

    for (auto const& cmd : cmds) {
        if (!cmd) continue;

        switch (cmd->getType()) {
            case DrawCommandType::Primitive: {
                auto* primCmd = static_cast<PrimitiveCommand*>(cmd.get());
                auto* geometry = primCmd->getGeometry();
                if (!geometry) break;

                auto techniqueId = geometry->getTechniqueId();
                TechniqueFlags flags = m_branchStack.getCurrentTechniqueFlags();
                // Position encoding is per-geometry, not per-view: select the quantized or
                // unquantized Surface/Edge variant from the actual cached geometry. The branch
                // stack leaves positionType at its default (Quantized); without this the cube
                // (raw FLOAT3 a_position, no VertexLUT) would pick the Quantized variant and
                // produce zero-coverage fragments. Ported from: itwinjs-core DrawCommand.ts:221-223
                // (posType = cachedGeometry.usesQuantizedPositions ? quantized : unquantized).
                flags.positionType = geometry->usesQuantizedPositions()
                    ? PositionType::Quantized
                    : PositionType::Unquantized;

                // isTranslucent from render pass
                if (pass == RenderPass::Translucent || pass == RenderPass::TranslucentLayers) {
                    flags.isTranslucent = true;
                }

                // Determine feature mode (pick during readPixels, else overrides
                // when the active batch carries a feature-override LUT).
                // Ported from: itwinjs-core DrawCommand.ts PrimitiveCommand.execute()
                //   — featureMode = target.isReadPixelsInProgress ? Pick : ...
                //   (drawForPick 期间的 pick 变体输出 `out uint fragColor`；
                //    此前 drawPass 不消费该标志 → Pick 变体从未被请求 →
                //    pick buffer 里画的是普通颜色而非 featureId)
                if (m_target.isReadPixelsInProgress()) {
                    flags.featureMode = FeatureMode::Pick;
                } else {
                    auto* currentBatch = m_batchState.getCurrentBatch();
                    if (currentBatch && currentBatch->hasFeatureOverrides()) {
                        flags.featureMode = FeatureMode::Overrides;
                    }
                }

                // Only switch shaders if technique or flags changed
                ShaderProgram* shader = nullptr;
                if (techniqueId == m_cachedTechniqueId &&
                    flags.featureMode == m_cachedTechniqueFlags.featureMode &&
                    flags.isClassified == m_cachedTechniqueFlags.isClassified &&
                    flags.isThematic == m_cachedTechniqueFlags.isThematic &&
                    m_cachedShader) {
                    shader = m_cachedShader;
                } else {
                    auto* technique = m_techniques.getTechnique(techniqueId);
                    if (technique) {
                        shader = technique->getShader(flags);
                        m_cachedTechniqueId = techniqueId;
                        m_cachedTechniqueFlags = flags;
                        m_cachedShader = shader;
                    }
                }

                if (shader) {
                    ShaderProgramParams params = m_frameParams;
                    // TEMP-DIAG（拾取 saga，env 门控）：本图元的变体选择证据。
                    // readPixels 图元 + 有 batch 的普通帧图元（高亮链核对）。
                    if (getenv("DANQING_PICK_TRACE")
                        && (m_target.isReadPixelsInProgress() || m_batchState.getCurrentBatch())) {
                        static int n = 0;
                        if (n++ < 8) {
                            auto const& pmvp = m_branchStack.getCurrentMvp();
                            printf("[PICK] draw prim: shader=%p pass=%d tech=%d featureMode=%d posType=%d batch=%u stackDepth=%d mvp22=%.3f\n",
                                   static_cast<void*>(shader),
                                   static_cast<int>(pass), static_cast<int>(techniqueId),
                                   static_cast<int>(flags.featureMode),
                                   static_cast<int>(flags.positionType),
                                   m_batchState.getCurrentBatchId(),
                                   static_cast<int>(m_branchStack.getDepth()),
                                   pmvp[10]);
                        }
                    }

                    // Set per-primitive uniforms
                    auto const& mvp = m_branchStack.getCurrentMvp();
                    params.setMatrix4("u_mvp", mvp.data());
                    params.setMatrix4("u_mv", m_branchStack.getCurrentMv().data());
                    // TEMP-DIAG（U 翻转 saga，env 门控）：u_mvp 的 16 元素 + 行列式。
                    if (getenv("DANQING_MVP_TRACE")) {
                        static int n = 0;
                        if (n++ < 8) {
                            double a[4][4];
                            for (int r = 0; r < 4; ++r)
                                for (int c = 0; c < 4; ++c)
                                    a[r][c] = mvp[r + c * 4];  // column-major storage
                            double det =
                                a[0][3]*a[1][2]*a[2][1]*a[3][0] - a[0][2]*a[1][3]*a[2][1]*a[3][0] -
                                a[0][3]*a[1][1]*a[2][2]*a[3][0] + a[0][1]*a[1][3]*a[2][2]*a[3][0] +
                                a[0][2]*a[1][1]*a[2][3]*a[3][0] - a[0][1]*a[1][2]*a[2][3]*a[3][0] -
                                a[0][3]*a[1][2]*a[2][0]*a[3][1] + a[0][2]*a[1][3]*a[2][0]*a[3][1] +
                                a[0][3]*a[1][0]*a[2][2]*a[3][1] - a[0][0]*a[1][3]*a[2][2]*a[3][1] -
                                a[0][2]*a[1][0]*a[2][3]*a[3][1] + a[0][0]*a[1][2]*a[2][3]*a[3][1] +
                                a[0][3]*a[1][1]*a[2][0]*a[3][2] - a[0][1]*a[1][3]*a[2][0]*a[3][2] -
                                a[0][3]*a[1][0]*a[2][1]*a[3][2] + a[0][0]*a[1][3]*a[2][1]*a[3][2] +
                                a[0][1]*a[1][0]*a[2][3]*a[3][2] - a[0][0]*a[1][1]*a[2][3]*a[3][2] +
                                a[0][2]*a[1][1]*a[2][0]*a[3][3] - a[0][1]*a[1][2]*a[2][0]*a[3][3] -
                                a[0][2]*a[1][0]*a[2][1]*a[3][3] + a[0][0]*a[1][2]*a[2][1]*a[3][3] +
                                a[0][1]*a[1][0]*a[2][2]*a[3][3] - a[0][0]*a[1][1]*a[2][2]*a[3][3];
                            printf("[MVP] col-major:");
                            for (int i = 0; i < 16; ++i) printf(" %.3f", mvp[i]);
                            printf("  det=%.3f\n", det);
                        }
                    }

                    // Upload the active render pass so fragment shaders can branch on it.
                    // Ported from: itwinjs-core TargetUniforms → ShaderProgram u_renderPass
                    // binding (addRenderPass registers it; this supplies the value per pass).
                    // Critical for Surface assignFragData: opaque passes (OpaqueLinear=2 ..
                    // OpaqueGeneral=5) force alpha=1, while overlay/translucent passes
                    // (WorldOverlay=12, etc.) premultiply — without this upload u_renderPass
                    // is whatever the program default is, so translucent overlay fills (ACS
                    // triad, alpha ~0.22) render opaque instead of faint.
                    params.setFloat("u_renderPass", static_cast<float>(pass));

                    // Set batch ID. The Pick variant's vertex shader adds it to the
                    // per-vertex feature index to form the global pick value
                    // (batchId + featureIndex), which readPixels translates back
                    // to an element id via BatchState.
                    // Ported from: itwinjs-core BatchUniforms.bindBatchId
                    //               (FeatureSymbology.ts:506-512, 514
                    //                v_feature_id = addUInt32s(u_batch_id, featureIndex))
                    {
                        uint32_t const batchId = m_batchState.getCurrentBatchId();
                        params.setFloat("u_batchId", static_cast<float>(batchId));
                    }

                    // Set feature override uniforms
                    // Ported from: itwinjs-core FeatureOverrides bindLUTParams /
                    //               bindLUT (FeatureOverrides.ts:443-450)
                    auto* currentBatch = m_batchState.getCurrentBatch();
                    if (flags.featureMode == FeatureMode::Overrides && currentBatch && currentBatch->hasFeatureOverrides()) {
                        auto& lut = currentBatch->getOrCreateFeatureOverrideLUT();
                        float width = static_cast<float>(lut.getWidth());
                        params.setFloat("u_featureOverrideWidth", width > 0.0f ? 1.0f / width : 0.0f);
                        params.setInt("u_featureOverrides", 7);
                        // u_hiliteColor feeds the override shader's Hilited mix
                        // (kOvrBit_Hilited → mix(baseColor, u_hiliteColor, ratio)，
                        //  ratio = 参考默认 visibleRatio 0.25，Hilite.ts:53).
                        params.setVec4("u_hiliteColor", m_hiliteColor);
                        // Flash uniforms（doApplyFlash，glsl/FeatureSymbology.ts:698-706）：
                        // intensity 逐帧（processFlash 爬升）；mode 默认 Brighten=1
                        // （FlashSettings.ts:15-20）。
                        params.setFloat("u_flashIntensity", m_target.getFlashIntensity());
                        params.setFloat("u_flashMode", 1.0f);
                    }

                    // Set thematic uniforms when thematic display is active.
                    // Ported from: itwinjs-core ThematicUniforms binding in draw pass
                    if (flags.isThematic) {
                        // Bind gradient texture to texture unit 1 for thematic color lookup.
                        // Ported from: itwinjs-core ThematicUniforms.bindGradientTexture()
                        m_thematicUniforms.bindGradientTexture(driver, 1);
                        params.setInt("u_thematicEnabled", 1);
                    }

                    // Set contour uniforms for contour line rendering.
                    // Ported from: itwinjs-core Contours.ts uniform binding
                    // The u_contourDefs array is bound via ContourUniforms::bindcontourDefs().
                    // The u_contourLUT and u_contourLUTWidth are bound via BatchUniforms.
                    // The model-to-world transform is used for height computation.
                    params.setMatrix4("u_modelToWorldC", m_branchStack.getCurrentMv().data());

                    // Set technique-specific uniforms
                    if (techniqueId == TechniqueId::Surface) {
                        // u_materialColor = vec4(-1) = itwinjs "no material" sentinel.
                        // decodeMaterialColor: mat_rgb.a = float(rgba.r >= 0) = 0 when r<0 →
                        // applyMaterialColor returns vertex color (a_color) unchanged.
                        // (1,1,1,1) made mat_rgb.a=1 → mix(vcolor, white, 1)=white → invisible.
                        float materialColor[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
                        params.setVec4("u_materialColor", materialColor);
                        // u_frustum（近/远/类型）——参考经 Common.addFrustum 的
                        // ProgramUniform 绑定在 use() 时上传；TD-15 登记：ProgramUniform
                        // 只写缓存不派发 GL，此经 params 名值映射 legacy 上传补齐
                        // （pick 变体的 computeLinearDepth 与透视光照的 kFrustumType
                        // 分支同靠它；真派发落地时随 TD-15 拆除）。
                        params.setVec3("u_frustum", m_target.getFrustumUniforms().getFrustumData());
                    } else if (techniqueId == TechniqueId::PlanarGrid) {
                        params.setMatrix4("u_mvpMatrix", m_branchStack.getCurrentMvp().data());
                    } else if (techniqueId == TechniqueId::Edge ||
                               techniqueId == TechniqueId::SilhouetteEdge) {
                        float viewport[2] = {
                            static_cast<float>(m_target.getViewRect().width()),
                            static_cast<float>(m_target.getViewRect().height())
                        };
                        params.setVec2("u_viewport", viewport);
                        params.setFloat("u_lineWeight", 1.0f);
                        params.setFloat("u_bgIntensity", 1.0f);
                    } else if (techniqueId == TechniqueId::SkySphereGradient) {
                        // Faithful itwinjs SkySphere gradient uniforms. The skybox
                        // graphic carries the colors + the per-frame worldPos
                        // (a_worldPos, uploaded in the geometry's draw) + worldEye.
                        // ← itwinjs-core SkySphere.ts addGradientUniforms + u_worldEye.
                        auto const* sky = static_cast<SkySphereViewportQuadGeometry const*>(geometry);
                        auto const& skyCol = sky->getColors();
                        params.setVec3("u_worldEye", sky->getWorldEye());
                        params.setVec3("u_skyParams", sky->getTypeAndExponents().data());
                        params.setFloat("u_zOffset", sky->getZOffset());
                        params.setVec3("u_zenithColor", skyCol.zenith.data());
                        params.setVec3("u_nadirColor", skyCol.nadir.data());
                        params.setVec3("u_skyColor", skyCol.sky.data());
                        params.setVec3("u_groundColor", skyCol.ground.data());
                        // Identity u_mvp so the NDC fullscreen quad covers the screen;
                        // the shader pushes it to the far plane via gl_Position=(u_mvp*pos).xyww.
                        static float const kIdentityMvp[16] = {
                            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
                        params.setMatrix4("u_mvp", kIdentityMvp);
                    } else if (techniqueId == TechniqueId::PointString) {
                        // gl_PointSize = lineWeight (itwinjs-core PointString.ts:22),
                        // sourced from _getLineWeight() = this.weight (PointString.ts:62).
                        // Upload the geometry's symbology weight so point strings render
                        // at their true size — e.g. the ACS Z-axis tip (weight 6) draws as
                        // a 6px round dot. Without this u_pointSize is the program default
                        // (0) and the point is invisible. Ported from: itwinjs-core
                        // ShaderProgramExecutor line-weight binding for PointString.
                        params.setFloat("u_pointSize", geometry->getLineWeight());
                    } else if (techniqueId == TechniqueId::Polyline) {
                        // Faithful thick-line dispatch (ported from: itwinjs-core
                        // ShaderProgramExecutor Polyline branch + Polyline.ts
                        // _getLineWeight/getColor + VertexLUT texture binding).
                        // Per-draw uniforms:
                        //   u_lineWeight  = geometry->getLineWeight() (clamp 1..31,
                        //                   CachedGeometry.ts:140-150) — drives the
                        //                   miter/displacement math in the Polyline shader;
                        //   u_vertLUT     = sampler2D bound to texture unit 2 (units 0/1
                        //                   = pick textures, 7 = featureOverrides/hilite,
                        //                   1 = thematic gradient);
                        //   u_vertParams  = (width, height, numRgbaPerVertex, numVertices)
                        //                   from VertexLutTexture::getParams();
                        //   u_color       = geometry uniform color (DisplayParams.lineColor)
                        //                   — Polyline.ts:129-131 uniform-color path;
                        //   u_viewport    = (w,h) — required by polylineAddLineCode
                        //                   (PolylineShaderBuilder.cpp:154-155) to convert
                        //                   pixel-space displacement back to clip space;
                        //                   mirrors the Edge branch (SceneCompositor:1038-1042);
                        //   u_bgIntensity = 1.0, u_reverseWhiteOnWhite = false (white-on-white
                        //                   reversal default — matches Edge dispatch).
                        // NOTE: dashing uniforms (u_lineCode, u_useCumDist,
                        //   u_pixelsPerWorld, u_numLineCodes, u_aaSamples) are
                        //   deliberately NOT set here — they default to 0, which
                        //   short-circuits the dash pipeline for solid ACS lines
                        //   (lineCode == 0). Remains correct until plan §8 dashing.
                        auto const* pg = static_cast<PolylineGeometry const*>(geometry);
                        auto const& lut = pg->getLut();
                        params.setFloat("u_lineWeight", pg->getLineWeight());
                        // LUT bound to texture unit 0 (matches the headless
                        // PolylineRenderTest binding, which is the canonical working
                        // configuration).
                        constexpr int32_t kPolylineLutTexUnit = 0;
                        if (lut.isValid()) {
                            driver.bindTexture(kPolylineLutTexUnit, lut.getTexture());
                            params.setInt("u_vertLUT", kPolylineLutTexUnit);
                            auto const& lp = lut.getParams();
                            float vertParams[4] = {
                                static_cast<float>(lp.texWidth),
                                static_cast<float>(lp.texHeight),
                                static_cast<float>(lp.numRgbaPerVert),
                                static_cast<float>(lp.numVertices),
                            };
                            params.setVec4("u_vertParams", vertParams);
                        }
                        dqCommon::ColorDef const color = pg->getColor();
                        dqCommon::ColorComponents const cc = color.getColors();
                        float lineColor[4] = {
                            static_cast<float>(cc.r) / 255.0f,
                            static_cast<float>(cc.g) / 255.0f,
                            static_cast<float>(cc.b) / 255.0f,
                            static_cast<float>(255 - cc.t) / 255.0f,
                        };
                        params.setVec4("u_color", lineColor);
                        float viewport[2] = {
                            static_cast<float>(m_target.getViewRect().width()),
                            static_cast<float>(m_target.getViewRect().height())
                        };
                        params.setVec2("u_viewport", viewport);
                        // modelToWindowCoordinates (PolylineShaders.h) uses u_proj,
                        // u_viewportTransformation, and u_frustum. Their ProgramUniform
                        // bindings (wireProjectionMatrix / wireViewportTransformation /
                        // addFrustum) are registered on the Polyline program but NOT
                        // invoked on this draw path — uploadUniforms uploads the params
                        // name->value map (u_viewport above works that way). Set them
                        // explicitly from the target's frustum + viewRect. Without this,
                        // u_proj = 0 → q = u_proj·q = 0 → gl_Position = 0 → the Polyline
                        // (ACS outlines + X/Y labels) emits 0 fragments while Surface
                        // fills (u_mvp) render. (The headless PolylineRenderTest masked
                        // this by setting u_proj/u_viewportTransformation manually.)
                        params.setMatrix4("u_proj",
                            m_target.getFrustumUniforms().getProjectionMatrix32().data);
                        params.setMatrix4("u_viewportTransformation",
                            m_target.getUniforms().viewRect.getViewportMatrix().data);
                        float const polylineFrustum[3] = {
                            m_target.getFrustumUniforms().getNearPlane(),
                            m_target.getFrustumUniforms().getFarPlane(),
                            static_cast<float>(static_cast<int>(
                                m_target.getFrustumUniforms().getType())),
                        };
                        params.setVec3("u_frustum", polylineFrustum);
                        // u_bgIntensity < 0 → adjustContrast returns baseColor unchanged
                        // (Edge.ts adjustContrast: `if (bgi < 0.0) return baseColor;`). With
                        // bgi >= 0 the axis colors desaturate to gray (R/G/B → gray →
                        // invisible). Pass-through so ACS strokes keep their true color.
                        params.setFloat("u_bgIntensity", -1.0f);
                        int const reverseWow = 0;
                        params.setInt("u_reverseWhiteOnWhite", reverseWow);
                    }

                    activateProgram(shader, driver, params);

                    // Invoke GraphicUniform bindings (u_mv, u_materialColor, ...).
                    // Ported from: itwinjs-core ShaderProgramExecutor.draw()
                    // The bindings read from DrawParams; uploadUniforms (below)
                    // overrides with the legacy map values during the transition.
                    DrawParams drawParams;
                    drawParams.setTarget(&m_target);

                    // Update BranchUniforms for this geometry (handles instanced/VIO/viewCoords).
                    // Ported from: itwinjs-core BranchUniforms.update() (line 181-257)
                    // Skip recomputation if BranchUniforms hasn't changed since last sync.
                    if (!m_branchObserver.isSynchronized(m_branchUniforms)) {
                        BranchUniforms::UpdateParams branchParams;
                        branchParams.instancedGeom = geometry->asInstanced();
                        branchParams.viewIndependentOrigin = geometry->viewIndependentOrigin();
                        branchParams.isViewCoords = false;  // viewCoords not yet wired
                        m_branchUniforms.update(
                            m_branchStack.getTop().getLocalToWorld(),
                            m_target.getViewMatrix(),
                            m_target.getFrustumUniforms().getProjectionMatrix(),
                            branchParams);
                        m_branchObserver.sync(m_branchUniforms);
                    }

                    // DrawParams mv/mvp feed the GraphicUniform bindings (u_mv, u_mvp).
                    // Use the BranchStack's precomputed mv/mvp — NOT BranchUniforms.
                    // BranchUniforms.update recomputes mv = FrustumUniforms.view * model, but
                    // FrustumUniforms.m_view is never populated on this camera path
                    // (changeFrustum has no caller; only changeProjectionMatrix runs in
                    // OpenGLRenderTarget::setViewportTransform), so it stays identity and
                    // BranchUniforms produces mv = identity * model (missing the camera
                    // recentering translation). The Polyline's modelToWindowCoordinates does
                    // u_proj * u_mv, with u_proj derived in setViewportTransform as
                    // mvp * viewMv⁻¹ (viewMv == BranchStack mv). For u_proj * u_mv to equal
                    // mvp (correct clip coords), u_mv MUST be the same view mv (BranchStack),
                    // not the identity recomputed by BranchUniforms — otherwise every vertex
                    // projects to the (-1,-1) corner and the Polyline (ACS outlines + labels)
                    // produces zero fragments while Surface fills (u_mvp) render correctly.
                    drawParams.setModelViewMatrix(m_branchStack.getCurrentMv().data());
                    drawParams.setModelViewProjectionMatrix(m_branchStack.getCurrentMvp().data());
                    float defaultMaterial[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                    drawParams.setMaterialColor(defaultMaterial);
                    // Material params (u_materialParams): read from geometry's material info,
                    // or use default material if none specified.
                    // Ported from: itwinjs-core Surface.ts computeMaterialParams (line 214-218)
                    auto const* matInfo = geometry->getMaterialInfo();
                    if (matInfo && !matInfo->isAtlas() && !matInfo->ignoresMaterial()) {
                        drawParams.setMaterialParams(matInfo->getFragUniforms());
                        drawParams.setMaterialColor(matInfo->getRgba());
                    } else {
                        auto const& defaultMat = RenderMaterialInternal::defaultMaterial();
                        drawParams.setMaterialParams(defaultMat.getFragUniforms());
                    }
                    // Surface flags: read from the current geometry (u_surfaceFlags[12]).
                    // Ported from: itwinjs-core SurfaceGeometry.computeSurfaceFlags() +
                    //              Surface.ts addSurfaceFlags graphic uniform
                    //              (per-draw setUniform1iv — Surface.ts:519-526).
                    // Dispatched by the wireSurfaceFlags GraphicUniform binding at
                    // shader->draw() below (reads dp.getSurfaceFlags(); handle
                    // location resolved at compile — TD-15). The legacy name-value
                    // upload of the same array was removed. Without the dispatch the
                    // array stays at the GL default (all false): HasTexture=0 makes
                    // sampleSurfaceTexture() return white and ApplyLighting=0 skips
                    // lighting — textured surfaces render as flat vertex color (glTF
                    // texture-on-screen regression, GltfTexturePixelTest).
                    int const* surfaceFlags = SurfaceGeometry::computeSurfaceFlags(
                        *geometry, m_target.displayNormalMaps,
                        m_target.getCurrentViewFlags());
                    drawParams.setSurfaceFlags(surfaceFlags);
                    // Normal matrix: transpose(inverse(mat3(mv))) for u_normalMatrix.
                    // Ported from: itwinjs-core Vertex.ts addNormalMatrix() —
                    // dispatched by the wireNormalMatrix GraphicUniform binding at
                    // shader->draw() (reads dp.getNormalMatrix(); legacy name-value
                    // upload removed — TD-15). A default-zero u_normalMatrix makes
                    // normalize(MAT_NORM * a_normal) NaN and every lit surface
                    // renders black.
                    float normalMatrix[9];
                    computeNormalMatrixFromMv(m_branchStack.getCurrentMv().data(), normalMatrix);
                    drawParams.setNormalMatrix(normalMatrix);
                    // Surface texture (s_texture): bind if the surface geometry has one.
                    // Ported from: itwinjs-core Surface.ts addTexture() (line 596-609) —
                    // routed via CachedGeometry::getSurfaceTexture() so both SurfaceGeometry
                    // and PolyfaceGraphic (glTF) paths reach the sampler.
                    rhi::TextureHandle const surfTex = geometry->getSurfaceTexture();
                    if (surfTex != rhi::TextureHandle{}) {
                        driver.bindTexture(0, surfTex);
                        params.setInt("s_texture", 0);
                        // TEMP-DIAG（U 翻转 saga）：绘制时纹理实际 wrap 状态。
                        if (getenv("DANQING_WRAP_TRACE")) {
                            static int n = 0;
                            if (n++ < 8) {
                                GLint ws = 0, wt = 0;
                                dqglGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &ws);
                                dqglGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &wt);
                                printf("[WRAP] wrapS=0x%04x wrapT=0x%04x (REPEAT=0x2901 MIRRORED=0x8370 CLAMP=0x812F)\n", ws, wt);
                            }
                        }
                    }
                    // Normal map (s_normalMap): bind when the HasNormalMap bit is
                    // set (wantNormalMaps passed inside computeSurfaceFlags).
                    // Ported from: itwinjs-core Surface.ts addNormal() (:612-620 —
                    // surfGeom.normalMap bound to TextureUnit.NormalMap when
                    // useNormalMap) + u_normalMapScale (:541-550 — uploaded when
                    // normalMapParams present; the value was greenUp-negated at
                    // bind time by the caller, glTF → -1.0). Texture unit 13 =
                    // TextureUnit.NormalMap (RenderFlags.ts :189); unit 0 stays
                    // reserved for s_texture above.
                    rhi::TextureHandle const normalTex = geometry->getNormalMapTexture();
                    if (getenv("DANQING_NM_TRACE") &&
                        (normalTex != rhi::TextureHandle{} || surfTex != rhi::TextureHandle{})) {
                        printf("[NMDIAG] draw surfTex=%d normalTex=%d hasNormalMapBit=%d "
                               "displayNormalMaps=%d renderMode=%d textures=%d applyLighting=%d\n",
                               surfTex != rhi::TextureHandle{} ? 1 : 0,
                               normalTex != rhi::TextureHandle{} ? 1 : 0,
                               surfaceFlags[static_cast<int>(GL::SurfaceBitIndex::HasNormalMap)],
                               m_target.displayNormalMaps ? 1 : 0,
                               static_cast<int>(m_target.getCurrentViewFlags().renderMode),
                               m_target.getCurrentViewFlags().textures ? 1 : 0,
                               surfaceFlags[static_cast<int>(GL::SurfaceBitIndex::ApplyLighting)]);
                    }
                    if (normalTex != rhi::TextureHandle{} &&
                        surfaceFlags[static_cast<int>(GL::SurfaceBitIndex::HasNormalMap)] != 0) {
                        driver.bindTexture(13, normalTex);
                        params.setInt("s_normalMap", 13);
                        params.setFloat("u_normalMapScale", geometry->getNormalMapScale());
                    }
                    // Procedural-grid uniforms (u_gridColor/u_gridProps/u_qTexCoordParams):
                    // uploaded per-graphic from the PlanarGridGraphic's props (mirrors
                    // itwinjs-core addGraphicUniform in glsl/PlanarGrid.ts:78-97). Without
                    // these the shader reads u_gridProps as 0 -> alpha 0 -> grid invisible.
                    if (auto* grid = geometry->asPlanarGrid()) {
                        auto const gridColor = grid->getGridColorRgb();
                        auto const gridProps = grid->getGridProps();
                        auto const texCoordParams = grid->getQTexCoordParams();
                        params.setVec3("u_gridColor", gridColor.data());
                        params.setVec4("u_gridProps", gridProps.data());
                        params.setVec4("u_qTexCoordParams", texCoordParams.data());
                        // Translucency.ts addFrustum — OIT 权重的 near/far
                        //（PlanarGridShaders 的 u_frustum vec3；手写 shader 不走
                        //   CommonShaders 的 ProgramUniform 绑定，须在此显式上传）。
                        params.setVec3("u_frustum", m_target.getFrustumUniforms().getFrustumData());
                    }
                    shader->draw(drawParams);

                    // Upload per-primitive uniforms (u_mvp/u_mv/u_materialColor/...)
                    // from the legacy name->value map. Transitional: the binding
                    // system (addBindings) covers u_frustum; the remainder migrate
                    // to GraphicUniform/ProgramUniform bindings in a later sweep.
                    // No-op if the program failed to compile (null GL handle).
                    shader->uploadUniforms(driver, params);
                }

                geometry->draw(driver);

                break;
            }
            case DrawCommandType::PushBranch: {
                auto* branchCmd = static_cast<PushBranchCommand*>(cmd.get());
                if (branchCmd->hasViewFlags()) {
                    m_branchStack.push(branchCmd->getMv(), branchCmd->getMvp(),
                                      branchCmd->getViewFlags());
                } else {
                    m_branchStack.pushTransform(branchCmd->getMv(), branchCmd->getMvp());
                }
                break;
            }
            case DrawCommandType::PopBranch:
                m_branchStack.pop();
                break;
            case DrawCommandType::PushBatch: {
                // Ported from: itwinjs-core DrawCommand.ts PushBatchCommand.execute()
                // Look up the batch from BatchState using the batchId.
                auto* batchCmd = static_cast<PushBatchCommand*>(cmd.get());
                uint32_t batchId = batchCmd->getBatchId();
                auto* batch = m_batchState.findBatch(batchId);
                if (batch) {
                    m_batchState.push(*batch);
                    // Lazy feature-state LUT update at draw time — the reference keeps
                    // per-batch observers on the target's hiliteSyncTarget + flashedId
                    // and recomputes the LUT when either changed since the last draw
                    // (FeatureOverrides.update → updateHilite/updateFlashed).
                    // Ported from: itwinjs-core FeatureOverrides.ts:412-441
                    if (batch->getLastHiliteVersion() != m_target.getHiliteVersion()) {
                        batch->updateFeatureStates(m_target.getHiliteElementIds(),
                                                   m_target.getFlashedId());
                        batch->setLastHiliteVersion(m_target.getHiliteVersion());
                        // TEMP-DIAG（拾取 saga，env 门控）
                        if (getenv("DANQING_PICK_TRACE")) {
                            printf("[PICK] PushBatch %u updateFeatureStates: hiliteSetSize=%zu flashed=0x%x lutHilited=%d\n",
                                   batchId, m_target.getHiliteElementIds().size(),
                                   m_target.getFlashedId(),
                                   batch->getFeatureOverrideLUT()
                                       ? (batch->getFeatureOverrideLUT()->anyHilited() ? 1 : 0)
                                       : -1);
                        }
                    }
                    // Create the LUT when the batch has a feature table: the
                    // Overrides variant (drawPass featureMode check reads
                    // hasFeatureOverrides()) samples it. A tableless batch has
                    // no LUT rows — the dead-LUT object would sample garbage.
                    if (batch->getFeatureTable()) {
                        auto& lut = batch->getOrCreateFeatureOverrideLUT();
                        lut.upload(driver);
                        if (lut.getTextureHandle()) {
                            driver.bindTexture(7, lut.getTextureHandle());
                        }
                    }
                }
                break;
            }
            case DrawCommandType::PopBatch:
                m_batchState.pop();
                break;
            case DrawCommandType::PushClip: {
                // Ported from: itwinjs-core DrawCommand.ts PushClipCommand.execute()
                auto* clipCmd = static_cast<PushClipCommand*>(cmd.get());
                auto* clipVol = clipCmd->getClipVolume();
                if (clipVol) {
                    m_clipStack.push(*clipVol);
                }
                break;
            }
            case DrawCommandType::PopClip:
                // Ported from: itwinjs-core DrawCommand.ts PopClipCommand.execute()
                m_clipStack.pop();
                break;
            case DrawCommandType::PushState: {
                // Push a BranchState onto the stack (used for decorations).
                // Ported from: itwinjs-core DrawCommand.ts PushStateCommand.execute()
                auto* stateCmd = static_cast<PushStateCommand*>(cmd.get());
                m_branchStack.pushState(stateCmd->getState());
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// drawForPick — draw for pixel reading (pick rendering)
// Ported from: itwinjs-core Compositor.drawForReadPixels() (lines 1523-1605)
// ---------------------------------------------------------------------------
void SceneCompositor::drawForPick(RenderCommands& commands)
{
    auto& driver = m_target.getDriver();
    auto pickTarget = m_target.getPickTarget();
    if (!pickTarget) return;

    auto rect = m_target.getViewRect();

    // Clear pick buffer to 0 (no geometry)
    rhi::RenderPassParams params;
    params.flags.clear = rhi::TargetBufferFlags::COLOR_ALL | rhi::TargetBufferFlags::DEPTH;
    params.clearColor.f[0] = 0.0f;
    params.clearColor.f[1] = 0.0f;
    params.clearColor.f[2] = 0.0f;
    params.clearColor.f[3] = 0.0f;
    params.clearDepth = 1.0;
    params.viewport = {static_cast<int32_t>(rect.left), static_cast<int32_t>(rect.top),
                       rect.width(), rect.height()};

    driver.beginRenderPass(pickTarget, params);

    // TEMP-DIAG（拾取 saga，env 门控）：进入 pick pass 的命令数（按 pass 桶）。
    if (getenv("DANQING_PICK_TRACE")) {
        printf("[PICK] drawForPick total=%zu Linear=%zu Planar=%zu General=%zu Translucent=%zu\n",
               static_cast<size_t>(commands.getCommandCount()),
               commands.getCommands(RenderPass::OpaqueLinear).size(),
               commands.getCommands(RenderPass::OpaquePlanar).size(),
               commands.getCommands(RenderPass::OpaqueGeneral).size(),
               commands.getCommands(RenderPass::Translucent).size());
    }

    // Draw all opaque passes with pick mode (outputs feature IDs)
    drawPass(commands, RenderPass::OpaqueLinear);
    drawPass(commands, RenderPass::OpaquePlanar);
    drawPass(commands, RenderPass::OpaqueGeneral);

    driver.endRenderPass();
}

// ---------------------------------------------------------------------------
// applyRenderState — select and apply the RenderState for a pass.
// Ported from: itwinjs-core Compositor.getRenderState() (lines 2304-2334)
// + System.instance.applyRenderState() which diffs against the previous state.
// ---------------------------------------------------------------------------
void SceneCompositor::applyRenderState(RenderPass pass)
{
    // Select the pre-configured RenderState for this pass.
    RenderState const* state = nullptr;

    switch (pass) {
        case RenderPass::OpaqueLayers:
        case RenderPass::OverlayLayers:
            state = &m_layerRenderState;
            break;
        case RenderPass::TranslucentLayers: {
            // Like Layers but drawn without depth write.
            // We reuse layerRenderState but override depthMask.
            // itwinjs-core: getRenderState() for TranslucentLayers sets
            //   blend = true, depthMask = false, depthFunc = Default
            // We use a static local for this variant.
            static RenderState sTranslucentLayerState;
            static bool sInit = false;
            if (!sInit) {
                sTranslucentLayerState = m_layerRenderState;
                sTranslucentLayerState.flags.depthMask = false;
                sInit = true;
            }
            state = &sTranslucentLayerState;
            break;
        }
        case RenderPass::OpaqueLinear:
        case RenderPass::OpaquePlanar:
        case RenderPass::OpaqueGeneral:
        case RenderPass::HilitePlanarClassification:
            state = &m_opaqueRenderState;
            break;

        case RenderPass::Translucent:
            state = &m_translucentRenderState;
            break;

        case RenderPass::Hilite:
        case RenderPass::HiliteClassification:
            state = &m_hiliteRenderState;
            break;

        case RenderPass::BackgroundMap:
            state = &m_backgroundMapRenderState;
            break;

        case RenderPass::PointClouds:
            state = &m_pointCloudRenderState;
            break;

        case RenderPass::WorldOverlay:
        case RenderPass::ViewOverlay:
            // Overlay decorations: blend (premultiplied alpha) + no depth test/mask,
            // matching itwinjs Target._overlayRenderState. Translucent overlay fills
            // (ACS triad) composite faintly. Ported from: itwinjs-core getRenderState().
            state = &m_overlayRenderState;
            break;

        default:
            state = &m_noDepthMaskRenderState;
            break;
    }

    // Diff against the previously-applied state and issue only changed GL calls.
    // Ported from: itwinjs-core System.instance.applyRenderState()
    if (state) {
        state->apply(m_currentRenderState);
        m_currentRenderState = *state;
    }
}

END_DQ_RENDER_NAMESPACE

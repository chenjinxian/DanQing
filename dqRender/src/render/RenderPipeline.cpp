// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Rendering pipeline implementation
// Authored: integration layer for dqApp::Viewport; combines itwinjs-core
//           Viewport.ts rendering loop concepts with filament RHI pipeline
#include "dqRender/RenderPipeline.h"
#include "dqRender/Swapchain.h"
#include "dqRender/rhi/OpenGLPlatform.h"
#include "rhi/opengl/OpenGLSwapchain.h"
#include "platform/PlatformFactory.h"
#include "render/TechniqueRegistry.h" // createPlatform —— 平台后端工厂（WGL/GLX/CocoaGL）
#include "RenderSystemImpl.h"
#include "ShaderProgramImpl.h"
#include "OpenGLRenderSystem.h"
#include "OpenGLRenderTarget.h"
#include "TargetImpl.h"
#include "Graphic.h"
#include "TechniqueImpl.h"
#include "PlanarGridTechnique.h"
#include "PlanarGridGraphic.h"
#include <dqGeom/IndexedPolyface.h>
#include <dqRender/GraphicBranch.h>
#include <dqRender/RenderGraphic.h>

#include "SilhouetteEdgeTechnique.h"
#include "PostProcessTechniques.h"
#include "PointCloudTechnique.h"
#include "SkyTechniques.h"
#include "CompositeTechniques.h"
#include "VolumeClassTechniques.h"
#include "MultiVariantTechnique.h"
#include "SurfaceVariantCompiler.h"
#include "PolylineVariantCompiler.h"
#include "IndexedGeometry.h"
#include "PolyfaceGraphic.h"

#include "gl/GL.h"

#include <cmath>
#include <cstring>

BEGIN_DQ_RENDER_NAMESPACE

RenderPipeline::RenderPipeline() = default;
RenderPipeline::~RenderPipeline() { shutdown(); }

bool RenderPipeline::initialize(void* nativeWindow, uint32_t width, uint32_t height)
{
    if (m_initialized) return true;

    // Create platform-specific RHI backend via the platform factory
    // (PlatformFactory.cpp：macOS CocoaGL / Windows WGL / Linux GLX)。
    // Vulkan 后端接入时在工厂内扩展，此处不感知具体平台。
    auto* platform = rhi::createPlatform();
    if (!platform) return false;

    // Create the RHI driver (creates its own GL context)
    rhi::DriverConfig config;
    auto* driver = platform->createDriver(nullptr, config);
    if (!driver) return false;

    // Create render system (takes ownership of driver)
    m_renderSystem = std::make_unique<RenderSystemImpl>(std::unique_ptr<rhi::Driver>(driver));

    // Create Swapchain from native window handle.
    // Binds the GL context to the window surface (NSView on macOS).
    // The Swapchain owns the SwapChain handle and default render target (FBO 0).
    m_swapchain = std::make_unique<OpenGLSwapchain>(
        m_renderSystem->getDriver(), nativeWindow, width, height);
    if (!m_swapchain->isValid()) {
        m_swapchain.reset();
        return false;
    }

    // Use Swapchain's render target as the default target
    m_defaultTarget = m_swapchain->getRenderTarget();

    // Create pick buffer (R32UI) for feature ID readback
    m_pickTarget = m_renderSystem->getDriver().createRenderTarget(
        rhi::TargetBufferFlags::ALL, 1024, 1024, 1, 1);

    // Create techniques and register PlanarGrid
    m_techniques = createDefaultTechniques(m_renderSystem->getDriver());


    // Grid graphic will be created lazily on first use
    // ← itwinjs-core: PlanarGrid is created in System.onInitialized()
    m_gridGraphic = nullptr;

    // Create the public RenderSystem wrapper (non-owning reference to m_renderSystem)
    auto oglSystem = std::make_unique<OpenGLRenderSystem>(*m_renderSystem);
    oglSystem->setTechniques(m_techniques.get());
    m_publicRenderSystem = std::move(oglSystem);

    // Install as the app-wide RenderSystem singleton (IModelApp.renderSystem
    // equivalent). Last-created pipeline wins — debug tooling (RenderSystem::get()
    // .debugControl: GPU profiler, RenderSystemTest) reads the singleton.
    // Multi-viewport caveat: the GLTimer lives per-pipeline; the profiler sees
    // the singleton's frames (sequential per-viewport frames are coherent).
    RenderSystem::setInstance(m_publicRenderSystem.get());

    m_initialized = true;
    return true;
}

void RenderPipeline::shutdown()
{
    if (!m_initialized) return;

    // If we own the app-wide singleton, release it before tearing down.
    if (m_publicRenderSystem && &RenderSystem::get() == m_publicRenderSystem.get())
        RenderSystem::setInstance(nullptr);

    m_pickEntries.clear();
    m_gridGraphic = nullptr;
    m_publicRenderSystem.reset();  // must be before m_renderSystem (non-owning reference)
    m_techniques.reset();

    if (m_pickTarget) {
        if (m_renderSystem) {
            m_renderSystem->getDriver().destroyRenderTarget(m_pickTarget);
        }
        m_pickTarget = {};
    }

    // Destroy Swapchain before Driver (Swapchain owns SwapChain handle + FBO 0)
    m_swapchain.reset();

    if (m_defaultTarget) {
        if (m_renderSystem) {
            m_renderSystem->getDriver().destroyRenderTarget(m_defaultTarget);
        }
        m_defaultTarget = {};
    }

    m_renderSystem.reset();
    m_initialized = false;
}

rhi::Driver& RenderPipeline::getDriver()
{
    return m_renderSystem->getDriver();
}

std::unique_ptr<RenderTarget> RenderPipeline::createRenderTarget(uint32_t width, uint32_t height)
{
    if (!m_renderSystem || !m_techniques || !m_publicRenderSystem) return nullptr;

    ViewRect rect(0, 0, width, height);
    auto* targetImpl = m_renderSystem->createTarget(rect, *m_techniques);
    if (!targetImpl) return nullptr;

    // Set the SwapChain's render target as the output target for endPaint().
    // This enables the compositor to blit to the SwapChain surface
    // instead of the default framebuffer (FBO 0).
    // Works for both OpenGL and Vulkan backends.
    if (m_swapchain && m_swapchain->isValid()) {
        targetImpl->setOutputTarget(m_swapchain->getRenderTarget());
    }

    // Cast to OpenGLRenderSystem (we know it's the concrete type)
    auto& oglSystem = static_cast<OpenGLRenderSystem&>(*m_publicRenderSystem);
    return std::make_unique<OpenGLRenderTarget>(oglSystem, std::unique_ptr<TargetImpl>(targetImpl));
}

void RenderPipeline::createGridGraphic(float extent, int gridLines)
{
    if (!m_initialized || !m_renderSystem) return;

    // Create the grid graphic (must be called with valid GL context)
    if (!m_gridGraphic) {
        m_gridGraphic = new PlanarGridGraphic(m_renderSystem->getDriver(), extent, gridLines);
    }
}

std::unique_ptr<RenderGraphic> RenderPipeline::createGridGraphicWrapped(float extent, int gridLines)
{
    if (!m_renderSystem) return nullptr;

    auto* gridGeometry = new PlanarGridGraphic(m_renderSystem->getDriver(), extent, gridLines);
    auto* primitive = new Primitive(gridGeometry);
    return std::unique_ptr<RenderGraphic>(primitive);
}

std::unique_ptr<RenderGraphic> RenderPipeline::createGridGraphicFromExisting()
{
    if (!m_gridGraphic || !m_renderSystem) return nullptr;

    // Create a new PlanarGridGraphic with the same parameters, owned by the Primitive
    auto* gridGeometry = new PlanarGridGraphic(m_renderSystem->getDriver(), 100.0f, 20);
    auto* primitive = new Primitive(gridGeometry);
    return std::unique_ptr<RenderGraphic>(primitive);
}

RenderGraphic* RenderPipeline::createGraphicFromPolyface(
    dqGeom::IndexedPolyface const* polyface, uint32_t defaultColor, uint32_t featureId,
    rhi::TextureHandle texture, rhi::TextureHandle normalMapTexture, float normalMapScale,
    bool textureExternal, bool normalMapTextureExternal)
{
    // Driver-based factory — the global RenderSystem::get() is a no-op stub unless a
    // system was installed via setInstance(), so driver-bound graphics must come from
    // the pipeline that owns the GL driver. Ported from OpenGLRenderSystem::createGraphicFromPolyface.
    if (!m_renderSystem || !polyface) return nullptr;
    auto const& mesh = *polyface;
    if (mesh.Data().PointCount() == 0 || mesh.FacetCount() == 0) return nullptr;

    auto* polyfaceGeometry = new PolyfaceGraphic(m_renderSystem->getDriver(), mesh, defaultColor, featureId);
    // featureId 已在构造时烘入顶点（pickableOptions { id } 的 batch-local 下标）——
    // 参考在 mesh 构建期写顶点表 featureIndex（MeshBuilder.ts:142-151）。
    // ← itwinjs-core SurfaceGeometry.setTexture (texture set at geometry creation)
    if (texture != rhi::TextureHandle{}) {
        polyfaceGeometry->setTexture(texture);
        polyfaceGeometry->setTextureExternal(textureExternal);
    }
    // ← itwinjs-core MeshData.normalMap (MeshData.ts :61-67 — geometry-sourced
    //   normal map; scale already greenUp-negated by the caller per
    //   Surface.ts :541-550).
    if (normalMapTexture != rhi::TextureHandle{}) {
        polyfaceGeometry->setNormalMapTexture(normalMapTexture);
        polyfaceGeometry->setNormalMapTextureExternal(normalMapTextureExternal);
        polyfaceGeometry->setNormalMapScale(normalMapScale);
    }
    return new Primitive(polyfaceGeometry);
}

// ← itwinjs-core RenderSystem.createTexture (System.ts) — driver-bound (per-viewport).
rhi::TextureHandle RenderPipeline::createTexture(CreateTextureArgs const& args)
{
    // Route through the public OpenGLRenderSystem wrapper (RenderSystemImpl has no
    // createTexture); the wrapper owns the same driver via its impl pointer.
    if (!m_publicRenderSystem) return {};
    return m_publicRenderSystem->createTexture(args);
}

RenderGraphic* RenderPipeline::createGraphicList(std::vector<RenderGraphic*> graphics)
{
    if (graphics.empty()) return nullptr;
    if (graphics.size() == 1) return graphics[0];
    auto* branch = new GraphicBranch(false);  // non-owning — caller/owner manages lifetime
    for (auto* g : graphics)
        branch->add(g);
    return branch;
}

RenderGraphicOwner* RenderPipeline::createGraphicOwner(RenderGraphic* owned)
{
    if (!owned) return nullptr;
    return new RenderGraphicOwner(owned);
}

void RenderPipeline::beginFrame()
{
    if (!m_initialized) return;
    m_pickEntries.clear();
    m_renderSystem->getDriver().beginFrame(0, 0, 0);
}

void RenderPipeline::endFrame()
{
    if (!m_initialized) return;
    m_renderSystem->getDriver().endFrame(0);
}

void RenderPipeline::renderGrid(float const* mvp16, int32_t viewportWidth, int32_t viewportHeight,
                                float const* clearColor)
{
    if (!m_initialized || !m_renderSystem) return;

    auto& driver = m_renderSystem->getDriver();

    // Use the RHI pipeline for grid rendering.
    // With Swapchain, the default target is FBO 0 (the SwapChain surface),
    // so beginRenderPass() works correctly.
    //
    // The Swapchain exposes the default target as FBO 0 (the SwapChain surface)
    // via makeCurrent(), so beginRenderPass() works directly — no raw GL workaround needed.

    if (m_gridGraphic && m_gridGraphic->getVertexCount() > 0) {
        auto* gridTech = static_cast<PlanarGridTechnique*>(
            m_techniques->getTechnique(TechniqueId::PlanarGrid));
        if (gridTech) {
            auto* shader = gridTech->getShader({});
            if (shader && shader->isValid()) {
                // Set shader program and MVP uniform via RHI
                ShaderProgramParams params;
                params.setMatrix4("u_mvpMatrix", mvp16);
                shader->use(driver, params);

                // Begin render pass on the default target (FBO 0 / SwapChain surface)
                rhi::RenderPassParams passParams;
                passParams.flags.clear = rhi::TargetBufferFlags::COLOR_ALL | rhi::TargetBufferFlags::DEPTH;
                passParams.clearColor.f[0] = clearColor[0];
                passParams.clearColor.f[1] = clearColor[1];
                passParams.clearColor.f[2] = clearColor[2];
                passParams.clearColor.f[3] = clearColor[3];
                passParams.clearDepth = 1.0;
                passParams.viewport = {0, 0, static_cast<uint32_t>(viewportWidth),
                                       static_cast<uint32_t>(viewportHeight)};

                driver.beginRenderPass(m_defaultTarget, passParams);
                m_gridGraphic->draw(driver);
                driver.endRenderPass();
            }
        }
    }
}

void RenderPipeline::renderSurface(float const* mvp16, float const* mv16,
                                   int32_t viewportWidth, int32_t viewportHeight,
                                   float const* clearColor)
{
    if (!m_initialized) return;

    auto& driver = m_renderSystem->getDriver();

    // Get the Surface technique and shader (multi-variant)
    auto* surfaceTech = m_techniques->getTechnique(TechniqueId::Surface);
    if (!surfaceTech) return;

    TechniqueFlags flags;  // default: opaque, non-quantized, no features
    auto* shader = surfaceTech->getShader(flags);
    if (!shader) return;

    // Upload uniforms
    ShaderProgramParams params;
    params.setMatrix4("u_mvp", mvp16);
    params.setMatrix4("u_mv", mv16);
    // Default lighting: sun from above-right, ambient gray
    float sunDir[3] = {0.3f, 0.5f, 0.8f};
    float sunIntensity = 0.7f;
    float ambientColor[3] = {0.3f, 0.3f, 0.35f};
    float sunDirNorm = std::sqrt(sunDir[0]*sunDir[0] + sunDir[1]*sunDir[1] + sunDir[2]*sunDir[2]);
    sunDir[0] /= sunDirNorm; sunDir[1] /= sunDirNorm; sunDir[2] /= sunDirNorm;
    params.setVec4("u_sunDir", sunDir);  // vec3 uploaded as vec4 (unused w)
    params.setFloat("u_sunIntensity", sunIntensity);
    params.setVec4("u_ambientColor", ambientColor);

    shader->use(driver, params);

    // Create a test triangle (vertex + normal + color)
    struct SurfaceVertex {
        float position[3];
        float normal[3];
        float color[4];
    };

    SurfaceVertex vertices[3] = {
        {{ 0.0f,  0.5f, 0.0f}, {0, 0, 1}, {1, 0, 0, 1}},  // top, red
        {{-0.5f, -0.5f, 0.0f}, {0, 0, 1}, {0, 1, 0, 1}},  // left, green
        {{ 0.5f, -0.5f, 0.0f}, {0, 0, 1}, {0, 0, 1, 1}},  // right, blue
    };
    uint16_t indices[3] = {0, 1, 2};

    // Create vertex buffer info: position(3f) + normal(3f) + color(4f) = 10 floats = 40 bytes
    rhi::AttributeArray attrs = {};
    attrs[0].buffer = 0;
    attrs[0].offset = 0;
    attrs[0].type = rhi::ElementType::FLOAT3;  // position
    attrs[1].buffer = 0;
    attrs[1].offset = 12;
    attrs[1].type = rhi::ElementType::FLOAT3;  // normal
    attrs[2].buffer = 0;
    attrs[2].offset = 24;
    attrs[2].type = rhi::ElementType::FLOAT4;  // color

    auto vbih = driver.createVertexBufferInfo(1, 3, attrs);
    auto vbh = driver.createVertexBuffer(3, vbih);

    auto vbo = driver.createBufferObject(sizeof(vertices), rhi::BufferObjectBinding::VERTEX,
                                         rhi::BufferUsage::STATIC);
    rhi::BufferDescriptor vboData(vertices, sizeof(vertices));
    driver.updateBufferObject(vbo, std::move(vboData), 0);
    driver.setVertexBufferObject(vbh, 0, vbo);

    auto ibh = driver.createIndexBuffer(rhi::ElementType::USHORT, 3, rhi::BufferUsage::STATIC);
    auto ibo = driver.createBufferObject(sizeof(indices), rhi::BufferObjectBinding::VERTEX,
                                         rhi::BufferUsage::STATIC);
    rhi::BufferDescriptor iboData(indices, sizeof(indices));
    driver.updateBufferObject(ibo, std::move(iboData), 0);

    auto primitive = driver.createRenderPrimitive(vbh, ibh, rhi::PrimitiveType::TRIANGLES);

    // Begin render pass
    rhi::RenderPassParams passParams;
    passParams.flags.clear = rhi::TargetBufferFlags::COLOR_ALL | rhi::TargetBufferFlags::DEPTH;
    passParams.clearColor.f[0] = clearColor[0];
    passParams.clearColor.f[1] = clearColor[1];
    passParams.clearColor.f[2] = clearColor[2];
    passParams.clearColor.f[3] = clearColor[3];
    passParams.clearDepth = 1.0;
    passParams.viewport = {0, 0, static_cast<uint32_t>(viewportWidth),
                           static_cast<uint32_t>(viewportHeight)};

    driver.beginRenderPass(m_defaultTarget, passParams);
    driver.bindRenderPrimitive(primitive);
    driver.draw2(0, 3, 1);
    driver.endRenderPass();

    // Cleanup
    driver.destroyRenderPrimitive(primitive);
    driver.destroyBufferObject(ibo);
    driver.destroyIndexBuffer(ibh);
    driver.destroyBufferObject(vbo);
    driver.destroyVertexBuffer(vbh);
    driver.destroyVertexBufferInfo(vbih);
}

void RenderPipeline::renderEdges(float const* mvp16, int32_t viewportWidth, int32_t viewportHeight,
                                 float const* clearColor)
{
    if (!m_initialized) return;

    auto& driver = m_renderSystem->getDriver();

    // Get the Edge technique and shader (multi-variant)
    auto* edgeTech = m_techniques->getTechnique(TechniqueId::Edge);
    if (!edgeTech) return;

    TechniqueFlags flags;
    auto* shader = edgeTech->getShader(flags);
    if (!shader) return;

    // Upload uniforms
    ShaderProgramParams params;
    params.setMatrix4("u_mvp", mvp16);
    shader->use(driver, params);

    // Create test edge geometry (3 line segments forming a triangle outline)
    struct EdgeVertex {
        float position[3];
        float color[4];
    };

    EdgeVertex vertices[6] = {
        {{ 0.0f,  0.5f, 0.0f}, {1, 1, 0, 1}},  // top, yellow
        {{-0.5f, -0.5f, 0.0f}, {1, 1, 0, 1}},  // left
        {{-0.5f, -0.5f, 0.0f}, {0, 1, 1, 1}},  // left, cyan
        {{ 0.5f, -0.5f, 0.0f}, {0, 1, 1, 1}},  // right
        {{ 0.5f, -0.5f, 0.0f}, {1, 0, 1, 1}},  // right, magenta
        {{ 0.0f,  0.5f, 0.0f}, {1, 0, 1, 1}},  // top
    };
    uint16_t indices[6] = {0, 1, 2, 3, 4, 5};

    // Create vertex buffer info: position(3f) + color(4f) = 7 floats = 28 bytes
    rhi::AttributeArray attrs = {};
    attrs[0].buffer = 0;
    attrs[0].offset = 0;
    attrs[0].type = rhi::ElementType::FLOAT3;  // position
    attrs[1].buffer = 0;
    attrs[1].offset = 12;
    attrs[1].type = rhi::ElementType::FLOAT4;  // color

    auto vbih = driver.createVertexBufferInfo(1, 2, attrs);
    auto vbh = driver.createVertexBuffer(6, vbih);

    auto vbo = driver.createBufferObject(sizeof(vertices), rhi::BufferObjectBinding::VERTEX,
                                         rhi::BufferUsage::STATIC);
    rhi::BufferDescriptor vboData(vertices, sizeof(vertices));
    driver.updateBufferObject(vbo, std::move(vboData), 0);
    driver.setVertexBufferObject(vbh, 0, vbo);

    auto ibh = driver.createIndexBuffer(rhi::ElementType::USHORT, 6, rhi::BufferUsage::STATIC);
    auto ibo = driver.createBufferObject(sizeof(indices), rhi::BufferObjectBinding::VERTEX,
                                         rhi::BufferUsage::STATIC);
    rhi::BufferDescriptor iboData(indices, sizeof(indices));
    driver.updateBufferObject(ibo, std::move(iboData), 0);

    auto primitive = driver.createRenderPrimitive(vbh, ibh, rhi::PrimitiveType::LINES);

    // Begin render pass
    rhi::RenderPassParams passParams;
    passParams.flags.clear = rhi::TargetBufferFlags::COLOR_ALL | rhi::TargetBufferFlags::DEPTH;
    passParams.clearColor.f[0] = clearColor[0];
    passParams.clearColor.f[1] = clearColor[1];
    passParams.clearColor.f[2] = clearColor[2];
    passParams.clearColor.f[3] = clearColor[3];
    passParams.clearDepth = 1.0;
    passParams.viewport = {0, 0, static_cast<uint32_t>(viewportWidth),
                           static_cast<uint32_t>(viewportHeight)};

    driver.beginRenderPass(m_defaultTarget, passParams);
    driver.bindRenderPrimitive(primitive);
    driver.draw2(0, 6, 1);
    driver.endRenderPass();

    // Cleanup
    driver.destroyRenderPrimitive(primitive);
    driver.destroyBufferObject(ibo);
    driver.destroyIndexBuffer(ibh);
    driver.destroyBufferObject(vbo);
    driver.destroyVertexBuffer(vbh);
    driver.destroyVertexBufferInfo(vbih);
}

void RenderPipeline::renderPolyface(void const* polyfacePtr, float const* mvp16,
                                    float const* mv16, int32_t viewportWidth,
                                    int32_t viewportHeight, float const* clearColor)
{
    if (!m_initialized || !polyfacePtr) return;

    auto const* polyface = static_cast<dqGeom::IndexedPolyface const*>(polyfacePtr);
    if (polyface->FacetCount() == 0) return;

    auto& driver = m_renderSystem->getDriver();

    // Create GPU geometry from polyface
    PolyfaceGraphic graphic(driver, *polyface);

    // Get the Surface technique and shader
    auto* surfaceTech = m_techniques->getTechnique(TechniqueId::Surface);
    if (!surfaceTech) return;

    auto* shader = surfaceTech->getShader({});
    if (!shader) return;

    // Upload uniforms
    ShaderProgramParams params;
    params.setMatrix4("u_mvp", mvp16);
    params.setMatrix4("u_mv", mv16);

    float sunDir[3] = {0.3f, 0.5f, 0.8f};
    float sunIntensity = 0.7f;
    float ambientColor[3] = {0.3f, 0.3f, 0.35f};
    float sunDirNorm = std::sqrt(sunDir[0]*sunDir[0] + sunDir[1]*sunDir[1] + sunDir[2]*sunDir[2]);
    sunDir[0] /= sunDirNorm; sunDir[1] /= sunDirNorm; sunDir[2] /= sunDirNorm;
    params.setVec4("u_sunDir", sunDir);
    params.setFloat("u_sunIntensity", sunIntensity);
    params.setVec4("u_ambientColor", ambientColor);

    shader->use(driver, params);

    // Begin render pass
    rhi::RenderPassParams passParams;
    passParams.flags.clear = rhi::TargetBufferFlags::COLOR_ALL | rhi::TargetBufferFlags::DEPTH;
    passParams.clearColor.f[0] = clearColor[0];
    passParams.clearColor.f[1] = clearColor[1];
    passParams.clearColor.f[2] = clearColor[2];
    passParams.clearColor.f[3] = clearColor[3];
    passParams.clearDepth = 1.0;
    passParams.viewport = {0, 0, static_cast<uint32_t>(viewportWidth),
                           static_cast<uint32_t>(viewportHeight)};

    driver.beginRenderPass(m_defaultTarget, passParams);
    graphic.draw(driver);
    driver.endRenderPass();
}

void RenderPipeline::renderPolyfaceWithMaterial(void const* polyfacePtr, float const* mvp16,
                                                float const* mv16, float const* baseColor,
                                                int32_t viewportWidth, int32_t viewportHeight,
                                                float const* clearColor)
{
    if (!m_initialized || !polyfacePtr) return;

    auto const* polyface = static_cast<dqGeom::IndexedPolyface const*>(polyfacePtr);
    if (polyface->FacetCount() == 0) return;

    auto& driver = m_renderSystem->getDriver();

    // Create GPU geometry from polyface
    PolyfaceGraphic graphic(driver, *polyface);

    // Get the Surface technique and shader
    auto* surfaceTech = m_techniques->getTechnique(TechniqueId::Surface);
    if (!surfaceTech) return;

    auto* shader = surfaceTech->getShader({});
    if (!shader) return;

    // Upload uniforms
    ShaderProgramParams params;
    params.setMatrix4("u_mvp", mvp16);
    params.setMatrix4("u_mv", mv16);

    float sunDir[3] = {0.3f, 0.5f, 0.8f};
    float sunIntensity = 0.7f;
    float ambientColor[3] = {0.3f, 0.3f, 0.35f};
    float sunDirNorm = std::sqrt(sunDir[0]*sunDir[0] + sunDir[1]*sunDir[1] + sunDir[2]*sunDir[2]);
    sunDir[0] /= sunDirNorm; sunDir[1] /= sunDirNorm; sunDir[2] /= sunDirNorm;
    params.setVec4("u_sunDir", sunDir);
    params.setFloat("u_sunIntensity", sunIntensity);
    params.setVec4("u_ambientColor", ambientColor);

    // Material base color
    if (baseColor) {
        params.setVec4("u_baseColor", baseColor);
    } else {
        float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        params.setVec4("u_baseColor", white);
    }

    shader->use(driver, params);

    // Begin render pass
    rhi::RenderPassParams passParams;
    passParams.flags.clear = rhi::TargetBufferFlags::COLOR_ALL | rhi::TargetBufferFlags::DEPTH;
    passParams.clearColor.f[0] = clearColor[0];
    passParams.clearColor.f[1] = clearColor[1];
    passParams.clearColor.f[2] = clearColor[2];
    passParams.clearColor.f[3] = clearColor[3];
    passParams.clearDepth = 1.0;
    passParams.viewport = {0, 0, static_cast<uint32_t>(viewportWidth),
                           static_cast<uint32_t>(viewportHeight)};

    driver.beginRenderPass(m_defaultTarget, passParams);
    graphic.draw(driver);
    driver.endRenderPass();
}

uint32_t RenderPipeline::getGridVertexCount() const noexcept
{
    return m_gridGraphic ? m_gridGraphic->getVertexCount() : 0;
}

uint32_t RenderPipeline::getGridIndexCount() const noexcept
{
    return m_gridGraphic ? m_gridGraphic->getIndexCount() : 0;
}

uint32_t RenderPipeline::pickQuery(int32_t x, int32_t y)
{
    if (!m_initialized || !m_pickTarget) return 0;

    // Read a single pixel from the pick buffer using GL_RED_INTEGER / GL_UNSIGNED_INT
    uint32_t featureId = 0;
    auto& driver = m_renderSystem->getDriver();

    // Bind pick target FBO and read pixel
    rhi::PixelBufferDescriptor pbd(&featureId, sizeof(uint32_t),
                                    static_cast<GLenum>(GL::Texture::Format::RedInteger),
                                    static_cast<GLenum>(GL::DataType::UnsignedInt));
    driver.readPixels(m_pickTarget, x, y, 1, 1, std::move(pbd));

    return featureId;
}

void RenderPipeline::recordPickEntry(void const* polyface, uint32_t featureId,
                                     float const* mvp16, float const* mv16)
{
    if (!polyface || featureId == 0) return;

    PickEntry entry;
    entry.polyface = polyface;
    entry.featureId = featureId;
    std::memcpy(entry.mvp, mvp16, 16 * sizeof(float));
    std::memcpy(entry.mv, mv16, 16 * sizeof(float));
    m_pickEntries.push_back(entry);
}

void RenderPipeline::drawForPick(int32_t viewportWidth, int32_t viewportHeight)
{
    if (!m_initialized || !m_pickTarget || m_pickEntries.empty()) return;

    auto& driver = m_renderSystem->getDriver();

    // Get Surface technique with pick shader variant (featureMode = Pick)
    auto* surfaceTech = m_techniques->getTechnique(TechniqueId::Surface);
    if (!surfaceTech) return;

    TechniqueFlags pickFlags;
    pickFlags.featureMode = FeatureMode::Pick;
    auto* pickShader = surfaceTech->getShader(pickFlags);
    if (!pickShader) return;

    // Clear pick buffer to 0
    rhi::RenderPassParams passParams;
    passParams.flags.clear = rhi::TargetBufferFlags::COLOR_ALL | rhi::TargetBufferFlags::DEPTH;
    passParams.clearColor.f[0] = 0.0f;
    passParams.clearColor.f[1] = 0.0f;
    passParams.clearColor.f[2] = 0.0f;
    passParams.clearColor.f[3] = 0.0f;
    passParams.clearDepth = 1.0;
    passParams.viewport = {0, 0, static_cast<uint32_t>(viewportWidth),
                           static_cast<uint32_t>(viewportHeight)};

    driver.beginRenderPass(m_pickTarget, passParams);

    // Re-render each recorded mesh with pick shader
    for (auto const& entry : m_pickEntries) {
        auto const* polyface = static_cast<dqGeom::IndexedPolyface const*>(entry.polyface);
        if (polyface->FacetCount() == 0) continue;

        PolyfaceGraphic graphic(driver, *polyface, 0xFF8080FF, entry.featureId);

        ShaderProgramParams params;
        params.setMatrix4("u_mvp", entry.mvp);
        params.setMatrix4("u_mv", entry.mv);

        pickShader->use(driver, params);
        graphic.draw(driver);
    }

    driver.endRenderPass();
}

void RenderPipeline::renderPolyfaceWithPick(void const* polyfacePtr, uint32_t featureId,
                                            float const* mvp16, float const* mv16,
                                            int32_t viewportWidth, int32_t viewportHeight)
{
    if (!m_initialized || !polyfacePtr) return;

    auto const* polyface = static_cast<dqGeom::IndexedPolyface const*>(polyfacePtr);
    if (polyface->FacetCount() == 0) return;

    auto& driver = m_renderSystem->getDriver();

    // Create GPU geometry with feature ID（构造时烘入——buildFromPolyface 只跑一次）
    PolyfaceGraphic graphic(driver, *polyface, 0xFF8080FF, featureId);

    // Get the Surface technique with pick shader variant
    auto* surfaceTech = m_techniques->getTechnique(TechniqueId::Surface);
    if (!surfaceTech) return;

    TechniqueFlags flags;
    flags.featureMode = FeatureMode::Overrides;
    auto* shader = surfaceTech->getShader(flags);
    if (!shader) return;

    // Upload uniforms
    ShaderProgramParams params;
    params.setMatrix4("u_mvp", mvp16);
    params.setMatrix4("u_mv", mv16);
    float sunDir[3] = {0.3f, 0.5f, 0.8f};
    float sunIntensity = 0.7f;
    float ambientColor[3] = {0.3f, 0.3f, 0.35f};
    float sunDirNorm = std::sqrt(sunDir[0]*sunDir[0] + sunDir[1]*sunDir[1] + sunDir[2]*sunDir[2]);
    sunDir[0] /= sunDirNorm; sunDir[1] /= sunDirNorm; sunDir[2] /= sunDirNorm;
    params.setVec4("u_sunDir", sunDir);
    params.setFloat("u_sunIntensity", sunIntensity);
    params.setVec4("u_ambientColor", ambientColor);

    shader->use(driver, params);

    // Render to default target (flags default-constructed to NONE)
    rhi::RenderPassParams passParams;
    passParams.viewport = {0, 0, static_cast<uint32_t>(viewportWidth),
                           static_cast<uint32_t>(viewportHeight)};
    driver.beginRenderPass(m_defaultTarget, passParams);
    graphic.draw(driver);
    driver.endRenderPass();
}

END_DQ_RENDER_NAMESPACE

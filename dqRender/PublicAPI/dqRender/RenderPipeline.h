// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Rendering pipeline (public API for dqApp)
// Ported from: itwinjs-core core/frontend/src/Viewport.ts (render loop, §10.2 17-step)
//              filament filament/src/renderer.h (Renderer backend lifecycle)
// Authored: dqApp::Viewport integration glue — opaque handles hiding RenderSystemImpl/
//           TargetImpl/Techniques; no single reference source for the integration shape.
//
// Provides a high-level interface for creating and driving the rendering
// pipeline.  Hides internal types (RenderSystemImpl, TargetImpl, Techniques)
// behind opaque handles.
#pragma once

#include "Export.h"
#include "CreateTextureArgs.h"
#include "dqRender/rhi/DriverEnums.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace dqRender {
class Swapchain;
}

namespace dqGeom { class IndexedPolyface; }  // global-scope forward-decl (NOT inside namespace dqRender)

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

namespace rhi {
class Driver;
class OpenGLPlatform;
}

// Forward declarations (internal types, not exposed)
class RenderSystemImpl;
class RenderSystem;
class Techniques;
class PlanarGridGraphic;
class ShaderProgram;
class RenderTarget;
class RenderGraphic;
class RenderGraphicOwner;

// ---------------------------------------------------------------------------
// RenderPipeline — high-level rendering pipeline for Viewport integration
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT RenderPipeline {
public:
    RenderPipeline();
    ~RenderPipeline();

    RenderPipeline(RenderPipeline const&) = delete;
    RenderPipeline& operator=(RenderPipeline const&) = delete;

    /// Initialize the pipeline from a native window handle.
    /// Creates the RHI Driver, Swapchain, RenderSystem, Techniques, and grid geometry.
    /// @param nativeWindow Platform-specific window handle (NSView* on macOS, HWND on Windows).
    /// @param width Initial viewport width in pixels.
    /// @param height Initial viewport height in pixels.
    /// @return true on success.
    bool initialize(void* nativeWindow, uint32_t width, uint32_t height);

    /// Shut down the pipeline and release all GPU resources.
    void shutdown();

    /// Check if the pipeline is initialized.
    bool isInitialized() const noexcept { return m_initialized; }

    /// Get the underlying RHI driver.
    rhi::Driver& getDriver();
    /// U-flip saga minimal repro probe (test-only). Returns 0=correct,
    /// 1=U-flipped, 2=render failed, 3=GL resource failure.

    /// Get the public RenderSystem (OpenGLRenderSystem wrapper).
    RenderSystem* renderSystem() { return m_publicRenderSystem.get(); }

    /// Get the Swapchain (created during Initialize).
    Swapchain* getSwapchain() { return m_swapchain.get(); }

    /// Create a RenderTarget for the given viewport size.
    /// Uses the pipeline's internal RenderSystem and Techniques.
    std::unique_ptr<RenderTarget> createRenderTarget(uint32_t width, uint32_t height);

    /// Create the grid graphic (must be called with valid GL context).
    void createGridGraphic(float extent = 100.0f, int gridLines = 20);

    /// Create a grid graphic wrapped as RenderGraphic.
    std::unique_ptr<RenderGraphic> createGridGraphicWrapped(float extent = 100.0f, int gridLines = 20);

    /// Wrap the existing grid graphic (from Initialize) as RenderGraphic.
    std::unique_ptr<RenderGraphic> createGridGraphicFromExisting();

    /// Create a RenderGraphic from an IndexedPolyface (Surface-shaded, pickable).
    /// Driver-based — uses THIS pipeline's GL driver. This is the real factory; the
    /// global RenderSystem::get() is a no-op stub unless a system was installed via
    /// RenderSystem::setInstance(), so graphics that need a driver must come from here.
    /// Ported from: OpenGLRenderSystem::createGraphicFromPolyface.
    /// normalMapTexture/normalMapScale: the geometry-sourced normal map
    /// (MeshData.ts :61-67) — the caller passes the greenUp-negated scale
    /// (Surface.ts :541-550; glTF binds greenUp=true → -1.0).
    RenderGraphic* createGraphicFromPolyface(dqGeom::IndexedPolyface const* polyface,
                                             uint32_t defaultColor, uint32_t featureId,
                                             rhi::TextureHandle texture = {},
                                             rhi::TextureHandle normalMapTexture = {},
                                             float normalMapScale = 1.0f,
                                             bool textureExternal = false,
                                             bool normalMapTextureExternal = false);

    /// Create a GPU texture from decoded image data (driver-bound, per-viewport).
    /// ← itwinjs-core RenderSystem.createTexture (System.ts)
    rhi::TextureHandle createTexture(CreateTextureArgs const& args);

    /// Wrap a list of graphics into one RenderGraphic (GraphicBranch). Driver-independent.
    RenderGraphic* createGraphicList(std::vector<RenderGraphic*> graphics);

    /// Wrap a graphic in a RenderGraphicOwner (prevents auto-disposal). Driver-independent.
    RenderGraphicOwner* createGraphicOwner(RenderGraphic* owned);

    /// Begin a new frame.
    void beginFrame();

    /// End the current frame.
    void endFrame();

    /// Render the ground grid with the given MVP matrix.
    /// @param mvp16 Column-major 4×4 model-view-projection matrix.
    /// @param viewportWidth Viewport width in pixels.
    /// @param viewportHeight Viewport height in pixels.
    /// @param clearColor RGBA background color (0-1 range).
    void renderGrid(float const* mvp16, int32_t viewportWidth, int32_t viewportHeight,
                    float const* clearColor);

    /// Render a test surface (colored triangle) with lighting.
    /// @param mvp16 Column-major 4×4 model-view-projection matrix.
    /// @param mv16 Column-major 4×4 model-view matrix (for normals).
    /// @param viewportWidth Viewport width in pixels.
    /// @param viewportHeight Viewport height in pixels.
    /// @param clearColor RGBA background color (0-1 range).
    void renderSurface(float const* mvp16, float const* mv16,
                       int32_t viewportWidth, int32_t viewportHeight,
                       float const* clearColor);

    /// Render test edges (line segments).
    void renderEdges(float const* mvp16, int32_t viewportWidth, int32_t viewportHeight,
                     float const* clearColor);

    /// Render an IndexedPolyface mesh with lighting.
    /// @param polyface The mesh to render.
    /// @param mvp16 Column-major 4×4 model-view-projection matrix.
    /// @param mv16 Column-major 4×4 model-view matrix (for normals).
    /// @param viewportWidth Viewport width in pixels.
    /// @param viewportHeight Viewport height in pixels.
    /// @param clearColor RGBA background color (0-1 range).
    void renderPolyface(void const* polyface, float const* mvp16, float const* mv16,
                        int32_t viewportWidth, int32_t viewportHeight,
                        float const* clearColor);

    /// Render an IndexedPolyface mesh with lighting and material color.
    /// @param polyface The mesh to render.
    /// @param mvp16 Column-major 4×4 model-view-projection matrix.
    /// @param mv16 Column-major 4×4 model-view matrix (for normals).
    /// @param baseColor RGBA base color (0-1 range, from glTF material).
    /// @param viewportWidth Viewport width in pixels.
    /// @param viewportHeight Viewport height in pixels.
    /// @param clearColor RGBA background color (0-1 range).
    void renderPolyfaceWithMaterial(void const* polyface, float const* mvp16, float const* mv16,
                                    float const* baseColor,
                                    int32_t viewportWidth, int32_t viewportHeight,
                                    float const* clearColor);

    /// Get grid vertex count (for testing).
    uint32_t getGridVertexCount() const noexcept;

    /// Get grid index count (for testing).
    uint32_t getGridIndexCount() const noexcept;

    /// Pick query: read feature ID at pixel coordinate.
    /// @param x Pixel X coordinate.
    /// @param y Pixel Y coordinate.
    /// @return Feature ID at (x, y), or 0 if no geometry.
    uint32_t pickQuery(int32_t x, int32_t y);

    /// Render a polyface with pick IDs (writes to pick buffer).
    /// @param polyface The mesh to render.
    /// @param featureId Feature ID to assign to all vertices.
    /// @param mvp16 Model-view-projection matrix.
    /// @param mv16 Model-view matrix.
    /// @param viewportWidth Viewport width.
    /// @param viewportHeight Viewport height.
    void renderPolyfaceWithPick(void const* polyface, uint32_t featureId,
                                float const* mvp16, float const* mv16,
                                int32_t viewportWidth, int32_t viewportHeight);

    /// Record a mesh for pick rendering. Called during main render pass.
    /// The mesh will be re-rendered with pick shader in drawForPick().
    void recordPickEntry(void const* polyface, uint32_t featureId,
                         float const* mvp16, float const* mv16);

    /// Draw the pick pass: re-render all recorded meshes with pick shader
    /// variant to the pick buffer. Called after endFrame().
    /// @param viewportWidth Viewport width in pixels.
    /// @param viewportHeight Viewport height in pixels.
    void drawForPick(int32_t viewportWidth, int32_t viewportHeight);

private:
    /// Entry recorded during main render pass for pick re-rendering.
    struct PickEntry {
        void const* polyface;
        uint32_t featureId;
        float mvp[16];
        float mv[16];
    };

    bool m_initialized = false;
    std::unique_ptr<rhi::Driver> m_driver;  // transferred to m_renderSystem
    std::unique_ptr<RenderSystemImpl> m_renderSystem;
    std::unique_ptr<RenderSystem> m_publicRenderSystem;  // OpenGLRenderSystem wrapper
    std::unique_ptr<Techniques> m_techniques;
    std::unique_ptr<Swapchain> m_swapchain;
    PlanarGridGraphic* m_gridGraphic = nullptr;  // owned by m_renderSystem lifetime
    rhi::RenderTargetHandle m_defaultTarget;
    rhi::RenderTargetHandle m_pickTarget;  // R32UI pick buffer
    std::vector<PickEntry> m_pickEntries;  // meshes to re-render for picking
};

END_DQ_RENDER_NAMESPACE

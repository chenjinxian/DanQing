// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Planar classifier
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/PlanarClassifier.ts
//
// Stencil-based classification of reality meshes (e.g., DTM vs. building).
// Classifies geometry onto a planar surface using stencil operations.
//
// Architecture:
//   ClassifierTextures (color + feature + hilite textures)
//   ClassifierFrameBuffers (FBO pair for rendering classifier geometry)
//   MaskFrameBuffer (FBO for mask geometry)
//   ClassifierCombinationBuffer (composites classifier + mask textures)
//   PlanarClassifier (main class — draw, collectGraphics, getParams)
#pragma once

#include "TextureHandle.h"
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <array>
#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Forward declarations
class RenderCommands;
class BranchStack;
class BatchState;
class TargetImpl;

// ---------------------------------------------------------------------------
// PlanarClassifierContent — what the classifier contains
// (Ported from: itwinjs-core PlanarClassifier.ts PlanarClassifierContent)
// ---------------------------------------------------------------------------
enum class PlanarClassifierContent : uint8_t {
    None = 0,
    MaskOnly = 1,
    ClassifierOnly = 2,
    ClassifierAndMask = 3,
};

// ---------------------------------------------------------------------------
// ClassifierTextures — GPU textures for planar classification
// (Ported from: itwinjs-core PlanarClassifier.ts ClassifierTextures)
// ---------------------------------------------------------------------------
class ClassifierTextures {
public:
    rhi::TextureHandle color;
    rhi::TextureHandle feature;
    rhi::TextureHandle hilite;

    ClassifierTextures() = default;
    ClassifierTextures(rhi::TextureHandle c, rhi::TextureHandle f, rhi::TextureHandle h)
        : color(c), feature(f), hilite(h) {}

    bool isValid() const noexcept {
        return static_cast<bool>(color) && static_cast<bool>(feature) && static_cast<bool>(hilite);
    }

    void destroy(rhi::Driver& driver) {
        if (color) { driver.destroyTexture(color); color = {}; }
        if (feature) { driver.destroyTexture(feature); feature = {}; }
        if (hilite) { driver.destroyTexture(hilite); hilite = {}; }
    }

    /// Create classifier textures at the given resolution.
    /// Ported from: itwinjs-core ClassifierTextures.create()
    static ClassifierTextures create(rhi::Driver& driver, uint32_t width, uint32_t height) {
        auto c = driver.createTexture(rhi::SamplerType::SAMPLER_2D, 1, rhi::TextureFormat::RGBA8,
                                       width, height, 1, rhi::TextureUsage::DEFAULT);
        auto f = driver.createTexture(rhi::SamplerType::SAMPLER_2D, 1, rhi::TextureFormat::RGBA8,
                                       width, height, 1, rhi::TextureUsage::DEFAULT);
        auto h = driver.createTexture(rhi::SamplerType::SAMPLER_2D, 1, rhi::TextureFormat::RGBA8,
                                       width, height, 1, rhi::TextureUsage::DEFAULT);
        return ClassifierTextures(c, f, h);
    }
};

// ---------------------------------------------------------------------------
// ClassifierFrameBuffers — FBO pair for rendering classifier geometry
// (Ported from: itwinjs-core PlanarClassifier.ts ClassifierFrameBuffers)
//
// Manages the FBO pair (color+feature textures, hilite texture) that the
// classifier renders into. has draw() and drawHilite() methods that execute
// draw commands into the correct render passes.
// ---------------------------------------------------------------------------
class ClassifierFrameBuffers {
public:
    ClassifierTextures textures;

    ClassifierFrameBuffers() = default;
    ClassifierFrameBuffers(ClassifierTextures tex) : textures(tex) {}

    bool isValid() const noexcept { return textures.isValid(); }

    void destroy(rhi::Driver& driver) { textures.destroy(driver); }

    /// Draw classifier commands into the FBO.
    /// Ported from: itwinjs-core ClassifierFrameBuffers.draw()
    void draw(RenderCommands& cmds, TargetImpl& target);

    /// Draw hilite commands into the hilite FBO.
    /// Ported from: itwinjs-core ClassifierFrameBuffers.drawHilite()
    void drawHilite(RenderCommands& cmds, TargetImpl& target);

    /// Create classifier frame buffers at the given resolution.
    /// Ported from: itwinjs-core ClassifierFrameBuffers.create()
    static ClassifierFrameBuffers create(rhi::Driver& driver, uint32_t width, uint32_t height) {
        auto textures = ClassifierTextures::create(driver, width, height);
        return ClassifierFrameBuffers(textures);
    }
};

// ---------------------------------------------------------------------------
// PlanarClassifier — stencil-based planar classification
// (Ported from: itwinjs-core PlanarClassifier.ts PlanarClassifier)
//
// Main class that manages the planar classification rendering pipeline.
// Renders classifier geometry into FBO textures, composites them, and
// provides texture parameters for the shader pipeline.
// ---------------------------------------------------------------------------
class PlanarClassifier {
public:
    PlanarClassifier() = default;
    ~PlanarClassifier() = default;

    PlanarClassifier(PlanarClassifier const&) = delete;
    PlanarClassifier& operator=(PlanarClassifier const&) = delete;

    // --- Content mode ---
    void setContent(PlanarClassifierContent content) noexcept { m_content = content; }
    PlanarClassifierContent getContent() const noexcept { return m_content; }
    bool isActive() const noexcept { return m_content != PlanarClassifierContent::None; }

    // --- Classifier color (RGBA) ---
    void setColor(float r, float g, float b, float a) noexcept {
        m_color[0] = r; m_color[1] = g; m_color[2] = b; m_color[3] = a;
    }
    float const* getColor() const noexcept { return m_color; }

    // --- Display modes ---
    // Ported from: itwinjs-core PlanarClassifier.insideDisplay/outsideDisplay
    uint32_t getInsideDisplay() const noexcept { return m_insideDisplay; }
    void setInsideDisplay(uint32_t v) noexcept { m_insideDisplay = v; }
    uint32_t getOutsideDisplay() const noexcept { return m_outsideDisplay; }
    void setOutsideDisplay(uint32_t v) noexcept { m_outsideDisplay = v; }

    // --- Texture parameters for shader pipeline ---
    // Ported from: itwinjs-core PlanarClassifier.getParams()
    void getParams(float* params) const noexcept {
        params[0] = static_cast<float>(m_insideDisplay);
        params[1] = static_cast<float>(m_outsideDisplay);
        params[2] = static_cast<float>(m_content);
        params[3] = m_transparency;
    }

    // --- Batch ID management ---
    // Ported from: itwinjs-core PlanarClassifier.baseBatchId
    uint32_t getBaseBatchId() const noexcept { return m_baseBatchId; }
    void setBaseBatchId(uint32_t id) noexcept { m_baseBatchId = id; }

    // --- Hilite state ---
    bool isAnyHilited() const noexcept { return m_anyHilited; }
    void setAnyHilited(bool v) noexcept { m_anyHilited = v; }

    // --- Opaque/Translucent state ---
    bool isAnyOpaque() const noexcept { return m_anyOpaque; }
    bool isAnyTranslucent() const noexcept { return m_anyTranslucent; }

    // --- Texture access ---
    // Ported from: itwinjs-core PlanarClassifier.texture/hiliteTexture
    rhi::TextureHandle getTexture() const noexcept { return m_combinedTexture; }
    rhi::TextureHandle getHiliteTexture() const noexcept { return m_hiliteTexture; }

    // --- Resolution ---
    void setResolution(uint32_t width, uint32_t height) noexcept {
        m_width = width; m_height = height;
    }
    uint32_t getWidth() const noexcept { return m_width; }
    uint32_t getHeight() const noexcept { return m_height; }

    // --- Transparency ---
    void setTransparency(float t) noexcept { m_transparency = t; }

    // --- Rendering pipeline ---
    // Ported from: itwinjs-core PlanarClassifier.draw()

    /// Draw the classifier geometry into FBO textures.
    /// This is the core rendering method that:
    /// 1. Sets up FBOs and textures
    /// 2. Overrides target state (view flags, frustum, render state)
    /// 3. Renders classifier/mask graphics into FBOs
    /// 4. Composites the results
    void draw(TargetImpl& target);

    /// Collect graphics from tile tree references for classification.
    /// Ported from: itwinjs-core PlanarClassifier.collectGraphics()
    void collectGraphics(void* sceneContext, void* target);

    /// Push batch state for feature ID management.
    /// Ported from: itwinjs-core PlanarClassifier.pushBatchState()
    void pushBatchState(BatchState& batchState);

    /// add a graphic to the classifier.
    /// Ported from: itwinjs-core PlanarClassifier.addGraphic()
    void addGraphic(void* graphic) { m_graphics.push_back(graphic); }

    // --- Projection matrix ---
    std::array<float, 16> const& getProjectionMatrix() const noexcept { return m_projectionMatrix; }
    void setProjectionMatrix(std::array<float, 16> const& m) noexcept { m_projectionMatrix = m; }

private:
    PlanarClassifierContent m_content = PlanarClassifierContent::None;
    float m_color[4] = {1.0f, 1.0f, 1.0f, 0.5f};
    uint32_t m_insideDisplay = 0;   // SpatialClassifierInsideDisplay::Off
    uint32_t m_outsideDisplay = 1;  // SpatialClassifierOutsideDisplay::On
    uint32_t m_baseBatchId = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    float m_transparency = -1.0f;
    bool m_anyHilited = false;
    bool m_anyOpaque = false;
    bool m_anyTranslucent = false;
    rhi::TextureHandle m_combinedTexture;
    rhi::TextureHandle m_hiliteTexture;

    // Rendering pipeline state (Ported from: itwinjs-core PlanarClassifier members)
    ClassifierFrameBuffers m_classifierBuffers;
    std::array<float, 16> m_projectionMatrix = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    std::vector<void*> m_graphics;  // RenderGraphics collected for classification
    std::vector<void*> m_classifierGraphics;
    std::vector<void*> m_maskGraphics;
};

END_DQ_RENDER_NAMESPACE

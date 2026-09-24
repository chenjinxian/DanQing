// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Mock render system (for testing)
// Ported from: itwinjs-core core/frontend/src/internal/render/MockRender.ts
//
// Mock render system for testing.  Provides a no-op implementation of
// the render system interface.
#pragma once

#include "dqRender/RenderSystem.h"

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// MockRenderSystem — no-op render system for testing
// (Ported from: itwinjs-core MockRender.ts)
// ---------------------------------------------------------------------------
class MockRenderSystem : public RenderSystem {
public:
    MockRenderSystem() = default;
    ~MockRenderSystem() override = default;

    // --- RenderSystem interface (all no-ops) ---
    std::unique_ptr<RenderTarget> createTarget(void*, uint32_t, uint32_t) override { return nullptr; }
    std::unique_ptr<GraphicBuilder> createGraphicBuilder(GraphicBuilderOptions const&) override { return nullptr; }
    GraphicBranch* createBranch(bool) override { return nullptr; }
    RenderGraphic* createBranchGraphic(GraphicBranch*) override { return nullptr; }
    RenderGraphic* createGraphicList(std::vector<RenderGraphic*>) override { return nullptr; }
    RenderGraphicOwner* createGraphicOwner(RenderGraphic*) override { return nullptr; }
    bool isValid() const noexcept override { return true; }
};

END_DQ_RENDER_NAMESPACE

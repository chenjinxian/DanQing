// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Rendering engine entry point implementation
// Ported from: filament backend/src/DriverBase.cpp (initialization pattern)
#include "dqRender/DqRender.h"
#include "dqRender/RenderSystem.h"

#include <atomic>
#include <memory>

BEGIN_DQ_RENDER_NAMESPACE

static std::atomic<bool> s_initialized{false};

// Global RenderSystem singleton (← IModelApp.renderSystem)
static RenderSystem* s_renderSystemInstance = nullptr;

// ---------------------------------------------------------------------------
// NullRenderSystem — no-op concrete RenderSystem used as the lazy default
// returned by get() when setInstance() has not been called.
//
// Faithfulness note: in itwinjs-core, IModelApp.renderSystem is always non-null
// after IModelApp.startup (established by frontend host setup). DanQing does not
// yet plumb SetInstance through app startup (TODO: wire in dqApp initialization),
// so to preserve the ref invariant ("get() never returns null") we lazily
// install a no-op system. All factory methods return nullptr/empty, matching
// MockRender's no-op semantics (ref core/frontend/src/internal/render/MockRender.ts).
// ---------------------------------------------------------------------------
namespace {
class NullRenderSystem final : public RenderSystem {
public:
    NullRenderSystem() = default;
    ~NullRenderSystem() final = default;

    bool isValid() const noexcept override { return false; }
    std::unique_ptr<RenderTarget> createTarget(void*, uint32_t, uint32_t) override { return nullptr; }
    std::unique_ptr<GraphicBuilder> createGraphicBuilder(GraphicBuilderOptions const&) override { return nullptr; }
    GraphicBranch* createBranch(bool) override { return nullptr; }
    RenderGraphic* createBranchGraphic(GraphicBranch*) override { return nullptr; }
    RenderGraphic* createGraphicList(std::vector<RenderGraphic*>) override { return nullptr; }
    RenderGraphicOwner* createGraphicOwner(RenderGraphic*) override { return nullptr; }
};
}  // namespace

// Lazily-constructed default instance; lifetime is the program lifetime.
// Thread-safe: local static initialization is guarded by the C++ runtime.
static RenderSystem& defaultRenderSystem()
{
    static NullRenderSystem s_default;
    return s_default;
}

void DqRenderLib::initialize() noexcept
{
    bool expected = false;
    if (!s_initialized.compare_exchange_strong(expected, true)) {
        return;  // Already initialized
    }
    // TODO: Initialize GL function pointers, register shader types, etc.
}

void DqRenderLib::shutdown() noexcept
{
    bool expected = true;
    if (!s_initialized.compare_exchange_strong(expected, false)) {
        return;  // Not initialized or already shut down
    }
    // TODO: Release all GPU resources, destroy driver
    // Note: do NOT clear s_renderSystemInstance here — ownership belongs to the
    // caller of SetInstance; clearing could mask double-shutdown bugs.
}

// ---------------------------------------------------------------------------
// RenderSystem static methods
// ---------------------------------------------------------------------------

RenderSystem& RenderSystem::get()
{
    // Faithful to itwinjs-core: IModelApp.renderSystem is never null. If no
    // concrete system was installed via setInstance(), fall back to a no-op
    // default so callers never dereference null (P0 crash fix).
    if (s_renderSystemInstance != nullptr)
        return *s_renderSystemInstance;
    return defaultRenderSystem();
}

void RenderSystem::setInstance(RenderSystem* instance)
{
    s_renderSystemInstance = instance;
}

END_DQ_RENDER_NAMESPACE

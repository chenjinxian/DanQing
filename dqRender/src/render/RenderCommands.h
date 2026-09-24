// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Draw command collection and sorting
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/RenderCommands.ts
//
// Collects DrawCommands from scene graph traversal and sorts them into
// render passes (23 passes total, matching itwinjs RenderPass enum).
#pragma once

#include "DrawCommand.h"
#include "BranchStack.h"
#include "FrustumPlanes.h"
#include "gl/RenderFlags.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Forward declarations (avoid circular includes with Graphic.h)
class CachedGeometry;
class Primitive;
class Branch;
class Batch;
class Graphic;
class TargetImpl;
class BatchState;
class TargetGraphics;

// ---------------------------------------------------------------------------
// RenderCommands — sorted command buffer
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/RenderCommands.ts
//
// Collects DrawCommands from scene graph traversal and distributes them into
// per-pass command lists.  Implements the full pass routing logic including
// forced render passes, feature-less remapping, double-pass rendering,
// translucent-as-opaque, and opaque/translucent overrides.
// ---------------------------------------------------------------------------
class RenderCommands {
public:
    /// Construct with references to the rendering context.
    /// Ported from: itwinjs-core RenderCommands constructor (line 97-105)
    RenderCommands(TargetImpl& target, BranchStack& stack, BatchState& batchState);
    ~RenderCommands() = default;

    // --- Reset ---
    /// Reset for a new frame.
    /// Ported from: itwinjs-core RenderCommands.reset() (line 107-113)
    void reset(TargetImpl& target, BranchStack& stack, BatchState& batchState);

    // --- Core scene graph traversal ---

    /// add graphics from a scene graph list.
    /// Ported from: itwinjs-core RenderCommands.addGraphics() (line 124-128)
    void addGraphics(std::vector<Graphic*>& scene,
                     RenderPass forcedPass = RenderPass::None);

    /// add a primitive (leaf geometry) to the command buffer.
    /// Ported from: itwinjs-core RenderCommands.addPrimitive() (line 577-602)
    void addPrimitive(Primitive& prim);

    /// add a primitive from raw CachedGeometry (convenience overload).
    /// Creates a PrimitiveCommand and routes it through addPrimitiveCommand.
    void addPrimitive(CachedGeometry* geometry);

    /// add a branch (transform/viewFlags node) to the command buffer.
    /// Ported from: itwinjs-core RenderCommands.addBranch() (line 604-608)
    void addBranch(Branch& branch);

    /// add a batch (feature grouping node) to the command buffer.
    /// Ported from: itwinjs-core RenderCommands.addBatch() (line 634-701)
    void addBatch(Batch& batch);

    // --- Primitive command routing ---

    /// add a primitive command with full pass routing.
    /// Ported from: itwinjs-core RenderCommands.addPrimitiveCommand() (line 203-280)
    ///
    /// Implements: forced pass, feature-less remapping, double-pass,
    /// translucent-as-opaque, opaque/translucent overrides.
    void addPrimitiveCommand(PrimitiveCommand& command,
                             GL::Pass pass = GL::Pass::None);

    /// add a primitive command DIRECTLY into a specific RenderPass bucket (no
    /// routing). For hilite-branch draws (Primitive::addHiliteCommands): the
    /// caller's pass is a RenderPass (Hilite) with no GL::Pass equivalent, so
    /// the routing layer can't reach it — write straight into the bucket the
    /// compositor's drawPass(RenderPass::Hilite) reads.
    void addHilitePrimitiveCommand(PrimitiveCommand& command, RenderPass pass)
    {
        if (pass == RenderPass::None || !command.getGeometry())
            return;
        m_commands[static_cast<size_t>(pass)].push_back(
            std::make_unique<PrimitiveCommand>(command.getGeometry()));
    }

    // --- Background / overlay / decoration methods ---

    /// add background map graphics to their own render pass.
    /// Ported from: itwinjs-core RenderCommands.addBackgroundMapGraphics() (line 131-135)
    void addBackgroundMapGraphics(std::vector<Graphic*>& backgroundMapGraphics);

    /// add overlay graphics to the world overlay pass.
    /// Ported from: itwinjs-core RenderCommands.addOverlayGraphics() (line 137-141)
    void addOverlayGraphics(std::vector<Graphic*>& overlayGraphics);

    /// add decorations to a specific pass.
    /// Ported from: itwinjs-core RenderCommands.addDecorations() (line 143-150)
    void addDecorations(std::vector<Graphic*>& dec,
                        RenderPass forcedPass = RenderPass::None);

    /// add world decorations (wrapped in a WorldDecorations branch).
    /// Ported from: itwinjs-core RenderCommands.addWorldDecorations() (line 152-159)
    void addWorldDecorations(std::vector<Graphic*>& decs);

    /// add the view background graphic.
    /// Ported from: itwinjs-core RenderCommands.addBackground() (line 181-190)
    void addBackground(Graphic* gf);

    /// add the sky box graphic.
    /// Ported from: itwinjs-core RenderCommands.addSkyBox() (line 192-201)
    void addSkyBox(Graphic* gf);

    // --- Hilite methods ---

    /// add hilite commands for a branch.
    /// Ported from: itwinjs-core RenderCommands.addHiliteBranch() (line 297-301)
    void addHiliteBranch(Branch& branch, RenderPass pass);

    /// Compute the hilite render pass for a batch.
    /// Ported from: itwinjs-core RenderCommands.computeBatchHiliteRenderPass() (line 610-632)
    RenderPass computeBatchHiliteRenderPass(Batch& batch);

    // --- Command buffer initialization ---

    /// Initialize commands for normal rendering.
    /// Ported from: itwinjs-core RenderCommands.initForRender() (line 542-575)
    void initForRender(TargetGraphics& gfx);

    /// Initialize commands for readPixels (translucent drawn as opaque).
    /// Ported from: itwinjs-core RenderCommands.initForReadPixels() (line 520-540)
    void initForReadPixels(TargetGraphics& gfx);

    /// Initialize commands for overlay picking.
    /// Ported from: itwinjs-core RenderCommands.initForPickOverlays() (line 497-518)
    void initForPickOverlays(std::vector<Graphic*>& sceneOverlays,
                             std::vector<Graphic*>* worldOverlayDecorations,
                             std::vector<Graphic*>* viewOverlayDecorations);

    // --- Push/pop with callbacks ---

    /// Push/pop a branch with a callback.
    /// Ported from: itwinjs-core RenderCommands.pushAndPopBranch() (line 449-458)
    void pushAndPopBranch(Branch& branch, std::function<void()> func);

    /// Push/pop a branch for a specific render pass.
    /// Ported from: itwinjs-core RenderCommands.pushAndPopBranchForPass() (line 357-394)
    void pushAndPopBranchForPass(RenderPass pass, Branch& branch,
                                 std::function<void()> func);

    /// Push/pop a BranchState with a callback.
    /// Ported from: itwinjs-core RenderCommands.pushAndPopState() (line 473-477)
    void pushAndPopState(BranchState const& state, std::function<void()> func);

    // --- Command buffer access ---

    /// Get commands for a specific render pass.
    std::vector<std::unique_ptr<DrawCommand>> const& getCommands(RenderPass pass) const;
    std::vector<std::unique_ptr<DrawCommand>>& getCommands(RenderPass pass);

    /// Replace commands for a specific render pass.
    /// Ported from: itwinjs-core RenderCommands.replaceCommands() (line 291-295)
    void replaceCommands(RenderPass pass, std::vector<std::unique_ptr<DrawCommand>>& cmds);

    /// Get total command count across all passes.
    size_t getCommandCount() const;

    /// Render-command breakdown for the diagnostics panel.
    /// Ported from: itwinjs-core RenderCommands.dump() (RenderCommands.ts:742-771)
    /// — Primitives/Batches/Branches counts across all passes.
    struct CommandCount {
        uint32_t primitives = 0;
        uint32_t batches = 0;
        uint32_t branches = 0;
    };
    CommandCount dump() const;

    /// Get the number of render passes.
    static constexpr size_t getPassCount() { return static_cast<size_t>(RenderPass::COUNT); }

    /// Clear all commands.
    /// Ported from: itwinjs-core RenderCommands.clear() (line 479-482)
    void clear();

    /// Check if any pass has commands.
    /// Ported from: itwinjs-core RenderCommands.isEmpty (line 58-64)
    bool isEmpty() const;

    /// Check if a specific pass has commands.
    /// Ported from: itwinjs-core RenderCommands.hasCommands()
    bool hasCommands(RenderPass pass) const;

    /// Check if currently drawing layers.
    /// Ported from: itwinjs-core RenderCommands.isDrawingLayers (line 67-75)
    bool isDrawingLayers() const;

    /// Get composite flags (what needs compositing).
    /// Ported from: itwinjs-core RenderCommands.compositeFlags (line 78-90)
    uint8_t getCompositeFlags() const;

    /// Check if a pass is opaque.
    /// Ported from: itwinjs-core RenderCommands.isOpaquePass()
    static bool isOpaquePass(RenderPass pass);

    /// Set a culling frustum from an MVP matrix.
    /// Ported from: itwinjs-core RenderCommands.setCheckRange() (line 704)
    void setCheckRange(float const* mvp16);

    /// Clear the culling frustum.
    /// Ported from: itwinjs-core RenderCommands.clearCheckRange() (line 706)
    void clearCheckRange();

private:
    /// Generic push/pop helper (callback pattern).
    /// Ported from: itwinjs-core RenderCommands.pushAndPop() (line 396-447)
    void pushAndPop(DrawCommand& push, DrawCommand& pop, std::function<void()> func);

    /// Internal push/pop branch (without animation check).
    /// Ported from: itwinjs-core RenderCommands._pushAndPopBranch() (line 460-471)
    void pushAndPopBranchInternal(Branch& branch, std::function<void()> func);

    /// Set up classification by volume (split classification pass into per-index).
    /// Ported from: itwinjs-core RenderCommands.setupClassificationByVolume() (line 708-740)
    void setupClassificationByVolume();

    /// Clear all command lists.
    /// Ported from: itwinjs-core RenderCommands._clearCommands() (line 484-487)
    void clearCommands();

    static constexpr size_t kPassCount = static_cast<size_t>(RenderPass::COUNT);

    std::array<std::vector<std::unique_ptr<DrawCommand>>, kPassCount> m_commands;

    // --- Rendering context (Ported from: itwinjs-core RenderCommands members) ---
    TargetImpl* m_target = nullptr;
    BranchStack* m_stack = nullptr;
    BatchState* m_batchState = nullptr;

    // --- Pass routing state ---
    // Ported from: itwinjs-core RenderCommands._forcedRenderPass, etc.
    RenderPass m_forcedRenderPass = RenderPass::None;
    [[maybe_unused]] bool m_addLayersAsNormalGraphics = false;  // Ported from: itwinjs-core — used by layer processing
    bool m_opaqueOverrides = false;
    bool m_translucentOverrides = false;
    bool m_addTranslucentAsOpaque = false;  // true during readPixels

    // --- Frustum culling ---
    // Ported from: itwinjs-core RenderCommands._frustumPlanes
    FrustumPlanes m_frustumPlanes;
};

END_DQ_RENDER_NAMESPACE

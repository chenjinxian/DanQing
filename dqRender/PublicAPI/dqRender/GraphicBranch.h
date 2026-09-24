// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GraphicBranch (scene graph node)
//
// Ported from: itwinjs-core core/frontend/src/render/GraphicBranch.ts
// A node in a scene graph containing child graphics with transforms and overrides.
//
// NOTE: In itwinjs-core, GraphicBranch implements Disposable (NOT RenderGraphic).
// In DanQing, GraphicBranch inherits from RenderGraphic for compatibility with
// the existing code that stores RenderGraphic* in Scene lists. This is a known
// structural divergence that will be addressed in a future refactor.
#pragma once

#include "Export.h"
#include "RenderGraphic.h"

#include <dqCommon/ViewFlags.h>
#include <dqRender/FeatureSymbology.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

#include <cstdint>
#include <optional>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// GraphicBranchOptions — options for RenderSystem.createGraphicBranch()
// Ported from: itwinjs-core GraphicBranch.ts GraphicBranchOptions
// ---------------------------------------------------------------------------
struct GraphicBranchOptions {
    bool ownsEntries = false;
    // Ported from: itwinjs-core GraphicBranch options
    // These properties will be added when the corresponding types are ported:
    // planarClassifier, textureDrape, edgeSettings, iModel, frustum,
    // appearanceProvider, secondaryClassifiers, contourLine,
    // viewAttachmentId, inSectionDrawingAttachment, disableClipStyle
};

// ---------------------------------------------------------------------------
// GraphicBranch — scene graph branch node
// Ported from: itwinjs-core core/frontend/src/render/GraphicBranch.ts
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT GraphicBranch : public RenderGraphic {
public:
    // Child graphics.
    std::vector<RenderGraphic*> entries;

    // If true, dispose children when this branch is disposed.
    bool ownsEntries = false;

    // ViewFlag overrides for this branch.
    // Ported from: itwinjs-core GraphicBranch.ts viewFlagOverrides
    dqCommon::ViewFlagsProperties viewFlagOverrides;

    // Symbology overrides for this branch.
    // Ported from: itwinjs-core GraphicBranch.ts symbologyOverrides
    std::optional<FeatureAppearance> symbologyOverrides;

    // Animation node ID for this branch.
    // Ported from: itwinjs-core GraphicBranch.ts animationNodeId
    std::optional<int32_t> animationNodeId;

    // Group node ID for this branch.
    // Ported from: itwinjs-core GraphicBranch.ts groupNodeId
    std::optional<int32_t> groupNodeId;

    // Reality model display settings.
    // Ported from: itwinjs-core GraphicBranch.ts realityModelDisplaySettings
    // @beta — not yet implemented

    GraphicBranch() = default;
    explicit GraphicBranch(bool owns) : ownsEntries(owns) {}
    explicit GraphicBranch(GraphicBranchOptions const& opts) : ownsEntries(opts.ownsEntries) {}

    ~GraphicBranch() override
    {
        clear();
    }

    // add a graphic to this branch.
    // Ported from: itwinjs-core GraphicBranch.add()
    void add(RenderGraphic* graphic) { entries.push_back(graphic); }

    // Number of child graphics.
    size_t size() const noexcept { return entries.size(); }

    // Get child at index.
    RenderGraphic* get(size_t index) const noexcept { return entries[index]; }

    // Type identification (no RTTI).
    bool isBranch() const noexcept override { return true; }

    // Check if this branch has any entries.
    // Ported from: itwinjs-core GraphicBranch.isEmpty
    bool isEmpty() const noexcept { return entries.empty(); }

    // clear all entries (dispose if ownsEntries).
    // Ported from: itwinjs-core GraphicBranch.clear()
    void clear()
    {
        if (ownsEntries) {
            for (auto* g : entries)
                delete g;
        }
        entries.clear();
    }

    // Get merged view flags from overrides.
    // Ported from: itwinjs-core GraphicBranch.getViewFlags()
    dqCommon::ViewFlagsProperties getViewFlags() const { return viewFlagOverrides; }

    // Set view flag overrides from a ViewFlags object.
    // Ported from: itwinjs-core GraphicBranch.setViewFlags()
    void setViewFlags(dqCommon::ViewFlagsProperties const& flags) { viewFlagOverrides = flags; }

    // Extend range to include all children.
    void unionRange(dqGeom::Range3d& range) const override
    {
        for (const auto* g : entries)
            g->unionRange(range);
    }
};

END_DQ_RENDER_NAMESPACE

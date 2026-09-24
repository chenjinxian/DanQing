// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderGraphicAdapter
// Authored: DanQing adapter bridging the public RenderGraphic hierarchy
//           (itwinjs-core core/frontend/src/render/RenderGraphic.ts) to the
//           internal Graphic scene-graph nodes used by TargetImpl.
//
// Bridges the public RenderGraphic hierarchy to the internal Graphic hierarchy.
// Wraps a public RenderGraphic (e.g., GraphicBranch from the public API) so it
// can be used as an internal Graphic node in the scene graph for TargetImpl.
//
// Note: Cannot use dynamic_cast because the project is compiled with -fno-rtti.
// Instead, we check if the RenderGraphic is also a Graphic (internal) by
// attempting a static approach.
#pragma once

#include "Graphic.h"
#include "dqRender/RenderGraphic.h"
#include "dqRender/GraphicBranch.h"

#include <cstdio>
#include <cstdlib>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// RenderGraphicAdapter — wraps a public RenderGraphic as an internal Graphic
// ---------------------------------------------------------------------------
class RenderGraphicAdapter : public Graphic {
public:
    explicit RenderGraphicAdapter(RenderGraphic* graphic) : m_graphic(graphic) {}

    void addCommands(RenderCommands& commands) override
    {
        if (!m_graphic) return;

        // If the wrapped graphic is also an internal Graphic, delegate directly.
        // Since we can't use dynamic_cast with -fno-rtti, we check via
        // a static cast attempt. This is safe because:
        // - Internal graphics (Primitive, GraphicsArray, Branch) inherit from
        //   both Graphic and RenderGraphic (via Graphic)
        // - Public graphics (GraphicBranch) only inherit from RenderGraphic
        //
        // We use a helper to check if the pointer is an internal Graphic.
        if (isInternalGraphic(m_graphic)) {
            auto* internalGraphic = static_cast<Graphic*>(m_graphic);
            internalGraphic->addCommands(commands);
            return;
        }

        // If the wrapped graphic is a GraphicBranch, iterate its entries
        auto* branch = static_cast<GraphicBranch*>(m_graphic);
        for (auto* entry : branch->entries) {
            RenderGraphicAdapter adapter(entry);
            adapter.addCommands(commands);
        }
    }

    void unionRange(dqGeom::Range3d& range) const override
    {
        if (m_graphic)
            m_graphic->unionRange(range);
    }

private:
    // Check if a RenderGraphic* is actually an internal Graphic*.
    // Since we can't use dynamic_cast with -fno-rtti, we use the virtual
    // isBranch() method on RenderGraphic.
    //
    // Inheritance hierarchy:
    //   Public API:  GraphicBranch → RenderGraphic
    //   Internal:    PlanarGridGraphic → CachedGeometry → Graphic → RenderGraphic
    //                Primitive → Graphic → RenderGraphic
    //
    // GraphicBranch::isBranch() returns true; all internal Graphics return false.
    static bool isInternalGraphic(RenderGraphic* g)
    {
        if (!g) return false;
        return !g->isBranch();
    }

    RenderGraphic* m_graphic;  // Not owned
};

// ---------------------------------------------------------------------------
// Utility: convert a public GraphicList to an internal GraphicsArray
// ---------------------------------------------------------------------------
inline std::unique_ptr<GraphicsArray> AdaptGraphicList(GraphicList const& list)
{
    auto array = std::make_unique<GraphicsArray>();
    for (auto* g : list) {
        if (!g) continue;
        array->add(std::make_unique<RenderGraphicAdapter>(g));
    }
    return array;
}

END_DQ_RENDER_NAMESPACE

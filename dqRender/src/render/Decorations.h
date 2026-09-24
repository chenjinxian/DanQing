// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — InternalDecorations container (internal)
// Ported from: itwinjs-core core/frontend/src/render/Decorations.ts
//
// Internal decorations container using internal Graphic type.
// The public Decorations class (PublicAPI/dqRender/Decorations.h) uses
// the public RenderGraphic type and is used by the Viewport.
#pragma once

#include "Graphic.h"

#include <memory>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// InternalDecorations — container for decoration graphics (internal)
// (Ported from: itwinjs-core Decorations.ts)
// Renamed from Decorations to avoid conflict with public Decorations class.
// ---------------------------------------------------------------------------
class InternalDecorations {
public:
    InternalDecorations() = default;
    ~InternalDecorations() = default;

    /// add a world-space decoration (rendered in 3D, affected by camera).
    void addWorldDecoration(std::unique_ptr<Graphic> graphic)
    {
        m_worldDecorations.push_back(std::move(graphic));
    }

    /// add a view-space decoration (rendered in 3D but always facing camera).
    void addViewDecoration(std::unique_ptr<Graphic> graphic)
    {
        m_viewDecorations.push_back(std::move(graphic));
    }

    /// add an overlay decoration (rendered on top of everything).
    void addOverlayDecoration(std::unique_ptr<Graphic> graphic)
    {
        m_overlayDecorations.push_back(std::move(graphic));
    }

    /// add all decoration commands to the render commands buffer.
    void addCommands(RenderCommands& commands)
    {
        for (auto& g : m_worldDecorations) {
            if (g) g->addCommands(commands);
        }
        for (auto& g : m_viewDecorations) {
            if (g) g->addCommands(commands);
        }
        for (auto& g : m_overlayDecorations) {
            if (g) g->addCommands(commands);
        }
    }

    /// Clear all decorations.
    void clear()
    {
        m_worldDecorations.clear();
        m_viewDecorations.clear();
        m_overlayDecorations.clear();
    }

    /// Check if any decorations exist.
    bool isEmpty() const noexcept
    {
        return m_worldDecorations.empty() && m_viewDecorations.empty() &&
               m_overlayDecorations.empty();
    }

private:
    std::vector<std::unique_ptr<Graphic>> m_worldDecorations;
    std::vector<std::unique_ptr<Graphic>> m_viewDecorations;
    std::vector<std::unique_ptr<Graphic>> m_overlayDecorations;
};

END_DQ_RENDER_NAMESPACE

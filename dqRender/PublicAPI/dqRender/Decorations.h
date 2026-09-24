// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Decorations container (public API)
//
// Ported from: itwinjs-core core/frontend/src/render/Decorations.ts
// Container for decorator graphics, categorized by rendering type.
// Used by the Viewport's DecorateContext to collect decorations from decorators,
// then passed to RenderTarget::changeDecorations() for rendering.
#pragma once

#include "Export.h"
#include "CanvasDecoration.h"
#include "GraphicBuilder.h"
#include "RenderGraphic.h"

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

#include <cstdint>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

// GraphicType is defined in GraphicBuilder.h

// Container for decorator graphics.
// Ported from: itwinjs-core Decorations.ts
//
// Holds graphics in six categories:
//   skyBox         — rendered behind everything
//   viewBackground — drawn first, view units, no z-buffer
//   normal         — drawn with z-buffer and scene lighting (GraphicType.Scene)
//   world          — drawn with z-buffer, default lighting (GraphicType.WorldDecoration)
//   worldOverlay   — overlay mode, world units (GraphicType.WorldOverlay)
//   viewOverlay    — overlay mode, view units (GraphicType.ViewOverlay)
class DQ_RENDER_EXPORT Decorations {
public:
    Decorations() = default;
    ~Decorations() = default;

    // Skybox rendered behind everything.
    // ← decorations.skyBox
    RenderGraphic* skyBox = nullptr;

    // Drawn first, view units, no z-buffer, smooth shading.
    // ← decorations.viewBackground
    RenderGraphic* viewBackground = nullptr;

    // Drawn with z-buffer and scene lighting (GraphicType.Scene).
    // ← decorations.normal
    GraphicList normal;

    // Drawn with z-buffer, default lighting, smooth shading (GraphicType.WorldDecoration).
    // ← decorations.world
    GraphicList world;

    // Overlay mode, world units (GraphicType.WorldOverlay).
    // ← decorations.worldOverlay
    GraphicList worldOverlay;

    // Overlay mode, view units (GraphicType.ViewOverlay).
    // ← decorations.viewOverlay
    GraphicList viewOverlay;

    // CanvasDecorations（2D 层装饰）——HTML canvas 2D 的 DanQing 映射，由视口 2D
    // overlay 层逐条绘制（Target.ts:1413-1424）。
    // Ported from: itwinjs-core render/Decorations.ts:25 (canvasDecorations)。
    std::vector<CanvasDecoration> canvasDecorations;

    // add a graphic to the appropriate list based on GraphicType.
    // ← DecorateContext.addDecoration(type, graphic)
    void add(GraphicType type, RenderGraphic* graphic)
    {
        switch (type) {
            case GraphicType::Scene:           normal.push_back(graphic); break;
            case GraphicType::WorldDecoration: world.push_back(graphic); break;
            case GraphicType::WorldOverlay:    worldOverlay.push_back(graphic); break;
            case GraphicType::ViewOverlay:     viewOverlay.push_back(graphic); break;
            case GraphicType::ViewBackground:  viewBackground = graphic; break;
        }
    }

    // clear all decorations.
    // ← Decorations[Symbol.dispose]()
    // 注：参考的 dispose 不含 canvasDecorations（纯 JS 对象靠 GC 回收，Decorations.ts:69-76）；
    // DanQing 的 clear() 是 CollectDecorations 的逐帧重置点（参考每帧新建 Decorations，
    // Viewport.ts:2675），故一并清空以保持"每次收集从空开始"的等价语义。
    void clear()
    {
        skyBox = nullptr;
        viewBackground = nullptr;
        normal.clear();
        world.clear();
        worldOverlay.clear();
        viewOverlay.clear();
        canvasDecorations.clear();
    }

    // Check if any decorations exist.
    bool isEmpty() const noexcept
    {
        return !skyBox && !viewBackground &&
               normal.empty() && world.empty() &&
               worldOverlay.empty() && viewOverlay.empty() &&
               canvasDecorations.empty();
    }
};

END_DQ_RENDER_NAMESPACE

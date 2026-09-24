// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Scene container
//
// Ported from: itwinjs-core core/frontend/src/render/Scene.ts
// Container for the scene graph to be rendered.
#pragma once

#include "Export.h"
#include "GraphicBranch.h"
#include "RenderGraphic.h"

#include <dqGeom/Range3d.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

// Forward declarations for internal render pipeline types.
class RenderPlanarClassifier;
class RenderTextureDrape;
class SceneVolumeClassifier;

// Container for the scene graph to be rendered.
// Ported from: itwinjs-core Scene.ts
//
// Three ordered lists of RenderGraphics:
//   foreground — drawn with z-buffer and scene lighting (normal scene content)
//   background — drawn behind all other graphics
//   overlay    — drawn on top of all other graphics
class DQ_RENDER_EXPORT Scene {
public:
    // Graphics drawn as a normal part of the scene with depth (z-buffer, scene lighting).
    // ← scene.foreground
    GraphicList foreground;

    // Graphics drawn behind all other graphics.
    // ← scene.background
    GraphicList background;

    // Graphics overlaid on top of all other graphics.
    // ← scene.overlay
    GraphicList overlay;

    // Planar classifiers for reality model classification.
    // Ported from: itwinjs-core Scene.ts planarClassifiers (@internal)
    std::unordered_map<std::string, RenderPlanarClassifier*> planarClassifiers;

    // Texture drapes for reality model draping.
    // Ported from: itwinjs-core Scene.ts textureDrapes (@internal)
    std::unordered_map<std::string, RenderTextureDrape*> textureDrapes;

    // Volume classifier for reality model classification.
    // Ported from: itwinjs-core Scene.ts volumeClassifier (@internal)
    SceneVolumeClassifier* volumeClassifier = nullptr;

    // add a graphic to the foreground list.
    void add(RenderGraphic* graphic) { foreground.push_back(graphic); }

    // Total number of graphics across all lists.
    size_t size() const noexcept { return foreground.size() + background.size() + overlay.size(); }

    // Check if the scene is empty.
    bool isEmpty() const noexcept { return size() == 0; }

    // clear all graphics from all lists.
    void clear() noexcept
    {
        foreground.clear();
        background.clear();
        overlay.clear();
        planarClassifiers.clear();
        textureDrapes.clear();
        volumeClassifier = nullptr;
    }

    // Compute the bounding range of all foreground graphics.
    dqGeom::Range3d computeRange() const
    {
        auto range = dqGeom::Range3d::CreateNull();
        for (const auto* g : foreground)
            g->unionRange(range);
        return range;
    }
};

END_DQ_RENDER_NAMESPACE

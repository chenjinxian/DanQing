// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — GltfDecoration: pickable glTF decoration
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//
// A decorator that owns glTF scene data and renders it to the viewport.
// The entire glTF is treated as a single pickable element with one feature ID.
// Matches itwinjs-core: pickableOptions { id, modelId } where id is the
// transient element ID and modelId must differ from id.
#pragma once

#include <dqApp/Decorator.h>

#include <dqRender/GltfReader.h>
#include <dqRender/RenderGraphic.h>
#include <dqRender/rhi/Handle.h>
#include <dqCommon/Image.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>

#include <QString>

#include <memory>
#include <utility>
#include <vector>

namespace dqRender { namespace rhi { class Driver; } }

namespace dqApp {

class Viewport;  // forward-decl — passed to SetScene so BuildGraphic can use the viewport's GL driver

// ---------------------------------------------------------------------------
// GltfDecoration — pickable glTF decoration
// Ported from: itwinjs-core GltfDecoration.ts (line 14)
//
// Implements IDecorator::Decorate() to add a RenderGraphic via the
// DecorateContext, matching itwinjs-core's decorate(context) pattern.
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT GltfDecoration : public IDecorator {
public:
    /// @param pickableId The feature ID for the entire glTF decoration.
    /// @param name Display name (e.g., file path or model name).
    GltfDecoration(uint32_t pickableId, QString const& name);

    ~GltfDecoration() override;

    /// Set the glTF scene to render. Builds the RenderGraphic from scene data using the
    /// viewport's RenderPipeline (the real GL driver — the global RenderSystem::get() is a
    /// no-op stub unless installed via setInstance()). ← itwinjs-core: readGltfTemplate() +
    /// createGraphicFromTemplate(); DanQing routes graphic creation through the per-viewport driver.
    void SetScene(std::unique_ptr<dqRender::GltfScene> scene, Viewport& vp);

    /// Get the glTF scene.
    dqRender::GltfScene* GetScene() const { return m_scene.get(); }

    /// Extend `range` to include the decoration graphic's world-space bounding box.
    /// Returns false if no graphic was built. `range` should be default-constructed
    /// (null) on entry.
    /// ← itwinjs-core GltfDecoration.ts:211-212 (graphic.unionRange(range))
    /// Used by loadGltf to fit the view to the imported glTF so a unit-scale model
    /// is visible (the blank connection's view is framed on a 1000-unit volume).
    bool GetGraphicRange(dqGeom::Range3d& range) const;

    /// add decorations to the context.
    /// ← itwinjs-core: GltfDecoration.decorate(context)
    /// Calls context.AddDecoration(GraphicType::Scene, m_graphic).
    void Decorate(DecorateContext& context) override;

    /// Test if this decorator owns the given feature ID.
    bool TestDecorationHit(uint32_t featureId) const override;

    /// Get tooltip for the hit decoration.
    QString GetDecorationToolTip(uint32_t featureId) const override;

    /// Get the pickable feature ID.
    uint32_t GetPickableId() const { return m_pickableId; }

    /// Get the decoration name.
    QString const& GetName() const { return m_name; }

private:
    /// Build RenderGraphic from the glTF scene data.
    void BuildGraphic(Viewport& vp);

    uint32_t m_pickableId;
    QString m_name;
    std::unique_ptr<dqRender::GltfScene> m_scene;

    // ← itwinjs-core: _graphic (the RenderGraphic)
    dqRender::RenderGraphic* m_graphic = nullptr;

    // ← itwinjs-core: graphicOwner (prevents auto-disposal on decoration change)
    dqRender::RenderGraphicOwner* m_graphicOwner = nullptr;

    // Resolved-texture cache — 1:1 GltfReader._resolvedTextures (GltfReader.ts:533):
    // meshes sharing an image decode+upload ONCE. Lifetime = this decoration
    // (longer than the consuming graphics — destroyed in ~GltfDecoration AFTER
    // the graphic dispose, so no texture dies under a live graphic; TD-14).
    // The consuming PolyfaceGraphics hold these handles with external-ownership
    // flags (never destroy them).
    std::vector<std::pair<dqRender::rhi::TextureHandle,
                          std::unique_ptr<dqCommon::ImageBuffer>>> m_resolvedTextures;
    dqRender::rhi::Driver* m_textureCacheDriver = nullptr;
};

}  // namespace dqApp

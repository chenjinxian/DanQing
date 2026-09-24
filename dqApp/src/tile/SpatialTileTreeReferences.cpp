// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — spatial tile tree references implementation
// Ported from: itwinjs-core core/frontend/src/internal/tile/PrimaryTileTree.ts
#include "dqApp/tile/SpatialTileTreeReferences.h"

#include "dqApp/Viewport.h"
#include "dqApp/IModelConnection.h"
#include "dqApp/ViewState.h"
#include "dqApp/tile/SceneContext.h"
#include "dqApp/tile/PrimaryTileTreeSupplier.h"
#include "dqApp/tile/Tiles.h"
#include "dqApp/tile/TiledGraphicsProvider.h"

BEGIN_DQ_APP_NAMESPACE

SpatialTileTreeReferences::CreateFn SpatialTileTreeReferences::s_createOverride = nullptr;

namespace {

// Default: per-model primary tree references driven by the view's model
// selector (PrimaryTileTree.ts:608-705 SpatialModelRefs minimal form — one
// PrimaryTileTreeReference per selected model; animated/section-cut refs and
// exclusion arrive with their features).
// The supplier is process-wide (the reference's is a module singleton,
// PrimaryTileTree.ts:49-51).
PrimaryTileTreeSupplier& primaryTileTreeSupplier()
{
    static PrimaryTileTreeSupplier supplier;
    return supplier;
}

class ModelSelectorSpatialTileTreeReferences final : public SpatialTileTreeReferences {
public:
    explicit ModelSelectorSpatialTileTreeReferences(SpatialViewState& view);

    void forEachTileTreeRef(
        std::function<void(TileTreeReference&)> const& func) const override;

private:
    SpatialViewState& m_view;
};

ModelSelectorSpatialTileTreeReferences::ModelSelectorSpatialTileTreeReferences(
    SpatialViewState& view)
    : m_view(view)
{
}

void ModelSelectorSpatialTileTreeReferences::forEachTileTreeRef(
    std::function<void(TileTreeReference&)> const& func) const
{
    // One reference per selected model (SpatialRefs iteration,
    // PrimaryTileTree.ts:117-124 + SpatialModelRefs :622).
    // The tree id encodes the model's root props for the supplier (see
    // PrimaryTileTreeSupplier.h — stands in for requestTileTreeProps).
    auto const& models = m_view.GetModelSelector().getModels();
    if (models.empty())
        return;

    IModelConnection* iModel = m_view.GetIModel();
    if (!iModel)
        return;

    auto& supplier = primaryTileTreeSupplier();
    for (auto const& modelId : models) {
        // Default root props: project-extents-sized root at depth 0.
        // The per-model geometry extents arrive with the model-state layer
        // (registered — until then a unit-scaled root keeps the seam alive).
        std::ostringstream id;
        id << modelId.ToString() << "|0/0/0/0|"
           << "-100,-100,-100,100,100,100|"
           << "-100,-100,-100,100,100,100|"
           << 512 << "|3d";
        TileTreeOwner& owner = iModel->GetTiles().getTileTreeOwner(id.str(), supplier);
        PrimaryTileTreeReference ref(owner);
        func(ref);
    }
}

}  // namespace

std::unique_ptr<SpatialTileTreeReferences> SpatialTileTreeReferences::create(SpatialViewState& view)
{
    // Ported from: SpatialTileTreeReferences.create (PrimaryTileTree.ts
    // :601-606 — `create` is the assignment target frontend-tiles replaces).
    if (s_createOverride)
        return s_createOverride(view);
    return std::make_unique<ModelSelectorSpatialTileTreeReferences>(view);
}

// ---------------------------------------------------------------------------
// TiledGraphicsProviders helpers (TiledGraphicsProvider.ts:47-85)
// ---------------------------------------------------------------------------

namespace TiledGraphicsProviders {

void addToScene(TiledGraphicsProvider& provider, SceneContext& context)
{
    // Ported from: TiledGraphicsProvider.addToScene (:49-54).
    provider.forEachTileTreeRef(context.getViewport(),
                                [&context](TileTreeReference& ref) { ref.addToScene(context); });
}

bool isLoadingComplete(TiledGraphicsProvider& provider, Viewport& viewport)
{
    // Ported from: TiledGraphicsProvider.isLoadingComplete (:57-68).
    if (!provider.isLoadingCompleteOverride(viewport))
        return false;

    bool allLoaded = true;
    provider.forEachTileTreeRef(viewport, [&allLoaded](TileTreeReference& ref) {
        allLoaded = allLoaded && ref.isLoadingComplete();
    });
    return allLoaded;
}

std::vector<TileTreeReference*> getTileTreeRefs(TiledGraphicsProvider& provider,
                                                Viewport& viewport)
{
    // Ported from: TiledGraphicsProvider.getTileTreeRefs (:73-84).
    std::vector<TileTreeReference*> refs;
    provider.forEachTileTreeRef(viewport, [&refs](TileTreeReference& ref) {
        refs.push_back(&ref);
    });
    return refs;
}

}  // namespace TiledGraphicsProviders

END_DQ_APP_NAMESPACE

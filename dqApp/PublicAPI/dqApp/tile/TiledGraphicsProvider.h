// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — application channel for injecting tile tree references
// Ported from: itwinjs-core core/frontend/src/tile/TiledGraphicsProvider.ts
#pragma once

#include "../Export.h"
#include "TileTreeReference.h"

#include <functional>
#include <vector>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

class SceneContext;
class Viewport;

// Provides a way for applications to inject additional non-decorative
// graphics into a Viewport by supplying TileTreeReferences.
// Ported from: itwinjs-core TiledGraphicsProvider (TiledGraphicsProvider.ts:20-44).
class DQ_APP_EXPORT TiledGraphicsProvider {
public:
    virtual ~TiledGraphicsProvider() = default;

    /// For each TileTreeReference belonging to this provider that should be
    /// drawn in the specified viewport, invoke `func`.
    /// Ported from: TiledGraphicsProvider.forEachTileTreeRef (:26).
    virtual void forEachTileTreeRef(
        Viewport& viewport, std::function<void(TileTreeReference&)> const& func) const = 0;

    /// If defined, overrides the logic for adding this provider's graphics
    /// into the scene (otherwise addToScene is invoked per reference).
    /// Ported from: TiledGraphicsProvider.addToScene? (:36).
    virtual void addToSceneOverride(SceneContext& /*context*/) {}

    /// If defined, returns whether the provider's trees are loaded and ready
    /// to draw (otherwise TileTreeReference.isLoadingComplete per reference).
    /// Ported from: TiledGraphicsProvider.isLoadingComplete? (:43).
    virtual bool isLoadingCompleteOverride(Viewport& /*viewport*/) const { return true; }
};

// Namespace helpers over providers (TiledGraphicsProvider.ts:47-85).
namespace TiledGraphicsProviders {

/// Add the provider's graphics to the scene.
/// Ported from: TiledGraphicsProvider.addToScene (:49-54).
DQ_APP_EXPORT void addToScene(TiledGraphicsProvider& provider, SceneContext& context);

/// Whether all of the provider's referenced trees are loaded.
/// Ported from: TiledGraphicsProvider.isLoadingComplete (:57-68).
DQ_APP_EXPORT bool isLoadingComplete(TiledGraphicsProvider& provider, Viewport& viewport);

/// Collect the provider's references.
/// Ported from: TiledGraphicsProvider.getTileTreeRefs (:73-84).
DQ_APP_EXPORT std::vector<TileTreeReference*> getTileTreeRefs(
    TiledGraphicsProvider& provider, Viewport& viewport);

}  // namespace TiledGraphicsProviders

END_DQ_APP_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — spatial tile tree references (overridable factory seam)
// Ported from: itwinjs-core core/frontend/src/internal/tile/PrimaryTileTree.ts
//              (SpatialTileTreeReferences namespace, :598-606)
//
// This is THE seam the reference's frontend-tiles package replaces:
//   SpatialTileTreeReferences.create = (view) => createBatchedSpatialTileTreeReferences(...)
//   (FrontendTiles.ts:215). The TS overridable namespace function maps to a
// settable std::function factory in C++ (setCreateOverride / clearCreateOverride).
//
// DanQing adaptation (registered): the reference's default factory produces the
// model-selector-driven SpatialRefs family (PrimaryTileTree.ts:608-861,
// per-model PrimaryTreeReference from GeometricModelState); DanQing has no
// per-model tile production yet, so the default yields an EMPTY reference
// set — spatial content arrives via TiledGraphicsProvider (application
// injection) or a factory override (frontend-tiles pattern).
#pragma once

#include "../Export.h"
#include "TileTreeReference.h"

#include <functional>
#include <memory>
#include <vector>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

class SpatialViewState;
class Viewport;

// Provides the TileTreeReferences for a SpatialViewState.
// Ported from: itwinjs-core SpatialTileTreeReferences interface
// (PrimaryTileTree.ts:570-596).
class DQ_APP_EXPORT SpatialTileTreeReferences {
public:
    virtual ~SpatialTileTreeReferences() = default;

    /// Invoke `func` for every tile tree reference of the view.
    virtual void forEachTileTreeRef(
        std::function<void(TileTreeReference&)> const& func) const = 0;

    /// Re-evaluate the references (model selector / display style changes).
    /// Ported from: SpatialTileTreeReferences.update (:126-131).
    virtual void update() {}

    // Attach/detach viewport-side listeners — DanQing has none yet (the
    // reference wires model-selector and display-style listeners here).
    virtual void attachToViewport(Viewport& /*viewport*/) {}
    virtual void detachFromViewport() {}

    // --- Overridable factory seam (PrimaryTileTree.ts:601-606) ---
    using CreateFn =
        std::function<std::unique_ptr<SpatialTileTreeReferences>(SpatialViewState&)>;

    /// Create a SpatialTileTreeReferences object reflecting the contents of
    /// the specified view. Honors the override when one is installed.
    /// Ported from: SpatialTileTreeReferences.create (PrimaryTileTree.ts:603-605).
    static std::unique_ptr<SpatialTileTreeReferences> create(SpatialViewState& view);

    /// Replace the factory (frontend-tiles pattern, FrontendFiles.ts:215).
    static void setCreateOverride(CreateFn fn) { s_createOverride = std::move(fn); }

    /// Restore the default factory (tests must clean up).
    static void clearCreateOverride() { s_createOverride = nullptr; }

    static bool hasCreateOverride() noexcept { return s_createOverride != nullptr; }

private:
    static CreateFn s_createOverride;
};

END_DQ_APP_NAMESPACE

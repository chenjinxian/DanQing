// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — tile tree owner and supplier interfaces
// Ported from: itwinjs-core core/frontend/src/tile/TileTreeOwner.ts
//              core/frontend/src/tile/TileTreeSupplier.ts
//
// DanQing adaptations (registered):
//  - `iModel` omitted from TileTreeOwner: DanQing's tile trees carry no
//    iModel association yet (RealityTileTree has none); the reference uses
//    it for ECEF-dependent purging which has no DanQing counterpart.
//  - createTileTree is synchronous: the reference returns a Promise
//    (TreeOwner._load awaits it); DanQing's tree creation (tileset bytes →
//    tree) is synchronous today, so the NotLoaded→Loading→Loaded sequence
//    completes within one load() call. TODO(async): revisit when tree
//    loading gains an async stage.
#pragma once

#include "../Export.h"

#include <dqRender/tile/TileTree.h>

#include <functional>
#include <memory>
#include <string>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

class IModelConnection;

// ---------------------------------------------------------------------------
// TileTreeOwner — owns and manages the lifecycle of a TileTree. It is in
// turn owned by an IModelConnection's Tiles registry.
// Ported from: itwinjs-core TileTreeOwner (TileTreeOwner.ts:19-48)
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT TileTreeOwner {
public:
    virtual ~TileTreeOwner() = default;

    /// The owned TileTree, or nullptr if the tile tree is not loaded.
    /// @note Do not store a direct reference to the TileTree — it may be
    /// disposed by its owner. Use load() to ensure loading.
    virtual dqRender::TileTree* getTileTree() const noexcept = 0;

    /// The current load state of the tree.
    virtual dqRender::TileTreeLoadStatus getLoadStatus() const noexcept = 0;

    /// If the tree has not yet been loaded, load it (NotLoaded → Loading →
    /// Loaded/NotFound). Returns the loaded tree, or nullptr while loading
    /// or after failure.
    /// Ported from: TileTreeOwner.load (TileTreeOwner.ts:35-39).
    virtual dqRender::TileTree* load() = 0;

    /// Dispose the owned tree and reset to NotLoaded.
    /// Ported from: TileTreeOwner[Symbol.dispose] (TileTreeOwner.ts:41-44).
    virtual void dispose() = 0;
};

// Tree identifier: unique within a supplier. The reference types this as
// `any` (supplier-private); DanQing uses a string (RealityTree id = tileset
// URL; composite ids serialize — see ContentIdProvider precedent).
using TileTreeId = std::string;

// ---------------------------------------------------------------------------
// TileTreeSupplier — supplies TileTrees for rendering. A supplier can supply
// any number of trees; each has a unique (supplier-private) id.
// Ported from: itwinjs-core TileTreeSupplier (TileTreeSupplier.ts:22-48)
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT TileTreeSupplier {
public:
    virtual ~TileTreeSupplier() = default;

    /// Compare two tree ids (negative/zero/positive — strict weak order).
    /// The ids must be treated as immutable (they are lookup keys).
    /// Ported from: TileTreeSupplier.compareTileTreeIds (TileTreeSupplier.ts:24).
    virtual int compareTileTreeIds(TileTreeId const& lhs, TileTreeId const& rhs) const = 0;

    /// Produce the TileTree corresponding to the specified id.
    /// Ported from: TileTreeSupplier.createTileTree (TileTreeSupplier.ts:27 —
    /// Promise<TileTree|undefined>; synchronous in DanQing, see header note).
    virtual std::unique_ptr<dqRender::TileTree> createTileTree(
        TileTreeId const& id, IModelConnection& iModel) = 0;
};

END_DQ_APP_NAMESPACE

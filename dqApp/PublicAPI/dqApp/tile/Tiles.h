// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — tile tree registry for an IModelConnection
// Ported from: itwinjs-core core/frontend/src/Tiles.ts
//
// DanQing adaptations (registered):
//  - Ordered Dictionary keyed by supplier-id comparator → std::map with a
//    transparent comparator over (supplier, id).
//  - ECEF/project-extents listeners (Tiles.ts:89-100) omitted — no DanQing
//    counterpart events yet.
//  - schedule-script/spatial-model aggregation (addModelsAnimatedByScript /
//    addSpatialModels, :124-160) omitted — no schedule script support.
#pragma once

#include "../Export.h"
#include "TileTreeOwner.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

class IModelConnection;

// ---------------------------------------------------------------------------
// Tiles — provides access to the TileTrees associated with an
// IModelConnection, indirectly via TileTreeOwners.
// Ported from: itwinjs-core Tiles (Tiles.ts:77-246)
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT Tiles {
public:
    explicit Tiles(IModelConnection& iModel);

    /// Obtain the owner of a TileTree. The id is unique within the supplier;
    /// a TileTreeReference uses this to obtain the tree it refers to.
    /// Ported from: Tiles.getTileTreeOwner (Tiles.ts:166-180).
    TileTreeOwner& getTileTreeOwner(TileTreeId const& id, TileTreeSupplier& supplier);

    /// Remove the tile tree owner matching the supplier/id (disposes its tree).
    /// Ported from: Tiles.resetTileTreeOwner (Tiles.ts:185-192).
    void resetTileTreeOwner(TileTreeId const& id, TileTreeSupplier& supplier);

    /// Dispose all TileTrees belonging to the supplier and forget it.
    /// Ported from: Tiles.dropSupplier (Tiles.ts:195-202).
    void dropSupplier(TileTreeSupplier& supplier);

    /// Invoke a function on each extant TileTreeOwner.
    /// Ported from: Tiles.forEachTreeOwner (Tiles.ts:205-208).
    void forEachTreeOwner(std::function<void(TileTreeOwner&)> const& func) const;

    /// Dispose every owner (tests / connection teardown).
    /// Ported from: Tiles.reset (Tiles.ts:112-117).
    void reset();

private:
    // TreeOwner — the TileTreeOwner implementation (Tiles.ts:14-67).
    class TreeOwner final : public TileTreeOwner {
    public:
        TreeOwner(TileTreeId id, TileTreeSupplier& supplier, IModelConnection& iModel);

        dqRender::TileTree* getTileTree() const noexcept override { return m_tileTree.get(); }
        dqRender::TileTreeLoadStatus getLoadStatus() const noexcept override { return m_loadStatus; }

        dqRender::TileTree* load() override;
        void dispose() override;

    private:
        // Ported from: TreeOwner._load (Tiles.ts:47-66 — NotLoaded → Loading
        // → Loaded/NotFound; ServerTimeout retries as NotLoaded. DanQing's
        // createTileTree is synchronous, so the sequence completes in one
        // load(); onTileTreeLoad is raised when it lands).
        void doLoad();

        TileTreeId m_id;
        TileTreeSupplier& m_supplier;
        IModelConnection& m_iModel;
        std::unique_ptr<dqRender::TileTree> m_tileTree;
        dqRender::TileTreeLoadStatus m_loadStatus = dqRender::TileTreeLoadStatus::NotLoaded;
    };

    struct SupplierEntry {
        TileTreeSupplier* supplier;  // not owned — outlives the registry per contract
        std::map<TileTreeId, std::unique_ptr<TreeOwner>> owners;
    };

    IModelConnection& m_iModel;
    // unique_ptr indirection: vector growth must move, never copy (map's
    // move ctor is not noexcept, so _Move_if_noexcept would fall back to a
    // copy of unique_ptr owners — deleted).
    std::vector<std::unique_ptr<SupplierEntry>> m_treesBySupplier;
};

END_DQ_APP_NAMESPACE

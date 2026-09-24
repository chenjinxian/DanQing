// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — tile tree registry implementation
// Ported from: itwinjs-core core/frontend/src/Tiles.ts
#include "dqApp/tile/Tiles.h"

#include "dqApp/IModelConnection.h"
#include <dqRender/tile/TileAdmin.h>

#include <algorithm>

BEGIN_DQ_APP_NAMESPACE

// ---------------------------------------------------------------------------
// Tiles::TreeOwner
// ---------------------------------------------------------------------------

Tiles::TreeOwner::TreeOwner(TileTreeId id, TileTreeSupplier& supplier,
                            IModelConnection& iModel)
    : m_id(std::move(id))
    , m_supplier(supplier)
    , m_iModel(iModel)
{
}

void Tiles::TreeOwner::doLoad()
{
    // Ported from: TreeOwner._load (Tiles.ts:47-66).
    if (m_loadStatus != dqRender::TileTreeLoadStatus::NotLoaded)
        return;

    m_loadStatus = dqRender::TileTreeLoadStatus::Loading;

    std::unique_ptr<dqRender::TileTree> tree;
    dqRender::TileTreeLoadStatus newStatus;
    tree = m_supplier.createTileTree(m_id, m_iModel);
    // Reference: a tree with null root content range counts as NotFound
    // (Tiles.ts:56). DanQing's trees compute ranges at parse time — a null
    // root range means the same "nothing to show".
    newStatus = tree && !tree->getRootTile()->getRange().isNull()
        ? dqRender::TileTreeLoadStatus::Loaded
        : dqRender::TileTreeLoadStatus::NotFound;

    if (m_loadStatus == dqRender::TileTreeLoadStatus::Loading) {
        // NotFound keeps no tree (reference: tree stays undefined — a null
        // root range means "nothing to show", the object is discarded).
        m_tileTree = newStatus == dqRender::TileTreeLoadStatus::Loaded
            ? std::move(tree)
            : nullptr;
        m_loadStatus = newStatus;
        // Reference raises with the OWNER regardless of outcome (Tiles.ts:64
        // raiseEvent(this)); DanQing's event signature is TileTree&, so the
        // raise is limited to the Loaded case (registered adaptation — a
        // NotFound owner has no tree to pass).
        if (m_tileTree && dqRender::TileAdmin::hasInstance())
            dqRender::TileAdmin::instance().onTileTreeLoad.Raise(*m_tileTree);
    }
}

dqRender::TileTree* Tiles::TreeOwner::load()
{
    doLoad();
    return getTileTree();
}

void Tiles::TreeOwner::dispose()
{
    // Ported from: TreeOwner[Symbol.dispose] (Tiles.ts:42-45).
    m_tileTree.reset();
    m_loadStatus = dqRender::TileTreeLoadStatus::NotLoaded;
}

// ---------------------------------------------------------------------------
// Tiles
// ---------------------------------------------------------------------------

Tiles::Tiles(IModelConnection& iModel)
    : m_iModel(iModel)
{
}

TileTreeOwner& Tiles::getTileTreeOwner(TileTreeId const& id, TileTreeSupplier& supplier)
{
    // Ported from: Tiles.getTileTreeOwner (Tiles.ts:166-180 — one dictionary
    // per supplier, one owner per id).
    SupplierEntry* entry = nullptr;
    for (auto& e : m_treesBySupplier) {
        if (e->supplier == &supplier) {
            entry = e.get();
            break;
        }
    }
    if (!entry) {
        entry = m_treesBySupplier.emplace_back(
            std::make_unique<SupplierEntry>(SupplierEntry{&supplier, {}})).get();
    }

    auto it = entry->owners.find(id);
    if (it == entry->owners.end()) {
        it = entry->owners.emplace(
            id, std::make_unique<TreeOwner>(id, supplier, m_iModel)).first;
    }
    return *it->second;
}

void Tiles::resetTileTreeOwner(TileTreeId const& id, TileTreeSupplier& supplier)
{
    // Ported from: Tiles.resetTileTreeOwner (Tiles.ts:185-192).
    for (auto& e : m_treesBySupplier) {
        if (e->supplier != &supplier)
            continue;
        auto it = e->owners.find(id);
        if (it != e->owners.end())
            e->owners.erase(it);
        return;
    }
}

void Tiles::dropSupplier(TileTreeSupplier& supplier)
{
    // Ported from: Tiles.dropSupplier (Tiles.ts:195-202).
    m_treesBySupplier.erase(
        std::remove_if(m_treesBySupplier.begin(), m_treesBySupplier.end(),
                       [&](std::unique_ptr<SupplierEntry> const& e) { return e->supplier == &supplier; }),
        m_treesBySupplier.end());
}

void Tiles::forEachTreeOwner(std::function<void(TileTreeOwner&)> const& func) const
{
    // Ported from: Tiles.forEachTreeOwner (Tiles.ts:205-208).
    for (auto const& e : m_treesBySupplier)
        for (auto const& kv : e->owners)
            func(*kv.second);
}

void Tiles::reset()
{
    // Ported from: Tiles.reset (Tiles.ts:112-117).
    m_treesBySupplier.clear();
}

END_DQ_APP_NAMESPACE

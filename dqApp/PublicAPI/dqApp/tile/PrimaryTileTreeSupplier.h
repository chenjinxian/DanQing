// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — primary tile tree supplier (per-model production)
// Ported from: itwinjs-core core/frontend/src/internal/tile/PrimaryTileTree.ts
//              (PrimaryTreeSupplier :53-98 — createTileTree from tree props;
//              PrimaryTreeReference :155-195 — the per-model reference)
//
// DanQing adaptation (registered): the reference's tree props arrive via the
// requestTileTreeProps RPC (TileAdmin.ts:648-657); DanQing has no backend, so
// the tree id ENCODES the root props ("<modelId>|<contentId>|<x0,y0,z0,x1,y1,z1>|<cx,cy,cz,...>|<tileScreenSize>|<is2d>")
// — test/asset side composes it. The RPC plumbing replaces the encoding when
// a backend lands.
#pragma once

#include "../Export.h"
#include "TileTreeReference.h"

#include <string>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

class IModelConnection;

// Supplies the primary (per-model) imdl tile trees.
// Ported from: PrimaryTreeSupplier (PrimaryTileTree.ts:53-98).
class DQ_APP_EXPORT PrimaryTileTreeSupplier : public TileTreeSupplier {
public:
    int compareTileTreeIds(TileTreeId const& lhs, TileTreeId const& rhs) const override;

    // Decode the id-encoded root props and build the ImdlTileTree.
    // Ported from: PrimaryTreeSupplier.createTileTree (:63-98 — the
    // reference awaits requestTileTreeProps then constructs IModelTileTree).
    std::unique_ptr<dqRender::TileTree> createTileTree(
        TileTreeId const& id, IModelConnection& iModel) override;
};

// A per-model primary tree reference.
// Ported from: PrimaryTreeReference (PrimaryTileTree.ts:155-195 — holds the
// owner; computeTransform from tree.iModelTransform).
class DQ_APP_EXPORT PrimaryTileTreeReference final : public TileTreeReference {
public:
    explicit PrimaryTileTreeReference(TileTreeOwner& owner)
        : m_owner(owner)
    {
    }

    TileTreeOwner& getTreeOwner() override { return m_owner; }

private:
    TileTreeOwner& m_owner;
};

END_DQ_APP_NAMESPACE

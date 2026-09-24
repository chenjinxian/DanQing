// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — scaffold bridge: a caller-owned tree as a TileTreeReference
// Ported from: itwinjs-core core/frontend/src/tile/RenderGraphicTileTree.ts
//              (Supplier/GraphicTree pattern — wrapping a ready-made tree
//              behind the owner/reference seam)
//
// Bridge for Viewport::AddTileTree (the temporary scaffold): wraps each
// caller-owned tree in the faithful owner/reference chain so the scaffold's
// content flows through the same Viewport → refs/providers → addToScene path
// as real references. The DirectOwner holds nothing — the tree is caller-
// owned per AddTileTree's contract (must outlive the viewport).
#pragma once

#include "../Export.h"
#include "TileTreeOwner.h"
#include "TileTreeReference.h"

#include <dqRender/tile/TileTree.h>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

// A TileTreeOwner over a caller-owned tree (no lifecycle authority).
class DQ_APP_EXPORT DirectTileTreeOwner final : public TileTreeOwner {
public:
    explicit DirectTileTreeOwner(dqRender::TileTree* tree) noexcept
        : m_tree(tree)
    {
    }

    dqRender::TileTree* getTileTree() const noexcept override { return m_tree; }
    dqRender::TileTreeLoadStatus getLoadStatus() const noexcept override
    {
        return m_tree ? dqRender::TileTreeLoadStatus::Loaded
                      : dqRender::TileTreeLoadStatus::NotFound;
    }
    dqRender::TileTree* load() override { return m_tree; }
    void dispose() override
    {
        // Caller-owned; teardown is handled by Viewport::Shutdown's
        // freeContents pass while the GL driver is still alive.
    }

private:
    dqRender::TileTree* m_tree;  // not owned
};

// A TileTreeReference to one caller-owned tree.
class DQ_APP_EXPORT SimpleTileTreeReference final : public TileTreeReference {
public:
    explicit SimpleTileTreeReference(dqRender::TileTree* tree)
        : m_owner(tree)
    {
    }

    TileTreeOwner& getTreeOwner() override { return m_owner; }

private:
    DirectTileTreeOwner m_owner;
};

END_DQ_APP_NAMESPACE

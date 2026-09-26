// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — iModel tile tree (per-model production)
// Ported from: itwinjs-core core/frontend/src/internal/tile/IModelTileTree.ts
//              (tree shell + maxDepth=32 :418)
//              core/common/src/tile/TileMetadata.ts computeChildTileProps
//              (:777-853 — pure-frontend child subdivision) and ContentIdProvider
//              V1 scheme (:665-674 — "d/i/j/k/mult" components).
#pragma once

#include "../Export.h"
#include "Tile.h"
#include "TileDrawArgs.h"
#include "TileTree.h"

#include <dqGeom/Range3d.h>

#include <memory>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Tile metadata for child computation (the subset the subdivision reads).
// Ported from: itwinjs-core TileMetadata (TileMetadata.ts:750-775 subset).
struct ImdlTileMetadata {
    std::string contentId;
    dqGeom::Range3d range;
    dqGeom::Range3d contentRange;
    bool isLeaf = false;
    double sizeMultiplier = 0.0;   // 0 = not set (magnification branch off)
    uint32_t emptySubRangeMask = 0;
};

// Tree-level metadata for child computation.
// Ported from: itwinjs-core TileTreeMetadata (:760-775 subset).
struct ImdlTreeMetadata {
    dqGeom::Range3d contentRange;  // model range (empty = unknown)
    uint32_t tileScreenSize = 512;  // TileProps.ts:66 default
    bool is2d = false;
};

// Content-id components — V1 scheme (ContentIdProvider, TileMetadata.ts:665-674:
// "d"epth / "i","j","k" sub-volume indices / "mult"iplier, "/"-separated).
struct DQ_RENDER_EXPORT ImdlContentIdSpec {
    uint32_t depth = 0;
    uint32_t i = 0, j = 0, k = 0;
    uint32_t mult = 0;  // 0 = no multiplier component
};

// Compute the children of a tile: magnification branch (one child, same
// volume, 2x multiplier) or 3d/2d bisection (8/4 children with empty-mask
// and model-range rejection).
// Ported from: computeChildTileProps (TileMetadata.ts:777-853).
struct DQ_RENDER_EXPORT ImdlChildTileProps {
    std::string contentId;
    dqGeom::Range3d range;
    bool isLeaf = false;
    double sizeMultiplier = 0.0;
    // Every child's maximumSize = root.tileScreenSize (:795 magnification /
    // :847 subdivision — the SSE criterion's per-tile operand,
    // Tile.ts:459-463).
    double maximumSize = 0.0;
};
std::vector<ImdlChildTileProps> DQ_RENDER_EXPORT
computeImdlChildTileProps(ImdlTileMetadata const& parent,
                          ImdlTreeMetadata const& root);

// Parse/format a V1 content id ("<depth>/<i>/<j>/<k>" [+ "/<mult>"]).
// Ported from: ContentIdProvider V1 (TileMetadata.ts:665-674).
ImdlContentIdSpec DQ_RENDER_EXPORT parseImdlContentId(std::string const& id);
std::string DQ_RENDER_EXPORT formatImdlContentId(ImdlContentIdSpec const& spec);

// ---------------------------------------------------------------------------
// ImdlTile — a tile of an ImdlTileTree.
// Ported from: itwinjs-core IModelTile (IModelTile.ts:56 — contentId-carrying
// tile; content acquisition goes through the TileAdmin fetcher with a URL
// composed from treeId + contentId — the RPC layer's TODO stands in for the
// reference's generateTileContent).
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT ImdlTileTree;

class DQ_RENDER_EXPORT ImdlTile : public Tile {
public:
    ImdlTile(ImdlTileTree& tree, Tile* parent,
             std::string contentId, dqGeom::Range3d const& range,
             double sizeMultiplier, double maximumSize);

    std::string const& getContentId() const noexcept { return m_contentId; }
    double getSizeMultiplier() const noexcept { return m_sizeMultiplier; }

    // Ported from: itwinjs-core IModelTile.maximumSize (IModelTile.ts:81-83)
    // — super.maximumSize * (this.sizeMultiplier ?? 1.0). The reference's
    // sizeMultiplier is `number | undefined`; DanQing's 0.0 stands for
    // "not set" (ImdlTileMetadata.sizeMultiplier same convention).
    double getMaximumSize() const noexcept override
    {
        return Tile::getMaximumSize()
               * (m_sizeMultiplier > 0.0 ? m_sizeMultiplier : 1.0);
    }

    // --- Tile overrides ---
    bool requestContent() override;
    TileContent readContent(uint8_t const* data, size_t dataSize) override;
    void loadChildren() override;
    bool hasContent() const noexcept override { return !m_contentId.empty(); }

private:
    std::string m_contentId;
    double m_sizeMultiplier = 0.0;
};

// ---------------------------------------------------------------------------
// ImdlTileTree — per-model imdl tree.
// Ported from: IModelTileTree (IModelTileTree.ts:354-460 subset — the
// constructor receives root tile props; maxDepth 32).
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT ImdlTileTree : public TileTree {
public:
    static constexpr uint32_t kMaxDepth = 32;  // IModelTileTree.ts:418

    // treeId identifies the tree (model + options); the root's content id
    // comes from the tree props (the reference's requestTileTreeProps RPC —
    // DanQing's props arrive via the supplier's id encoding).
    ImdlTileTree(std::string treeId, std::string rootContentId,
                 dqGeom::Range3d const& rootRange,
                 ImdlTreeMetadata treeMetadata);

    std::string const& getTreeId() const noexcept { return m_treeId; }
    ImdlTreeMetadata const& metadata() const noexcept { return m_metadata; }

    TileVisibility computeVisibility(TileDrawArgs& args, Tile* tile) override;

    // Child computation for a tile of this tree (computeImdlChildTileProps
    // with this tree's metadata; IModelTile._loadChildren :153-169).
    std::vector<ImdlChildTileProps> childPropsFor(ImdlTileMetadata const& parent) const
    {
        return computeImdlChildTileProps(parent, m_metadata);
    }

    // Content URL: <treeId>/<contentId> (the fetch layer's composition point —
    // the reference routes through generateTileContent RPC / tile cache).
    std::string contentUrl(std::string const& contentId) const;

private:
    std::string m_treeId;
    ImdlTreeMetadata m_metadata;
};

END_DQ_RENDER_NAMESPACE

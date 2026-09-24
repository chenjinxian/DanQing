// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — disclosed tile tree set
// Ported from: itwinjs-core core/frontend/src/tile/DisclosedTileTreeSet.ts
#pragma once

#include "../Export.h"

#include <dqRender/tile/TileTree.h>

#include <unordered_set>
#include <vector>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

class DisclosedTileTreeSet;

// Interface adopted by an object that contains references to TileTrees.
// Ported from: itwinjs-core TileTreeDiscloser (DisclosedTileTreeSet.ts:17-20).
class TileTreeDiscloser {
public:
    virtual ~TileTreeDiscloser() = default;
    virtual void discloseTileTrees(DisclosedTileTreeSet& trees) = 0;
};

// A set of TileTrees disclosed by TileTreeDisclosers, used to collect the
// trees in use. Any tree NOT disclosed becomes a candidate for purging.
// Ported from: itwinjs-core DisclosedTileTreeSet (DisclosedTileTreeSet.ts:26-73).
class DQ_APP_EXPORT DisclosedTileTreeSet {
public:
    bool has(dqRender::TileTree const* tree) const;
    void add(dqRender::TileTree* tree);
    std::vector<dqRender::TileTree*> const& trees() const noexcept { return m_trees; }
    size_t size() const noexcept { return m_trees.size(); }

    /// Add all tile trees referenced by `discloser` (each discloser processed
    /// at most once — reference semantics, DisclosedTileTreeSet.ts:60-66).
    void disclose(TileTreeDiscloser& discloser);

    void clear();

private:
    std::unordered_set<TileTreeDiscloser*> m_processed;
    std::vector<dqRender::TileTree*> m_trees;
    std::unordered_set<dqRender::TileTree*> m_members;
};

END_DQ_APP_NAMESPACE

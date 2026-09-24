// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — disclosed tile tree set implementation
// Ported from: itwinjs-core core/frontend/src/tile/DisclosedTileTreeSet.ts
#include "dqApp/tile/DisclosedTileTreeSet.h"

BEGIN_DQ_APP_NAMESPACE

bool DisclosedTileTreeSet::has(dqRender::TileTree const* tree) const
{
    return m_members.find(const_cast<dqRender::TileTree*>(tree)) != m_members.end();
}

void DisclosedTileTreeSet::add(dqRender::TileTree* tree)
{
    if (m_members.insert(tree).second)
        m_trees.push_back(tree);
}

void DisclosedTileTreeSet::disclose(TileTreeDiscloser& discloser)
{
    // Ported from: DisclosedTileTreeSet.disclose (:60-66) — each discloser
    // processed at most once.
    if (m_processed.insert(&discloser).second)
        discloser.discloseTileTrees(*this);
}

void DisclosedTileTreeSet::clear()
{
    m_processed.clear();
    m_trees.clear();
    m_members.clear();
}

END_DQ_APP_NAMESPACE

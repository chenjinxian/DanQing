// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — SelectionSet and HiliteSet implementation
// Ported from: itwinjs-core core/frontend/src/SelectionSet.ts
#include "dqApp/SelectionSet.h"

namespace dqApp {

// ---------------------------------------------------------------------------
// SelectionSet
// ---------------------------------------------------------------------------
void SelectionSet::add(QSet<uint32_t> const& ids)
{
    m_elements.unite(ids);
    OnChanged.Raise(this);
}

void SelectionSet::Remove(QSet<uint32_t> const& ids)
{
    for (auto id : ids) {
        m_elements.remove(id);
    }
    OnChanged.Raise(this);
}

void SelectionSet::Replace(QSet<uint32_t> const& ids)
{
    m_elements = ids;
    OnChanged.Raise(this);
}

void SelectionSet::EmptyAll()
{
    if (m_elements.isEmpty()) return;
    m_elements.clear();
    OnChanged.Raise(this);
}

// ---------------------------------------------------------------------------
// HiliteSet
// ---------------------------------------------------------------------------
void HiliteSet::SyncWith(SelectionSet const& selection)
{
    m_elements = selection.GetElements();
}

void HiliteSet::clear()
{
    m_elements.clear();
}

}  // namespace dqApp

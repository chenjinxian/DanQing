// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — SelectionSet and HiliteSet implementation
// Ported from: itwinjs-core core/frontend/src/SelectionSet.ts
#include "dqApp/SelectionSet.h"

namespace dqApp {

// ---------------------------------------------------------------------------
// SelectionSet
// ---------------------------------------------------------------------------
// 变更门（1:1 SelectionSet.ts #add/#remove/#replace 的 changed 守卫——
// oldSize !== size 才 sendEvent；M-P P-F：clearControls 对缺席 id 的 no-op
// remove 不得再触发 OnChanged[否则 Synch→clearControls 无限往复]）。
void SelectionSet::add(QSet<uint32_t> const& ids)
{
    int const oldSize = m_elements.size();
    m_elements.unite(ids);
    if (m_elements.size() != oldSize)
        OnChanged.Raise(this);
}

void SelectionSet::Remove(QSet<uint32_t> const& ids)
{
    int const oldSize = m_elements.size();
    for (auto id : ids) {
        m_elements.remove(id);
    }
    if (m_elements.size() != oldSize)
        OnChanged.Raise(this);
}

void SelectionSet::Replace(QSet<uint32_t> const& ids)
{
    if (m_elements == ids)
        return;
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

// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — SelectionSet and HiliteSet
// Ported from: itwinjs-core core/frontend/src/SelectionSet.ts
//
// SelectionSet tracks which features (by feature ID) are currently selected.
// HiliteSet mirrors SelectionSet for rendering the hilite visual effect.
#pragma once

#include "Export.h"

#include <dqBase/DqEvent.h>

#include <QSet>

namespace dqApp {

// ---------------------------------------------------------------------------
// SelectionSet — tracks selected feature IDs
// Ported from: itwinjs-core SelectionSet.ts (line 325)
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT SelectionSet {
public:
    SelectionSet() = default;

    /// add feature IDs to the selection.
    void add(QSet<uint32_t> const& ids);

    /// Remove feature IDs from the selection.
    void Remove(QSet<uint32_t> const& ids);

    /// Replace the entire selection.
    void Replace(QSet<uint32_t> const& ids);

    /// clear all selections.
    void EmptyAll();

    /// Check if a feature ID is selected.
    bool Contains(uint32_t id) const { return m_elements.contains(id); }

    /// Get all selected feature IDs.
    QSet<uint32_t> const& GetElements() const { return m_elements; }

    /// Get the number of selected features.
    int size() const { return m_elements.size(); }

    /// Check if the selection is empty.
    bool isEmpty() const { return m_elements.isEmpty(); }

    /// Event raised when the selection changes.
    dqBase::DqEvent<void*> OnChanged;

private:
    QSet<uint32_t> m_elements;
};

// ---------------------------------------------------------------------------
// HiliteSet — mirrors SelectionSet for hilite rendering
// Ported from: itwinjs-core SelectionSet.ts (line 160)
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT HiliteSet {
public:
    HiliteSet() = default;

    /// Synchronize with a SelectionSet. Call this to mirror selection changes.
    void SyncWith(SelectionSet const& selection);

    /// Get the hilite feature IDs.
    QSet<uint32_t> const& GetElements() const { return m_elements; }

    /// Check if a feature ID is hilited.
    bool Contains(uint32_t id) const { return m_elements.contains(id); }

    /// clear all hilite state.
    void clear();

private:
    QSet<uint32_t> m_elements;
};

}  // namespace dqApp

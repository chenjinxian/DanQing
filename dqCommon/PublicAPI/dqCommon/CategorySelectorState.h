// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — CategorySelectorState (which categories are visible in a view)
// Ported from: itwinjs-core core/frontend/src/CategorySelectorState.ts
//
// Holds a set of category IDs visible in a view. Sibling of ModelSelectorState.
// (itwinjs stores categories as Set<Id64String>; DanQing uses bset<DqId>.)
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <dqBase/DqEvent.h>
#include <dqBase/DqId.h>
#include <dqBase/bset.h>

#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// ---------------------------------------------------------------------------
// CategorySelectorState — set of visible category IDs
// Ported from: itwinjs-core CategorySelectorState
// ---------------------------------------------------------------------------
class DQ_COMMON_EXPORT CategorySelectorState {
public:
    CategorySelectorState() = default;

    /// Add a category to the visible set.
    /// Ported from: itwinjs-core CategorySelectorState.addCategories
    void addCategory(dqBase::DqId id)
    {
        if (m_categories.insert(id).second)
            OnCategoryAdded.Raise(id);
    }

    /// Remove a category from the visible set.
    /// Ported from: itwinjs-core CategorySelectorState.dropCategories
    void dropCategory(dqBase::DqId id)
    {
        if (m_categories.erase(id) > 0)
            OnCategoryRemoved.Raise(id);
    }

    /// Check if a category is in the visible set.
    /// Ported from: itwinjs-core CategorySelectorState.isCategoryViewed
    bool containsCategory(dqBase::DqId id) const
    {
        return m_categories.find(id) != m_categories.end();
    }

    /// itwinjs `has(id)` alias (CategorySelectorState.ts:has).
    bool has(dqBase::DqId id) const { return containsCategory(id); }

    /// Clear all categories.
    /// Ported from: itwinjs-core CategorySelectorState.categories.clear
    void clear()
    {
        m_categories.clear();
        OnCategoryCleared.Raise();
    }

    /// Get the set of category IDs.
    dqBase::bset<dqBase::DqId> const& getCategories() const { return m_categories; }

    /// Get the number of categories.
    size_t getCount() const { return m_categories.size(); }

    /// Check if the set is empty.
    bool isEmpty() const { return m_categories.empty(); }

    // --- Plural / batched / display-control mutations ---
    // Ported from: itwinjs-core CategorySelectorState.addCategories (CategorySelectorState.ts:80-83)
    void addCategories(std::vector<dqBase::DqId> const& ids)
    {
        for (auto id : ids)
            addCategory(id);  // per-id event (non-batched path)
    }

    // Ported from: itwinjs-core CategorySelectorState.dropCategories (:86-89)
    void dropCategories(std::vector<dqBase::DqId> const& ids)
    {
        for (auto id : ids)
            dropCategory(id);
    }

    // Ported from: itwinjs-core CategorySelectorState.addCategoriesBatched (:94-96).
    // Raises a SINGLE batch event (not N per-id events) — the point of the Batched
    // variant (perf: 50k categories, full-stack-tests Viewport.test.ts:219-235).
    void addCategoriesBatched(std::vector<dqBase::DqId> const& ids)
    {
        std::vector<dqBase::DqId> added;
        for (auto id : ids)
            if (m_categories.insert(id).second)
                added.push_back(id);
        if (!added.empty())
            OnCategoriesBatchAdded.Raise(added);
    }

    // Ported from: itwinjs-core CategorySelectorState.dropCategoriesBatched (:101-103)
    void dropCategoriesBatched(std::vector<dqBase::DqId> const& ids)
    {
        std::vector<dqBase::DqId> removed;
        for (auto id : ids)
            if (m_categories.erase(id) > 0)
                removed.push_back(id);
        if (!removed.empty())
            OnCategoriesBatchRemoved.Raise(removed);
    }

    // Ported from: itwinjs-core CategorySelectorState.changeCategoryDisplay (:109-114)
    void changeCategoryDisplay(std::vector<dqBase::DqId> const& ids, bool add)
    {
        if (add)
            addCategories(ids);
        else
            dropCategories(ids);
    }

    // Ported from: itwinjs-core CategorySelectorState.equalState (:59-68).
    // NOTE: itwinjs also compares name/id (ElementState identity); the DanQing dqCommon
    // CategorySelectorState is a value member of ViewState without identity fields, so
    // this checks set-equality (size + subset) only.
    bool equalState(CategorySelectorState const& other) const
    {
        if (m_categories.size() != other.m_categories.size())
            return false;
        for (auto const& c : m_categories)
            if (!other.containsCategory(c))
                return false;
        return true;
    }

    // Events — fired on mutation
    dqBase::DqEvent<dqBase::DqId> OnCategoryAdded;
    dqBase::DqEvent<dqBase::DqId> OnCategoryRemoved;
    dqBase::DqEvent<> OnCategoryCleared;
    // Batched mutation events (single event for the whole batch).
    // TODO: toJSON (needs CategorySelectorProps) + ElementState identity (name/id) for
    // full equalState parity are deferred (DanQing's standalone dqCommon selector has no
    // props/identity infrastructure yet).
    dqBase::DqEvent<std::vector<dqBase::DqId> const&> OnCategoriesBatchAdded;
    dqBase::DqEvent<std::vector<dqBase::DqId> const&> OnCategoriesBatchRemoved;

private:
    dqBase::bset<dqBase::DqId> m_categories;
};

END_DQ_COMMON_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — ModelSelectorState (which models are visible in a view)
// Ported from: itwinjs-core core/frontend/src/ModelSelectorState.ts
//
// Holds a set of model IDs that are visible in a spatial view.
// When the set changes, events are fired so the Viewport can
// invalidate its scene and rebuild tile tree references.
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <dqBase/DqEvent.h>
#include <dqBase/DqId.h>
#include <dqBase/bset.h>

#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// ---------------------------------------------------------------------------
// ModelSelectorState — set of visible model IDs
// Ported from: itwinjs-core ModelSelectorState
// ---------------------------------------------------------------------------
class DQ_COMMON_EXPORT ModelSelectorState {
public:
    ModelSelectorState() = default;

    /// add a model to the visible set.
    /// Ported from: itwinjs-core ModelSelectorState.models.add
    void addModel(dqBase::DqId id)
    {
        if (m_models.insert(id).second)
            OnModelAdded.Raise(id);
    }

    /// Remove a model from the visible set.
    /// Ported from: itwinjs-core ModelSelectorState.models.delete
    void dropModel(dqBase::DqId id)
    {
        if (m_models.erase(id) > 0)
            OnModelRemoved.Raise(id);
    }

    /// Check if a model is in the visible set.
    /// Ported from: itwinjs-core ModelSelectorState.models.has
    bool containsModel(dqBase::DqId id) const
    {
        return m_models.find(id) != m_models.end();
    }

    /// clear all models from the visible set.
    /// Ported from: itwinjs-core ModelSelectorState.models.clear
    void clear()
    {
        m_models.clear();
        OnModelCleared.Raise();
    }

    /// Get the set of model IDs.
    dqBase::bset<dqBase::DqId> const& getModels() const { return m_models; }

    /// Get the number of models.
    size_t getCount() const { return m_models.size(); }

    /// Check if the set is empty.
    bool isEmpty() const { return m_models.empty(); }

    // --- Plural mutations ---
    // Ported from: itwinjs-core ModelSelectorState.addModels (ModelSelectorState.ts:74-77)
    void addModels(std::vector<dqBase::DqId> const& ids)
    {
        for (auto id : ids)
            addModel(id);
    }

    // Ported from: itwinjs-core ModelSelectorState.dropModels (:80-83)
    void dropModels(std::vector<dqBase::DqId> const& ids)
    {
        for (auto id : ids)
            dropModel(id);
    }

    // Ported from: itwinjs-core ModelSelectorState.equalState (:62-71).
    // NOTE: itwinjs also compares name/id; DanQing's dqCommon ModelSelectorState has no
    // identity fields, so this checks set-equality (size + subset) only.
    // TODO: toJSON (needs ModelSelectorProps) + load (needs IModelConnection.models.load)
    // deferred — the DanQing dqCommon selector has no props/iModel infrastructure yet.
    bool equalState(ModelSelectorState const& other) const
    {
        if (m_models.size() != other.m_models.size())
            return false;
        for (auto const& m : m_models)
            if (!other.containsModel(m))
                return false;
        return true;
    }

    // Events — fired on mutation
    dqBase::DqEvent<dqBase::DqId> OnModelAdded;
    dqBase::DqEvent<dqBase::DqId> OnModelRemoved;
    dqBase::DqEvent<> OnModelCleared;

private:
    dqBase::bset<dqBase::DqId> m_models;
};

END_DQ_COMMON_NAMESPACE

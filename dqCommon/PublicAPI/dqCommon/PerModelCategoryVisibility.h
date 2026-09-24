// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Per-model category visibility overrides
// Ported from: itwinjs-core core/frontend/src/PerModelCategoryVisibility.ts
// NOTE: ref lives in core/frontend (not core/common); DanQing places this props/visibility
// data type in dqCommon for cross-module sharing. The type itself is pure data (enum +
// props), so the frontend-origin citation is an honest §3 mapping, not a layering dependency.
//
// Allows controlling category visibility on a per-model basis.
// Each override specifies whether a category should be visible or hidden
// within a specific model, overriding the global category visibility.
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// ---------------------------------------------------------------------------
// PerModelCategoryOverride — visibility override for a category in a model
// Ported from: itwinjs-core PerModelCategoryVisibility.PerModelCategoryOverride
// ---------------------------------------------------------------------------
enum class PerModelCategoryOverride : uint8_t {
    None = 0,   // No override (use global visibility)
    Show = 1,   // Force show (override global hide)
    Hide = 2,   // Force hide (override global show)
};

// ---------------------------------------------------------------------------
// PerModelCategoryVisibilityOverrides — collection of per-model category overrides
// Ported from: itwinjs-core PerModelCategoryVisibility.Overrides
// ---------------------------------------------------------------------------
class DQ_COMMON_EXPORT PerModelCategoryVisibilityOverrides {
public:
    PerModelCategoryVisibilityOverrides() = default;

    // Get the override for a specific model+category pair.
    // Returns PerModelCategoryOverride::None if no override exists.
    // Ported from: itwinjs-core PerModelCategoryVisibility.Overrides.getOverride()
    PerModelCategoryOverride getOverride(uint64_t modelId, uint64_t categoryId) const;

    // Set an override for a specific model+category pair.
    // Ported from: itwinjs-core PerModelCategoryVisibility.Overrides.setOverride()
    void setOverride(uint64_t modelId, uint64_t categoryId, PerModelCategoryOverride override);

    // clear all overrides.
    // Ported from: itwinjs-core PerModelCategoryVisibility.Overrides.clear()
    void clear();

    // Get all overrides as a list.
    struct Entry {
        uint64_t modelId;
        uint64_t categoryId;
        PerModelCategoryOverride override;
    };
    std::vector<Entry> getEntries() const;

    // Check if there are any overrides.
    bool isEmpty() const { return m_overrides.empty(); }

private:
    // Key: (modelId << 32) | categoryId (assuming 32-bit IDs are sufficient)
    // For 64-bit IDs, use a pair hash.
    struct Key {
        uint64_t modelId;
        uint64_t categoryId;
        bool operator==(Key const& rhs) const {
            return modelId == rhs.modelId && categoryId == rhs.categoryId;
        }
    };
    struct KeyHash {
        size_t operator()(Key const& k) const {
            // Simple hash combining
            size_t h1 = std::hash<uint64_t>{}(k.modelId);
            size_t h2 = std::hash<uint64_t>{}(k.categoryId);
            return h1 ^ (h2 << 1);
        }
    };

    std::unordered_map<Key, PerModelCategoryOverride, KeyHash> m_overrides;
};

END_DQ_COMMON_NAMESPACE

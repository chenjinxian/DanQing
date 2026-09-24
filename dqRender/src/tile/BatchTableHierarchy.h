// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — 3DTILES_batch_table_hierarchy extension parser
// Ported from: itwinjs-core core/frontend/src/internal/tile/BatchedTileIdMap.ts
//
// Parses the 3DTILES_batch_table_hierarchy extension from b3dm batch tables.
// Maps batch IDs to element IDs through a hierarchy of classes and instances.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// A class in the batch table hierarchy.
struct BatchTableClass {
    std::string name;
    // Per-instance properties (indexed by instance index within this class).
    std::vector<uint64_t> elementIds;
    std::vector<uint64_t> subCategoryIds;
};

// ---------------------------------------------------------------------------
// BatchTableHierarchy — parsed 3DTILES_batch_table_hierarchy extension
// ---------------------------------------------------------------------------
struct BatchTableHierarchy {
    std::vector<BatchTableClass> classes;
    uint32_t instancesLength = 0;
    std::vector<uint32_t> classIds;    // classId per instance
    std::vector<int32_t> parentIds;    // parentId per instance (-1 = root)

    /// Resolve the element ID for a given batch ID (instance index).
    /// Returns 0 if not found.
    uint64_t getElementId(uint32_t batchId) const;

    /// Resolve the subcategory ID for a given batch ID.
    /// Returns 0 if not found.
    uint64_t getSubCategoryId(uint32_t batchId) const;

    /// Check if the hierarchy is valid.
    bool isValid() const;
};

// Parse a 3DTILES_batch_table_hierarchy JSON object.
// Returns an empty hierarchy if parsing fails.
BatchTableHierarchy parseBatchTableHierarchy(char const* json, size_t jsonSize);

END_DQ_RENDER_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Packed feature table for GPU consumption
//
// Ported from: itwinjs-core core/common/src/internal/PackedFeatureTable.ts
// Converts a FeatureTable into a flat uint32 array suitable for texture upload.
//
// Layout per feature (3 x uint32 = 12 bytes):
//   offset+0: elementId.lower  (uint32)
//   offset+1: elementId.upper  (uint32)
//   offset+2: subCategoryIndex (24 bits) | geometryClass (8 bits)
//
// Subcategory table (appended after features, 2 x uint32 per unique subcategory):
//   offset+0: subCategoryId.lower  (uint32)
//   offset+1: subCategoryId.upper  (uint32)
#pragma once

#include "Export.h"
#include "FeatureTable.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Packed binary representation of a FeatureTable for GPU upload.
// Ported from: itwinjs-core PackedFeatureTable (lines 20-219)
class DQ_COMMON_EXPORT PackedFeatureTable {
public:
    // pack a FeatureTable into a flat uint32 array.
    // Ported from: itwinjs-core PackedFeatureTable.pack()
    static PackedFeatureTable pack(const FeatureTable& table);

    // Default construction (used by pack()).
    PackedFeatureTable() = default;

    // Construct directly from imdl wire packed words (3×u32/feature + 2×u32
    // per subcategory tail). Ported from: itwinjs-core PackedFeatureTable
    // constructor (PackedFeatureTable.ts:35-58 — data carries the subcategory
    // tail; numSubCategories derives from data length).
    PackedFeatureTable(std::vector<uint32_t> data, uint64_t modelId,
                       uint32_t numFeatures, BatchType type);

    // ── Data access ───────────────────────────────────────────────
    const uint32_t* getData() const noexcept { return m_data.data(); }
    size_t getDataSize() const noexcept { return m_data.size(); }
    size_t getDataBytes() const noexcept { return m_data.size() * sizeof(uint32_t); }

    // ── Feature count ─────────────────────────────────────────────
    uint32_t getNumFeatures() const noexcept { return m_numFeatures; }
    uint32_t getNumSubCategories() const noexcept { return m_numSubCategories; }

    // ── Model ID ──────────────────────────────────────────────────
    uint64_t getModelId() const noexcept { return m_modelId; }
    uint32_t getModelIdLower() const noexcept { return static_cast<uint32_t>(m_modelId & 0xFFFFFFFF); }
    uint32_t getModelIdUpper() const noexcept { return static_cast<uint32_t>(m_modelId >> 32); }

    // ── PackedFeature ─────────────────────────────────────────────
    struct PackedFeature {
        uint64_t elementId = 0;
        uint64_t subCategoryId = 0;
        GeometryClass geometryClass = GeometryClass::Primary;
        uint32_t animationNodeId = 0;
    };

    // Look up a feature by index.
    // Ported from: itwinjs-core PackedFeatureTable.getFeature()
    PackedFeature getFeature(uint32_t index) const;

    // Find feature by index (returns nullopt if out of bounds).
    // Ported from: itwinjs-core PackedFeatureTable.findFeature()
    std::optional<PackedFeature> findFeature(uint32_t index) const;

    // Find element ID by index.
    // Ported from: itwinjs-core PackedFeatureTable.findElementId()
    std::optional<uint64_t> findElementId(uint32_t index) const;

    // Get element ID as lower/upper uint32 pair.
    // Ported from: itwinjs-core PackedFeatureTable.getElementIdPair()
    void getElementIdPair(uint32_t index, uint32_t& outLower, uint32_t& outUpper) const;

    // ── Uniform / Classifier ──────────────────────────────────────

    // True if table contains exactly one feature.
    // Ported from: itwinjs-core PackedFeatureTable.isUniform
    bool isUniform() const noexcept { return m_numFeatures == 1; }

    // If uniform, returns the single feature; otherwise nullopt.
    // Ported from: itwinjs-core PackedFeatureTable.getUniform()
    std::optional<PackedFeature> getUniform() const;

    // Batch type.
    BatchType getType() const noexcept { return m_type; }

    // True if associated with VolumeClassifier geometry.
    bool isVolumeClassifier() const noexcept { return m_type == BatchType::VolumeClassifier; }
    // True if associated with PlanarClassifier geometry.
    bool isPlanarClassifier() const noexcept { return m_type == BatchType::PlanarClassifier; }

    // ── Animation nodes ───────────────────────────────────────────

    // Get animation node ID for a feature.
    // Ported from: itwinjs-core PackedFeatureTable.getAnimationNodeId()
    uint32_t getAnimationNodeId(uint32_t index) const;

    // ── unpack ────────────────────────────────────────────────────

    // Convert back to a FeatureTable.
    // Ported from: itwinjs-core PackedFeatureTable.unpack()
    FeatureTable unpack() const;

private:
    // Helper: read subcategory ID pair from packed data
    void getSubCategoryIdPair(uint32_t index, uint64_t& outSubCatId) const;

    std::vector<uint32_t> m_data;
    uint32_t m_numFeatures = 0;
    uint32_t m_numSubCategories = 0;
    uint64_t m_modelId = 0;
    BatchType m_type = BatchType::Primary;
    std::vector<uint32_t> m_animationNodeIds;  // empty if not populated
};

END_DQ_COMMON_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — PackedFeatureTable implementation
// Ported from: itwinjs-core core/common/src/internal/PackedFeatureTable.ts
#include "dqCommon/PackedFeatureTable.h"

#include <utility>

BEGIN_DQ_COMMON_NAMESPACE

// Helper: split a64-bit ID into lower/upper32-bit pair.
static void SplitId(uint64_t id, uint32_t& lower, uint32_t& upper)
{
    lower = static_cast<uint32_t>(id & 0xFFFFFFFF);
    upper = static_cast<uint32_t>((id >> 32) & 0xFFFFFFFF);
}

// Helper: combine lower/upper32-bit pair into64-bit ID.
static uint64_t CombineId(uint32_t lower, uint32_t upper)
{
    return (static_cast<uint64_t>(upper) << 32) | static_cast<uint64_t>(lower);
}

// Ported from: itwinjs-core PackedFeatureTable.ts constructor (:35-58).
PackedFeatureTable::PackedFeatureTable(std::vector<uint32_t> data, uint64_t modelId,
                                       uint32_t numFeatures, BatchType type)
    : m_data(std::move(data))
{
    m_numFeatures = numFeatures;
    m_modelId = modelId;
    m_type = type;
    // _subCategoriesOffset = 3 * numFeatures（PackedFeatureTable.ts:44-46 区）;
    // 尾部 2×u32/subcat。
    size_t const subCatsOffset = 3u * static_cast<size_t>(numFeatures);
    m_numSubCategories = m_data.size() > subCatsOffset
        ? static_cast<uint32_t>((m_data.size() - subCatsOffset) / 2u) : 0u;
}

PackedFeatureTable PackedFeatureTable::pack(const FeatureTable& table)
{
    // Ported from: itwinjs-core PackedFeatureTable.pack()
    PackedFeatureTable result;
    result.m_numFeatures = static_cast<uint32_t>(table.getArraySize());
    result.m_modelId = table.getModelId().GetValue();
    result.m_type = table.getType();

    if (result.m_numFeatures == 0)
        return result;

    // Collect unique subcategories and assign indices.
    std::unordered_map<uint64_t, uint32_t> subCatMap;
    const auto* array = table.getArray();
    for (int i = 0; i < table.getArraySize(); ++i) {
        uint64_t subCatId = array[i].value.subCategoryId.GetValue();
        if (subCatMap.find(subCatId) == subCatMap.end()) {
            subCatMap[subCatId] = static_cast<uint32_t>(subCatMap.size());
        }
    }
    result.m_numSubCategories = static_cast<uint32_t>(subCatMap.size());

    // Allocate data: 3 * numFeatures + 2 * numSubCategories.
    size_t totalWords = 3 * result.m_numFeatures + 2 * result.m_numSubCategories;
    result.m_data.resize(totalWords, 0);

    // pack features.
    for (uint32_t i = 0; i < result.m_numFeatures; ++i) {
        const Feature& feature = array[i].value;
        uint32_t elemLower, elemUpper;
        SplitId(feature.elementId.GetValue(), elemLower, elemUpper);

        uint32_t subCatIndex = subCatMap[feature.subCategoryId.GetValue()];
        uint32_t geoClass = static_cast<uint32_t>(feature.geometryClass) & 0xFF;

        size_t base = 3 * i;
        result.m_data[base + 0] = elemLower;
        result.m_data[base + 1] = elemUpper;
        result.m_data[base + 2] = (subCatIndex & 0x00FFFFFF) | (geoClass << 24);
    }

    // pack subcategory table (appended after features).
    size_t subCatBase = 3 * result.m_numFeatures;
    for (const auto& [subCatId, index] : subCatMap) {
        uint32_t scLower, scUpper;
        SplitId(subCatId, scLower, scUpper);
        size_t offset = subCatBase + 2 * index;
        result.m_data[offset + 0] = scLower;
        result.m_data[offset + 1] = scUpper;
    }

    return result;
}

PackedFeatureTable::PackedFeature PackedFeatureTable::getFeature(uint32_t index) const
{
    PackedFeature result;
    if (index >= m_numFeatures)
        return result;

    size_t base = 3 * index;
    uint32_t elemLower = m_data[base + 0];
    uint32_t elemUpper = m_data[base + 1];
    uint32_t packed = m_data[base + 2];

    result.elementId = CombineId(elemLower, elemUpper);
    uint32_t subCatIndex = packed & 0x00FFFFFF;
    result.geometryClass = static_cast<GeometryClass>((packed >> 24) & 0xFF);

    // Look up subcategory ID.
    size_t subCatBase = 3 * m_numFeatures;
    if (subCatIndex < m_numSubCategories) {
        uint32_t scLower = m_data[subCatBase + 2 * subCatIndex + 0];
        uint32_t scUpper = m_data[subCatBase + 2 * subCatIndex + 1];
        result.subCategoryId = CombineId(scLower, scUpper);
    }

    return result;
}

std::optional<PackedFeatureTable::PackedFeature> PackedFeatureTable::findFeature(uint32_t index) const
{
    if (index >= m_numFeatures)
        return std::nullopt;
    return getFeature(index);
}

std::optional<uint64_t> PackedFeatureTable::findElementId(uint32_t index) const
{
    if (index >= m_numFeatures)
        return std::nullopt;
    size_t base = 3 * index;
    return CombineId(m_data[base], m_data[base + 1]);
}

void PackedFeatureTable::getElementIdPair(uint32_t index, uint32_t& outLower, uint32_t& outUpper) const
{
    size_t base = 3 * index;
    outLower = m_data[base];
    outUpper = m_data[base + 1];
}

std::optional<PackedFeatureTable::PackedFeature> PackedFeatureTable::getUniform() const
{
    if (m_numFeatures != 1)
        return std::nullopt;
    return getFeature(0);
}

uint32_t PackedFeatureTable::getAnimationNodeId(uint32_t index) const
{
    if (m_animationNodeIds.empty() || index >= m_numFeatures)
        return 0;
    return m_animationNodeIds[index];
}

void PackedFeatureTable::getSubCategoryIdPair(uint32_t index, uint64_t& outSubCatId) const
{
    size_t base = 3 * index;
    uint32_t packed = m_data[base + 2];
    uint32_t subCatIndex = packed & 0x00FFFFFF;
    size_t subCatBase = 3 * m_numFeatures;
    if (subCatIndex < m_numSubCategories) {
        uint32_t scLower = m_data[subCatBase + 2 * subCatIndex];
        uint32_t scUpper = m_data[subCatBase + 2 * subCatIndex + 1];
        outSubCatId = CombineId(scLower, scUpper);
    } else {
        outSubCatId = 0;
    }
}

FeatureTable PackedFeatureTable::unpack() const
{
    FeatureTable table(m_numFeatures, dqBase::DqId(m_modelId), m_type);
    for (uint32_t i = 0; i < m_numFeatures; ++i) {
        PackedFeature pf = getFeature(i);
        Feature feature(dqBase::DqId(pf.elementId), dqBase::DqId(pf.subCategoryId), pf.geometryClass);
        table.insertWithIndex(feature, static_cast<int>(i));
    }
    return table;
}

END_DQ_COMMON_NAMESPACE

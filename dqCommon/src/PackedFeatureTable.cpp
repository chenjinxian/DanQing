// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — PackedFeatureTable implementation
// Ported from: itwinjs-core core/common/src/internal/PackedFeatureTable.ts
#include "dqCommon/PackedFeatureTable.h"

#include <algorithm>
#include <utility>
#include <vector>

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
    // 输出等价优化（§11.8 核读 + 2026-10-20 bridge-edit 卡顿取证）：
    // 参考 unpack（PackedFeatureTable.ts:184-193）逐条 insertWithIndex——JS
    // splice 在输入已按 Feature.compare 升序时 O(1/条)，而 imdl packed words
    // 的 feature 序不保证升序：DanQing 的有序数组插入（lowerBound+移位）在
    // 非升序下退化为 O(n²)（实测 n=108,778 → 1.52s；n=71,306 → 0.74s，
    // [TT] unpack 探针）。先按同一比较器排序再插入——输出内容与逐条插入
    // 完全一致（FeatureTable 是 Feature.compare 序的有序表；重复 feature 的
    // 相对序两边都未定义——参考 assert(!bound.equal) 仅在 dev 生效），
    // 复杂度 O(n log n)。
    // 参考偏差登记（TD-33）：参考 imdl 路径**从不调用 unpack**（
    // convertFeatureTable 直接交 PackedFeatureTable 给 createBatch，
    // ParseImdlDocument.ts:1259-1266）；DanQing 在内容创建时 unpack 成
    // FeatureTable 是 Batch 消费面未接口化的偏差——接口化后本转换整体消除。
    struct Entry {
        Feature feature;
        int index;
    };
    std::vector<Entry> entries;
    entries.reserve(m_numFeatures);
    for (uint32_t i = 0; i < m_numFeatures; ++i) {
        PackedFeature pf = getFeature(i);
        entries.push_back({Feature(dqBase::DqId(pf.elementId),
                                   dqBase::DqId(pf.subCategoryId),
                                   pf.geometryClass),
                           static_cast<int>(i)});
    }
    std::stable_sort(entries.begin(), entries.end(),
                     [](Entry const& a, Entry const& b) {
                         return a.feature.compare(b.feature) < 0;
                     });

    FeatureTable table(m_numFeatures, dqBase::DqId(m_modelId), m_type);
    for (auto const& e : entries)
        table.insertWithIndex(e.feature, e.index);  // 升序插入 = O(log n)/条
    return table;
}

END_DQ_COMMON_NAMESPACE

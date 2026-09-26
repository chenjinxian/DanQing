// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — FeatureTable implementation
//
// Ported from: itwinjs-core core/common/src/FeatureTable.ts
#include "dqCommon/FeatureTable.h"
#include "dqCommon/PackedFeatureTable.h"

#include <algorithm>
#include <cstring>
#include <utility>

BEGIN_DQ_COMMON_NAMESPACE

using namespace dqBase;

// Ported from: itwinjs-core FeatureTable constructor
FeatureTable::FeatureTable(int maxFeatures, const DqId& modelId, BatchType type)
    : m_modelId(modelId)
    , m_type(type)
    , m_maxFeatures(maxFeatures)
    , m_size(0)
    , m_array(new IndexedFeature[static_cast<size_t>(maxFeatures)])
{
}

// TD-21: reference is garbage-collected; C++ owns the array.
FeatureTable::~FeatureTable() { delete[] m_array; }

FeatureTable::FeatureTable(FeatureTable const& rhs)
    : m_modelId(rhs.m_modelId)
    , m_type(rhs.m_type)
    , m_maxFeatures(rhs.m_maxFeatures)
    , m_size(rhs.m_size)
    , m_array(new IndexedFeature[static_cast<size_t>(rhs.m_maxFeatures)])
{
    // IndexedFeature (IndexedValue<Feature>) is trivially copyable; a moved-from
    // source carries m_size == 0 with a null array, so guard the memcpy.
    if (rhs.m_size > 0) {
        std::memcpy(m_array, rhs.m_array,
                    sizeof(IndexedFeature) * static_cast<size_t>(rhs.m_size));
    }
}

FeatureTable& FeatureTable::operator=(FeatureTable const& rhs)
{
    if (this != &rhs) {
        FeatureTable tmp(rhs);  // copy-and-swap: self-assign safe
        swap(tmp);
    }
    return *this;
}

FeatureTable::FeatureTable(FeatureTable&& rhs) noexcept
    : m_modelId(rhs.m_modelId)
    , m_type(rhs.m_type)
    , m_maxFeatures(rhs.m_maxFeatures)
    , m_size(rhs.m_size)
    , m_array(rhs.m_array)
{
    rhs.m_array = nullptr;
    rhs.m_size = 0;
    rhs.m_maxFeatures = 0;
}

FeatureTable& FeatureTable::operator=(FeatureTable&& rhs) noexcept
{
    if (this != &rhs) {
        delete[] m_array;
        m_modelId = rhs.m_modelId;
        m_type = rhs.m_type;
        m_maxFeatures = rhs.m_maxFeatures;
        m_size = rhs.m_size;
        m_array = rhs.m_array;
        rhs.m_array = nullptr;
        rhs.m_size = 0;
        rhs.m_maxFeatures = 0;
    }
    return *this;
}

void FeatureTable::swap(FeatureTable& other) noexcept
{
    std::swap(m_modelId, other.m_modelId);
    std::swap(m_type, other.m_type);
    std::swap(m_maxFeatures, other.m_maxFeatures);
    std::swap(m_size, other.m_size);
    std::swap(m_array, other.m_array);
}

// Ported from: itwinjs-core FeatureTable.anyDefined
bool FeatureTable::isAnyDefined() const noexcept
{
    if (m_size > 1)
        return true;
    if (m_size == 1)
        return m_array[0].value.isDefined();
    return false;
}

// Ported from: itwinjs-core FeatureTable.uniform
std::optional<Feature> FeatureTable::getUniform() const noexcept
{
    if (m_size == 1)
        return m_array[0].value;
    return std::nullopt;
}

// Binary search for the position where feature should be inserted.
// Ported from: itwinjs-core IndexMap.lowerBound
int FeatureTable::lowerBound(const Feature& feature) const
{
    int low = 0;
    int high = m_size;
    while (low < high) {
        const int mid = (low + high) / 2;
        const int cmp = feature.compare(m_array[mid].value);
        if (cmp == 0)
            return mid;
        if (cmp < 0)
            high = mid;
        else
            low = mid + 1;
    }
    return low;
}

// Ported from: itwinjs-core IndexMap.insert (inherited)
int FeatureTable::insert(const Feature& feature)
{
    const int pos = lowerBound(feature);
    if (pos < m_size && m_array[pos].value.equals(feature))
        return m_array[pos].index;

    if (m_size >= m_maxFeatures)
        return -1;

    const int newIndex = m_size;
    // Shift elements right
    for (int i = m_size; i > pos; --i)
        m_array[i] = m_array[i - 1];
    m_array[pos] = {feature, newIndex};
    ++m_size;
    return newIndex;
}

// Ported from: itwinjs-core FeatureTable.findFeature
std::optional<Feature> FeatureTable::findFeature(int index) const
{
    for (int i = 0; i < m_size; ++i) {
        if (m_array[i].index == index)
            return m_array[i].value;
    }
    return std::nullopt;
}

// Ported from: itwinjs-core FeatureTable.insertWithIndex
void FeatureTable::insertWithIndex(const Feature& feature, int index)
{
    const int pos = lowerBound(feature);
    if (m_size >= m_maxFeatures)
        return;

    // Shift elements right
    for (int i = m_size; i > pos; --i)
        m_array[i] = m_array[i - 1];
    m_array[pos] = {feature, index};
    ++m_size;
}

// Ported from: itwinjs-core FeatureTable.pack()
PackedFeatureTable FeatureTable::pack() const
{
    return PackedFeatureTable::pack(*this);
}

END_DQ_COMMON_NAMESPACE

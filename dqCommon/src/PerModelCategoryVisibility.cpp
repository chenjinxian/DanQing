// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Per-model category visibility overrides implementation
// Ported from: itwinjs-core core/common/src/PerModelCategoryVisibility.ts
#include "dqCommon/PerModelCategoryVisibility.h"

BEGIN_DQ_COMMON_NAMESPACE

PerModelCategoryOverride PerModelCategoryVisibilityOverrides::getOverride(
    uint64_t modelId, uint64_t categoryId) const
{
    // Ported from: itwinjs-core PerModelCategoryVisibility.Overrides.getOverride()
    Key key{modelId, categoryId};
    auto it = m_overrides.find(key);
    if (it != m_overrides.end()) {
        return it->second;
    }
    return PerModelCategoryOverride::None;
}

void PerModelCategoryVisibilityOverrides::setOverride(
    uint64_t modelId, uint64_t categoryId, PerModelCategoryOverride override)
{
    // Ported from: itwinjs-core PerModelCategoryVisibility.Overrides.setOverride()
    Key key{modelId, categoryId};
    if (override == PerModelCategoryOverride::None) {
        m_overrides.erase(key);
    } else {
        m_overrides[key] = override;
    }
}

void PerModelCategoryVisibilityOverrides::clear()
{
    // Ported from: itwinjs-core PerModelCategoryVisibility.Overrides.clear()
    m_overrides.clear();
}

std::vector<PerModelCategoryVisibilityOverrides::Entry>
PerModelCategoryVisibilityOverrides::getEntries() const
{
    std::vector<Entry> entries;
    entries.reserve(m_overrides.size());
    for (auto const& [key, override] : m_overrides) {
        entries.push_back({key.modelId, key.categoryId, override});
    }
    return entries;
}

END_DQ_COMMON_NAMESPACE

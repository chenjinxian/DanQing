// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — FeatureOverrideProvider
//
// Ported from: itwinjs-core core/frontend/src/FeatureOverrideProvider.ts
// Viewport-level feature override management.
#pragma once

#include "Export.h"

#include <dqCommon/FeatureSymbology.h>

#include <dqBase/DqId.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dqApp {

class Viewport;

// Interface for objects that customize feature appearance within a Viewport.
// Ported from: itwinjs-core FeatureOverrideProvider
class DQ_APP_EXPORT FeatureOverrideProvider {
public:
    virtual ~FeatureOverrideProvider() = default;

    // add symbology overrides to be applied to the specified viewport.
    // Ported from: itwinjs-core FeatureOverrideProvider.addFeatureOverrides()
    virtual void addFeatureOverrides(const Viewport& viewport) = 0;
};

// Manages feature appearance overrides for a viewport.
// Ported from: itwinjs-core FeatureSymbology.Overrides
class DQ_APP_EXPORT ViewportFeatureOverrides {
public:
    ViewportFeatureOverrides() = default;

    // Register a provider for custom overrides.
    // Ported from: itwinjs-core Viewport.addFeatureOverrideProvider()
    void AddProvider(FeatureOverrideProvider* provider) { m_providers.push_back(provider); }

    // Remove a provider.
    void RemoveProvider(FeatureOverrideProvider* provider)
    {
        for (auto it = m_providers.begin(); it != m_providers.end(); ++it) {
            if (*it == provider) {
                m_providers.erase(it);
                return;
            }
        }
    }

    // Get the registered providers.
    const std::vector<FeatureOverrideProvider*>& GetProviders() const noexcept { return m_providers; }

    // Get the appearance for a specific element.
    // Ported from: itwinjs-core FeatureOverrides.getFeatureAppearance()
    std::optional<dqCommon::FeatureAppearance> getAppearance(const dqBase::DqId& elementId) const
    {
        auto it = m_elementAppearances.find(elementId.GetValue());
        if (it != m_elementAppearances.end())
            return it->second;
        return std::nullopt;
    }

    // Set appearance for an element.
    void SetElementAppearance(const dqBase::DqId& elementId, const dqCommon::FeatureAppearance& appearance)
    {
        m_elementAppearances.insert_or_assign(elementId.GetValue(), appearance);
    }

    // Remove appearance for an element.
    void ClearElementAppearance(const dqBase::DqId& elementId)
    {
        m_elementAppearances.erase(elementId.GetValue());
    }

    // Get/set default overrides.
    const dqCommon::FeatureAppearance& getDefaultOverrides() const noexcept { return m_defaultOverrides; }
    void setDefaultOverrides(const dqCommon::FeatureAppearance& overrides) { m_defaultOverrides = overrides; }

    // clear all overrides.
    void clear() noexcept
    {
        m_elementAppearances.clear();
        m_defaultOverrides = dqCommon::FeatureAppearance::defaults();
    }

private:
    std::vector<FeatureOverrideProvider*> m_providers;
    std::unordered_map<uint64_t, dqCommon::FeatureAppearance> m_elementAppearances;
    dqCommon::FeatureAppearance m_defaultOverrides = dqCommon::FeatureAppearance::defaults();
};

}  // namespace dqApp

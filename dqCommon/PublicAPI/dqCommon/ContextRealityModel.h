// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Context reality model
// Ported from: itwinjs-core core/common/src/ContextRealityModel.ts
#pragma once

#include "Export.h"
#include "FeatureSymbology.h"
#include "PlanarClipMask.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Reality data source key.
// Ported from: itwinjs-core RealityDataSourceKey
struct RealityDataSourceKey {
    std::string provider;
    std::string format;
    std::string id;
    std::optional<std::string> iTwinId;

    bool isEqual(const RealityDataSourceKey& rhs) const noexcept
    {
        return provider == rhs.provider && format == rhs.format && id == rhs.id;
    }

    std::string toString() const
    {
        return provider + ":" + format + ":" + id;
    }
};

// JSON persistence.
struct ContextRealityModelProps {
    std::optional<RealityDataSourceKey> rdSourceKey;
    std::string tilesetUrl;
    std::optional<std::string> realityDataId;
    std::optional<std::string> name;
    std::optional<std::string> description;
    std::optional<PlanarClipMaskProps> planarClipMask;
    std::optional<FeatureAppearanceProps> appearanceOverrides;
    std::optional<bool> invisible;
};

// Context reality model data.
// Ported from: itwinjs-core ContextRealityModel
class DQ_COMMON_EXPORT ContextRealityModel {
public:
    std::optional<RealityDataSourceKey> rdSourceKey;
    std::string tilesetUrl;
    std::optional<std::string> realityDataId;
    std::string name;
    std::string description;
    std::optional<PlanarClipMaskSettings> planarClipMask;
    std::optional<FeatureAppearance> appearanceOverrides;
    bool invisible = false;

    static ContextRealityModel fromJSON(const ContextRealityModelProps& props)
    {
        ContextRealityModel m;
        m.rdSourceKey = props.rdSourceKey;
        m.tilesetUrl = props.tilesetUrl;
        m.realityDataId = props.realityDataId;
        if (props.name) m.name = *props.name;
        if (props.description) m.description = *props.description;
        if (props.planarClipMask) m.planarClipMask = PlanarClipMaskSettings::fromJSON(&*props.planarClipMask);
        if (props.appearanceOverrides) m.appearanceOverrides = FeatureAppearance::fromJSON(&*props.appearanceOverrides);
        if (props.invisible) m.invisible = *props.invisible;
        return m;
    }

    ContextRealityModelProps toJSON() const
    {
        ContextRealityModelProps p;
        p.rdSourceKey = rdSourceKey;
        p.tilesetUrl = tilesetUrl;
        p.realityDataId = realityDataId;
        p.name = name;
        p.description = description;
        if (planarClipMask) p.planarClipMask = planarClipMask->toJSON();
        if (appearanceOverrides) p.appearanceOverrides = appearanceOverrides->toJSON();
        p.invisible = invisible;
        return p;
    }

    bool matchesNameAndUrl(const std::string& n, const std::string& u) const noexcept
    {
        return name == n && tilesetUrl == u;
    }

    bool equals(const ContextRealityModel& rhs) const noexcept
    {
        return tilesetUrl == rhs.tilesetUrl && name == rhs.name;
    }
};

// Collection of context reality models.
// Ported from: itwinjs-core ContextRealityModels
class DQ_COMMON_EXPORT ContextRealityModels {
public:
    ContextRealityModels() = default;

    const std::vector<ContextRealityModel>& getModels() const noexcept { return m_models; }
    size_t getCount() const noexcept { return m_models.size(); }
    bool isEmpty() const noexcept { return m_models.empty(); }

    ContextRealityModel& add(const ContextRealityModelProps& props)
    {
        m_models.push_back(ContextRealityModel::fromJSON(props));
        return m_models.back();
    }

    bool Delete(size_t index)  // NOLINT(readability-identifier-naming) `delete` is a C++ keyword; TS ContextRealityModel.delete(index) not portable (§3.4)
    {
        if (index >= m_models.size()) return false;
        m_models.erase(m_models.begin() + static_cast<ptrdiff_t>(index));
        return true;
    }

    void clear() { m_models.clear(); }

private:
    std::vector<ContextRealityModel> m_models;
};

END_DQ_COMMON_NAMESPACE

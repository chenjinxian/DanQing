// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Spatial classification
// Ported from: itwinjs-core core/common/src/SpatialClassification.ts
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <dqBase/DqId.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Spatial classifier inside display mode.
// Ported from: itwinjs-core SpatialClassifierInsideDisplay
enum class SpatialClassifierInsideDisplay : uint8_t {
    Off = 0,
    On = 1,
    Dimmed = 2,
    Hilite = 3,
    ElementColor = 4,
};

// Spatial classifier outside display mode.
// Ported from: itwinjs-core SpatialClassifierOutsideDisplay
enum class SpatialClassifierOutsideDisplay : uint8_t {
    Off = 0,
    On = 1,
    Dimmed = 2,
};

// JSON persistence.
struct SpatialClassifierFlagsProps {
    SpatialClassifierInsideDisplay inside = SpatialClassifierInsideDisplay::Off;
    SpatialClassifierOutsideDisplay outside = SpatialClassifierOutsideDisplay::Off;
    std::optional<bool> isVolumeClassifier;
};

struct SpatialClassifierProps {
    std::string modelId;
    double expand = 0.0;
    SpatialClassifierFlagsProps flags;
    std::string name;
    std::optional<bool> isActive;
};

// Spatial classifier flags.
// Ported from: itwinjs-core SpatialClassifierFlags
class DQ_COMMON_EXPORT SpatialClassifierFlags {
public:
    SpatialClassifierInsideDisplay inside = SpatialClassifierInsideDisplay::Off;
    SpatialClassifierOutsideDisplay outside = SpatialClassifierOutsideDisplay::Off;
    bool isVolumeClassifier = false;

    static SpatialClassifierFlags fromJSON(const SpatialClassifierFlagsProps& props)
    {
        SpatialClassifierFlags f;
        f.inside = props.inside;
        f.outside = props.outside;
        if (props.isVolumeClassifier) f.isVolumeClassifier = *props.isVolumeClassifier;
        return f;
    }

    SpatialClassifierFlagsProps toJSON() const
    {
        SpatialClassifierFlagsProps p;
        p.inside = inside;
        p.outside = outside;
        p.isVolumeClassifier = isVolumeClassifier;
        return p;
    }

    bool equals(const SpatialClassifierFlags& rhs) const noexcept
    {
        return inside == rhs.inside && outside == rhs.outside && isVolumeClassifier == rhs.isVolumeClassifier;
    }

    SpatialClassifierFlags clone() const { return *this; }
};

// Spatial classifier.
// Ported from: itwinjs-core SpatialClassifier
class DQ_COMMON_EXPORT SpatialClassifier {
public:
    dqBase::DqId modelId;
    double expand = 0.0;
    SpatialClassifierFlags flags;
    std::string name;

    static SpatialClassifier fromJSON(const SpatialClassifierProps& props)
    {
        SpatialClassifier c;
        c.modelId = dqBase::DqId::FromString(props.modelId);
        c.expand = props.expand;
        c.flags = SpatialClassifierFlags::fromJSON(props.flags);
        c.name = props.name;
        return c;
    }

    SpatialClassifierProps toJSON() const
    {
        SpatialClassifierProps p;
        p.modelId = modelId.ToString();
        p.expand = expand;
        p.flags = flags.toJSON();
        p.name = name;
        return p;
    }

    bool equals(const SpatialClassifier& rhs) const noexcept
    {
        return modelId == rhs.modelId && expand == rhs.expand && flags.equals(rhs.flags) && name == rhs.name;
    }

    SpatialClassifier clone() const { return *this; }
};

// Collection of spatial classifiers.
// Ported from: itwinjs-core SpatialClassifiers
class DQ_COMMON_EXPORT SpatialClassifiers {
public:
    SpatialClassifiers() = default;

    size_t getSize() const noexcept { return m_classifiers.size(); }
    bool isEmpty() const noexcept { return m_classifiers.empty(); }

    const SpatialClassifier* getActive() const noexcept
    {
        return m_activeIndex < m_classifiers.size() ? &m_classifiers[m_activeIndex] : nullptr;
    }

    void setActive(size_t index)
    {
        if (index < m_classifiers.size())
            m_activeIndex = index;
    }

    const SpatialClassifier* find(const dqBase::DqId& modelId) const
    {
        for (const auto& c : m_classifiers) {
            if (c.modelId == modelId) return &c;
        }
        return nullptr;
    }

    bool has(const dqBase::DqId& modelId) const { return find(modelId) != nullptr; }

    void add(const SpatialClassifier& classifier)
    {
        m_classifiers.push_back(classifier);
    }

    void clear()
    {
        m_classifiers.clear();
        m_activeIndex = 0;
    }

    const std::vector<SpatialClassifier>& getClassifiers() const noexcept { return m_classifiers; }

private:
    std::vector<SpatialClassifier> m_classifiers;
    size_t m_activeIndex = 0;
};

END_DQ_COMMON_NAMESPACE

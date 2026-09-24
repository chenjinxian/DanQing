// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Planar clip mask settings
// Ported from: itwinjs-core core/common/src/PlanarClipMask.ts
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Planar clip mask mode.
// Ported from: itwinjs-core PlanarClipMaskMode
enum class PlanarClipMaskMode : uint8_t {
    None = 0,
    Priority = 1,
    Models = 2,
    IncludeSubCategories = 3,
    IncludeElements = 4,
    ExcludeElements = 5,
};

// Well-known priority values.
// Ported from: itwinjs-core PlanarClipMaskPriority
namespace PlanarClipMaskPriority {
    inline constexpr int BackgroundMap = -2048;
    inline constexpr int GlobalRealityModel = -1024;
    inline constexpr int RealityModel = 0;
    inline constexpr int DesignModel = 2048;
}

// JSON persistence.
struct PlanarClipMaskProps {
    PlanarClipMaskMode mode = PlanarClipMaskMode::None;
    std::optional<std::vector<uint64_t>> modelIds;
    std::optional<std::vector<uint64_t>> subCategoryOrElementIds;
    std::optional<int> priority;
    std::optional<double> transparency;
    std::optional<bool> invert;
};

// Planar clip mask settings.
// Ported from: itwinjs-core PlanarClipMaskSettings
class DQ_COMMON_EXPORT PlanarClipMaskSettings {
public:
    PlanarClipMaskMode mode = PlanarClipMaskMode::None;
    std::vector<uint64_t> modelIds;
    std::vector<uint64_t> subCategoryOrElementIds;
    std::optional<int> priority;
    std::optional<double> transparency;
    bool invert = false;

    static const PlanarClipMaskSettings& defaults() noexcept
    {
        static const PlanarClipMaskSettings s_default;
        return s_default;
    }

    static PlanarClipMaskSettings fromJSON(const PlanarClipMaskProps* props = nullptr)
    {
        PlanarClipMaskSettings s;
        if (!props) return s;
        s.mode = props->mode;
        if (props->modelIds) s.modelIds = *props->modelIds;
        if (props->subCategoryOrElementIds) s.subCategoryOrElementIds = *props->subCategoryOrElementIds;
        s.priority = props->priority;
        s.transparency = props->transparency;
        if (props->invert) s.invert = *props->invert;
        return s;
    }

    PlanarClipMaskProps toJSON() const
    {
        PlanarClipMaskProps p;
        p.mode = mode;
        if (!modelIds.empty()) p.modelIds = modelIds;
        if (!subCategoryOrElementIds.empty()) p.subCategoryOrElementIds = subCategoryOrElementIds;
        p.priority = priority;
        p.transparency = transparency;
        p.invert = invert;
        return p;
    }

    bool isValid() const noexcept
    {
        return mode != PlanarClipMaskMode::None;
    }

    bool equals(const PlanarClipMaskSettings& rhs) const noexcept
    {
        return mode == rhs.mode && priority == rhs.priority &&
               transparency == rhs.transparency && invert == rhs.invert;
    }

    PlanarClipMaskSettings clone() const { return *this; }

private:
    PlanarClipMaskSettings() = default;
};

END_DQ_COMMON_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Render schedule types
// Ported from: itwinjs-core core/common/src/RenderSchedule.ts
//
// Defines timeline-based animation for render schedule scripts.
// The full Script class with transform/symbology evaluation lives in dqRender.
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Render schedule namespace.
// Ported from: itwinjs-core RenderSchedule namespace
namespace RenderSchedule {

    // Interpolation mode.
    // Ported from: itwinjs-core RenderSchedule.Interpolation
    enum class Interpolation : uint8_t {
        Step = 1,
        Linear = 2,
    };

    // Timeline entry props.
    struct TimelineEntryProps {
        double time = 0.0;
        std::optional<Interpolation> interpolation;
    };

    // Visibility entry.
    struct VisibilityEntryProps : TimelineEntryProps {
        std::optional<double> value;  // 0-1
    };

    // Color entry.
    struct ColorEntryProps : TimelineEntryProps {
        struct Color { double red = 0.0; double green = 0.0; double blue = 0.0; };
        std::optional<Color> value;
    };

    // Cutting plane props.
    struct CuttingPlaneProps {
        std::vector<double> position;  // [x, y, z]
        std::vector<double> direction; // [x, y, z]
        std::optional<bool> visible;
        std::optional<bool> hidden;
    };

    // Cutting plane entry.
    struct CuttingPlaneEntryProps : TimelineEntryProps {
        std::optional<CuttingPlaneProps> value;
    };

    // Transform components.
    struct TransformComponentsProps {
        std::optional<std::vector<double>> position;    // [x, y, z]
        std::optional<std::vector<double>> orientation; // [qx, qy, qz, qw]
        std::optional<std::vector<double>> pivot;       // [x, y, z]
    };

    // Transform entry.
    struct TransformEntryProps : TimelineEntryProps {
        std::optional<TransformComponentsProps> value;
    };

    // Timeline (visibility + color + transform + cutting plane).
    struct TimelineProps {
        std::optional<std::vector<VisibilityEntryProps>> visibilityTimeline;
        std::optional<std::vector<ColorEntryProps>> colorTimeline;
        std::optional<std::vector<TransformEntryProps>> transformTimeline;
        std::optional<std::vector<CuttingPlaneEntryProps>> cuttingPlaneTimeline;
    };

    // Element timeline.
    struct ElementTimelineProps : TimelineProps {
        int batchId = 0;
        std::vector<std::string> elementIds;
    };

    // Model timeline.
    struct ModelTimelineProps : TimelineProps {
        std::string modelId;
        std::optional<std::string> realityModelUrl;
        std::vector<ElementTimelineProps> elementTimelines;
    };

    // Script props (array of model timelines).
    using ScriptProps = std::vector<ModelTimelineProps>;

    // Simplified render schedule script.
    // Ported from: itwinjs-core RenderSchedule.Script
    class DQ_COMMON_EXPORT Script {
    public:
        std::vector<ModelTimelineProps> modelTimelines;

        static std::optional<Script> fromJSON(const ScriptProps& props)
        {
            if (props.empty()) return std::nullopt;
            Script s;
            s.modelTimelines = props;
            return s;
        }

        ScriptProps toJSON() const
        {
            return modelTimelines;
        }

        bool equals(const Script& rhs) const noexcept
        {
            return modelTimelines.size() == rhs.modelTimelines.size();
        }

        bool containsTransform() const noexcept
        {
            for (const auto& mt : modelTimelines) {
                if (mt.transformTimeline.has_value() && !mt.transformTimeline->empty())
                    return true;
                for (const auto& et : mt.elementTimelines) {
                    if (et.transformTimeline.has_value() && !et.transformTimeline->empty())
                        return true;
                }
            }
            return false;
        }

        bool containsModelClipping() const noexcept
        {
            for (const auto& mt : modelTimelines) {
                if (mt.cuttingPlaneTimeline.has_value() && !mt.cuttingPlaneTimeline->empty())
                    return true;
                for (const auto& et : mt.elementTimelines) {
                    if (et.cuttingPlaneTimeline.has_value() && !et.cuttingPlaneTimeline->empty())
                        return true;
                }
            }
            return false;
        }
    };

}  // namespace RenderSchedule

END_DQ_COMMON_NAMESPACE

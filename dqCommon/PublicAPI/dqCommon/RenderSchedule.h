// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Render schedule script
// Ported from: itwinjs-core core/common/src/RenderSchedule.ts
//
// Namespace containing types that collectively define a script that animates the
// contents of a view by adjusting the visibility, position, and/or symbology of
// groups of elements over time. A RenderSchedule::Script is hosted by a
// RenderTimeline element or by DisplayStyleSettings::scheduleScriptProps.
//
// M-U U-a：数据层全量移植（DTA Timeline.ts 消费面见 samples/DisplayTestApp）。
// C++ 适配（§3.4/§11.10）：
//   - JS WeakMap 比较缓存（Script/ModelTimeline._cachedComparisons）→ 直比
//     （EQUIVALENCE E6：无 GC 弱引用语义的纯性能制品，可观察比较结果不变——
//      RenderSchedule.test.ts "weakly caches" 例只锁结果值，spy 面系 JS
//      猴子补丁专属不可移植）；
//   - Id64.Uint32Map（getTimelineForElement）→ unordered_map<uint64_t, ...>；
//   - elementIds 双形态（array | CompressedId64Set string）→ variant 承载，
//     toJSON 原样返回原文形态；
//   - 参考的泛型 TimelineEntryList<T,P,V>+子类 getValue 覆写 → 模板基类 +
//     四个具名条目表（参考仅 Visibility/Transform 两子类具名，Color/
//     CuttingPlane 为匿名实例化——DanQing 统一具名，语义同参考）。
#pragma once

#include "Export.h"
#include "FeatureSymbology.h"
#include "RgbColor.h"

#include <dqBase/CompressedId64Set.h>
#include <dqBase/DqCompare.h>
#include <dqBase/DqId.h>
#include <dqBase/DqTypes.h>

#include <dqGeom/ClipPlane.h>
#include <dqGeom/ClipPrimitive.h>
#include <dqGeom/ClipVector.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/Plane3dByOriginAndUnitNormal.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Point4d.h>
#include <dqGeom/Range3d.h>  // Range1d 定义于此
#include <dqGeom/Transform.h>
#include <dqGeom/UnionOfConvexClipPlaneSets.h>
#include <dqGeom/Vector3d.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Forward declaration (defined in FeatureOverrides.h).
class FeatureOverrides;

// ---------------------------------------------------------------------------
// RenderSchedule — schedule script 命名空间（对齐参考 namespace RenderSchedule）
// Ported from: itwinjs-core core/common/src/RenderSchedule.ts
// ---------------------------------------------------------------------------
namespace RenderSchedule {

// ── detail：文件级助手（参考 RenderSchedule.ts 文件级函数 + Compare.ts 适配） ──
namespace detail {

// start + fraction * (end - start).
// Ported from: itwinjs-core RenderSchedule.ts interpolate (:21-23)
inline double interpolate(double start, double end, double fraction) noexcept
{
    return start + fraction * (end - start);
}

// Ported from: itwinjs-core RenderSchedule.ts interpolateRgb (:25-27)
inline RgbColor interpolateRgb(RgbColor const& start, RgbColor const& end,
                               double fraction) noexcept
{
    return RgbColor(static_cast<int>(interpolate(start.r, end.r, fraction)),
                    static_cast<int>(interpolate(start.g, end.g, fraction)),
                    static_cast<int>(interpolate(start.b, end.b, fraction)));
}

// compareNumbers/compareBooleans/compareStrings 直接用 dqBase::（DqCompare.h，
// 1:1 移植自 itwinjs-core Compare.ts）。

// Ported from: itwinjs-core RenderSchedule.ts compareXYZ (:29-31)
template<typename T> int compareXYZ(T const& lhs, T const& rhs) noexcept
{
    int cmp = dqBase::compareNumbers(lhs.x, rhs.x);
    if (0 != cmp)
        return cmp;
    cmp = dqBase::compareNumbers(lhs.y, rhs.y);
    if (0 != cmp)
        return cmp;
    return dqBase::compareNumbers(lhs.z, rhs.z);
}

// Ported from: itwinjs-core RenderSchedule.ts compare4d (:33-35)
inline int compare4d(dqGeom::Point4d const& lhs, dqGeom::Point4d const& rhs) noexcept
{
    int cmp = dqBase::compareNumbers(lhs.x, rhs.x);
    if (0 != cmp)
        return cmp;
    cmp = dqBase::compareNumbers(lhs.y, rhs.y);
    if (0 != cmp)
        return cmp;
    cmp = dqBase::compareNumbers(lhs.z, rhs.z);
    if (0 != cmp)
        return cmp;
    return dqBase::compareNumbers(lhs.w, rhs.w);
}

// Ported from: itwinjs-core RenderSchedule.ts compareMatrices (:39-43)——
// 参考按列（columnX/Y/Z → compareXYZ）比；DanQing coffs 行主序，数学列 I =
// (coffs[I], coffs[3+I], coffs[6+I])。
inline int compareMatrices(dqGeom::Matrix3d const& lhs, dqGeom::Matrix3d const& rhs) noexcept
{
    auto const& a = lhs.coffs;
    auto const& b = rhs.coffs;
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            int const cmp = dqBase::compareNumbers(a[row * 3 + col], b[row * 3 + col]);
            if (0 != cmp)
                return cmp;
        }
    }
    return 0;
}

// Ported from: itwinjs-core RenderSchedule.ts compareDurations (:45-47)
inline int compareDurations(dqGeom::Range1d const& lhs, dqGeom::Range1d const& rhs) noexcept
{
    int const cmp = dqBase::compareNumbers(lhs.low, rhs.low);
    return 0 != cmp ? cmp : dqBase::compareNumbers(lhs.high, rhs.high);
}

// std::optional 版 comparePossiblyUndefined（dqBase 版为指针形——本文件全用 optional）。
inline int compareStringsOpt(std::optional<std::string> const& lhs,
                             std::optional<std::string> const& rhs)
{
    if (!lhs && !rhs)
        return 0;
    if (!lhs)
        return -1;
    if (!rhs)
        return 1;
    return dqBase::compareStrings(*lhs, *rhs);
}

template<typename T, typename CompareFn>
int comparePossiblyUndefinedOpt(CompareFn&& compareDefined,
                                std::optional<T> const& lhs,
                                std::optional<T> const& rhs)
{
    if (!lhs && !rhs)
        return 0;
    if (!lhs)
        return -1;
    if (!rhs)
        return 1;
    return compareDefined(*lhs, *rhs);
}

}  // namespace detail

// ── Props（JSON 表示——宿主层负责 JSON 解析后填充，dqCommon 保持 JSON-free §8.2） ──

// How to interpolate between two entries in a timeline.
// Ported from: itwinjs-core RenderSchedule.Interpolation (:59-66)
enum class Interpolation : uint8_t {
    // Each timeline entry's value is discrete.
    Step = 1,
    // Linearly interpolate based on the timepoint's distance between the entries.
    Linear = 2,
};

// Ported from: itwinjs-core RenderSchedule.TimelineEntryProps (:69-77)
struct TimelineEntryProps {
    // The time point in seconds in the Unix Epoch.
    double time = 0.0;
    // How to interpolate from this entry to the next.
    std::optional<Interpolation> interpolation;
};

// Ported from: itwinjs-core RenderSchedule.VisibilityEntryProps (:80-85)
struct VisibilityEntryProps : TimelineEntryProps {
    // Visibility 0..100 (default 100).
    std::optional<double> value;
};

// Ported from: itwinjs-core RenderSchedule.ColorEntryProps (:88-94)
struct ColorEntryProps : TimelineEntryProps {
    // The color applied to the geometry; absent = actual color.
    std::optional<RgbColorProps> value;
};

// Ported from: itwinjs-core RenderSchedule.CuttingPlaneProps (:97-106)
struct CuttingPlaneProps {
    // (x,y,z) of a point on the plane.
    std::array<double, 3> position{{0.0, 0.0, 0.0}};
    // (x,y,z) of the plane direction (towards the clip).
    std::array<double, 3> direction{{0.0, 0.0, 1.0}};
    // If true, the clip plane is ignored and the geometry is displayed unclipped.
    std::optional<bool> visible;
    // If true, the clip plane is ignored and the geometry is not displayed.
    std::optional<bool> hidden;
};

// Ported from: itwinjs-core RenderSchedule.CuttingPlaneEntryProps (:109-112)
struct CuttingPlaneEntryProps : TimelineEntryProps {
    // The clip plane, or absent if the geometry is not clipped.
    std::optional<CuttingPlaneProps> value;
};

// Ported from: itwinjs-core RenderSchedule.TransformComponentsProps (:115-122)
struct TransformComponentsProps {
    // (x,y,z) of position - applied after rotation.
    std::optional<std::array<double, 3>> position;
    // Quaternion representing rotation.
    std::optional<std::array<double, 4>> orientation;
    // (x,y,z) of pivot - applied before rotation.
    std::optional<std::array<double, 3>> pivot;
};

// Ported from: itwinjs-core RenderSchedule.TransformProps (:125-132)
struct TransformProps : TransformComponentsProps {
    // 3 X 4 transformation matrix: 3 rows [qx qy qz ax]; the fourth column of
    // each row holds the translation. Absent = identity transform. Only used if
    // position, orientation, and/or pivot are absent.
    std::optional<std::array<std::array<double, 4>, 3>> transform;
};

// Ported from: itwinjs-core RenderSchedule.TransformEntryProps (:135-138)
struct TransformEntryProps : TimelineEntryProps {
    // The transformation matrix, with absent corresponding to an identity matrix.
    std::optional<TransformProps> value;
};

// Ported from: itwinjs-core RenderSchedule.TimelineProps (:141-150)
struct TimelineProps {
    std::optional<std::vector<VisibilityEntryProps>> visibilityTimeline;
    std::optional<std::vector<ColorEntryProps>> colorTimeline;
    std::optional<std::vector<TransformEntryProps>> transformTimeline;
    std::optional<std::vector<CuttingPlaneEntryProps>> cuttingPlaneTimeline;
};

// Ported from: itwinjs-core RenderSchedule.ElementTimelineProps (:153-162)
struct ElementTimelineProps : TimelineProps {
    // A positive integer uniquely identifying this timeline among all element
    // timelines in the Script.
    uint32_t batchId = 0;
    // The Ids of the elements to which this timeline applies (array or
    // CompressedId64Set string). DanQing: variant 承载双形态（参考同形）。
    std::variant<dqBase::DqIdVector, dqBase::DqString> elementIds;
};

// Ported from: itwinjs-core RenderSchedule.ModelTimelineProps (:165-172)
struct ModelTimelineProps : TimelineProps {
    // The Id of the GeometricModel to which the timeline applies.
    dqBase::DqString modelId;
    // Defined only for "context" reality models.
    std::optional<dqBase::DqString> realityModelUrl;
    // Timelines affecting groups of elements.
    std::optional<std::vector<ElementTimelineProps>> elementTimelines;
};

// Ported from: itwinjs-core RenderSchedule.ScriptProps (:175)
using ScriptProps = std::vector<ModelTimelineProps>;

// ── Timeline entries ────────────────────────────────────────────────────────

// The value of some property at a specific point along a timeline.
// Ported from: itwinjs-core RenderSchedule.TimelineEntry (:183-212)
class TimelineEntry {
public:
    // The time point in seconds in the Unix Epoch.
    double time;
    // How to interpolate from this entry to the next entry in the timeline.
    Interpolation interpolation;

    explicit TimelineEntry(TimelineEntryProps const& props)
        : time(props.time)
        , interpolation(props.interpolation == Interpolation::Linear
                             ? Interpolation::Linear
                             : Interpolation::Step)
    {
    }

    virtual ~TimelineEntry() = default;

    TimelineEntryProps toJSON() const
    {
        TimelineEntryProps props;
        props.time = time;
        if (interpolation == Interpolation::Linear)
            props.interpolation = interpolation;
        return props;
    }

    int compareTo(TimelineEntry const& other) const
    {
        int const cmp = dqBase::compareNumbers(static_cast<int>(interpolation),
                                               static_cast<int>(other.interpolation));
        return 0 != cmp ? cmp : dqBase::compareNumbers(time, other.time);
    }

    bool equals(TimelineEntry const& other) const { return 0 == compareTo(other); }
};

// A timeline entry that controls the visibility of the associated geometry.
// Ported from: itwinjs-core RenderSchedule.VisibilityEntry (:215-241)
class VisibilityEntry : public TimelineEntry {
public:
    // Visibility in [0, 100]: 0 invisible, 100 fully visible.
    double value;

    explicit VisibilityEntry(VisibilityEntryProps const& props)
        : TimelineEntry(props)
        , value(props.value ? clampVisibility(*props.value) : 100.0)
    {
    }

    VisibilityEntryProps toJSON() const
    {
        VisibilityEntryProps props{TimelineEntry::toJSON()};
        if (100.0 != value)
            props.value = value;
        return props;
    }

    int compareTo(VisibilityEntry const& other) const
    {
        int const cmp = TimelineEntry::compareTo(other);
        return 0 != cmp ? cmp : dqBase::compareNumbers(value, other.value);
    }

    bool equals(VisibilityEntry const& other) const { return 0 == compareTo(other); }

    static double clampVisibility(double v) noexcept
    {
        return v < 0.0 ? 0.0 : (v > 100.0 ? 100.0 : v);
    }
};

// A timeline entry controlling the color of the affected geometry.
// Ported from: itwinjs-core RenderSchedule.ColorEntry (:244-271)
class ColorEntry : public TimelineEntry {
public:
    // If defined, the color in which to draw the geometry. If absent, the
    // geometry is drawn in its actual color.
    std::optional<RgbColor> value;

    explicit ColorEntry(ColorEntryProps const& props)
        : TimelineEntry(props)
    {
        if (props.value)
            value = RgbColor::fromJSON(&*props.value);
    }

    ColorEntryProps toJSON() const
    {
        ColorEntryProps props{TimelineEntry::toJSON()};
        if (value) {
            RgbColorProps rgb;
            rgb.r = value->r;
            rgb.g = value->g;
            rgb.b = value->b;
            props.value = rgb;
        }
        return props;
    }

    int compareTo(ColorEntry const& other) const
    {
        int const cmp = TimelineEntry::compareTo(other);
        if (0 != cmp)
            return cmp;
        return detail::comparePossiblyUndefinedOpt(
            [](RgbColor const& a, RgbColor const& b) { return a.compareTo(b); }, value,
            other.value);
    }

    bool equals(ColorEntry const& other) const { return 0 == compareTo(other); }
};

// Describes the components of a TransformEntry as a rotation around a pivot
// point followed by a translation.
// Ported from: itwinjs-core RenderSchedule.TransformComponents (:274-310)
class TransformComponents {
public:
    // Pivot point - applied before rotation.
    dqGeom::Vector3d pivot;
    // Quaternion rotation.
    dqGeom::Point4d orientation;
    // Translation - applied after rotation.
    dqGeom::Vector3d position;

    TransformComponents(dqGeom::Vector3d const& position_,
                        dqGeom::Vector3d const& pivot_,
                        dqGeom::Point4d const& orientation_)
        : pivot(pivot_)
        , orientation(orientation_)
        , position(position_)
    {
    }

    // Defined only if pivot && position && orientation are all present.
    // Ported from: itwinjs-core RenderSchedule.TransformComponents.fromJSON (:288-294)
    static std::optional<TransformComponents> fromJSON(TransformComponentsProps const& props)
    {
        if (props.pivot && props.position && props.orientation) {
            return TransformComponents(
                dqGeom::Vector3d::From((*props.position)[0], (*props.position)[1],
                                       (*props.position)[2]),
                dqGeom::Vector3d::From((*props.pivot)[0], (*props.pivot)[1], (*props.pivot)[2]),
                dqGeom::Point4d((*props.orientation)[0], (*props.orientation)[1],
                                (*props.orientation)[2], (*props.orientation)[3]));
        }
        return std::nullopt;
    }

    TransformComponentsProps toJSON() const
    {
        TransformComponentsProps props;
        props.position = std::array<double, 3>{{position.x, position.y, position.z}};
        props.pivot = std::array<double, 3>{{pivot.x, pivot.y, pivot.z}};
        props.orientation = std::array<double, 4>{
            {orientation.x, orientation.y, orientation.z, orientation.w}};
        return props;
    }

    int compareTo(TransformComponents const& other) const
    {
        int cmp = detail::compareXYZ(pivot, other.pivot);
        if (0 != cmp)
            return cmp;
        cmp = detail::compareXYZ(position, other.position);
        if (0 != cmp)
            return cmp;
        return detail::compare4d(orientation, other.orientation);
    }

    bool equals(TransformComponents const& other) const
    {
        return pivot.AlmostEqual(other.pivot) && position.AlmostEqual(other.position) &&
               orientation.isAlmostEqual(other.orientation);
    }
};

// A timeline entry that applies rotation, scaling, and/or translation to the
// affected geometry.
// Ported from: itwinjs-core RenderSchedule.TransformEntry (:313-353)
class TransformEntry : public TimelineEntry {
public:
    // The transform matrix to be applied to the geometry, used only if
    // components is not defined.
    dqGeom::Transform value;
    // The transform represented as a rotation about a pivot point followed by a
    // translation. If absent, value is used instead.
    std::optional<TransformComponents> components;

    explicit TransformEntry(TransformEntryProps const& props)
        : TimelineEntry(props)
        , value(dqGeom::Transform::CreateIdentity())
    {
        if (props.value) {
            if (props.value->transform)
                value = dqGeom::Transform::FromRows(*props.value->transform);
            components = TransformComponents::fromJSON(*props.value);
        }
    }

    TransformEntryProps toJSON() const
    {
        TransformEntryProps props{TimelineEntry::toJSON()};
        TransformProps val;
        if (components) {
            TransformComponentsProps const compProps = components->toJSON();
            val.position = compProps.position;
            val.orientation = compProps.orientation;
            val.pivot = compProps.pivot;
        }
        val.transform = value.ToRows();
        props.value = val;
        return props;
    }

    int compareTo(TransformEntry const& other) const
    {
        int const cmp = TimelineEntry::compareTo(other);
        if (0 != cmp)
            return cmp;

        if (components.has_value() || other.components.has_value()) {
            if (!components.has_value() || !other.components.has_value())
                return components.has_value() ? 1 : -1;
            return components->compareTo(*other.components);
        }

        int const cmp2 = detail::compareXYZ(value.origin, other.value.origin);
        return 0 != cmp2 ? cmp2 : detail::compareMatrices(value.matrix, other.value.matrix);
    }

    bool equals(TransformEntry const& other) const { return 0 == compareTo(other); }
};

// Defines a ClipPlane associated with a CuttingPlaneEntry.
// Ported from: itwinjs-core RenderSchedule.CuttingPlane (:356-395)
class CuttingPlane {
public:
    // A point on the plane.
    dqGeom::Point3d position;
    // The direction perpendicular to the plane pointing toward the clip.
    dqGeom::Vector3d direction;
    // If true, the clip plane is ignored and the geometry is never clipped.
    bool visible;
    // If true, the clip plane is ignored and the geometry is always clipped.
    bool hidden;

    explicit CuttingPlane(CuttingPlaneProps const& props)
        : position(dqGeom::Point3d::From(props.position[0], props.position[1], props.position[2]))
        , direction(dqGeom::Vector3d::From(props.direction[0], props.direction[1],
                                           props.direction[2]))
        , hidden(props.hidden.value_or(false))
        , visible(props.visible.value_or(false))
    {
    }

    CuttingPlaneProps toJSON() const
    {
        CuttingPlaneProps props;
        props.position = std::array<double, 3>{{position.x, position.y, position.z}};
        props.direction = std::array<double, 3>{{direction.x, direction.y, direction.z}};
        if (visible)
            props.visible = true;
        if (hidden)
            props.hidden = true;
        return props;
    }

    int compareTo(CuttingPlane const& other) const
    {
        int cmp = detail::compareXYZ(position, other.position);
        if (0 != cmp)
            return cmp;
        cmp = detail::compareXYZ(direction, other.direction);
        if (0 != cmp)
            return cmp;
        cmp = dqBase::compareBooleans(visible, other.visible);
        if (0 != cmp)
            return cmp;
        return dqBase::compareBooleans(hidden, other.hidden);
    }

    bool equals(CuttingPlane const& other) const { return 0 == compareTo(other); }
};

// A timeline entry that applies a ClipPlane to the affected geometry.
// Ported from: itwinjs-core RenderSchedule.CuttingPlaneEntry (:398-420)
class CuttingPlaneEntry : public TimelineEntry {
public:
    // The definition of the ClipPlane, or absent if this entry applies no clipping.
    std::optional<CuttingPlane> value;

    explicit CuttingPlaneEntry(CuttingPlaneEntryProps const& props)
        : TimelineEntry(props)
    {
        if (props.value)
            value = CuttingPlane(*props.value);
    }

    CuttingPlaneEntryProps toJSON() const
    {
        CuttingPlaneEntryProps props{TimelineEntry::toJSON()};
        if (value)
            props.value = value->toJSON();
        return props;
    }

    int compareTo(CuttingPlaneEntry const& other) const
    {
        int const cmp = TimelineEntry::compareTo(other);
        if (0 != cmp)
            return cmp;
        return detail::comparePossiblyUndefinedOpt(
            [](CuttingPlane const& a, CuttingPlane const& b) { return a.compareTo(b); }, value,
            other.value);
    }

    bool equals(CuttingPlaneEntry const& other) const { return 0 == compareTo(other); }
};

// ── Timeline entry lists ────────────────────────────────────────────────────

// A fractional position along a timeline between any two entries.
// Ported from: itwinjs-core RenderSchedule.Interval (:425-442)
class Interval {
public:
    // The index of the first timeline entry within the list.
    size_t lowerIndex = 0;
    // The index of the second timeline entry within the list.
    size_t upperIndex = 0;
    // The normalized distance between the two timeline entries.
    double fraction = 0.0;

    Interval() = default;
    Interval(size_t lower, size_t upper, double fraction_)
    {
        init(lower, upper, fraction_);
    }

    void init(size_t lower = 0, size_t upper = 0, double fraction_ = 0.0)
    {
        lowerIndex = lower;
        upperIndex = upper;
        fraction = fraction_;
    }
};

// A list of the TimelineEntry objects within a Timeline.
// Ported from: itwinjs-core RenderSchedule.TimelineEntryList (:449-536)
// C++ 适配：参考的泛型<T,P,V>+子类 getValue 默认覆写 → 模板基类持条目/时长/
// findInterval/compareTo/toJSON；四个具体条目表各自实现带参考缺省值的 getValue
//（可见性 100 / 变换 identity——参考 VisibilityTimelineEntries/
// TransformTimelineEntries 两处 override 原样；Color/CuttingPlane 参考为
// 匿名实例化，缺省=条目缺席即缺省，DanQing 统一具名）。
template<typename EntryT, typename PropsT> class TimelineEntryList {
public:
    // The entries in this list.
    std::vector<EntryT> m_entries;
    // The total time period represented by the entries in this list.
    dqGeom::Range1d m_duration;

    explicit TimelineEntryList(std::vector<PropsT> const& props)
    {
        m_duration = dqGeom::Range1d::CreateNull();
        m_entries.reserve(props.size());
        for (auto const& p : props) {
            m_entries.emplace_back(p);
            m_duration.Extend(m_entries.back().time);
        }
    }

    virtual ~TimelineEntryList() = default;

    // The number of entries in the list.
    size_t length() const { return m_entries.size(); }

    // Look up an entry by its position in the list (nullptr if out of range).
    EntryT const* getEntry(size_t index) const
    {
        return index < m_entries.size() ? &m_entries[index] : nullptr;
    }

    std::vector<PropsT> toJSON() const
    {
        std::vector<PropsT> props;
        props.reserve(m_entries.size());
        for (auto const& e : m_entries)
            props.push_back(e.toJSON());
        return props;
    }

    int compareTo(TimelineEntryList const& other) const
    {
        int cmp = dqBase::compareNumbers(static_cast<double>(m_entries.size()),
                                         static_cast<double>(other.m_entries.size()));
        if (0 == cmp)
            cmp = detail::compareDurations(m_duration, other.m_duration);
        if (0 == cmp) {
            for (size_t i = 0; i < length(); i++) {
                cmp = m_entries[i].compareTo(other.m_entries[i]);
                if (0 != cmp)
                    break;
            }
        }
        return cmp;
    }

    bool equals(TimelineEntryList const& other) const { return 0 == compareTo(other); }

    // Identify the interval containing the specified time, if any.
    // Ported from: itwinjs-core RenderSchedule.TimelineEntryList.findInterval (:503-535)
    std::optional<Interval> findInterval(double time, Interval* interval = nullptr) const
    {
        if (length() == 0)
            return std::nullopt;

        Interval scratch;
        Interval& iv = interval ? *interval : scratch;
        if (time < m_entries[0].time) {
            iv.init(0, 0, 0.0);
            return iv;
        }

        size_t const last = length() - 1;
        if (time >= m_entries[last].time) {
            iv.init(last, last, 0.0);
            return iv;
        }

        for (size_t i = 0; i < last; i++) {
            double const time0 = m_entries[i].time;
            double const time1 = m_entries[i + 1].time;
            if (time0 <= time && time1 >= time) {
                double fraction = 0.0;
                if (Interpolation::Linear == m_entries[i].interpolation)
                    fraction = (time - time0) / (time1 - time0);
                iv.init(i, i + 1, fraction);
                return iv;
            }
        }

        return std::nullopt;
    }
};

// A list of VisibilityEntrys within a Timeline.
// Ported from: itwinjs-core RenderSchedule.VisibilityTimelineEntries (:541-546)
class VisibilityTimelineEntries : public TimelineEntryList<VisibilityEntry, VisibilityEntryProps> {
public:
    using TimelineEntryList::TimelineEntryList;

    // The visibility value at the specified position, or 100 if no such entry.
    double getValue(size_t index) const
    {
        auto const* entry = getEntry(index);
        return entry ? entry->value : 100.0;
    }
};

// A list of ColorEntrys within a Timeline.
// Ported from: itwinjs-core RenderSchedule Timeline ctor 的 color 序列
// (:585-588 TimelineEntryList<ColorEntry, ColorEntryProps, RgbColor|undefined>)。
class ColorTimelineEntries : public TimelineEntryList<ColorEntry, ColorEntryProps> {
public:
    using TimelineEntryList::TimelineEntryList;

    // The color at the specified position, or absent if no such entry.
    std::optional<RgbColor> getValue(size_t index) const
    {
        auto const* entry = getEntry(index);
        return entry ? entry->value : std::optional<RgbColor>();
    }
};

// A list of TransformEntrys within a Timeline.
// Ported from: itwinjs-core RenderSchedule.TransformTimelineEntries (:549-554)
class TransformTimelineEntries : public TimelineEntryList<TransformEntry, TransformEntryProps> {
public:
    using TimelineEntryList::TimelineEntryList;

    // The transform at the specified position, or identity if no such entry.
    dqGeom::Transform getValue(size_t index) const
    {
        auto const* entry = getEntry(index);
        return entry ? entry->value : dqGeom::Transform::CreateIdentity();
    }
};

// A list of CuttingPlaneEntrys within a Timeline.
// Ported from: itwinjs-core RenderSchedule Timeline ctor 的 cuttingPlane 序列
// (:595-598 TimelineEntryList<CuttingPlaneEntry, CuttingPlaneEntryProps, ...>)。
class CuttingPlaneTimelineEntries
    : public TimelineEntryList<CuttingPlaneEntry, CuttingPlaneEntryProps> {
public:
    using TimelineEntryList::TimelineEntryList;

    // The cutting plane at the specified position, or absent if no such entry.
    std::optional<CuttingPlane> getValue(size_t index) const
    {
        auto const* entry = getEntry(index);
        return entry ? entry->value : std::optional<CuttingPlane>();
    }
};

// ── Timeline ────────────────────────────────────────────────────────────────

// Specifies how to animate a set of geometry over time within a Script.
// Ported from: itwinjs-core RenderSchedule.Timeline (:560-743)
class Timeline {
public:
    // Sequence controlling the visibility of the geometry.
    std::optional<VisibilityTimelineEntries> visibility;
    // Sequence controlling the color of the geometry.
    std::optional<ColorTimelineEntries> color;
    // Sequence controlling the position, orientation, and/or scale of the geometry.
    std::optional<TransformTimelineEntries> transform;
    // Sequence controlling how the geometry is clipped.
    std::optional<CuttingPlaneTimelineEntries> cuttingPlane;
    // The total time period represented by this timeline.
    dqGeom::Range1d duration;
    // Indicates whether the schedule editing session has been finalized and is
    // no longer active.
    bool isEditingCommitted = false;

    explicit Timeline(TimelineProps const& props)
    {
        duration = dqGeom::Range1d::CreateNull();

        if (props.visibilityTimeline) {
            visibility.emplace(*props.visibilityTimeline);
            duration.ExtendRange(visibility->m_duration);
        }
        if (props.colorTimeline) {
            color.emplace(*props.colorTimeline);
            duration.ExtendRange(color->m_duration);
        }
        if (props.transformTimeline) {
            transform.emplace(*props.transformTimeline);
            duration.ExtendRange(transform->m_duration);
        }
        if (props.cuttingPlaneTimeline) {
            cuttingPlane.emplace(*props.cuttingPlaneTimeline);
            duration.ExtendRange(cuttingPlane->m_duration);
        }
    }

    virtual ~Timeline() = default;

    TimelineProps toJSON() const
    {
        TimelineProps props;
        if (visibility)
            props.visibilityTimeline = visibility->toJSON();
        if (color)
            props.colorTimeline = color->toJSON();
        if (transform)
            props.transformTimeline = transform->toJSON();
        if (cuttingPlane)
            props.cuttingPlaneTimeline = cuttingPlane->toJSON();
        return props;
    }

    int compareTo(Timeline const& other) const
    {
        int const cmp = detail::compareDurations(duration, other.duration);
        if (0 != cmp)
            return cmp;

        // Do cheaper checks before iterating through timeline entries.
        if (visibility.has_value() != other.visibility.has_value())
            return visibility.has_value() ? 1 : -1;
        if (color.has_value() != other.color.has_value())
            return color.has_value() ? 1 : -1;
        if (transform.has_value() != other.transform.has_value())
            return transform.has_value() ? 1 : -1;
        if (cuttingPlane.has_value() != other.cuttingPlane.has_value())
            return cuttingPlane.has_value() ? 1 : -1;

        int cmp2 = detail::comparePossiblyUndefinedOpt(
            [](VisibilityTimelineEntries const& a, VisibilityTimelineEntries const& b) {
                return a.compareTo(b);
            },
            visibility, other.visibility);
        if (0 != cmp2)
            return cmp2;
        cmp2 = detail::comparePossiblyUndefinedOpt(
            [](ColorTimelineEntries const& a, ColorTimelineEntries const& b) {
                return a.compareTo(b);
            },
            color, other.color);
        if (0 != cmp2)
            return cmp2;
        cmp2 = detail::comparePossiblyUndefinedOpt(
            [](TransformTimelineEntries const& a, TransformTimelineEntries const& b) {
                return a.compareTo(b);
            },
            transform, other.transform);
        if (0 != cmp2)
            return cmp2;
        return detail::comparePossiblyUndefinedOpt(
            [](CuttingPlaneTimelineEntries const& a, CuttingPlaneTimelineEntries const& b) {
                return a.compareTo(b);
            },
            cuttingPlane, other.cuttingPlane);
    }

    bool equals(Timeline const& other) const { return 0 == compareTo(other); }

    // Get the visibility of the geometry at the specified time point.
    // Ported from: itwinjs-core RenderSchedule.Timeline.getVisibility (:634-644)
    double getVisibility(double time) const
    {
        if (!visibility)
            return 100.0;

        auto const found = visibility->findInterval(time);
        if (!found)
            return 100.0;

        double visibilityValue = visibility->getValue(found->lowerIndex);
        if (found->fraction > 0.0)
            visibilityValue = detail::interpolate(visibilityValue,
                                                  visibility->getValue(found->upperIndex),
                                                  found->fraction);
        return visibilityValue;
    }

    // Get the color of the geometry at the specified time point, or absent if
    // the color is not overridden at that time point.
    // Ported from: itwinjs-core RenderSchedule.Timeline.getColor (:647-660)
    std::optional<RgbColor> getColor(double time) const
    {
        if (!color)
            return std::nullopt;

        auto const found = color->findInterval(time);
        if (!found)
            return std::nullopt;

        auto const start = color->getValue(found->lowerIndex);
        if (start && found->fraction > 0.0) {
            auto const end = color->getValue(found->upperIndex);
            if (end)
                return detail::interpolateRgb(*start, *end, found->fraction);
        }

        return start;
    }

    // Get the transform applied to the geometry at the specified time point.
    // Ported from: itwinjs-core RenderSchedule.Timeline.getAnimationTransform (:663-696)
    dqGeom::Transform getAnimationTransform(double time) const
    {
        if (!transform)
            return dqGeom::Transform::CreateIdentity();

        auto const found = transform->findInterval(time);
        if (!found)
            return dqGeom::Transform::CreateIdentity();

        dqGeom::Transform transformValue = transform->getValue(found->lowerIndex);
        if (found->fraction > 0.0) {
            static std::optional<TransformComponents> const s_noComponents;
            auto const* entry0 = transform->getEntry(found->lowerIndex);
            auto const* entry1 = transform->getEntry(found->upperIndex);
            auto const& comp0 = entry0 ? entry0->components : s_noComponents;
            auto const& comp1 = entry1 ? entry1->components : s_noComponents;
            if (comp0 && comp1) {
                dqGeom::Point4d const sum = dqGeom::Point4d::interpolateQuaternions(
                    comp0->orientation, found->fraction, comp1->orientation);
                dqGeom::Matrix3d const matrix =
                    dqGeom::Matrix3d::CreateFromQuaternion(sum.x, sum.y, sum.z, sum.w);
                dqGeom::Transform const pre = dqGeom::Transform::CreateTranslation(comp0->pivot);
                dqGeom::Vector3d const interpPos = dqGeom::Vector3d::FromInterpolate(
                    comp0->position, found->fraction, comp1->position);
                dqGeom::Transform const post = dqGeom::Transform::CreateTranslation(interpPos);
                // product = post * matrix * pre（参考 multiplyTransformMatrix3d +
                // multiplyTransformTransform :677-678）。
                dqGeom::Transform product(post.origin, post.matrix.MultiplyMatrix(matrix));
                product = product.MultiplyTransform(pre);
                transformValue = product;
            } else {
                dqGeom::Transform const end = transform->getValue(found->upperIndex);
                dqGeom::Matrix3d inv0, inv1;
                if (transformValue.matrix.Inverse(inv0) && end.matrix.Inverse(inv1)) {
                    dqGeom::Point4d const q0 = inv0.ToQuaternion();
                    dqGeom::Point4d const q1 = inv1.ToQuaternion();
                    dqGeom::Point4d const sum =
                        dqGeom::Point4d::interpolateQuaternions(q0, found->fraction, q1);
                    dqGeom::Matrix3d const matrix =
                        dqGeom::Matrix3d::CreateFromQuaternion(sum.x, sum.y, sum.z, sum.w);
                    dqGeom::Vector3d const origin0 = dqGeom::Vector3d::From(
                        transformValue.origin.x, transformValue.origin.y, transformValue.origin.z);
                    dqGeom::Vector3d const origin1 = dqGeom::Vector3d::From(
                        end.origin.x, end.origin.y, end.origin.z);
                    dqGeom::Vector3d const interpOrigin =
                        dqGeom::Vector3d::FromInterpolate(origin0, found->fraction, origin1);
                    transformValue = dqGeom::Transform(
                        dqGeom::Point3d::From(interpOrigin.x, interpOrigin.y, interpOrigin.z),
                        matrix);
                }
            }
        }

        return transformValue;
    }

    // Get the clipping plane applied to the geometry at the specified time
    // point, or absent if the geometry is unclipped at that time point.
    // Ported from: itwinjs-core RenderSchedule.Timeline.getCuttingPlane (:699-723)
    std::optional<dqGeom::Plane3dByOriginAndUnitNormal> getCuttingPlane(double time) const
    {
        if (!cuttingPlane)
            return std::nullopt;

        auto const found = cuttingPlane->findInterval(time);
        if (!found)
            return std::nullopt;

        auto const start = cuttingPlane->getValue(found->lowerIndex);
        if (!start)
            return std::nullopt;

        dqGeom::Point3d position = start->position;
        dqGeom::Vector3d direction = start->direction;
        if (found->fraction > 0.0) {
            auto const end = cuttingPlane->getValue(found->upperIndex);
            if (end) {
                position.Interpolate(start->position, found->fraction, end->position);
                direction.Interpolate(start->direction, found->fraction, end->direction);
            } else {
                if (start->hidden || start->visible)
                    return std::nullopt;
            }
        }

        direction.Negate();
        direction.Normalize();
        return dqGeom::Plane3dByOriginAndUnitNormal::create(position, direction);
    }

    // Create a ClipVector from the CuttingPlane applied to the geometry at the
    // specified time point, if any.
    // Ported from: itwinjs-core RenderSchedule.Timeline.getClipVector (:726-735)
    dqGeom::ClipVector::Ptr getClipVector(double time) const
    {
        auto const plane = getCuttingPlane(time);
        if (!plane)
            return nullptr;

        auto const cp = dqGeom::ClipPlane::createPlane(*plane);
        auto const cps =
            dqGeom::ConvexClipPlaneSet::createPlanes(std::vector<dqGeom::ClipPlane>{cp});
        auto const unionSets = dqGeom::UnionOfConvexClipPlaneSets::createConvexSets({cps});
        auto const prim = dqGeom::ClipPrimitive::createCapture(unionSets);
        std::vector<dqGeom::ClipPrimitive::Ptr> prims;
        prims.push_back(prim);
        return dqGeom::ClipVector::createCapture(prims);
    }

protected:
    // FeatureAppearance (visibility transparency + color) for the specified
    // visibility at the specified time.
    // Ported from: itwinjs-core RenderSchedule.Timeline.getFeatureAppearance (:738-742)
    std::optional<FeatureAppearance> getFeatureAppearance(double visibility_, double time) const
    {
        std::optional<double> transparency;
        if (visibility_ < 100.0)
            transparency = 1.0 - visibility_ / 100.0;
        auto const rgb = getColor(time);

        if (!rgb && !transparency)
            return std::nullopt;

        FeatureAppearanceProps props;
        if (rgb) {
            RgbColorProps rgbProps;
            rgbProps.r = rgb->r;
            rgbProps.g = rgb->g;
            rgbProps.b = rgb->b;
            props.rgb = rgbProps;
        }
        props.transparency = transparency;
        return FeatureAppearance::fromJSON(&props);
    }
};

// ── ElementTimeline ─────────────────────────────────────────────────────────

// Specifies how to animate the geometry belonging to a set of GeometricElements.
// Ported from: itwinjs-core RenderSchedule.ElementTimeline (:746-857)
class ElementTimeline : public Timeline {
public:
    // A positive integer uniquely identifying this timeline among all
    // ElementTimelines in the Script.
    uint32_t batchId;

    explicit ElementTimeline(ElementTimelineProps const& props)
        : Timeline(props)
        , batchId(props.batchId)
        , m_elementIds(props.elementIds)
    {
    }

    static ElementTimeline fromJSON(ElementTimelineProps const& props)
    {
        return ElementTimeline(props);
    }

    ElementTimelineProps toJSON() const
    {
        ElementTimelineProps props{Timeline::toJSON()};
        props.batchId = batchId;
        props.elementIds = m_elementIds;
        return props;
    }

    int compareTo(ElementTimeline const& other) const
    {
        int cmp = dqBase::compareNumbers(batchId, other.batchId);
        if (0 != cmp)
            return cmp;
        cmp = compareElementIds(other);
        if (0 != cmp)
            return cmp;
        return Timeline::compareTo(other);
    }

    bool equals(ElementTimeline const& other) const { return 0 == compareTo(other); }

    // True if this timeline lists any element Ids.
    // Ported from: itwinjs-core RenderSchedule.ElementTimeline.containsElementIds (:769-771)
    bool containsElementIds() const
    {
        if (auto const* arr = std::get_if<dqBase::DqIdVector>(&m_elementIds))
            return !arr->empty();
        return !dqBase::CompressedId64Set::isEmpty(std::get<dqBase::DqString>(m_elementIds));
    }

    // The Ids of the elements controlled by this timeline.
    // Ported from: itwinjs-core RenderSchedule.ElementTimeline.elementIds (:819-821)
    dqBase::DqIdVector elementIds() const
    {
        if (auto const* arr = std::get_if<dqBase::DqIdVector>(&m_elementIds))
            return *arr;
        return dqBase::CompressedId64Set::DecompressArray(
            std::get<dqBase::DqString>(m_elementIds));
    }

    // True if this timeline affects the color or transparency of the elements.
    // Ported from: itwinjs-core RenderSchedule.ElementTimeline.containsFeatureOverrides (:824-826)
    bool containsFeatureOverrides() const
    {
        return visibility.has_value() || color.has_value();
    }

    // If true, applying this timeline requires special tiles with elements
    // batched into nodes.
    // Ported from: itwinjs-core RenderSchedule.ElementTimeline.requiresBatching (:831-836)
    bool requiresBatching() const
    {
        if (cuttingPlane.has_value())
            return true;

        return 0 != batchId && (color.has_value() || visibility.has_value());
    }

    // True if this timeline affects the position, orientation, or scale of the elements.
    // Ported from: itwinjs-core RenderSchedule.ElementTimeline.containsTransform (:839-841)
    bool containsTransform() const { return transform.has_value(); }

    // FeatureOverrides contribution at the specified time (batchId path).
    // Ported from: itwinjs-core RenderSchedule.ElementTimeline.addSymbologyOverrides (:844-856)
    void addSymbologyOverrides(FeatureOverrides& overrides, double time) const;

private:
    // Ported from: itwinjs-core RenderSchedule.ElementTimeline.compareElementIds (:773-800)
    int compareElementIds(ElementTimeline const& other) const
    {
        bool const mineCompressed = std::holds_alternative<dqBase::DqString>(m_elementIds);
        bool const theirsCompressed = std::holds_alternative<dqBase::DqString>(other.m_elementIds);

        if (mineCompressed == theirsCompressed) {
            if (mineCompressed) {
                // 参考同形态字符串走 compareStrings（CompressedId64Set 编码是
                // 规范形的——同集合同串，字典序等价展开序比较）。
                return dqBase::compareStrings(std::get<dqBase::DqString>(m_elementIds),
                                              std::get<dqBase::DqString>(other.m_elementIds));
            }

            auto const& a = std::get<dqBase::DqIdVector>(m_elementIds);
            auto const& b = std::get<dqBase::DqIdVector>(other.m_elementIds);
            int cmp = dqBase::compareNumbers(static_cast<double>(a.size()),
                                             static_cast<double>(b.size()));
            if (0 != cmp)
                return cmp;
            for (size_t i = 0; i < a.size(); i++) {
                if (a[i] != b[i])
                    return a[i] < b[i] ? -1 : 1;
            }
            return 0;
        }

        // One or both stored as arrays, in which case they might contain the
        // same Ids in different orders. We will consider them different in that
        // case (参考 :785-799 的混合形态展开序比较）。
        auto const mine = elementIds();
        auto const theirs = other.elementIds();
        size_t i = 0;
        for (; i < mine.size() && i < theirs.size(); i++) {
            if (mine[i] != theirs[i])
                return mine[i] < theirs[i] ? -1 : 1;
        }
        if (mine.size() != theirs.size())
            return mine.size() < theirs.size() ? -1 : 1;
        return 0;
    }

    // The Ids of the elements controlled by this timeline (original form:
    // array or CompressedId64Set string, returned verbatim by toJSON).
    std::variant<dqBase::DqIdVector, dqBase::DqString> m_elementIds;
};

// ── ModelTimeline ───────────────────────────────────────────────────────────

// Specifies how to animate the geometry within a GeometricModel.
// Ported from: itwinjs-core RenderSchedule.ModelTimeline (:860-1037)
class ModelTimeline : public Timeline {
public:
    // The Id of the GeometricModel to be animated.
    dqBase::DqString modelId;
    // Defined only for "context" reality models (attached through display style).
    std::optional<dqBase::DqString> realityModelUrl;
    // Timelines specifying how to animate groups of elements within the model.
    std::vector<ElementTimeline> elementTimelines;
    // The batch Ids of the subset of elementTimelines that apply a transform.
    std::vector<uint32_t> transformBatchIds;
    // True if this timeline affects the color or transparency of the geometry.
    bool containsFeatureOverrides = false;
    // True if this timeline applies clipping to the model.
    bool containsModelClipping = false;
    // If true, applying this timeline requires special tiles with elements
    // batched into nodes.
    bool requiresBatching = false;
    // True if this timeline affects the position, orientation, or scale of the geometry.
    bool containsTransform = false;
    // True if any ElementTimelines exist and none of them contain element Ids.
    bool omitsElementIds = false;

    explicit ModelTimeline(ModelTimelineProps const& props)
        : Timeline(props)
        , modelId(props.modelId)
        , realityModelUrl(props.realityModelUrl)
    {
        containsModelClipping = cuttingPlane.has_value();

        bool containsElementIds = false;
        if (props.elementTimelines) {
            elementTimelines.reserve(props.elementTimelines->size());
            for (auto const& elProps : *props.elementTimelines) {
                elementTimelines.push_back(ElementTimeline::fromJSON(elProps));
                auto const& el = elementTimelines.back();

                duration.ExtendRange(el.duration);

                if (el.containsTransform()) {
                    containsTransform = true;
                    if (el.batchId)
                        transformBatchIds.push_back(el.batchId);
                }

                containsFeatureOverrides =
                    containsFeatureOverrides || el.containsFeatureOverrides();
                requiresBatching = requiresBatching || el.requiresBatching();
                containsElementIds = containsElementIds || el.containsElementIds();
            }
        }

        omitsElementIds = !elementTimelines.empty() && !containsElementIds;
    }

    static ModelTimeline fromJSON(ModelTimelineProps const& props)
    {
        return ModelTimeline(props);
    }

    ModelTimelineProps toJSON() const
    {
        ModelTimelineProps props{Timeline::toJSON()};
        props.modelId = modelId;
        props.realityModelUrl = realityModelUrl;
        std::vector<ElementTimelineProps> els;
        els.reserve(elementTimelines.size());
        for (auto const& el : elementTimelines)
            els.push_back(el.toJSON());
        props.elementTimelines = std::move(els);
        return props;
    }

    // Ported from: itwinjs-core RenderSchedule.ModelTimeline.compareTo (:943-966)
    // （WeakMap 缓存腿删除——EQUIVALENCE E6；比较字段序照参考原文。）
    int compareTo(ModelTimeline const& other) const
    {
        if (this == &other)
            return 0;

        int cmp = dqBase::compareStrings(modelId, other.modelId);
        if (0 != cmp)
            return cmp;
        cmp = detail::compareStringsOpt(realityModelUrl, other.realityModelUrl);
        if (0 != cmp)
            return cmp;
        cmp = dqBase::compareNumbers(static_cast<double>(elementTimelines.size()),
                                     static_cast<double>(other.elementTimelines.size()));
        if (0 != cmp)
            return cmp;
        cmp = dqBase::compareBooleans(containsFeatureOverrides, other.containsFeatureOverrides);
        if (0 != cmp)
            return cmp;
        cmp = dqBase::compareBooleans(containsModelClipping, other.containsModelClipping);
        if (0 != cmp)
            return cmp;
        cmp = dqBase::compareBooleans(containsTransform, other.containsTransform);
        if (0 != cmp)
            return cmp;
        cmp = Timeline::compareTo(other);
        if (0 != cmp)
            return cmp;

        for (size_t i = 0; i < elementTimelines.size(); i++) {
            cmp = elementTimelines[i].compareTo(other.elementTimelines[i]);
            if (0 != cmp)
                break;
        }
        return cmp;
    }

    bool equals(ModelTimeline const& other) const { return 0 == compareTo(other); }

    // Look up the element timeline with the specified batch Id.
    ElementTimeline const* findByBatchId(uint32_t batchId_) const
    {
        for (auto const& el : elementTimelines)
            if (el.batchId == batchId_)
                return &el;
        return nullptr;
    }

    // FeatureOverrides contribution at the specified time (model-level + element).
    // Ported from: itwinjs-core RenderSchedule.ModelTimeline.addSymbologyOverrides (:974-981)
    void addSymbologyOverrides(FeatureOverrides& overrides, double time) const;

    // Obtain the transform applied to the model at the specified time point, if any.
    // Ported from: itwinjs-core RenderSchedule.ModelTimeline.getTransform (:984-986)
    std::optional<dqGeom::Transform> getTransform(uint32_t batchId_, double time) const
    {
        auto const* timeline = findByBatchId(batchId_);
        if (!timeline)
            return std::nullopt;
        return timeline->getAnimationTransform(time);
    }

    // Get the highest batchId of any ElementTimeline in this timeline.
    // Ported from: itwinjs-core RenderSchedule.ModelTimeline.maxBatchId (:989-997)
    uint32_t maxBatchId() const
    {
        if (!m_maxBatchId.has_value()) {
            uint32_t maxId = 0;
            for (auto const& el : elementTimelines)
                maxId = std::max(maxId, el.batchId);
            m_maxBatchId = maxId;
        }
        return *m_maxBatchId;
    }

    // Given the two halves of an Id64, return the ElementTimeline containing the
    // corresponding element. First call populates the lookup map.
    // Ported from: itwinjs-core RenderSchedule.ModelTimeline.getTimelineForElement (:1003-1015)
    ElementTimeline const* getTimelineForElement(uint32_t idLo, uint32_t idHi) const
    {
        if (!m_idPairToElementTimeline) {
            m_idPairToElementTimeline.emplace();
            for (auto const& timeline : elementTimelines) {
                for (auto const& elementId : timeline.elementIds()) {
                    // NB: a malformed script may place the same element Id into
                    // multiple timelines. We're not going to check for such data
                    // errors here.
                    m_idPairToElementTimeline->emplace(elementId.GetValue(), &timeline);
                }
            }
        }

        auto const key = dqBase::Id64::FromUint32Pair(idLo, idHi).GetValue();
        auto const it = m_idPairToElementTimeline->find(key);
        return it != m_idPairToElementTimeline->end() ? it->second : nullptr;
    }

    // The batch Ids of the subset of elementTimelines that apply a transform
    // and/or cutting plane.
    // Ported from: itwinjs-core RenderSchedule.ModelTimeline.discreteBatchIds (:1020-1029)
    std::set<uint32_t> const& discreteBatchIds() const
    {
        if (!m_discreteBatchIds) {
            m_discreteBatchIds.emplace();
            for (auto const& timeline : elementTimelines) {
                if (!timeline.containsTransform() && timeline.cuttingPlane.has_value())
                    m_discreteBatchIds->insert(timeline.batchId);
            }
            for (auto const batchId_ : transformBatchIds)
                m_discreteBatchIds->insert(batchId_);
        }
        return *m_discreteBatchIds;
    }

    // The batch Id for the specified feature (see ImdlReader.readAnimationBranches).
    // Ported from: itwinjs-core RenderSchedule.ModelTimeline.getBatchIdForFeature (:1032-1036)
    uint32_t getBatchIdForFeature(uint32_t elemLo, uint32_t elemHi) const
    {
        auto const* elementTimeline = getTimelineForElement(elemLo, elemHi);
        return elementTimeline ? elementTimeline->batchId : 0;
    }

private:
    // Lookup map initialized on first getTimelineForElement call (参考 :887）。
    mutable std::optional<std::unordered_map<uint64_t, ElementTimeline const*>>
        m_idPairToElementTimeline;
    mutable std::optional<std::set<uint32_t>> m_discreteBatchIds;
    mutable std::optional<uint32_t> m_maxBatchId;
};

// ── Script ──────────────────────────────────────────────────────────────────

// Specifies how to animate the contents of a ViewState over time.
// Ported from: itwinjs-core RenderSchedule.Script (:1046-1228)
class Script {
public:
    // Timelines specifying how to animate individual models within the view.
    std::vector<ModelTimeline> modelTimelines;
    // True if this script applies clipping to any models.
    bool containsModelClipping = false;
    // If true, applying this timeline requires special tiles with elements batched into nodes.
    bool requiresBatching = false;
    // True if this script affects the position, orientation, or scale of the geometry.
    bool containsTransform = false;
    // True if this script affects the color or transparency of the geometry.
    bool containsFeatureOverrides = false;
    // The total time period over which this script animates.
    dqGeom::Range1d duration;
    // The batchIds of all nodes in all timelines that apply a transform.
    std::set<uint32_t> transformBatchIds;

    explicit Script(ScriptProps const& props)
    {
        duration = dqGeom::Range1d::CreateNull();

        for (auto const& modelProps : props) {
            modelTimelines.push_back(ModelTimeline::fromJSON(modelProps));
            auto const& model = modelTimelines.back();

            duration.ExtendRange(model.duration);

            containsModelClipping = containsModelClipping || model.containsModelClipping;
            requiresBatching = requiresBatching || model.requiresBatching;
            containsTransform = containsTransform || model.containsTransform;
            containsFeatureOverrides = containsFeatureOverrides || model.containsFeatureOverrides;

            for (auto const batchId_ : model.transformBatchIds)
                transformBatchIds.insert(batchId_);
        }

        requiresBatching = requiresBatching || containsTransform;
    }

    // Returns nullptr if props is empty (参考 :1130-1134 的空数组拒绝）。
    static std::unique_ptr<Script> fromJSON(ScriptProps const& props)
    {
        if (props.empty())
            return nullptr;
        return std::make_unique<Script>(props);
    }

    ScriptProps toJSON() const
    {
        ScriptProps props;
        props.reserve(modelTimelines.size());
        for (auto const& model : modelTimelines)
            props.push_back(model.toJSON());
        return props;
    }

    // Ported from: itwinjs-core RenderSchedule.Script.compareTo (:1071-1092)
    // （WeakMap 缓存腿删除——EQUIVALENCE E6；比较字段序照参考原文。）
    int compareTo(Script const& other) const
    {
        if (this == &other)
            return 0;

        int cmp = dqBase::compareNumbers(static_cast<double>(modelTimelines.size()),
                                         static_cast<double>(other.modelTimelines.size()));
        if (0 == cmp)
            cmp = dqBase::compareBooleans(containsModelClipping, other.containsModelClipping);
        if (0 == cmp)
            cmp = dqBase::compareBooleans(requiresBatching, other.requiresBatching);
        if (0 == cmp)
            cmp = dqBase::compareBooleans(containsTransform, other.containsTransform);
        if (0 == cmp)
            cmp = dqBase::compareBooleans(containsFeatureOverrides, other.containsFeatureOverrides);
        if (0 == cmp)
            cmp = detail::compareDurations(duration, other.duration);

        if (0 == cmp) {
            for (size_t i = 0; i < modelTimelines.size(); i++) {
                cmp = modelTimelines[i].compareTo(other.modelTimelines[i]);
                if (0 != cmp)
                    break;
            }
        }

        return cmp;
    }

    bool equals(Script const& other) const { return 0 == compareTo(other); }

    // Look up the timeline that animates the specified model, if any.
    ModelTimeline const* find(dqBase::DqString const& modelId_) const
    {
        for (auto const& timeline : modelTimelines)
            if (timeline.modelId == modelId_)
                return &timeline;
        return nullptr;
    }

    // The transform batch Ids for the specified model, if any.
    std::vector<uint32_t> const* getTransformBatchIds(dqBase::DqString const& modelId_) const
    {
        auto const* timeline = find(modelId_);
        return timeline ? &timeline->transformBatchIds : nullptr;
    }

    // The transform applied to the specified model/batch at the specified time.
    // Ported from: itwinjs-core RenderSchedule.Script.getTransform (:1152-1154)
    std::optional<dqGeom::Transform> getTransform(dqBase::DqString const& modelId_,
                                                  uint32_t batchId_, double time) const
    {
        auto const* timeline = find(modelId_);
        if (!timeline)
            return std::nullopt;
        return timeline->getTransform(batchId_, time);
    }

    // FeatureOverrides contribution of every model timeline at the specified time.
    // Ported from: itwinjs-core RenderSchedule.Script.addSymbologyOverrides (:1157-1160)
    void addSymbologyOverrides(FeatureOverrides& overrides, double time) const;

    // Used by Entity.collectReferenceIds in RenderTimeline and DisplayStyle.
    // C++ 适配：EntityReferenceSet（backend 面）→ 两 id 出参（dqCommon 无
    // EntityReferenceSet 类型；集合语义不变，登记）。
    // Ported from: itwinjs-core RenderSchedule.Script.discloseIds (:1165-1172)
    void discloseIds(std::vector<dqBase::DqId>& modelIds,
                     std::vector<dqBase::DqId>& elementIds) const
    {
        for (auto const& model : modelTimelines) {
            modelIds.push_back(dqBase::DqId::FromString(model.modelId));
            for (auto const& element : model.elementTimelines)
                for (auto const& id : element.elementIds())
                    elementIds.push_back(id);
        }
    }

    // Only if the script contains animation (cutting plane, transform or
    // visibility by node ID) do we require separate tilesets for animations.
    // Ported from: itwinjs-core RenderSchedule.Script.modelRequiresBatching (:1175-1178)
    bool modelRequiresBatching(dqBase::DqString const& modelId_) const
    {
        if (!requiresBatching)
            return false;
        for (auto const& timeline : modelTimelines)
            if (timeline.modelId == modelId_ && timeline.requiresBatching)
                return true;
        return false;
    }

    // The batch Ids of the subset of elementTimelines that apply a transform
    // and/or cutting plane, across all model timelines.
    // Ported from: itwinjs-core RenderSchedule.Script.discreteBatchIds (:1183-1193)
    std::set<uint32_t> const& discreteBatchIds() const
    {
        if (!m_discreteBatchIds) {
            m_discreteBatchIds.emplace();
            for (auto const& model : modelTimelines)
                for (auto const batchId_ : model.discreteBatchIds())
                    m_discreteBatchIds->insert(batchId_);
        }
        return *m_discreteBatchIds;
    }

    // The batch Id for the specified feature. C++ 适配：参考的
    // PackedFeatureWithIndex 入参 → model/element 两对 lo/hi（ImdlReader 消费
    // 侧同形签名；_lastFeatureModelTimeline 缓存腿原样保留）。
    // Ported from: itwinjs-core RenderSchedule.Script.getBatchIdForFeature (:1196-1208)
    uint32_t getBatchIdForFeature(uint32_t modelLo, uint32_t modelHi,
                                  uint32_t elemLo, uint32_t elemHi) const
    {
        ModelTimeline const* timeline = nullptr;
        if (m_lastFeatureModelTimeline &&
            m_lastFeatureModelTimeline->idLower == modelLo &&
            m_lastFeatureModelTimeline->idUpper == modelHi) {
            timeline = m_lastFeatureModelTimeline->timeline;
        } else {
            auto const modelId = dqBase::Id64::FromUint32Pair(modelLo, modelHi);
            timeline = find(modelId.ToString());
            m_lastFeatureModelTimeline = LastFeatureModelTimeline{timeline, modelLo, modelHi};
        }

        return timeline ? timeline->getBatchIdForFeature(elemLo, elemHi) : 0;
    }

    // The highest batchId of any ElementTimeline in the script.
    // Ported from: itwinjs-core RenderSchedule.Script.maxBatchId (:1211-1213)
    uint32_t maxBatchId() const
    {
        if (!m_maxBatchId.has_value()) {
            uint32_t maxId = 0;
            for (auto const& timeline : modelTimelines)
                maxId = std::max(maxId, timeline.maxBatchId());
            m_maxBatchId = maxId;
        }
        return *m_maxBatchId;
    }

    // Replaces all elementIds in a ScriptProps object with an empty string.
    // Ported from: itwinjs-core RenderSchedule.Script.removeScheduleScriptElementIds (:1219-1228)
    static ScriptProps removeScheduleScriptElementIds(ScriptProps scheduleScript)
    {
        for (auto& modelTimeline : scheduleScript) {
            if (modelTimeline.elementTimelines) {
                for (auto& elementTimeline : *modelTimeline.elementTimelines)
                    elementTimeline.elementIds = dqBase::DqString();
            }
        }
        return scheduleScript;
    }

private:
    struct LastFeatureModelTimeline {
        ModelTimeline const* timeline = nullptr;
        uint32_t idLower = 0;
        uint32_t idUpper = 0;
    };
    mutable std::optional<LastFeatureModelTimeline> m_lastFeatureModelTimeline;
    mutable std::optional<std::set<uint32_t>> m_discreteBatchIds;
    mutable std::optional<uint32_t> m_maxBatchId;
};

// ── ScriptReference ─────────────────────────────────────────────────────────

// A reference to a Script, optionally identifying the source of the script.
// Ported from: itwinjs-core RenderSchedule.ScriptReference (:1235-1270)
class ScriptReference {
public:
    // The Id of the element - if any - from which the script originated.
    dqBase::DqString sourceId;
    // The script defining the rendering timelines to be applied.
    Script script;

    // Create a reference to a script with no sourceId.
    explicit ScriptReference(Script const& script_)
        : sourceId("0")
        , script(script_)
    {
    }

    // Create a reference to a script with the specified sourceId.
    ScriptReference(dqBase::DqString const& sourceId_, Script const& script_)
        : sourceId(sourceId_)
        , script(script_)
    {
    }
};

// ── ScriptBuilder ───────────────────────────────────────────────────────────

// Assembles a Timeline. Entries must be appended in chronological order.
// Ported from: itwinjs-core RenderSchedule.TimelineBuilder (:1277-1360)
class TimelineBuilder {
public:
    std::optional<std::vector<VisibilityEntryProps>> visibility;
    std::optional<std::vector<ColorEntryProps>> color;
    std::optional<std::vector<TransformEntryProps>> transform;
    std::optional<std::vector<CuttingPlaneEntryProps>> cuttingPlane;

    virtual ~TimelineBuilder() = default;

    // Append a new VisibilityEntry. `time` must be more recent than any
    // previously-appended visibility entries.
    void addVisibility(double time, std::optional<double> visibility_,
                       Interpolation interpolation = Interpolation::Linear)
    {
        if (!this->visibility)
            this->visibility.emplace();
        VisibilityEntryProps props;
        props.time = time;
        props.value = visibility_;
        props.interpolation = interpolation;
        this->visibility->push_back(props);
    }

    // Append a new ColorEntry.
    void addColor(double time, std::optional<RgbColor> color_,
                  Interpolation interpolation = Interpolation::Linear)
    {
        if (!this->color)
            this->color.emplace();
        ColorEntryProps props;
        props.time = time;
        if (color_) {
            RgbColorProps rgb;
            rgb.r = color_->r;
            rgb.g = color_->g;
            rgb.b = color_->b;
            props.value = rgb;
        }
        props.interpolation = interpolation;
        this->color->push_back(props);
    }

    // Append a new CuttingPlaneEntry.
    void addCuttingPlane(double time, std::optional<CuttingPlaneProps> plane,
                         Interpolation interpolation = Interpolation::Linear)
    {
        if (!this->cuttingPlane)
            this->cuttingPlane.emplace();
        CuttingPlaneEntryProps props;
        props.time = time;
        props.value = plane;
        props.interpolation = interpolation;
        this->cuttingPlane->push_back(props);
    }

    // Append a new TransformEntry.
    // C++ 适配：参考的 components 对象字面量入参 → TransformComponentsProps。
    void addTransform(double time, std::optional<dqGeom::Transform> const& transform_,
                      std::optional<TransformComponentsProps> components = std::nullopt,
                      Interpolation interpolation = Interpolation::Linear)
    {
        if (!this->transform)
            this->transform.emplace();
        TransformEntryProps props;
        props.time = time;
        TransformProps val;
        if (transform_)
            val.transform = transform_->ToRows();
        if (components) {
            val.position = components->position;
            val.orientation = components->orientation;
            val.pivot = components->pivot;
        }
        props.value = val;
        props.interpolation = interpolation;
        this->transform->push_back(props);
    }

    // Obtain the JSON representation of the Timeline produced by this builder.
    // C++ 适配：参考子类以宽化返回类型 override finish()（TS 合法）——C++ 按值
    // 返回不支持协变，base 改非虚、子类同名隐藏（调用面均为具体类型，无语义损失）。
    TimelineProps finish() const
    {
        TimelineProps props;
        if (visibility && !visibility->empty())
            props.visibilityTimeline = *visibility;
        if (color && !color->empty())
            props.colorTimeline = *color;
        if (transform && !transform->empty())
            props.transformTimeline = *transform;
        if (cuttingPlane && !cuttingPlane->empty())
            props.cuttingPlaneTimeline = *cuttingPlane;
        return props;
    }
};

// Assembles an ElementTimeline as part of a ScriptBuilder.
// Ported from: itwinjs-core RenderSchedule.ElementTimelineBuilder (:1365-1391)
class ElementTimelineBuilder : public TimelineBuilder {
public:
    // A positive integer uniquely identifying this timeline among all element
    // timelines in the Script. ScriptBuilder ensures each ElementTimelineBuilder
    // receives a unique batch Id.
    uint32_t batchId;
    // The compressed set of Ids of the elements affected by this timeline.
    dqBase::DqString elementIds;

    ElementTimelineBuilder(uint32_t batchId_, dqBase::DqString elementIds_)
        : batchId(batchId_)
        , elementIds(std::move(elementIds_))
    {
    }

    ElementTimelineProps finish() const
    {
        ElementTimelineProps props{TimelineBuilder::finish()};
        props.batchId = batchId;
        props.elementIds = elementIds;
        return props;
    }
};

// Assembles a ModelTimeline as part of a ScriptBuilder.
// Ported from: itwinjs-core RenderSchedule.ModelTimelineBuilder (:1396-1451)
class ModelTimelineBuilder : public TimelineBuilder {
public:
    // The Id of the model affected by this timeline.
    dqBase::DqString modelId;
    std::optional<std::string> realityModelUrl;

    ModelTimelineBuilder(dqBase::DqString modelId_, std::function<uint32_t()> obtainNextBatchId)
        : modelId(std::move(modelId_))
        , m_obtainNextBatchId(std::move(obtainNextBatchId))
    {
    }

    // Add a new ElementTimeline to be applied to the specified elements.
    // This function will sort and compress the Ids if they are not already compressed.
    // C++ 适配：参考的单 Id 字符串/压缩串/Iterable 三形态入参 → 两个重载
    //（string 重载内做单 Id 包装判断，忠实 :1422-1432；容器用 deque 保引用稳定）。
    ElementTimelineBuilder& addElementTimeline(dqBase::DqString const& elementIds)
    {
        auto const batchId_ = m_obtainNextBatchId();
        dqBase::DqString ids;

        // It's far too easy to accidentally pass a single Id.
        auto const single = dqBase::DqId::FromString(elementIds);
        if (single.isValid()) {
            dqBase::DqIdSet set;
            set.insert(single);
            ids = dqBase::CompressedId64Set::SortAndCompress(set);
        } else {
            // Already compressed.
            ids = elementIds;
        }

        m_elements.emplace_back(batchId_, std::move(ids));
        return m_elements.back();
    }

    // Array form: sort and compress the Ids.
    ElementTimelineBuilder& addElementTimeline(dqBase::DqIdVector const& elementIds)
    {
        auto const batchId_ = m_obtainNextBatchId();
        dqBase::DqIdSet set;
        for (auto const& id : elementIds)
            set.insert(id);
        m_elements.emplace_back(batchId_, dqBase::CompressedId64Set::SortAndCompress(set));
        return m_elements.back();
    }

    ModelTimelineProps finish() const
    {
        ModelTimelineProps props{TimelineBuilder::finish()};
        props.modelId = modelId;
        if (realityModelUrl.has_value())
            props.realityModelUrl = dqBase::DqString(*realityModelUrl);

        std::vector<ElementTimelineProps> els;
        els.reserve(m_elements.size());
        for (auto const& e : m_elements)
            els.push_back(e.finish());
        props.elementTimelines = std::move(els);
        return props;
    }

private:
    std::function<uint32_t()> m_obtainNextBatchId;
    std::deque<ElementTimelineBuilder> m_elements;
};

// Assembles the JSON representation for a new Script. Ensure that entries on
// any given element timeline are added chronologically.
// Ported from: itwinjs-core RenderSchedule.ScriptBuilder (:1462-1479)
class ScriptBuilder {
public:
    // Add a new ModelTimeline to be applied to the specified model.
    ModelTimelineBuilder& addModelTimeline(dqBase::DqString const& modelId)
    {
        auto const nextId = [this] { return m_nextBatchId++; };
        m_models.emplace_back(modelId, nextId);
        return m_models.back();
    }

    // Obtain the JSON representation of the Script produced by this builder.
    ScriptProps finish() const
    {
        ScriptProps props;
        props.reserve(m_models.size());
        for (auto const& m : m_models)
            props.push_back(m.finish());
        return props;
    }

private:
    uint32_t m_nextBatchId = 1;
    std::deque<ModelTimelineBuilder> m_models;
};

}  // namespace RenderSchedule

END_DQ_COMMON_NAMESPACE

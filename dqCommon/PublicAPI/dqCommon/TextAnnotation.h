// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — text annotation
// Ported from: itwinjs-core core/common/src/annotation/TextAnnotation.ts
#pragma once

#include "DqCommon.h"
#include "TextBlock.h"
#include "TextStyle.h"

#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>  // dqGeom::Range2d 宿主（既有 2D range，Point3d z=0 形）
#include <dqGeom/Transform.h>
#include <dqGeom/YawPitchRollAngles.h>

#include <optional>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Anchor vertical: "top" | "middle" | "bottom".
// Ported from: TextAnnotationAnchor.vertical (TextAnnotation.ts:28)
namespace AnchorVertical {
    inline constexpr char const* Top = "top";
    inline constexpr char const* Middle = "middle";
    inline constexpr char const* Bottom = "bottom";
}

// Anchor horizontal: "left" | "center" | "right".
// Ported from: TextAnnotationAnchor.horizontal (TextAnnotation.ts:36)
namespace AnchorHorizontal {
    inline constexpr char const* Left = "left";
    inline constexpr char const* Center = "center";
    inline constexpr char const* Right = "right";
}

// How to attach a leader to the frame around a TextBlock — KeyPoint/TextPoint/
// Nearest discriminated union.
// Ported from: itwinjs-core LeaderAttachment (TextAnnotation.ts:47-50) —
// TS 判别联合 → kind 标签 + 载荷（§3.4 适配）。
// Ported from: LeaderTextPointOptions (TextAnnotation.ts:61)
struct LeaderAttachment {
    // TextPoint 模式的四角挂点。
    enum class TextPoint : int32_t { TopLeft = 0, TopRight, BottomLeft, BottomRight };
    enum class Mode : int32_t { KeyPoint = 0, TextPoint, Nearest };
    Mode mode = Mode::Nearest;
    // KeyPoint 载荷。
    uint32_t curveIndex = 0;
    double fraction = 0.0;
    // TextPoint 载荷。
    TextPoint position = TextPoint::TopLeft;

    // 参考 equals 以 JSON.stringify 比较附件（TextAnnotation.ts:293）——全字段
    // 等值（判别联合形态固定）。
    bool operator==(LeaderAttachment const& other) const noexcept
    {
        if (mode != other.mode)
            return false;
        switch (mode) {
            case Mode::KeyPoint: return curveIndex == other.curveIndex && fraction == other.fraction;
            case Mode::TextPoint: return position == other.position;
            case Mode::Nearest: return true;
        }
        return false;
    }
};

// A line connecting a TextAnnotation to a point in space relative to another
// element in the iModel.
// Ported from: itwinjs-core TextAnnotationLeader (TextAnnotation.ts:71-85)
struct TextAnnotationLeader {
    // The point where the leader starts（指向另一元素上的点）。
    dqGeom::Point3d startPoint;
    // How the leader attaches to the frame around the textBlock.
    LeaderAttachment attachment;
    // Optional intermediate points the leader passes through（缺席 = 直线）。
    std::vector<dqGeom::Point3d> intermediatePoints;
    // Optional style overrides for this leader.
    std::optional<TextStyleSettingsProps> styleOverrides;

    // 参考 styleOverrides 以 JSON.stringify 比较（TextAnnotation.ts:294）——
    // 经 TextStyleSettings 全字段 equals（缺席==缺席，TextBlock.h propsEquals 同式）。
    bool operator==(TextAnnotationLeader const& other) const noexcept
    {
        bool const soEq =
            styleOverrides.has_value() == other.styleOverrides.has_value()
            && (!styleOverrides.has_value()
                || TextStyleSettings::fromJSON(&*styleOverrides)
                       .equals(TextStyleSettings::fromJSON(&*other.styleOverrides)));
        return startPoint.AlmostEqual(other.startPoint, 1.0e-6)  // isAlmostEqual 默认容差
            && attachment == other.attachment && soEq
            && intermediatePoints.size() == other.intermediatePoints.size()
            && std::equal(intermediatePoints.begin(), intermediatePoints.end(),
                          other.intermediatePoints.begin(),
                          [](dqGeom::Point3d const& a, dqGeom::Point3d const& b) {
                              return a.AlmostEqual(b, 1.0e-6);
                          });
    }
};

// Properties for a TextAnnotation.
// Ported from: itwinjs-core TextAnnotationProps (TextAnnotation.ts:105-116)
struct TextAnnotationProps {
    std::optional<dqGeom::Point3d> offset;
    std::optional<dqGeom::YawPitchRollAngles> orientation;
    // children 携实例（§3.4 同 TextBlock.h 登记——TextBlock.create(props) 对偶面
    // 由调用方构造）。
    std::optional<TextBlock::Props> textBlock;
    struct Anchor {
        std::string vertical = AnchorVertical::Top;      // AnchorVertical 值域
        std::string horizontal = AnchorHorizontal::Left; // AnchorHorizontal 值域
        bool operator==(Anchor const& o) const noexcept
        {
            return vertical == o.vertical && horizontal == o.horizontal;
        }
    };
    std::optional<Anchor> anchor;
    std::vector<TextAnnotationLeader> leaders;
};

// Arguments supplied to TextAnnotation.create.
// Ported from: itwinjs-core TextAnnotationCreateArgs (TextAnnotation.ts:121-132)
struct TextAnnotationCreateArgs {
    std::optional<dqGeom::Point3d> offset;
    std::optional<dqGeom::YawPitchRollAngles> orientation;
    std::optional<TextBlock> textBlock;  // 克隆持有
    TextAnnotationProps::Anchor anchor;  // 默认 top-left
    std::vector<TextAnnotationLeader> leaders;
};

// Represents a formatted block of text positioned in 2d or 3d space.
// Ported from: itwinjs-core TextAnnotation (TextAnnotation.ts:147-318)
class DQ_COMMON_EXPORT TextAnnotation {
public:
    using Anchor = TextAnnotationProps::Anchor;

    // The rotation of the annotation（2d 元素只用 yaw）。
    dqGeom::YawPitchRollAngles orientation;
    // The formatted document.
    TextBlock textBlock;
    // Describes how to compute the textBlock's anchor point.
    Anchor anchor;  // default {top, left}
    // Offset applied to the anchor point.
    dqGeom::Point3d offset;
    // The leaders of the text annotation（缺席 = 无——参考 `leaders?:`）。
    std::optional<std::vector<TextAnnotationLeader>> leaders;

    // Ported from: itwinjs-core TextAnnotation.create (:170-177)
    static TextAnnotation create(TextAnnotationCreateArgs const* args = nullptr);

    // Ported from: itwinjs-core TextAnnotation.fromJSON (:182-195) —
    // props 缺席字段经 create 缺省（DanQing Props 的 textBlock 为 Props 形）。
    static TextAnnotation fromJSON(TextAnnotationProps const* props);

    // Ported from: itwinjs-core TextAnnotation.toJSON (:200-227) —
    // 空块也写出 textBlock（style ID/overrides 续编辑语义）；offset/orientation/
    // anchor 仅在偏离缺省时写；leaders 恒写（参考恒 map——缺席→undefined，
    // 此处以空 optional 对齐）。
    TextAnnotationProps toJSON() const;

    // Transform positioning/orienting this annotation relative to its anchor
    // point（anchor 对齐 (0,0)，orientation 绕 anchor 旋转，offset 平移，
    // scaleFactor 缩放——:240-252）。
    // Ported from: itwinjs-core TextAnnotation.computeTransform (:240-252)
    dqGeom::Transform computeTransform(dqGeom::Range2d const& boundingBox,
                                       double scaleFactor = 1.0) const;

    // Ported from: itwinjs-core TextAnnotation.computeAnchorPoint (:258-281)
    dqGeom::Point3d computeAnchorPoint(dqGeom::Range2d const& boundingBox) const;

    // Logically equivalent to `other`.
    // Ported from: itwinjs-core TextAnnotation.equals (:307-317) +
    //               areLeadersEqual (:284-304)
    bool equals(TextAnnotation const& other) const;

private:
    explicit TextAnnotation(dqGeom::Point3d offset, dqGeom::YawPitchRollAngles angles,
                            TextBlock textBlock, Anchor anchor,
                            std::optional<std::vector<TextAnnotationLeader>> leaders);
};

END_DQ_COMMON_NAMESPACE

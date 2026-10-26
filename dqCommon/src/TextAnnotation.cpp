// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — TextAnnotation implementation
// Ported from: itwinjs-core core/common/src/annotation/TextAnnotation.ts
#include <dqCommon/TextAnnotation.h>

namespace dqCommon {

// Ported from: itwinjs-core TextAnnotation ctor (:161-167)
TextAnnotation::TextAnnotation(dqGeom::Point3d off, dqGeom::YawPitchRollAngles angles,
                               TextBlock tb, Anchor a,
                               std::optional<std::vector<TextAnnotationLeader>> l)
    : orientation(angles)
    , textBlock(std::move(tb))
    , anchor(a)
    , offset(off)
    , leaders(std::move(l))
{
}

// Ported from: itwinjs-core TextAnnotation.create (:170-177)
TextAnnotation TextAnnotation::create(TextAnnotationCreateArgs const* args)
{
    dqGeom::Point3d const offset = args && args->offset ? *args->offset : dqGeom::Point3d::FromZero();
    dqGeom::YawPitchRollAngles const angles = args && args->orientation ? *args->orientation
                                                                          : dqGeom::YawPitchRollAngles{};
    // clone 返回基类指针——经 toJSONBlockProps 深拷贝（TextBlockProps 往返，
    // 参考 create 语义：无参 = 空块）。
    TextBlock::Props const blockProps =
        args && args->textBlock ? args->textBlock->toJSONBlockProps() : TextBlock::Props{};
    TextBlock block = std::move(*TextBlock::create(&blockProps));
    Anchor const anchor = args ? args->anchor : Anchor{};  // default {top,left}
    std::optional<std::vector<TextAnnotationLeader>> leaders;
    if (args && !args->leaders.empty())
        leaders = args->leaders;
    return TextAnnotation(offset, angles, std::move(block), anchor, std::move(leaders));
}

// Ported from: itwinjs-core TextAnnotation.fromJSON (:182-195)
TextAnnotation TextAnnotation::fromJSON(TextAnnotationProps const* props)
{
    TextAnnotationCreateArgs args;
    if (props) {
        if (props->offset)
            args.offset = *props->offset;
        if (props->orientation)
            args.orientation = *props->orientation;
        if (props->textBlock)
            args.textBlock = std::move(*TextBlock::create(
                const_cast<TextBlock::Props*>(&*props->textBlock)));
        if (props->anchor)
            args.anchor = *props->anchor;
        args.leaders = props->leaders;
    }
    return create(&args);
}

// Ported from: itwinjs-core TextAnnotation.toJSON (:200-227)
TextAnnotationProps TextAnnotation::toJSON() const
{
    TextAnnotationProps props;

    // Even if the text block is empty, we want to record its style ID and
    // overrides (:203-204).
    props.textBlock = textBlock.toJSONBlockProps();

    // if (!this.offset.isZero) — Point3d.isZero = 精确零（geometry-core）。
    if (offset.x != 0.0 || offset.y != 0.0 || offset.z != 0.0)
        props.offset = offset;

    if (!orientation.isIdentity())
        props.orientation = orientation;

    if (!(anchor.vertical == AnchorVertical::Top && anchor.horizontal == AnchorHorizontal::Left))
        props.anchor = anchor;

    if (leaders)
        props.leaders = *leaders;

    return props;
}

// Ported from: itwinjs-core TextAnnotation.computeTransform (:240-252)
dqGeom::Transform TextAnnotation::computeTransform(dqGeom::Range2d const& boundingBox,
                                                   double scaleFactor) const
{
    dqGeom::Point3d const anchorPt = computeAnchorPoint(boundingBox);
    dqGeom::Matrix3d const matrix = orientation.ToMatrix3d();

    // transform = identity × translation × scaleTransform × rotation（参考经
    // multiplyTransformTransform 三连乘，DanQing 以 MultiplyTransform 链等价）。
    dqGeom::Transform const translation = dqGeom::Transform::CreateTranslation(
        offset.x - anchorPt.x, offset.y - anchorPt.y, offset.z - anchorPt.z);
    dqGeom::Transform const scaleTransform =
        dqGeom::Transform::CreateScaleAboutPoint(anchorPt, scaleFactor);
    dqGeom::Transform const rotation =
        dqGeom::Transform::CreateFixedPointAndMatrix(anchorPt, matrix);
    dqGeom::Transform transform = dqGeom::Transform::CreateIdentity();
    transform = transform.MultiplyTransform(translation);
    transform = transform.MultiplyTransform(scaleTransform);
    transform = transform.MultiplyTransform(rotation);
    return transform;
}

// Ported from: itwinjs-core TextAnnotation.computeAnchorPoint (:258-281)
dqGeom::Point3d TextAnnotation::computeAnchorPoint(dqGeom::Range2d const& boundingBox) const
{
    double x = boundingBox.low.x;
    double y = boundingBox.high.y;

    if (anchor.horizontal == AnchorHorizontal::Center)
        x += boundingBox.XLength() / 2.0;
    else if (anchor.horizontal == AnchorHorizontal::Right)
        x += boundingBox.XLength();

    if (anchor.vertical == AnchorVertical::Middle)
        y -= boundingBox.YLength() / 2.0;
    else if (anchor.vertical == AnchorVertical::Bottom)
        y -= boundingBox.YLength();

    return dqGeom::Point3d::From(x, y, 0.0);
}

// Ported from: itwinjs-core TextAnnotation.areLeadersEqual (:284-304)
static bool areLeadersEqual(std::optional<std::vector<TextAnnotationLeader>> const& a,
                            std::optional<std::vector<TextAnnotationLeader>> const& b)
{
    // leadersA === leadersB 同一性——C++ optional 同值即同形（无 undefined 恒等
    // 观测面）；均缺席 = 相等（参考 undefined===undefined true 臂）。
    bool const aAbsent = !a.has_value();
    bool const bAbsent = !b.has_value();
    if (aAbsent || bAbsent)
        return aAbsent && bAbsent;
    if (a->size() != b->size())
        return false;
    for (size_t i = 0; i < a->size(); ++i) {
        TextAnnotationLeader const& la = (*a)[i];
        TextAnnotationLeader const& lb = (*b)[i];
        if (!(la == lb))
            return false;
    }
    return true;
}

namespace {
// Ported from: itwinjs-core YawPitchRollAngles.isAlmostEqual (:148-152) —
// Angle.isAlmostEqualAllowPeriodShift 三轴（radians 形直用）。
bool orientationAlmostEqual(dqGeom::YawPitchRollAngles const& a,
                            dqGeom::YawPitchRollAngles const& b) noexcept
{
    using dqGeom::Angle;
    return Angle::isAlmostEqualRadiansAllowPeriodShift(a.yaw.Radians(), b.yaw.Radians())
        && Angle::isAlmostEqualRadiansAllowPeriodShift(a.pitch.Radians(), b.pitch.Radians())
        && Angle::isAlmostEqualRadiansAllowPeriodShift(a.roll.Radians(), b.roll.Radians());
}
}  // namespace

// Ported from: itwinjs-core TextAnnotation.equals (:307-317)
bool TextAnnotation::equals(TextAnnotation const& other) const
{
    if (!(anchor == other.anchor) || !orientationAlmostEqual(orientation, other.orientation)
        || !offset.AlmostEqual(other.offset, 1.0e-6)  // Point3d.isAlmostEqual 默认容差
        || !textBlock.equals(other.textBlock))
        return false;
    return areLeadersEqual(leaders, other.leaders);
}

}  // namespace dqCommon

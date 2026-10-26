// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — leader geometry implementation
// Ported from: itwinjs-core core/backend/src/annotations/LeaderGeometry.ts
#include <dqCommon/LeaderGeometry.h>

#include <dqCommon/GeometryStreamBuilder.h>

#include <dqGeom/Arc3d.h>
#include <dqGeom/LineSegment3d.h>

#include <cmath>
#include <limits>
#include <numbers>

namespace dqCommon {
namespace {

constexpr int kFrameSamplesPerCurve = 64;  // 采样 EQUIVALENCE（文件头登记）

// 帧曲线均匀采样点（Nearest/TextPoint 两臂共用）。
std::vector<dqGeom::Point3d> sampleFrame(FrameGeometryResult const& frameCurve)
{
    std::vector<dqGeom::Point3d> pts;
    for (auto const& curve : frameCurve.curves) {
        if (!curve)
            continue;
        for (int i = 0; i < kFrameSamplesPerCurve; ++i) {
            double const f = static_cast<double>(i) / static_cast<double>(kFrameSamplesPerCurve);
            pts.push_back(curve->FractionToPoint(f));
        }
    }
    return pts;
}

dqGeom::Point3d plusScaled(dqGeom::Point3d const& p, dqGeom::Vector3d const& v, double s)
{
    return Point3d::From(p.x + v.x * s, p.y + v.y * s, p.z + v.z * s);
}

}  // namespace

// Ported from: itwinjs-core createTerminatorGeometry (:164-285) —
// 单 switch 双枚举级联（isArrow ? terminatorShape : targetPointShape——参考
// 同形；None/circle 臂共享）。
bool createTerminatorGeometry(CreateTerminatorGeometryArgs& args)
{
    GeometryStreamBuilder& builder = *args.builder;
    dqGeom::Point3d const point = args.point;
    dqGeom::Vector3d dir = args.dir;
    bool const isArrow = args.isArrow;

    bool result = true;
    // termY = dir × unitZ（unitCrossProduct）；dir 平行 Z 时不可建（:170）。
    dqGeom::Vector3d const unitZ = dqGeom::Vector3d::From(0.0, 0.0, 1.0);
    dqGeom::Vector3d termY = dqGeom::Vector3d::FromNormalizedCrossProduct(dir, unitZ);
    if (termY.x == 0.0 && termY.y == 0.0 && termY.z == 0.0)
        return false;

    double const terminatorHeight =
        args.textStyleSettings.leaderTerminatorHeightFactor * args.textHeight;
    double const terminatorWidth =
        args.textStyleSettings.leaderTerminatorWidthFactor * args.textHeight;
    double const terminatorHalfHeight = terminatorHeight / 2.0;
    dqGeom::Point3d const basePoint = plusScaled(point, dir, terminatorWidth);
    dqGeom::Point3d const point1 = plusScaled(basePoint, termY, terminatorHalfHeight);
    dqGeom::Point3d const point2 = plusScaled(basePoint, termY, -terminatorHalfHeight);

    auto addFillParams = [&]() {
        // params.fillDisplay = Always——DanQing 面经 AreaFillProps（Blanking/Always
        // 值承载；BackgroundFill Solid 臂=blanking 先例同源）。
        AreaFillProps fill;
        fill.display = FillDisplay::Always;
        builder.appendGeometryParamsChange(args.params, &fill);
    };
    auto addGeometry = [&](dqGeom::GeometryQuery* content, bool useLoop = false) {
        if (useLoop)
            result = result && builder.appendGeometry(dqGeom::Loop::Create(
                          {dqBase::StaticCast<dqGeom::CurvePrimitive>(content->clone())}));
        else
            result = result && builder.appendGeometry(content->clone());
    };

    // shape = isArrow ? leader.terminatorShape : leader.targetPointShape（:189）。
    if (isArrow) {
        switch (args.textStyleSettings.leaderTerminatorShape) {
            case TerminatorShape::OpenArrow: {
                auto ls = dqGeom::LineString3d::create({point1, point, point2});
                addGeometry(ls.get());
                break;
            }
            case TerminatorShape::ClosedArrow: {
                auto ls = dqGeom::LineString3d::create({point1, point, point2, point1});
                addGeometry(ls.get());
                break;
            }
            case TerminatorShape::ClosedArrowFilled: {
                addFillParams();
                auto ls = dqGeom::LineString3d::create({point1, point, point2, point1});
                addGeometry(ls.get(), /*useLoop=*/true);
                break;
            }
            case TerminatorShape::Circle: {
                auto circle = dqGeom::Arc3d::CreateXY(point, terminatorHalfHeight);
                addGeometry(circle.get());
                break;
            }
            case TerminatorShape::CircleFilled: {
                addFillParams();
                auto circle = dqGeom::Arc3d::CreateXY(point, terminatorHalfHeight);
                addGeometry(circle.get(), /*useLoop=*/true);
                break;
            }
            case TerminatorShape::Slash: {
                dqGeom::Vector3d normalizedVector = dir;
                if (normalizedVector.Normalize() == 0.0)
                    return false;  // 参考 throw——C++ false 退（§3.4）
                double const radians = 45.0 * std::numbers::pi / 180.0;
                normalizedVector.RotateXY(radians);
                dqGeom::Point3d const startPoint =
                    plusScaled(point, normalizedVector, -terminatorHalfHeight);
                dqGeom::Point3d const endPoint =
                    plusScaled(point, normalizedVector, terminatorHalfHeight);
                auto slashLine = dqGeom::LineSegment3d::create(startPoint, endPoint);
                addGeometry(slashLine.get());
                break;
            }
            case TerminatorShape::None:
            default:
                break;
        }
        return result;
    }

    switch (args.textStyleSettings.leaderTargetPointShape) {
        case TargetPointShape::Cross: {  // 轴对齐 X（:237-243）
            double const halfSize = terminatorHalfHeight;
            auto d1 = dqGeom::LineSegment3d::create(
                Point3d::From(point.x - halfSize, point.y - halfSize, point.z),
                Point3d::From(point.x + halfSize, point.y + halfSize, point.z));
            addGeometry(d1.get());
            auto d2 = dqGeom::LineSegment3d::create(
                Point3d::From(point.x - halfSize, point.y + halfSize, point.z),
                Point3d::From(point.x + halfSize, point.y - halfSize, point.z));
            addGeometry(d2.get());
            break;
        }
        case TargetPointShape::Plus: {  // 轴对齐 +（:245-251）
            double const halfSize = terminatorHalfHeight;
            auto h = dqGeom::LineSegment3d::create(
                Point3d::From(point.x - halfSize, point.y, point.z),
                Point3d::From(point.x + halfSize, point.y, point.z));
            addGeometry(h.get());
            auto v = dqGeom::LineSegment3d::create(
                Point3d::From(point.x, point.y - halfSize, point.z),
                Point3d::From(point.x, point.y + halfSize, point.z));
            addGeometry(v.get());
            break;
        }
        case TargetPointShape::Circle: {  // circle 臂共享（:211-215）
            auto circle = dqGeom::Arc3d::CreateXY(point, terminatorHalfHeight);
            addGeometry(circle.get());
            break;
        }
        case TargetPointShape::Square: {  // 轴对齐方（:253-265）
            double const halfSize = terminatorHalfHeight;
            auto sq = dqGeom::LineString3d::create({
                Point3d::From(point.x - halfSize, point.y - halfSize, point.z),
                Point3d::From(point.x + halfSize, point.y - halfSize, point.z),
                Point3d::From(point.x + halfSize, point.y + halfSize, point.z),
                Point3d::From(point.x - halfSize, point.y + halfSize, point.z),
                Point3d::From(point.x - halfSize, point.y - halfSize, point.z),
            });
            addGeometry(sq.get());
            break;
        }
        case TargetPointShape::Rectangle: {  // 宽高因子独立（:267-281）
            double const halfWidth = terminatorWidth / 2.0;
            double const halfHeight = terminatorHalfHeight;
            auto rect = dqGeom::LineString3d::create({
                Point3d::From(point.x - halfWidth, point.y - halfHeight, point.z),
                Point3d::From(point.x + halfWidth, point.y - halfHeight, point.z),
                Point3d::From(point.x + halfWidth, point.y + halfHeight, point.z),
                Point3d::From(point.x - halfWidth, point.y + halfHeight, point.z),
                Point3d::From(point.x - halfWidth, point.y - halfHeight, point.z),
            });
            addGeometry(rect.get());
            break;
        }
    }
    return result;
}

// Ported from: itwinjs-core computeElbowDirection (:299-327) —
// 左/右近侧判定 + 相切抑制（切向经采样邻点 EQUIVALENCE——CurveCurve 解析交点
// TODO，文件头登记）。
std::optional<dqGeom::Vector3d> computeElbowDirection(
    dqGeom::Point3d const& attachmentPoint, FrameGeometryResult const& frameCurve,
    double elbowLength)
{
    // isCloserToLeft（:303）——帧 range 未经 CurveChain range()：采样点包络。
    std::vector<dqGeom::Point3d> const samples = sampleFrame(frameCurve);
    if (samples.empty())
        return std::nullopt;
    double lowX = samples.front().x, highX = samples.front().x;
    for (auto const& p : samples) {
        lowX = std::min(lowX, p.x);
        highX = std::max(highX, p.x);
    }
    bool const isCloserToLeft = std::fabs(attachmentPoint.x - lowX)
                              < std::fabs(attachmentPoint.x - highX);
    dqGeom::Vector3d elbowDirection = dqGeom::Vector3d::UnitX();
    if (isCloserToLeft)
        elbowDirection.Negate();

    // 相切抑制：肘点若落在帧采样点容差内（肘触帧）且局部切向与肘向对齐
    //（|dot|>0.999）→ 无肘。采样近邻方向替代解析导数（EQUIVALENCE）。
    dqGeom::Point3d const elbowPoint = plusScaled(attachmentPoint, elbowDirection, elbowLength);
    double const tol = elbowLength > 0.0 ? elbowLength * 1.0e-3 : 1.0e-6;
    for (size_t i = 0; i < samples.size(); ++i) {
        if (samples[i].Distance(elbowPoint) <= tol) {
            dqGeom::Point3d const& prev = samples[(i + samples.size() - 1) % samples.size()];
            dqGeom::Point3d const& next = samples[(i + 1) % samples.size()];
            dqGeom::Vector3d tangent = dqGeom::Vector3d::From(next.x - prev.x, next.y - prev.y,
                                                              next.z - prev.z);
            if (tangent.Normalize() != 0.0) {
                dqGeom::Vector3d lineDirection = dqGeom::Vector3d::From(
                    elbowPoint.x - attachmentPoint.x, elbowPoint.y - attachmentPoint.y,
                    elbowPoint.z - attachmentPoint.z);
                if (lineDirection.Normalize() != 0.0
                    && std::fabs(tangent.DotProduct(lineDirection)) > 0.999)
                    return std::nullopt;
            }
            break;
        }
    }
    return elbowDirection;
}

// Ported from: itwinjs-core computeLeaderAttachmentPoint (:344-390) —
// 三模式（Nearest 采样最近 / KeyPoint curves 索引 / TextPoint 投影线最近前向
// 采样点——EQUIVALENCE 文件头登记）。
std::optional<dqGeom::Point3d> computeLeaderAttachmentPoint(
    TextAnnotationLeader const& leader, FrameGeometryResult const& frameCurve,
    TextBlockLayout const& textLayout, dqGeom::Transform const& transform)
{
    if (leader.attachment.mode == LeaderAttachment::Mode::Nearest) {
        dqGeom::Point3d const start = leader.startPoint;
        std::vector<dqGeom::Point3d> const samples = sampleFrame(frameCurve);
        dqGeom::Point3d best;
        double bestDist = std::numeric_limits<double>::max();
        for (auto const& p : samples) {
            double const d = p.Distance(start);
            if (d < bestDist) {
                bestDist = d;
                best = p;
            }
        }
        return best;
    }
    if (leader.attachment.mode == LeaderAttachment::Mode::KeyPoint) {
        auto const& curves = frameCurve.curves;
        if (curves.empty())
            return std::nullopt;
        uint32_t const curveIndex = leader.attachment.curveIndex;
        double const fraction = leader.attachment.fraction;
        // curveIndex 越界 → 末曲线回退（:358-361）。
        auto const& curve = curveIndex < curves.size() ? curves[curveIndex] : curves.back();
        return curve ? curve->FractionToPoint(fraction) : std::optional<dqGeom::Point3d>{};
    }
    // TextPoint（:365-388）。
    dqGeom::Vector3d scaleDirection = transform.matrix.ColumnX();
    scaleDirection.Negate();
    uint32_t lineIndex = 0;
    // position.includes("Top")——枚举序 TopLeft/TopRight 前缀 Top。
    bool const isTop = leader.attachment.position == LeaderAttachment::TextPoint::TopLeft
        || leader.attachment.position == LeaderAttachment::TextPoint::TopRight;
    bool const isRight = leader.attachment.position == LeaderAttachment::TextPoint::TopRight
        || leader.attachment.position == LeaderAttachment::TextPoint::BottomRight;
    if (!isTop && !textLayout.lines.empty())
        lineIndex = static_cast<uint32_t>(textLayout.lines.size() - 1);
    if (lineIndex >= textLayout.lines.size())
        return std::nullopt;
    auto const& line = textLayout.lines[lineIndex];
    double const lineRangeY = line.range.YLength();
    double const lineRangeX = line.range.XLength();
    dqGeom::Point3d const origin = transform.MultiplyPoint3d(
        dqGeom::Point3d::From(line.offsetFromDocument.x, line.offsetFromDocument.y, 0.0));
    dqGeom::Point3d attachmentPointOnText = plusScaled(
        origin, transform.matrix.ColumnY(), lineRangeY / 2.0);
    if (isRight) {
        attachmentPointOnText = plusScaled(attachmentPointOnText, transform.matrix.ColumnX(),
                                           lineRangeX);
        scaleDirection.Negate();
    }
    // 投影线远端目标 + 帧采样最近前向点（解析线-弧交点 TODO）。
    dqGeom::Point3d const targetPoint = plusScaled(attachmentPointOnText, scaleDirection, 1.0e6);
    dqGeom::Vector3d ray = dqGeom::Vector3d::From(targetPoint.x - attachmentPointOnText.x,
                                                  targetPoint.y - attachmentPointOnText.y,
                                                  targetPoint.z - attachmentPointOnText.z);
    if (ray.Normalize() == 0.0)
        return std::nullopt;
    std::vector<dqGeom::Point3d> const samples = sampleFrame(frameCurve);
    std::optional<dqGeom::Point3d> best;
    double bestT = std::numeric_limits<double>::max();
    double const rayLen = targetPoint.Distance(attachmentPointOnText);
    for (auto const& p : samples) {
        dqGeom::Vector3d rel = dqGeom::Vector3d::From(p.x - attachmentPointOnText.x,
                                                      p.y - attachmentPointOnText.y,
                                                      p.z - attachmentPointOnText.z);
        double const t = rel.DotProduct(ray);
        if (t < 0.0 || t > bestT)
            continue;
        // 距射线容差（采样步长同量级）。
        double const perp = std::sqrt(rel.DotProduct(rel) - t * t);
        if (perp <= 1.0e-2 * rayLen + 1.0e-6) {
            bestT = t;
            best = p;
        }
    }
    (void)rayLen;
    return best;
}

// Ported from: itwinjs-core appendLeadersToBuilder (:32-135)
bool appendLeadersToBuilder(GeometryStreamBuilder& builder,
                            std::vector<TextAnnotationLeader> const& leaders,
                            TextBlockLayout const& layout, dqGeom::Transform const& transform,
                            GeometryAppearanceProps params, TextStyleResolver const& textStyleResolver,
                            double scaleFactor)
{
    bool result = true;
    double const scaledBlockTextHeight = textStyleResolver.blockSettings.textHeight * scaleFactor;
    TextFrameStyleProps const frame = textStyleResolver.blockSettings.toJSON().frame.value();

    // 无帧 → 矩形帧承载附件点计算（:38-40）。
    if (!frame.shape.has_value() || *frame.shape == TextAnnotationFrameShape::None)
        frame.shape = TextAnnotationFrameShape::Rectangle;
    if (!frame.shape.has_value() || *frame.shape == TextAnnotationFrameShape::None)
        return false;

    ComputeFrameArgs cfa;
    cfa.frame = *frame.shape;
    cfa.range = layout.range;
    cfa.transform = transform;
    FrameGeometryResult const frameCurve = computeFrame(cfa);

    for (auto const& leader : leaders) {
        static TextStyleSettingsProps const kEmpty;
        TextStyleSettingsProps const& overrides =
            leader.styleOverrides ? *leader.styleOverrides : kEmpty;
        TextStyleSettings const leaderStyle = textStyleResolver.resolveSettings(overrides, true);

        // effectiveColor（:47-53）：inherit → 样式色；subcategory → 子类色；rgb → 其色。
        bool effectiveIsSub = false;
        ColorDefProps effectiveRgb = 0;
        if (leaderStyle.leaderColor.source == TextLeaderStyleProps::Color::Source::Inherit) {
            if (leaderStyle.color.subcategory)
                effectiveIsSub = true;
            else
                effectiveRgb = leaderStyle.color.color;
        } else if (leaderStyle.leaderColor.source
                   == TextLeaderStyleProps::Color::Source::Subcategory) {
            effectiveIsSub = true;
        } else {
            effectiveRgb = leaderStyle.leaderColor.rgb;
        }

        if (!effectiveIsSub) {
            params.color = effectiveRgb;  // 参考 params.lineColor 可变——按值形参承载
            builder.appendGeometryParamsChange(params);
        }

        if (leaderStyle.leaderShowLeaders) {
            auto const attachmentPoint = computeLeaderAttachmentPoint(leader, frameCurve,
                                                                      layout, transform);
            if (!attachmentPoint)
                return false;

            std::optional<dqGeom::Point3d> elbowPoint;
            if (leaderStyle.leaderWantElbow) {
                double const elbowLength = leaderStyle.leaderElbowLength * scaledBlockTextHeight;
                auto const elbowDirection = computeElbowDirection(*attachmentPoint, frameCurve,
                                                                  elbowLength);
                if (elbowDirection)
                    elbowPoint = plusScaled(*attachmentPoint, *elbowDirection, elbowLength);
            }

            double const terminatorWidth =
                leaderStyle.leaderTerminatorWidthFactor * scaledBlockTextHeight;
            dqGeom::Point3d leaderStartPoint = leader.startPoint;
            if (leaderStyle.leaderShowTargetPoint
                && leaderStyle.leaderTargetPointOffsetFactor != 0.0) {
                double const offset =
                    leaderStyle.leaderTargetPointOffsetFactor * scaledBlockTextHeight;
                dqGeom::Point3d firstSegmentEnd = !leader.intermediatePoints.empty()
                    ? leader.intermediatePoints.front()
                    : (elbowPoint ? *elbowPoint : *attachmentPoint);
                double const firstSegmentLength = leader.startPoint.Distance(firstSegmentEnd);
                double const clampDistance = leaderStyle.leaderShowTerminators
                    ? terminatorWidth
                    : scaledBlockTextHeight;
                double const maxOffset = std::max(0.0, firstSegmentLength - clampDistance);
                double const clampedOffset = std::min(std::max(offset, 0.0), maxOffset);
                dqGeom::Vector3d leaderDirection = dqGeom::Vector3d::From(
                    firstSegmentEnd.x - leader.startPoint.x,
                    firstSegmentEnd.y - leader.startPoint.y,
                    firstSegmentEnd.z - leader.startPoint.z);
                if (leaderDirection.Normalize() != 0.0)
                    leaderStartPoint = plusScaled(leader.startPoint, leaderDirection, clampedOffset);
            }

            std::vector<dqGeom::Point3d> leaderLinePoints;
            leaderLinePoints.push_back(leaderStartPoint);
            for (auto const& p : leader.intermediatePoints)
                leaderLinePoints.push_back(p);
            if (elbowPoint)
                leaderLinePoints.push_back(*elbowPoint);
            leaderLinePoints.push_back(*attachmentPoint);

            if (leaderLinePoints.size() < 2)
                continue;

            dqGeom::Vector3d terminatorDirection = dqGeom::Vector3d::From(
                leaderLinePoints[1].x - leaderLinePoints[0].x,
                leaderLinePoints[1].y - leaderLinePoints[0].y,
                leaderLinePoints[1].z - leaderLinePoints[0].z);
            bool const hasTerminatorDir = terminatorDirection.Normalize() != 0.0;

            // closedArrow 首段截断（:115-118）。
            if (leaderStyle.leaderShowTerminators
                && leaderStyle.leaderTerminatorShape == TerminatorShape::ClosedArrow
                && hasTerminatorDir)
                leaderLinePoints[0] = plusScaled(leaderLinePoints[0], terminatorDirection,
                                                 terminatorWidth);

            builder.appendGeometry(dqGeom::LineString3d::create(leaderLinePoints)->clone());
            if (leaderStyle.leaderTerminatorShape != TerminatorShape::None
                && leaderStyle.leaderShowTerminators) {
                if (!hasTerminatorDir)
                    continue;  // 无向 leader 合法跳过（:123）
                CreateTerminatorGeometryArgs targs;
                targs.builder = &builder;
                targs.point = leaderStartPoint;
                targs.dir = terminatorDirection;
                targs.params = params;
                targs.textStyleSettings = leaderStyle;
                targs.textHeight = scaledBlockTextHeight;
                targs.isArrow = true;
                result = result && createTerminatorGeometry(targs);
            }
        }

        if (leaderStyle.leaderShowTargetPoint) {
            CreateTerminatorGeometryArgs targs;
            targs.builder = &builder;
            targs.point = leader.startPoint;
            targs.dir = dqGeom::Vector3d::UnitX();
            targs.params = params;
            targs.textStyleSettings = leaderStyle;
            targs.textHeight = scaledBlockTextHeight;
            targs.isArrow = false;
            result = result && createTerminatorGeometry(targs);
        }
    }
    return result;
}

}  // namespace dqCommon

// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — frame geometry implementation
// Ported from: itwinjs-core core/backend/src/annotations/FrameGeometry.ts
#include <dqCommon/FrameGeometry.h>

#include <dqGeom/Angle.h>
#include <dqGeom/AngleSweep.h>

#include <cmath>
#include <numbers>

namespace dqCommon {

namespace {
using dqGeom::Arc3d;
using dqGeom::CurvePrimitivePtr;
using dqGeom::LineString3d;
using dqGeom::Loop;
using dqGeom::Path;
using dqGeom::Point3d;

// Line — currently just adds an underline（:127-132）。
FrameGeometryResult computeLine(dqGeom::Range2d const& range, dqGeom::Transform const& transform)
{
    std::vector<Point3d> const points = {
        Point3d::From(range.low.x, range.low.y, 0.0),
        Point3d::From(range.high.x, range.low.y, 0.0),
    };
    auto frame = LineString3d::create(points);
    FrameGeometryResult out;
    out.isLoop = false;
    auto cp = dqBase::StaticCast<dqGeom::CurvePrimitive>(frame->CloneTransformed(transform));
    out.curves.push_back(cp);
    out.geometry = Path::Create({cp});
    return out;
}

// Rectangle — simplest frame（:135-140）。corners3d(true) = 闭合点序（首点重复）。
FrameGeometryResult computeRectangle(dqGeom::Range2d range, dqGeom::Transform const& transform)
{
    std::vector<Point3d> points = {
        Point3d::From(range.low.x, range.low.y, 0.0),
        Point3d::From(range.high.x, range.low.y, 0.0),
        Point3d::From(range.high.x, range.high.y, 0.0),
        Point3d::From(range.low.x, range.high.y, 0.0),
        Point3d::From(range.low.x, range.low.y, 0.0),
    };
    auto frame = LineString3d::create(points);
    FrameGeometryResult out;
    out.isLoop = true;
    auto cp = dqBase::StaticCast<dqGeom::CurvePrimitive>(frame->CloneTransformed(transform));
    out.curves.push_back(cp);
    out.geometry = Loop::Create({cp});
    return out;
}

// Rounded Rectangle（:143-178）——圆角半径 = yLength * radiusFactor * √2。
FrameGeometryResult computeRoundedRectangle(dqGeom::Range2d const& range,
                                            dqGeom::Transform const& transform,
                                            double radiusFactor = 0.25)
{
    double const radius = range.YLength() * radiusFactor * std::sqrt(2.0);
    double const radiusOffsetFactor = range.YLength() * radiusFactor;

    double const inLeft = range.low.x + radiusOffsetFactor;
    double const inRight = range.high.x - radiusOffsetFactor;
    double const inBottom = range.low.y + radiusOffsetFactor;
    double const inTop = range.high.y - radiusOffsetFactor;

    double const exLeft = inLeft - radius;
    double const exRight = inRight + radius;
    double const exBottom = inBottom - radius;
    double const exTop = inTop + radius;

    dqGeom::AngleSweep const q1 = dqGeom::AngleSweep::FromStartEndDegrees(0.0, 90.0);
    dqGeom::AngleSweep const q2 = dqGeom::AngleSweep::FromStartEndDegrees(90.0, 180.0);
    dqGeom::AngleSweep const q3 = dqGeom::AngleSweep::FromStartEndDegrees(180.0, 270.0);
    dqGeom::AngleSweep const q4 = dqGeom::AngleSweep::FromStartEndDegrees(270.0, 360.0);

    std::vector<CurvePrimitivePtr> curves;
    auto addLine = [&](Point3d const& a, Point3d const& b) {
        auto ls = LineString3d::create({a, b});
        curves.push_back(dqBase::StaticCast<dqGeom::CurvePrimitive>(ls->CloneTransformed(transform)));
    };
    auto addArc = [&](Point3d const& center, dqGeom::AngleSweep const& sweep) {
        auto arc = Arc3d::CreateXY(center, radius, sweep);
        curves.push_back(dqBase::StaticCast<dqGeom::CurvePrimitive>(arc->CloneTransformed(transform)));
    };
    // （curves 即 FrameGeometryResult.curves 载体——addLine/addArc 已收集。）
    addLine(Point3d::From(inRight, exTop, 0.0), Point3d::From(inLeft, exTop, 0.0));       // top
    addArc(Point3d::From(inLeft, inTop, 0.0), q2);                                        // top left
    addLine(Point3d::From(exLeft, inTop, 0.0), Point3d::From(exLeft, inBottom, 0.0));     // left
    addArc(Point3d::From(inLeft, inBottom, 0.0), q3);                                     // bottom left
    addLine(Point3d::From(inLeft, exBottom, 0.0), Point3d::From(inRight, exBottom, 0.0)); // bottom
    addArc(Point3d::From(inRight, inBottom, 0.0), q4);                                    // bottom right
    addLine(Point3d::From(exRight, inBottom, 0.0), Point3d::From(exRight, inTop, 0.0));   // right
    addArc(Point3d::From(inRight, inTop, 0.0), q1);                                       // top right

    FrameGeometryResult out;
    out.isLoop = true;
    out.curves = curves;
    out.geometry = Loop::CreateArray(curves);
    return out;
}

// Circle（:182-186）。
FrameGeometryResult computeCircle(dqGeom::Range2d const& range, dqGeom::Transform const& transform)
{
    double const radius = range.low.Distance(range.high) / 2.0;
    auto frame = Arc3d::CreateXY(range.Center(), radius);
    FrameGeometryResult out;
    out.isLoop = true;
    auto cp = dqBase::StaticCast<dqGeom::CurvePrimitive>(frame->CloneTransformed(transform));
    out.curves.push_back(cp);
    out.geometry = Loop::Create({cp});
    return out;
}

// Equilateral Triangle（:189-213）。
FrameGeometryResult computeTriangle(dqGeom::Range2d const& range, dqGeom::Transform const& transform)
{
    double const xLength = range.XLength();
    double const yLength = range.YLength();
    dqGeom::Point3d const center = range.Center();  // 既有 Range2d.Center() 返回 Point3d（z=0）

    double const magnitude = (xLength > yLength)
        ? (xLength * std::sqrt(3.0) + yLength) / 2.0
        : (yLength * std::sqrt(3.0) + xLength) / 2.0;

    // v1 = (0, magnitude)；rotateXY 120°/240°（Vector2d.rotateXY 的 cos/sin 内联）。
    auto rotated = [](double vx, double vy, double thetaRadians) {
        return std::pair<double, double>{vx * std::cos(thetaRadians) - vy * std::sin(thetaRadians),
                                         vx * std::sin(thetaRadians) + vy * std::cos(thetaRadians)};
    };
    double const d120 = 120.0 * std::numbers::pi / 180.0;
    double const d240 = 240.0 * std::numbers::pi / 180.0;
    auto const left = rotated(0.0, magnitude, d120);
    auto const right = rotated(0.0, magnitude, d240);

    std::vector<Point3d> const points = {
        Point3d::From(center.x + 0.0, center.y + magnitude, 0.0),
        Point3d::From(center.x + left.first, center.y + left.second, 0.0),
        Point3d::From(center.x + right.first, center.y + right.second, 0.0),
        Point3d::From(center.x + 0.0, center.y + magnitude, 0.0),
    };
    auto frame = LineString3d::create(points);
    FrameGeometryResult out;
    out.isLoop = true;
    auto cp = dqBase::StaticCast<dqGeom::CurvePrimitive>(frame->CloneTransformed(transform));
    out.curves.push_back(cp);
    out.geometry = Loop::Create({cp});
    return out;
}

// Diamond — square rotated 45°（:216-231）。
FrameGeometryResult computeDiamond(dqGeom::Range2d const& range, dqGeom::Transform const& transform)
{
    double const offset = (range.XLength() + range.YLength()) / 2.0;
    dqGeom::Point3d const center = range.Center();  // 既有 Range2d.Center() 返回 Point3d（z=0）

    std::vector<Point3d> const points = {
        Point3d::From(center.x, center.y + offset, 0.0), // top
        Point3d::From(center.x + offset, center.y, 0.0), // right
        Point3d::From(center.x, center.y - offset, 0.0), // bottom
        Point3d::From(center.x - offset, center.y, 0.0), // left
        Point3d::From(center.x, center.y + offset, 0.0), // top
    };
    auto frame = LineString3d::create(points);
    FrameGeometryResult out;
    out.isLoop = true;
    auto cp = dqBase::StaticCast<dqGeom::CurvePrimitive>(frame->CloneTransformed(transform));
    out.curves.push_back(cp);
    out.geometry = Loop::Create({cp});
    return out;
}

// Square（:234-252）——range 沿短轴向扩成正方（参考就地 extendPoint 后取角）。
FrameGeometryResult computeSquare(dqGeom::Range2d range, dqGeom::Transform const& transform)
{
    double const xLength = range.XLength() / 2.0;
    double const yLength = range.YLength() / 2.0;
    dqGeom::Point3d const center = range.Center();  // 既有 Range2d.Center() 返回 Point3d（z=0）
    if (xLength > yLength) {
        range.ExtendXY(center.x, center.y + xLength);
        range.ExtendXY(center.x, center.y - xLength);
    } else {
        range.ExtendXY(center.x + yLength, center.y);
        range.ExtendXY(center.x - yLength, center.y);
    }

    std::vector<Point3d> const points = {
        Point3d::From(range.low.x, range.low.y, 0.0),
        Point3d::From(range.high.x, range.low.y, 0.0),
        Point3d::From(range.high.x, range.high.y, 0.0),
        Point3d::From(range.low.x, range.high.y, 0.0),
        Point3d::From(range.low.x, range.low.y, 0.0),
    };
    auto frame = LineString3d::create(points);
    FrameGeometryResult out;
    out.isLoop = true;
    auto cp = dqBase::StaticCast<dqGeom::CurvePrimitive>(frame->CloneTransformed(transform));
    out.curves.push_back(cp);
    out.geometry = Loop::Create({cp});
    return out;
}

// Capsule (pill)（:255-283）。
FrameGeometryResult computeCapsule(dqGeom::Range2d const& range, dqGeom::Transform const& transform)
{
    double const height = range.YLength();
    double const radius = height * (std::sqrt(2.0) / 2.0);
    double const radiusOffsetFactor = height / 2.0;

    double const inLeft = range.low.x + radiusOffsetFactor;
    double const inRight = range.high.x - radiusOffsetFactor;
    double const inBottom = range.low.y + radiusOffsetFactor;
    double const inTop = range.high.y - radiusOffsetFactor;

    double const exBottom = inBottom - radius;
    double const exTop = inTop + radius;

    dqGeom::AngleSweep const leftHalfCircle = dqGeom::AngleSweep::FromStartEndDegrees(90.0, 270.0);
    dqGeom::AngleSweep const rightHalfCircle = dqGeom::AngleSweep::FromStartEndDegrees(-90.0, 90.0);
    double const centerY = range.Center().y;

    std::vector<CurvePrimitivePtr> curves;
    auto addLine = [&](Point3d const& a, Point3d const& b) {
        auto ls = LineString3d::create({a, b});
        curves.push_back(dqBase::StaticCast<dqGeom::CurvePrimitive>(ls->CloneTransformed(transform)));
    };
    addLine(Point3d::From(inRight, exTop, 0.0), Point3d::From(inLeft, exTop, 0.0));       // top
    {
        auto arc = Arc3d::CreateXY(Point3d::From(inLeft, centerY, 0.0), radius, leftHalfCircle);
        curves.push_back(dqBase::StaticCast<dqGeom::CurvePrimitive>(arc->CloneTransformed(transform)));
    }
    addLine(Point3d::From(inLeft, exBottom, 0.0), Point3d::From(inRight, exBottom, 0.0)); // bottom
    {
        auto arc = Arc3d::CreateXY(Point3d::From(inRight, centerY, 0.0), radius, rightHalfCircle);
        curves.push_back(dqBase::StaticCast<dqGeom::CurvePrimitive>(arc->CloneTransformed(transform)));
    }

    FrameGeometryResult out;
    out.isLoop = true;
    out.curves = curves;
    out.geometry = Loop::CreateArray(curves);
    return out;
}

// Regular polygon with n sides（:286-315）。
FrameGeometryResult computePolygon(int n, dqGeom::Range2d const& range,
                                   dqGeom::Transform const& transform, double angleOffsetDegrees = 0.0)
{
    dqGeom::Point3d const center = range.Center();  // 既有 Range2d.Center() 返回 Point3d（z=0）
    double const inradius = range.low.Distance(range.high) / 2.0;
    double const circumradius = inradius / std::cos(std::numbers::pi / static_cast<double>(n));
    double const angleIncrement = 360.0 / static_cast<double>(n);

    std::vector<Point3d> vertices;
    for (int i = 0; i < n; ++i) {
        double const angleDegrees = static_cast<double>(i) * angleIncrement + angleOffsetDegrees;
        double const radians = angleDegrees * std::numbers::pi / 180.0;
        vertices.push_back(Point3d::From(center.x + circumradius * std::cos(radians),
                                         center.y + circumradius * std::sin(radians), 0.0));
    }
    vertices.push_back(vertices.front());  // Close the polygon.

    auto frame = LineString3d::create(vertices);
    FrameGeometryResult out;
    out.isLoop = true;
    out.geometry = Loop::Create({dqBase::StaticCast<dqGeom::CurvePrimitive>(
        frame->CloneTransformed(transform))});
    return out;
}
}  // namespace

// Ported from: itwinjs-core computeFrame (:79-94)
FrameGeometryResult computeFrame(ComputeFrameArgs const& args)
{
    switch (args.frame) {
        case TextAnnotationFrameShape::Line:
            return computeLine(args.range, args.transform);
        case TextAnnotationFrameShape::Rectangle:
            return computeRectangle(args.range, args.transform);
        case TextAnnotationFrameShape::Circle:
            return computeCircle(args.range, args.transform);
        case TextAnnotationFrameShape::EquilateralTriangle:
            return computeTriangle(args.range, args.transform);
        case TextAnnotationFrameShape::Diamond:
            return computeDiamond(args.range, args.transform);
        case TextAnnotationFrameShape::Square:
            return computeSquare(args.range, args.transform);
        case TextAnnotationFrameShape::Pentagon:
            return computePolygon(5, args.range, args.transform, 90.0);
        case TextAnnotationFrameShape::Hexagon:
            return computePolygon(6, args.range, args.transform);
        case TextAnnotationFrameShape::Octagon:
            return computePolygon(8, args.range, args.transform, 180.0 / 8.0);  // pi/8 rad
        case TextAnnotationFrameShape::Capsule:
            return computeCapsule(args.range, args.transform);
        case TextAnnotationFrameShape::RoundedRectangle:
            return computeRoundedRectangle(args.range, args.transform);
        case TextAnnotationFrameShape::None:
        default:
            return computeRectangle(args.range, args.transform);
    }
}

// Ported from: itwinjs-core computeIntervalPoints (:113-124)
std::optional<std::vector<dqGeom::Point3d>> computeIntervalPoints(
    ComputeFrameArgs const& args, std::optional<double> lineIntervalFactor,
    std::optional<double> arcIntervalFactor)
{
    double const lineFactor = lineIntervalFactor.value_or(0.5);
    double const arcFactor = arcIntervalFactor.value_or(0.25);
    (void)lineFactor;   // TODO 缝消費（CurveChain children——函数体 TODO 注）
    (void)arcFactor;

    std::vector<dqGeom::Point3d> points;
    FrameGeometryResult frame = computeFrame(args);
    if (!frame.geometry)
        return std::nullopt;
    // collectCurvePrimitives 面：Loop/Path 均为 CurveChain——曲线族子原语。此处
    // 帧几何仅一 CurveChain；链子原语遍历（Arc→arcFactor 步进，线→lineFactor）。
    // DanQing 的 CurveChain 子原语访问经 CloneStroked 路径为 Phase-N——按帧形
    // 已知子原语结构直接展开（computeFrame 各臂的曲线清单是封闭集）。
    (void)frame;
    // TODO: 子原语遍历随 CurveChain 公开 children 缝落地复核（参考
    // collectCurvePrimitives 泛型遍历）——LeaderAttachment KeyPoint 消费面
    // （Y-c3 LeaderGeometry）启用时补。
    return points;
}

// Ported from: itwinjs-core appendFrameToBuilder (:22-59)
bool appendFrameToBuilder(GeometryStreamBuilder& builder, TextFrameStyleProps const& frame,
                          dqGeom::Range2d const& range, dqGeom::Transform const& transform,
                          GeometryAppearanceProps const& geomParams)
{
    if (frame.shape == TextAnnotationFrameShape::None || !frame.shape.has_value())
        return false;

    GeometryAppearanceProps params = geomParams;

    // Fill arms（TextAnnotationFillColor 四态——TextStyle.ts:79）。
    AreaFillProps fill;
    bool blankingFill = false;
    if (!frame.fillColor.has_value() || frame.fillColor->source == TextAnnotationFillColor::Source::None) {
        fill.display = FillDisplay::Never;
    } else if (frame.fillColor->source == TextAnnotationFillColor::Source::Background) {
        // params.backgroundFill = Solid——DanQing AreaFillProps 无 backgroundFill
        // 面，Blanking fill 承载（视觉等价：框内底色）。
        fill.display = FillDisplay::Blanking;
        blankingFill = true;
    } else if (frame.fillColor->source != TextAnnotationFillColor::Source::Subcategory) {
        params.color = frame.fillColor->rgb;     // params.fillColor
        fill.display = FillDisplay::Blanking;
        blankingFill = true;
    }

    if (frame.borderColor.has_value() && !frame.borderColor->subcategory)
        params.color = frame.borderColor->color;
    if (frame.borderWeight.has_value())
        params.weight = static_cast<int>(*frame.borderWeight);

    ComputeFrameArgs cfa;
    cfa.frame = *frame.shape;
    cfa.range = range;
    cfa.transform = transform;
    FrameGeometryResult const frameGeometry = computeFrame(cfa);
    if (!frameGeometry.geometry)
        return false;

    // builder.appendGeometryParamsChange + appendGeometryQuery。
    builder.appendGeometryParamsChange(params, blankingFill ? &fill : nullptr);
    builder.appendGeometry(frameGeometry.geometry);

    // The tile generator does not produce an outline for shapes with blanking
    // fill — add the outline separately（:50-56：Loop→其子曲线 Path）。
    if (blankingFill && frameGeometry.isLoop) {
        // 参考 Path.createArray(frameGeometry.children)——Loop 子原语的 Path
        // 形。DanQing 子原语遍历同 computeIntervalPoints TODO 缝——以 CloneStroked
        // 前置延后登记。
        // TODO: Loop→Path 拆解随 CurveChain children 缝落地。
    }
    return true;
}

}  // namespace dqCommon

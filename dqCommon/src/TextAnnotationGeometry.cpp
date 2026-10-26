// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — text annotation geometry assembly implementation
// Ported from: itwinjs-core core/backend/src/annotations/TextAnnotationGeometry.ts
#include <dqCommon/TextAnnotationGeometry.h>

#include <dqCommon/GeometryStreamBuilder.h>
#include <dqCommon/TextBlockGeometryProps.h>

#include <dqGeom/LineSegment3d.h>
#include <dqGeom/LineString3d.h>

namespace dqCommon {
namespace {

dqGeom::Point3d plusXYZ(dqGeom::Point3d const& p, double dx, double dy)
{
    return dqGeom::Point3d::From(p.x + dx, p.y + dy, p.z);
}

// Ported from: debugAnchorPoint (:122-150) — 蓝框 margins + 蓝十字 anchor +
// 红框 textRange。
bool debugAnchorPoint(GeometryStreamBuilder& builder, TextAnnotation const& annotation,
                      TextBlockLayout const& layout, dqGeom::Transform const& transform)
{
    dqGeom::Range2d const range = layout.range;
    dqGeom::Point3d const debugAnchorPt =
        transform.MultiplyPoint3d(annotation.computeAnchorPoint(range));

    GeometryAppearanceProps blueParams;
    blueParams.color = ColorDef::blue.getTbgr();
    builder.appendGeometryParamsChange(blueParams);  // void 面
    bool result = true;

    // margins 蓝框（corners3d(true) 闭合序）。
    std::vector<dqGeom::Point3d> marginCorners = {
        plusXYZ(range.low, 0.0, 0.0),
        plusXYZ(range.high, 0.0, 0.0),
        dqGeom::Point3d::From(range.high.x, range.high.y, 0.0),
        dqGeom::Point3d::From(range.low.x, range.high.y, 0.0),
        plusXYZ(range.low, 0.0, 0.0),
    };
    for (auto& c : marginCorners)
        c = transform.MultiplyPoint3d(c);
    builder.appendGeometry(dqGeom::LineString3d::create(marginCorners)->clone());  // void 面

    // anchor 蓝十字（1m × 1m）。
    auto x1 = dqGeom::LineSegment3d::create(plusXYZ(debugAnchorPt, -1.0, -1.0),
                                            plusXYZ(debugAnchorPt, 1.0, 1.0));
    builder.appendGeometry(x1->clone());
    auto x2 = dqGeom::LineSegment3d::create(plusXYZ(debugAnchorPt, 1.0, -1.0),
                                            plusXYZ(debugAnchorPt, -1.0, 1.0));
    builder.appendGeometry(x2->clone());

    // textRange 红框。
    GeometryAppearanceProps redParams;
    redParams.color = ColorDef::red.getTbgr();
    builder.appendGeometryParamsChange(redParams);  // void 面

    std::vector<dqGeom::Point3d> rangeCorners = {
        plusXYZ(layout.textRange.low, 0.0, 0.0),
        plusXYZ(layout.textRange.high, 0.0, 0.0),
        dqGeom::Point3d::From(layout.textRange.high.x, layout.textRange.high.y, 0.0),
        dqGeom::Point3d::From(layout.textRange.low.x, layout.textRange.high.y, 0.0),
        plusXYZ(layout.textRange.low, 0.0, 0.0),
    };
    for (auto& c : rangeCorners)
        c = transform.MultiplyPoint3d(c);
    builder.appendGeometry(dqGeom::LineString3d::create(rangeCorners)->clone());  // void 面
    return result;
}

// Ported from: debugSnapPoints (:153-165) — interval 点画为黑点（PointString3d
// 的 dqGeom 缺 → 每点小十字短线，视觉等价 EQUIVALENCE——头注登记）。
bool debugSnapPoints(GeometryStreamBuilder& builder, TextStyleSettings const& style,
                     dqGeom::Range2d const& range, dqGeom::Transform const& transform)
{
    if (style.frameShape == TextAnnotationFrameShape::None)
        return false;
    ComputeFrameArgs cfa;
    cfa.frame = style.frameShape;
    cfa.range = range;
    cfa.transform = transform;
    auto const points = computeIntervalPoints(cfa, 0.5, 0.25);
    if (!points || points->empty())
        return false;

    GeometryAppearanceProps params;
    params.color = ColorDef::black.getTbgr();
    params.weight = static_cast<int>(style.frameBorderWeight * 5.0);  // 点大于框沿（:160）

    builder.appendGeometryParamsChange(params);  // void 面
    double const cross = 0.05;  // 视觉等价十字半长（米）
    bool result = true;
    for (auto const& p : *points) {
        auto d1 = dqGeom::LineSegment3d::create(plusXYZ(p, -cross, -cross), plusXYZ(p, cross, cross));
        builder.appendGeometry(d1->clone());
        auto d2 = dqGeom::LineSegment3d::create(plusXYZ(p, -cross, cross), plusXYZ(p, cross, -cross));
        builder.appendGeometry(d2->clone());
    }
    return result;
}

// Ported from: debugRunLayout (:179-230) — 逐 run 布局框（类型着色）。
bool debugRunLayout(GeometryStreamBuilder& builder, TextBlockLayout const& layout,
                    dqGeom::Transform const& documentTransform)
{
    bool result = true;
    dqCommon::ColorDefProps lastColor = 0;
    bool haveColor = false;

    // 类型→色（:184-191 fromString 值）。
    auto colorFor = [&](TextBlockComponent const* src) -> ColorDefProps {
        if (!src)
            return ColorDef::black.getTbgr();
        switch (src->getKind()) {
            case TextBlockComponent::Kind::Text: return ColorDef::fromString("orange").getTbgr();
            case TextBlockComponent::Kind::LineBreak: return ColorDef::fromString("yellow").getTbgr();
            case TextBlockComponent::Kind::Fraction: return ColorDef::fromString("green").getTbgr();
            case TextBlockComponent::Kind::Tab: return ColorDef::fromString("aquamarine").getTbgr();
            case TextBlockComponent::Kind::Field: return ColorDef::fromString("purple").getTbgr();
            default: return ColorDef::black.getTbgr();
        }
    };

    auto drawBox = [&](RunLayout const& run, dqGeom::Transform const& lineTransform,
                       ColorDefProps newColor, bool isLineBreak) {
        if (!haveColor || lastColor != newColor) {
            GeometryAppearanceProps colorParams;
            colorParams.color = newColor;
            if (isLineBreak)
                colorParams.weight = 3;
            builder.appendGeometryParamsChange(colorParams);  // void 面（同 LeaderGeometry 登记）
            lastColor = newColor;
            haveColor = true;
        }
        dqGeom::Transform runTrans = dqGeom::Transform::CreateTranslation(
            run.offsetFromLine.x, run.offsetFromLine.y, 0.0);
        runTrans = lineTransform.MultiplyTransform(runTrans);
        std::vector<dqGeom::Point3d> corners = {
            plusXYZ(run.range.low, 0.0, 0.0),
            plusXYZ(run.range.high, 0.0, 0.0),
            dqGeom::Point3d::From(run.range.high.x, run.range.high.y, 0.0),
            dqGeom::Point3d::From(run.range.low.x, run.range.high.y, 0.0),
            plusXYZ(run.range.low, 0.0, 0.0),
        };
        for (auto& c : corners)
            c = runTrans.MultiplyPoint3d(c);
        builder.appendGeometry(dqGeom::LineString3d::create(corners)->clone());  // void 面
    };

    for (auto const& line : layout.lines) {
        dqGeom::Transform lineTrans = dqGeom::Transform::CreateTranslation(
            line.offsetFromDocument.x, line.offsetFromDocument.y, 0.0);
        lineTrans = documentTransform.MultiplyTransform(lineTrans);

        if (line.marker()) {
            bool const isMarker = true;
            (void)isMarker;
            drawBox(*line.marker(), lineTrans, ColorDef::fromString("pink").getTbgr(), false);
        }
        for (auto const& run : line.runs()) {
            bool const isLineBreak = run.source
                && run.source->getKind() == TextBlockComponent::Kind::LineBreak;
            drawBox(run, lineTrans, colorFor(run.source), isLineBreak);
        }
    }
    return result;
}
}  // namespace

// Ported from: itwinjs-core appendTextAnnotationGeometry (:60-114)
bool appendTextAnnotationGeometry(AppendTextAnnotationGeometryArgs& args)
{
    TextAnnotation const& annotation = *args.annotation;
    TextBlockLayout const& layout = *args.layout;
    TextStyleResolver const& resolver = *args.textStyleResolver;

    dqGeom::Transform const transform = annotation.computeTransform(layout.range, args.scaleFactor);

    // TextBlockGeometry（:66-75）。
    GeometryAppearanceProps params;
    params.subCategory = args.subCategoryId;
    if (args.renderPriority.annotationLabels.has_value())
        params.displayPriority = args.renderPriority.annotationLabels;  // elmPriority 映射（头注）

    TextBlockGeometryProps const entries = produceTextBlockGeometry(layout, transform);
    // 参考 builder.appendTextBlock(entries, params) 逐条目带参——DanQing 面以
    // 参数前置 change 承载（subcategory 色臂走缺省子类——语义等价，EQUIVALENCE
    // 头注登记）。
    args.builder->appendGeometryParamsChange(params);
    if (!args.builder->appendTextBlock(entries))
        return false;

    // Frame（:77-82）。
    auto const& blockSettings = resolver.blockSettings;
    if (blockSettings.frameShape != TextAnnotationFrameShape::None) {
        TextFrameStyleProps frame = blockSettings.toJSON().frame.value();
        if (!appendFrameToBuilder(*args.builder, frame, layout.range, transform, params))
            return false;
    }

    // annotation 优先级（:84-90）。
    if (args.renderPriority.annotation.has_value()) {
        params.displayPriority = args.renderPriority.annotation;
        args.builder->appendGeometryParamsChange(params);  // void 面
    }

    // Leaders（:92-97）。
    if (annotation.leaders && !annotation.leaders->empty()) {
        if (!appendLeadersToBuilder(*args.builder, *annotation.leaders, layout, transform,
                                    params, resolver, args.scaleFactor))
            return false;
    }

    // Debug geometry（:99-111）。
    if (args.wantDebugGeometry) {
        if (!debugAnchorPoint(*args.builder, annotation, layout, transform)
            || !debugRunLayout(*args.builder, layout, transform))
            return false;
        if (blockSettings.frameShape != TextAnnotationFrameShape::None) {
            if (!debugSnapPoints(*args.builder, blockSettings, layout.range, transform))
                return false;
        }
    }

    return true;
}

}  // namespace dqCommon

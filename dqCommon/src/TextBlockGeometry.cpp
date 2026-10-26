// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — produceTextBlockGeometry implementation
// Ported from: itwinjs-core core/backend/src/annotations/TextBlockGeometry.ts
#include <dqCommon/TextBlockGeometryProps.h>
#include <dqCommon/TextBlockLayout.h>
#include <dqCommon/TextString.h>

#include <dqGeom/LineSegment3d.h>
#include <dqGeom/Vector3d.h>

#include <cassert>

namespace dqCommon {
namespace {

// Ported from: GeometryContext (TextBlockGeometry.ts:14-17)
struct GeometryContext {
    std::optional<TextStyleColor> curColor;
    std::vector<TextBlockGeometryPropsEntry> entries;
};

// Ported from: setColor (:19-24)
void setColor(TextStyleColor color, GeometryContext& context)
{
    // TS `color !== context.curColor` 值比较（数字/字符串联合）。
    bool const same = context.curColor.has_value()
        && context.curColor->subcategory == color.subcategory
        && context.curColor->color == color.color;
    if (!same) {
        context.curColor = color;
        context.entries.push_back(TextBlockGeometryPropsEntry::FromColor(color));
    }
}

// Ported from: createTextString (:26-41)
TextString createTextString(std::string const& text, RunLayout const& run,
                            dqGeom::Point3d const* origin = nullptr)
{
    assert(!text.empty());

    TextStringProps props;
    props.text = text;
    props.font = run.fontId;
    props.height = run.style.textHeight;
    props.widthFactor = run.style.widthFactor;
    props.bold = run.style.isBold;
    props.italic = run.style.isItalic;
    props.underline = run.style.isUnderlined;
    if (origin)
        props.origin = *origin;
    return TextString(props);
}

// Ported from: processTextRun (:43-64)
void processTextRun(RunLayout const& run, dqGeom::Transform const& transform,
                    GeometryContext& context)
{
    assert(run.source && run.source->getKind() == TextBlockComponent::Kind::Text);
    auto const* textRun = run.source->toTextRun();
    std::string const text = textRun->content.substr(run.charOffset, run.numChars);
    if (text.empty())
        return;

    TextString ts = createTextString(text, run);
    if (textRun->baselineShift != BaselineShift::None) {
        bool const isSub = textRun->baselineShift == BaselineShift::Subscript;
        double const offsetFactor =
            isSub ? run.style.subScriptOffsetFactor : run.style.superScriptOffsetFactor;
        double const scale = isSub ? run.style.subScriptScale : run.style.superScriptScale;

        ts.origin = dqGeom::Point3d::From(ts.origin.x,
                                          ts.origin.y + offsetFactor * ts.height, ts.origin.z);
        ts.height *= scale;
    }

    ts.transformInPlace(transform);

    setColor(run.style.color, context);
    context.entries.push_back(TextBlockGeometryPropsEntry::FromText(ts.toJSON()));
}

// Ported from: processFieldRun (:66-78)
void processFieldRun(RunLayout const& run, dqGeom::Transform const& transform,
                     GeometryContext& context)
{
    assert(run.source && run.source->getKind() == TextBlockComponent::Kind::Field);
    auto const* fieldRun = run.source->toFieldRun();
    std::string const text = fieldRun->getCachedContent().substr(run.charOffset, run.numChars);
    if (text.empty())
        return;

    TextString ts = createTextString(text, run);
    ts.transformInPlace(transform);

    setColor(run.style.color, context);
    context.entries.push_back(TextBlockGeometryPropsEntry::FromText(ts.toJSON()));
}

// Ported from: createFractionTextString (:80-89)
TextString createFractionTextString(std::string const& text, RunLayout const& run,
                                    dqGeom::Point3d const& origin,
                                    dqGeom::Transform const& transform)
{
    TextString ts = createTextString(text, run, &origin);
    assert(ts.widthFactor.has_value());

    ts.height *= run.style.stackedFractionScale;
    ts.transformInPlace(transform);
    return ts;
}

// Ported from: processFractionRun (:91-139)
void processFractionRun(RunLayout const& run, dqGeom::Transform const& transform,
                        GeometryContext& context)
{
    auto const* source = run.source->toFractionRun();

    if (source->numerator.empty() && source->denominator.empty())
        return;

    assert(run.numeratorRange.has_value() && run.denominatorRange.has_value());

    double const fontW = run.style.textHeight * run.style.widthFactor;
    double const fontH = run.style.textHeight;

    dqGeom::Point3d const numeratorOffset(run.numeratorRange->low.x,
                                          run.numeratorRange->low.y, 0.0);
    dqGeom::Point3d const denominatorOffset(run.denominatorRange->low.x,
                                            run.denominatorRange->low.y, 0.0);

    setColor(run.style.color, context);

    if (!source->numerator.empty())
        context.entries.push_back(TextBlockGeometryPropsEntry::FromText(
            createFractionTextString(source->numerator, run, numeratorOffset, transform)
                .toJSON()));

    dqGeom::Range2d const& numeratorRange = *run.numeratorRange;
    dqGeom::Range2d const& denominatorRange = *run.denominatorRange;

    dqGeom::Point3d p0, p1;
    if (run.style.stackedFractionType == StackedFractionType::Horizontal) {
        double const fractionWidth =
            std::max(numeratorRange.XLength(), denominatorRange.XLength());
        double const y = 1.25 * denominatorRange.YLength();
        p0 = dqGeom::Point3d::From(0.0, y, 0.0);
        p1 = dqGeom::Point3d::From(fractionWidth, y, 0.0);
    } else {
        p0 = dqGeom::Point3d::From(denominatorRange.low.x - fontW / 2.0,
                                   denominatorRange.low.y + fontH * (1.0 / 3.0), 0.0);
        p1 = dqGeom::Point3d::From(p0.x + fontW, p0.y + fontH * 1.5, 0.0);
    }

    auto separator = dqGeom::LineSegment3d::create(p0, p1);
    separator->TryTransformInPlace(transform);

    TextBlockGeometryPropsEntry::Separator sep;
    sep.startPoint = separator->Point0Ref();
    sep.endPoint = separator->Point1Ref();
    context.entries.push_back(TextBlockGeometryPropsEntry::FromSeparator(std::move(sep)));

    if (!source->denominator.empty())
        context.entries.push_back(TextBlockGeometryPropsEntry::FromText(
            createFractionTextString(source->denominator, run, denominatorOffset, transform)
                .toJSON()));
}
}  // namespace

// Produces the geometry for a text block interpretable by GeometryStreamBuilder.
// Ported from: itwinjs-core produceTextBlockGeometry (TextBlockGeometry.ts:149-184)
TextBlockGeometryProps produceTextBlockGeometry(TextBlockLayout const& layout,
                                                dqGeom::Transform const& documentTransform)
{
    GeometryContext context;
    for (auto const& line : layout.lines) {
        dqGeom::Transform const lineTrans = dqGeom::Transform::CreateTranslation(
            line.offsetFromDocument.x, line.offsetFromDocument.y, 0.0);

        // List markers.
        if (line.marker()) {
            dqGeom::Transform markerTrans = dqGeom::Transform::CreateTranslation(
                line.marker()->offsetFromLine.x, line.marker()->offsetFromLine.y, 0.0);
            // markerTrans = lineTrans × markerTrans（multiplyTransformTransform
            // 结果落第二参）；再 documentTransform × markerTrans。
            markerTrans = lineTrans.MultiplyTransform(markerTrans);
            markerTrans = documentTransform.MultiplyTransform(markerTrans);
            processTextRun(*line.marker(), markerTrans, context);
        }

        // Runs——纯空白跳过（:165-167）。
        for (auto const& run : line.runs()) {
            if (run.source->isWhitespace())
                continue;

            dqGeom::Transform runTrans = dqGeom::Transform::CreateTranslation(
                run.offsetFromLine.x, run.offsetFromLine.y, 0.0);
            runTrans = lineTrans.MultiplyTransform(runTrans);
            runTrans = documentTransform.MultiplyTransform(runTrans);
            switch (run.source->getKind()) {
                case TextBlockComponent::Kind::Text:
                    processTextRun(run, runTrans, context);
                    break;
                case TextBlockComponent::Kind::Fraction:
                    processFractionRun(run, runTrans, context);
                    break;
                default:
                    processFieldRun(run, runTrans, context);
                    break;
            }
        }
    }

    TextBlockGeometryProps out;
    out.entries = std::move(context.entries);
    return out;
}

}  // namespace dqCommon

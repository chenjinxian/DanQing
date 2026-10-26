// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — text decoration tool implementation
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/TextDecoration.ts
//              （TextEditor/TextDecorationTool）+ 字形度量缝 QRawFont 宿主形。
#include "TextDecorationTool.h"

#include "dqApp/Application.h"
#include "dqApp/DecorateContext.h"
#include "dqApp/ToolAdmin.h"
#include "dqRender/GraphicBuilder.h"
#include "dqRender/RenderSystem.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/FrameGeometry.h>
#include <dqCommon/TextAnnotationGeometry.h>
#include <dqCommon/TextBlockGeometryProps.h>
#include <dqCommon/TextString.h>

#include <dqGeom/CurveCollection.h>
#include <dqGeom/LineSegment3d.h>
#include <dqGeom/LineString3d.h>

#include <QFont>
#include <QFontMetricsF>
#include <QPainterPath>
#include <QRawFont>

#include <cmath>
#include <cstdio>

namespace dta {

using namespace dqCommon;

// ---------------------------------------------------------------------------
// computeRangesForTextLayoutQt — 字形度量缝宿主形
// ---------------------------------------------------------------------------

// QFont 像素基准 1000px/em → advance_em = px/1000（参考
// DgnDb.computeRangesForText 的 EQUIVALENCE 宿主形——文件头登记）。
dqCommon::TextLayoutRanges computeRangesForTextLayoutQt(
    dqCommon::ComputeRangesForTextLayoutArgs const& args)
{
    QFont font;
    font.setPixelSize(1000);
    font.setBold(args.bold);
    font.setItalic(args.italic);
    QFontMetricsF const metrics(font);

    QString const text = QString::fromStdString(args.chars);
    double const advanceEm = metrics.horizontalAdvance(text) / 1000.0;
    double const width = advanceEm * args.widthFactor * args.textHeight;
    double const height = args.textHeight;

    // justification = 扣尾随空白 advance。
    QString trimmed = text;
    while (!trimmed.isEmpty() && trimmed.back().isSpace())
        trimmed.chop(1);
    double const justWidth = trimmed.isEmpty()
        ? 0.0
        : (metrics.horizontalAdvance(trimmed) / 1000.0) * args.widthFactor * args.textHeight;

    dqCommon::TextLayoutRanges out;
    out.layout = dqGeom::Range2d(0.0, 0.0, width, height);
    out.justification = dqGeom::Range2d(0.0, 0.0, justWidth, height);
    return out;
}

// ---------------------------------------------------------------------------
// TextAnnotationHost
// ---------------------------------------------------------------------------

// appendRunToLastChild 的末端段落定位（:102-125——列表最深项段落形态）。
namespace {
dqCommon::Paragraph& lastParagraphOf(dqCommon::TextBlock& block, std::string const& fontName)
{
    if (block.getChildren().empty())
        block.appendParagraph();
    dqCommon::TextBlockComponent* cur = block.getChildren().back().get();
    while (true) {
        if (auto* p = cur->toParagraph())
            return *p;
        if (auto* l = cur->toList()) {
            if (l->getChildren().empty()) {
                dqCommon::Paragraph::Props pp;
                pp.styleOverrides = dqCommon::TextStyleSettingsProps{};
                pp.styleOverrides->font = dqCommon::FontFamilySelector{
                    dqCommon::FontType::TrueType, fontName};
                l->getChildren().push_back(dqCommon::Paragraph::create(&pp));
            }
            cur = l->getChildren().back().get();
            continue;
        }
        block.appendParagraph();
        return *block.getChildren().back()->toParagraph();
    }
}
}  // namespace

TextAnnotationHost::TextAnnotationHost()
{
    runStyle.font = dqCommon::FontFamilySelector{dqCommon::FontType::TrueType, "Arial"};
    textBlock = dqCommon::TextBlock::create();
}

// Ported from: TextEditor.init (:146-153)
void TextAnnotationHost::init(dqApp::Viewport& vp)
{
    clear();
    m_vp = &vp;
    dqApp::Application::Get().GetViewManager().AddDecorator(this);
}

// Ported from: TextEditor.clear (:155-170) — anchor 复位 center/middle。
void TextAnnotationHost::clear()
{
    if (m_vp)
        dqApp::Application::Get().GetViewManager().DropDecorator(this);
    m_vp = nullptr;
    m_graphic = nullptr;
    m_stream.clear();
    textBlock = dqCommon::TextBlock::create();
    origin = dqGeom::Point3d::FromZero();
    rotation = 0.0;
    offsetX = offsetY = 0.0;
    anchor = {dqCommon::AnchorHorizontal::Center, dqCommon::AnchorVertical::Middle};
    debugAnchorPointAndRange = false;
    runStyle = dqCommon::TextStyleSettingsProps{};
    runStyle.font = dqCommon::FontFamilySelector{dqCommon::FontType::TrueType, "Arial"};
    baselineShift = dqCommon::BaselineShift::None;
    leaders.clear();
}

// Ported from: TextEditor.appendText (:172-180)
void TextAnnotationHost::appendText(std::string const& content)
{
    auto& para = lastParagraphOf(*textBlock,
                                 runStyle.font ? runStyle.font->name : std::string("Arial"));
    dqCommon::TextRun::Props p;
    p.styleOverrides = runStyle;
    p.content = content;
    p.baselineShift = baselineShift;
    para.getChildren().push_back(dqCommon::TextRun::create(&p));
}

// Ported from: TextEditor.appendFraction (:181-188)
void TextAnnotationHost::appendFraction(std::string const& numerator,
                                        std::string const& denominator)
{
    auto& para = lastParagraphOf(*textBlock,
                                 runStyle.font ? runStyle.font->name : std::string("Arial"));
    dqCommon::FractionRun::Props p;
    p.styleOverrides = runStyle;
    p.numerator = numerator;
    p.denominator = denominator;
    para.getChildren().push_back(dqCommon::FractionRun::create(&p));
}

// Ported from: TextEditor.appendTab (:203-208)
void TextAnnotationHost::appendTab(std::optional<double> spaces)
{
    auto& para = lastParagraphOf(*textBlock,
                                 runStyle.font ? runStyle.font->name : std::string("Arial"));
    dqCommon::TabRun::Props p;
    p.styleOverrides = runStyle;
    if (spaces)
        p.styleOverrides->tabInterval = *spaces;
    para.getChildren().push_back(dqCommon::TabRun::create(&p));
}

// Ported from: TextEditor.appendBreak (:209-213)
void TextAnnotationHost::appendBreak()
{
    auto& para = lastParagraphOf(*textBlock,
                                 runStyle.font ? runStyle.font->name : std::string("Arial"));
    dqCommon::LineBreakRun::Props p;
    p.styleOverrides = runStyle;
    para.getChildren().push_back(dqCommon::LineBreakRun::create(&p));
}

// Ported from: TextEditor.appendParagraph (:225-227)
void TextAnnotationHost::appendParagraph()
{
    dqCommon::Paragraph::Props pp;
    pp.styleOverrides = runStyle;
    textBlock->appendParagraph(&pp, false);
}

// Ported from: TextEditor.setDocumentWidth (:233-235)
void TextAnnotationHost::setDocumentWidth(double width)
{
    textBlock->width = width;
}

// Ported from: TextEditor.setIndentation (:229-238)
void TextAnnotationHost::setIndentation(double indentation)
{
    if (textBlock->getChildren().empty())
        return;
    auto& para = *textBlock->getChildren().back()->toParagraph();
    para.getStyleOverrides().indentation = indentation;
    runStyle.indentation = indentation;
}

// Ported from: TextEditor.setMargins (:239-249)
void TextAnnotationHost::setMargins(dqCommon::TextBlockMargins const& margins)
{
    runStyle.margins = dqCommon::TextBlockMargins{
        margins.left.value_or(0.0), margins.right.value_or(0.0),
        margins.top.value_or(0.0), margins.bottom.value_or(0.0)};
}

// Ported from: TextEditor.setLeaderProps (:250-253)
void TextAnnotationHost::setLeaderProps()
{
    dqCommon::TextAnnotationLeader leader;
    leader.startPoint = dqGeom::Point3d::From(-20.0, 0.0, 0.0);
    leader.attachment = dqCommon::LeaderAttachment{};  // Nearest
    leaders.push_back(std::move(leader));
}

// Ported from: TextEditor.annotationProps (:86-100)
dqCommon::TextAnnotationProps TextAnnotationHost::annotationProps() const
{
    dqCommon::TextAnnotationCreateArgs args;
    {   // TextBlock 移动唯一（children unique_ptr）——经 props 往返复制。
        dqCommon::TextBlock::Props const blockProps = textBlock->toJSONBlockProps();
        args.textBlock = std::move(*dqCommon::TextBlock::create(&blockProps));
    }
    args.anchor.vertical = anchor.vertical;
    args.anchor.horizontal = anchor.horizontal;
    args.orientation = dqGeom::YawPitchRollAngles::CreateDegrees(rotation, 0.0, 0.0);
    args.offset = dqGeom::Point3d::From(offsetX, offsetY, 0.0);
    args.leaders = leaders;
    dqCommon::TextAnnotation anno = dqCommon::TextAnnotation::create(&args);
    return anno.toJSON();
}

// 曲线几何 → 折线（LineString 直通 / LineSegment 两点 / 其余 32 段采样 /
// Loop·Path 链 → 逐子曲线）。
namespace {
void strokeToPolyline(dqGeom::GeometryQuery const& g, std::vector<dqGeom::Point3d>& pts)
{
    auto strokeCurve = [&](dqGeom::CurvePrimitive const& cp) {
        if (cp.GetCurveType() == dqGeom::CurveType::LineString) {
            auto const& ls = static_cast<dqGeom::LineString3d const&>(cp);
            for (auto const& p : ls.Points())
                pts.push_back(p);
            return;
        }
        if (cp.GetCurveType() == dqGeom::CurveType::LineSegment) {
            auto const& seg = static_cast<dqGeom::LineSegment3d const&>(cp);
            pts.push_back(seg.Point0Ref());
            pts.push_back(seg.Point1Ref());
            return;
        }
        constexpr int kSegs = 32;
        for (int i = 0; i <= kSegs; ++i)
            pts.push_back(cp.FractionToPoint(
                static_cast<double>(i) / static_cast<double>(kSegs)));
    };

    if (g.Category() == dqGeom::GeometryCategory::CurvePrimitive) {
        strokeCurve(static_cast<dqGeom::CurvePrimitive const&>(g));
        return;
    }
    if (g.Category() == dqGeom::GeometryCategory::CurveCollection) {
        auto const& chain = static_cast<dqGeom::CurveChain const&>(g);
        for (auto const& child : chain.Curves())
            if (child)
                strokeCurve(*child);
        return;
    }
}
}  // namespace

// Ported from: TextEditor.update (:263-292) — 本地面（参考 RPC
// generateTextAnnotationGeometry → readElementGraphics 的 DanQing 等价 =
// 进程内 layout + appendTextAnnotationGeometry + GeometryStream → GraphicBuilder）。
bool TextAnnotationHost::update(dqApp::Viewport& vp)
{
    m_vp = &vp;
    if (textBlock->isEmpty() || textBlock->isWhitespace())
        return false;

    // layout（注入缝 = QRawFont 度量；参考缺省 iModel 原生面不在移植范围）。
    dqCommon::LayoutTextBlockArgs largs;
    largs.textBlock = textBlock.get();
    largs.findTextStyle = [](uint64_t) { return dqCommon::TextStyleSettings::defaults(); };
    largs.computeTextRange = computeRangesForTextLayoutQt;
    largs.findFontId = [](std::string const&, std::optional<dqCommon::FontType>) {
        return dqCommon::FontId{1};
    };
    dqCommon::TextStyleResolver resolver(*textBlock, 0, largs.findTextStyle);
    dqCommon::LayoutContext lctx(resolver, largs.computeTextRange, largs.findFontId, nullptr);
    dqCommon::TextBlockLayout const layout(*textBlock, lctx);

    dqCommon::TextAnnotationProps ap = annotationProps();
    dqCommon::TextAnnotation annotation = dqCommon::TextAnnotation::fromJSON(&ap);

    m_stream.clear();
    if (std::getenv("DANQING_TEXT_TRACE"))
        std::fprintf(stderr, "[TEXT-upd] layout lines=%zu\n", layout.lines.size());
    dqCommon::AppendTextAnnotationGeometryArgs gargs;
    gargs.annotation = &annotation;
    gargs.layout = &layout;
    gargs.textStyleResolver = &resolver;
    gargs.scaleFactor = 1.0;
    gargs.builder = &m_stream;
    gargs.wantDebugGeometry = debugAnchorPointAndRange;
    dqCommon::RenderPriority prio;
    prio.annotation = 100;        // 参考 { annotation: 100, annotationLabels: 110 }
    prio.annotationLabels = 110;
    gargs.renderPriority = prio;
    if (!dqCommon::appendTextAnnotationGeometry(gargs)) {
        if (std::getenv("DANQING_TEXT_TRACE"))
            std::fprintf(stderr, "[TEXT-upd] appendTextAnnotationGeometry FAILED\n");
        return false;
    }
    if (std::getenv("DANQING_TEXT_TRACE"))
        std::fprintf(stderr, "[TEXT-upd] entries=%zu geoms=%zu\n",
                     m_stream.Entries().size(), m_stream.Geometries().size());

    auto* system = vp.renderSystem();
    if (!system)
        return false;
    if (std::getenv("DANQING_TEXT_TRACE"))
        std::printf("[TEXT-upd] system ok\n");
    dqRender::GraphicBuilderOptions opts;
    opts.type = dqRender::GraphicType::Scene;
    // finish() 需要 computeChordTolerance 闭包（PrimitiveBuilder 的 LOD 门；
    // 缺省恒 null → 产出 null——AcsTriadDecorator.cpp:355-359 同式）。
    double const worldPerPixel =
        vp.GetViewingSpace().getPixelSizeAtPoint(&origin);
    opts.computeChordTolerance = [worldPerPixel]() { return worldPerPixel; };
    auto builder = vp.createGraphicBuilder(opts);
    if (!builder)
        return false;

    // GeometryStream → GraphicBuilder。text 臂 = QRawFont 字形轮廓折线（参考
    // DgnFont 填充多边形的轮廓 EQUIVALENCE——文件头登记）；其余曲线臂 = 折线。
    // 放置变换：参考把 TextAnnotation 作为元素以 Placement2d（origin +
    // angle=0）挂入 iModel（TextDecoration.ts:89-94 placementProps、
    // :300 updateElement 消费），后端生成的元素几何为**局部坐标**，placement
    // 由渲染侧应用。本进程内宿主在构建 graphic 时等价应用该放置——angle 恒
    // 0 ⇒ 纯平移（与参考同一数学，无发散）。
    dqGeom::Transform const placement =
        dqGeom::Transform::CreateTranslation(origin.x, origin.y, origin.z);

    QFont glyphFont;
    glyphFont.setPixelSize(100);
    double constexpr kGlyphEmPx = 100.0;

    size_t geomIndex = 0;
    for (auto const& entry : m_stream.Entries()) {
        if (entry.opcode == dqCommon::ElementGeometryOpcode::BasicSymbology) {
            if (entry.appearance) {
                dqCommon::ColorDef const color =
                    dqCommon::ColorDef::fromTbgr(entry.appearance->color.value_or(0));
                builder->setSymbology(color, color, entry.appearance->weight.value_or(1));
            }
            continue;
        }
        if (entry.opcode == dqCommon::ElementGeometryOpcode::TextString) {
            if (!entry.textString)
                continue;
            dqCommon::TextString const ts(*entry.textString);
            QFont f = glyphFont;
            f.setBold(ts.bold);
            f.setItalic(ts.italic);
            QRawFont const font = QRawFont::fromFont(f);
            QString const qtext = QString::fromStdString(ts.text);
            QVector<quint32> const glyphIndices = font.glyphIndexesForString(qtext);
            QList<QPointF> const advances = font.advancesForGlyphIndexes(glyphIndices);

            // scale = height/emPx（宽向 × widthFactor；y 翻转——QPainterPath
            // y 向下 vs 文本数学 y 向上）。
            double const scale = ts.height / kGlyphEmPx;
            double const wf = ts.widthFactor ? *ts.widthFactor : 1.0;
            dqGeom::Transform const place = placement.MultiplyTransform(
                dqGeom::Transform::CreateOriginAndMatrix(ts.origin, ts.rotation.ToMatrix3d()));

            double penX = 0.0;
            for (int gi = 0; gi < glyphIndices.size(); ++gi) {
                QPainterPath const glyphPath = font.pathForGlyph(glyphIndices[gi]);
                if (!glyphPath.isEmpty()) {
                    QList<QPolygonF> const subpaths = glyphPath.toSubpathPolygons();
                    for (QPolygonF const& sub : subpaths) {
                        std::vector<dqGeom::Point3d> pts;
                        pts.reserve(sub.size());
                        for (QPointF const& pt : sub) {
                            dqGeom::Point3d const local(pt.x() * scale * wf,
                                                        -pt.y() * scale, 0.0);
                            pts.push_back(place.MultiplyPoint3d(
                                dqGeom::Point3d::From(penX + local.x, local.y, local.z)));
                        }
                        if (pts.size() >= 2)
                            builder->addLineString(pts.data(), pts.size());
                    }
                }
                if (gi < advances.size())
                    penX += advances[gi].x() * scale * wf;
            }
            continue;
        }
        // 曲线几何（separator/frame/leader——与 Geometries() 按序对应）。
        if (geomIndex >= m_stream.Geometries().size())
            continue;
        auto const& g = m_stream.Geometries()[geomIndex++];
        if (!g)
            continue;
        std::vector<dqGeom::Point3d> pts;
        strokeToPolyline(*g, pts);
        for (auto& pt : pts)
            pt = placement.MultiplyPoint3d(pt);
        if (pts.size() >= 2)
            builder->addLineString(pts.data(), pts.size());
    }

    if (std::getenv("DANQING_TEXT_TRACE"))
        std::printf("[TEXT-upd] glyph loop done\n");
    m_graphic = builder->finish();
    if (std::getenv("DANQING_TEXT_TRACE"))
        std::printf("[TEXT-upd] finish=%p\n", static_cast<void*>(m_graphic));
    // 参考 :290 IModelApp.viewManager.invalidateCachedDecorationsAllViews(this) —
    // 缓存持有上一版图形（DanQing DecorationsCache 直接 delete）；删条目使下一次
    // CollectDecorations 重新走 decorate 挂新图形（同时触发 InvalidateDecorations）。
    vp.InvalidateCachedDecorations(this);
    vp.RequestRedraw();
    return m_graphic != nullptr;
}

// Ported from: TextEditor.decorate (:295-299)
void TextAnnotationHost::Decorate(dqApp::DecorateContext& context)
{
    if (std::getenv("DANQING_TEXT_TRACE"))
        std::fprintf(stderr, "[TEXT-deco] graphic=%p\n", static_cast<void*>(m_graphic));
    if (m_graphic)
        context.AddDecoration(dqRender::GraphicType::Scene, m_graphic);
}

// ---------------------------------------------------------------------------
// runTextDecorationCommand — TextDecorationTool.parseAndRun switch（:335-627）
// ---------------------------------------------------------------------------

namespace {
// TextBlock stringify log（:544-554 console.log 的 stderr 形）。
void logAnnotation(TextAnnotationHost const& editor)
{
    dqCommon::TextBlockStringifyOptions opts;
    opts.paragraphBreak = "\n";
    opts.lineBreak = "\n";
    std::fprintf(stderr, "[TEXT] %s\n", editor.textBlock->stringify(&opts).c_str());
    std::fflush(stderr);
}

double parseNum(std::string const& s, bool* ok = nullptr)
{
    try {
        size_t idx = 0;
        double const v = std::stod(s, &idx);
        if (ok)
            *ok = idx == s.size();
        return v;
    } catch (...) {
        if (ok)
            *ok = false;
        return 0.0;
    }
}
}  // namespace

// Ported from: TextDecorationTool.parseAndRun (:335-627) — 命令面直映。
bool runTextDecorationCommand(dqApp::Viewport& vp, TextAnnotationHost& editor,
                              std::vector<std::string> const& inArgs, std::string* outError)
{
    auto fail = [&](std::string const& msg) {
        if (outError)
            *outError = msg;
        return false;
    };
    if (inArgs.empty())
        return fail("Expected a command");
    std::string const cmd = inArgs[0];
    std::string const arg = inArgs.size() > 1 ? inArgs[1] : std::string();

    if (cmd == "clear")
    { editor.clear(); return true; }
    if (cmd == "init")
    { editor.init(vp); return true; }
    if (cmd == "center") {
        // vp.view.getCenter() —— NPC 立方中心 → 世界（DanQing 面）。
        editor.origin = vp.NpcToWorld(dqGeom::Point3d::From(0.5, 0.5, 0.5));
        return true;
    }
    if (cmd == "rotation") {
        bool ok = false;
        double const v = parseNum(arg, &ok);
        if (!ok)
            return fail("Expected a number for rotation");
        editor.rotation = v;
        return true;
    }
    if (cmd == "offset") {
        if (inArgs.size() != 3)
            return fail("Expected x and y");
        editor.offsetX = parseNum(inArgs[1]);
        editor.offsetY = parseNum(inArgs[2]);
        return true;
    }
    if (cmd == "font") {
        editor.runStyle.font = dqCommon::FontFamilySelector{dqCommon::FontType::TrueType, arg};
        return true;
    }
    if (cmd == "text") {
        if (arg.empty())
            return fail("Expected text content");
        editor.appendText(arg);
        return true;
    }
    if (cmd == "fraction") {
        if (inArgs.size() != 3)
            return fail("Expected numerator and denominator");
        editor.appendFraction(inArgs[1], inArgs[2]);
        return true;
    }
    if (cmd == "break")
    { editor.appendBreak(); return true; }
    if (cmd == "tab") {
        std::optional<double> spaces;
        if (!arg.empty())
            spaces = parseNum(arg);
        editor.appendTab(spaces);
        return true;
    }
    if (cmd == "paragraph")
    { editor.appendParagraph(); return true; }
    if (cmd == "color") {
        editor.runStyle.color =
            dqCommon::TextStyleColor::FromColor(dqCommon::ColorDef::fromString(arg));
        return true;
    }
    if (cmd == "docheight") {
        editor.textBlock->getStyleOverrides().textHeight = parseNum(arg);
        return true;
    }
    if (cmd == "textheight") {
        editor.runStyle.textHeight = parseNum(arg);
        return true;
    }
    if (cmd == "widthfactor") {
        editor.textBlock->getStyleOverrides().widthFactor = parseNum(arg);
        return true;
    }
    if (cmd == "width") {
        editor.setDocumentWidth(parseNum(arg));
        return true;
    }
    if (cmd == "justify") {
        if (arg != "left" && arg != "center" && arg != "right")
            return fail("Expected left, right, or center");
        if (arg == "left")
            editor.textBlock->getStyleOverrides().justification = dqCommon::TextJustification::Left;
        else if (arg == "center")
            editor.textBlock->getStyleOverrides().justification = dqCommon::TextJustification::Center;
        else
            editor.textBlock->getStyleOverrides().justification = dqCommon::TextJustification::Right;
        return true;
    }
    if (cmd == "indent") {
        editor.setIndentation(parseNum(arg));
        return true;
    }
    if (cmd == "spacing") {
        editor.textBlock->getStyleOverrides().lineSpacingFactor = parseNum(arg);
        return true;
    }
    if (cmd == "bold") {
        editor.runStyle.isBold = !editor.runStyle.isBold.value_or(false);
        return true;
    }
    if (cmd == "italic") {
        editor.runStyle.isItalic = !editor.runStyle.isItalic.value_or(false);
        return true;
    }
    if (cmd == "underline") {
        editor.runStyle.isUnderlined = !editor.runStyle.isUnderlined.value_or(false);
        return true;
    }
    if (cmd == "fractionscale") {
        editor.runStyle.stackedFractionScale = parseNum(arg);
        return true;
    }
    if (cmd == "fractiontype") {
        if (arg != "horizontal" && arg != "diagonal")
            return fail("Expected horizontal or diagonal");
        editor.runStyle.stackedFractionType =
            arg == "horizontal" ? dqCommon::StackedFractionType::Horizontal
                                : dqCommon::StackedFractionType::Diagonal;
        return true;
    }
    if (cmd == "subscriptscale") {
        bool ok = false;
        double const v = parseNum(arg, &ok);
        if (!ok)
            return fail("Expected a number for subscript scale");
        editor.runStyle.subScriptScale = v;
        return true;
    }
    if (cmd == "superscriptscale") {
        bool ok = false;
        double const v = parseNum(arg, &ok);
        if (!ok)
            return fail("Expected a number for superscript scale");
        editor.runStyle.superScriptScale = v;
        return true;
    }
    if (cmd == "shift") {
        if (arg != "none" && arg != "superscript" && arg != "subscript")
            return fail("Expected none, superscript, or subscript");
        editor.baselineShift = arg;
        return true;
    }
    if (cmd == "anchor") {
        if (arg == "left" || arg == "center" || arg == "right")
            editor.anchor.horizontal = arg;
        else if (arg == "top" || arg == "middle" || arg == "bottom")
            editor.anchor.vertical = arg;
        else
            return fail("Expected top, middle, bottom, left, center, or right");
        return true;
    }
    if (cmd == "margin") {
        if (inArgs.size() != 3)
            return fail("Expected margin location followed by a number");
        std::string const loc = inArgs[1];
        double const val = parseNum(inArgs[2]);
        dqCommon::TextBlockMargins m;
        if (loc == "left") m.left = val;
        else if (loc == "right") m.right = val;
        else if (loc == "top") m.top = val;
        else if (loc == "bottom") m.bottom = val;
        else if (loc == "all") { m.left = m.right = m.top = m.bottom = val; }
        else if (loc == "horizontal") { m.left = m.right = val; }
        else if (loc == "vertical") { m.top = m.bottom = val; }
        else return fail("Expected left, right, top, bottom, all, horizontal, or vertical");
        editor.setMargins(m);
        return true;
    }
    if (cmd == "debug") {
        editor.debugAnchorPointAndRange = !editor.debugAnchorPointAndRange;
        return true;
    }
    if (cmd == "log") {
        logAnnotation(editor);
        return true;
    }
    if (cmd == "frame") {
        if (inArgs.size() != 3)
            return fail("Expected shape, fillColor, borderColor, borderWeight");
        std::string const key = inArgs[1];
        std::string const val = inArgs[2];
        auto& docFrame = editor.textBlock->getStyleOverrides().frame;
        if (!docFrame)
            docFrame = dqCommon::TextFrameStyleProps{};
        if (key == "shape") {
            if (val == "none") docFrame->shape = dqCommon::TextAnnotationFrameShape::None;
            else if (val == "line") docFrame->shape = dqCommon::TextAnnotationFrameShape::Line;
            else if (val == "rectangle") docFrame->shape = dqCommon::TextAnnotationFrameShape::Rectangle;
            else if (val == "circle") docFrame->shape = dqCommon::TextAnnotationFrameShape::Circle;
            else if (val == "equilateralTriangle") docFrame->shape = dqCommon::TextAnnotationFrameShape::EquilateralTriangle;
            else if (val == "diamond") docFrame->shape = dqCommon::TextAnnotationFrameShape::Diamond;
            else if (val == "square") docFrame->shape = dqCommon::TextAnnotationFrameShape::Square;
            else if (val == "pentagon") docFrame->shape = dqCommon::TextAnnotationFrameShape::Pentagon;
            else if (val == "hexagon") docFrame->shape = dqCommon::TextAnnotationFrameShape::Hexagon;
            else if (val == "octagon") docFrame->shape = dqCommon::TextAnnotationFrameShape::Octagon;
            else if (val == "capsule") docFrame->shape = dqCommon::TextAnnotationFrameShape::Capsule;
            else if (val == "roundedRectangle") docFrame->shape = dqCommon::TextAnnotationFrameShape::RoundedRectangle;
            else return fail("Unknown frame shape");
        } else if (key == "fillColor") {
            if (val == "background")
                docFrame->fillColor = dqCommon::TextAnnotationFillColor::Background();
            else if (val == "subcategory")
                docFrame->fillColor = dqCommon::TextAnnotationFillColor::FromSubcategory();
            else
                docFrame->fillColor =
                    dqCommon::TextAnnotationFillColor::FromColor(dqCommon::ColorDef::fromString(val));
        } else if (key == "borderColor") {
            docFrame->borderColor =
                dqCommon::TextStyleColor::FromColor(dqCommon::ColorDef::fromString(val));
        } else if (key == "borderWeight") {
            docFrame->borderWeight = parseNum(val);
        } else {
            return fail("Expected shape, fillColor, borderColor, borderWeight");
        }
        return true;
    }
    if (cmd == "leader") {
        editor.setLeaderProps();
        return true;
    }
    if (cmd == "applystyle") {
        // ➖9 面本地承载：样式表缺席——overrides 清除（文件头登记）。
        editor.textBlock->clearStyleOverrides();
        return true;
    }
    if (cmd == "insert" || cmd == "insertstyle" || cmd == "updatestyle"
        || cmd == "deletestyle") {
        // ➖9 编辑写入族（用户 2026-10-01 裁决——iModel 写入不做引擎实现）。
        return fail(cmd + " is not supported in DanQing (iModel write-back excluded)");
    }
    if (cmd == "update") {
        if (!editor.isInitialized())
            editor.init(vp);
        if (!editor.update(vp))
            return fail("update failed (empty block?)");
        return true;
    }
    return fail("Unknown command: " + cmd);
}

// 参考 module 级单例（:320）——函数局部静态（跨 keyin 存活的编辑态）。
TextAnnotationHost& globalTextEditor()
{
    static TextAnnotationHost s_editor;
    return s_editor;
}

// Ported from: TextDecorationTool.parseAndRun (:324-334) —
// selectedView 缺席 = false；2d 臂（baseModelId）不适用——DanQing 无 2d 视口
//（登记）；命令分派经 runTextDecorationCommand。
bool TextDecorationTool::parseAndRun(std::vector<std::string> const& args)
{
    auto* vp = dqApp::Application::Get().GetViewManager().GetSelectedViewport();
    if (!vp)
        return false;
    std::string error;
    bool const ok = runTextDecorationCommand(*vp, globalTextEditor(), args, &error);
    if (!ok && !error.empty()) {
        std::fprintf(stderr, "[TEXT] %s\n", error.c_str());
        std::fflush(stderr);
    }
    return ok;
}

}  // namespace dta

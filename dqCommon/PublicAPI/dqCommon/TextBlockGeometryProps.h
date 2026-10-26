// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — text block geometry products
// Ported from: itwinjs-core core/common/src/annotation/TextBlockGeometryProps.ts
#pragma once

#include "DqCommon.h"
#include "TextString.h"
#include "TextStyle.h"

#include <dqGeom/Point3d.h>
#include <dqGeom/Transform.h>

#include <variant>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// A single entry in a TextBlockGeometryProps: a TextString, a fraction
// separator (line segment), or a color change applying to subsequent entries.
// Ported from: itwinjs-core TextBlockGeometryPropsEntry
//              (TextBlockGeometryProps.ts:20-33) —
// TS 三臂判别（never 字段互斥）→ std::variant（§3.4 适配）。
struct TextBlockGeometryPropsEntry {
    // separator 臂：LineSegment3d 两端点。
    struct Separator {
        dqGeom::Point3d startPoint;
        dqGeom::Point3d endPoint;
        // Point3d 无运算符重载（§7.2）——AlmostEqual（1e-6 = smallMetricDistance）。
        bool operator==(Separator const& o) const noexcept
        {
            return startPoint.AlmostEqual(o.startPoint, 1.0e-6)
                && endPoint.AlmostEqual(o.endPoint, 1.0e-6);
        }
    };

    std::variant<TextStringProps, Separator, TextStyleColor> payload;

    static TextBlockGeometryPropsEntry FromText(TextStringProps t) { return {std::move(t)}; }
    static TextBlockGeometryPropsEntry FromSeparator(Separator s) { return {std::move(s)}; }
    static TextBlockGeometryPropsEntry FromColor(TextStyleColor c) { return {std::move(c)}; }
};

// JSON representation of the geometric primitives displaying a TextBlock.
// Ported from: itwinjs-core TextBlockGeometryProps (TextBlockGeometryProps.ts:41-46)
struct TextBlockGeometryProps {
    // The set of geometric primitives representing the TextBlock contents.
    std::vector<TextBlockGeometryPropsEntry> entries;
};

class TextBlockLayout;

// Produces the geometry for a text block interpretable by GeometryStreamBuilder
// (whole-TextAnnotation form = appendTextAnnotationGeometry，Y-c 后续件)。
// Ported from: itwinjs-core produceTextBlockGeometry
//              (TextBlockGeometry.ts:149-184)
DQ_COMMON_EXPORT TextBlockGeometryProps produceTextBlockGeometry(
    TextBlockLayout const& layout, dqGeom::Transform const& documentTransform);

END_DQ_COMMON_NAMESPACE

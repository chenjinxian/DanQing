// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — a single line of text
// Ported from: itwinjs-core core/common/src/geometry/TextString.ts
#pragma once

#include "DqCommon.h"
#include "Fonts.h"

#include <dqGeom/Point3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/YawPitchRollAngles.h>

#include <optional>
#include <string>

BEGIN_DQ_COMMON_NAMESPACE

// Properties for a TextString.
// Ported from: itwinjs-core TextStringProps (TextString.ts:26-47)
struct TextStringProps {
    std::string text;
    FontId font = 0;
    double height = 0.0;                        // text height, in meters
    std::optional<double> widthFactor;          // width / height ratio, default 1.0
    bool bold = false;
    bool italic = false;
    bool underline = false;
    dqGeom::Point3d origin;                     // relative to element's placement
    dqGeom::YawPitchRollAngles rotation;        // relative to element's placement
};

// A single line of text, all with the same font, styles (underline, bold, italic),
// and size. This class also holds the origin and direction for the text.
// A paragraph is composed of one or more instances of TextStrings.
// Ported from: itwinjs-core TextString (TextString.ts:53-127)
class DQ_COMMON_EXPORT TextString {
public:
    std::string text;
    FontId font = 0;
    double height = 0.0;
    std::optional<double> widthFactor;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    dqGeom::Point3d origin;
    dqGeom::YawPitchRollAngles rotation;

    // Ported from: itwinjs-core TextString ctor (TextString.ts:76-86)
    explicit TextString(TextStringProps const& props)
        : text(props.text)
        , font(props.font)
        , height(props.height)
        , widthFactor(props.widthFactor)
        , bold(props.bold)
        , italic(props.italic)
        , underline(props.underline)
        , origin(props.origin)
        , rotation(props.rotation)
    {
    }

    // width / height ratio. Default is 1.0.
    // Ported from: itwinjs-core TextString.width (TextString.ts:75)
    double width() const noexcept { return height * (widthFactor ? *widthFactor : 1.0); }

    // Ported from: itwinjs-core TextString.toJSON (TextString.ts:88-105) —
    // widthFactor/bold/italic/underline 恒写（undefined 字段在 TS JSON 化时
    // 丢弃——此处以 optional 承载同语义）；origin/rotation 仅在非零/非恒等时写。
    TextStringProps toJSON() const;

    // Transform this TextString in place. Returns false and leaves this unaltered
    // on degenerate transforms (singular matrix / collapsed size).
    // Ported from: itwinjs-core TextString.transformInPlace (TextString.ts:107-126)
    bool transformInPlace(dqGeom::Transform const& transform);
};

END_DQ_COMMON_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — text block layout result
// Ported from: itwinjs-core core/common/src/annotation/TextBlockLayoutResult.ts
#pragma once

#include "DqCommon.h"
#include "Fonts.h"
#include "TextStyle.h"

#include <dqGeom/Point2d.h>
#include <dqGeom/Range3d.h>  // dqGeom::Range2d 宿主

#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Represents a single run in a LineLayoutResult.
// Ported from: itwinjs-core RunLayoutResult (TextBlockLayoutResult.ts:15-38)
struct RunLayoutResult {
    // The ID of the font for the run.
    FontId fontId = 0;
    // Characters already appeared in the layout from the source run（单源 Run 可
    // 跨多行拆分）。
    uint32_t characterOffset = 0;
    // The number of characters in this result.
    uint32_t characterCount = 0;
    // Offset from the top/left of the owning line（XAndY）。
    dqGeom::Point2d offsetFromLine;
    // Bounding box enclosing this run's content.
    dqGeom::Range2d range;
    // Resolved style for the run（overrides + 父级样式合并后）。
    TextStyleSettingsProps textStyle;
    // Bounding box used when justifying the run（可小于 range）。
    std::optional<dqGeom::Range2d> justificationRange;
    // FractionRun numerator range.
    std::optional<dqGeom::Range2d> numeratorRange;
    // FractionRun denominator range.
    std::optional<dqGeom::Range2d> denominatorRange;
};

// Represents a single line in a TextBlockLayoutResult.
// Ported from: itwinjs-core LineLayoutResult (TextBlockLayoutResult.ts:45-58)
struct LineLayoutResult {
    // The runs contained in the line.
    std::vector<RunLayoutResult> runs;
    // The marker run for the line, if any.
    std::optional<RunLayoutResult> marker;
    // The range of all runs (including the marker) in the line.
    dqGeom::Range2d range;
    // Bounding box used when justifying the line.
    dqGeom::Range2d justificationRange;
    // Offset from the top/left of the TextBlock.
    dqGeom::Point2d offsetFromDocument;
};

// The result of laying out a TextBlock's contents into lines of runs.
// Ported from: itwinjs-core TextBlockLayoutResult (TextBlockLayoutResult.ts:65-71)
struct TextBlockLayoutResult {
    // The laid out lines.
    std::vector<LineLayoutResult> lines;
    // The range containing the TextBlock contents.
    dqGeom::Range2d range;
};

END_DQ_COMMON_NAMESPACE

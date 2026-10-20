// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — StrokeOptions (tessellation quality control)
//
// Ported from: itwinjs-core core/geometry/src/curve/StrokeOptions.ts
//
// Controls how curves and surfaces are tessellated into polylines and meshes.
// Three core tolerances (chord, angle, maxEdgeLength) each independently
// demand a minimum stroke count; the maximum wins.
#pragma once

#include "Angle.h"
#include "DqGeom.h"
#include "Geometry.h"  // stepCount/conditionalDivideFraction（M-W W-b2 增段）

#include <algorithm>
#include <cmath>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// StrokeOptions — tessellation parameters
// ---------------------------------------------------------------------------
struct DQ_GEOM_EXPORT StrokeOptions {
    // --- Quality tolerances (0 = inactive) ---
    double chordTol = 0.0;             // max distance from stroke to geometry
    double angleTolRadians = 0.0;      // turning angle between consecutive strokes
    double maxEdgeLength = 0.0;        // max length of a single stroke edge

    // --- Minimum strokes ---
    int minStrokesPerPrimitive = 0;    // minimum strokes per curve

    // --- Default stroke counts ---
    int defaultCircleStrokes = 16;     // default strokes for a full circle

    // --- Output flags ---
    bool shouldTriangulate = false;    // triangulate quads into triangles
    bool needNormals = false;          // request normals
    bool needParams = false;           // request UV parameters
    bool needColors = false;           // request per-vertex colors
    bool needTwoSided = true;          // facets viewable from both sides

    // --- Factories ---
    static StrokeOptions CreateForCurves()
    {
        StrokeOptions opts;
        opts.angleTolRadians = Angle::DegreesToRadians(15.0);
        return opts;
    }

    static StrokeOptions CreateForFacets()
    {
        StrokeOptions opts;
        opts.angleTolRadians = Angle::DegreesToRadians(22.5);
        return opts;
    }

    // --- Compute stroke count for an arc ---
    // Ported from: itwinjs StrokeOptions.applyTolerancesToArc
    // Returns the maximum stroke count demanded by all active tolerances.
    int ApplyTolerancesToArc(double radius, double sweepRadians) const
    {
        double absSweep = std::abs(sweepRadians);
        if (absSweep < Angle::kSmallAngleRadians) return 1;

        int count = 0;

        // Angle tolerance
        if (angleTolRadians > Angle::kSmallAngleRadians) {
            int angleCount = static_cast<int>(std::ceil(absSweep / angleTolRadians));
            count = std::max(count, angleCount);
        }

        // Chord tolerance: chord ≈ radius * (1 - cos(halfAngle))
        // For small angles: chord ≈ radius * angle^2 / 8
        if (chordTol > 0.0 && radius > 0.0) {
            // Solve: chordTol = radius * (1 - cos(sweep/(2*n)))
            // Approximate: n = sweep / (2 * acos(1 - chordTol/radius))
            double ratio = chordTol / radius;
            if (ratio < 1.0) {
                double halfAngle = std::acos(1.0 - ratio);
                if (halfAngle > Angle::kSmallAngleRadians) {
                    int chordCount = static_cast<int>(std::ceil(absSweep / (2.0 * halfAngle)));
                    count = std::max(count, chordCount);
                }
            }
        }

        // Max edge length
        if (maxEdgeLength > 0.0) {
            double arcLength = radius * absSweep;
            int edgeCount = static_cast<int>(std::ceil(arcLength / maxEdgeLength));
            count = std::max(count, edgeCount);
        }

        // Minimum strokes
        if (minStrokesPerPrimitive > 0) {
            count = std::max(count, minStrokesPerPrimitive);
        }

        // Default circle strokes (for full circles)
        if (Angle::IsFullCircleRadians(sweepRadians)) {
            count = std::max(count, defaultCircleStrokes);
        }

        return std::max(count, 1);
    }

    // --- Compute stroke count for a line (always 1) ---
    int ApplyToLine() const
    {
        return std::max(minStrokesPerPrimitive, 1);
    }

    // --- Ported from: StrokeOptions.applyMaxEdgeLength (member, :132-138) ---
    // maxEdgeLength 未激活（≤0）时原样返回 minCount。
    // M-W W-b2 订正：归位 Geometry.stepCount 调用（参考同调——stepCount 默认
    // maxCount=101 帽，此前内联 floor 缺帽）。
    int ApplyMaxEdgeLength(int minCount, double totalLength) const
    {
        totalLength = std::fabs(totalLength);
        if (maxEdgeLength > 0.0 && minCount * maxEdgeLength < totalLength) {
            minCount = stepCount(maxEdgeLength, totalLength, minCount);
        }
        return minCount;
    }

    // --- M-W W-b2 增段（BezierCurveBase::computeStrokeCountForOptions 消费面） ---

    // Ported from: StrokeOptions.applyChordTol（member，StrokeOptions.ts:193-200——
    // chordTol 激活且 < radius 时按 2·acos(1−a/radius) 步长取 stepCount）。
    int ApplyChordTol(int minCount, double radius, double sweepRadians) const
    {
        if (chordTol > 0.0 && chordTol < radius) {
            double const a = chordTol;
            double const stepRadians = 2.0 * std::acos(1.0 - a / radius);
            minCount = stepCount(stepRadians, sweepRadians, minCount);
        }
        return minCount;
    }

    // Ported from: StrokeOptions.applyChordTolToLengthAndRadians（member，:205-212
    // ——radius = length/sweepRadians 条件除）。
    int ApplyChordTolToLengthAndRadians(int minCount, double length, double sweepRadians) const
    {
        if (chordTol > 0.0) {
            auto const radius = conditionalDivideFraction(length, sweepRadians);
            if (radius.has_value())
                return ApplyChordTol(minCount, *radius, sweepRadians);
        }
        return minCount;
    }

    // Ported from: StrokeOptions.applyAngleTol（**static**，:151-161——options 可缺席
    // 直承；defaultStepRadians 未给时 π/8）。
    static int ApplyAngleTol(StrokeOptions const* options, int minCount, double sweepRadians,
                             double defaultStepRadians)
    {
        sweepRadians = std::fabs(sweepRadians);
        double stepRadians = defaultStepRadians != 0.0 ? defaultStepRadians : 3.14159265358979323846 / 8.0;
        if (options != nullptr && options->angleTolRadians > 0.0)
            stepRadians = options->angleTolRadians;
        if (minCount * stepRadians < sweepRadians)
            minCount = stepCount(stepRadians, sweepRadians, minCount);
        return minCount;
    }

    // Ported from: StrokeOptions.applyMaxEdgeLength（**static**，:168-179——options
    // 可缺席直承；minCount<1 抬升 1 的原样）。
    static int ApplyMaxEdgeLength(StrokeOptions const* options, int minCount, double edgeLength)
    {
        if (edgeLength < 0.0)
            edgeLength = -edgeLength;
        if (minCount < 1)
            minCount = 1;
        if (options != nullptr && options->maxEdgeLength > 0.0
            && options->maxEdgeLength * minCount < edgeLength) {
            minCount = stepCount(options->maxEdgeLength, edgeLength, minCount);
        }
        return minCount;
    }
};

END_DQ_GEOM_NAMESPACE

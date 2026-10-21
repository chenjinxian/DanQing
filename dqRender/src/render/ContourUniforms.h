// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Contour display uniforms
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ContourUniforms.ts
//
// Maintains state for uniforms related to contour display.
// Packs contour definitions (colors, patterns, widths, intervals) into float arrays
// for GPU upload.
//
// NOTE: The reference uses SyncTarget/SyncObserver for change detection.
// The current dqRender uniform pattern does not use sync (uniforms are uploaded
// every frame). The sync optimization can be added later when UniformHandle
// gains SyncObserver support.
#pragma once

#include "UniformHandle.h"
#include "LineCode.h"

#include <dqCommon/ContourDisplay.h>
#include <dqCommon/RgbColor.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Forward declaration
class TargetImpl;

// ---------------------------------------------------------------------------
// ContourUniforms — contour display uniform handler
// Ported from: itwinjs-core ContourUniforms.ts
//
// Packs contour definitions into float arrays for GPU upload.
// Uses 1.5x ContourDisplay.MaxContourGroups of indexable vec4 uniforms,
// also limited to 14 by the feature lookup texture packing scheme.
// ---------------------------------------------------------------------------
class ContourUniforms {
public:
    // We use 1.5x ContourDisplay.MaxContourGroups of indexable vec4 uniforms,
    // also limited to 14 by the feature lookup texture packing scheme.
    static constexpr size_t kContourDefsSize = static_cast<size_t>(
        (dqCommon::ContourDisplay::MaxContourGroups * 3 + 1) / 2);  // ceil(MaxContourGroups * 1.5)

    ContourUniforms() = default;

    /// Get the current contour display settings（值存管副本——参考
    /// _contourDisplay 为 TS 引用[GC 存续]；DanQing 栈顶 BranchState 的
    /// optional 值会在栈 push 重定位时搬迁，指针存管=悬空，故拷贝值
    /// 自持。失效比对经 equals[值]——参考 :58-61 同语义）。
    dqCommon::ContourDisplay const* getContourDisplay() const noexcept
    {
        return m_contourDisplay.has_value() ? &*m_contourDisplay : nullptr;
    }

    /// Update contour uniforms from target state.
    /// Ported from: itwinjs-core ContourUniforms.update()
    void update(TargetImpl& target);

    /// Bind contour definitions uniform.
    /// Ported from: itwinjs-core ContourUniforms.bindcontourDefs()
    ///（ContourUniforms.ts:103-106——`uniform.setUniform4fv(this._contourDefs)`
    ///   全数组上传；参考 UniformHandle.setUniform4fv 按数组长推导 count=
    ///   32/4=8——DanQing 显式传 kContourDefsSize[默认 count=1 只会上传首
    ///   vec4——X-a3 修]）。
    void bindcontourDefs(UniformHandle& uniform) const
    {
        uniform.setUniform4fv(m_contourDefs.data(), kContourDefsSize);
    }

private:
    /// pack two colors (major in upper byte, minor in lower byte) into float.
    /// Ported from: itwinjs-core ContourUniforms.packColor()
    void packColor(size_t startNdx, dqCommon::RgbColor const& majorColor, dqCommon::RgbColor const& minorColor)
    {
        // pack 2 bytes major (upper) minor (lower) into each float
        m_contourDefs[startNdx] = static_cast<float>(majorColor.r * 256 + minorColor.r);
        m_contourDefs[startNdx + 1] = static_cast<float>(majorColor.g * 256 + minorColor.g);
        m_contourDefs[startNdx + 2] = static_cast<float>(majorColor.b * 256 + minorColor.b);
    }

    /// pack pattern, width, and showGeometry flag into float.
    /// Ported from: itwinjs-core ContourUniforms.packPatWidth()
    void packPatWidth(size_t startNdx, int majorPattern, int minorPattern,
                      double majorWidth, double minorWidth, bool showGeometry)
    {
        // pack 2 bytes into this float, which is 4th float of vec4
        //   width is a 4-bit value that is biased by 1.0 and has 3-bits value with one fraction bit, so range is 1.0 to 8.5
        //   pattern is a line code index 0 to 10 (0 is solid)
        //   pack major into upper byte (upper nibble -> pattern, lower nibble -> 4-bit encoded width)
        //   pack minor into lower byte (upper nibble -> pattern, lower nibble -> 4-bit encoded width)
        // NB: showGeometry flag is packed into bit 16 (above major pattern)
        int majWt = static_cast<int>(std::floor((std::min(8.5, std::max(1.0, majorWidth)) - 1.0) * 2 + 0.5));
        int minWt = static_cast<int>(std::floor((std::min(8.5, std::max(1.0, minorWidth)) - 1.0) * 2 + 0.5));
        m_contourDefs[startNdx + 3] = static_cast<float>(
            (showGeometry ? 65536 : 0) + majorPattern * 4096 + majWt * 256 + minorPattern * 16 + minWt);
    }

    /// pack minor interval and major interval count into float.
    /// Ported from: itwinjs-core ContourUniforms.packIntervals()
    void packIntervals(size_t startNdx, bool even, double minorInterval, int majorIntervalCount)
    {
        // minorInterval is a float of interval in meters, majorIntervalCount is an int > 0 count of minor intervals per major interval
        // minorInterval is stored in r or b (0 or 2) and majorIntervalCount is stored in g or a (1 or 3) depending on even or odd index
        size_t offset = (even ? 0 : 1) * 2;
        m_contourDefs[startNdx + offset] = minorInterval <= 0.0 ? 1.0f : static_cast<float>(minorInterval);
        majorIntervalCount = static_cast<int>(std::floor(majorIntervalCount + 0.5));
        m_contourDefs[startNdx + offset + 1] = majorIntervalCount < 1 ? 1.0f : static_cast<float>(majorIntervalCount);
    }

    std::array<float, kContourDefsSize * 4> m_contourDefs{};
    // 参考 _contourDisplay（ContourUniforms.ts:22）——TS 引用 → 值存管
    //（getContourDisplay 注）。nullopt = 参考 undefined。
    std::optional<dqCommon::ContourDisplay> m_contourDisplay;
};

END_DQ_RENDER_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Contour display uniforms implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ContourUniforms.ts
#include "ContourUniforms.h"
#include "TargetImpl.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// update — update contour uniforms from target state
// Ported from: itwinjs-core ContourUniforms.update()
// ---------------------------------------------------------------------------
void ContourUniforms::update(TargetImpl& target)
{
    auto const* currentContours = target.getCurrentContours();

    // Early out if contour display hasn't changed
    if (m_contourDisplay && currentContours && m_contourDisplay == currentContours) {
        return;
    }

    m_contourDisplay = currentContours;

    if (!m_contourDisplay)
        return;

    /* uniform packing for contourDefs:
        The line pattern code is put into 4 bits, and the width is packed into 4 bits, so together with the pattern use 8 bits.
        This and the color bytes are then packed 2 bytes per float component, major in upper, as a float (e.g.: majorByte * 256 + minorByte)
        The minorInterval and majorCount each take a full float component, so they are combined with a second entry to use a full vec4
          Because of this, the overall indexing for a given contourDef is a bit different
        E.g.: the first 2 contour definitions (if both used) are packed into the first 3 vec4 uniform indexes like so:
          0.r = majCol[0].r << 8 | minCol[0].r  (0 to 65535 as float)
          0.g = majCol[0].g << 8 | minCol[0].g  (0 to 65535 as float)
          0.b = majCol[0].b << 8 | minCol[0].b  (0 to 65535 as float)
          0.a = (majPat[0] << 12 | majW[0] << 8) | (minPat[0] << 4 | minW[0])  (0 to 65535 as float)
          1.r = minorInterval[0]  (as float)
          1.g = majorCount[0]  (int, as float)
          1.b = minorInterval[1]  (as float)
          1.a = majorCount[1]  (int, as float)
          2.r = majCol[1].r << 8 | minCol[1].r  (0 to 65535 as float)
          2.g = majCol[1].g << 8 | minCol[1].g  (0 to 65535 as float)
          2.b = majCol[1].b << 8 | minCol[1].b  (0 to 65535 as float)
          2.a = (majPat[1] << 12 | majW[1] << 8) | (minPat[1] << 4 | minW[1])  (0 to 65535 as float)
        Then this usage pattern repeats the same way with every 2 contour definitions used taking 3 vec4 uniforms.
           (If just 1 contour def remains then it takes 2 vec4 uniforms, of which 1.5 is actually used.)
    */

    auto const& groups = m_contourDisplay->groups;
    size_t len = std::min(groups.size(), static_cast<size_t>(dqCommon::ContourDisplay::MaxContourGroups));

    for (size_t index = 0; index < len; ++index) {
        auto const& contourDef = groups[index].contourDef;
        bool even = (index & 1) == 0;
        size_t colorDefsNdx = static_cast<size_t>((even ? index * 1.5 : (index - 1) * 1.5 + 2) * 4);

        packColor(colorDefsNdx, contourDef.majorStyle.color, contourDef.minorStyle.color);

        int majorPattern = LineCode::valueFromLinePixels(contourDef.majorStyle.pattern);
        int minorPattern = LineCode::valueFromLinePixels(contourDef.minorStyle.pattern);

        packPatWidth(colorDefsNdx, majorPattern, minorPattern,
                     contourDef.majorStyle.pixelWidth, contourDef.minorStyle.pixelWidth,
                     contourDef.showGeometry);

        size_t intervalsPairNdx = static_cast<size_t>((std::floor(index * 0.5) * 3 + 1) * 4);
        packIntervals(intervalsPairNdx, even, contourDef.minorInterval, contourDef.majorIntervalCount);
    }
}

END_DQ_RENDER_NAMESPACE

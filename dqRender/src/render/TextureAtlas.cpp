// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Texture atlas implementation
// Ported from: itwinjs-core core/frontend/src/render/TextureAtlas.ts
//
// Skyline Bottom-Left bin-packing algorithm.
#include "TextureAtlas.h"

#include <algorithm>
#include <limits>

BEGIN_DQ_RENDER_NAMESPACE

void TextureAtlas::init(uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;
    m_skyline.clear();
    m_rects.clear();

    // Initial skyline: one node spanning the full width at y=0
    m_skyline.push_back({0, 0, width});
}

AtlasRect TextureAtlas::addRect(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0 || width > m_width || height > m_height) {
        return {0, 0, 0, 0};
    }

    uint32_t bestX = 0;
    uint32_t bestY = 0;
    if (!findPosition(width, height, bestX, bestY)) {
        return {0, 0, 0, 0};  // Doesn't fit
    }

    addSkylineNode(bestX, bestY, width, height);

    AtlasRect rect = {bestX, bestY, width, height};
    m_rects.push_back(rect);
    return rect;
}

bool TextureAtlas::findPosition(uint32_t width, uint32_t height, uint32_t& outX, uint32_t& outY)
{
    uint32_t bestY = std::numeric_limits<uint32_t>::max();
    uint32_t bestX = 0;
    bool found = false;

    for (size_t i = 0; i < m_skyline.size(); ++i) {
        uint32_t x = m_skyline[i].x;
        uint32_t y = m_skyline[i].y;

        // Check if the rect fits at this skyline position
        if (x + width > m_width) continue;

        // Find the maximum y across all skyline nodes that this rect would overlap
        uint32_t maxY = y;
        uint32_t covered = 0;
        size_t j = i;
        while (covered < width && j < m_skyline.size()) {
            maxY = std::max(maxY, m_skyline[j].y);
            covered += m_skyline[j].width;
            ++j;
        }

        // Check if we covered the full width
        if (covered < width) continue;

        // Check if the rect fits vertically
        if (maxY + height > m_height) continue;

        // Prefer lower y, then lower x (bottom-left heuristic)
        if (maxY < bestY || (maxY == bestY && x < bestX)) {
            bestY = maxY;
            bestX = x;
            found = true;
        }
    }

    if (found) {
        outX = bestX;
        outY = bestY;
    }
    return found;
}

void TextureAtlas::addSkylineNode(uint32_t x, uint32_t /*y*/, uint32_t width, uint32_t height)
{
    // The new skyline segment is at the top of the placed rect
    // We need to update the skyline to reflect that this region is now occupied
    // up to y + height

    // Find the skyline node that contains position x
    size_t insertIdx = 0;
    for (size_t i = 0; i < m_skyline.size(); ++i) {
        if (m_skyline[i].x + m_skyline[i].width > x) {
            insertIdx = i;
            break;
        }
        insertIdx = i + 1;
    }

    // Calculate the y value for the new skyline segment
    // It should be the y of the skyline at position x + height of the placed rect
    uint32_t baseY = (insertIdx < m_skyline.size()) ? m_skyline[insertIdx].y : 0;
    uint32_t newSegmentY = baseY + height;

    // Split the skyline at x and x + width
    std::vector<SkylineNode> newSkyline;

    // add nodes before the insertion region
    for (size_t i = 0; i < insertIdx; ++i) {
        newSkyline.push_back(m_skyline[i]);
    }

    // add left split if needed
    if (insertIdx < m_skyline.size() && m_skyline[insertIdx].x < x) {
        newSkyline.push_back({m_skyline[insertIdx].x, m_skyline[insertIdx].y,
                              x - m_skyline[insertIdx].x});
    }

    // add the new segment
    newSkyline.push_back({x, newSegmentY, width});

    // Skip any nodes that are completely covered by the new segment
    size_t skipIdx = insertIdx;
    while (skipIdx < m_skyline.size() &&
           m_skyline[skipIdx].x + m_skyline[skipIdx].width <= x + width) {
        ++skipIdx;
    }

    // add right split if needed
    if (skipIdx < m_skyline.size() && m_skyline[skipIdx].x < x + width) {
        uint32_t rightX = x + width;
        uint32_t rightWidth = m_skyline[skipIdx].x + m_skyline[skipIdx].width - rightX;
        if (rightWidth > 0) {
            newSkyline.push_back({rightX, m_skyline[skipIdx].y, rightWidth});
        }
        ++skipIdx;
    }

    // add remaining nodes
    for (size_t i = skipIdx; i < m_skyline.size(); ++i) {
        newSkyline.push_back(m_skyline[i]);
    }

    m_skyline = std::move(newSkyline);
}

bool TextureAtlas::uploadToGpu(rhi::Driver& driver, uint8_t const* data)
{
    if (m_width == 0 || m_height == 0 || !data) return false;

    if (!m_texture) {
        m_texture = driver.createTexture(
            rhi::SamplerType::SAMPLER_2D, 1, rhi::TextureFormat::RGBA8,
            m_width, m_height, 1, rhi::TextureUsage::SAMPLEABLE);
    }

    if (!m_texture) return false;

    rhi::PixelBufferDescriptor pbd(data, m_width * m_height * 4, 0, 0);
    driver.setTextureData(m_texture, 0, 0, 0, 0, m_width, m_height, 1, std::move(pbd));
    return true;
}

END_DQ_RENDER_NAMESPACE

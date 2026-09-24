// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Feature index and color index
//
// Ported from: itwinjs-core core/common/src/FeatureIndex.ts
// Describes features and colors associated with mesh/polyline vertices.
#pragma once

#include "ColorDef.h"
#include "Export.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Describes the type of a FeatureIndex.
// Ported from: itwinjs-core FeatureIndexType
enum class FeatureIndexType : uint8_t {
    Empty = 0,
    Uniform = 1,
    NonUniform = 2,
};

// Describes the set of Features associated with a mesh or polyline.
// Ported from: itwinjs-core core/common/src/FeatureIndex.ts
class DQ_COMMON_EXPORT FeatureIndex {
public:
    FeatureIndexType type = FeatureIndexType::Empty;
    uint32_t featureID = 0;
    std::vector<uint32_t> featureIDs;

    // True if type is Uniform.
    // Ported from: itwinjs-core FeatureIndex.isUniform
    bool isUniform() const noexcept { return type == FeatureIndexType::Uniform; }

    // True if type is Empty.
    // Ported from: itwinjs-core FeatureIndex.isEmpty
    bool isEmpty() const noexcept { return type == FeatureIndexType::Empty; }

    // Reset to empty.
    // Ported from: itwinjs-core FeatureIndex.reset()
    void reset() noexcept
    {
        type = FeatureIndexType::Empty;
        featureID = 0;
        featureIDs.clear();
    }
};

// Per-vertex colors for a mesh or polyline.
// Ported from: itwinjs-core NonUniformColor
class DQ_COMMON_EXPORT NonUniformColor {
public:
    std::vector<uint32_t> colors;   // ColorDef TBGR values
    std::vector<uint16_t> indices;  // Per-vertex index into colors
    bool isOpaque = true;

    NonUniformColor() noexcept = default;
    NonUniformColor(const std::vector<uint32_t>& colorTable, const std::vector<uint16_t>& vertexIndices,
                    bool hasAlpha)
        : colors(colorTable), indices(vertexIndices), isOpaque(!hasAlpha)
    {
    }
};

// Describes the color(s) of mesh/polyline vertices.
// Ported from: itwinjs-core core/common/src/FeatureIndex.ts
class DQ_COMMON_EXPORT ColorIndex {
public:
    // Whether the colors have transparency.
    // Ported from: itwinjs-core ColorIndex.hasAlpha
    bool hasAlpha() const noexcept { return !m_isOpaque; }

    // Whether this index specifies a single uniform color.
    // Ported from: itwinjs-core ColorIndex.isUniform
    bool isUniform() const noexcept { return m_isUniform; }

    // Number of colors.
    // Ported from: itwinjs-core ColorIndex.numColors
    int numColors() const noexcept { return m_isUniform ? 1 : static_cast<int>(m_nonUniform.colors.size()); }

    // Constructor — default to uniform white.
    ColorIndex() noexcept = default;

    // Reset to uniform white.
    // Ported from: itwinjs-core ColorIndex.reset()
    void reset() noexcept
    {
        m_isUniform = true;
        m_isOpaque = true;
        m_uniformColor = ColorDef::white.getTbgr();
        m_nonUniform = NonUniformColor{};
    }

    // Get uniform color (if uniform).
    // Ported from: itwinjs-core ColorIndex.uniform
    std::optional<ColorDef> getUniform() const noexcept
    {
        if (m_isUniform)
            return ColorDef::fromTbgr(m_uniformColor);
        return std::nullopt;
    }

    // Set uniform color.
    // Ported from: itwinjs-core ColorIndex.initUniform()
    void initUniform(uint32_t colorTbgr) noexcept
    {
        m_isUniform = true;
        m_uniformColor = colorTbgr;
        m_isOpaque = ColorDef::isOpaque(colorTbgr);
    }
    void initUniform(const ColorDef& color) noexcept { initUniform(color.getTbgr()); }

    // Get non-uniform colors (if non-uniform).
    // Ported from: itwinjs-core ColorIndex.nonUniform
    const NonUniformColor* getNonUniform() const noexcept
    {
        return m_isUniform ? nullptr : &m_nonUniform;
    }

    // Set non-uniform colors.
    // Ported from: itwinjs-core ColorIndex.initNonUniform()
    void initNonUniform(const std::vector<uint32_t>& colors, const std::vector<uint16_t>& indices,
                        bool hasAlpha)
    {
        m_isUniform = false;
        m_nonUniform = NonUniformColor(colors, indices, hasAlpha);
        m_isOpaque = !hasAlpha;
    }

private:
    bool m_isUniform = true;
    bool m_isOpaque = true;
    uint32_t m_uniformColor = ColorDef::white.getTbgr();
    NonUniformColor m_nonUniform;
};

END_DQ_COMMON_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Render texture (abstract base)
// Ported from: itwinjs-core core/common/src/RenderTexture.ts
//
// Abstract base for texture images used in rendering.
// Concrete implementations live in dqRender.
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <dqBase/RefCounted.h>

#include <cstdint>
#include <string>

BEGIN_DQ_COMMON_NAMESPACE

// Abstract base class for render textures.
// Ported from: itwinjs-core RenderTexture
class DQ_COMMON_EXPORT RenderTexture : public dqBase::RefCounted<RenderTexture> {
public:
    // Texture type.
    // Ported from: itwinjs-core RenderTexture.Type
    enum class Type : uint8_t {
        Normal = 0,
        Glyph = 1,
        TileSection = 2,
        SkyBox = 3,
        FilteredTileSection = 4,
        ThematicGradient = 5,
    };

    virtual ~RenderTexture() = default;

    // The type of this texture.
    Type type;

    // True if this is a tile section texture.
    bool isTileSection() const noexcept { return type == Type::TileSection || type == Type::FilteredTileSection; }

    // True if this is a glyph texture.
    bool isGlyph() const noexcept { return type == Type::Glyph; }

    // True if this is a sky box texture.
    bool isSkyBox() const noexcept { return type == Type::SkyBox; }

    // Memory used by this texture (bytes). override in subclasses.
    virtual int getBytesUsed() const noexcept { return 0; }

    // Ordered comparison.
    int compare(const RenderTexture& rhs) const noexcept
    {
        if (type != rhs.type) return static_cast<int>(type) < static_cast<int>(rhs.type) ? -1 : 1;
        return 0;
    }

protected:
    explicit RenderTexture(Type t = Type::Normal) : type(t) {}
};

// Texture image specification (element ID or URL).
// Ported from: itwinjs-core TextureImageSpec
using TextureImageSpec = std::string;

END_DQ_COMMON_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Render material (abstract base)
// Ported from: itwinjs-core core/common/src/RenderMaterial.ts
//
// Abstract base for materials applied to surfaces.
// Concrete implementations live in dqRender.
#pragma once

#include "Export.h"
#include "TextureMapping.h"
#include "DqCommon.h"

#include <dqBase/RefCounted.h>

#include <cstdint>
#include <optional>
#include <string>

BEGIN_DQ_COMMON_NAMESPACE

// Abstract base class for render materials.
// Ported from: itwinjs-core RenderMaterial
class DQ_COMMON_EXPORT RenderMaterial : public dqBase::RefCounted<RenderMaterial> {
public:
    virtual ~RenderMaterial() = default;

    // Optional key identifying this material.
    std::string key;

    // Optional texture mapping.
    std::optional<TextureMapping> textureMapping;

    // True if this material has an associated texture.
    bool hasTexture() const noexcept { return textureMapping.has_value(); }

    // Ordered comparison.
    int compare(const RenderMaterial& rhs) const noexcept;

protected:
    RenderMaterial() = default;
    RenderMaterial(std::string k, std::optional<TextureMapping> tm = std::nullopt)
        : key(std::move(k)), textureMapping(std::move(tm)) {}
};

inline int RenderMaterial::compare(const RenderMaterial& rhs) const noexcept
{
    if (key != rhs.key) return key < rhs.key ? -1 : 1;
    const bool hasTex = textureMapping.has_value();
    const bool rhsHasTex = rhs.textureMapping.has_value();
    if (hasTex != rhsHasTex) return hasTex ? 1 : -1;
    return 0;
}

END_DQ_COMMON_NAMESPACE

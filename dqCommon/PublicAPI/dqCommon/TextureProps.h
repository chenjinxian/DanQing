// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Texture properties & transparency
// Ported from: itwinjs-core core/common/src/TextureProps.ts
//
// This header ports the TextureTransparency enum from TextureProps.ts.
// The sibling interfaces (TextureProps, TextureLoadProps, TextureData) are not yet
// ported: TextureProps extends DefinitionElementProps, which depends on the Element
// backend model (data/platform layer, out of engine scope) not yet ported — TODO per §11 (no half-impl).
#pragma once

#include "DqCommon.h"

#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// Describes the type of transparency in the pixels of a TextureImage.
// Each pixel can be classified as either opaque or translucent. The transparency of
// the image as a whole is based on the combination of pixel transparencies. If this
// information is known, it should be supplied when creating a texture for more
// efficient rendering.
// Ported from: itwinjs-core TextureTransparency (core/common/src/TextureProps.ts)
enum class TextureTransparency : uint8_t {
    // The image contains only opaque pixels. It should not blend with other objects.
    Opaque = 0,
    // The image contains only translucent pixels. It should blend with other objects.
    Translucent = 1,
    // The image contains both opaque and translucent pixels. More expensive to render.
    Mixed = 2,
};

END_DQ_COMMON_NAMESPACE

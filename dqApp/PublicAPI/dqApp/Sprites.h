// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Sprite + SpriteLocation (canvas sprite decorations)
// Ported from: itwinjs-core core/frontend/src/Sprites.ts
//              Sprite (:33-59) + SpriteLocation (:92-145)
//
// The reference loads PNG sprite images as HTMLImageElement and blits them via
// ctx.drawImage; DanQing decodes the same PNGs (dqCommon::DecodeImage — stb) into
// an rhi texture (the same path as glTF baseColor textures) and blits via
// CanvasContext::drawImage. Same images, same center-offset placement.
#pragma once

#include "Export.h"

#include <dqRender/CanvasDecoration.h>
#include <dqRender/rhi/Handle.h>

#include <string>

namespace dqApp {

class Viewport;
class DecorateContext;

// ---------------------------------------------------------------------------
// Sprite — a loaded image for canvas decoration
// Ported from: itwinjs-core Sprites.ts:33-59
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT Sprite {
public:
    Sprite() = default;

    // Load the sprite image from a PNG file, uploading it as a texture on the
    // viewport's driver (RenderSystem.createTexture — same routing as glTF
    // baseColor). ← Sprites.ts:48-58 (constructor: imageElementFromUrl → loaded
    // image + naturalWidth/Height). A failed load leaves isLoaded() false and
    // drawImage suppressed (Sprites.ts:127-129 undefined image → return).
    bool loadFromFile(std::string const& filePath, Viewport& vp);

    bool isLoaded() const noexcept { return m_texture != dqRender::rhi::TextureHandle{}; }

    dqRender::rhi::TextureHandle texture() const noexcept { return m_texture; }

    // Natural image size. ← Sprites.ts:52 (image.naturalWidth/Height)
    uint32_t width() const noexcept { return m_width; }
    uint32_t height() const noexcept { return m_height; }

    // Offset to the middle of the sprite. ← Sprites.ts:41
    // (Math.round(size) / 2 — drawImage is called with -offset so the sprite
    // centers on the CanvasDecoration position).
    double offsetX() const noexcept { return static_cast<double>(m_width) / 2.0; }
    double offsetY() const noexcept { return static_cast<double>(m_height) / 2.0; }

private:
    dqRender::rhi::TextureHandle m_texture;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

// ---------------------------------------------------------------------------
// SpriteLocation — a Sprite shown at a view-coordinate location in one viewport
// Ported from: itwinjs-core Sprites.ts:92-145
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT SpriteLocation {
public:
    // Activate this SpriteLocation to show a Sprite in a single viewport.
    // ← Sprites.ts:110-119 (activate: store sprite/viewport, convert the world
    //   location to view coordinates). DanQing takes the view-coordinate location
    //   directly (the hover locate chain already works in view pixels);
    //   invalidateDecorations is issued so the sprite shows on the next frame.
    void activate(Sprite const& sprite, Viewport& vp, double viewX, double viewY);

    // Turn this SpriteLocation off. ← Sprites.ts:121-128 (deactivate)
    void deactivate();

    bool isActive() const noexcept { return m_viewport != nullptr; }

    // Add the sprite as a CanvasDecoration if the context is for this
    // SpriteLocation's viewport. ← Sprites.ts:131-145 (drawDecoration:
    // ctx.drawImage(sprite.image, -offset.x, -offset.y) after the position
    // translate; decorate: addCanvasDecoration when viewport matches).
    void decorate(DecorateContext& context) const;

private:
    Viewport* m_viewport = nullptr;
    dqRender::rhi::TextureHandle m_texture;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    double m_viewX = 0.0;
    double m_viewY = 0.0;
};

}  // namespace dqApp

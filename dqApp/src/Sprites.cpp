// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Sprite + SpriteLocation implementation
// Ported from: itwinjs-core core/frontend/src/Sprites.ts:33-59, 92-145
#include "dqApp/Sprites.h"

#include "dqApp/DecorateContext.h"
#include "dqApp/Viewport.h"

#include <dqCommon/Image.h>
#include <dqRender/CreateTextureArgs.h>

#include <cstdio>
#include <fstream>
#include <vector>

namespace dqApp {

bool Sprite::loadFromFile(std::string const& filePath, Viewport& vp)
{
    std::ifstream f(filePath, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        fprintf(stderr, "[SPRITE] failed to open %s\n", filePath.c_str());
        return false;
    }
    size_t const size = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(size);
    f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));

    auto decoded = dqCommon::DecodeImage({ bytes, dqCommon::ImageSourceFormat::Png });
    if (!decoded.has_value()) {
        fprintf(stderr, "[SPRITE] failed to decode %s\n", filePath.c_str());
        return false;
    }

    dqRender::CreateTextureArgs args{};
    m_width = decoded->width;             // natural size before the move
    m_height = decoded->getHeight();
    args.imageBuffer = std::move(*decoded);
    m_texture = vp.createTexture(args);
    if (m_texture == dqRender::rhi::TextureHandle{}) {
        fprintf(stderr, "[SPRITE] texture upload failed for %s\n", filePath.c_str());
        return false;
    }
    return true;
}

void SpriteLocation::activate(Sprite const& sprite, Viewport& vp, double viewX, double viewY)
{
    // ← Sprites.ts:110-119. An unloaded sprite never activates (the reference's
    // load-failure path clears the viewport association, :118).
    if (!sprite.isLoaded())
        return;
    m_texture = sprite.texture();
    m_width = sprite.width();
    m_height = sprite.height();
    m_viewport = &vp;
    m_viewX = viewX;
    m_viewY = viewY;
    vp.InvalidateDecorations();  // ← :116-117 (invalidateDecorations on load)
}

void SpriteLocation::deactivate()
{
    // ← Sprites.ts:121-128.
    if (!isActive())
        return;
    if (m_viewport)
        m_viewport->InvalidateDecorations();
    m_viewport = nullptr;
}

void SpriteLocation::decorate(DecorateContext& context) const
{
    // ← Sprites.ts:131-145.
    if (!isActive() || &context.GetViewport() != m_viewport)
        return;
    dqRender::CanvasDecoration dec;
    dec.position = dqGeom::Point2d{ m_viewX, m_viewY };
    dec.drawDecoration = [tex = m_texture, w = m_width, h = m_height](
                             dqRender::CanvasContext& ctx) {
        // ctx.drawImage(sprite.image, -sprite.offset.x, -sprite.offset.y)
        // — the position translate above centers the image on m_viewX/Y
        // (Sprites.ts:41 offset = round(size)/2).
        ctx.drawImage(tex, w, h, -static_cast<double>(w) / 2.0,
                      -static_cast<double>(h) / 2.0);
    };
    context.AddCanvasDecoration(std::move(dec));
}

}  // namespace dqApp

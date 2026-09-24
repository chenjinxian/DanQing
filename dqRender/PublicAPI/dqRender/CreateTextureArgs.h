// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — arguments for creating a RenderTexture.
//
// Ported from: itwinjs-core core/frontend/src/render/CreateTextureArgs.ts
//
// Dedicated header matching the itwinjs-core filename 1:1. The args struct
// was previously inlined in RenderMaterial.h; it is now declared here and
// re-exported from RenderMaterial.h for backward compatibility.
//
// Type-only port:
//   - CreateTextureArgs           (public)
//   - CreateTextureFromSourceArgs (public)
//   - TextureCacheOwnership       (public)
//   - TextureOwnership            (type alias)
//
// Note: itwinjs CreateTextureArgs.image is a TextureImage (compressed ImageSource
// or uncompressed ImageBuffer). DanQing faithfully models this as an ImageSource
// (data bytes + format) plus an optional uncompressed path; the discriminated
// union is flattened into a single struct with a format tag (§6 — no invention
// beyond what is required to express the type surface in C++).
#pragma once

#include "Export.h"
#include "RenderMaterial.h"  // RenderTexture::Type, TextureTransparency

#include <dqBase/DqId.h>
#include <dqCommon/Image.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#endif
#ifndef END_DQ_RENDER_NAMESPACE
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Ownership descriptor for a RenderTexture that is cached on an IModelConnection
// by a unique key. When the IModelConnection is closed, the texture is disposed.
// Ported from: itwinjs-core TextureCacheOwnership
//
// TODO: IModelConnection not yet ported — handle is opaque (§6: no half-impl).
struct DQ_RENDER_EXPORT TextureCacheOwnership {
    // The iModel on which the texture will be cached.
    void* iModel = nullptr;  // TODO: IModelConnection* once ported

    // The key uniquely identifying the texture amongst all textures cached on the iModel.
    std::string key;
};

// Describes the ownership of a RenderTexture, controlling when it is disposed.
// Ported from: itwinjs-core TextureOwnership
//
// - TextureCacheOwnership: disposed when the IModelConnection closes; lookups
//   by the same key always return the cached texture.
// - External: lifetime controlled externally (e.g. by a Decorator), which is
//   responsible for calling dispose when done.
//
// Discriminator: external flag. If external == true, ownership is "external"
// and cache fields are ignored. Otherwise cache fields are populated.
struct DQ_RENDER_EXPORT TextureOwnership {
    bool external = false;
    TextureCacheOwnership cache;
};

// Arguments supplied to RenderSystem.createTexture.
// Ported from: itwinjs-core CreateTextureArgs
struct DQ_RENDER_EXPORT CreateTextureArgs {
    // The type of texture to create. Default: RenderTexture.Type.normal.
    RenderTexture::Type type = RenderTexture::Type::Normal;

    // The image from which to create the texture (compressed bytes + format).
    std::vector<uint8_t> imageData;
    dqCommon::ImageSourceFormat format = dqCommon::ImageSourceFormat::Png;

    // Already-decoded RGBA8 image (spec §4.1 — decode once at load time).
    // When set, createTexture uploads these bytes directly; imageData/format
    // (compressed source) is the alternate path (TODO: decode-on-demand, 未实现).
    std::optional<dqCommon::ImageBuffer> imageBuffer;

    // The ownership of the texture. If unset, lifetime is controlled by the
    // first RenderGraphic with which it associates.
    std::optional<TextureOwnership> ownership;
};

// Arguments supplied to RenderSystem.createTextureFromSource.
// Ported from: itwinjs-core CreateTextureFromSourceArgs
struct DQ_RENDER_EXPORT CreateTextureFromSourceArgs {
    // The type of texture to create. Default: RenderTexture.Type.normal.
    RenderTexture::Type type = RenderTexture::Type::Normal;

    // The image source (compressed bytes + format).
    std::vector<uint8_t> sourceData;
    dqCommon::ImageSourceFormat sourceFormat = dqCommon::ImageSourceFormat::Png;

    // Describes the transparency of the image. Can improve performance if supplied.
    // Default for PNG: Mixed; for JPEG: Opaque.
    std::optional<TextureTransparency> transparency;

    // The ownership of the texture. If unset, lifetime is controlled by the
    // first RenderGraphic with which it associates.
    std::optional<TextureOwnership> ownership;
};

END_DQ_RENDER_NAMESPACE

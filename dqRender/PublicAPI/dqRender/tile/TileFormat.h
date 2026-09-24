// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — 3D Tiles format definitions
//
// Ported from: itwinjs-core core/common/src/tile/TileIO.ts
// Defines magic numbers and header structures for b3dm, i3dm, pnts, cmpt formats.
#pragma once

#include <cstdint>
#include <cstring>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Tile format magic numbers.
// Ported from: itwinjs-core TileFormat enum
enum class TileFormat : uint32_t {
    B3dm  = 0x6d643362,  // "b3dm" — Batched 3D Model
    I3dm  = 0x6d643369,  // "i3dm" — Instanced 3D Model
    Pnts  = 0x73746e70,  // "pnts" — point Cloud
    Cmpt  = 0x74706d63,  // "cmpt" — Composite
    Gltf  = 0x46546c67,  // "glTF" — Raw glTF
    IModel = 0x6c644d69, // "iMdl" — Bentley iModel
};

// b3dm header (28 bytes).
// Ported from: itwinjs-core B3dmHeader
struct B3dmHeader {
    uint32_t magic;               // Must be 0x6d643362 ("b3dm")
    uint32_t version;             // Must be 1
    uint32_t byteLength;          // Total length of the tile
    uint32_t featureTableJsonLength;
    uint32_t featureTableBinaryLength;
    uint32_t batchTableJsonLength;
    uint32_t batchTableBinaryLength;
};

// i3dm header (32 bytes).
// Ported from: itwinjs-core I3dmHeader
struct I3dmHeader {
    uint32_t magic;               // Must be 0x6d643369 ("i3dm")
    uint32_t version;             // Must be 1
    uint32_t byteLength;          // Total length of the tile
    uint32_t featureTableJsonLength;
    uint32_t featureTableBinaryLength;
    uint32_t batchTableJsonLength;
    uint32_t batchTableBinaryLength;
    uint8_t  gltfFormat;          // 0 = glb, 1 = glTF URI
};

// Detect tile format from the first 4 bytes of content.
// Ported from: itwinjs-core _getFormat()
inline TileFormat DetectTileFormat(uint8_t const* data, size_t size)
{
    if (size < 4)
        return TileFormat::Gltf;  // Assume raw glTF for small files
    uint32_t magic;
    std::memcpy(&magic, data, 4);
    return static_cast<TileFormat>(magic);
}

// Validate a b3dm header.
inline bool ValidateB3dmHeader(B3dmHeader const& header)
{
    return header.magic == static_cast<uint32_t>(TileFormat::B3dm)
        && header.version == 1
        && header.byteLength >= sizeof(B3dmHeader);
}

// Validate an i3dm header.
inline bool ValidateI3dmHeader(I3dmHeader const& header)
{
    return header.magic == static_cast<uint32_t>(TileFormat::I3dm)
        && header.version == 1
        && header.byteLength >= sizeof(I3dmHeader);
}

END_DQ_RENDER_NAMESPACE

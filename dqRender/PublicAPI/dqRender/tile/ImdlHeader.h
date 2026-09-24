// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — imdl binary format layer
// Ported from: itwinjs-core core/common/src/tile/TileIO.ts (ByteStream subset,
//              TileFormat, TileHeader)
//              core/common/src/tile/IModelTileIO.ts (ImdlFlags,
//              CurrentImdlVersion, ImdlHeader, FeatureTableHeader)
#pragma once

#include "../Export.h"
#include "TileFormat.h"

#include <dqGeom/Range3d.h>

#include <cstdint>
#include <cstring>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// ByteStream — little-endian cursor over a byte buffer.
// Ported from: itwinjs-core ByteStream (core-bentley — the read* subset the
// imdl headers use: readUint32/readFloat64/readPoint3d64/advance/isPastTheEnd).
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT ImdlByteStream {
public:
    ImdlByteStream(uint8_t const* data, size_t size)
        : m_data(data)
        , m_size(size)
    {
    }

    uint32_t readUint32()
    {
        uint32_t v = 0;
        readBytes(&v, 4);
        return v;
    }

    double readFloat64()
    {
        double v = 0;
        readBytes(&v, 8);
        return v;
    }

    // Ported from: TileIO.ts nextPoint3d64FromByteStream (3×float64 little-
    // endian, point in native alignment — DanQing packs as 3 doubles).
    void readPoint3d64(double out[3])
    {
        out[0] = readFloat64();
        out[1] = readFloat64();
        out[2] = readFloat64();
    }

    void readBytes(void* dst, size_t count)
    {
        if (m_pos + count > m_size) {
            m_pastEnd = true;
            m_pos = m_size;
            return;
        }
        std::memcpy(dst, m_data + m_pos, count);
        m_pos += count;
    }

    void advance(size_t count)
    {
        if (m_pos + count > m_size)
            m_pastEnd = true;
        m_pos = (m_pos + count > m_size) ? m_size : m_pos + count;
    }

    size_t curPos() const noexcept { return m_pos; }
    bool isPastTheEnd() const noexcept { return m_pastEnd || m_pos > m_size; }

    /// Direct view of the remaining bytes from the cursor (for bulk copies).
    uint8_t const* remainingData() const noexcept { return m_data + m_pos; }
    size_t remainingSize() const noexcept { return m_pos <= m_size ? m_size - m_pos : 0; }

private:
    uint8_t const* m_data;
    size_t m_size;
    size_t m_pos = 0;
    bool m_pastEnd = false;
};

// Flags describing the geometry contained within an iMdl tile.
// Ported from: itwinjs-core ImdlFlags (IModelTileIO.ts:17-28).
enum class ImdlFlags : uint32_t {
    None = 0,
    ContainsCurves = 1u << 0,
    Incomplete = 1u << 2,
    DisallowMagnification = 1u << 3,
    MultiModelFeatureTable = 1u << 4,
};

// Maximum iMdl format version this package reads.
// Ported from: itwinjs-core CurrentImdlVersion (IModelTileIO.ts:33-45).
struct CurrentImdlVersion {
    static constexpr uint16_t Major = 37;
    static constexpr uint16_t Minor = 0;
    static constexpr uint32_t Combined = (uint32_t(Major) << 0x10) | Minor;
};

// Header embedded at the beginning of binary iMdl tile data.
// Ported from: itwinjs-core ImdlHeader (IModelTileIO.ts:50-102 — field order
// format/version/headerLength/flags/contentRange(2×Point3d64)/tolerance/
// numElementsIncluded/numElementsExcluded/tileLength/emptySubRanges(v2+),
// then skip to headerLength).
struct DQ_RENDER_EXPORT ImdlHeader {
    uint32_t format = 0;      // TileFormat (TileHeader base)
    uint32_t version = 0;     // (major << 0x10) | minor
    uint32_t headerLength = 0;
    ImdlFlags flags = ImdlFlags::None;
    dqGeom::Range3d contentRange;  // null range if the tile is empty
    double tolerance = 0.0;
    uint32_t numElementsIncluded = 0;
    uint32_t numElementsExcluded = 0;
    uint32_t tileLength = 0;
    uint32_t emptySubRanges = 0;   // v2.00+

    uint16_t versionMajor() const noexcept { return static_cast<uint16_t>(version >> 0x10); }
    uint16_t versionMinor() const noexcept { return static_cast<uint16_t>(version & 0xffff); }
    bool isValid() const noexcept { return format == static_cast<uint32_t>(TileFormat::IModel); }
    bool isReadableVersion() const noexcept { return versionMajor() <= CurrentImdlVersion::Major; }

    // Deserialize from the stream's current position; marks itself invalid
    // (format=0) on truncation, mirroring the reference's invalidate().
    static ImdlHeader readFrom(ImdlByteStream& stream);
};

// Header preceding the feature table embedded in an iMdl tile's content.
// Ported from: itwinjs-core FeatureTableHeader (IModelTileIO.ts:107-130).
struct DQ_RENDER_EXPORT ImdlFeatureTableHeader {
    uint32_t length = 0;          // bytes the entire table occupies
    uint32_t numSubCategories = 0;
    uint32_t count = 0;           // number of features

    static constexpr size_t sizeInBytes = 12;

    // Ported from: FeatureTableHeader.readFrom (:116-121 — nullopt on
    // truncation expressed as the bool return).
    static bool readFrom(ImdlByteStream& stream, ImdlFeatureTableHeader& out);
};

END_DQ_RENDER_NAMESPACE

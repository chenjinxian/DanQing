// SPDX-License-Identifier: Apache-2.0
// Authored: regression test for the VertexLutTexture upload contract. No reference
// test exists in itwinjs-core / imodel-native for this RHI-level upload (itwinjs
// uses WebGL-internal texImage2D which does its own bounds checking); the itwinjs
// VertexLUT.ts computeDimensions also rounds dims, but WebGL's texImage2D semantics
// differ from glTexSubImage2D's strict width*height*4 read. This test drives
// VertexLutTexture::create through a recording rhi::Driver mock (no GL context).
//
// Contract under test: VertexLutTexture::create MUST upload to a texture whose dims
// are EXACTLY the caller-supplied (width, height) — the VertexTableBuilder's dims.
// The builder emits a transposed SoA layout keyed on width%numRgbaPerVert==0 (a
// vertex's texels never wrap rows), and the shader's samplePosition keys off
// u_vertParams.xy=(width,height). Recomputing pow-2 dims here corrupts that layout
// → the shader samples the wrong texels → garbage positions → 0 fragments (the ACS
// Polyline invisibility bug). data is exactly width*height*4 bytes, so the upload
// is exact (no overread, no zero-padding).
#include <gtest/gtest.h>

#include "NullDriver.h"

#include "rhi/DriverBase.h"
#include "rhi/HandleAllocator.h"
#include "render/VertexLutTexture.h"

#include <cstring>
#include <vector>

using namespace dqRender;
using namespace dqRender::rhi;

namespace {

// A recording driver: returns a real (non-null) TextureHandle from createTexture
// (so VertexLutTexture::create's null-check passes) and captures the
// PixelBufferDescriptor passed to setTextureData, snapshotting its bytes so the
// test can inspect the upload content after the call returns.
class RecordingDriver : public NullDriver {
public:
    TextureHandle createTexture(SamplerType, uint8_t, TextureFormat,
                                uint32_t, uint32_t, uint32_t, TextureUsage) noexcept override
    {
        return m_allocator.allocate<HwTexture>();
    }

    void setTextureData(TextureHandle, uint32_t, uint32_t, uint32_t, uint32_t,
                        uint32_t width, uint32_t height, uint32_t,
                        PixelBufferDescriptor&& data) noexcept override
    {
        m_lastWidth = width;
        m_lastHeight = height;
        m_lastSize = data.size();
        auto const* b = static_cast<uint8_t const*>(data.buffer());
        m_lastBytes.assign(b, b + data.size());
    }

    uint32_t lastWidth() const noexcept { return m_lastWidth; }
    uint32_t lastHeight() const noexcept { return m_lastHeight; }
    size_t lastSize() const noexcept { return m_lastSize; }
    std::vector<uint8_t> const& lastBytes() const noexcept { return m_lastBytes; }

private:
    HandleAllocator m_allocator;
    uint32_t m_lastWidth = 0;
    uint32_t m_lastHeight = 0;
    size_t m_lastSize = 0;
    std::vector<uint8_t> m_lastBytes;
};

}  // namespace

// Authored: no reference test exists in itwinjs-core/imodel-native for the RHI upload
// contract. VertexLutTexture::create must use the caller's (width, height) verbatim
// — NOT recompute pow-2 dims. 3 verts × 6 rgba/vert = 18 texels; the builder lays
// these out as width=6 (== numRgbaPerVert), height=3 (one row per vertex). The
// texture must be 6×3 and the upload exactly 72 bytes (the source size), with no
// zero-padding and no overread.
TEST(VertexLutTextureTest, CreateUsesBuilderDimsVerbatim)
{
    RecordingDriver driver;

    uint32_t const numVertices = 3;
    uint32_t const numRgbaPerVert = 6;
    uint32_t const width = numRgbaPerVert;       // 6 (builder: width % numRgbaPerVert == 0)
    uint32_t const height = numVertices;          // 3
    uint32_t const texBytes = width * height * 4u;  // 72

    std::vector<uint8_t> src(texBytes);
    for (uint32_t i = 0; i < texBytes; ++i) src[i] = static_cast<uint8_t>((i + 1) & 0xFF);

    VertexLutTexture lut;
    ASSERT_TRUE(lut.create(driver, src.data(), width, height, numVertices, numRgbaPerVert));

    // (a) Texture + upload use the builder's exact dims (NOT pow-2 8×3).
    EXPECT_EQ(driver.lastWidth(), width);
    EXPECT_EQ(driver.lastHeight(), height);
    EXPECT_EQ(driver.lastSize(), texBytes);

    // (b) Params reported faithfully.
    auto const& params = lut.getParams();
    EXPECT_EQ(params.texWidth, width);
    EXPECT_EQ(params.texHeight, height);
    EXPECT_EQ(params.numRgbaPerVert, numRgbaPerVert);
    EXPECT_EQ(params.numVertices, numVertices);

    // (c) Source bytes uploaded verbatim — no zero-padding, no overread.
    auto const& bytes = driver.lastBytes();
    ASSERT_EQ(bytes.size(), texBytes);
    EXPECT_EQ(std::memcmp(bytes.data(), src.data(), texBytes), 0);

    lut.destroy(driver);
}

// Authored: the layout is preserved for any builder (width, height), including a
// non-square, non-pow-2 packing the builder may choose (e.g. width=12, height=2
// for 6 verts × 4 rgba = 24 texels). The texture must match exactly.
TEST(VertexLutTextureTest, CreatePreservesNonPow2Dims)
{
    RecordingDriver driver;

    uint32_t const numVertices = 6;
    uint32_t const numRgbaPerVert = 4;
    uint32_t const width = 12;                    // builder-chosen (divisible by numRgbaPerVert)
    uint32_t const height = 2;
    uint32_t const texBytes = width * height * 4u;  // 96

    std::vector<uint8_t> src(texBytes, 0xAB);

    VertexLutTexture lut;
    ASSERT_TRUE(lut.create(driver, src.data(), width, height, numVertices, numRgbaPerVert));

    EXPECT_EQ(driver.lastWidth(), width);
    EXPECT_EQ(driver.lastHeight(), height);
    EXPECT_EQ(driver.lastSize(), texBytes);

    auto const& bytes = driver.lastBytes();
    ASSERT_EQ(bytes.size(), texBytes);
    EXPECT_EQ(std::memcmp(bytes.data(), src.data(), texBytes), 0);

    lut.destroy(driver);
}

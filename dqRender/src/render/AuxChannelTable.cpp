// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Auxiliary channel table implementation
// Ported from: itwinjs-core core/frontend/src/common/internal/render/AuxChannelTable.ts
#include "AuxChannelTable.h"
#include "VertexTableBuilder.h"
#include "dqRender/rhi/Driver.h"
#include "gl/RenderFlags.h"

#include <dqCommon/OctEncodedNormal.h>
#include <dqCommon/QPoint.h>
#include <dqGeom/AuxData.h>

#include <cmath>

BEGIN_DQ_RENDER_NAMESPACE

namespace {

// Ported from: AuxChannelTable.ts:191-196 invert。
double invert(double num)
{
    if (0.0 != num)
        num = 1.0 / num;
    return num;
}

// Ported from: AuxChannelTable.ts:361-371 computeNumBytesPerVertex。
uint32_t computeNumBytesPerVertex(dqGeom::AuxChannel const& channel)
{
    size_t const nEntries = channel.data.size();
    switch (channel.dataType) {
        case dqGeom::AuxChannelDataType::Vector:
            return static_cast<uint32_t>(6 * nEntries);  // 3 16-bit quantized coordinate values per entry.
        case dqGeom::AuxChannelDataType::Normal:
        case dqGeom::AuxChannelDataType::Distance:
        case dqGeom::AuxChannelDataType::Scalar:
            return static_cast<uint32_t>(2 * nEntries);  // 1 16-bit quantized value per entry.
    }
    return 0;
}

// setUint16 LE（参考 DataView.setUint16(byteIndex, value, true) 的 C++ 承载——
// 显式低字节在前，与宿主机端序无关）。
void setUint16LE(std::vector<uint8_t>& data, size_t byteIndex, uint16_t value)
{
    data[byteIndex] = static_cast<uint8_t>(value & 0xFF);
    data[byteIndex + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

// Ported from: AuxChannelTable.ts:198-359 AuxChannelTableBuilder。
// _view: DataView → 直写 m_data 字节（setUint16LE）；_props 三族数组成员直持。
class AuxChannelTableBuilder {
public:
    AuxChannelTableBuilder(std::vector<uint8_t>& data, uint32_t numBytesPerVertex)
        : m_data(data)
        , m_numBytesPerVertex(numBytesPerVertex)
    {}

    std::vector<AuxDisplacementChannel> m_displacements;
    std::vector<AuxChannel> m_normals;
    std::vector<AuxParamChannel> m_params;

    // Ported from: AuxChannelTable.ts:239-251 build。
    void build(std::vector<dqGeom::AuxChannel> const& channels)
    {
        size_t byteOffset = 0;
        for (auto const& channel : channels) {
            if (dqGeom::AuxChannelDataType::Normal == channel.dataType)
                addNormals(channel, byteOffset);
            else if (dqGeom::AuxChannelDataType::Vector == channel.dataType)
                addDisplacements(channel, byteOffset);
            else
                addParams(channel, byteOffset);

            byteOffset += computeNumBytesPerVertex(channel);
        }
    }

private:
    std::vector<uint8_t>& m_data;
    uint32_t m_numBytesPerVertex;

    // Ported from: AuxChannelTable.ts:253-275 addNormals。
    void addNormals(dqGeom::AuxChannel const& channel, size_t byteOffset)
    {
        std::vector<float> inputs;
        std::vector<int> indices;

        for (size_t i = 0; i < channel.data.size(); i++) {
            size_t byteIndex = byteOffset + i * 2;  // 2 bytes per normal
            indices.push_back(static_cast<int>(byteIndex / 2));  // indices aligned to 2-byte intervals

            auto const& data = channel.data[i];
            inputs.push_back(static_cast<float>(data.input));

            for (size_t j = 0; j + 2 < data.values.size(); j += 3) {
                dqGeom::Vector3d normal = dqGeom::Vector3d::From(data.values[j], data.values[j + 1],
                                                                 data.values[j + 2]);
                normal.Normalize();

                uint16_t const encodedNormal =
                    static_cast<uint16_t>(dqCommon::OctEncodedNormal::encode(normal));
                setUint16LE(m_data, byteIndex, encodedNormal);
                byteIndex += m_numBytesPerVertex;
            }
        }

        m_normals.emplace_back(channel.name.value_or(""), std::move(inputs), std::move(indices));
    }

    // Ported from: AuxChannelTable.ts:285-316 addParams。
    void addParams(dqGeom::AuxChannel const& channel, size_t byteOffset)
    {
        std::vector<float> inputs;
        std::vector<int> indices;

        auto range = dqGeom::Range1d::CreateNull();
        for (auto const& data : channel.data) {
            inputs.push_back(static_cast<float>(data.input));
            for (double const v : data.values)
                range.Extend(v);
        }

        double const qScale = dqCommon::Quantization::computeScale(range.high - range.low);

        for (size_t i = 0; i < channel.data.size(); i++) {
            size_t byteIndex = byteOffset + i * 2;  // 2 bytes per double
            indices.push_back(static_cast<int>(byteIndex / 2));  // indices aligned to 2-byte intervals

            for (double const value : channel.data[i].values) {
                uint16_t const quantized =
                    dqCommon::Quantization::quantize(value, range.low, qScale);
                setUint16LE(m_data, byteIndex, quantized);
                byteIndex += m_numBytesPerVertex;
            }
        }

        m_params.emplace_back(channel.name.value_or(""), std::move(inputs), std::move(indices),
                              static_cast<float>(range.low),
                              static_cast<float>(invert(qScale)));
    }

    // Ported from: AuxChannelTable.ts:318-358 addDisplacements。
    void addDisplacements(dqGeom::AuxChannel const& channel, size_t byteOffset)
    {
        std::vector<float> inputs;
        std::vector<int> indices;

        auto range = dqGeom::Range3d::CreateNull();
        for (auto const& data : channel.data) {
            inputs.push_back(static_cast<float>(data.input));
            for (size_t i = 0; i + 2 < data.values.size(); i += 3)
                range.ExtendPoint(dqGeom::Point3d::From(data.values[i], data.values[i + 1],
                                                        data.values[i + 2]));
        }

        auto const qParams = dqCommon::QParams3d::fromRange(range);
        for (size_t i = 0; i < channel.data.size(); i++) {
            size_t byteIndex = byteOffset + i * 6;  // 2 bytes per coordinate
            indices.push_back(static_cast<int>(byteIndex / 2));  // indices aligned to 2-byte intervals

            auto const& data = channel.data[i];
            for (size_t j = 0; j + 2 < data.values.size(); j += 3) {
                auto const qPoint = dqCommon::QPoint3d::create(
                    dqGeom::Point3d::From(data.values[j], data.values[j + 1], data.values[j + 2]),
                    qParams);

                setUint16LE(m_data, byteIndex + 0, qPoint.x);
                setUint16LE(m_data, byteIndex + 2, qPoint.y);
                setUint16LE(m_data, byteIndex + 4, qPoint.z);
                byteIndex += m_numBytesPerVertex;
            }
        }

        float qOrigin[3] = {static_cast<float>(qParams.origin.x), static_cast<float>(qParams.origin.y),
                            static_cast<float>(qParams.origin.z)};
        float qScale[3] = {static_cast<float>(invert(qParams.scale.x)),
                           static_cast<float>(invert(qParams.scale.y)),
                           static_cast<float>(invert(qParams.scale.z))};
        m_displacements.emplace_back(channel.name.value_or(""), std::move(inputs),
                                     std::move(indices), qOrigin, qScale);
    }
};

}  // namespace

// Ported from: AuxChannelTable.ts:186-188 fromChannels + :209-237 buildAuxChannelTable。
// computeDimensions 复用 VertexTableBuilder（= VertexTable.ts:53-81 的既有 1:1 移植）。
std::unique_ptr<AuxChannelTable> AuxChannelTable::fromChannels(
    std::vector<dqGeom::AuxChannel> const& channels, uint32_t numVertices, uint32_t maxDimension)
{
    uint32_t numBytesPerVertex = 0;
    for (auto const& channel : channels)
        numBytesPerVertex += computeNumBytesPerVertex(channel);
    if (0 == numBytesPerVertex)
        return nullptr;

    uint32_t const nRgbaPerVertex = (numBytesPerVertex + 3) / 4;
    uint32_t const nUnusedBytesPerVertex = nRgbaPerVertex * 4 - numBytesPerVertex;
    // 参考断言（:216）：0 === nUnusedBytesPerVertex || 2 === nUnusedBytesPerVertex——
    // numBytesPerVertex 恒偶（2B/6B 族）故恒成立，不再重复断言。

    // We don't want any unused bytes. If we've got 2 extra, make every other vertex's
    // channel start in the middle of the first texel.（:218-223 怪癖原样）
    uint32_t width = 0, height = 0;
    if (0 != nUnusedBytesPerVertex)
        VertexTableBuilder::computeDimensions((numVertices + 1) / 2, numBytesPerVertex / 2, 0,
                                              maxDimension, width, height);  // twice as many RGBA for half as many vertices.
    else
        VertexTableBuilder::computeDimensions(numVertices, nRgbaPerVertex, 0, maxDimension,
                                              width, height);

    std::vector<uint8_t> data(static_cast<size_t>(width) * height * 4, 0);

    AuxChannelTableBuilder builder(data, numBytesPerVertex);
    builder.build(channels);

    // 参考 :236 经 AuxChannelTable.fromJSON(props) 构造（三族全空→undefined 语义——
    // 本路径 numBytesPerVertex>0 已保证至少一族非空，fromJSON 早退不可达）。
    return std::unique_ptr<AuxChannelTable>(new AuxChannelTable(
        std::move(data), width, height, numVertices, numBytesPerVertex,
        std::move(builder.m_displacements), std::move(builder.m_normals),
        std::move(builder.m_params)));
}

AuxChannelTable::AuxChannelTable(
    std::vector<uint8_t> data, uint32_t width, uint32_t height,
    uint32_t numVertices, uint32_t numBytesPerVertex,
    std::vector<AuxDisplacementChannel> displacements,
    std::vector<AuxChannel> normals,
    std::vector<AuxParamChannel> params)
    : m_data(std::move(data))
    , m_width(width)
    , m_height(height)
    , m_numVertices(numVertices)
    , m_numBytesPerVertex(numBytesPerVertex)
    , m_displacements(std::move(displacements))
    , m_normals(std::move(normals))
    , m_params(std::move(params))
{
}

std::unique_ptr<AuxChannelTable> AuxChannelTable::create(
    std::vector<uint8_t> data, uint32_t width, uint32_t height,
    uint32_t numVertices, uint32_t numBytesPerVertex,
    std::vector<AuxDisplacementChannel> displacements,
    std::vector<AuxChannel> normals,
    std::vector<AuxParamChannel> params)
{
    if (data.empty() || width == 0 || height == 0)
        return nullptr;

    return std::unique_ptr<AuxChannelTable>(new AuxChannelTable(
        std::move(data), width, height, numVertices, numBytesPerVertex,
        std::move(displacements), std::move(normals), std::move(params)));
}

rhi::TextureHandle AuxChannelTable::createLutTexture(rhi::Driver& driver) const
{
    if (m_texture)
        return m_texture;

    // Create RGBA8 texture with packed animation data
    // Ported from: itwinjs-core VertexLutTexture pattern
    m_texture = driver.createTexture(
        rhi::SamplerType::SAMPLER_2D,
        1,  // levels
        rhi::TextureFormat::RGBA8,
        m_width,
        m_height,
        1,  // depth
        rhi::TextureUsage::DEFAULT);

    if (!m_texture)
        return m_texture;

    // Upload packed RGBA8 data
    rhi::PixelBufferDescriptor pbd(
        m_data.data(),
        m_data.size(),
        0,  // format (unused for RGBA8)
        0,  // type (unused for RGBA8)
        0,  // stride
        1,  // alignment
        0, 0,  // left, top
        m_width, m_height, 1);  // width, height, depth

    driver.setTextureData(m_texture, 0, 0, 0, 0, m_width, m_height, 0, std::move(pbd));

    return m_texture;
}

END_DQ_RENDER_NAMESPACE

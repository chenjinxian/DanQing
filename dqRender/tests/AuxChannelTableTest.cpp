// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — AuxChannelTable::fromChannels（AuxChannelTableBuilder）测试
// Ported from: itwinjs-core core/frontend/src/common/internal/render/AuxChannelTable.ts
//              （buildAuxChannelTable :209-237 + addNormals :253-275 + addParams :285-316
//               + addDisplacements :318-358 + computeNumBytesPerVertex :361-371 + invert :191-196；
//               尺寸怪癖 :214-223 的 computeDimensions 复用 VertexTable.ts:53-81）
// 打包钉值（期望值=从参考算法手工计算的确定值；判据形参照
// core/frontend/src/test/render/primitives/MeshBuilder.test.ts 的 aux data 四例
// :417-500——那四例锁全链（MeshBuilder→toMeshArgs→createMeshParams），W-a4 移植；
// 本文件锁 W-a3 的打包器单体）。
#include <gtest/gtest.h>

#include "../src/render/AuxChannelTable.h"

#include <dqGeom/AuxData.h>

#include <cstring>
#include <vector>

using namespace dqRender;
using dqGeom::AuxChannelData;
using dqGeom::AuxChannelDataType;
// 注：通道类型显式 dqGeom::AuxChannel——dqRender::AuxChannel（AuxChannelTable.h）同名在域。

namespace {

// data 字节 → uint16 LE 序列（1:1 参考测试的 new Uint16Array(aux.data.buffer) 视图）。
std::vector<uint16_t> asUint16(std::vector<uint8_t> const& data)
{
    std::vector<uint16_t> out(data.size() / 2);
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<uint16_t>(data[i * 2] | (static_cast<uint16_t>(data[i * 2 + 1]) << 8));
    return out;
}

}  // namespace

// Ported from: AuxChannelTable.ts 打包语义（判据形参照 MeshBuilder.test.ts:417-434
// "preserves aux data for triangle facets" 的 expectAuxChannelTable 段——
// scalar 1 帧 3 顶点 [0,0x7fff,0xffff] 输入 → [0,0x7fff,0xffff,0] + 尾零未用字节）。
TEST(AuxChannelTableTest, FromChannelsScalarPacksQuantizedUint16)
{
    dqGeom::AuxChannel channel({AuxChannelData(1.0, {0.0, 32767.0, 65535.0})}, AuxChannelDataType::Scalar,
                       std::string("s"));
    auto table = AuxChannelTable::fromChannels({channel}, 3, 2048);
    ASSERT_TRUE(table != nullptr);
    EXPECT_EQ(table->numVertices(), 3u);
    EXPECT_EQ(table->numBytesPerVertex(), 2u);
    // 未用字节怪癖（:214-223）：2 余字节 → computeDimensions((3+1)/2, 2/2, 0, 2048) = 2×1。
    EXPECT_EQ(table->width(), 2u);
    EXPECT_EQ(table->height(), 1u);
    // range [0,65535] → qScale=1 → 值恒等量化；尾零 = 末 texel 未用字节。
    EXPECT_EQ(asUint16(table->data()), std::vector<uint16_t>({0, 0x7FFF, 0xFFFF, 0}));

    // 通道元数据（AuxParamChannel 族：qOrigin=[range.low]、qScale=[invert(qScale)]）。
    ASSERT_EQ(table->params().size(), 1u);
    EXPECT_EQ(table->params()[0].name(), "s");
    EXPECT_EQ(table->params()[0].inputs(), std::vector<float>({1.0f}));
    EXPECT_EQ(table->params()[0].indices(), std::vector<int>({0}));
    EXPECT_DOUBLE_EQ(table->params()[0].qOrigin(), 0.0);
    EXPECT_DOUBLE_EQ(table->params()[0].qScale(), 1.0);
    EXPECT_TRUE(table->displacements().empty());
    EXPECT_TRUE(table->normals().empty());
}

// Ported from: AuxChannelTable.ts addParams 多帧布局（:297-306 的 byteIndex=offset+i*2
// 帧步进 + byteIndex += numBytesPerVertex 顶点步进）。
TEST(AuxChannelTableTest, FromChannelsScalarTwoFramesInterleaveFramesPerVertex)
{
    dqGeom::AuxChannel channel({AuxChannelData(0.0, {0.0, 10.0}), AuxChannelData(1.0, {20.0, 30.0})},
                       AuxChannelDataType::Scalar, std::string("s2"));
    auto table = AuxChannelTable::fromChannels({channel}, 2, 2048);
    ASSERT_TRUE(table != nullptr);
    // 2 帧 scalar → numBytesPerVertex=4 → nRgbaPerVertex=1、无未用字节 → 常规路径
    // computeDimensions(2, 1, 0, 2048) = 2×1。
    EXPECT_EQ(table->numBytesPerVertex(), 4u);
    EXPECT_EQ(table->width(), 2u);
    EXPECT_EQ(table->height(), 1u);
    // range [0,30] → qScale=2184.5：f0=[0,21845] f1=[43690,65535]；
    // 布局帧主序顶点内联：v0={f0@0, f1@2}、v1={f0@4, f1@6}。
    EXPECT_EQ(asUint16(table->data()),
              std::vector<uint16_t>({0, 43690, 21845, 65535}));
    // indices：帧 i 的 2 字节槽起始 /2（:299 注释"indices aligned to 2-byte intervals"）。
    ASSERT_EQ(table->params().size(), 1u);
    EXPECT_EQ(table->params()[0].inputs(), std::vector<float>({0.0f, 1.0f}));
    EXPECT_EQ(table->params()[0].indices(), std::vector<int>({0, 1}));
    EXPECT_DOUBLE_EQ(table->params()[0].qOrigin(), 0.0);
    // qScale 存储为 float（DanQing 骨架通道类型——GPU uniform 精度面）；
    // 期望值 = invert(computeScale(30)) = 30/65535 经 float 往返。
    EXPECT_FLOAT_EQ(table->params()[0].qScale(), static_cast<float>(30.0 / 65535.0));
}

// Ported from: AuxChannelTable.ts addNormals（:253-275——逐顶点 normalize +
// OctEncodedNormal.encode → setUint16；M-W 前置件归位后的 16 位参考格式）。
TEST(AuxChannelTableTest, FromChannelsNormalPacksOctEncodedUint16)
{
    dqGeom::AuxChannel channel({AuxChannelData(0.0, {0.0, 0.0, 1.0, 0.0, 0.0, -1.0})},
                       AuxChannelDataType::Normal, std::string("n"));
    auto table = AuxChannelTable::fromChannels({channel}, 2, 2048);
    ASSERT_TRUE(table != nullptr);
    EXPECT_EQ(table->numBytesPerVertex(), 2u);
    // (0,0,1) → 0x8080；(0,0,-1) → 折叠后 0xFFFF（OctEncodedNormal.ts:52-63 钉值，
    // dqCommon OctEncodedNormalTest.EncodesReference16BitValues 同值）。
    // 2 余字节怪癖路径：computeDimensions((2+1)/2, 1, 0, 2048) = 1×1 → 4 字节恰装 2 顶点。
    EXPECT_EQ(table->width(), 1u);
    EXPECT_EQ(table->height(), 1u);
    EXPECT_EQ(asUint16(table->data()), std::vector<uint16_t>({0x8080, 0xFFFF}));
    ASSERT_EQ(table->normals().size(), 1u);
    EXPECT_EQ(table->normals()[0].name(), "n");
    EXPECT_EQ(table->normals()[0].inputs(), std::vector<float>({0.0f}));
    EXPECT_TRUE(table->params().empty());
    EXPECT_TRUE(table->displacements().empty());
}

// Ported from: AuxChannelTable.ts addDisplacements（:318-358——QParams3d.fromRange +
// QPoint3d.init + qScale 逐分量 invert；判据形参照 MeshBuilder.test.ts:479-500
// "produces aux data for vector channel"）。
TEST(AuxChannelTableTest, FromChannelsDisplacementPacksQPoint3dUint16x3)
{
    dqGeom::AuxChannel channel({AuxChannelData(0.0, {0.0, 0.0, 0.0, 6.0, 12.0, 18.0})},
                       AuxChannelDataType::Vector, std::string("v"));
    auto table = AuxChannelTable::fromChannels({channel}, 2, 2048);
    ASSERT_TRUE(table != nullptr);
    // Vector 1 帧 → 6B/顶点（:365）→ nRgbaPerVertex=(6+3)/4=2、**未用字节=2** → quirk
    // 路径 computeDimensions((2+1)/2, 6/2, 0, 2048) = 3×1（"every other vertex's channel
    // start in the middle of the first texel"——2 顶点 12B 恰 3 texel）。
    EXPECT_EQ(table->numBytesPerVertex(), 6u);
    EXPECT_EQ(table->width(), 3u);
    EXPECT_EQ(table->height(), 1u);
    // v0=(0,0,0)→(0,0,0)；v1=(6,12,18)→(0xFFFF,0xFFFF,0xFFFF)；无尾填充（12B 恰好）。
    EXPECT_EQ(asUint16(table->data()),
              std::vector<uint16_t>({0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF}));
    ASSERT_EQ(table->displacements().size(), 1u);
    EXPECT_EQ(table->displacements()[0].name(), "v");
    // qOrigin=range.low=(0,0,0)；qScale=invert(65535/extent) 逐分量。
    auto const& disp = table->displacements()[0];
    EXPECT_FLOAT_EQ(disp.qOrigin()[0], 0.0f);
    EXPECT_FLOAT_EQ(disp.qOrigin()[1], 0.0f);
    EXPECT_FLOAT_EQ(disp.qOrigin()[2], 0.0f);
    EXPECT_FLOAT_EQ(disp.qScale()[0], static_cast<float>(6.0 / 65535.0));
    EXPECT_FLOAT_EQ(disp.qScale()[1], static_cast<float>(12.0 / 65535.0));
    EXPECT_FLOAT_EQ(disp.qScale()[2], static_cast<float>(18.0 / 65535.0));
}

// Ported from: AuxChannelTable.ts buildAuxChannelTable 空通道早退（:210-212——
// numBytesPerVertex==0 → undefined）。
TEST(AuxChannelTableTest, FromChannelsEmptyChannelsReturnsNull)
{
    EXPECT_TRUE(AuxChannelTable::fromChannels({}, 3, 2048) == nullptr);
}

// Ported from: AuxChannelTable.ts 多通道交错（build :239-251 的 byteOffset 逐通道累加）
// + 2 余字节怪癖的偶数顶点形态（:218-223"every other vertex's channel start in the
// middle of the first texel"）。
TEST(AuxChannelTableTest, FromChannelsMultiChannelByteOffsets)
{
    // scalar 1 帧（2B）+ displacement 1 帧（6B）→ numBytesPerVertex=8、nRgbaPerVertex=2、
    // 无未用字节。scalar 在 byteOffset=0、displacement 在 byteOffset=2（build 的 :249 累加）。
    dqGeom::AuxChannel scalarCh({AuxChannelData(0.0, {0.0, 30.0})}, AuxChannelDataType::Scalar,
                        std::string("s"));
    dqGeom::AuxChannel dispCh({AuxChannelData(0.0, {0.0, 0.0, 0.0, 1.0, 2.0, 3.0})},
                      AuxChannelDataType::Vector, std::string("v"));
    auto table = AuxChannelTable::fromChannels({scalarCh, dispCh}, 2, 2048);
    ASSERT_TRUE(table != nullptr);
    EXPECT_EQ(table->numBytesPerVertex(), 8u);
    EXPECT_EQ(table->width(), 4u);   // 2 顶点 × 2 texel
    EXPECT_EQ(table->height(), 1u);
    // v0: scalar=0 @0..1；disp=(0,0,0) @2..7 → 全 0。
    // v1: scalar=65535 @8..9；disp=(1,2,3)→(65535,65535,65535) @10..15。
    EXPECT_EQ(asUint16(table->data()),
              std::vector<uint16_t>({0, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}));
    ASSERT_EQ(table->params().size(), 1u);
    ASSERT_EQ(table->displacements().size(), 1u);
    // displacement 槽索引（:336 的 byteIndex/2）：(2 + 0*6)/2 = 1。
    EXPECT_EQ(table->displacements()[0].indices(), std::vector<int>({1}));
    EXPECT_EQ(table->params()[0].indices(), std::vector<int>({0}));
}

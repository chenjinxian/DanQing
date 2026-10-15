// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — MeshBuilder auxData 消费链测试（W-a4）
// Ported from: itwinjs-core core/frontend/src/test/render/primitives/MeshBuilder.test.ts
//              describe("aux data")（:407-501 四例）+ GeometryAccumulator.ts:101-115
//              （analysisStyleDisplacement range 扩展腿）。
//
// EQUIVALENCE: 参考源=MeshBuilder.test.ts:408-415 的 expectAuxChannelTable 经
// mesh.toMeshArgs()→createMeshParams（VertexTableBuilder.ts:93 的 fromChannels 调用点）；
// 发散=DanQing 无 builder 网格的 toMeshArgs/createMeshParams 生产面（PrimitiveBuilder::finish
// 走 MeshRenderGeometry 而非 MeshArgs——参考侧亦无从 Decoration Mesh 回读 MeshArgs 的测试面），
// 本测试以同参直调 AuxChannelTable::fromChannels(mesh.auxChannels, numVertices, 2048)
// 替代该两跳（createMeshParams 内部即同参直调，数据无变换）；验证法=四例参考期望字节值
// 逐位相等（:433/:453/:476/:496 的原样钉值）。
#include <gtest/gtest.h>

#include "render/GeometryAccumulator.h"
#include "render/MeshBuilder.h"

#include "../src/render/AuxChannelLut.h"
#include "../src/render/AuxChannelTable.h"
#include "NullDriver.h"
#include "rhi/DriverBase.h"
#include "rhi/HandleAllocator.h"

#include <dqGeom/AuxData.h>
#include <dqGeom/PolyfaceBuilder.h>

#include <cstring>
#include <vector>

using namespace dqRender;
using dqGeom::AuxChannelData;
using dqGeom::AuxChannelDataType;
using dqGeom::Point3d;
using dqGeom::PolyfaceAuxData;
// 注：通道类型显式 dqGeom::AuxChannel——dqRender::AuxChannel（AuxChannelTable.h）同名在域。

namespace {

std::vector<uint16_t> asUint16(std::vector<uint8_t> const& data)
{
    std::vector<uint16_t> out(data.size() / 2);
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<uint16_t>(data[i * 2] | (static_cast<uint16_t>(data[i * 2 + 1]) << 8));
    return out;
}

// 1:1 MeshBuilder.test.ts createMeshBuilder（:392-405——quantizePositions:false/tolerance
// 0.15/areaTolerance=ToleranceRatio.facetArea*tolerance 的缺省面）的 DanQing 落形
// （DanQing Props 直接携 tolerance/areaTolerance——VertexMap 容差取 tolerance）。
std::unique_ptr<MeshBuilder> createMeshBuilder(dqGeom::Range3d const& range)
{
    MeshBuilder::Props props;
    props.quantizePositions = false;
    props.type = MeshPrimitiveType::Mesh;
    props.range = range;
    props.tolerance = 0.15;
    props.areaTolerance = 0.15;  // ToleranceRatio.facetArea × tolerance 的具体值与本族断言无关
    props.displayParams = DisplayParams::createForMesh(dqCommon::GraphicParams{}, false);
    props.is2d = false;
    props.isPlanar = true;
    return MeshBuilder::create(std::move(props));
}

}  // namespace

// Ported from: MeshBuilder.test.ts it("preserves aux data for triangle facets")（:417-434）。
// 参考的 PolyfaceAuxData indices [0,1,2]（0-based 无符号）→ DanQing 约定 signed 1-based
// [1,2,3]（AuxData.h 文件头 EQUIVALENCE）。
TEST(MeshBuilderAuxDataTest, PreservesAuxDataForTriangleFacets)
{
    auto pfBuilder = dqGeom::PolyfaceBuilder::create();
    Point3d const tri[3] = {Point3d::From(0, 0, 0), Point3d::From(1, 0, 0), Point3d::From(1, 1, 0)};
    pfBuilder->AddTriangleFacet(tri);

    auto pf = pfBuilder->ClaimPolyface();
    dqGeom::AuxChannel channel({AuxChannelData(1.0, {0.0, 32767.0, 65535.0})}, AuxChannelDataType::Scalar,
                       std::string("s"));
    pf->Data().auxData = PolyfaceAuxData({channel}, {1, 2, 3});

    auto meshBuilder = createMeshBuilder(dqGeom::Range3d::CreateXYZXYZ(0, 0, 0, 1, 1, 0));
    MeshBuilder::PolyfaceOptions opts;
    opts.includeParams = false;
    opts.fillColor = 0;
    meshBuilder->addFromPolyface(*pf, opts, std::nullopt);
    auto const& mesh = meshBuilder->mesh();
    EXPECT_EQ(mesh.points().length(), 3u);
    ASSERT_TRUE(mesh.auxChannels().has_value());
    EXPECT_EQ(mesh.auxChannels()->size(), 1u);
    // 通道值逐顶点搬运（无去重分支）：gathered=[0, 32767, 65535]。
    ASSERT_EQ(mesh.auxChannels()->at(0).data.size(), 1u);
    EXPECT_EQ(mesh.auxChannels()->at(0).data[0].values,
              std::vector<double>({0.0, 32767.0, 65535.0}));

    // 打包字节（fromChannels 同参直调——EQUIVALENCE 见文件头）：scalar 1 帧 3 顶点 →
    // [0, 0x7fff, 0xffff, 0]（尾零=末 texel 未用字节，参考 :433 注释原样）。
    auto table = AuxChannelTable::fromChannels(*mesh.auxChannels(),
                                               static_cast<uint32_t>(mesh.points().length()), 2048);
    ASSERT_TRUE(table != nullptr);
    EXPECT_EQ(asUint16(table->data()), std::vector<uint16_t>({0, 0x7FFF, 0xFFFF, 0}));
}

// Ported from: MeshBuilder.test.ts it("preserves aux data for facets with more than 3 sides")
// （:436-456——quad fan 三角化 2 三角 6 顶点无去重）。
TEST(MeshBuilderAuxDataTest, PreservesAuxDataForFacetsWithMoreThan3Sides)
{
    auto pfBuilder = dqGeom::PolyfaceBuilder::create();
    Point3d const quad[4] = {Point3d::From(0, 0, 0), Point3d::From(0, 1, 0), Point3d::From(1, 1, 0),
                             Point3d::From(1, 0, 0)};
    pfBuilder->AddQuadFacet(quad);

    auto pf = pfBuilder->ClaimPolyface();
    dqGeom::AuxChannel channel({AuxChannelData(1.0, {0.0, 0x4fff, 0xbfff, 0xffff})},
                       AuxChannelDataType::Scalar, std::string("s"));
    pf->Data().auxData = PolyfaceAuxData({channel}, {1, 2, 3, 4});

    auto meshBuilder = createMeshBuilder(dqGeom::Range3d::CreateXYZXYZ(0, 0, 0, 1, 1, 0));
    MeshBuilder::PolyfaceOptions opts;
    opts.includeParams = false;
    opts.fillColor = 0;
    meshBuilder->addFromPolyface(*pf, opts, std::nullopt);
    auto const& mesh = meshBuilder->mesh();
    EXPECT_EQ(mesh.points().length(), 6u);
    ASSERT_TRUE(mesh.auxChannels().has_value());
    ASSERT_EQ(mesh.auxChannels()->size(), 1u);

    auto const& aux = mesh.auxChannels()->at(0);
    ASSERT_EQ(aux.data.size(), 1u);
    // 参考期望（:453）：[0, 0x4fff, 0xbfff, 0, 0xbfff, 0xffff]——fan (0,1,2)+(0,2,3)。
    std::vector<double> const expectedData = {0.0, 0x4fff, 0xbfff, 0.0, 0xbfff, 0xffff};
    EXPECT_EQ(aux.data[0].values, expectedData);

    auto table = AuxChannelTable::fromChannels(*mesh.auxChannels(),
                                               static_cast<uint32_t>(mesh.points().length()), 2048);
    ASSERT_TRUE(table != nullptr);
    // 打包：range [0,65535] → 恒等；6 顶点 2B → quirk 路径 3×2=6 uint16 恰排（无尾零）。
    EXPECT_EQ(asUint16(table->data()),
              std::vector<uint16_t>({0, 0x4FFF, 0xBFFF, 0, 0xBFFF, 0xFFFF}));
}

// Ported from: MeshBuilder.test.ts it("maps aux data to vertices based on indices")
// （:458-477——indices [2,0,0,1]（0-based）→ DanQing [3,1,1,2]）。
TEST(MeshBuilderAuxDataTest, MapsAuxDataToVerticesBasedOnIndices)
{
    auto pfBuilder = dqGeom::PolyfaceBuilder::create();
    Point3d const quad[4] = {Point3d::From(0, 0, 0), Point3d::From(0, 1, 0), Point3d::From(1, 1, 0),
                             Point3d::From(1, 0, 0)};
    pfBuilder->AddQuadFacet(quad);

    auto pf = pfBuilder->ClaimPolyface();
    dqGeom::AuxChannel channel({AuxChannelData(1.0, {0x4000, 0x6000, 0x8000})}, AuxChannelDataType::Scalar,
                       std::string("s"));
    pf->Data().auxData = PolyfaceAuxData({channel}, {3, 1, 1, 2});

    auto meshBuilder = createMeshBuilder(dqGeom::Range3d::CreateXYZXYZ(0, 0, 0, 1, 1, 0));
    MeshBuilder::PolyfaceOptions opts;
    opts.includeParams = false;
    opts.fillColor = 0;
    meshBuilder->addFromPolyface(*pf, opts, std::nullopt);
    auto const& mesh = meshBuilder->mesh();
    EXPECT_EQ(mesh.points().length(), 6u);
    ASSERT_TRUE(mesh.auxChannels().has_value());

    auto const& aux = mesh.auxChannels()->at(0);
    ASSERT_EQ(aux.data.size(), 1u);
    // 参考期望（:475）：索引映射重排后的逐顶点值。
    EXPECT_EQ(aux.data[0].values,
              std::vector<double>({0x8000, 0x4000, 0x4000, 0x8000, 0x4000, 0x6000}));

    auto table = AuxChannelTable::fromChannels(*mesh.auxChannels(),
                                               static_cast<uint32_t>(mesh.points().length()), 2048);
    ASSERT_TRUE(table != nullptr);
    // 参考期望（:476）：range [0x4000,0x8000] → qScale=65535/16384≈4.0 → 量化值。
    EXPECT_EQ(asUint16(table->data()),
              std::vector<uint16_t>({0xFFFF, 0, 0, 0xFFFF, 0, 0x8000}));
}

// Ported from: MeshBuilder.test.ts it("produces aux data for vector channel")（:479-500）。
TEST(MeshBuilderAuxDataTest, ProducesAuxDataForVectorChannel)
{
    auto pfBuilder = dqGeom::PolyfaceBuilder::create();
    Point3d const quad[4] = {Point3d::From(0, 0, 0), Point3d::From(0, 1, 0), Point3d::From(1, 1, 0),
                             Point3d::From(1, 0, 0)};
    pfBuilder->AddQuadFacet(quad);

    auto pf = pfBuilder->ClaimPolyface();
    dqGeom::AuxChannel channel({AuxChannelData(1.0, {0.0, 1.0, 2.0, 3.0, 4.0, 65535.0})},
                       AuxChannelDataType::Vector, std::string("v"));
    pf->Data().auxData = PolyfaceAuxData({channel}, {1, 2, 2, 1});

    auto meshBuilder = createMeshBuilder(dqGeom::Range3d::CreateXYZXYZ(0, 0, 0, 1, 1, 0));
    MeshBuilder::PolyfaceOptions opts;
    opts.includeParams = false;
    opts.fillColor = 0;
    meshBuilder->addFromPolyface(*pf, opts, std::nullopt);

    ASSERT_TRUE(meshBuilder->mesh().auxChannels().has_value());
    auto const& aux = meshBuilder->mesh().auxChannels()->at(0);
    // 参考期望（:492-495）。
    EXPECT_EQ(aux.data[0].values,
              std::vector<double>({0, 1, 2, 3, 4, 65535.0, 3, 4, 65535.0,
                                   0, 1, 2, 3, 4, 65535.0, 0, 1, 2}));

    auto table = AuxChannelTable::fromChannels(*meshBuilder->mesh().auxChannels(),
                                               static_cast<uint32_t>(meshBuilder->mesh().points().length()),
                                               2048);
    ASSERT_TRUE(table != nullptr);
    // 参考期望（:496-499）：range [(0,1,2)..(3,4,65535)] 量化——z 轴 [2,65535] 主导钉值。
    EXPECT_EQ(asUint16(table->data()),
              std::vector<uint16_t>({0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
                                     0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF, 0, 0, 0}));
}

// Ported from: MeshPrimitives.ts:189-220 addAuxChannels 的失配 bail 怪癖（:190-203——
// "For now just bail on mismatch"：通道数/逐通道 dataType+name+inputName 不合即静默丢弃）。
// 参考无对应单元测试（PolyfaceAuxData/MeshBuilder 测试均不覆盖 bail 腿——Authored 锚定
// 该段参考源码语义）。
TEST(MeshBuilderAuxDataTest, MismatchedChannelsBailWithoutAppending)
{
    auto pfBuilder = dqGeom::PolyfaceBuilder::create();
    Point3d const quad[4] = {Point3d::From(0, 0, 0), Point3d::From(0, 1, 0), Point3d::From(1, 1, 0),
                             Point3d::From(1, 0, 0)};
    pfBuilder->AddQuadFacet(quad);
    auto pf = pfBuilder->ClaimPolyface();
    dqGeom::AuxChannel channelA({AuxChannelData(1.0, {0.0, 1.0, 2.0, 3.0})}, AuxChannelDataType::Scalar,
                        std::string("s"), std::string("i"));
    pf->Data().auxData = PolyfaceAuxData({channelA}, {1, 2, 3, 4});

    auto meshBuilder = createMeshBuilder(dqGeom::Range3d::CreateXYZXYZ(0, 0, 0, 1, 1, 0));
    MeshBuilder::PolyfaceOptions opts;
    opts.includeParams = false;
    opts.fillColor = 0;
    meshBuilder->addFromPolyface(*pf, opts, std::nullopt);
    ASSERT_TRUE(meshBuilder->mesh().auxChannels().has_value());
    size_t const countAfterFirst = meshBuilder->mesh().auxChannels()->at(0).data[0].values.size();
    EXPECT_EQ(countAfterFirst, 6u);

    // 第二 polyface：同名但 dataType 不同（Scalar→Distance）→ bail（不追加）。
    auto pfBuilder2 = dqGeom::PolyfaceBuilder::create();
    pfBuilder2->AddQuadFacet(quad);
    auto pf2 = pfBuilder2->ClaimPolyface();
    dqGeom::AuxChannel channelB({AuxChannelData(1.0, {9.0, 9.0, 9.0, 9.0})}, AuxChannelDataType::Distance,
                        std::string("s"), std::string("i"));
    pf2->Data().auxData = PolyfaceAuxData({channelB}, {1, 2, 3, 4});
    meshBuilder->addFromPolyface(*pf2, opts, std::nullopt);

    auto const& values = meshBuilder->mesh().auxChannels()->at(0).data[0].values;
    EXPECT_EQ(values.size(), countAfterFirst) << "mismatched channel dataType must bail (no append)";
    // 第三 polyface：通道数不同（2 通道 vs 1）→ bail。
    auto pf3 = pf->clone().StaticCast<dqGeom::IndexedPolyface>();
    dqGeom::AuxChannel channelC({AuxChannelData(2.0, {5.0, 5.0, 5.0, 5.0})}, AuxChannelDataType::Scalar,
                        std::string("s"), std::string("i"));
    pf3->Data().auxData = PolyfaceAuxData({channelA, channelC}, {1, 2, 3, 4});
    meshBuilder->addFromPolyface(*pf3, opts, std::nullopt);
    EXPECT_EQ(meshBuilder->mesh().auxChannels()->at(0).data[0].values.size(), countAfterFirst)
        << "mismatched channel count must bail (no append)";

    // 第四 polyface：同型同名 → 正常追加（+6）。
    auto pf4 = pf->clone().StaticCast<dqGeom::IndexedPolyface>();
    meshBuilder->addFromPolyface(*pf4, opts, std::nullopt);
    EXPECT_EQ(meshBuilder->mesh().auxChannels()->at(0).data[0].values.size(), countAfterFirst + 6u)
        << "matching channels append";
}

// Ported from: GeometryAccumulator.ts:101-115 addPolyface 的 analysisStyleDisplacement
// 分支（range 按位移域逐顶点六面扩展）。Authored：参考无该分支的单元测试（锚定源码语义）。
TEST(MeshBuilderAuxDataTest, AddPolyfaceExtendsRangeByDisplacement)
{
    // z=0 平面 quad + Vector 通道 "disp"（顶点 0 位移 (0,0,0)、顶点 1-3 位移 (0,0,5)——
    // 位移域 [(0,0,0),(0,0,5)]，scale=1）。
    auto pfBuilder = dqGeom::PolyfaceBuilder::create();
    Point3d const quad[4] = {Point3d::From(0, 0, 0), Point3d::From(0, 1, 0), Point3d::From(1, 1, 0),
                             Point3d::From(1, 0, 0)};
    pfBuilder->AddQuadFacet(quad);
    auto pf = pfBuilder->ClaimPolyface();
    dqGeom::AuxChannel dispCh({AuxChannelData(0.0, {0, 0, 0, 0, 0, 5.0, 0, 0, 5.0, 0, 0, 5.0})},
                      AuxChannelDataType::Vector, std::string("disp"));
    pf->Data().auxData = PolyfaceAuxData({dispCh}, {1, 2, 3, 4});

    dqCommon::AnalysisStyleDisplacement displacement;
    displacement.channelName = "disp";
    displacement.scale = 1.0;
    GeometryAccumulatorOptions options;
    options.analysisStyleDisplacement = displacement;
    GeometryAccumulator accum(options);

    DisplayParams const displayParams =
        DisplayParams::createForMesh(dqCommon::GraphicParams{}, false);
    ASSERT_TRUE(accum.addPolyface(pf, displayParams, dqGeom::Transform::CreateIdentity()));
    ASSERT_EQ(accum.geometries().length(), 1u);
    // 位移域 [0,0,5]×scale → 每顶点 z 扩 [0+0, 0+5]（GeometryAccumulator.ts:109-116）。
    auto const& range = accum.geometries().first()->tileRange();
    EXPECT_DOUBLE_EQ(range.low.z, 0.0 + 0.0);
    EXPECT_DOUBLE_EQ(range.high.z, 0.0 + 5.0);
    EXPECT_DOUBLE_EQ(range.low.x, 0.0);
    EXPECT_DOUBLE_EQ(range.high.x, 1.0);

    // 无 analysisStyleDisplacement → 位移分支不触（range=裸 polyface 域）。
    GeometryAccumulator accumPlain;
    ASSERT_TRUE(accumPlain.addPolyface(pf, displayParams, dqGeom::Transform::CreateIdentity()));
    auto const& plainRange = accumPlain.geometries().first()->tileRange();
    EXPECT_DOUBLE_EQ(plainRange.high.z, 0.0);

    // 通道名不命中 → 分支不触（:106 find 失败 → getPrimitiveRange 回退）。
    dqCommon::AnalysisStyleDisplacement miss;
    miss.channelName = "absent";
    miss.scale = 1.0;
    GeometryAccumulatorOptions missOptions;
    missOptions.analysisStyleDisplacement = miss;
    GeometryAccumulator accumMiss(missOptions);
    ASSERT_TRUE(accumMiss.addPolyface(pf, displayParams, dqGeom::Transform::CreateIdentity()));
    EXPECT_DOUBLE_EQ(accumMiss.geometries().first()->tileRange().high.z, 0.0);
}

// ---------------------------------------------------------------------------
// AuxChannelLUT（VertexLUT.ts:21-66）——Authored：参考无 AuxChannelLUT 单元测试
// （其消费面在 WebGL draw 期），锚定 :34-54 的 initChannels/hasScalarAnimation 语义。
// ---------------------------------------------------------------------------
namespace {

// VertexLutTextureTest.cpp 的 RecordingDriver 先例（返回真句柄 + 捕获上传维度）。
class AuxRecordingDriver : public rhi::NullDriver {
public:
    rhi::TextureHandle createTexture(rhi::SamplerType, uint8_t, rhi::TextureFormat,
                                     uint32_t width, uint32_t height, uint32_t /*depth*/,
                                     rhi::TextureUsage) noexcept override
    {
        m_lastWidth = width;
        m_lastHeight = height;
        return m_allocator.allocate<rhi::HwTexture>();
    }

    uint32_t m_lastWidth = 0;
    uint32_t m_lastHeight = 0;
    rhi::HandleAllocator m_allocator;
};

}  // namespace

// 锚定 VertexLUT.ts:34-54（initChannels 逐族填图 + hasScalarAnimation=params 表存在性）
// + :62-65 create（纹理尺寸=表维度）。
TEST(MeshBuilderAuxDataTest, AuxChannelLutMapsChannelsByName)
{
    // scalar 通道 "s"（1 帧 2 顶点）+ displacement 通道 "v"（1 帧）——双族混合。
    dqGeom::AuxChannel scalarCh({AuxChannelData(0.0, {0.0, 30.0})}, AuxChannelDataType::Scalar,
                                std::string("s"));
    dqGeom::AuxChannel dispCh({AuxChannelData(0.0, {0.0, 0.0, 0.0, 1.0, 2.0, 3.0})},
                              AuxChannelDataType::Vector, std::string("v"));
    auto table = AuxChannelTable::fromChannels({scalarCh, dispCh}, 2, 2048);
    ASSERT_TRUE(table != nullptr);

    AuxRecordingDriver driver;
    auto lut = AuxChannelLUT::create(driver, *table);
    ASSERT_TRUE(lut != nullptr);
    // 纹理创建维度 = 表维度（VertexLUT.ts:63 的 TextureHandle.createForData 等价）。
    EXPECT_EQ(driver.m_lastWidth, table->width());
    EXPECT_EQ(driver.m_lastHeight, table->height());
    // 逐族填图：name→channel。
    ASSERT_EQ(lut->params.size(), 1u);
    ASSERT_EQ(lut->displacements.size(), 1u);
    EXPECT_EQ(lut->normals.size(), 0u);
    EXPECT_TRUE(lut->params.find("s") != lut->params.end());
    EXPECT_TRUE(lut->displacements.find("v") != lut->displacements.end());
    // hasScalarAnimation = params 表非空（:54）。
    EXPECT_TRUE(lut->hasScalarAnimation());
    EXPECT_EQ(lut->numVertices(), 2u);
    EXPECT_EQ(lut->numBytesPerVertex(), 8u);  // scalar 2B + disp 6B

    // 纯位移（无 scalar）→ hasScalarAnimation=false。
    auto dispOnly = AuxChannelTable::fromChannels({dispCh}, 2, 2048);
    ASSERT_TRUE(dispOnly != nullptr);
    auto lut2 = AuxChannelLUT::create(driver, *dispOnly);
    ASSERT_TRUE(lut2 != nullptr);
    EXPECT_FALSE(lut2->hasScalarAnimation());
    EXPECT_EQ(lut2->params.size(), 0u);
}

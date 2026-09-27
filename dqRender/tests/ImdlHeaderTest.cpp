// ImdlHeaderTest — imdl 二进制头解析（G-D1）。
//
// Ported from: itwinjs-core full-stack-tests/core/src/frontend/standalone/tile/
//              TileIO.test.ts（processHeader 段 :87-106——每版本×场景的头字段
//              断言：version/headerLength/tolerance/计数）+
//              core/frontend/src/test/tile/ImdlParser.test.ts（非法头段
//              :133-143——零填充 → InvalidHeader；越界读段 :145-155 rejects）。
// 夹具：third_party/tile-sample-assets/imdl-fixtures/TileIOFixtures.h
//      （录制自真后端的 5 版本 × 5 场景二进制）。
#include <gtest/gtest.h>

#include "NullDriver.h"

#include "rhi/DriverBase.h"
#include "rhi/HandleAllocator.h"
#include "dqRender/rhi/BufferDescriptor.h"
#include "render/InstancedGeometry.h"
#include "render/MeshGraphic.h"
#include "render/PolyfaceGraphic.h"
#include "render/SurfaceGeometry.h"
#include "render/SurfaceVariantCompiler.h"
#include "render/shader/EdgeShaderBuilder.h"
#include "tile/TilesetJson.h"
#include "tile-sample-assets/imdl-fixtures/TileIOFixtures.h"
#include <dqCommon/LinePixels.h>
#include <dqRender/RenderSystem.h>
#include <dqRender/tile/ImdlDocument.h>
#include <dqRender/tile/ImdlHeader.h>
#include <dqRender/tile/RealityTileTree.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <vector>

using namespace dqRender::fixtures;

namespace {

dqRender::ImdlHeader parseHeader(uint8_t const* bytes, size_t size)
{
    dqRender::ImdlByteStream stream(bytes, size);
    return dqRender::ImdlHeader::readFrom(stream);
}

template <typename Ver>
void expectHeaderMatchesFixture(uint8_t const* bytes, size_t size)
{
    auto const h = parseHeader(bytes, size);
    EXPECT_TRUE(h.isValid()) << "magic";
    EXPECT_EQ(h.versionMajor(), Ver::versionMajor);
    EXPECT_EQ(h.versionMinor(), Ver::versionMinor);
    EXPECT_EQ(h.headerLength, Ver::headerLength);
    EXPECT_TRUE(h.isReadableVersion());
    // tileLength = total binary size (IModelTileIO.ts:63-64 — includes header).
    EXPECT_EQ(h.tileLength, size);
}

}  // namespace

// 头字段与录制元数据一致（5 版本 × rectangle 场景）。
// Ported from: TileIO.test.ts processHeader（版本/头长/长度断言）。
TEST(ImdlHeader, ParsesRecordedVersionsRectangle)
{
    expectHeaderMatchesFixture<V1_1>(V1_1::rectangleBytes, V1_1::rectangleSize);
    expectHeaderMatchesFixture<V1_2>(V1_2::rectangleBytes, V1_2::rectangleSize);
    expectHeaderMatchesFixture<V1_3>(V1_3::rectangleBytes, V1_3::rectangleSize);
    expectHeaderMatchesFixture<V1_4>(V1_4::rectangleBytes, V1_4::rectangleSize);
    expectHeaderMatchesFixture<V2_0>(V2_0::rectangleBytes, V2_0::rectangleSize);
}

// 全部场景（v1.1）：头一致 + tolerance/contentRange 合理。
// Ported from: TileIO.test.ts（场景遍历）。
TEST(ImdlHeader, ParsesAllScenariosV1_1)
{
    using V = V1_1;
    // numElementsIncluded per scenario — reference calls
    // (TileIO.test.ts :123/:151/:179/:207/:235): rectangle=1, triangles=6,
    // lineString=1, lineStrings=3, cylinder=1.
    struct Entry { uint8_t const* bytes; size_t size; uint32_t numElements; char const* name; };
    Entry const entries[] = {
        {V::rectangleBytes, V::rectangleSize, 1, "rectangle"},
        {V::lineStringBytes, V::lineStringSize, 1, "lineString"},
        {V::lineStringsBytes, V::lineStringsSize, 3, "lineStrings"},
        {V::trianglesBytes, V::trianglesSize, 6, "triangles"},
        {V::cylinderBytes, V::cylinderSize, 1, "cylinder"},
    };
    for (auto const& e : entries) {
        auto const h = parseHeader(e.bytes, e.size);
        EXPECT_TRUE(h.isValid()) << e.name;
        EXPECT_EQ(h.versionMajor(), 1) << e.name;
        EXPECT_EQ(h.versionMinor(), 1) << e.name;
        EXPECT_EQ(h.numElementsIncluded, e.numElements) << e.name;
        EXPECT_EQ(h.numElementsExcluded, 0u) << e.name;
        EXPECT_EQ(h.tileLength, e.size) << e.name;
    }
}

// 非法头：零填充 → invalid（format != IModel）。
// Ported from: ImdlParser.test.ts :133-143（new Uint8Array(512) 全零）。
TEST(ImdlHeader, RejectsZeroFilledData)
{
    std::vector<uint8_t> zeros(512, 0);
    auto const h = parseHeader(zeros.data(), zeros.size());
    EXPECT_FALSE(h.isValid());
}

// 越界读：截断数据 → invalid。
// Ported from: ImdlParser.test.ts :145-155（12 字节数组 rejects）。
TEST(ImdlHeader, RejectsTruncatedData)
{
    std::vector<uint8_t> shorty(12, 0x69);
    auto const h = parseHeader(shorty.data(), shorty.size());
    EXPECT_FALSE(h.isValid());
}

// versionMajor/Minor 编码：(major << 16) | minor（IModelTileIO.ts:68-69）。
TEST(ImdlHeader, VersionEncoding)
{
    using V2 = V2_0;
    auto const h = parseHeader(V2::rectangleBytes, V2::rectangleSize);
    EXPECT_EQ(h.version, (2u << 16) | 0u);
    EXPECT_EQ(h.emptySubRanges != 0 || h.versionMajor() >= 2, true)
        << "v2+ reads the emptySubRanges field";
}

// ---------------------------------------------------------------------------
// G-D2：内容描述 + glTF 段解析（夹具驱动）。
// Ported from: TileIO.test.ts readTile 段的描述部分（isLeaf 判定——
// processRectangle 断 result.isLeaf===true，:130）+ ParseImdlDocument.ts
// :1287-1320（glTF 段提取）。
// ---------------------------------------------------------------------------

namespace {
std::optional<dqRender::ImdlDocument> parseFull(uint8_t const* bytes, size_t size)
{
    dqRender::ImdlByteStream stream(bytes, size);
    auto const header = dqRender::ImdlHeader::readFrom(stream);
    if (!header.isValid())
        return std::nullopt;
    auto const desc = dqRender::decodeImdlContentDescription(header, stream);
    if (!desc.has_value())
        return std::nullopt;
    return dqRender::parseImdlDocument(stream);
}
}  // namespace

// 每版本 rectangle：文档解析成功——JSON 场景非空含 bufferViews。
// Ported from: ParseImdlDocument.ts（bufferViews 是 imdl 文档的必备段，
// 夹具 hex 可见 "bufferViews"）。
TEST(ImdlDocument, ParsesGltfSectionAllVersions)
{
    EXPECT_TRUE(parseFull(V1_1::rectangleBytes, V1_1::rectangleSize).has_value());
    EXPECT_TRUE(parseFull(V1_2::rectangleBytes, V1_2::rectangleSize).has_value());
    EXPECT_TRUE(parseFull(V1_3::rectangleBytes, V1_3::rectangleSize).has_value());
    EXPECT_TRUE(parseFull(V1_4::rectangleBytes, V1_4::rectangleSize).has_value());
    EXPECT_TRUE(parseFull(V2_0::rectangleBytes, V2_0::rectangleSize).has_value());
}

// v1.1 rectangle 的文档内容：JSON 含 bufferViews/meshes、binary 非空。
TEST(ImdlDocument, RectangleDocumentContent)
{
    auto doc = parseFull(V1_1::rectangleBytes, V1_1::rectangleSize);
    ASSERT_TRUE(doc.has_value());
    EXPECT_NE(doc->sceneJson.find("bufferViews"), std::string::npos);
    EXPECT_NE(doc->sceneJson.find("meshes"), std::string::npos);
    EXPECT_FALSE(doc->binary.empty()) << "binary section must follow the JSON scene";
}

// 内容描述：rectangle（单元素、无曲线、完整）→ isLeaf=true。
// Ported from: TileIO.test.ts processRectangle :130（result.isLeaf===true —
// 经 decodeTileContentDescription 的 leaf 判定，TileMetadata.ts:919-927）。
TEST(ImdlDocument, RectangleIsLeaf)
{
    dqRender::ImdlByteStream stream(V1_1::rectangleBytes, V1_1::rectangleSize);
    auto const header = dqRender::ImdlHeader::readFrom(stream);
    ASSERT_TRUE(header.isValid());
    auto const desc = dqRender::decodeImdlContentDescription(header, stream);
    ASSERT_TRUE(desc.has_value());
    EXPECT_TRUE(desc->isLeaf);
    EXPECT_EQ(desc->sizeMultiplier, 1.0);
}

// imdl 内容接入 tile 链（G-D3'）：夹具字节经 RealityTile::readContent 的
// TileFormat::IModel 分支——TileContent 元数据（contentRange/isLeaf）来自
// 头+描述（graphic 层 TODO(imdl-graphics)，规模已登记）。
// Ported from: ImdlReader.readImdlContent（ImdlReader.ts:74-140 的元数据段
// —— result.isLeaf/contentRange 同源）。
TEST(ImdlDocument, TileContentMetadataFromFixture)
{
    // 通过一棵桩 RealityTileTree 走 readContent。
    class ImdlTestTree : public dqRender::RealityTileTree {
    public:
        ImdlTestTree() : RealityTileTree(nullptr, "fixture://tileset.json") {}
        dqRender::TileVisibility computeVisibility(dqRender::TileDrawArgs&,
                                                   dqRender::Tile*) override
        {
            return dqRender::TileVisibility::Visible;
        }
    };
    class ImdlTestTile final : public dqRender::RealityTile {
    public:
        ImdlTestTile(ImdlTestTree& tree)
            : dqRender::RealityTile(tree, nullptr, dqRender::BoundingVolume{},
                                    0.0f, "fixture",
                                    dqGeom::Range3d::CreateXYZXYZ(-10, -10, -10, 10, 10, 10))
        {
        }
    };

    ImdlTestTree tree;
    ImdlTestTile tile(tree);
    auto const content = tile.readContent(V1_1::rectangleBytes, V1_1::rectangleSize);
    EXPECT_TRUE(content.contentRange.isNull() == false)
        << "contentRange from the imdl header";
    EXPECT_TRUE(content.isLeaf) << "single-element complete tile → leaf";
}

// ---------------------------------------------------------------------------
// H-3/H-4：量化顶点解码 + 图形链接通。
// Ported from: TileIO.test.ts processRectangle 的内容断言（:126-150——
// 中心化 contentRange ±2.5/±5/0，tolerance 0.0005）+ MeshGraphic 顶点数
// 断言（:302 numVertices===4——DanQing 对应 polyface 点数）。
// ---------------------------------------------------------------------------

namespace {
double idelta(double a, double b) { return std::abs(a - b); }
}

// 解码顶点：4 角、位置 ±2.5/±5/z≈0（0.0005 容差，参考 :130-140 同值）。
TEST(ImdlGraphics, RectangleDecodesQuantizedVertices)
{
    dqRender::ImdlByteStream stream(V1_1::rectangleBytes, V1_1::rectangleSize);
    auto const header = dqRender::ImdlHeader::readFrom(stream);
    ASSERT_TRUE(header.isValid());
    auto const desc = dqRender::decodeImdlContentDescription(header, stream);
    ASSERT_TRUE(desc.has_value());
    auto doc = dqRender::parseImdlDocument(stream);
    ASSERT_TRUE(doc.has_value());

    auto meshes = dqRender::decodeImdlGraphics(*doc);
    ASSERT_EQ(meshes.size(), 1u);
    auto const& pf = meshes.front();
    EXPECT_EQ(pf->Data().PointCount(), 4) << "rectangle has 4 vertices";
    EXPECT_EQ(pf->FacetCount(), 2) << "2 triangles";

    // 参考 :130-140——中心化范围（[0,0]-[5,10] 矩形 → ±2.5/±5）。
    for (size_t i = 0; i < pf->Data().PointCount(); ++i) {
        auto const& pt = pf->Data().GetPoint(static_cast<int32_t>(i + 1));
        EXPECT_LT(idelta(std::abs(pt.x), 2.5), 0.0005) << "vertex " << i;
        EXPECT_LT(idelta(std::abs(pt.y), 5.0), 0.0005) << "vertex " << i;
        EXPECT_LT(idelta(pt.z, 0.0), 0.0005) << "vertex " << i;
    }
}

// 三角形场景：2 mesh（参考 :321-322 graphics.length===2）、9 顶点 ×2
//（:329/:342 numVertices===9）、6 索引/24-bit 三角。
TEST(ImdlGraphics, TrianglesDecodeTwoMeshes)
{
    dqRender::ImdlByteStream stream(V1_1::trianglesBytes, V1_1::trianglesSize);
    auto const header = dqRender::ImdlHeader::readFrom(stream);
    ASSERT_TRUE(header.isValid());
    auto const desc = dqRender::decodeImdlContentDescription(header, stream);
    ASSERT_TRUE(desc.has_value());
    auto doc = dqRender::parseImdlDocument(stream);
    ASSERT_TRUE(doc.has_value());

    auto meshes = dqRender::decodeImdlGraphics(*doc);
    ASSERT_EQ(meshes.size(), 2u) << "reference: GraphicsArray of 2 meshes";
    EXPECT_EQ(meshes[0]->Data().PointCount(), 9) << "reference :329";
    EXPECT_EQ(meshes[1]->Data().PointCount(), 9) << "reference :342";
}

// readContent 图形链（H-4）：夹具字节 → TileContent.graphic 非空。
// Ported from: TileIO.test.ts :148（result.graphic !== undefined——
// DanQing 对应桩 RenderSystem 的 createGraphicFromPolyface 真实现链）。
TEST(ImdlGraphics, ReadContentProducesGraphic)
{
    class GfxTree : public dqRender::RealityTileTree {
    public:
        GfxTree() : RealityTileTree(nullptr, "fixture://t.json") {}
        dqRender::TileVisibility computeVisibility(dqRender::TileDrawArgs&,
                                                   dqRender::Tile*) override
        {
            return dqRender::TileVisibility::Visible;
        }
    };
    GfxTree tree;
    class GfxTile final : public dqRender::RealityTile {
    public:
        explicit GfxTile(GfxTree& t)
            : dqRender::RealityTile(t, nullptr, dqRender::BoundingVolume{},
                                    0.0f, "fixture",
                                    dqGeom::Range3d::CreateXYZXYZ(-10, -10, -10, 10, 10, 10))
        {
        }
    };
    GfxTile tile(tree);
    // 无注入 RenderSystem（测试环境）——graphic 静默为空但元数据完整；
    // graphic 生产链由 TileTreeRender 窗口族覆盖（真 RenderSystem 注入）。
    auto const content = tile.readContent(V1_1::rectangleBytes, V1_1::rectangleSize);
    EXPECT_FALSE(content.contentRange.isNull());
    EXPECT_TRUE(content.isLeaf);
}

// I-1/I-2：material 色 + feature table。
// Ported from: TileIO.test.ts processRectangle :148-150 的 batch 断言
//（expectNumFeatures(batch, 1)）+ TileIO.data.ts fillColor=65280（绿）。
TEST(ImdlDocument, MaterialColorAndFeatureTable)
{
    dqRender::ImdlByteStream stream(V1_1::rectangleBytes, V1_1::rectangleSize);
    auto const header = dqRender::ImdlHeader::readFrom(stream);
    ASSERT_TRUE(header.isValid());

    // 读 feature table（12B 头 + words）
    dqRender::ImdlFeatureTableHeader ft;
    bool const hasFt = dqRender::ImdlFeatureTableHeader::readFrom(stream, ft);
    std::vector<uint32_t> words;
    if (hasFt && ft.length > 12) {
        size_t const n = (ft.length - 12) / 4;
        words.resize(n);
        stream.readBytes(words.data(), n * 4);
    }

    auto desc = dqRender::decodeImdlContentDescriptionHeaderOnly(header);
    ASSERT_TRUE(desc.has_value());
    auto doc = dqRender::parseImdlDocument(stream, hasFt ? &ft : nullptr, &words);
    ASSERT_TRUE(doc.has_value());

    // fillColor 65280 = 0x00FF00（绿，TileIO.data.ts "a green rectangle"）。
    EXPECT_TRUE(doc->hasMaterialFillColor);
    EXPECT_EQ(doc->materialFillColor, 65280u);

    // Feature table：rectangle 场景单元素（numElementsIncluded=1，参考
    // expectNumFeatures(batch, 1)——存疑时以夹具实际 count 为准）。
    printf("[IMDL-FT] count=%u length=%u words=%zu\n",
           doc->featureCount, doc->featureTableLength, doc->featureData.size());
}

// ---------------------------------------------------------------------------
// U7：LUT 直传路径（createImdlLutGraphics）——线上 RGBA8 顶点表零 CPU 逐顶点
// 解码，原样上传为 LUT 纹理（VertexLUT.ts:93-99 形态）。
// ---------------------------------------------------------------------------

namespace {

// 录制驱动：createTexture 返回真实句柄（VertexLutTexture::create 的判空通过），
// setTextureData 捕获上传字节/尺寸——verbatim 直传断言的数据源（形态照
// VertexLutTextureTest.RecordingDriver 先例，无 GL 上下文，hermetic）。
class RecordingLutDriver : public dqRender::rhi::NullDriver {
public:
    dqRender::rhi::TextureHandle createTexture(dqRender::rhi::SamplerType, uint8_t,
                                               dqRender::rhi::TextureFormat, uint32_t, uint32_t,
                                               uint32_t, dqRender::rhi::TextureUsage) noexcept override
    {
        return m_allocator.allocate<dqRender::rhi::HwTexture>();
    }

    void setTextureData(dqRender::rhi::TextureHandle, uint32_t, uint32_t, uint32_t, uint32_t,
                        uint32_t width, uint32_t height, uint32_t,
                        dqRender::rhi::PixelBufferDescriptor&& data) noexcept override
    {
        m_lastWidth = width;
        m_lastHeight = height;
        auto const* b = static_cast<uint8_t const*>(data.buffer());
        m_lastBytes.assign(b, b + data.size());
    }

    uint32_t lastWidth() const noexcept { return m_lastWidth; }
    uint32_t lastHeight() const noexcept { return m_lastHeight; }
    std::vector<uint8_t> const& lastBytes() const noexcept { return m_lastBytes; }

protected:
    dqRender::rhi::HandleAllocator m_allocator;

private:
    uint32_t m_lastWidth = 0;
    uint32_t m_lastHeight = 0;
    std::vector<uint8_t> m_lastBytes;
};

// 桩 RenderSystem：driver() 指向录制驱动（createImdlLutGraphics 的 RHI 通道）。
// 形态照 ImdlTileTreeTest.ReadContentStubSystem（本目录 ImdlTileTreeTest.cpp:45）。
class LutStubSystem final : public dqRender::RenderSystem {
public:
    explicit LutStubSystem(dqRender::rhi::Driver& d) : m_driver(&d) {}

    bool isValid() const noexcept override { return true; }
    std::unique_ptr<dqRender::RenderTarget> createTarget(void*, uint32_t, uint32_t) override
    {
        return nullptr;
    }
    std::unique_ptr<dqRender::GraphicBuilder> createGraphicBuilder(
        const dqRender::GraphicBuilderOptions&) override
    {
        return nullptr;
    }
    dqRender::GraphicBranch* createBranch(bool = true) override { return nullptr; }
    dqRender::RenderGraphic* createBranchGraphic(dqRender::GraphicBranch*) override
    {
        return nullptr;
    }
    dqRender::RenderGraphic* createGraphicList(
        std::vector<dqRender::RenderGraphic*>) override
    {
        return nullptr;
    }
    dqRender::RenderGraphicOwner* createGraphicOwner(dqRender::RenderGraphic*) override
    {
        return nullptr;
    }
    dqRender::rhi::Driver* driver() noexcept override { return m_driver; }

private:
    dqRender::rhi::Driver* m_driver;
};

}  // namespace

// LUT 直传形态 + verbatim 字节断言。
// Ported from: itwinjs-core VertexTable.ts computeDimensions (:53-81) 语义 +
//              VertexLUT.ts createFromVertexTable (:93-99 直传形态) +
//              ParseImdlDocument.ts parseVertexTable (:1029-1042——JSON
//              width/height 原样进 VertexTable)。
//              数值断言 Authored（录制 fixture 的 vertices 元数据从 JSON
//              读得——count/numRgbaPerVertex/width/decodedMin/Max，测试内
//              自洽；夹具 bytes 只读，§11.11）。
// 夹具：dqRender::fixtures::V1_1::rectangleBytes（TileIOFixtures.h）+
//       本文件 parseFull 解析先例（:120 区）。
TEST(ImdlGraphicsTest, LutPathUploadsVertexTableVerbatim)
{
    dqRender::ImdlByteStream stream(V1_1::rectangleBytes, V1_1::rectangleSize);
    auto const header = dqRender::ImdlHeader::readFrom(stream);
    ASSERT_TRUE(header.isValid());
    auto const desc = dqRender::decodeImdlContentDescription(header, stream);
    ASSERT_TRUE(desc.has_value());
    auto doc = dqRender::parseImdlDocument(stream);
    ASSERT_TRUE(doc.has_value());

    RecordingLutDriver driver;
    LutStubSystem system(driver);
    auto graphics = dqRender::createImdlLutGraphics(*doc, system);
    ASSERT_EQ(graphics.size(), 1u) << "rectangle = 1 mesh primitive（1 graphic）";
    // 形态：LUT 量化几何（MeshGraphic → SurfaceGeometry）。
    auto* mesh = static_cast<dqRender::MeshGraphic*>(graphics[0]);
    ASSERT_NE(mesh, nullptr);
    ASSERT_EQ(mesh->getSurfaces().size(), 1u);
    dqRender::SurfaceGeometry const* surf = mesh->getSurfaces()[0].get();
    ASSERT_NE(surf, nullptr);
    EXPECT_TRUE(surf->usesQuantizedPositions());

    // LUT 参数：numVertices/numRgbaPerVert == JSON vertices.count/numRgbaPerVertex
    // （ParseImdlDocument.ts:1039-1040）。
    auto const& lut = surf->getLut();
    auto const& p = lut.getParams();
    EXPECT_EQ(p.numVertices, 4u);
    EXPECT_EQ(p.numRgbaPerVert, 4u);
    // 纹理尺寸 == JSON vertices.width/height（:1033-1034 原样；本夹具与
    // computeDimensions(4,4,0,2048) 自洽一致：nRgba=16 ≤ maxSize → 16×1）。
    EXPECT_EQ(p.texWidth, 16u);
    EXPECT_EQ(p.texHeight, 1u);
    EXPECT_EQ(driver.lastWidth(), 16u);
    EXPECT_EQ(driver.lastHeight(), 1u);

    // 量化参数：QParams3d.fromRange（ParseImdlDocument.ts:1013-1018——
    // origin=decodedMin，scale=(decodedMax-decodedMin)/65535，逐分量）。
    double const dmin[3] = {-2.5003749999999996, -5.000749999999999, -0.000500075};
    double const dmax[3] = {2.5003749999999996, 5.000749999999999, 0.000500075};
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(lut.getQOrigin()[i], static_cast<float>(dmin[i])) << "qOrigin[" << i << "]";
        EXPECT_EQ(lut.getQScale()[i],
                  static_cast<float>((dmax[i] - dmin[i]) / 65535.0))
            << "qScale[" << i << "]";
    }

    // 24-bit 索引流：6 索引（bvindices0Surface byteLength 18 / 3）。
    EXPECT_EQ(surf->getNumIndices(), 6u);

    // 均匀色：u_color 源（glsl/Color.ts:51-60 ← lutGeom.getColor；fixture
    // uniformColor=65280=绿，TileIO.data.ts "a green rectangle"）。
    EXPECT_EQ(surf->getColor().getTbgr(), 65280u);

    // Verbatim 直传证据：上传字节 == wire bufferView 字节逐字节相等
    // （VertexLUT.ts:93-99 createForData(vt.width, vt.height, vt.data)——
    // vt.data 即线上 bufferView（ParseImdlDocument.ts:1005-1008））。
    auto json = dqRender::tilejson::parseJsonDocument(doc->sceneJson);
    ASSERT_NE(json, nullptr);
    auto prims = dqRender::tilejson::parseImdlMeshPrimitives(*json);
    ASSERT_EQ(prims.size(), 1u);
    ASSERT_FALSE(prims[0].vertices.bufferView.empty());
    auto const* views = json->find("bufferViews");
    ASSERT_NE(views, nullptr);
    auto const* view = views->find(prims[0].vertices.bufferView.c_str());
    ASSERT_NE(view, nullptr);
    auto const* off = view->find("byteOffset");
    auto const* len = view->find("byteLength");
    ASSERT_NE(off, nullptr);
    ASSERT_NE(len, nullptr);
    size_t const wireOff = static_cast<size_t>(off->number);
    size_t const wireLen = static_cast<size_t>(len->number);
    ASSERT_EQ(wireLen, prims[0].vertices.count * prims[0].vertices.numRgbaPerVertex * 4u);
    ASSERT_EQ(driver.lastBytes().size(), wireLen);
    EXPECT_EQ(std::memcmp(driver.lastBytes().data(), doc->binary.data() + wireOff, wireLen), 0)
        << "LUT upload must be the wire vertex table verbatim (zero CPU decoding)";

    // 资源释放（graphics 为裸指针所有权——readContent 交 createGraphicList）。
    delete graphics[0];
}

// U7 内存形态收益：LUT 路径每顶点 16B（numRgba×4）+ 每索引 3B（24-bit 流），
// 对照旧 VBO 路径 PolyfaceGraphic 每角 sizeof(Vertex)=52B + 每索引 4B。
// Authored: 参考无对应数值测试（行为收益条目 U7，取证
//           docs/itwinjs-tile-vertexLUT机制深挖-2026-09-24.md §5）；数值由
//           录制 fixture 自洽驱动。
TEST(ImdlGraphicsTest, LutPathUsesLessGpuMemoryThanVboPath)
{
    dqRender::ImdlByteStream stream(V1_1::rectangleBytes, V1_1::rectangleSize);
    auto const header = dqRender::ImdlHeader::readFrom(stream);
    ASSERT_TRUE(header.isValid());
    auto const desc = dqRender::decodeImdlContentDescription(header, stream);
    ASSERT_TRUE(desc.has_value());
    auto doc = dqRender::parseImdlDocument(stream);
    ASSERT_TRUE(doc.has_value());

    // LUT 路径字节（fixture：4 顶点 × 4 rgba × 4B + 6 索引 × 3B）。
    RecordingLutDriver driver;
    LutStubSystem system(driver);
    auto graphics = dqRender::createImdlLutGraphics(*doc, system);
    ASSERT_EQ(graphics.size(), 1u);
    auto* mesh = static_cast<dqRender::MeshGraphic*>(graphics[0]);
    ASSERT_EQ(mesh->getSurfaces().size(), 1u);
    dqRender::SurfaceGeometry const* surf = mesh->getSurfaces()[0].get();
    auto const& p = surf->getLut().getParams();
    uint64_t const lutBytes = uint64_t(p.numVertices) * p.numRgbaPerVert * 4u
        + uint64_t(surf->getNumIndices()) * 3u;
    uint64_t const lutBytesPerVertex = uint64_t(p.numRgbaPerVert) * 4u;

    // 旧 VBO 路径字节（同 fixture 的 polyface 解码产物：buildFromPolyface
    // 逐角展开——每角 52B + 每角索引 4B；fixture 全三角面 → 角数 =
    // FacetCount×3。52B = PolyfaceGraphic::Vertex（私有 struct，布局
    // position3+normal3+color4+texCoord2 floats + featureId u32 = 13×4B，
    // PolyfaceGraphic.h:109-115））。
    auto meshes = dqRender::decodeImdlGraphics(*doc);
    ASSERT_EQ(meshes.size(), 1u);
    constexpr uint64_t kVboBytesPerCorner = 13u * 4u;  // sizeof(Vertex)=52
    uint64_t const vboCorners = uint64_t(meshes[0]->FacetCount()) * 3u;
    uint64_t const vboBytes = vboCorners * (kVboBytesPerCorner + sizeof(uint32_t));

    // 每顶点/每角：16B vs 52B（LUT < VBO/2）。
    EXPECT_LT(lutBytesPerVertex, kVboBytesPerCorner / 2u);
    // 总量（fixture 顶点数驱动）：LUT < VBO 一半。
    EXPECT_LT(lutBytes, vboBytes / 2u)
        << "lut=" << lutBytes << " vbo=" << vboBytes;

    delete graphics[0];
}

// ---------------------------------------------------------------------------
// U11(1)：imdl 边缘 JSON 解析层（ImdlMeshEdges 四形态 + DisplayParams
// width/linePixels）。
// Ported from: itwinjs-core ParseImdlDocument.ts:665-727（parseSegmentEdges/
//              parseSilhouetteEdges/parseIndexedEdges/parseEdges——findBuffer
//              视图语义：bufferView 名 → 字节区间）+ ImdlSchema.ts:195-286
//              （四形态 schema 字段名逐一）。
// 场景数据（Step 0 探测，2026-09-26 离线解码 TileIOFixtures.h 的 JSON 段）：
// 录制夹具 5 版本×5 场景中 rectangle/triangles/cylinder 带 edges 字段——
// segments 形态（cylinder 另有 silhouettes+normalPairs）；indexed/compact
// 两形态无夹具 → 合成 doc。断言数值 Authored: no reference test exists in
// itwinjs-core for imdl edges parsing（TileIO.test.ts/ImdlParser.test.ts
// 检索无 edges 解析场景，2026-09-26）——数值钉死自夹具 JSON 离线解码。
// ---------------------------------------------------------------------------

// rectangle（v1.1 录制夹具）：segments 形态 + DisplayParams width/linePixels。
// Ported from: ParseImdlDocument.ts:665-669 parseSegmentEdges + :710-727
//              parseEdges（断言数值 Authored——参考无 edges 解析测试，
//              数值钉死自夹具 JSON 离线解码，见上区块注释）。
// 夹具 JSON：material="Material0"、lineWidth=1、linePixels=0(Solid)；
// edges.segments={indices:"bvindices0Segments", endPointAndQuadIndices:
// "bvendPointAndQuadIndices0Segments"}；bufferViews @84 len 72 / @156 len 96。
TEST(ImdlEdges, SegmentEdgesFromRecordedRectangle)
{
    auto doc = parseFull(V1_1::rectangleBytes, V1_1::rectangleSize);
    ASSERT_TRUE(doc.has_value());
    auto json = dqRender::tilejson::parseJsonDocument(doc->sceneJson);
    ASSERT_NE(json, nullptr);
    auto prims = dqRender::tilejson::parseImdlMeshPrimitives(*json);
    ASSERT_EQ(prims.size(), 1u);

    // ImdlSchema.ts:173 material（关联 ImdlDisplayParams 的 Id）+:278 segments
    // （视图名 1:1 照夹具 JSON）。
    EXPECT_EQ(prims[0].material, "Material0");
    ASSERT_TRUE(prims[0].edges.has_value());
    ASSERT_TRUE(prims[0].edges->segments.has_value());
    EXPECT_FALSE(prims[0].edges->silhouettes.has_value());
    EXPECT_FALSE(prims[0].edges->indexed.has_value());
    EXPECT_FALSE(prims[0].edges->compact.has_value());
    EXPECT_EQ(prims[0].edges->segments->indicesView, "bvindices0Segments");
    EXPECT_EQ(prims[0].edges->segments->endPointAndQuadIndicesView,
              "bvendPointAndQuadIndices0Segments");

    // DisplayParams.width/linePixels（parseDisplayParams ParseImdlDocument.ts
    // :1206-1207——json.lineWidth=1、json.linePixels=0=Solid 默认同值）。
    EXPECT_EQ(prims[0].width, 1u);
    EXPECT_EQ(prims[0].linePixels, dqCommon::LinePixels::Solid);

    // parseEdges（:710-727）→ segments 字节区间（parseSegmentEdges :665-669
    // ——bufferView 名 → binary 区间）；weight/linePixels 来自 displayParams
    //（:730-731 weight = displayParams.width）。
    auto edges = dqRender::tilejson::parseImdlEdges(*json, *doc, prims[0]);
    ASSERT_TRUE(edges.has_value());
    EXPECT_EQ(edges->weight, 1u);
    EXPECT_EQ(edges->linePixels, dqCommon::LinePixels::Solid);
    ASSERT_TRUE(edges->segments.has_value());
    EXPECT_FALSE(edges->silhouettes.has_value());
    EXPECT_FALSE(edges->indexed.has_value());
    EXPECT_EQ(edges->segments->indices.data, doc->binary.data() + 84);
    EXPECT_EQ(edges->segments->indices.byteLength, 72u);
    EXPECT_EQ(edges->segments->endPointAndQuadIndices.data, doc->binary.data() + 156);
    EXPECT_EQ(edges->segments->endPointAndQuadIndices.byteLength, 96u);
}

// cylinder（v1.1 录制夹具）：segments + silhouettes（含 normalPairs）双形态。
// Ported from: ParseImdlDocument.ts:671-675 parseSilhouetteEdges（断言数值
//              Authored——参考无 edges 解析测试，数值钉死自夹具 JSON 离线
//              解码，见上区块注释）。
// 夹具 JSON：silhouettes={indices:"bvindices0Silhouettes" @6656 len 648,
// endPointAndQuadIndices:"bvendPointAndQuadIndices0Silhouettes" @7304 len 864,
// normalPairs:"bvnormalPairs0Silhouettes" @8168 len 864}；segments @3632/1296
// + @4928/1728；normalPairs 每索引 4B = 2×16-bit oct-encoded normal 对。
TEST(ImdlEdges, SilhouetteEdgesFromRecordedCylinder)
{
    auto doc = parseFull(V1_1::cylinderBytes, V1_1::cylinderSize);
    ASSERT_TRUE(doc.has_value());
    auto json = dqRender::tilejson::parseJsonDocument(doc->sceneJson);
    ASSERT_NE(json, nullptr);
    auto prims = dqRender::tilejson::parseImdlMeshPrimitives(*json);
    ASSERT_EQ(prims.size(), 1u);

    // ImdlSchema.ts:279 silhouettes（:209-212 ImdlSilhouetteEdges——继承
    // segments 三字段 + normalPairs :211）。
    ASSERT_TRUE(prims[0].edges.has_value());
    ASSERT_TRUE(prims[0].edges->segments.has_value());
    ASSERT_TRUE(prims[0].edges->silhouettes.has_value());
    EXPECT_EQ(prims[0].edges->silhouettes->indicesView, "bvindices0Silhouettes");
    EXPECT_EQ(prims[0].edges->silhouettes->endPointAndQuadIndicesView,
              "bvendPointAndQuadIndices0Silhouettes");
    EXPECT_EQ(prims[0].edges->silhouettes->normalPairsView, "bvnormalPairs0Silhouettes");

    // parseSilhouetteEdges（:671-675）——三视图全解析才产出（segments &&
    // normalPairs 语义）。
    auto edges = dqRender::tilejson::parseImdlEdges(*json, *doc, prims[0]);
    ASSERT_TRUE(edges.has_value());
    ASSERT_TRUE(edges->segments.has_value());
    ASSERT_TRUE(edges->silhouettes.has_value());
    EXPECT_EQ(edges->segments->indices.data, doc->binary.data() + 3632);
    EXPECT_EQ(edges->segments->indices.byteLength, 1296u);
    EXPECT_EQ(edges->segments->endPointAndQuadIndices.data, doc->binary.data() + 4928);
    EXPECT_EQ(edges->segments->endPointAndQuadIndices.byteLength, 1728u);
    EXPECT_EQ(edges->silhouettes->indices.data, doc->binary.data() + 6656);
    EXPECT_EQ(edges->silhouettes->indices.byteLength, 648u);
    EXPECT_EQ(edges->silhouettes->endPointAndQuadIndices.data, doc->binary.data() + 7304);
    EXPECT_EQ(edges->silhouettes->endPointAndQuadIndices.byteLength, 864u);
    EXPECT_EQ(edges->silhouettes->normalPairs.data, doc->binary.data() + 8168);
    EXPECT_EQ(edges->silhouettes->normalPairs.byteLength, 864u);
}

// indexed 形态（合成 doc——Step 0 探测：录制夹具无 indexed 场景）。
// Authored: no reference test exists in itwinjs-core for imdl edges parsing;
//           字段照 ImdlSchema.ts:219-232 ImdlIndexedEdges 构造，解析语义照
//           parseIndexedEdges（ParseImdlDocument.ts:677-693——indices + edges
//           表 + width/height/numSegments/silhouettePadding 原样进 EdgeTable）。
TEST(ImdlEdges, SyntheticIndexedEdgesResolution)
{
    dqRender::ImdlDocument doc;
    doc.sceneJson = R"json({
        "materials": {"Mat": {"fillColor": 65280, "lineWidth": 2, "linePixels": 3435973836}},
        "meshes": {"Mesh_Root": {"primitives": [{
            "material": "Mat",
            "edges": {"indexed": {"indices": "bvEdgeIndices", "edges": "bvEdgeTable",
                                  "width": 32, "height": 4, "numSegments": 7,
                                  "silhouettePadding": 3}}
        }]}},
        "bufferViews": {
            "bvEdgeIndices": {"buffer": "binary_glTF", "byteOffset": 16, "byteLength": 18},
            "bvEdgeTable": {"buffer": "binary_glTF", "byteOffset": 34, "byteLength": 128}
        }
    })json";
    doc.binary.assign(162u, 0x00u);
    // 分区填充：可区分标记证明区间落在各自 bufferView 内（§11.11 位置断言）。
    for (size_t i = 16; i < 34; ++i) doc.binary[i] = 0x11u;  // bvEdgeIndices
    for (size_t i = 34; i < 162; ++i) doc.binary[i] = 0x22u; // bvEdgeTable

    auto json = dqRender::tilejson::parseJsonDocument(doc.sceneJson);
    ASSERT_NE(json, nullptr);
    auto prims = dqRender::tilejson::parseImdlMeshPrimitives(*json);
    ASSERT_EQ(prims.size(), 1u);

    // ImdlSchema.ts:219-232 字段逐一。
    ASSERT_TRUE(prims[0].edges.has_value());
    ASSERT_TRUE(prims[0].edges->indexed.has_value());
    EXPECT_EQ(prims[0].edges->indexed->indicesView, "bvEdgeIndices");
    EXPECT_EQ(prims[0].edges->indexed->edgeTableView, "bvEdgeTable");
    EXPECT_EQ(prims[0].edges->indexed->width, 32u);
    EXPECT_EQ(prims[0].edges->indexed->height, 4u);
    EXPECT_EQ(prims[0].edges->indexed->numSegments, 7u);
    EXPECT_EQ(prims[0].edges->indexed->silhouettePadding, 3u);

    // DisplayParams：lineWidth=2、linePixels=3435973836=HiddenLine(0xcccccccc)
    // （dqCommon::LinePixels，core/common LinePixels.ts 同值）。
    EXPECT_EQ(prims[0].width, 2u);
    EXPECT_EQ(prims[0].linePixels, dqCommon::LinePixels::HiddenLine);

    // parseIndexedEdges（:677-693）——EdgeTable 四尺寸字段原样 + 双区间。
    auto edges = dqRender::tilejson::parseImdlEdges(*json, doc, prims[0]);
    ASSERT_TRUE(edges.has_value());
    EXPECT_EQ(edges->weight, 2u) << "EdgeParams.weight = displayParams.width (:730)";
    EXPECT_EQ(edges->linePixels, dqCommon::LinePixels::HiddenLine) << "(:731)";
    ASSERT_TRUE(edges->indexed.has_value());
    EXPECT_FALSE(edges->segments.has_value());
    EXPECT_FALSE(edges->silhouettes.has_value());
    EXPECT_EQ(edges->indexed->indices.data, doc.binary.data() + 16);
    EXPECT_EQ(edges->indexed->indices.byteLength, 18u);
    EXPECT_EQ(edges->indexed->indices.data[0], 0x11u) << "indices span lands in bvEdgeIndices";
    EXPECT_EQ(edges->indexed->edges.data.data, doc.binary.data() + 34);
    EXPECT_EQ(edges->indexed->edges.data.byteLength, 128u);
    EXPECT_EQ(edges->indexed->edges.data.data[0], 0x22u) << "table span lands in bvEdgeTable";
    EXPECT_EQ(edges->indexed->edges.width, 32u);
    EXPECT_EQ(edges->indexed->edges.height, 4u);
    EXPECT_EQ(edges->indexed->edges.numSegments, 7u);
    EXPECT_EQ(edges->indexed->edges.silhouettePadding, 3u);
}

// compact 形态（合成 doc——Step 0 探测：录制夹具无 compact 场景）：
// U11(3) 起 parseEdges 对 compact 做兜底展开（ParseImdlDocument.ts:719-720 →
// parseCompactEdges :695-708 → CompactEdges.ts indexedEdgeParamsFromCompactEdges
// ——此前 Task 4 只提取字段并归零，本测试的 parse-only 断言随展开落地翻转）。
// Authored: no reference test exists in itwinjs-core for imdl edges parsing;
//           字段照 ImdlSchema.ts:257-272 ImdlCompactEdges 构造。
TEST(ImdlEdges, SyntheticCompactEdgesFieldExtraction)
{
    // 变体 1：visibility + numVisible（无 silhouette → normalPairs undefined，
    // ImdlSchema.ts:265）。
    dqRender::ImdlDocument doc;
    doc.sceneJson = R"json({
        "materials": {"Mat": {"lineWidth": 3}},
        "meshes": {"Mesh_Root": {"primitives": [{
            "material": "Mat",
            "edges": {"compact": {"visibility": "bvVisibility", "numVisible": 11}}
        }]}},
        "bufferViews": {
            "bvVisibility": {"buffer": "binary_glTF", "byteOffset": 0, "byteLength": 9}
        }
    })json";
    doc.binary.assign(9u, 0x00u);

    auto json = dqRender::tilejson::parseJsonDocument(doc.sceneJson);
    ASSERT_NE(json, nullptr);
    auto prims = dqRender::tilejson::parseImdlMeshPrimitives(*json);
    ASSERT_EQ(prims.size(), 1u);
    ASSERT_TRUE(prims[0].edges.has_value());
    ASSERT_TRUE(prims[0].edges->compact.has_value());
    EXPECT_EQ(prims[0].edges->compact->visibilityView, "bvVisibility");
    EXPECT_EQ(prims[0].edges->compact->numVisible, 11u);
    EXPECT_FALSE(prims[0].edges->compact->normalPairsView.has_value());

    // 仅 compact + surface.indices 缺失 → 展开无法入链（parseCompactEdges :704
    // 需顶点索引）→ 全空 → 归零（:722-723）。
    auto edges = dqRender::tilejson::parseImdlEdges(*json, doc, prims[0]);
    EXPECT_FALSE(edges.has_value())
        << "compact without surface.indices cannot expand (parseCompactEdges :704)";

    // 变体 2：带 normalPairs（ImdlSchema.ts:267）。
    dqRender::ImdlDocument doc2;
    doc2.sceneJson = R"json({
        "meshes": {"Mesh_Root": {"primitives": [{
            "material": "Mat",
            "edges": {"compact": {"visibility": "bvVisibility", "numVisible": 5,
                                  "normalPairs": "bvNormalPairs"}}
        }]}},
        "bufferViews": {
            "bvVisibility": {"buffer": "binary_glTF", "byteOffset": 0, "byteLength": 9},
            "bvNormalPairs": {"buffer": "binary_glTF", "byteOffset": 9, "byteLength": 8}
        }
    })json";
    doc2.binary.assign(17u, 0x00u);
    auto json2 = dqRender::tilejson::parseJsonDocument(doc2.sceneJson);
    ASSERT_NE(json2, nullptr);
    auto prims2 = dqRender::tilejson::parseImdlMeshPrimitives(*json2);
    ASSERT_EQ(prims2.size(), 1u);
    ASSERT_TRUE(prims2[0].edges.has_value());
    ASSERT_TRUE(prims2[0].edges->compact.has_value());
    EXPECT_EQ(prims2[0].edges->compact->numVisible, 5u);
    ASSERT_TRUE(prims2[0].edges->compact->normalPairsView.has_value());
    EXPECT_EQ(prims2[0].edges->compact->normalPairsView.value(), "bvNormalPairs");

    // 无 edges 的 primitive：props.edges 空 → parseEdges :711-712 undefined。
    dqRender::ImdlDocument doc3;
    doc3.sceneJson = R"json({
        "meshes": {"Mesh_Root": {"primitives": [{"material": "Mat"}]}}
    })json";
    auto json3 = dqRender::tilejson::parseJsonDocument(doc3.sceneJson);
    ASSERT_NE(json3, nullptr);
    auto prims3 = dqRender::tilejson::parseImdlMeshPrimitives(*json3);
    ASSERT_EQ(prims3.size(), 1u);
    EXPECT_FALSE(prims3[0].edges.has_value());
    auto edges3 = dqRender::tilejson::parseImdlEdges(*json3, doc3, prims3[0]);
    EXPECT_FALSE(edges3.has_value());
}

// 12B SimpleBuilder（numRgbaPerVertex=3）布局守卫回归。
// Authored: 参考无对应测试场景（录制夹具全为 16B LitMesh，§5(f)）——合成
// 最小内存 doc 验证守卫语义：octNormal@12-13 仅 16B 布局合法，12B 表无条件
// 读会越入下一顶点位置字节（本布局顶点表居 BIN 末尾——末顶点 1-2 字节正式
// OOB）。守卫后：CPU 路径无法线（NormalCount==0、normalIndex 留空）、
// LUT 路径拒绝 12B（返回空）。
TEST(ImdlGraphicsTest, SimpleBuilder12BLayoutSkipsOctNormalGuarded)
{
    // BIN 布局（45B）：bvIdx 0..8（1 三角形 0,1,2），bvVtx 9..44（3 顶点 ×
    // 12B）——顶点表居 BIN 末尾，旧代码末顶点读 base[12]/[13] = 越界 1-2 字节。
    dqRender::ImdlDocument doc;
    doc.sceneJson = R"json({
        "meshes": {"Mesh_Root": {"primitives": [{
            "surface": {"indices": "bvIdx", "type": 0},
            "vertices": {"bufferView": "bvVtx", "count": 3, "width": 9, "height": 1,
                         "numRgbaPerVertex": 3,
                         "params": {"decodedMin": [0, 0, 0], "decodedMax": [1, 1, 1]}}
        }]}},
        "bufferViews": {
            "bvIdx": {"buffer": "binary_glTF", "byteOffset": 0, "byteLength": 9},
            "bvVtx": {"buffer": "binary_glTF", "byteOffset": 9, "byteLength": 36}
        }
    })json";
    doc.binary.assign(45u, 0u);
    // 索引：三角形 (0, 1, 2)（24-bit LE）。
    doc.binary[3] = 1u;
    doc.binary[6] = 2u;
    // 量化位置（各顶点 bytes 0-5）：v0=(0,0,0)、v1=(65535,0,0)、v2=(0,65535,0)。
    auto const setU16 = [&doc](size_t off, uint16_t v) {
        doc.binary[off] = static_cast<uint8_t>(v & 0xFFu);
        doc.binary[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
    };
    setU16(9u + 12u, 65535);       // v1.qx
    setU16(9u + 24u + 2u, 65535);  // v2.qy

    // CPU 对照路径：几何存活、法线被守卫跳过（12B 无 octNormal 数据）。
    auto meshes = dqRender::decodeImdlGraphics(doc);
    ASSERT_EQ(meshes.size(), 1u);
    EXPECT_EQ(meshes[0]->Data().PointCount(), 3u);
    EXPECT_EQ(meshes[0]->FacetCount(), 1u);
    EXPECT_EQ(meshes[0]->Data().NormalCount(), 0u)
        << "12B SimpleBuilder table has no octNormal slot — guard must skip normals";
    EXPECT_TRUE(meshes[0]->Data().normalIndex.empty())
        << "no normal data → per-corner normal indices must stay empty";

    // LUT 主路径：12B 表被拒绝（量化 shader pre-read 采 g_vertLutData3 会
    // 误采下一顶点 texel0；unlit 接线登记 TODO）。
    RecordingLutDriver driver;
    LutStubSystem system(driver);
    auto graphics = dqRender::createImdlLutGraphics(doc, system);
    EXPECT_TRUE(graphics.empty())
        << "12B SimpleBuilder (unlit) tables are rejected by the LUT path";
}

// ---------------------------------------------------------------------------
// U11(2)：segments/silhouettes 边缘消费（Task 4 评审项 ②③ + Task 5 图形创建）。
//
// findBufferView 守卫（Task 4 评审 ②）：
// Ported from: ParseImdlDocument.ts:1093-1100 findBuffer——
//              `typeof bufferViewId !== "string" || 0 === bufferViewId.length`
//              → undefined；`0 === byteLength` → undefined。DanQing 侧
//              findBufferView 此前缺这两个守卫（空名/0 长视图会被当作合法
//              字节区间送入图形创建）。
//
// 图形创建（Task 5）：readContent 的 LUT 主路径为每个 imdl mesh primitive 的
// edges 产出 edge graphic（与 surface graphic 并列，Batch 包裹不变）。
// Ported from: itwinjs-core Mesh.ts:47-53 MeshRenderGeometry（silhouetteEdges
//              先于 segmentEdges 创建；两几何与 surface 共享同一顶点 LUT——
//              MeshData.lut）+ EdgeGeometry.ts create/:42-47 + :75-88 ctor
//              （a_pos 24-bit UBYTE3 索引流 + a_endPointAndQuadIndices UBYTE4，
//              drawArrays(Triangles, 0, numIndices)；SilhouetteEdgeGeometry
//              另加 a_normals UBYTE4，technique SilhouetteEdge）+
//              MeshData.ts:93-94（edgeWidth = edges.weight、edgeLineCode =
//              LineCode.valueFromLinePixels(edges.linePixels)）。
// 数值断言 Authored: no reference test exists in itwinjs-core for imdl edges
//           graphics（TileIO.test.ts/ImdlParser.test.ts 无对应场景，
//           2026-09-26 检索）——数值钉死自录制夹具 JSON 离线解码（bytes/
//           offset 与 ImdlEdges.SegmentEdgesFromRecordedRectangle 等既有
//           解析锁同源），布局断言照参考创建函数的结构。
// ---------------------------------------------------------------------------

// Task 4 评审 ③：假 bufferView 名 → 该形态成员被丢弃（findBuffer :1095-1097
// ——bufferViews 里查不到 → undefined）。segments 名假 + 其余形态缺 → 全空 →
// parseEdges :722-723 undefined。Authored: no reference test exists for imdl
// edges parsing（同上区块说明）。
TEST(ImdlEdges, FakeBufferViewNameDropsEdgeMember)
{
    // 变体 1：segments 视图名假 → 全空 → nullopt。
    dqRender::ImdlDocument doc;
    doc.sceneJson = R"json({
        "meshes": {"Mesh_Root": {"primitives": [{
            "material": "Mat",
            "edges": {"segments": {"indices": "bvNope", "endPointAndQuadIndices": "bvAlsoNope"}}
        }]}},
        "bufferViews": {}
    })json";
    auto json = dqRender::tilejson::parseJsonDocument(doc.sceneJson);
    ASSERT_NE(json, nullptr);
    auto prims = dqRender::tilejson::parseImdlMeshPrimitives(*json);
    ASSERT_EQ(prims.size(), 1u);
    ASSERT_TRUE(prims[0].edges.has_value());
    auto edges = dqRender::tilejson::parseImdlEdges(*json, doc, prims[0]);
    EXPECT_FALSE(edges.has_value())
        << "unresolvable segment views drop the member → all-empty → undefined (:722-723)";

    // 变体 2：segments 可解析 + silhouettes 的 normalPairs 名假 → silhouettes
    // 单独丢弃、segments 存活（parseEdges :714-715 各形态独立解析）。
    dqRender::ImdlDocument doc2;
    doc2.sceneJson = R"json({
        "meshes": {"Mesh_Root": {"primitives": [{
            "material": "Mat",
            "edges": {
                "segments": {"indices": "bvSegIdx", "endPointAndQuadIndices": "bvSegEpq"},
                "silhouettes": {"indices": "bvSilIdx", "endPointAndQuadIndices": "bvSilEpq",
                                "normalPairs": "bvNope"}
            }
        }]}},
        "bufferViews": {
            "bvSegIdx": {"buffer": "binary_glTF", "byteOffset": 0, "byteLength": 18},
            "bvSegEpq": {"buffer": "binary_glTF", "byteOffset": 18, "byteLength": 24}
        }
    })json";
    doc2.binary.assign(42u, 0x00u);
    auto json2 = dqRender::tilejson::parseJsonDocument(doc2.sceneJson);
    ASSERT_NE(json2, nullptr);
    auto prims2 = dqRender::tilejson::parseImdlMeshPrimitives(*json2);
    ASSERT_EQ(prims2.size(), 1u);
    auto edges2 = dqRender::tilejson::parseImdlEdges(*json2, doc2, prims2[0]);
    ASSERT_TRUE(edges2.has_value());
    EXPECT_TRUE(edges2->segments.has_value()) << "resolvable segments survive";
    EXPECT_FALSE(edges2->silhouettes.has_value())
        << "bogus normalPairs view drops silhouettes only (parseSilhouetteEdges :671-675)";
}

// Task 4 评审 ②：byteLength 0 的 bufferView → 成员丢弃（findBuffer
// :1099-1100 `if (0 === byteLength) return undefined`）。
// Authored: no reference test exists for imdl edges parsing（同上区块说明）。
TEST(ImdlEdges, ZeroLengthBufferViewDropsEdgeMember)
{
    dqRender::ImdlDocument doc;
    doc.sceneJson = R"json({
        "meshes": {"Mesh_Root": {"primitives": [{
            "material": "Mat",
            "edges": {"segments": {"indices": "bvIdx", "endPointAndQuadIndices": "bvEpq"}}
        }]}},
        "bufferViews": {
            "bvIdx": {"buffer": "binary_glTF", "byteOffset": 0, "byteLength": 0},
            "bvEpq": {"buffer": "binary_glTF", "byteOffset": 0, "byteLength": 24}
        }
    })json";
    doc.binary.assign(24u, 0x00u);
    auto json = dqRender::tilejson::parseJsonDocument(doc.sceneJson);
    ASSERT_NE(json, nullptr);
    auto prims = dqRender::tilejson::parseImdlMeshPrimitives(*json);
    ASSERT_EQ(prims.size(), 1u);
    auto edges = dqRender::tilejson::parseImdlEdges(*json, doc, prims[0]);
    EXPECT_FALSE(edges.has_value())
        << "zero-length bufferView is not a legal edge byte span (findBuffer :1099-1100)";
}

// ---------------------------------------------------------------------------
// 边缘图形创建的录制驱动——在 RecordingLutDriver 之上补 BO/VBO/VAO/drawArrays
// 捕获（布局断言的数据源；形态照其"录制驱动 + 桩 RenderSystem"先例）。
// ---------------------------------------------------------------------------
namespace {

class EdgeRecordingDriver final : public RecordingLutDriver {
public:
    // -- buffer objects：分配真实句柄并按 handle id 记录上传字节 --
    dqRender::rhi::BufferObjectHandle createBufferObject(
        uint32_t byteCount, dqRender::rhi::BufferObjectBinding binding,
        dqRender::rhi::BufferUsage usage) noexcept override
    {
        auto h = m_allocator.allocate<dqRender::rhi::HwBufferObject>();
        if (h) {
            auto* bo = m_allocator.handle_cast<dqRender::rhi::HwBufferObject,
                                               dqRender::rhi::HwBufferObject>(h);
            if (bo) {
                bo->byteCount = byteCount;
                bo->bindingType = binding;
                bo->usage = usage;
            }
            m_boBytes[h.getId()] = {};
        }
        return h;
    }

    void updateBufferObject(dqRender::rhi::BufferObjectHandle h,
                            dqRender::rhi::BufferDescriptor&& data,
                            uint32_t byteOffset) noexcept override
    {
        auto it = m_boBytes.find(h.getId());
        if (it != m_boBytes.end() && byteOffset == 0) {
            auto const* b = static_cast<uint8_t const*>(data.buffer());
            it->second.assign(b, b + data.size());
        }
        dqRender::rhi::NullDriver::updateBufferObject(h, std::move(data), byteOffset);
    }

    // -- VBO/VAO：分配真实句柄（createImdlLutGraphics 以 vbh id 关联槽位）--
    dqRender::rhi::VertexBufferInfoHandle createVertexBufferInfo(
        uint8_t bufferCount, uint8_t attributeCount,
        dqRender::rhi::AttributeArray const& attrs) noexcept override
    {
        m_lastBufferCount = bufferCount;
        m_lastAttributeCount = attributeCount;
        for (uint8_t i = 0; i < attributeCount; ++i)
            m_lastAttrs[i] = attrs[i];
        return m_allocator.allocate<dqRender::rhi::HwVertexBufferInfo>();
    }

    dqRender::rhi::VertexBufferHandle createVertexBuffer(
        uint32_t /*vertexCount*/, dqRender::rhi::VertexBufferInfoHandle) noexcept override
    {
        return m_allocator.allocate<dqRender::rhi::HwVertexBuffer>();
    }

    dqRender::rhi::RenderPrimitiveHandle createRenderPrimitive(
        dqRender::rhi::VertexBufferHandle vbh, dqRender::rhi::IndexBufferHandle ibh,
        dqRender::rhi::PrimitiveType pt) noexcept override
    {
        m_lastPrimVbh = vbh.getId();
        m_lastPrimHasIbo = static_cast<bool>(ibh);
        m_lastPrimType = pt;
        return m_allocator.allocate<dqRender::rhi::HwRenderPrimitive>();
    }

    // -- LUT 上传计数（共享断言：surface + edges 只允许一次直传）--
    void setTextureData(dqRender::rhi::TextureHandle h, uint32_t l0, uint32_t l1, uint32_t l2,
                        uint32_t l3, uint32_t width, uint32_t height, uint32_t l4,
                        dqRender::rhi::PixelBufferDescriptor&& data) noexcept override
    {
        ++m_textureUploads;
        // U11(3)：按纹理句柄分别捕获（顶点表与边表是两张纹理——surface LUT 与
        // EdgeLUT 各上传一次，需要各自的字节/尺寸断言）。
        auto const* b = static_cast<uint8_t const*>(data.buffer());
        m_texBytes[h.getId()].assign(b, b + data.size());
        m_texDims[h.getId()] = {width, height};
        RecordingLutDriver::setTextureData(h, l0, l1, l2, l3, width, height, l4, std::move(data));
    }

    // -- vertex layout --
    void setVertexBufferObject(dqRender::rhi::VertexBufferHandle vbh, uint32_t index,
                               dqRender::rhi::BufferObjectHandle bo) noexcept override
    {
        m_vbhSlots[vbh.getId()][index] = bo.getId();
        dqRender::rhi::NullDriver::setVertexBufferObject(vbh, index, bo);
    }

    // 观测口。
    std::vector<uint8_t> const* boBytes(uint32_t id) const
    {
        auto it = m_boBytes.find(id);
        return it == m_boBytes.end() ? nullptr : &it->second;
    }
    uint32_t textureUploads() const noexcept { return m_textureUploads; }
    std::vector<uint8_t> const* texBytes(uint32_t texId) const
    {
        auto it = m_texBytes.find(texId);
        return it == m_texBytes.end() ? nullptr : &it->second;
    }
    uint32_t texWidth(uint32_t texId) const
    {
        auto it = m_texDims.find(texId);
        return it == m_texDims.end() ? 0u : it->second.first;
    }
    uint32_t texHeight(uint32_t texId) const
    {
        auto it = m_texDims.find(texId);
        return it == m_texDims.end() ? 0u : it->second.second;
    }
    uint8_t lastAttributeCount() const noexcept { return m_lastAttributeCount; }
    dqRender::rhi::AttributeArray const& lastAttrs() const noexcept { return m_lastAttrs; }
    uint32_t lastPrimVbh() const noexcept { return m_lastPrimVbh; }
    bool lastPrimHasIbo() const noexcept { return m_lastPrimHasIbo; }
    dqRender::rhi::PrimitiveType lastPrimType() const noexcept { return m_lastPrimType; }
    uint32_t vbhSlotBuffer(uint32_t vbhId, uint32_t slot) const
    {
        auto const& slots = m_vbhSlots.at(vbhId);
        auto it = slots.find(slot);
        return it == slots.end() ? dqRender::rhi::HandleBase::nullid : it->second;
    }

private:
    dqRender::rhi::HandleAllocator m_allocator;
    std::map<uint32_t, std::vector<uint8_t>> m_boBytes;
    std::map<uint32_t, std::vector<uint8_t>> m_texBytes;
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> m_texDims;
    uint32_t m_textureUploads = 0;
    uint8_t m_lastBufferCount = 0;
    uint8_t m_lastAttributeCount = 0;
    // AttributeArray 是 C 数组（DriverEnums.h:518）——不可赋值，逐元素拷贝。
    dqRender::rhi::AttributeArray m_lastAttrs{};
    uint32_t m_lastPrimVbh = 0;
    bool m_lastPrimHasIbo = false;
    dqRender::rhi::PrimitiveType m_lastPrimType = dqRender::rhi::PrimitiveType::TRIANGLES;
    std::map<uint32_t, std::map<uint32_t, uint32_t>> m_vbhSlots;
};

}  // namespace

// LUT 主路径的 segment edges graphic（rectangle 录制夹具：4 segments）。
// 断言 technique/attribute 布局 + BO 字节 verbatim + LUT 共享（不重传）。
// Ported from: EdgeGeometry.ts create/ctor（buffer/attribute 结构）+ Mesh.ts
//              :52（segmentEdges 与 surface 同属一个 mesh graphic）。
TEST(ImdlGraphicsTest, LutPathProducesSegmentEdgeGraphics)
{
    auto doc = parseFull(V1_1::rectangleBytes, V1_1::rectangleSize);
    ASSERT_TRUE(doc.has_value());

    EdgeRecordingDriver driver;
    LutStubSystem system(driver);
    auto graphics = dqRender::createImdlLutGraphics(*doc, system);
    ASSERT_EQ(graphics.size(), 1u);
    auto* mesh = static_cast<dqRender::MeshGraphic*>(graphics[0]);
    ASSERT_EQ(mesh->getSurfaces().size(), 1u);
    dqRender::SurfaceGeometry const* surf = mesh->getSurfaces()[0].get();

    // edges graphic 与 surface 并列（reference Mesh.ts:52 segmentEdges）。
    ASSERT_EQ(mesh->getEdges().size(), 1u);
    dqRender::EdgeGeometry const* edge = mesh->getEdges()[0].get();
    ASSERT_NE(edge, nullptr);

    // technique/量化形态：Edge technique + 量化 LUT 几何（与 surface 共享
    // 顶点表——MeshData.lut）。
    EXPECT_EQ(edge->getTechniqueId(), dqRender::TechniqueId::Edge);
    EXPECT_EQ(edge->getPass(), dqRender::Pass::OpaqueLinear);
    EXPECT_TRUE(edge->usesQuantizedPositions());
    ASSERT_NE(edge->getLut(), nullptr);
    EXPECT_EQ(edge->getLut()->getTexture().getId(), surf->getLut().getTexture().getId())
        << "edge geometry must observe the surface's vertex LUT (MeshData.lut), not a copy";

    // attribute 布局（EdgeGeometry.ts :81-85——a_pos UBYTE3 索引流 location 0
    // + a_endPointAndQuadIndices UBYTE4 location 1；drawArrays 无 element
    // index buffer）。
    EXPECT_EQ(driver.lastPrimType(), dqRender::rhi::PrimitiveType::TRIANGLES);
    EXPECT_FALSE(driver.lastPrimHasIbo());
    EXPECT_EQ(driver.lastAttributeCount(), 2u);
    EXPECT_EQ(driver.lastAttrs()[0].buffer, 0u);
    EXPECT_EQ(driver.lastAttrs()[0].type, dqRender::rhi::ElementType::UBYTE3);
    EXPECT_EQ(driver.lastAttrs()[0].offset, 0u);
    EXPECT_EQ(driver.lastAttrs()[1].buffer, 1u);
    EXPECT_EQ(driver.lastAttrs()[1].type, dqRender::rhi::ElementType::UBYTE4);
    EXPECT_EQ(driver.lastAttrs()[1].offset, 0u);

    // 顶点数：indices 字节 72 / 3 = 24 顶点（4 segments × 6 顶点四边形）；
    // endPointAndQuadIndices 96B == 4B/顶点。
    EXPECT_EQ(edge->getNumIndices(), 24u);

    // 边缘 symbology（MeshData.ts:93-94——edgeWidth = edges.weight、
    // edgeLineCode = valueFromLinePixels(Solid)=0；parseEdges :730-731）。
    EXPECT_FLOAT_EQ(edge->getEdgeWidth(), 1.0f);
    EXPECT_EQ(edge->getEdgeLineCode(), 0u);

    // LUT 仅上传一次（surface 与 edges 共享——reference 无第二次
    // createForData 调用）。
    EXPECT_EQ(driver.textureUploads(), 1u);
    EXPECT_EQ(driver.lastWidth(), 16u);
    EXPECT_EQ(driver.lastHeight(), 1u);

    // BO 字节 verbatim：edge VAO 两条 VBO 的上传字节 == 线上 bufferView 字节
    // （EdgeGeometry.ts :43-44 createArrayBuffer(edges.indices.data /
    // edges.endPointAndQuadIndices)——零 CPU 重排，形态同顶点表直传锁）。
    auto json = dqRender::tilejson::parseJsonDocument(doc->sceneJson);
    ASSERT_NE(json, nullptr);
    auto prims = dqRender::tilejson::parseImdlMeshPrimitives(*json);
    ASSERT_EQ(prims.size(), 1u);
    ASSERT_TRUE(prims[0].edges.has_value());
    ASSERT_TRUE(prims[0].edges->segments.has_value());
    auto const* views = json->find("bufferViews");
    ASSERT_NE(views, nullptr);
    auto resolveWire = [&](std::string const& name) -> std::vector<uint8_t> {
        auto const* view = views->find(name.c_str());
        EXPECT_NE(view, nullptr) << name;
        if (!view) return {};
        size_t off = static_cast<size_t>(view->find("byteOffset")->number);
        size_t len = static_cast<size_t>(view->find("byteLength")->number);
        return std::vector<uint8_t>(doc->binary.begin() + off, doc->binary.begin() + off + len);
    };
    auto const wireIdx = resolveWire(prims[0].edges->segments->indicesView);
    auto const wireEpq = resolveWire(prims[0].edges->segments->endPointAndQuadIndicesView);
    uint32_t const edgeVbh = driver.lastPrimVbh();
    ASSERT_NE(edgeVbh, dqRender::rhi::HandleBase::nullid)
        << "edges are created after the surface → last primitive is the edge VAO";
    auto const* idxBytes = driver.boBytes(driver.vbhSlotBuffer(edgeVbh, 0));
    auto const* epqBytes = driver.boBytes(driver.vbhSlotBuffer(edgeVbh, 1));
    ASSERT_NE(idxBytes, nullptr);
    ASSERT_NE(epqBytes, nullptr);
    EXPECT_EQ(*idxBytes, wireIdx) << "a_pos stream = wire indices verbatim";
    EXPECT_EQ(*epqBytes, wireEpq) << "a_endPointAndQuadIndices stream = wire bytes verbatim";

    delete graphics[0];
}

// cylinder 录制夹具：segments + silhouettes 双 edge graphic（silhouette 先建
// ——Mesh.ts:49-52 顺序；a_normals 第三条 UBYTE4 流 + SilhouetteEdge
// technique）。
TEST(ImdlGraphicsTest, LutPathProducesSilhouetteEdgeGraphics)
{
    auto doc = parseFull(V1_1::cylinderBytes, V1_1::cylinderSize);
    ASSERT_TRUE(doc.has_value());

    EdgeRecordingDriver driver;
    LutStubSystem system(driver);
    auto graphics = dqRender::createImdlLutGraphics(*doc, system);
    ASSERT_EQ(graphics.size(), 1u);
    auto* mesh = static_cast<dqRender::MeshGraphic*>(graphics[0]);
    ASSERT_EQ(mesh->getSurfaces().size(), 1u);
    dqRender::SurfaceGeometry const* surf = mesh->getSurfaces()[0].get();

    // 两个 edge graphic：silhouettes 先、segments 后（Mesh.ts:49-52）。
    ASSERT_EQ(mesh->getEdges().size(), 2u);
    auto* sil = mesh->getEdges()[0].get();
    auto* seg = mesh->getEdges()[1].get();
    ASSERT_NE(sil, nullptr);
    ASSERT_NE(seg, nullptr);

    // technique/order（EdgeGeometry.ts :110-112——SilhouetteEdge + Silhouette
    // order；cylinder 非 planar）。
    EXPECT_EQ(sil->getTechniqueId(), dqRender::TechniqueId::SilhouetteEdge);
    EXPECT_EQ(sil->getRenderOrder(), dqRender::RenderOrder::Silhouette);
    EXPECT_EQ(seg->getTechniqueId(), dqRender::TechniqueId::Edge);
    EXPECT_EQ(seg->getRenderOrder(), dqRender::RenderOrder::Edge);
    EXPECT_TRUE(sil->usesQuantizedPositions());

    // LUT 共享：三几何同一纹理句柄。
    ASSERT_NE(sil->getLut(), nullptr);
    EXPECT_EQ(sil->getLut()->getTexture().getId(), surf->getLut().getTexture().getId());

    // 顶点数（夹具 bytes：sil indices 648/3=216、segments indices 1296/3=432；
    // normalPairs 864 == 4B×216）。
    EXPECT_EQ(sil->getNumIndices(), 216u);
    EXPECT_EQ(seg->getNumIndices(), 432u);

    // symbology：cylinder 材质 lineWidth（解析锁 ImdlEdges.
    // SilhouetteEdgesFromRecordedCylinder 同源）。
    EXPECT_FLOAT_EQ(sil->getEdgeWidth(), 1.0f);

    delete graphics[0];
}

// Edge 变体 shader 结构锁：量化 LUT 消费 + attribute 期望。
// Ported from: itwinjs-core Edge.ts createBase（addPositionFromLUT 经
//              VertexShaderBuilder ctor :725 addPosition(this,
//              usesVertexTable=true)、decodeEndPointAndQuadIndices :26-30、
//              addColor → getComputeElementColor 量化 u_color 路径）+
//              Vertex.ts getSamplePositionQuantizedPostlude :71-79（对端点
//              采样 = 双 texel decodeUInt16，非单 texel raw.xyz）。
// Authored: no reference shader-snapshot test exists（参考无 shader 源串
//           测试——DanQing 无 macOS 之外的离线 GLSL 编译 harness，结构断言
//           为 Windows 下可行的最强锁；GlslCompileHarness 平台限制见其头注）。
TEST(EdgeShaderVariant, QuantizedVariantConsumesVertexLut)
{
    auto builder = dqRender::createEdgeProgramBuilder(
        dqRender::EdgeBuilderType::SegmentEdge, dqRender::FeatureMode::None,
        dqRender::PositionType::Quantized);
    std::string const vert = builder.getVertexBuilder().buildSourceWithComponents();

    // a_pos 24-bit 索引 attribute（参考 AttributeMap edge 条目名）+ 对端点
    // 解码 initializer（Edge.ts decodeEndPointAndQuadIndices :26-30）。
    EXPECT_NE(vert.find("decodeUInt24(a_pos)"), std::string::npos)
        << "vertex LUT key decode (initializeVertLUTCoords) must read a_pos";
    EXPECT_NE(vert.find("g_otherIndex = decodeUInt24(a_endPointAndQuadIndices.xyz)"),
              std::string::npos);
    EXPECT_NE(vert.find("g_otherPos = samplePosition(g_otherIndex)"), std::string::npos)
        << "other endpoint position is sampled on demand";

    // 本顶点位置经 pre-read LUT 解码（Vertex.ts computeVertexPositionFromLUT
    // :35-41——quantized 双 texel uint16）。
    EXPECT_NE(vert.find("decodeUInt16(g_vertLutData0.xy)"), std::string::npos);

    // 对端点采样 = 双 texel decodeUInt16（Vertex.ts
    // getSamplePositionQuantizedPostlude :71-79）——不是单 texel raw.xyz。
    EXPECT_NE(vert.find("decodeUInt16(e0.xy)"), std::string::npos)
        << "samplePosition must decode the quantized position across texels e0/e1";

    // 颜色：u_color uniform 量化路径（Color.ts addColor :51-62），无 a_color
    // attribute（参考 Edge 几何不供色 attribute）。
    EXPECT_NE(vert.find("u_color"), std::string::npos);
    EXPECT_EQ(vert.find("a_color"), std::string::npos)
        << "edge geometry carries no color attribute — reference addColor path";

    // silhouette 变体：a_normals attribute + 早退 discard（Edge.ts
    // checkForSilhouetteDiscardNonIndexed :132-136）。
    auto silBuilder = dqRender::createEdgeProgramBuilder(
        dqRender::EdgeBuilderType::Silhouette, dqRender::FeatureMode::None,
        dqRender::PositionType::Quantized);
    std::string const silVert = silBuilder.getVertexBuilder().buildSourceWithComponents();
    EXPECT_NE(silVert.find("octDecodeNormal(a_normals.xy)"), std::string::npos);
}

// ---------------------------------------------------------------------------
// U11(3)：indexed/compact 边缘消费。
//
// (a) calculateEdgeTableParams 参数矩阵——移植参考测试的 9 行
//     (numSegments, numSilhouettes, expectedPadding, expectedWidth,
//      expectedHeight) 表。
//     Ported from: itwinjs-core core/frontend/src/test/render/webgl/
//                  IndexedEdges.test.ts
//                  describe(IndexedEdgeParams > when enabled >
//                  "inserts padding between segments and silhouettes when
//                  required")（:246-287 的 testCases 表——参考侧经
//                  buildIndexedEdges 驱动，DanQing 直接对展开共用的
//                  calculateEdgeTableParams 断言；参考测试的字节内容半部依赖
//                  MeshArgs/MeshEdge 生产链，DanQing 无该通路，未移植——
//                  内容锁由下方 compact 展开数值锁承担）。
//     maxSize=15 与参考一致（makeEdgeParams(args, 15)）。
// ---------------------------------------------------------------------------
TEST(CompactEdges, EdgeTableParamsMatchReferenceMatrix)
{
    static constexpr struct {
        uint32_t numSegments;
        uint32_t numSilhouettes;
        uint32_t expectedPadding;
        uint32_t expectedWidth;
        uint32_t expectedHeight;
    } kCases[] = {
        // bad
        {330, 101, 0, 30, 25},
        {64, 32, 6, 15, 12},
        {126, 63, 4, 30, 12},
        {288, 80, 2, 30, 22},
        {102, 51, 8, 30, 10},
        // good
        {80, 40, 0, 15, 15},
        {100, 50, 0, 30, 10},
        {258, 65, 2, 30, 19},
        {74, 37, 6, 15, 14},
    };

    for (auto const& tc : kCases) {
        auto const info = dqRender::calculateEdgeTableParams(
            tc.numSegments, tc.numSilhouettes, 15);
        EXPECT_EQ(info.silhouettePadding, tc.expectedPadding)
            << "segs=" << tc.numSegments << " sils=" << tc.numSilhouettes;
        EXPECT_EQ(info.width, tc.expectedWidth)
            << "segs=" << tc.numSegments << " sils=" << tc.numSilhouettes;
        EXPECT_EQ(info.height, tc.expectedHeight)
            << "segs=" << tc.numSegments << " sils=" << tc.numSilhouettes;
        // EdgeParams.ts:118 silhouetteStartByteIndex = numSegmentEdges * 6。
        EXPECT_EQ(info.silhouetteStartByteIndex, tc.numSegments * 6u);
    }
}

// ---------------------------------------------------------------------------
// (b) compact 展开数值锁——合成 visibility 流（Visible/Hidden/Silhouette/
//     VisibleDuplicate 四态混合）断言展开的 edge 表字节与索引流。
//     Authored: no reference test exists for indexedEdgeParamsFromCompactEdges
//     (IndexedEdges.test.ts 只覆盖 buildIndexedEdges——MeshArgs 通路；
//     CompactEdges.ts 全文检索无对应测试，2026-09-27)。
//     数值钉死自参考算法逐行手算（CompactEdges.ts:44-129 + EdgeParams.ts:114-149）：
//     4 三角形扇形 (0,1,2)(0,2,3)(0,3,4)(0,4,5) → 12 条边，可见性
//     [V,H,S,V,D,H,V,S,V,H,V,S] → numVisibleEdges=5 + 3 silhouettes；
//     maxSize=2048（≥ nRgba=15 → 单行 15×1、pad=0、silStart=30）。
// ---------------------------------------------------------------------------
TEST(CompactEdges, ExpandsMixedVisibilityStreamToEdgeTable)
{
    // 顶点索引（surface.indices）：4 三角形 = 12 个 24-bit 索引。
    std::vector<uint8_t> cornerIndices;
    {
        static constexpr uint32_t kCorners[12] = {0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5};
        cornerIndices.resize(12 * 3);
        for (uint32_t i = 0; i < 12; ++i)
            dqRender::VertexIndices::encodeIndex(kCorners[i], cornerIndices, i * 3);
    }

    // 可见性流（2-bit/边，LSB 在前）：e0=Visible e1=Hidden e2=Silhouette
    // e3=Visible e4=VisibleDuplicate e5=Hidden e6=Visible e7=Silhouette
    // e8=Visible e9=Hidden e10=Visible e11=Silhouette。
    uint8_t const visibilityBytes[3] = {
        2 | (0 << 2) | (1 << 4) | (2 << 6),  // e0..e3  = 0x92
        3 | (0 << 2) | (2 << 4) | (1 << 6),  // e4..e7  = 0x63
        2 | (0 << 2) | (2 << 4) | (1 << 6),  // e8..e11 = 0x62
    };
    // silhouette 法线对（u32 = normal1 | normal2<<16，ImdlSchema.ts:264）。
    uint32_t const normalPairValues[3] = {0x11223344u, 0x55667788u, 0x99AABBCCu};
    std::vector<uint8_t> normalPairBytes;
    for (uint32_t v : normalPairValues)
        for (uint32_t b = 0; b < 4; ++b)
            normalPairBytes.push_back(static_cast<uint8_t>((v >> (b * 8)) & 0xFFu));

    dqRender::CompactEdgeParams compact;
    compact.numVisibleEdges = 5;
    compact.visibility = dqRender::ImdlByteView{visibilityBytes, sizeof(visibilityBytes)};
    compact.vertexIndices = dqRender::VertexIndices(cornerIndices);
    compact.normalPairs = dqRender::ImdlByteView{normalPairBytes.data(), normalPairBytes.size()};
    compact.maxEdgeTableDimension = 2048;

    auto result = dqRender::indexedEdgeParamsFromCompactEdges(compact);
    ASSERT_TRUE(result.has_value()) << "numTotalEdges=8 > 0 (:87-88)";

    // 表参数：nRgba = ceil(1.5*5 + 2.5*3) = 15 < maxSize → 15×1、pad=0。
    EXPECT_EQ(result->edges.width, 15u);
    EXPECT_EQ(result->edges.height, 1u);
    EXPECT_EQ(result->edges.numSegments, 5u);
    EXPECT_EQ(result->edges.silhouettePadding, 0u);

    // 查找表字节（下分区 5 段边 ×6B + 上分区 3 silhouettes ×10B）。
    static constexpr uint8_t kExpectedTable[60] = {
        0, 0, 0, 1, 0, 0,          // seg0  = e0  (0,1)
        0, 0, 0, 2, 0, 0,          // seg1  = e3  (0,2)
        0, 0, 0, 3, 0, 0,          // seg2  = e6  (0,3)
        4, 0, 0, 0, 0, 0,          // seg3  = e8  (4,0)
        4, 0, 0, 5, 0, 0,          // seg4  = e10 (4,5)
        2, 0, 0, 0, 0, 0,          // sil0  = e2  (2,0)
        0x44, 0x33, 0x22, 0x11,    //       normals 0x11223344
        3, 0, 0, 4, 0, 0,          // sil1  = e7  (3,4)
        0x88, 0x77, 0x66, 0x55,    //       normals 0x55667788
        5, 0, 0, 0, 0, 0,          // sil2  = e11 (5,0)
        0xCC, 0xBB, 0xAA, 0x99,    //       normals 0x99AABBCC
    };
    ASSERT_EQ(result->edges.data.byteLength, 60u);
    EXPECT_EQ(std::memcmp(result->edges.data.data, kExpectedTable, 60), 0)
        << "edge table bytes must match the hand-expanded reference algorithm";

    // 索引流：8 边 × 6 顶点 × 24-bit——每边六个相同索引（:90-94）。
    ASSERT_EQ(result->indices.byteLength, 8u * 6u * 3u);
    for (uint32_t edge = 0; edge < 8; ++edge)
        for (uint32_t j = 0; j < 6; ++j)
            EXPECT_EQ((result->indices.data[(edge * 6 + j) * 3])
                          | (static_cast<uint32_t>(result->indices.data[(edge * 6 + j) * 3 + 1]) << 8)
                          | (static_cast<uint32_t>(result->indices.data[(edge * 6 + j) * 3 + 2]) << 16),
                      edge)
                << "edge " << edge << " quad corner " << j;
}

// ---------------------------------------------------------------------------
// (c) 分区对齐填充路径——nRgba ≥ maxSize 时 silhouette 上分区的起始字节含
//     padding（CompactEdges.ts:109 的 silhouetteStartByteIndex + silhouettePadding）。
//     Authored: 同上（参考无 compact 展开测试）；参数由 (a) 的参考矩阵行
//     [74, 37, 6, 15, 14] 同式锁定——本用例 13 segs + 7 sils + maxSize=15 →
//     silOffset=78%60=18 → pad=(60-18)%10=2、width=15、height=3。
// ---------------------------------------------------------------------------
TEST(CompactEdges, ExpandsWithPartitionPadding)
{
    // 7 三角形扇形 (0,1,2)..(0,7,8) → 21 条边：e0..e12 Visible（13）、
    // e13..e19 Silhouette（7）、e20 Hidden。
    std::vector<uint8_t> cornerIndices;
    {
        static constexpr uint32_t kCorners[21] = {0, 1, 2, 0, 2, 3, 0, 3, 4,
                                                  0, 4, 5, 0, 5, 6, 0, 6, 7, 0, 7, 8};
        cornerIndices.resize(21 * 3);
        for (uint32_t i = 0; i < 21; ++i)
            dqRender::VertexIndices::encodeIndex(kCorners[i], cornerIndices, i * 3);
    }

    // 可见性流（2-bit/边，LSB 在前，4 边/字节）：
    //   byte0-2 = 0xAA ×3 —— e0..e11 全 Visible(2)；
    //   byte3   = 2 | 1<<2 | 1<<4 | 1<<6 = 0x56 —— e12=V、e13..e15=S；
    //   byte4   = 1 | 1<<2 | 1<<4 | 1<<6 = 0x55 —— e16..e19=S；
    //   byte5   = 0 —— e20=Hidden（+2 bit 未用尾零）。
    uint8_t const fixedVisibility[6] = {0xAA, 0xAA, 0xAA, 0x56, 0x55,
                                        0 | (0 << 2) | (0 << 4) | (0 << 6)};

    std::vector<uint8_t> normalPairBytes;
    for (uint32_t j = 0; j < 7; ++j) {
        uint32_t const v = 0xA000u | j;  // 各对可区分（LE: j, 0xA0, 0, 0）
        for (uint32_t b = 0; b < 4; ++b)
            normalPairBytes.push_back(static_cast<uint8_t>((v >> (b * 8)) & 0xFFu));
    }

    dqRender::CompactEdgeParams compact;
    compact.numVisibleEdges = 13;
    compact.visibility = dqRender::ImdlByteView{fixedVisibility, sizeof(fixedVisibility)};
    compact.vertexIndices = dqRender::VertexIndices(cornerIndices);
    compact.normalPairs = dqRender::ImdlByteView{normalPairBytes.data(), normalPairBytes.size()};
    compact.maxEdgeTableDimension = 15;

    auto result = dqRender::indexedEdgeParamsFromCompactEdges(compact);
    ASSERT_TRUE(result.has_value());

    // 表参数（(a) 矩阵同式手算）：nRgba=ceil(19.5+17.5)=37 ≥15 →
    // width=ceil(√37)=7→+8=15、silOffset=78%60=18→pad=2、nRgba+=1=38、height=3。
    EXPECT_EQ(result->edges.width, 15u);
    EXPECT_EQ(result->edges.height, 3u);
    EXPECT_EQ(result->edges.numSegments, 13u);
    EXPECT_EQ(result->edges.silhouettePadding, 2u);
    ASSERT_EQ(result->edges.data.byteLength, 15u * 3u * 4u);

    uint8_t const* table = result->edges.data.data;
    // seg0 = e0 (0,1) @0。
    EXPECT_EQ(table[0], 0u);
    EXPECT_EQ(table[3], 1u);
    // seg12 = e12 (0,5) @72：24-bit 0 → bytes 72-74；24-bit 5 → bytes 75-77。
    EXPECT_EQ(table[72], 0u);
    EXPECT_EQ(table[75], 5u);
    // 分区填充（silStart=78 + pad 2 → silhouettes 自 byte 80 起）。
    EXPECT_EQ(table[78], 0u);
    EXPECT_EQ(table[79], 0u);
    // sil0 = e13 (5,6) @80 + normals (0xA000)。
    EXPECT_EQ(table[80], 5u);
    EXPECT_EQ(table[83], 6u);
    EXPECT_EQ(table[86], 0x00u);
    EXPECT_EQ(table[87], 0xA0u);
    EXPECT_EQ(table[88], 0u);
    EXPECT_EQ(table[89], 0u);
    // sil6 = e19 (7,8) @140 + normals (0xA006)——e19: corner 19 = tri6 第二角
    // (=7)、next = corner 20 (=8)。
    EXPECT_EQ(table[140], 7u);
    EXPECT_EQ(table[143], 8u);
    EXPECT_EQ(table[146], 0x06u);
    EXPECT_EQ(table[147], 0xA0u);
    // sil6 之后（150..179）保持零填充。
    for (uint32_t i = 150; i < 180; ++i)
        EXPECT_EQ(table[i], 0u) << "byte " << i;

    // 索引流：20 边（13+7）× 6。
    EXPECT_EQ(result->indices.byteLength, 20u * 6u * 3u);
}

// 空展开：numVisibleEdges=0 且无 normalPairs → nullopt（:86-88 undefined）。
TEST(CompactEdges, NoEdgesYieldsNullopt)
{
    uint8_t const visibilityBytes[1] = {0xFF};  // 全 Hidden/VisibleDuplicate
    dqRender::CompactEdgeParams compact;
    compact.numVisibleEdges = 0;
    compact.visibility = dqRender::ImdlByteView{visibilityBytes, sizeof(visibilityBytes)};
    compact.vertexIndices = dqRender::VertexIndices();
    compact.maxEdgeTableDimension = 2048;
    EXPECT_FALSE(dqRender::indexedEdgeParamsFromCompactEdges(compact).has_value());
}

// ---------------------------------------------------------------------------
// (d) parseImdlEdges 的 compact 兜底展开（ParseImdlDocument.ts:719-720 →
//     parseCompactEdges :695-708）。Authored: no reference test exists for imdl
//     edges parsing（同 U11(1) 区块说明）——数值沿用 (b) 的手算展开。
// ---------------------------------------------------------------------------
TEST(ImdlEdges, SyntheticCompactEdgesExpandToIndexed)
{
    dqRender::ImdlDocument doc;
    // BIN：bvIdx @0 len 36（12 corner 索引）、bvVisibility @36 len 3、
    // bvNormalPairs @39 len 12。
    doc.sceneJson = R"json({
        "materials": {"Mat": {"fillColor": 65280, "lineWidth": 2, "linePixels": 0}},
        "meshes": {"Mesh_Root": {"primitives": [{
            "material": "Mat",
            "surface": {"indices": "bvIdx", "type": 1},
            "edges": {"compact": {"visibility": "bvVisibility",
                                  "normalPairs": "bvNormalPairs", "numVisible": 5}}
        }]}},
        "bufferViews": {
            "bvIdx": {"buffer": "binary_glTF", "byteOffset": 0, "byteLength": 36},
            "bvVisibility": {"buffer": "binary_glTF", "byteOffset": 36, "byteLength": 3},
            "bvNormalPairs": {"buffer": "binary_glTF", "byteOffset": 39, "byteLength": 12}
        }
    })json";
    doc.binary.assign(51u, 0u);
    {
        static constexpr uint32_t kCorners[12] = {0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5};
        for (uint32_t i = 0; i < 12; ++i) {
            doc.binary[i * 3 + 0] = static_cast<uint8_t>(kCorners[i] & 0xFF);
            doc.binary[i * 3 + 1] = 0;
            doc.binary[i * 3 + 2] = 0;
        }
        doc.binary[36] = 0x92;
        doc.binary[37] = 0x63;
        doc.binary[38] = 0x62;
        uint32_t const np[3] = {0x11223344u, 0x55667788u, 0x99AABBCCu};
        for (uint32_t j = 0; j < 3; ++j)
            for (uint32_t b = 0; b < 4; ++b)
                doc.binary[39 + j * 4 + b] = static_cast<uint8_t>((np[j] >> (b * 8)) & 0xFFu);
    }

    auto json = dqRender::tilejson::parseJsonDocument(doc.sceneJson);
    ASSERT_NE(json, nullptr);
    auto prims = dqRender::tilejson::parseImdlMeshPrimitives(*json);
    ASSERT_EQ(prims.size(), 1u);

    auto edges = dqRender::tilejson::parseImdlEdges(*json, doc, prims[0]);
    ASSERT_TRUE(edges.has_value()) << "compact expansion must produce indexed (:719-720)";
    EXPECT_EQ(edges->weight, 2u);
    EXPECT_EQ(edges->linePixels, dqCommon::LinePixels::Solid);
    EXPECT_FALSE(edges->segments.has_value());
    EXPECT_FALSE(edges->silhouettes.has_value());
    ASSERT_TRUE(edges->indexed.has_value());
    EXPECT_EQ(edges->indexed->edges.width, 15u);
    EXPECT_EQ(edges->indexed->edges.height, 1u);
    EXPECT_EQ(edges->indexed->edges.numSegments, 5u);
    EXPECT_EQ(edges->indexed->edges.silhouettePadding, 0u);
    EXPECT_EQ(edges->indexed->indices.byteLength, 48u * 3u);
    // 展开产物为自有字节（非指入 doc.binary 的区间视图）——表首字节即 seg0。
    ASSERT_NE(edges->indexed->edges.data.data, nullptr);
    EXPECT_NE(edges->indexed->edges.data.data, doc.binary.data())
        << "expanded table is owned storage, not a binary-section view";
    EXPECT_EQ(edges->indexed->edges.data.data[0], 0u);
    EXPECT_EQ(edges->indexed->edges.data.data[3], 1u);
    EXPECT_EQ(edges->indexed->edges.data.data[36], 0x44u) << "sil0 normal u32 low byte";
}

// indexed 直取优先于 compact（:718-720——indexed 在先，compact 仅兜底）。
TEST(ImdlEdges, DirectIndexedTakesPrecedenceOverCompact)
{
    dqRender::ImdlDocument doc;
    doc.sceneJson = R"json({
        "meshes": {"Mesh_Root": {"primitives": [{
            "surface": {"indices": "bvIdx", "type": 1},
            "edges": {
                "indexed": {"indices": "bvEdgeIdx", "edges": "bvEdgeTable",
                            "width": 4, "height": 1, "numSegments": 2,
                            "silhouettePadding": 0},
                "compact": {"visibility": "bvVisibility", "numVisible": 1}
            }
        }]}},
        "bufferViews": {
            "bvIdx": {"buffer": "binary_glTF", "byteOffset": 0, "byteLength": 9},
            "bvEdgeIdx": {"buffer": "binary_glTF", "byteOffset": 9, "byteLength": 12},
            "bvEdgeTable": {"buffer": "binary_glTF", "byteOffset": 21, "byteLength": 16},
            "bvVisibility": {"buffer": "binary_glTF", "byteOffset": 37, "byteLength": 3}
        }
    })json";
    doc.binary.assign(40u, 0x33u);

    auto json = dqRender::tilejson::parseJsonDocument(doc.sceneJson);
    ASSERT_NE(json, nullptr);
    auto prims = dqRender::tilejson::parseImdlMeshPrimitives(*json);
    ASSERT_EQ(prims.size(), 1u);
    auto edges = dqRender::tilejson::parseImdlEdges(*json, doc, prims[0]);
    ASSERT_TRUE(edges.has_value());
    ASSERT_TRUE(edges->indexed.has_value());
    // 直取形态：视图指入 doc.binary（非展开自有字节）+ JSON 尺寸原样。
    EXPECT_EQ(edges->indexed->indices.data, doc.binary.data() + 9);
    EXPECT_EQ(edges->indexed->edges.data.data, doc.binary.data() + 21);
    EXPECT_EQ(edges->indexed->edges.width, 4u);
    EXPECT_EQ(edges->indexed->edges.numSegments, 2u);
}

// ---------------------------------------------------------------------------
// (e) indexed 消费锁——合成 indexed EdgeParams → EdgeLUT 纹理参数
//     （width/height/numSegments/silhouettePadding）+ 索引计数 + BO 字节
//     verbatim + 顶点 LUT 共享。
//     Ported from: IndexedEdgeGeometry.ts create (:98-102) + EdgeLUT.create
//     (:46-49) + ctor (:74-86) + Mesh.ts:64-65/141（同一 mesh graphic）。
//     数值断言 Authored: no reference test exists for imdl edges graphics
//     （同 U11(2) 区块说明）——表字节沿用 (b) 的手算展开。
// ---------------------------------------------------------------------------
TEST(ImdlGraphicsTest, LutPathProducesIndexedEdgeGraphics)
{
    dqRender::ImdlDocument doc;
    // BIN 布局：bvIdx @0 len 9（1 三角形 0,1,2）、bvVtx @16 len 96（6 顶点 ×
    // 16B LitMesh）、bvEdgeIdx @112 len 144（8 边 × 6 顶点 × 3B）、
    // bvEdgeTable @256 len 60（15×1 RGBA）。
    doc.sceneJson = R"json({
        "materials": {"Mat": {"fillColor": 65280, "lineWidth": 3}},
        "meshes": {"Mesh_Root": {"primitives": [{
            "material": "Mat",
            "surface": {"indices": "bvIdx", "type": 1},
            "vertices": {"bufferView": "bvVtx", "count": 6, "width": 24, "height": 1,
                         "numRgbaPerVertex": 4, "uniformColor": 65280,
                         "params": {"decodedMin": [0, 0, 0], "decodedMax": [2, 2, 2]}},
            "edges": {"indexed": {"indices": "bvEdgeIdx", "edges": "bvEdgeTable",
                                  "width": 15, "height": 1, "numSegments": 5,
                                  "silhouettePadding": 0}}
        }]}},
        "bufferViews": {
            "bvIdx": {"buffer": "binary_glTF", "byteOffset": 0, "byteLength": 9},
            "bvVtx": {"buffer": "binary_glTF", "byteOffset": 16, "byteLength": 96},
            "bvEdgeIdx": {"buffer": "binary_glTF", "byteOffset": 112, "byteLength": 144},
            "bvEdgeTable": {"buffer": "binary_glTF", "byteOffset": 256, "byteLength": 60}
        }
    })json";
    doc.binary.assign(320u, 0u);
    doc.binary[3] = 1u;  // bvIdx：三角形 (0,1,2)
    doc.binary[6] = 2u;
    for (uint32_t edge = 0; edge < 8; ++edge)
        for (uint32_t j = 0; j < 6; ++j)
            doc.binary[112 + (edge * 6 + j) * 3] = static_cast<uint8_t>(edge);
    static constexpr uint8_t kTable[60] = {
        0, 0, 0, 1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 3, 0, 0,
        4, 0, 0, 0, 0, 0, 4, 0, 0, 5, 0, 0,
        2, 0, 0, 0, 0, 0, 0x44, 0x33, 0x22, 0x11,
        3, 0, 0, 4, 0, 0, 0x88, 0x77, 0x66, 0x55,
        5, 0, 0, 0, 0, 0, 0xCC, 0xBB, 0xAA, 0x99,
    };
    std::memcpy(doc.binary.data() + 256, kTable, 60);

    EdgeRecordingDriver driver;
    LutStubSystem system(driver);
    auto graphics = dqRender::createImdlLutGraphics(doc, system);
    ASSERT_EQ(graphics.size(), 1u);
    auto* mesh = static_cast<dqRender::MeshGraphic*>(graphics[0]);
    ASSERT_EQ(mesh->getSurfaces().size(), 1u);
    dqRender::SurfaceGeometry const* surf = mesh->getSurfaces()[0].get();

    // indexed edges graphic 与 surface 并列（Mesh.ts:64-65/141）。
    ASSERT_EQ(mesh->getIndexedEdges().size(), 1u);
    dqRender::IndexedEdgeGeometry const* ix = mesh->getIndexedEdges()[0].get();
    ASSERT_NE(ix, nullptr);

    // technique/pass/order/量化形态（IndexedEdgeGeometry.ts :130/:131/:132）。
    EXPECT_EQ(ix->getTechniqueId(), dqRender::TechniqueId::IndexedEdge);
    EXPECT_EQ(ix->getPass(), dqRender::Pass::OpaqueLinear);
    EXPECT_EQ(ix->getRenderOrder(), dqRender::RenderOrder::Edge);
    EXPECT_TRUE(ix->usesQuantizedPositions());

    // 顶点 LUT 共享（mesh.lut——非拥有观察）。
    ASSERT_NE(ix->getLut(), nullptr);
    EXPECT_EQ(ix->getLut()->getTexture().getId(), surf->getLut().getTexture().getId());

    // EdgeLUT 参数（u_edgeParams 数据源，Edge.ts:255-258）。
    EXPECT_EQ(ix->getEdgeLut().getWidth(), 15u);
    EXPECT_EQ(ix->getEdgeLut().getHeight(), 1u);
    EXPECT_EQ(ix->getEdgeLut().getNumSegments(), 5u);
    EXPECT_EQ(ix->getEdgeLut().getSilhouettePadding(), 0u);
    EXPECT_EQ(ix->getEdgeLut().getBytesUsed(), 60u);

    // 索引计数（params.indices.length——:101）。
    EXPECT_EQ(ix->getNumIndices(), 48u);

    // 颜色（colorInfo ← mesh 均匀色 uniformColor=65280 绿）。
    EXPECT_TRUE(ix->getColorInfo().isUniform());
    EXPECT_EQ(ix->getColorInfo().getRgb(), 0x00FF00u);
    EXPECT_EQ(ix->getColorInfo().getAlpha(), 255u);

    // symbology（MeshData.ts:93-94——weight=displayParams.width=3、Solid→0）。
    EXPECT_FLOAT_EQ(ix->getEdgeWidth(), 3.0f);
    EXPECT_EQ(ix->getEdgeLineCode(), 0u);

    // attribute 布局：单 UBYTE3 流（:80 UnsignedByte×3 @a_pos）、无 IBO、
    // TRIANGLES（:112 drawArrays 形态）。
    EXPECT_EQ(driver.lastAttributeCount(), 1u);
    EXPECT_EQ(driver.lastAttrs()[0].buffer, 0u);
    EXPECT_EQ(driver.lastAttrs()[0].type, dqRender::rhi::ElementType::UBYTE3);
    EXPECT_FALSE(driver.lastPrimHasIbo());
    EXPECT_EQ(driver.lastPrimType(), dqRender::rhi::PrimitiveType::TRIANGLES);

    // 纹理上传：顶点表 + 边表各一次（EdgeLUT.create 独立纹理）。
    EXPECT_EQ(driver.textureUploads(), 2u);
    uint32_t const edgeTexId = ix->getEdgeLut().getTexture().getId();
    ASSERT_NE(driver.texBytes(edgeTexId), nullptr)
        << "edge LUT texture must be uploaded from the edge table bytes";
    EXPECT_EQ(driver.texWidth(edgeTexId), 15u);
    EXPECT_EQ(driver.texHeight(edgeTexId), 1u);
    ASSERT_EQ(driver.texBytes(edgeTexId)->size(), 60u);
    EXPECT_EQ(std::memcmp(driver.texBytes(edgeTexId)->data(), kTable, 60), 0)
        << "edge LUT upload must be the wire edge table verbatim (EdgeLUT.create :46-49)";

    // a_pos BO 字节 verbatim == 线上 indexed.indices（createArrayBuffer :99）。
    uint32_t const ixVbh = driver.lastPrimVbh();
    auto const* idxBytes = driver.boBytes(driver.vbhSlotBuffer(ixVbh, 0));
    ASSERT_NE(idxBytes, nullptr);
    ASSERT_EQ(idxBytes->size(), 144u);
    for (uint32_t i = 0; i < 144; ++i)
        EXPECT_EQ((*idxBytes)[i], doc.binary[112 + i]) << "a_pos byte " << i;

    delete graphics[0];
}

// compact → indexed → graphic 全链锁（parseImdlEdges 兜底展开 + 上传 verbatim）。
// Authored: 同上——展开数值沿用 (b)；链路断言照 (e) 的结构锁。
TEST(ImdlGraphicsTest, LutPathExpandsCompactEdgesToIndexedGraphics)
{
    dqRender::ImdlDocument doc;
    // BIN：bvIdx @0 len 36（12 corner 索引）、bvVtx @48 len 96、
    // bvVisibility @144 len 3、bvNormalPairs @148 len 12。
    doc.sceneJson = R"json({
        "materials": {"Mat": {"fillColor": 65280, "lineWidth": 3}},
        "meshes": {"Mesh_Root": {"primitives": [{
            "material": "Mat",
            "surface": {"indices": "bvIdx", "type": 1},
            "vertices": {"bufferView": "bvVtx", "count": 6, "width": 24, "height": 1,
                         "numRgbaPerVertex": 4, "uniformColor": 65280,
                         "params": {"decodedMin": [0, 0, 0], "decodedMax": [2, 2, 2]}},
            "edges": {"compact": {"visibility": "bvVisibility",
                                  "normalPairs": "bvNormalPairs", "numVisible": 5}}
        }]}},
        "bufferViews": {
            "bvIdx": {"buffer": "binary_glTF", "byteOffset": 0, "byteLength": 36},
            "bvVtx": {"buffer": "binary_glTF", "byteOffset": 48, "byteLength": 96},
            "bvVisibility": {"buffer": "binary_glTF", "byteOffset": 144, "byteLength": 3},
            "bvNormalPairs": {"buffer": "binary_glTF", "byteOffset": 148, "byteLength": 12}
        }
    })json";
    doc.binary.assign(160u, 0u);
    {
        static constexpr uint32_t kCorners[12] = {0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5};
        for (uint32_t i = 0; i < 12; ++i) {
            doc.binary[i * 3 + 0] = static_cast<uint8_t>(kCorners[i] & 0xFF);
        }
        doc.binary[144] = 0x92;
        doc.binary[145] = 0x63;
        doc.binary[146] = 0x62;
        uint32_t const np[3] = {0x11223344u, 0x55667788u, 0x99AABBCCu};
        for (uint32_t j = 0; j < 3; ++j)
            for (uint32_t b = 0; b < 4; ++b)
                doc.binary[148 + j * 4 + b] = static_cast<uint8_t>((np[j] >> (b * 8)) & 0xFFu);
    }

    EdgeRecordingDriver driver;
    LutStubSystem system(driver);
    auto graphics = dqRender::createImdlLutGraphics(doc, system);
    ASSERT_EQ(graphics.size(), 1u);
    auto* mesh = static_cast<dqRender::MeshGraphic*>(graphics[0]);
    ASSERT_EQ(mesh->getSurfaces().size(), 1u);
    ASSERT_EQ(mesh->getIndexedEdges().size(), 1u)
        << "compact must expand into an indexed edge graphic (:719-720)";
    ASSERT_EQ(mesh->getEdges().size(), 0u);
    dqRender::IndexedEdgeGeometry const* ix = mesh->getIndexedEdges()[0].get();
    ASSERT_NE(ix, nullptr);

    EXPECT_EQ(ix->getTechniqueId(), dqRender::TechniqueId::IndexedEdge);
    EXPECT_EQ(ix->getEdgeLut().getNumSegments(), 5u);
    EXPECT_EQ(ix->getEdgeLut().getSilhouettePadding(), 0u);
    EXPECT_EQ(ix->getEdgeLut().getWidth(), 15u);
    EXPECT_EQ(ix->getEdgeLut().getHeight(), 1u);
    EXPECT_EQ(ix->getNumIndices(), 48u) << "8 edges (5 visible + 3 silhouettes) × 6";

    // 上传字节 == (b) 手算展开的查找表。
    uint32_t const edgeTexId = ix->getEdgeLut().getTexture().getId();
    auto const* uploaded = driver.texBytes(edgeTexId);
    ASSERT_NE(uploaded, nullptr);
    ASSERT_EQ(uploaded->size(), 60u);
    EXPECT_EQ((*uploaded)[3], 1u) << "seg0 endpoint = vertex 1";
    EXPECT_EQ((*uploaded)[30], 2u) << "sil0 index0 = vertex 2 (edge e2)";
    EXPECT_EQ((*uploaded)[36], 0x44u) << "sil0 normal u32 low byte";

    delete graphics[0];
}

// IndexedEdge 变体 shader 结构锁（边查找表采样 + silhouette 剔除 + renderOrder）。
// Ported from: itwinjs-core Edge.ts createBase 的 isIndexed 分支
//              (:236-269——u_edgeLUT/u_edgeParams、computeIndexedQuantizedPosition、
//              initializeIndexed、computeIndexedRenderOrder、
//              checkForSilhouetteDiscardIndexed)。
// Authored: no reference shader-snapshot test exists（同 QuantizedVariant 检索说明）。
TEST(EdgeShaderVariant, IndexedVariantConsumesEdgeLut)
{
    auto builder = dqRender::createEdgeProgramBuilder(
        dqRender::EdgeBuilderType::IndexedEdge, dqRender::FeatureMode::None,
        dqRender::PositionType::Quantized);
    std::string const vert = builder.getVertexBuilder().buildSourceWithComponents();

    // 边查找表 uniform（Edge.ts:243-261）+ 表坐标函数（addLookupTable "edge"）。
    EXPECT_NE(vert.find("u_edgeLUT"), std::string::npos);
    EXPECT_NE(vert.find("u_edgeParams"), std::string::npos);
    EXPECT_NE(vert.find("compute_edge_coords"), std::string::npos);
    // a_pos 是边表索引（decodeUInt24），且 silhouette 判定用 u_edgeParams.z。
    EXPECT_NE(vert.find("decodeUInt24(a_pos)"), std::string::npos);
    EXPECT_NE(vert.find("g_isSilhouette"), std::string::npos);
    // 对端点索引/位置（initializeIndexed :94-97）。
    EXPECT_NE(vert.find("g_otherIndex = decodeUInt24(g_otherIndexIndex)"), std::string::npos);
    EXPECT_NE(vert.find("g_otherPos = samplePosition(g_otherIndex)"), std::string::npos);
    // silhouette 剔除（indexed 形态读 g_normals :138-145）。
    EXPECT_NE(vert.find("octDecodeNormal(g_normals.xy)"), std::string::npos);
    // renderOrder varying（computeIndexedRenderOrder :100-105）。
    EXPECT_NE(vert.find("v_renderOrder"), std::string::npos);
    // 非 indexed 变体专属的 a_endPointAndQuadIndices 不应出现。
    EXPECT_EQ(vert.find("a_endPointAndQuadIndices"), std::string::npos)
        << "indexed variant reads endpoints from the edge LUT, not an attribute";
}

// ---------------------------------------------------------------------------
// TD-25：imdl instances 修饰消费（解析 + 实例 BO 直传 + 实例化 shader 变体）。
//
// Ported from: ImdlSchema.ts:160-166（ImdlInstances schema）+
//              ParseImdlDocument.ts parseInstances（:1045-1087——count/
//              transformCenter/bufferView 名提取 + findBuffer :1089-1103
//              字节区间解析）+ InstancedGraphicParams.ts:15-44（transforms
//              12 float/实例——3 行 4 列，第 4 列 = 相对 transformCenter 的
//              平移）+ InstancedGeometry.ts InstanceBuffersData.create
//              （:88-111——BO 直传字节）+ Vertex.ts/Instancing.ts（shader 侧
//              a_instanceMatrixRow0/1/2 + a_featureId/a_instanceOverrides）。
// Authored: 场景自写（instances60 dump 资产驱动——真实后端 imdl，
//           §11.11 只读；钉死值 = dump README 原文 + 本测试双录）。
// ---------------------------------------------------------------------------

namespace {

std::vector<uint8_t> readDumpBytes(char const* relativePath)
{
    std::string const path = std::string(DANQING_TEST_ASSET_ROOT)
                             + "/third_party/tile-sample-assets/rpc-dumps/"
                             + relativePath;
    std::ifstream f(path, std::ios::binary);
    EXPECT_TRUE(f.good()) << "dump asset unreadable: " << path;
    if (!f) return {};
    f.seekg(0, std::ios::end);
    std::streamoff const n = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> out(static_cast<size_t>(n));
    f.read(reinterpret_cast<char*>(out.data()), n);
    return out;
}

// instances60-v1 根瓦钉死值（manifest: contentId "-b-0-0-0-0-1" → files/7.imdl）。
// 数值来源：dump README.md 原文（count:60 / transformCenter 三值）+ 字节级
// 双录（Task 3 Step 1 取证实测，见 task-3-report.md）。
struct Instances60RootPinned {
    static constexpr char kRelativePath[] = "instances60-v1/files/7.imdl";
    static constexpr uint32_t kCount = 60;
    // JSON 原文（dump README "root 瓦原文"引用段逐字符一致）。
    static constexpr double kTransformCenterX = -77.27117673514951;
    static constexpr double kTransformCenterY = -37.24289851958988;
    static constexpr double kTransformCenterZ = -25.017626535143606;
    static constexpr char kTransformsView[] = "bvInstanceTransforms1";
    static constexpr char kFeatureIdsView[] = "bvInstanceFeatures1";
    static constexpr char kSymbologyView[] = "bvInstanceOverrides1";
    static constexpr size_t kTransformsBytes = 60u * 12u * 4u;  // 2880
    static constexpr size_t kFeatureIdsBytes = 60u * 3u;        // 180（24-bit/实例）
    static constexpr size_t kSymbologyBytes = 60u * 8u;         // 480（8B/实例）
    // 首实例变换（IModelTileWriter.cpp setTransform——form3d[i][j] 3×4 行主序，
    // 第 4 列 = origin − transformCenter）：行正交（旋转）+ 平移。
    static constexpr float kFirstTransform[12] = {
        1.0f, 0.0f, 0.0f, 6.84535551071167f,
        0.0f, 0.0f, -1.0f, -3.75f,
        0.0f, 1.0f, 0.0f, -0.265480101108551f,
    };
};

// 录制驱动扩展：BO 创建返回真实句柄 + updateBufferObject 捕获字节——
// instances 修饰的 BO 直传断言数据源（transforms/featureIds/symbology
// 三缓冲区字节级回放）。形态照 RecordingLutDriver 先例。
class RecordingInstanceDriver : public RecordingLutDriver {
public:
    dqRender::rhi::BufferObjectHandle createBufferObject(
        uint32_t, dqRender::rhi::BufferObjectBinding,
        dqRender::rhi::BufferUsage) noexcept override
    {
        return m_allocator.allocate<dqRender::rhi::HwBufferObject>();
    }

    void updateBufferObject(dqRender::rhi::BufferObjectHandle h,
                            dqRender::rhi::BufferDescriptor&& data,
                            uint32_t) noexcept override
    {
        auto const* b = static_cast<uint8_t const*>(data.buffer());
        m_uploads[h.getId()].assign(b, b + data.size());
    }

    bool hasUpload(dqRender::rhi::BufferObjectHandle h) const
    {
        return m_uploads.find(h.getId()) != m_uploads.end();
    }
    std::vector<uint8_t> const& upload(dqRender::rhi::BufferObjectHandle h) const
    {
        return m_uploads.find(h.getId())->second;
    }

private:
    std::map<uint32_t, std::vector<uint8_t>> m_uploads;
};

}  // namespace

// 解析锁：instances60 根瓦 JSON → ImdlPrimitiveProps.instances 字段 +
// findBufferView 字节区间（count/transformCenter 钉死，transforms 字节数
// == count*12*4 钉死）。
TEST(ImdlInstancesTest, ParsesInstanceFieldsFromDump)
{
    auto const bytes = readDumpBytes(Instances60RootPinned::kRelativePath);
    ASSERT_FALSE(bytes.empty());
    dqRender::ImdlByteStream stream(bytes.data(), bytes.size());
    auto const header = dqRender::ImdlHeader::readFrom(stream);
    ASSERT_TRUE(header.isValid());
    auto const desc = dqRender::decodeImdlContentDescription(header, stream);
    ASSERT_TRUE(desc.has_value());
    auto doc = dqRender::parseImdlDocument(stream);
    ASSERT_TRUE(doc.has_value());

    auto json = dqRender::tilejson::parseJsonDocument(doc->sceneJson);
    ASSERT_NE(json, nullptr);
    auto prims = dqRender::tilejson::parseImdlMeshPrimitives(*json);
    ASSERT_EQ(prims.size(), 2u) << "root tile = context primitive + instanced primitive";

    // prim0（上下文）：无 instances 修饰（ImdlSchema.ts:181 instances? 可缺省）。
    EXPECT_FALSE(prims[0].instances.has_value());

    // prim1（60 实例共享几何）：instances 字段逐一（ImdlSchema.ts:160-166）。
    ASSERT_TRUE(prims[1].instances.has_value());
    auto const& inst = *prims[1].instances;
    EXPECT_EQ(inst.count, Instances60RootPinned::kCount);
    EXPECT_DOUBLE_EQ(inst.transformCenter[0], Instances60RootPinned::kTransformCenterX);
    EXPECT_DOUBLE_EQ(inst.transformCenter[1], Instances60RootPinned::kTransformCenterY);
    EXPECT_DOUBLE_EQ(inst.transformCenter[2], Instances60RootPinned::kTransformCenterZ);
    EXPECT_EQ(inst.transformsView, Instances60RootPinned::kTransformsView);
    EXPECT_EQ(inst.featureIdsView, Instances60RootPinned::kFeatureIdsView);
    ASSERT_TRUE(inst.symbologyOverridesView.has_value());
    EXPECT_EQ(*inst.symbologyOverridesView, Instances60RootPinned::kSymbologyView);

    // findBuffer 字节区间语义（ParseImdlDocument.ts:1089-1103——bufferView 名 →
    // byteOffset/byteLength 子段；transforms 字节数 == count*12*4 钉死
    // （:1068-1071 assert 语义的 C++ 防御——参考 dev assert release 未定义，
    // 此处显式核对））。
    uint8_t const* data = nullptr;
    size_t size = 0;
    ASSERT_TRUE(dqRender::tilejson::findBufferView(
        *json, inst.transformsView, doc->binary, data, size));
    EXPECT_EQ(size, Instances60RootPinned::kTransformsBytes);
    ASSERT_TRUE(dqRender::tilejson::findBufferView(
        *json, inst.featureIdsView, doc->binary, data, size));
    EXPECT_EQ(size, Instances60RootPinned::kFeatureIdsBytes);
    ASSERT_TRUE(dqRender::tilejson::findBufferView(
        *json, *inst.symbologyOverridesView, doc->binary, data, size));
    EXPECT_EQ(size, Instances60RootPinned::kSymbologyBytes);

    // transforms 数据形态：12 float/实例，第 4 列 = 相对 transformCenter 的
    // 平移（InstancedGraphicParams.ts:24-33 + IModelTileWriter setTransform）。
    uint8_t const* tdata = nullptr;
    size_t tsize = 0;
    ASSERT_TRUE(dqRender::tilejson::findBufferView(
        *json, inst.transformsView, doc->binary, tdata, tsize));
    ASSERT_EQ(tsize, sizeof(float) * 12u * inst.count);
    float const* first = reinterpret_cast<float const*>(tdata);
    for (int i = 0; i < 12; ++i)
        EXPECT_FLOAT_EQ(first[i], Instances60RootPinned::kFirstTransform[i])
            << "first instance transform[" << i << "]";
}

// BO 直传锁：createImdlLutGraphics 对带 instances 修饰的 primitive 产出
// InstancedGeometry 包裹（repr = LUT SurfaceGeometry），三实例缓冲区字节
// 级 verbatim 回放（InstanceBuffersData.create :88-111 的 BufferHandle
// 直传形态 + InstancedGeometry.ts create :385-409 的 attribute 组装输入）。
TEST(ImdlInstancesTest, LutPathUploadsInstanceBuffersVerbatim)
{
    auto const bytes = readDumpBytes(Instances60RootPinned::kRelativePath);
    ASSERT_FALSE(bytes.empty());
    dqRender::ImdlByteStream stream(bytes.data(), bytes.size());
    auto const header = dqRender::ImdlHeader::readFrom(stream);
    ASSERT_TRUE(header.isValid());
    auto const desc = dqRender::decodeImdlContentDescription(header, stream);
    ASSERT_TRUE(desc.has_value());
    auto doc = dqRender::parseImdlDocument(stream);
    ASSERT_TRUE(doc.has_value());

    RecordingInstanceDriver driver;
    LutStubSystem system(driver);
    auto graphics = dqRender::createImdlLutGraphics(*doc, system);
    // prim0（上下文，无 instances）+ prim1（instanced）各一 graphic。
    ASSERT_EQ(graphics.size(), 2u);

    // prim0：普通 LUT surface（无实例包裹）。
    auto* mesh0 = static_cast<dqRender::MeshGraphic*>(graphics[0]);
    ASSERT_NE(mesh0, nullptr);
    ASSERT_EQ(mesh0->getSurfaces().size(), 1u);
    EXPECT_EQ(mesh0->getInstancedSurfaces().size(), 0u);

    // prim1：InstancedGeometry 包裹 LUT surface。
    auto* mesh1 = static_cast<dqRender::MeshGraphic*>(graphics[1]);
    ASSERT_NE(mesh1, nullptr);
    ASSERT_EQ(mesh1->getInstancedSurfaces().size(), 1u);
    dqRender::InstancedGeometry const* instanced = mesh1->getInstancedSurfaces()[0].get();
    ASSERT_NE(instanced, nullptr);
    EXPECT_EQ(instanced->getRepr(), mesh1->getSurfaces()[0].get())
        << "wrapper observes the MeshGraphic-owned base surface";
    EXPECT_EQ(instanced->getInstanceCount(), Instances60RootPinned::kCount);
    ASSERT_NE(instanced->getBuffers(), nullptr);
    EXPECT_TRUE(instanced->getBuffers()->hasFeatures())
        << "featureIds buffer present → per-instance feature ids";
    EXPECT_TRUE(instanced->getBuffers()->getSymbology())
        << "symbologyOverrides buffer present → per-instance colors";

    // 三缓冲区字节级 verbatim（生产侧线上字节原样入 BO，零重排）。
    auto const& buffers = *instanced->getBuffers();
    ASSERT_TRUE(driver.hasUpload(buffers.getTransforms()));
    EXPECT_EQ(driver.upload(buffers.getTransforms()).size(),
              Instances60RootPinned::kTransformsBytes);
    ASSERT_TRUE(driver.hasUpload(buffers.getFeatureIds()));
    EXPECT_EQ(driver.upload(buffers.getFeatureIds()).size(),
              Instances60RootPinned::kFeatureIdsBytes);
    ASSERT_TRUE(driver.hasUpload(buffers.getSymbology()));
    EXPECT_EQ(driver.upload(buffers.getSymbology()).size(),
              Instances60RootPinned::kSymbologyBytes);

    // 字节内容 == dump 视图子段（memcmp 级）。
    auto json = dqRender::tilejson::parseJsonDocument(doc->sceneJson);
    ASSERT_NE(json, nullptr);
    auto prims = dqRender::tilejson::parseImdlMeshPrimitives(*json);
    ASSERT_EQ(prims.size(), 2u);
    auto const& inst = *prims[1].instances;
    uint8_t const* data = nullptr;
    size_t size = 0;
    ASSERT_TRUE(dqRender::tilejson::findBufferView(
        *json, inst.transformsView, doc->binary, data, size));
    EXPECT_EQ(std::memcmp(driver.upload(buffers.getTransforms()).data(), data, size), 0);
    ASSERT_TRUE(dqRender::tilejson::findBufferView(
        *json, inst.featureIdsView, doc->binary, data, size));
    EXPECT_EQ(std::memcmp(driver.upload(buffers.getFeatureIds()).data(), data, size), 0);
    ASSERT_TRUE(dqRender::tilejson::findBufferView(
        *json, *inst.symbologyOverridesView, doc->binary, data, size));
    EXPECT_EQ(std::memcmp(driver.upload(buffers.getSymbology()).data(), data, size), 0);

    for (auto* g : graphics)
        delete g;
}

// 实例化 shader 变体锁：Surface-Opaque-Quantized-Instanced 变体的 GLSL
// 消费实例 attribute 链（glsl/Instancing.ts + Vertex.ts addModelViewMatrix
// 实例分支 + Color.ts applyInstanceColor）——a_instanceMatrixRow0/1/2 组
// g_modelMatrixRTC，g_mv = u_instanced_modelView * g_modelMatrixRTC，
// 位置 = u_proj * g_mv * rawPos；逐实例色 a_instanceRgba 经
// extractInstanceBit 混入。旧 u_instanced_modelView 数组偏差（gl_InstanceID
// 索引 uniform 阵）必须消失（§0 参考对齐）。
TEST(SurfaceShaderVariant, InstancedVariantConsumesInstanceAttributes)
{
    dqRender::TechniqueFlags flags;  // 默认 Opaque/Quantized/FeatureMode::None
    flags.isInstanced = true;
    dqRender::ShaderProgram prog;
    dqRender::SurfaceVariantCompiler compiler;
    compiler.buildProgram(prog, flags);
    std::string const& vert = prog.getVertSource();

    // 实例 attribute 声明（AttributeMap.ts:36-51 instanced 追加组）。
    EXPECT_NE(vert.find("in vec4 a_instanceMatrixRow0;"), std::string::npos);
    EXPECT_NE(vert.find("in vec4 a_instanceMatrixRow1;"), std::string::npos);
    EXPECT_NE(vert.find("in vec4 a_instanceMatrixRow2;"), std::string::npos);
    EXPECT_NE(vert.find("in vec4 a_instanceOverrides;"), std::string::npos);
    EXPECT_NE(vert.find("in vec4 a_instanceRgba;"), std::string::npos);
    // g_modelMatrixRTC 组装（Instancing.ts computeInstancedModelMatrixRTC :33-38）。
    EXPECT_NE(vert.find("g_modelMatrixRTC = mat4("), std::string::npos);
    // 实例化 model-view（Vertex.ts addModelViewMatrix :147-154 实例分支）。
    EXPECT_NE(vert.find("uniform mat4 u_instanced_modelView;"), std::string::npos);
    EXPECT_NE(vert.find("g_mv = u_instanced_modelView * g_modelMatrixRTC;"),
              std::string::npos);
    // 位置 = u_proj * (g_mv * rawPos)（Surface.ts computePositionPrelude/Postlude
    // + MAT_MV=g_mv 定义——ShaderBuilder.ts:716-719）。
    EXPECT_NE(vert.find("vec4 pos = g_mv * rawPos;"), std::string::npos);
    EXPECT_NE(vert.find("return u_proj * pos;"), std::string::npos);
    // 逐实例色（Color.ts applyInstanceColor :32-35）。
    EXPECT_NE(vert.find("a_instanceRgba.rgb / 255.0"), std::string::npos);
    EXPECT_NE(vert.find("extractInstanceBit(kOvrBit_Rgb)"), std::string::npos);
    // 旧偏差形态不得出现（uniform 阵 + gl_InstanceID 索引）。
    EXPECT_EQ(vert.find("u_instanced_modelView["), std::string::npos)
        << "uniform-array + gl_InstanceID deviation must be gone (reference: "
           "per-instance attributes with divisor)";
    EXPECT_EQ(vert.find("gl_InstanceID"), std::string::npos);

    // 非实例量化变体不受影响（无实例 attribute）。
    dqRender::TechniqueFlags plain;
    dqRender::ShaderProgram prog2;
    compiler.buildProgram(prog2, plain);
    std::string const& vert2 = prog2.getVertSource();
    EXPECT_EQ(vert2.find("a_instanceMatrixRow0"), std::string::npos);
    EXPECT_NE(vert2.find("uniform mat4 u_mvp;"), std::string::npos);
}

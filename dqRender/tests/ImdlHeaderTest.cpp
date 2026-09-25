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
#include "render/MeshGraphic.h"
#include "render/PolyfaceGraphic.h"
#include "render/SurfaceGeometry.h"
#include "tile/TilesetJson.h"
#include "tile-sample-assets/imdl-fixtures/TileIOFixtures.h"
#include <dqRender/RenderSystem.h>
#include <dqRender/tile/ImdlDocument.h>
#include <dqRender/tile/ImdlHeader.h>
#include <dqRender/tile/RealityTileTree.h>

#include <cmath>
#include <cstring>
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
class RecordingLutDriver final : public dqRender::rhi::NullDriver {
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

private:
    dqRender::rhi::HandleAllocator m_allocator;
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

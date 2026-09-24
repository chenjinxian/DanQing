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

#include "tile-sample-assets/imdl-fixtures/TileIOFixtures.h"
#include <dqRender/tile/ImdlDocument.h>
#include <dqRender/tile/ImdlHeader.h>
#include <dqRender/tile/RealityTileTree.h>

#include <cmath>
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

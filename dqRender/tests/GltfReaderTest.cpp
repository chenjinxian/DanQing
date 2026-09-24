// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core or imodel-native for cgltf reader integration
// DanQing dqRender — GltfReader tests
#include <gtest/gtest.h>

#include <dqRender/GltfReader.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/PolyfaceData.h>
#include <dqCommon/Image.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

#ifndef DANQING_TEST_ASSET_ROOT
#define DANQING_TEST_ASSET_ROOT "."
#endif

using namespace dqRender;

// ---------------------------------------------------------------------------
// Helper: create a minimal GLB file and write to temp path
// Single-triangle mesh with positions and indices.
// ---------------------------------------------------------------------------
static std::string WriteTestGlb()
{
    // Minimal glTF 2.0 JSON
    const char* json = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-1,-1,0],"max":[1,1,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],"buffers":[{"byteLength":42}]})";

    size_t jsonLen = strlen(json);
    size_t jsonPadded = (jsonLen + 3) & ~3;
    size_t binPadded = (42 + 3) & ~3;
    size_t totalLen = 12 + (8 + jsonPadded) + (8 + binPadded);

    std::vector<uint8_t> glb(totalLen);
    size_t off = 0;

    // GLB header
    glb[off++] = 'g'; glb[off++] = 'l'; glb[off++] = 'T'; glb[off++] = 'F';
    *reinterpret_cast<uint32_t*>(&glb[off]) = 2; off += 4;
    *reinterpret_cast<uint32_t*>(&glb[off]) = static_cast<uint32_t>(totalLen); off += 4;

    // JSON chunk
    *reinterpret_cast<uint32_t*>(&glb[off]) = static_cast<uint32_t>(jsonPadded); off += 4;
    *reinterpret_cast<uint32_t*>(&glb[off]) = 0x4E4F534A; off += 4;  // JSON
    memcpy(&glb[off], json, jsonLen); off += jsonLen;
    for (size_t i = jsonLen; i < jsonPadded; ++i) glb[off++] = ' ';

    // BIN chunk
    *reinterpret_cast<uint32_t*>(&glb[off]) = static_cast<uint32_t>(binPadded); off += 4;
    *reinterpret_cast<uint32_t*>(&glb[off]) = 0x004E4942; off += 4;  // BIN

    // 3 vertices (vec3 float)
    float verts[] = {0,0,0, 1,0,0, 0,1,0};
    memcpy(&glb[off], verts, 36); off += 36;

    // 3 indices (uint16)
    uint16_t idx[] = {0, 1, 2};
    memcpy(&glb[off], idx, 6);

    // Write to temp file
    std::string path = "/tmp/danqing_gltf_test.glb";
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<char const*>(glb.data()), static_cast<std::streamsize>(glb.size()));
    return path;
}

// Authored: adapted from GltfReader.test.ts NonIndexedTrianglesBounding box cases
//           (core/frontend/src/test/tile/GltfReader.test.ts:376-391) — DanQing's
//           reader output type differs (IndexedPolyface), scenario/assertion shape ported.
static std::string WriteTestGlbNonIndexed()
{
    const char* json = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-1,-1,0],"max":[1,1,0]}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36}],"buffers":[{"byteLength":36}]})";

    size_t jsonLen = strlen(json);
    size_t jsonPadded = (jsonLen + 3) & ~3;
    size_t binPadded = (36 + 3) & ~3;
    size_t totalLen = 12 + (8 + jsonPadded) + (8 + binPadded);

    std::vector<uint8_t> glb(totalLen);
    size_t off = 0;
    glb[off++] = 'g'; glb[off++] = 'l'; glb[off++] = 'T'; glb[off++] = 'F';
    *reinterpret_cast<uint32_t*>(&glb[off]) = 2; off += 4;
    *reinterpret_cast<uint32_t*>(&glb[off]) = static_cast<uint32_t>(totalLen); off += 4;
    *reinterpret_cast<uint32_t*>(&glb[off]) = static_cast<uint32_t>(jsonPadded); off += 4;
    *reinterpret_cast<uint32_t*>(&glb[off]) = 0x4E4F534A; off += 4;
    memcpy(&glb[off], json, jsonLen); off += jsonLen;
    for (size_t i = jsonLen; i < jsonPadded; ++i) glb[off++] = ' ';
    *reinterpret_cast<uint32_t*>(&glb[off]) = static_cast<uint32_t>(binPadded); off += 4;
    *reinterpret_cast<uint32_t*>(&glb[off]) = 0x004E4942; off += 4;
    float verts[] = {0,0,0, 1,0,0, 0,1,0};
    memcpy(&glb[off], verts, 36);

    std::string path = "/tmp/danqing_gltf_test_nonindexed.glb";
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<char const*>(glb.data()), static_cast<std::streamsize>(glb.size()));
    return path;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
// Authored: no reference test exists in itwinjs-core or imodel-native for cgltf reader integration
TEST(GltfReaderTest, NonexistentFileReturnsNull)
{
    auto scene = GltfReader::LoadFromFile("/nonexistent/path/model.glb");
    EXPECT_EQ(scene, nullptr);
    EXPECT_FALSE(GltfReader::getLastError().empty());
}

// Authored: no reference test exists in itwinjs-core or imodel-native for cgltf reader integration
TEST(GltfReaderTest, InvalidDataReturnsNull)
{
    uint8_t garbage[] = {0x00, 0x01, 0x02, 0x03};
    auto scene = GltfReader::LoadFromMemory(garbage, sizeof(garbage));
    EXPECT_EQ(scene, nullptr);
}

// Authored: no reference test exists in itwinjs-core or imodel-native for cgltf reader integration
TEST(GltfReaderTest, EmptyDataReturnsNull)
{
    auto scene = GltfReader::LoadFromMemory(nullptr, 0);
    EXPECT_EQ(scene, nullptr);
}

// Authored: no reference test exists in itwinjs-core or imodel-native for cgltf reader integration
TEST(GltfReaderTest, LoadFromFile)
{
    std::string path = WriteTestGlb();
    auto scene = GltfReader::LoadFromFile(path);
    ASSERT_NE(scene, nullptr) << "Error: " << GltfReader::getLastError();
    EXPECT_EQ(scene->meshes.size(), 1u);
}

// Authored: no reference test exists in itwinjs-core or imodel-native for cgltf reader integration
TEST(GltfReaderTest, MeshHasVertices)
{
    std::string path = WriteTestGlb();
    auto scene = GltfReader::LoadFromFile(path);
    ASSERT_NE(scene, nullptr) << "Error: " << GltfReader::getLastError();
    ASSERT_EQ(scene->meshes.size(), 1u);

    auto const& pf = scene->meshes[0].polyface;
    ASSERT_FALSE(pf.IsNull());
    EXPECT_EQ(pf->Data().PointCount(), 3u);
}

// Authored: no reference test exists in itwinjs-core or imodel-native for cgltf reader integration
TEST(GltfReaderTest, MeshHasIndices)
{
    std::string path = WriteTestGlb();
    auto scene = GltfReader::LoadFromFile(path);
    ASSERT_NE(scene, nullptr) << "Error: " << GltfReader::getLastError();
    ASSERT_EQ(scene->meshes.size(), 1u);

    auto const& pf = scene->meshes[0].polyface;
    ASSERT_FALSE(pf.IsNull());
    EXPECT_EQ(pf->Data().IndexCount(), 3u);
    EXPECT_EQ(pf->FacetCount(), 1u);
}

// Authored: no reference test exists in itwinjs-core or imodel-native for cgltf reader integration
TEST(GltfReaderTest, VertexPositions)
{
    std::string path = WriteTestGlb();
    auto scene = GltfReader::LoadFromFile(path);
    ASSERT_NE(scene, nullptr);
    ASSERT_EQ(scene->meshes.size(), 1u);

    auto const& data = scene->meshes[0].polyface->Data();
    ASSERT_EQ(data.PointCount(), 3u);

    auto p0 = data.GetPoint(1);  // 1-based
    EXPECT_FLOAT_EQ(static_cast<float>(p0.x), 0.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(p0.y), 0.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(p0.z), 0.0f);

    auto p1 = data.GetPoint(2);
    EXPECT_FLOAT_EQ(static_cast<float>(p1.x), 1.0f);

    auto p2 = data.GetPoint(3);
    EXPECT_FLOAT_EQ(static_cast<float>(p2.y), 1.0f);
}

// Authored: no reference test exists in itwinjs-core or imodel-native for cgltf reader integration
// 2026-09-15 更新：readGltfTemplate 恒 yAxisUp=true（GltfReader.ts:2687-2690，
// "glTF supports exactly one coordinate system with y axis up"）→ 根变换
// rotX(+90°) (x,y,z)->(x,-z,y)。夹具三角面 (0,0,0),(1,0,0),(0,1,0)（z=0）
// → (0,0,0),(1,0,0),(0,0,1)：y 恒 0、z∈[0,1]。
TEST(GltfReaderTest, SceneBounds)
{
    std::string path = WriteTestGlb();
    auto scene = GltfReader::LoadFromFile(path);
    ASSERT_NE(scene, nullptr);

    EXPECT_FALSE(scene->bounds.isNull());
    EXPECT_DOUBLE_EQ(scene->bounds.low.x, 0.0);
    EXPECT_DOUBLE_EQ(scene->bounds.high.x, 1.0);
    EXPECT_DOUBLE_EQ(scene->bounds.low.y, 0.0);
    EXPECT_DOUBLE_EQ(scene->bounds.high.y, 0.0);   // 原 y∈[0,1] → rotX90 后恒 0
    EXPECT_DOUBLE_EQ(scene->bounds.high.z, 1.0);   // 原 y∈[0,1] → z∈[0,1]
}

// Authored: no reference test exists in itwinjs-core or imodel-native for cgltf reader integration
TEST(GltfReaderTest, DefaultMaterial)
{
    std::string path = WriteTestGlb();
    auto scene = GltfReader::LoadFromFile(path);
    ASSERT_NE(scene, nullptr);
    ASSERT_EQ(scene->meshes.size(), 1u);

    auto const& mesh = scene->meshes[0];
    EXPECT_FLOAT_EQ(mesh.baseColorFactor[0], 1.0f);
    EXPECT_FLOAT_EQ(mesh.baseColorFactor[1], 1.0f);
    EXPECT_FLOAT_EQ(mesh.baseColorFactor[2], 1.0f);
    EXPECT_FLOAT_EQ(mesh.baseColorFactor[3], 1.0f);
}

// Authored: no reference test exists in itwinjs-core or imodel-native for cgltf reader integration
// 2026-09-15 更新：无节点矩阵的 glTF 其网格变换不再是单位阵——readGltfTemplate
// 恒 yAxisUp=true（GltfReader.ts:2687-2690 + getTileTransform :591-592），根变换
// = rotX(+90°)：rows (1,0,0 / 0,0,-1 / 0,1,0)，origin (0,0,0)。
TEST(GltfReaderTest, DefaultTransform)
{
    std::string path = WriteTestGlb();
    auto scene = GltfReader::LoadFromFile(path);
    ASSERT_NE(scene, nullptr);
    ASSERT_EQ(scene->meshes.size(), 1u);

    auto const& tf = scene->meshes[0].transform;
    EXPECT_DOUBLE_EQ(tf.matrix.coffs[0], 1.0);
    EXPECT_DOUBLE_EQ(tf.matrix.coffs[4], 0.0);
    EXPECT_DOUBLE_EQ(tf.matrix.coffs[5], -1.0);
    EXPECT_DOUBLE_EQ(tf.matrix.coffs[7], 1.0);
    EXPECT_DOUBLE_EQ(tf.origin.x, 0.0);
    EXPECT_DOUBLE_EQ(tf.origin.y, 0.0);
    EXPECT_DOUBLE_EQ(tf.origin.z, 0.0);
}

// Ported from: itwinjs-core GltfReader.test.ts:376-391 (non-indexed triangles —
//              DanQing asserts via IndexedPolyface counts/bounds, scenario unchanged).
TEST(GltfReaderTest, NonIndexedGeometryGetsSequentialIndices)
{
    std::string path = WriteTestGlbNonIndexed();
    auto scene = GltfReader::LoadFromFile(path);
    ASSERT_NE(scene, nullptr) << "Error: " << GltfReader::getLastError();
    ASSERT_EQ(scene->meshes.size(), 1u);

    auto const& data = scene->meshes[0].polyface->Data();
    EXPECT_EQ(data.PointCount(), 3u);
    EXPECT_EQ(data.IndexCount(), 3u);       // 0,1,2 -> 1-based 1,2,3
    EXPECT_EQ(scene->meshes[0].polyface->FacetCount(), 1u);
    EXPECT_FALSE(scene->bounds.isNull());   // bounds from positions (reference: :376-391)
    EXPECT_DOUBLE_EQ(scene->bounds.high.x, 1.0);
}

// Ported from: itwinjs-core GltfReader.test.ts:2195-2394 (resolveUrl suite —
//              baseUrl resolution of external resources; adapted: DanQing resolves
//              relative buffer/texture paths against the .gltf file's directory).
TEST(GltfReaderTest, ExternalGltfWithExternalBinLoads)
{
    // (brief's system("mkdir ... 2> nul") equivalent: create temp dir, no-op if
    //  it exists — std::filesystem::create_directories + temp_directory_path)
    std::filesystem::path dirPath =
        std::filesystem::temp_directory_path() / "danqing_gltf_external";
    (void)std::filesystem::create_directories(dirPath);
    std::string dir = dirPath.string();

    // model.gltf — JSON only; POSITION data lives in external model.bin
    const char* json = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],"buffers":[{"byteLength":42,"uri":"model.bin"}]})";
    {
        std::ofstream out(dir + "/model.gltf", std::ios::binary);
        out << json;
    }
    {
        // model.bin — 36B positions + 6B indices (same bytes as WriteTestGlb's BIN chunk)
        std::vector<uint8_t> bin(44);
        float verts[] = {0,0,0, 1,0,0, 0,1,0};
        uint16_t idx[] = {0, 1, 2};
        memcpy(bin.data(), verts, 36);
        memcpy(bin.data() + 36, idx, 6);
        std::ofstream out(dir + "/model.bin", std::ios::binary);
        out.write(reinterpret_cast<char const*>(bin.data()), 42);
    }

    auto scene = GltfReader::LoadFromFile(dir + "/model.gltf");
    ASSERT_NE(scene, nullptr) << "Error: " << GltfReader::getLastError();
    ASSERT_EQ(scene->meshes.size(), 1u);
    EXPECT_EQ(scene->meshes[0].polyface->Data().PointCount(), 3u);
}

// ---------------------------------------------------------------------------
// Texture helpers (spec C2 — TEXCOORD + baseColorTexture)
// ---------------------------------------------------------------------------
// Authored: reads the shared PNG asset (Task 2 DecodeImage fixture) — path is
// injected at compile time (DANQING_TEST_ASSET_ROOT), no CWD dependency.
static std::vector<uint8_t> ReadPngAsset()
{
    std::string path = DANQING_TEST_ASSET_ROOT "/dqCommon/tests/assets/red1x1.png";
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return {};
    size_t size = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(size);
    f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    return bytes;
}

// Authored: test-only base64 encoder (RFC 4648) to synthesize data-URI images.
static std::string Base64Encode(std::vector<uint8_t> const& in)
{
    static char const alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    for (size_t i = 0; i < in.size(); i += 3) {
        uint32_t v = in[i] << 16;
        if (i + 1 < in.size()) v |= in[i + 1] << 8;
        if (i + 2 < in.size()) v |= in[i + 2];
        out += alphabet[(v >> 18) & 63];
        out += alphabet[(v >> 12) & 63];
        out += (i + 1 < in.size()) ? alphabet[(v >> 6) & 63] : '=';
        out += (i + 2 < in.size()) ? alphabet[v & 63] : '=';
    }
    return out;
}

// Authored: adapted from GltfReader.test.ts embedded-image scenarios — builds a
// GLB whose material texture is either an in-BIN-chunk PNG (imagesJson empty →
// images:[{"bufferView":2}], PNG bytes land in the BIN chunk) or a data URI
// (imagesJson = the images array CONTENTS, e.g. {"uri":"data:..."}; the PNG
// bytes then stay OUT of the BIN chunk and bufferViews carries only pos/uv/
// idx). A TEXCOORD_0 accessor rides along. emissiveInstead=true swaps the
// material to emissiveTexture (extractTextureId fallback path).
// withNormalMap=true adds material.normalTexture pointing at the same image
// (the reference resolves normalTexture independently of the pattern texture
// but permits the same source — findTextureMapping :2578-2583).
static std::string WriteTexturedGlb(bool emissiveInstead, std::string const& imagesJson = "",
                                     bool withNormalMap = false)
{
    bool embeddedImage = imagesJson.empty();

    auto png = ReadPngAsset();
    // embedded layout: pos(36) uv(24) png(pad4) idx(6)
    // data-URI layout: pos(36) uv(24) idx(6) — no image bytes in BIN
    size_t pngOff = 60;
    size_t pngPadded = embeddedImage ? (png.size() + 3) & ~3 : 0;
    size_t idxOff = pngOff + pngPadded;
    size_t binLen = idxOff + 6;
    size_t totalBinPadded = (binLen + 3) & ~3;

    // 材质 JSON 两分支形状不同（baseColorTexture 嵌在 pbrMetallicRoughness 内），
    // 显式构造子串，避免格式串嵌套。
    char material[192];
    if (emissiveInstead) {
        snprintf(material, sizeof(material), "\"emissiveTexture\":{\"index\":0}%s",
                 withNormalMap ? ",\"normalTexture\":{\"index\":0}" : "");
    } else {
        snprintf(material, sizeof(material),
                 "\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}%s",
                 withNormalMap ? ",\"normalTexture\":{\"index\":0}" : "");
    }

    // bufferViews：embedded 版含 png 的 bufferView 2（indices 指向 3）；
    // data-URI 版只有 pos/uv/idx（indices 指向 2）。
    int idxAccessorView = embeddedImage ? 3 : 2;
    char bufferViews[256];
    if (embeddedImage) {
        snprintf(bufferViews, sizeof(bufferViews),
            "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
            "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":24},"
            "{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu},"
            "{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":6}",
            pngOff, png.size(), idxOff);
    } else {
        snprintf(bufferViews, sizeof(bufferViews),
            "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
            "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":24},"
            "{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":6}",
            idxOff);
    }

    char json[2048];
    int jsonLen = snprintf(json, sizeof(json),
        "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":1},\"indices\":2,\"material\":0}]}],"
        "\"materials\":[{%s}],"
        "\"textures\":[{\"source\":0}],\"images\":[%s],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
        "{\"bufferView\":%d,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"bufferViews\":[%s],"
        "\"buffers\":[{\"byteLength\":%zu}]}",
        material,
        embeddedImage ? "{\"bufferView\":2,\"mimeType\":\"image/png\"}" : imagesJson.c_str(),
        idxAccessorView, bufferViews, binLen);

    size_t jsonPadded = (static_cast<size_t>(jsonLen) + 3) & ~3;
    size_t totalLen = 12 + (8 + jsonPadded) + (8 + totalBinPadded);
    std::vector<uint8_t> glb(totalLen, 0);
    size_t off = 0;
    glb[off++] = 'g'; glb[off++] = 'l'; glb[off++] = 'T'; glb[off++] = 'F';
    *reinterpret_cast<uint32_t*>(&glb[off]) = 2; off += 4;
    *reinterpret_cast<uint32_t*>(&glb[off]) = static_cast<uint32_t>(totalLen); off += 4;
    *reinterpret_cast<uint32_t*>(&glb[off]) = static_cast<uint32_t>(jsonPadded); off += 4;
    *reinterpret_cast<uint32_t*>(&glb[off]) = 0x4E4F534A; off += 4;
    memcpy(&glb[off], json, static_cast<size_t>(jsonLen)); off += static_cast<size_t>(jsonLen);
    off = 12 + 8 + jsonPadded;
    *reinterpret_cast<uint32_t*>(&glb[off]) = static_cast<uint32_t>(totalBinPadded); off += 4;
    *reinterpret_cast<uint32_t*>(&glb[off]) = 0x004E4942; off += 4;
    float pos[] = {0,0,0, 1,0,0, 0,1,0};
    float uv[] = {0,0, 1,0, 0,1};
    memcpy(&glb[off], pos, 36); memcpy(&glb[off + 36], uv, 24);
    if (embeddedImage) {
        memcpy(&glb[off + pngOff], png.data(), png.size());
    }
    uint16_t idx[] = {0, 1, 2};
    memcpy(&glb[off + idxOff], idx, 6);

#ifdef _WIN32
    std::string dir = "C:/Windows/Temp";
#else
    std::string dir = "/tmp";
#endif
    std::string path = dir + (emissiveInstead ? "/danqing_gltf_emissive.glb"
                                              : "/danqing_gltf_textured.glb");
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<char const*>(glb.data()), static_cast<std::streamsize>(glb.size()));
    return path;
}

// ---------------------------------------------------------------------------
// Texture tests (spec C2)
// ---------------------------------------------------------------------------
// Authored: no reference test exists in itwinjs-core for baseColorTexture + UV
//              into polyface params; 行为锚定实现 GltfReader.ts
//              createDisplayParams(:1356) baseColorTexture path +
//              extractTextureId(:1228-1252).
TEST(GltfReaderTest, ReadsBaseColorTextureAndUv)
{
    std::string path = WriteTexturedGlb(false);
    auto scene = GltfReader::LoadFromFile(path);
    ASSERT_NE(scene, nullptr) << "Error: " << GltfReader::getLastError();
    ASSERT_EQ(scene->meshes.size(), 1u);

    auto const& mesh = scene->meshes[0];
    ASSERT_TRUE(mesh.baseColorTexture.has_value());
    EXPECT_EQ(mesh.baseColorTexture->width, 1);
    EXPECT_EQ(mesh.baseColorTexture->getHeight(), 1);
    // UVs landed in the polyface param arrays (Task 3 carrier).
    EXPECT_EQ(mesh.polyface->Data().ParamCount(), 3u);
    EXPECT_EQ(mesh.polyface->Data().paramIndex.size(), 3u);
    auto uv1 = mesh.polyface->Data().GetParam(2);  // 1-based
    EXPECT_DOUBLE_EQ(uv1.x, 1.0);
}

// Authored: no reference test exists in itwinjs-core for baseColorTexture miss
//              falling back to emissiveTexture; 行为锚定实现 GltfReader.ts
//              extractTextureId(:1228-1252).
TEST(GltfReaderTest, EmissiveTextureFallbackWhenNoBaseColor)
{
    std::string path = WriteTexturedGlb(true);
    auto scene = GltfReader::LoadFromFile(path);
    ASSERT_NE(scene, nullptr) << "Error: " << GltfReader::getLastError();
    ASSERT_EQ(scene->meshes.size(), 1u);
    EXPECT_TRUE(scene->meshes[0].baseColorTexture.has_value());
}

// Authored: no reference test exists in itwinjs-core for normalTexture parsing
//              into mesh params (GltfReader.test.ts only covers the Bentley
//              constant-LOD extension argument); 行为锚定实现 GltfReader.ts
//              extractNormalMapId(:1254-1262) + findTextureMapping(:2578-2583 —
//              normalMap resolved independently, isTransparent always false).
TEST(GltfReaderTest, ReadsNormalMapTexture)
{
    std::string path = WriteTexturedGlb(false, "", true);
    auto scene = GltfReader::LoadFromFile(path);
    ASSERT_NE(scene, nullptr) << "Error: " << GltfReader::getLastError();
    ASSERT_EQ(scene->meshes.size(), 1u);

    auto const& mesh = scene->meshes[0];
    ASSERT_TRUE(mesh.baseColorTexture.has_value());
    // normalTexture resolves independently of the pattern texture — both
    // populated when the material declares both (findTextureMapping :2578-2598).
    ASSERT_TRUE(mesh.normalMapTexture.has_value());
    EXPECT_EQ(mesh.normalMapTexture->width, 1);
    EXPECT_EQ(mesh.normalMapTexture->getHeight(), 1);
}

// Authored: no reference test exists for the normalTexture-absent case;
//              行为锚定 GltfReader.ts extractNormalMapId(:1254-1262 — returns
//              undefined when material.normalTexture is absent → nMap stays
//              undefined → no normal mapping).
TEST(GltfReaderTest, NoNormalMapWhenMaterialHasNone)
{
    std::string path = WriteTexturedGlb(false);
    auto scene = GltfReader::LoadFromFile(path);
    ASSERT_NE(scene, nullptr) << "Error: " << GltfReader::getLastError();
    ASSERT_EQ(scene->meshes.size(), 1u);
    EXPECT_TRUE(scene->meshes[0].baseColorTexture.has_value());
    EXPECT_FALSE(scene->meshes[0].normalMapTexture.has_value());
}

// Ported from: itwinjs-core GltfReader.test.ts:2195-2394 (resolveUrl — external
//              texture URI resolves against baseUrl) + resolveImage(:2518-2520).
TEST(GltfReaderTest, ExternalTextureUriResolvesAgainstBaseDir)
{
    auto png = ReadPngAsset();
    ASSERT_FALSE(png.empty());
    // C:/Windows/Temp/danqing_gltf_external — write a variant whose material
    // texture image has uri "tex.png" + external file.
    std::string dir =
#ifdef _WIN32
        "C:/Windows/Temp/danqing_gltf_external";
#else
        "/tmp/danqing_gltf_external";
#endif
    (void)std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir + "/tex.png", std::ios::binary);
        out.write(reinterpret_cast<char const*>(png.data()), static_cast<std::streamsize>(png.size()));
    }
    char json[2048];
    int jsonLen = snprintf(json, sizeof(json),
        "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":1},\"indices\":2,\"material\":0}]}],"
        "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}}],"
        "\"textures\":[{\"source\":0}],\"images\":[{\"uri\":\"tex.png\"}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
        "{\"bufferView\":2,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":24},"
        "{\"buffer\":0,\"byteOffset\":60,\"byteLength\":6}],"
        "\"buffers\":[{\"byteLength\":66,\"uri\":\"model.bin\"}]}");
    {
        std::ofstream out(dir + "/model_textured.gltf", std::ios::binary);
        out.write(json, jsonLen);
    }
    {
        std::vector<uint8_t> bin(66);
        float pos[] = {0,0,0, 1,0,0, 0,1,0};
        float uv[] = {0,0, 1,0, 0,1};
        uint16_t idx[] = {0, 1, 2};
        memcpy(bin.data(), pos, 36);
        memcpy(bin.data() + 36, uv, 24);
        memcpy(bin.data() + 60, idx, 6);
        std::ofstream out(dir + "/model.bin", std::ios::binary);
        out.write(reinterpret_cast<char const*>(bin.data()), 66);
    }

    auto scene = GltfReader::LoadFromFile(dir + "/model_textured.gltf");
    ASSERT_NE(scene, nullptr) << "Error: " << GltfReader::getLastError();
    ASSERT_TRUE(scene->meshes[0].baseColorTexture.has_value());
    EXPECT_EQ(scene->meshes[0].baseColorTexture->width, 1);
}

// Authored: no reference test exists in itwinjs-core for data-URI image decode;
//              行为锚定实现 GltfReader.ts resolveImage(:2487-2521) — data-URI
//              images decode via base64 (cgltf_load_buffer_base64).
TEST(GltfReaderTest, DataUriImageDecodes)
{
    auto png = ReadPngAsset();
    ASSERT_FALSE(png.empty());
    std::string dataUri = "data:image/png;base64," + Base64Encode(png);
    // WriteTexturedGlb's buffer layout minus the image bufferView — the images
    // array carries the data URI instead, and the PNG bytes stay out of the
    // BIN chunk (bufferViews: pos/uv/idx only). imagesJson = array contents
    // (WriteTexturedGlb supplies the surrounding [ ]).
    std::string imagesJson = "{\"uri\":\"" + dataUri + "\"}";
    std::string path = WriteTexturedGlb(false, imagesJson);
    auto scene = GltfReader::LoadFromFile(path);
    ASSERT_NE(scene, nullptr) << "Error: " << GltfReader::getLastError();
    ASSERT_EQ(scene->meshes.size(), 1u);
    ASSERT_TRUE(scene->meshes[0].baseColorTexture.has_value());
    EXPECT_EQ(scene->meshes[0].baseColorTexture->width, 1);
    EXPECT_EQ(scene->meshes[0].baseColorTexture->getHeight(), 1);
}


// Ported from: itwinjs-core GltfReader.test.ts "throws during traversal if scene
//              contains cycles" (L287-299) + GltfSchema.ts traverseGltfNodes
//              (L332-346) — a `traversed` set persists across the WHOLE traversal;
//              revisiting any node throws "Cycle detected while traversing glTF
//              nodes". DanQing 错误通道 = 拒载 + sLastError。
TEST(GltfReaderTest, NodeCycleRejected)
{
    std::filesystem::path dirPath =
        std::filesystem::temp_directory_path() / "danqing_gltf_cycle";
    (void)std::filesystem::create_directories(dirPath);
    std::string const path = dirPath.string() + "/cycle.gltf";
    const char* json = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"children":[1]},{"children":[0]}],"meshes":[]})";
    {
        std::ofstream out(path, std::ios::binary);
        out << json;
    }
    auto scene = GltfReader::LoadFromFile(path);
    // 两种合法拒载：cgltf schema 层（invalid_gltf——无 mesh 的节点图不合 glTF
    // 有效载荷）或遍历环防护（"Cycle detected"）。任一即环/无效场景被拒。
    EXPECT_EQ(scene, nullptr);
    EXPECT_FALSE(GltfReader::getLastError().empty());
}


// Authored: 全语料 reader 加载探针（2026-09-16）——枚举 Models 下全部 glb，
// 仅 LoadFromFile（无渲染/窗口），输出逐模型 meshes 数与拒载原因到
// build/parity-reader-report.txt。DANQING_READER_BATCH=Models根目录 启用。
TEST(GltfReaderTest, BatchAllSampleModelsLoad)
{
    char const* root = getenv("DANQING_READER_BATCH");
    if (!root || !*root)
        GTEST_SKIP() << "set DANQING_READER_BATCH=<Models root>";

    FILE* report = fopen("build/parity-reader-report.txt", "w");
    ASSERT_NE(report, nullptr);

    std::vector<std::string> glbs;
    std::string const pat = std::string(root) + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pat.c_str(), &fd);
    ASSERT_NE(hFind, INVALID_HANDLE_VALUE);
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') continue;
        std::string const glbDir = std::string(root) + "\\" + fd.cFileName + "\\glTF-Binary";
        WIN32_FIND_DATAA fd2;
        HANDLE h2 = FindFirstFileA((glbDir + "\\*.glb").c_str(), &fd2);
        if (h2 != INVALID_HANDLE_VALUE) {
            glbs.push_back(glbDir + "\\" + fd2.cFileName);
            FindClose(h2);
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    std::sort(glbs.begin(), glbs.end());
    fprintf(report, "# reader batch — %zu glb under %s\n", glbs.size(), root);

    int nOk = 0, nFail = 0;
    for (auto const& g : glbs) {
        auto scene = GltfReader::LoadFromFile(g);
        std::string const name = g.substr(g.find_last_of("\\/") + 1);
        if (scene) {
            size_t pts = 0;
            for (auto const& m : scene->meshes)
                if (!m.polyface.IsNull()) pts += m.polyface->Data().points.size();
            printf("[RBATCH] OK %s meshes=%zu pts=%zu\n", name.c_str(), scene->meshes.size(), pts);
            fprintf(report, "OK        %s meshes=%zu pts=%zu\n", name.c_str(), scene->meshes.size(), pts);
            ++nOk;
        } else {
            printf("[RBATCH] FAIL %s err=%s\n", name.c_str(), GltfReader::getLastError().c_str());
            fprintf(report, "LOAD-FAIL %s err=%s\n", name.c_str(), GltfReader::getLastError().c_str());
            ++nFail;
        }
    }
    fprintf(report, "# summary: OK=%d LOAD-FAIL=%d (total=%zu)\n", nOk, nFail, glbs.size());
    fclose(report);
    printf("[RBATCH] summary: OK=%d LOAD-FAIL=%d (total=%zu)\n", nOk, nFail, glbs.size());
}


// Authored: SimpleInstancing cgltf_parse 拒因二分探针（2026-09-16）。
TEST(GltfReaderTest, InstancingParseBisect)
{
    for (char const* v : {"a-orig", "b-no-extused", "c-no-nodeext", "d-bare"}) {
        std::string const path = std::string("build/instancing-probe/") + v + ".glb";
        auto scene = GltfReader::LoadFromFile(path);
        printf("[IB] %s scene=%s err='%s'\n", v, scene ? "ok" : "null",
               GltfReader::getLastError().c_str());
    }
    SUCCEED();
}


// Authored: EXT_mesh_gpu_instancing 展开验证（2026-09-16）——SimpleInstancing
// 的 125 实例（Khronos spec；DanQing 经 GltfMesh.instances 提取，GltfDecoration
// CPU 展开——EQUIVALENCE: 参考走 GPU 实例化（InstancedGraphicParams），结果
// 等价（同 N 实例同世界位置），机制差异已注释）。
TEST(GltfReaderTest, SimpleInstancingExtracts125Instances)
{
    auto scene = GltfReader::LoadFromFile(
        "D:/Github/glTF-Sample-Assets/Models/SimpleInstancing/glTF-Binary/SimpleInstancing.glb");
    ASSERT_NE(scene, nullptr) << "Error: " << GltfReader::getLastError();
    size_t instCount = 0;
    for (auto const& m : scene->meshes)
        instCount += m.instances.size();
    printf("[INST] meshes=%zu instances=%zu\n", scene->meshes.size(), instCount);
    EXPECT_EQ(instCount, 125u) << "EXT_mesh_gpu_instancing instances not extracted";
}


// Authored: KHR_node_visibility 子树跳过验证（2026-09-16，Khronos spec；
// 无参考实现——itwinjs 与上游 cgltf 未支持）。NodeVisibilityTest 的
// visible=false 节点（node1/6/8/9/10/13/14 子树）应被跳过。
TEST(GltfReaderTest, NodeVisibilitySkipsInvisibleSubtrees)
{
    auto scene = GltfReader::LoadFromFile(
        "D:/Github/glTF-Sample-Assets/Models/NodeVisibilityTest/glTF-Binary/NodeVisibilityTest.glb");
    ASSERT_NE(scene, nullptr) << "Error: " << GltfReader::getLastError();
    // 8 mesh 节点，visible=false 的 node1(mesh1)+node6(mesh4)+node13(mesh6)+
    // node14(mesh7) 及其子树应被跳过；node8/9/10(mesh5) 在 node8(visible=false)
    // 子树下也跳过。剩余可见 mesh = 8 - 4(显式false) - 1(node5 在 false 子树) = 3。
    printf("[VIS] meshes loaded=%zu\n", scene->meshes.size());
    EXPECT_LT(scene->meshes.size(), 8u) << "KHR_node_visibility invisible subtrees not skipped";
}


// Authored: Unicode 文件名加载验证（2026-09-16）——Unicode❤♻Test.glb。
TEST(GltfReaderTest, UnicodeFilenameLoads)
{
    auto scene = GltfReader::LoadFromFile(
        "D:/Github/glTF-Sample-Assets/Models/Unicode❤♻Test/glTF-Binary/Unicode❤♻Test.glb");
    printf("[UNI] scene=%s err='%s'\n", scene ? "ok" : "null", GltfReader::getLastError().c_str());
    EXPECT_NE(scene, nullptr) << "Unicode filename failed: " << GltfReader::getLastError();
}

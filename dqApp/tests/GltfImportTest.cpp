// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts:149-221
//              (GltfDecorationTool.run — loadGltf → addDecorator → lookAtVolume)
#include <gtest/gtest.h>
#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/GltfDecoration.h>
#include <dqApp/GltfImport.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqRender/GltfReader.h>
#include <QString>
#include <QWidget>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "QtAppFixture.h"  // the shared QApplication fixture header in dqApp/tests

namespace {

// Path of the minimal GLB test asset shared with dqRender/tests/GltfReaderTest.cpp.
// That test lives in the dqRenderTest binary, so this TU regenerates the identical
// bytes at the identical path before loading (WriteTestGlb below).
char const* const GLB_TEST_ASSET_PATH = "/tmp/danqing_gltf_test.glb";

// Copied verbatim from dqRender/tests/GltfReaderTest.cpp (WriteTestGlb — itself
// Authored: no reference test exists in itwinjs-core or imodel-native for cgltf
// reader integration). One-line adaptation: the hard-coded path literal is
// replaced by the shared GLB_TEST_ASSET_PATH constant above.
// Single-triangle mesh with positions and indices.
std::string WriteTestGlb()
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
    std::string path = GLB_TEST_ASSET_PATH;
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<char const*>(glb.data()), static_cast<std::streamsize>(glb.size()));
    return path;
}

// Copied verbatim from dqRender/tests/GltfReaderTest.cpp (ReadPngAsset — Task 2
// DecodeImage fixture / Task 6 texture helper). The PNG asset path is injected
// at compile time (DANQING_TEST_ASSET_ROOT, defined for dqAppTest in dqApp/
// CMakeLists.txt), no CWD dependency.
// Authored: no reference test exists in itwinjs-core or imodel-native for
// cgltf reader texture integration assets.
std::vector<uint8_t> ReadPngAsset()
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

// Copied verbatim from dqRender/tests/GltfReaderTest.cpp (WriteTexturedGlb,
// Task 6 final version with the imagesJson parameterization). Adapted from
// GltfReader.test.ts embedded-image scenarios — builds a GLB whose material
// texture is either an in-BIN-chunk PNG (imagesJson empty →
// images:[{"bufferView":2}], PNG bytes land in the BIN chunk) or a data URI
// (imagesJson = the images array CONTENTS, e.g. {"uri":"data:..."}; the PNG
// bytes then stay OUT of the BIN chunk and bufferViews carries only pos/uv/
// idx). A TEXCOORD_0 accessor rides along. emissiveInstead=true swaps the
// material to emissiveTexture (extractTextureId fallback path).
// Note: the dqApp side only exercises the embedded-image branch (imagesJson
// empty); the parameterization is kept 1:1 with the dqRender helper.
std::string WriteTexturedGlb(bool emissiveInstead, std::string const& imagesJson = "")
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
    char material[128];
    if (emissiveInstead) {
        snprintf(material, sizeof(material), "\"emissiveTexture\":{\"index\":0}");
    } else {
        snprintf(material, sizeof(material),
                 "\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}");
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

// Authored: test-only fixture mirroring EventDispatchTest's Fixture
//           (BlankConnection + SpatialViewState + Viewport).
struct GltfImportFixture {
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    std::unique_ptr<dqApp::Viewport> viewport;
    explicit GltfImportFixture(QWidget* parent) {
        dqApp::BlankConnectionProps props;
        props.name = "gltf import test";
        props.extents = dqGeom::Range3d(-10, -10, -10, 10, 10, 10);
        imodel = dqApp::BlankConnection::create(props);
        view = dqApp::SpatialViewState::CreateBlank(
            imodel.Get(), dqGeom::Point3d::From(0, 0, 0),
            dqGeom::Vector3d::From(10, 10, 10));
        viewport.reset(dqApp::Viewport::Create(parent, view));
    }
};
}  // namespace

// Ported from: itwinjs-core GltfDecoration.ts:165-208 (readGltfTemplate + addDecorator)
TEST(GltfImportTest, InstallRegistersDecorationAndRetainsScene) {
    QtApp& app = qtApp();  // shared QApplication fixture
    (void)app;
    GltfImportFixture f(nullptr);

    // Regenerate the shared minimal GLB asset (same bytes/path as
    // dqRender/tests/GltfReaderTest.cpp's WriteTestGlb), then load it.
    std::string const path = WriteTestGlb();
    EXPECT_EQ(path, GLB_TEST_ASSET_PATH);
    auto scene = dqRender::GltfReader::LoadFromFile(GLB_TEST_ASSET_PATH);
    ASSERT_NE(scene, nullptr);
    ASSERT_FALSE(scene->meshes.empty());

    auto const decoratorsBefore = dqApp::Application::Get().GetViewManager().GetDecorators().size();
    auto decoration = dqApp::InstallGltfDecoration(*f.viewport, std::move(scene), "cube.glb");

    ASSERT_NE(decoration, nullptr);
    EXPECT_NE(decoration->GetScene(), nullptr);              // scene retained by decoration
    EXPECT_EQ(dqApp::Application::Get().GetViewManager().GetDecorators().size(),
              decoratorsBefore + 1);                          // registered with ViewManager

    // ViewManager::AddDecorator is non-owning — the caller (owner of the returned
    // unique_ptr) must DropDecorator before the decoration is destroyed, or the
    // singleton ViewManager would hold a dangling pointer into the next test.
    dqApp::Application::Get().GetViewManager().DropDecorator(decoration.get());
}

// Ported from: itwinjs-core GltfDecoration.ts:162-164 (transientIds.getNext — pickableOptions)
TEST(GltfImportTest, PickableIdIsInReservedRangeAndUnique) {
    uint32_t a = dqApp::NextGltfPickableId();
    uint32_t b = dqApp::NextGltfPickableId();
    EXPECT_GE(a, 0x80000000u);   // reserved high range — no collision with element feature ids
    EXPECT_GE(b, 0x80000000u);
    EXPECT_NE(a, b);
}

// Authored: null/empty scene is a graceful no-op (returns nullptr, nothing registered).
TEST(GltfImportTest, NullSceneIsNoOp) {
    QtApp& app = qtApp();
    (void)app;
    GltfImportFixture f(nullptr);
    auto const before = dqApp::Application::Get().GetViewManager().GetDecorators().size();
    auto decoration = dqApp::InstallGltfDecoration(*f.viewport, nullptr, "empty");
    EXPECT_EQ(decoration, nullptr);
    EXPECT_EQ(dqApp::Application::Get().GetViewManager().GetDecorators().size(), before);
}

// Authored: no reference test exists in itwinjs-core/imodel-native for
// ViewState3d::LookAtVolume framing math in isolation. This validates the
// mechanism loadGltf relies on to make a unit-scale glTF visible: fitting the
// view to the model's range.
//
// The DisplayTestApp's blank-connection view is framed on a 1000-unit volume
// (View3DInventor ctor: SpatialViewState::CreateBlank(..., Vector3d(1000,1000,1000))).
// The test cube (cube.glb) is 2 units across. At a 1000-unit view it occupies
// ~0.2% of the viewport — a sub-pixel speck, "invisible" despite rendering
// correctly into the render-target FBO. LookAtVolume(cubeRange) shrinks the
// view extents to the cube diagonal so it fills the viewport.
TEST(GltfImportTest, LookAtVolumeFramesViewOnCubeRange) {
    QtApp& app = qtApp();
    (void)app;

    // Mirror the DisplayTestApp blank-connection view (View3DInventor ctor).
    dqApp::BlankConnectionProps props;
    props.name = "framing test";
    props.extents = dqGeom::Range3d(-1000, -1000, -100, 1000, 1000, 100);
    auto imodel = dqApp::BlankConnection::create(props);
    auto view = dqApp::SpatialViewState::CreateBlank(
        imodel.Get(), dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(1000, 1000, 1000));
    auto* view3d = view->AsViewState3d();
    ASSERT_NE(view3d, nullptr);

    // Before fit: view framed on the 1000-unit blank volume. A 2-unit cube here
    // would be sub-pixel -> invisible. This is the pre-fix state.
    EXPECT_NEAR(view3d->GetExtents().x, 1000.0, 1.0);

    // Fit to the test cube's range (cube.glb verts at +-1 -> [-1,-1,-1, 1,1,1]).
    // Ported math: itwinjs-core ViewState.lookAtViewAlignedVolume (ViewState.ts:1041-1122)
    // — origin = volume.low, extents = diagonal x 1.04 default dilation (else branch,
    // camera off). Cube [-1,1]^3 under identity (Top) rotation -> view-aligned diagonal
    // (2,2,2); x1.04 -> 2.08 per axis.
    view3d->LookAtVolume(dqGeom::Range3d(-1, -1, -1, 1, 1, 1));

    // Faithful x1.04 dilation of the (2,2,2) view-aligned diagonal.
    EXPECT_NEAR(view3d->GetExtents().x, 2.0 * 1.04, 1e-6);   // 2.08
    EXPECT_NEAR(view3d->GetExtents().y, 2.0 * 1.04, 1e-6);
    EXPECT_NEAR(view3d->GetExtents().z, 2.0 * 1.04, 1e-6);
    // Origin = volume.low (-1) re-centered by the dilation half-offset:
    //   low + (origDelta - newDelta)*0.5 = -1 + (2 - 2.08)*0.5 = -1.04 (identity rot).
    EXPECT_NEAR(view3d->GetOrigin().x, -1.04, 1e-6);
    EXPECT_NEAR(view3d->GetOrigin().y, -1.04, 1e-6);
}

// Authored: no reference test exists in itwinjs-core for textured glTF scene
//              installing a decoration; 行为锚定实现 GltfReader.ts
//              createDisplayParams(:1356) textured path + GltfDecoration.ts:165-208
//              (readGltf -> addDecorator).
TEST(GltfImportTest, TexturedSceneInstallsDecoration)
{
    // 复用 dqRender/tests/GltfReaderTest.cpp 的 WriteTexturedGlb 资产构造：
    // 带 baseColorTexture + TEXCOORD_0 的 GLB（图片字节 = red1x1.png，嵌入 BIN
    // chunk）。写入临时路径（C:/Windows/Temp/danqing_gltf_textured.glb，与
    // dqRenderTest 共享同一路径，本 TU 先重新生成同字节再加载——同 WriteTestGlb 模式）。
    std::string const texturedGlbPath = WriteTexturedGlb(false);
    QtApp& app = qtApp();  // shared QApplication fixture
    (void)app;
    GltfImportFixture f(nullptr);
    auto scene = dqRender::GltfReader::LoadFromFile(texturedGlbPath);
    ASSERT_NE(scene, nullptr);
    ASSERT_TRUE(scene->meshes.front().baseColorTexture.has_value());

    auto decoration = dqApp::InstallGltfDecoration(*f.viewport, std::move(scene), "textured.glb");
    ASSERT_NE(decoration, nullptr);
    // BuildGraphic 走了 createTexture + textured 工厂路径且不崩（像素级验证在
    // DisplayTestAppDtaTest.GltfTexturePixelTest，本用例锁安装链）。
    EXPECT_NE(decoration->GetScene(), nullptr);

    // ViewManager::AddDecorator is non-owning — DropDecorator before the
    // decoration is destroyed (same hygiene as the install test above).
    dqApp::Application::Get().GetViewManager().DropDecorator(decoration.get());
}


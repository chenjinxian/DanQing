// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
// DanQing dqRender — P2/P3 feature tests
//
// Tests for PlanarClassifier, ThematicDisplay, ContourLines,
// MonochromeMode, RealityMeshGeometry, Layer, LineCode, Sync,
// Decorations, PerformanceMetrics, RenderMemory, AttributeMap, MockRender,
// and remaining GLSL modules.
// （SolarShadowMap/GraphicTemplate 的 src/render 自创平行实现已删——前者与
// ShadowUniforms.h 的同名 struct、后者与公开 API 抽象接口构成 ODR 双定义。）

#include "render/PlanarClassifier.h"
#include "render/ThematicDisplay.h"
#include "render/ContourLines.h"
#include "render/MonochromeMode.h"
#include "render/RealityMeshGeometry.h"
#include "render/Layer.h"
#include "render/LineCode.h"
#include "render/Sync.h"
#include "render/Decorations.h"
#include "render/PerformanceMetrics.h"
#include "dqRender/RenderMemory.h"
#include "render/AttributeMap.h"
#include "render/MockRender.h"
#include "render/RemainingShaderModules.h"

#include <gtest/gtest.h>
#include <memory>

using namespace dqRender;

// ============================================================================
// PlanarClassifier tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(PlanarClassifierTest, DefaultState)
{
    PlanarClassifier classifier;
    EXPECT_EQ(classifier.getContent(), PlanarClassifierContent::None);
    EXPECT_FALSE(classifier.isActive());
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(PlanarClassifierTest, SetContent)
{
    PlanarClassifier classifier;
    classifier.setContent(PlanarClassifierContent::MaskOnly);
    EXPECT_TRUE(classifier.isActive());
    EXPECT_EQ(classifier.getContent(), PlanarClassifierContent::MaskOnly);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(PlanarClassifierTest, SetColor)
{
    PlanarClassifier classifier;
    classifier.setColor(1.0f, 0.0f, 0.0f, 0.5f);
    float const* color = classifier.getColor();
    EXPECT_FLOAT_EQ(color[0], 1.0f);
    EXPECT_FLOAT_EQ(color[1], 0.0f);
    EXPECT_FLOAT_EQ(color[3], 0.5f);
}

// ============================================================================
// ThematicDisplay tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(ThematicDisplayTest, DefaultState)
{
    ThematicDisplay thematic;
    EXPECT_FALSE(thematic.isEnabled());
    EXPECT_EQ(thematic.getMode(), ThematicDisplayMode::Height);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(ThematicDisplayTest, Configure)
{
    ThematicDisplay thematic;
    thematic.setEnabled(true);
    thematic.setMode(ThematicDisplayMode::Slope);
    thematic.setRange(0.0f, 90.0f);

    EXPECT_TRUE(thematic.isEnabled());
    EXPECT_EQ(thematic.getMode(), ThematicDisplayMode::Slope);
    EXPECT_FLOAT_EQ(thematic.getRangeMin(), 0.0f);
    EXPECT_FLOAT_EQ(thematic.getRangeMax(), 90.0f);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(ThematicDisplayTest, BuildGradientLut)
{
    ThematicDisplay thematic;
    std::vector<ThematicGradientColor> colors = {
        {0.0f, 0x0000FF},  // blue at 0
        {0.5f, 0x00FF00},  // green at 0.5
        {1.0f, 0xFF0000},  // red at 1
    };
    thematic.setGradientColors(colors);

    auto lut = thematic.buildGradientLutData();
    EXPECT_EQ(lut.size(), 256u * 4);

    // Check first pixel (blue)
    EXPECT_EQ(lut[2], 255);  // B
    EXPECT_EQ(lut[0], 0);    // R

    // Check last pixel (red)
    EXPECT_EQ(lut[255 * 4 + 0], 255);  // R
    EXPECT_EQ(lut[255 * 4 + 2], 0);    // B
}

// ============================================================================
// ContourLines tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(ContourLinesTest, DefaultState)
{
    ContourLines contours;
    EXPECT_FALSE(contours.isEnabled());
    EXPECT_FLOAT_EQ(contours.getInterval(), 1.0f);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(ContourLinesTest, Configure)
{
    ContourLines contours;
    contours.setEnabled(true);
    contours.setInterval(5.0f);
    contours.setOrigin(100.0f);
    contours.setColor(0x000000);
    contours.setWidth(2.0f);

    EXPECT_TRUE(contours.isEnabled());
    EXPECT_FLOAT_EQ(contours.getInterval(), 5.0f);
    EXPECT_FLOAT_EQ(contours.getOrigin(), 100.0f);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(ContourLinesTest, BuildContourLut)
{
    ContourLines contours;
    contours.setColor(0xFF0000);
    auto lut = contours.buildContourLutData(10);
    EXPECT_EQ(lut.size(), 40u);  // 10 levels * 4 bytes
    EXPECT_EQ(lut[0], 255);  // R
    EXPECT_EQ(lut[1], 0);    // G
    EXPECT_EQ(lut[2], 0);    // B
}

// ============================================================================
// MonochromeMode tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(MonochromeModeTest, DefaultState)
{
    MonochromeSettings mono;
    EXPECT_FALSE(mono.isEnabled());
    EXPECT_EQ(mono.getColor(), 0x808080u);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(MonochromeModeTest, Configure)
{
    MonochromeSettings mono;
    mono.setEnabled(true);
    mono.setColor(0xFF0000);
    mono.setAlpha(0.5f);

    EXPECT_TRUE(mono.isEnabled());
    EXPECT_EQ(mono.getColor(), 0xFF0000u);
    EXPECT_FLOAT_EQ(mono.getAlpha(), 0.5f);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(MonochromeModeTest, ColorFloat)
{
    MonochromeSettings mono;
    mono.setColor(0xFF8040);
    float rgb[3];
    mono.getColorFloat(rgb);
    EXPECT_FLOAT_EQ(rgb[0], 1.0f);
    EXPECT_FLOAT_EQ(rgb[1], 128.0f / 255.0f);
    EXPECT_FLOAT_EQ(rgb[2], 64.0f / 255.0f);
}

// ============================================================================
// RealityMeshGeometry tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(RealityMeshGeometryTest, Properties)
{
    RealityMeshGeometry mesh(1000, 500);
    EXPECT_EQ(mesh.getTechniqueId(), TechniqueId::RealityMesh);
    EXPECT_EQ(mesh.getPass(), Pass::Opaque);
    EXPECT_EQ(mesh.getNumIndices(), 1000u);
    EXPECT_EQ(mesh.getVertexCount(), 500u);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(RealityMeshGeometryTest, UvTransform)
{
    RealityMeshGeometry mesh(100, 50);
    mesh.setUvTransform(0.5f, 0.5f, 10.0f, 20.0f);
    float const* uv = mesh.getUvTransform();
    EXPECT_FLOAT_EQ(uv[0], 0.5f);
    EXPECT_FLOAT_EQ(uv[2], 10.0f);
}

// ============================================================================
// Layer tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(LayerTest, Create)
{
    auto graphic = std::make_unique<GraphicsArray>();
    Layer layer(LayerType::Overlay, std::move(graphic));
    EXPECT_EQ(layer.getType(), LayerType::Overlay);
    EXPECT_NE(layer.getGraphic(), nullptr);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(LayerContainerTest, AddLayers)
{
    LayerContainer container;
    container.addLayer(std::make_unique<Layer>(LayerType::Background, std::make_unique<GraphicsArray>()));
    container.addLayer(std::make_unique<Layer>(LayerType::Overlay, std::make_unique<GraphicsArray>()));
    EXPECT_EQ(container.getLayerCount(), 2u);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(LayerContainerTest, Clear)
{
    LayerContainer container;
    container.addLayer(std::make_unique<Layer>(LayerType::Background, std::make_unique<GraphicsArray>()));
    container.clear();
    EXPECT_EQ(container.getLayerCount(), 0u);
}

// ============================================================================
// Sync tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(SyncTest, TargetDesync)
{
    SyncTarget target;
    EXPECT_EQ(target.getSyncKey(), 1u);  // Starts at 1

    target.desync();
    EXPECT_EQ(target.getSyncKey(), 2u);

    target.desync();
    EXPECT_EQ(target.getSyncKey(), 3u);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(SyncTest, ObserverSync)
{
    SyncTarget target;
    SyncObserver observer;

    EXPECT_FALSE(observer.isSynchronized(target));

    observer.sync(target);
    EXPECT_TRUE(observer.isSynchronized(target));

    target.desync();
    EXPECT_FALSE(observer.isSynchronized(target));

    observer.sync(target);
    EXPECT_TRUE(observer.isSynchronized(target));
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(SyncTest, ObserverReset)
{
    SyncTarget target;
    SyncObserver observer;
    observer.sync(target);
    EXPECT_TRUE(observer.isSynchronized(target));

    observer.reset();
    EXPECT_FALSE(observer.isSynchronized(target));
}

// ============================================================================
// InternalDecorations tests (renamed from Decorations to avoid conflict with public class)
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(InternalDecorationsTest, Empty)
{
    InternalDecorations deco;
    EXPECT_TRUE(deco.isEmpty());
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(InternalDecorationsTest, AddDecorations)
{
    InternalDecorations deco;
    deco.addWorldDecoration(std::make_unique<GraphicsArray>());
    deco.addViewDecoration(std::make_unique<GraphicsArray>());
    deco.addOverlayDecoration(std::make_unique<GraphicsArray>());
    EXPECT_FALSE(deco.isEmpty());
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(InternalDecorationsTest, Clear)
{
    InternalDecorations deco;
    deco.addWorldDecoration(std::make_unique<GraphicsArray>());
    deco.clear();
    EXPECT_TRUE(deco.isEmpty());
}

// ============================================================================
// PerformanceMetrics tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(PerformanceMetricsTest, DefaultState)
{
    PerformanceMetrics metrics;
    EXPECT_EQ(metrics.getDrawCalls(), 0u);
    EXPECT_EQ(metrics.getTriangles(), 0u);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(PerformanceMetricsTest, RecordDraws)
{
    PerformanceMetrics metrics;
    metrics.recordDrawCall(100);
    metrics.recordDrawCall(200);
    metrics.recordLineDraw(50);
    metrics.recordPointDraw(30);

    EXPECT_EQ(metrics.getDrawCalls(), 4u);
    EXPECT_EQ(metrics.getTriangles(), 300u);
    EXPECT_EQ(metrics.getLines(), 50u);
    EXPECT_EQ(metrics.getPoints(), 30u);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(PerformanceMetricsTest, Reset)
{
    PerformanceMetrics metrics;
    metrics.recordDrawCall(100);
    metrics.reset();
    EXPECT_EQ(metrics.getDrawCalls(), 0u);
}

// ============================================================================
// RenderMemory tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for RenderMemory alone
// (FeatureSymbology.test.ts has none — 原标注有误；BatchVTest 同款标注)
TEST(RenderMemoryStatisticsTest, DefaultState)
{
    RenderMemory::Statistics stats;
    EXPECT_EQ(stats.totalBytes, 0u);
}

// Authored: no reference test exists in itwinjs-core for RenderMemory alone
TEST(RenderMemoryStatisticsTest, addBuffer)
{
    RenderMemory::Statistics stats;
    stats.addSurface(1024);
    stats.addIndexedEdges(512);
    stats.addTexture(2048);
    EXPECT_EQ(stats.totalBytes, 1024u + 512u + 2048u);
}

// Authored: no reference test exists in itwinjs-core for RenderMemory alone
TEST(RenderMemoryStatisticsTest, Reset)
{
    RenderMemory::Statistics stats;
    stats.addSurface(1024);
    stats.clear();
    EXPECT_EQ(stats.totalBytes, 0u);
}

// ============================================================================
// AttributeMap tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(AttributeMapTest, FindAttribute)
{
    // The AttributeMap is a singleton with pre-registered technique maps.
    // Verify that findAttribute returns the correct details for known techniques.
    auto const* pos = AttributeMap::findAttribute("a_pos", TechniqueId::Surface, false);
    EXPECT_NE(pos, nullptr);
    EXPECT_EQ(pos->location, 0u);
    EXPECT_EQ(pos->type, VariableType::Vec3);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(AttributeMapTest, FindInstancedAttribute)
{
    // Instance attributes should be available when instanced=true
    auto const* row0 = AttributeMap::findAttribute("a_instanceMatrixRow0", TechniqueId::Surface, true);
    EXPECT_NE(row0, nullptr);
    EXPECT_EQ(row0->type, VariableType::Vec4);

    // Instance attributes should NOT be available when instanced=false
    auto const* row0u = AttributeMap::findAttribute("a_instanceMatrixRow0", TechniqueId::Surface, false);
    EXPECT_EQ(row0u, nullptr);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(AttributeMapTest, TechniqueSpecificAttributes)
{
    // Polyline technique has a_prevIndex, a_nextIndex, a_param
    auto const* prev = AttributeMap::findAttribute("a_prevIndex", TechniqueId::Polyline, false);
    EXPECT_NE(prev, nullptr);

    auto const* next = AttributeMap::findAttribute("a_nextIndex", TechniqueId::Polyline, false);
    EXPECT_NE(next, nullptr);

    auto const* param = AttributeMap::findAttribute("a_param", TechniqueId::Polyline, false);
    EXPECT_NE(param, nullptr);
    EXPECT_EQ(param->type, VariableType::Float);
}

// ============================================================================
// MockRenderSystem tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(MockRenderSystemTest, isValid)
{
    MockRenderSystem mock;
    EXPECT_TRUE(mock.isValid());
    EXPECT_EQ(mock.createTarget(nullptr, 0, 0), nullptr);
}

// ============================================================================
// Remaining GLSL module tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(RemainingShaderModulesTest, Lighting)
{
    char const* code = getLightingFunctions();
    std::string src(code);
    EXPECT_NE(src.find("computeLighting"), std::string::npos);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(RemainingShaderModulesTest, Clipping)
{
    char const* code = getClippingFunctions();
    std::string src(code);
    EXPECT_NE(src.find("isClipped"), std::string::npos);
    EXPECT_NE(src.find("u_clipPlanes"), std::string::npos);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(RemainingShaderModulesTest, Monochrome)
{
    char const* code = getMonochromeFunctions();
    std::string src(code);
    EXPECT_NE(src.find("applyMonochrome"), std::string::npos);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(RemainingShaderModulesTest, Translucency)
{
    char const* code = getTranslucencyFunctions();
    std::string src(code);
    EXPECT_NE(src.find("oitAccumulate"), std::string::npos);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(RemainingShaderModulesTest, LogDepth)
{
    char const* code = getLogDepthFunctions();
    std::string src(code);
    EXPECT_NE(src.find("computeLogDepth"), std::string::npos);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(RemainingShaderModulesTest, CommonUtilities)
{
    char const* code = getCommonUtilities();
    std::string src(code);
    EXPECT_NE(src.find("nthBitSet"), std::string::npos);
    EXPECT_NE(src.find("u_frustum"), std::string::npos);
}

// Ported from: itwinjs-core core/frontend/src/test/FeatureSymbology.test.ts
TEST(RemainingShaderModulesTest, Viewport)
{
    char const* code = getViewportFunctions();
    std::string src(code);
    EXPECT_NE(src.find("modelToWindowCoordinates"), std::string::npos);
}

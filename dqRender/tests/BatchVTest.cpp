// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core for these type-only ports
// (itwinjs tests them indirectly via RenderSystem.create* round-trips; DanQing
// ports only the type surface here, so we test construct + field round-trip).
// DanQing dqRender — Batch V: 9 missing public render headers (type-only).
//
// Covers: RenderMemory, CreateTextureArgs, PolylineArgs, RenderClipVolume,
// FrameStats, GraphicTemplate, VisibleFeature, RealityMeshParams,
// CreateRenderMaterialArgs.

#include "dqRender/CreateRenderMaterialArgs.h"
#include "dqRender/CreateTextureArgs.h"
#include "dqRender/FrameStats.h"
#include "dqRender/GraphicTemplate.h"
#include "dqRender/PolylineArgs.h"
#include "dqRender/RealityMeshParams.h"
#include "dqRender/RenderClipVolume.h"
#include "dqRender/RenderMaterial.h"
#include "dqRender/RenderMemory.h"
#include "dqRender/VisibleFeature.h"

#include <gtest/gtest.h>

using namespace dqRender;

// ============================================================================
// RenderMemory — BufferType / ConsumerType enum values + Statistics round-trip.
// Authored: no reference test exists in itwinjs-core for RenderMemory alone.
TEST(RenderMemoryTest, EnumValuesMatchItwinjs)
{
    // Ported from: itwinjs-core RenderMemory.BufferType
    EXPECT_EQ(static_cast<int>(RenderMemory::BufferType::Surfaces), 0);
    EXPECT_EQ(static_cast<int>(RenderMemory::BufferType::VisibleEdges), 1);
    EXPECT_EQ(static_cast<int>(RenderMemory::BufferType::SilhouetteEdges), 2);
    EXPECT_EQ(static_cast<int>(RenderMemory::BufferType::PolylineEdges), 3);
    EXPECT_EQ(static_cast<int>(RenderMemory::BufferType::IndexedEdges), 4);
    EXPECT_EQ(static_cast<int>(RenderMemory::BufferType::Polylines), 5);
    EXPECT_EQ(static_cast<int>(RenderMemory::BufferType::PointStrings), 6);
    EXPECT_EQ(static_cast<int>(RenderMemory::BufferType::PointClouds), 7);
    EXPECT_EQ(static_cast<int>(RenderMemory::BufferType::Instances), 8);
    EXPECT_EQ(static_cast<int>(RenderMemory::BufferType::Terrain), 9);
    EXPECT_EQ(static_cast<int>(RenderMemory::BufferType::RealityMesh), 10);
    EXPECT_EQ(static_cast<int>(RenderMemory::BufferType::COUNT), 11);

    // Ported from: itwinjs-core RenderMemory.ConsumerType
    EXPECT_EQ(static_cast<int>(RenderMemory::ConsumerType::Textures), 0);
    EXPECT_EQ(static_cast<int>(RenderMemory::ConsumerType::VertexTables), 1);
    EXPECT_EQ(static_cast<int>(RenderMemory::ConsumerType::EdgeTables), 2);
    EXPECT_EQ(static_cast<int>(RenderMemory::ConsumerType::FeatureTables), 3);
    EXPECT_EQ(static_cast<int>(RenderMemory::ConsumerType::FeatureOverrides), 4);
    EXPECT_EQ(static_cast<int>(RenderMemory::ConsumerType::ClipVolumes), 5);
    EXPECT_EQ(static_cast<int>(RenderMemory::ConsumerType::PlanarClassifiers), 6);
    EXPECT_EQ(static_cast<int>(RenderMemory::ConsumerType::ShadowMaps), 7);
    EXPECT_EQ(static_cast<int>(RenderMemory::ConsumerType::TextureAttachments), 8);
    EXPECT_EQ(static_cast<int>(RenderMemory::ConsumerType::ThematicTextures), 9);
    EXPECT_EQ(static_cast<int>(RenderMemory::ConsumerType::COUNT), 10);
}

// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(RenderMemoryTest, ConsumersAddAndClear)
{
    RenderMemory::Consumers c;
    c.addConsumer(100);
    c.addConsumer(50);
    EXPECT_EQ(c.totalBytes, 150u);
    EXPECT_EQ(c.maxBytes, 100u);
    EXPECT_EQ(c.count, 2u);

    c.clear();
    EXPECT_EQ(c.totalBytes, 0u);
    EXPECT_EQ(c.maxBytes, 0u);
    EXPECT_EQ(c.count, 0u);
}

// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(RenderMemoryTest, StatisticsAddTextureAndSurface)
{
    RenderMemory::Statistics s = RenderMemory::Statistics::create();
    EXPECT_EQ(s.totalBytes, 0u);

    s.addTexture(2048);
    s.addSurface(4096);
    EXPECT_EQ(s.totalBytes, 2048u + 4096u);

    // textures consumer received the texture bytes.
    EXPECT_EQ(s.consumers[static_cast<int>(RenderMemory::ConsumerType::Textures)].totalBytes, 2048u);
    // surfaces buffer received the surface bytes.
    EXPECT_EQ(
        s.buffers.consumers[static_cast<int>(RenderMemory::BufferType::Surfaces)].totalBytes,
        4096u);

    s.clear();
    EXPECT_EQ(s.totalBytes, 0u);
}

// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(RenderMemoryTest, AddContoursClassifiedAsFeatureOverrides)
{
    RenderMemory::Statistics s;
    s.addContours(512);
    EXPECT_EQ(
        s.consumers[static_cast<int>(RenderMemory::ConsumerType::FeatureOverrides)].totalBytes,
        512u);
}

// ============================================================================
// CreateTextureArgs — default + round-trip + ownership tagging.
// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(CreateTextureArgsTest, DefaultConstructionMatchesItwinjs)
{
    const CreateTextureArgs args;
    EXPECT_EQ(args.type, RenderTexture::Type::Normal);
    EXPECT_TRUE(args.imageData.empty());
    EXPECT_FALSE(args.ownership.has_value());
}

// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(CreateTextureArgsTest, OwnershipExternalRoundTrip)
{
    CreateTextureArgs args;
    args.type = RenderTexture::Type::TileSection;
    args.ownership = TextureOwnership{};
    args.ownership->external = true;
    ASSERT_TRUE(args.ownership.has_value());
    EXPECT_TRUE(args.ownership->external);

    CreateTextureFromSourceArgs src;
    src.transparency = TextureTransparency::Opaque;
    src.ownership = TextureOwnership{};
    src.ownership->cache.key = "k1";
    EXPECT_EQ(*src.transparency, TextureTransparency::Opaque);
    EXPECT_EQ(src.ownership->cache.key, "k1");
}

// ============================================================================
// PolylineArgs — defaults + addPolyline round-trip (re-exported via MeshArgs.h).
// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(PolylineArgsBatchVTest, DefaultConstructionMatchesItwinjs)
{
    PolylineArgs args;
    EXPECT_EQ(args.width, 1);
    EXPECT_EQ(args.flags, PolylineFlags::None);
    EXPECT_TRUE(args.polylines.empty());

    PolylineIndices idx = {0, 1, 2};
    args.polylines.push_back(idx);
    ASSERT_EQ(args.polylines.size(), 1u);
    EXPECT_EQ(args.polylines[0].size(), 3u);
}

// ============================================================================
// RenderClipVolume — abstract base, forward-declares ClipVector.
// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(RenderClipVolumeTest, IsAbstract)
{
    // compile-only: confirm RenderClipVolume is abstract. The forward-declared
    // ClipVector placeholder is sufficient — no concrete impl is required to
    // exercise the type surface.
    static_assert(
        std::is_abstract_v<RenderClipVolume>,
        "RenderClipVolume must be abstract (matches itwinjs abstract class)");
}

// ============================================================================
// FrameStats — 17-field aggregate, all default zero.
// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(FrameStatsTest, DefaultConstructionAllZero)
{
    const FrameStats f;
    EXPECT_EQ(f.frameId, 0u);
    EXPECT_DOUBLE_EQ(f.totalSceneTime, 0.0);
    EXPECT_DOUBLE_EQ(f.animationTime, 0.0);
    EXPECT_DOUBLE_EQ(f.setupViewTime, 0.0);
    EXPECT_DOUBLE_EQ(f.createChangeSceneTime, 0.0);
    EXPECT_DOUBLE_EQ(f.validateRenderPlanTime, 0.0);
    EXPECT_DOUBLE_EQ(f.decorationsTime, 0.0);
    EXPECT_DOUBLE_EQ(f.onBeforeRenderTime, 0.0);
    EXPECT_DOUBLE_EQ(f.totalFrameTime, 0.0);
    EXPECT_DOUBLE_EQ(f.opaqueTime, 0.0);
    EXPECT_DOUBLE_EQ(f.onRenderOpaqueTime, 0.0);
    EXPECT_DOUBLE_EQ(f.translucentTime, 0.0);
    EXPECT_DOUBLE_EQ(f.overlaysTime, 0.0);
    EXPECT_DOUBLE_EQ(f.shadowsTime, 0.0);
    EXPECT_DOUBLE_EQ(f.classifiersTime, 0.0);
    EXPECT_DOUBLE_EQ(f.screenspaceEffectsTime, 0.0);
    EXPECT_DOUBLE_EQ(f.backgroundTime, 0.0);
}

// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(FrameStatsTest, FieldRoundTrip)
{
    FrameStats f;
    f.frameId = 42;
    f.totalSceneTime = 1.5;
    f.totalFrameTime = 16.7;
    f.opaqueTime = 8.0;
    EXPECT_EQ(f.frameId, 42u);
    EXPECT_DOUBLE_EQ(f.totalSceneTime, 1.5);
    EXPECT_DOUBLE_EQ(f.totalFrameTime, 16.7);
    EXPECT_DOUBLE_EQ(f.opaqueTime, 8.0);
}

// ============================================================================
// GraphicTemplate — abstract base; isInstanceable is the single public method.
// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(GraphicTemplateTest, IsAbstract)
{
    static_assert(
        std::is_abstract_v<GraphicTemplate>,
        "GraphicTemplate must be abstract (matches itwinjs interface + impl)");
}

// ============================================================================
// VisibleFeature — struct defaults + query-option source discriminators.
// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(VisibleFeatureTest, DefaultConstruction)
{
    const VisibleFeature f;
    EXPECT_FALSE(f.elementId.isValid());
    EXPECT_FALSE(f.subCategoryId.isValid());
    EXPECT_FALSE(f.modelId.isValid());
    EXPECT_EQ(f.geometryClass, dqCommon::GeometryClass::Primary);
    EXPECT_EQ(f.iModel, nullptr);
}

// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(VisibleFeatureTest, FieldRoundTrip)
{
    VisibleFeature f;
    f.elementId = dqBase::DqId{0x123ull};
    f.subCategoryId = dqBase::DqId{0x456ull};
    f.modelId = dqBase::DqId{0x789ull};
    f.geometryClass = dqCommon::GeometryClass::Construction;
    EXPECT_TRUE(f.elementId.isValid());
    EXPECT_EQ(f.elementId.GetValue(), 0x123ull);
    EXPECT_EQ(f.geometryClass, dqCommon::GeometryClass::Construction);
}

// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(QueryVisibleFeaturesOptionsTest, SourceDiscriminators)
{
    QueryScreenFeaturesOptions screen;
    EXPECT_EQ(screen.source, VisibleFeatureSource::Screen);

    QueryTileFeaturesOptions tiles;
    EXPECT_EQ(tiles.source, VisibleFeatureSource::Tiles);

    // enum values are stable.
    EXPECT_EQ(static_cast<int>(VisibleFeatureSource::Screen), 0);
    EXPECT_EQ(static_cast<int>(VisibleFeatureSource::Tiles), 1);
}

// ============================================================================
// RealityMeshParams — struct defaults + indices round-trip.
// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(RealityMeshParamsTest, DefaultConstruction)
{
    const RealityMeshParams p;
    EXPECT_FALSE(p.normals.has_value());
    EXPECT_TRUE(p.indices.empty());
    EXPECT_EQ(p.featureID, 0u);
    EXPECT_EQ(p.texture, nullptr);
}

// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(RealityMeshParamsTest, IndicesRoundTrip)
{
    RealityMeshParams p;
    p.indices = {0, 1, 2,  2, 1, 3};
    EXPECT_EQ(p.indices.size(), 6u);
    EXPECT_EQ(p.indices[3], 2u);
}

// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(RealityMeshParamsBuilderOptionsTest, DefaultsMatchItwinjs)
{
    const RealityMeshParamsBuilderOptions o;
    EXPECT_FALSE(o.wantNormals);
    EXPECT_FALSE(o.initialVertexCapacity.has_value());
    EXPECT_FALSE(o.initialIndexCapacity.has_value());
    // Default UV range [0,1] — matches itwinjs default Range2d(0,0,1,1).
    EXPECT_DOUBLE_EQ(o.uvRangeMinX, 0.0);
    EXPECT_DOUBLE_EQ(o.uvRangeMinY, 0.0);
    EXPECT_DOUBLE_EQ(o.uvRangeMaxX, 1.0);
    EXPECT_DOUBLE_EQ(o.uvRangeMaxY, 1.0);
}

// ============================================================================
// CreateRenderMaterialArgs — defaults + MaterialParams round-trip + source.
// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(CreateRenderMaterialArgsBatchVTest, DefaultConstruction)
{
    const CreateRenderMaterialArgs args;
    EXPECT_FALSE(args.diffuseColor.has_value());
    EXPECT_FALSE(args.specularColor.has_value());
    EXPECT_FALSE(args.finish.has_value());
    EXPECT_FALSE(args.diffuse.has_value());
    EXPECT_FALSE(args.specular.has_value());
    EXPECT_FALSE(args.transmit.has_value());
    EXPECT_FALSE(args.source.has_value());
    EXPECT_TRUE(args.key.empty());
}

// Authored: no reference test exists in itwinjs-core for these type-only ports
TEST(CreateRenderMaterialArgsBatchVTest, FieldRoundTrip)
{
    CreateRenderMaterialArgs args;
    args.diffuse = 0.7;
    args.specular = 0.3;
    args.finish = 32.0;
    args.key = "0x1";
    ASSERT_TRUE(args.diffuse.has_value());
    EXPECT_DOUBLE_EQ(*args.diffuse, 0.7);
    EXPECT_DOUBLE_EQ(*args.finish, 32.0);
    EXPECT_EQ(args.key, "0x1");

    args.source = RenderMaterialSource{};
    args.source->id = dqBase::DqId{0xabcull};
    ASSERT_TRUE(args.source.has_value());
    EXPECT_EQ(args.source->id.GetValue(), 0xabcull);
}

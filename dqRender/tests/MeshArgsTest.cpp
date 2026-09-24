// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/render/MeshArgs.ts
// DanQing dqRender — MeshArgs, PolylineArgs, RenderMaterial tests
#include "dqRender/MeshArgs.h"
#include "dqRender/RenderMaterial.h"

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqCommon;
using namespace dqGeom;

// Fix unqualified names
using dqCommon::ImageSourceFormat;
using dqCommon::FillFlags;
using dqCommon::LinePixels;
using dqCommon::FeatureIndexType;
using dqCommon::ColorDef;
using dqCommon::QPoint3dList;
using dqCommon::QParams3d;
using dqCommon::ColorIndex;
using dqCommon::FeatureIndex;

// Authored: no reference test exists in itwinjs-core for RenderTexture.Type numeric
//           values (core/frontend/src/test/MeshArgs.test.ts does not exist); enum
//           order verified 1:1 against core/common/src/RenderTexture.ts.
TEST(RenderTextureType, Values)
{
    EXPECT_EQ(static_cast<int>(dqRender::RenderTexture::Type::Normal), 0);
    EXPECT_EQ(static_cast<int>(dqRender::RenderTexture::Type::Glyph), 1);
    EXPECT_EQ(static_cast<int>(dqRender::RenderTexture::Type::TileSection), 2);
    EXPECT_EQ(static_cast<int>(dqRender::RenderTexture::Type::SkyBox), 3);
    EXPECT_EQ(static_cast<int>(dqRender::RenderTexture::Type::FilteredTileSection), 4);
    EXPECT_EQ(static_cast<int>(dqRender::RenderTexture::Type::ThematicGradient), 5);
}

// Authored: no reference test exists in itwinjs-core for TextureTransparency numeric
//           values; enum order verified 1:1 against core/common/src/TextureProps.ts.
TEST(TextureTransparency, Values)
{
    EXPECT_EQ(static_cast<int>(TextureTransparency::Opaque), 0);
    EXPECT_EQ(static_cast<int>(TextureTransparency::Translucent), 1);
    EXPECT_EQ(static_cast<int>(TextureTransparency::Mixed), 2);
}

// Authored: no reference test exists in itwinjs-core for CreateRenderMaterialArgs
//           default construction; type from core/frontend/src/render/CreateRenderMaterialArgs.ts.
TEST(CreateRenderMaterialArgs, DefaultConstruction)
{
    const CreateRenderMaterialArgs args;
    EXPECT_FALSE(args.diffuseColor.has_value());
    EXPECT_FALSE(args.specularColor.has_value());
    EXPECT_FALSE(args.finish.has_value());
    EXPECT_FALSE(args.diffuse.has_value());
    EXPECT_FALSE(args.specular.has_value());
    EXPECT_FALSE(args.transmit.has_value());
    EXPECT_TRUE(args.key.empty());
}

// Authored: no reference test exists in itwinjs-core for CreateTextureArgs default
//           construction; type from core/frontend/src/render/CreateTextureArgs.ts.
TEST(CreateTextureArgs, DefaultConstruction)
{
    const CreateTextureArgs args;
    EXPECT_EQ(args.type, dqRender::RenderTexture::Type::Normal);
    EXPECT_TRUE(args.imageData.empty());
    EXPECT_EQ(args.format, ImageSourceFormat::Png);
    // ownership defaults to unset (no external, no cache).
    EXPECT_FALSE(args.ownership.has_value());
}

// Authored: no reference test exists in itwinjs-core for MeshArgs default construction;
//           type from core/frontend/src/render/MeshArgs.ts.
TEST(MeshArgs, DefaultConstruction)
{
    MeshArgs args;
    EXPECT_TRUE(args.vertIndices.empty());
    EXPECT_EQ(args.fillFlags, FillFlags::ByView);
    EXPECT_FALSE(args.isPlanar);
    EXPECT_FALSE(args.is2d);
    EXPECT_FALSE(args.hasBakedLighting);
    EXPECT_EQ(args.material, nullptr);
    EXPECT_FALSE(args.textureMapping.has_value());
}

// Authored: no reference test exists in itwinjs-core for MeshArgs triangle assembly;
//           type from core/frontend/src/render/MeshArgs.ts.
TEST(MeshArgs, AddTriangles)
{
    MeshArgs args;
    QParams3d params;
    params.setFromRange(Range3d::CreateXYZXYZ(0, 0, 0, 100, 100, 100));
    args.points = QPoint3dList(params);

    args.points.add(Point3d::From(0, 0, 0));
    args.points.add(Point3d::From(100, 0, 0));
    args.points.add(Point3d::From(50, 100, 0));

    args.vertIndices = {0, 1, 2};

    EXPECT_EQ(args.points.length(), 3);
    EXPECT_EQ(args.vertIndices.size(), 3u);
}

// Authored: no reference test exists in itwinjs-core for MeshArgs ColorIndex; type
//           from core/frontend/src/render/MeshArgs.ts.
TEST(MeshArgs, ColorIndex)
{
    MeshArgs args;
    args.colors.initUniform(ColorDef::red);
    EXPECT_TRUE(args.colors.isUniform());
    EXPECT_TRUE(args.colors.getUniform()->equals(ColorDef::red));
}

// Authored: no reference test exists in itwinjs-core for MeshArgs FeatureIndex; type
//           from core/frontend/src/render/MeshArgs.ts.
TEST(MeshArgs, FeatureIndex)
{
    MeshArgs args;
    args.features.type = FeatureIndexType::Uniform;
    args.features.featureID = 42;
    EXPECT_TRUE(args.features.isUniform());
    EXPECT_EQ(args.features.featureID, 42u);
}

// Authored: no reference test exists in itwinjs-core for PolylineArgs default
//           construction; type from core/frontend/src/render/PolylineArgs.ts.
TEST(PolylineArgs, DefaultConstruction)
{
    PolylineArgs args;
    EXPECT_EQ(args.width, 1);
    EXPECT_EQ(args.linePixels, LinePixels::Solid);
    EXPECT_EQ(args.flags, PolylineFlags::None);
    EXPECT_TRUE(args.polylines.empty());
}

// Authored: no reference test exists in itwinjs-core for PolylineArgs.addPolyline;
//           type from core/frontend/src/render/PolylineArgs.ts.
TEST(PolylineArgs, addPolyline)
{
    PolylineArgs args;
    QParams3d params;
    params.setFromRange(Range3d::CreateXYZXYZ(0, 0, 0, 100, 100, 0));
    args.points = QPoint3dList(params);

    args.points.add(Point3d::From(0, 0, 0));
    args.points.add(Point3d::From(50, 50, 0));
    args.points.add(Point3d::From(100, 0, 0));

    PolylineIndices line = {0, 1, 2};
    args.polylines.push_back(line);

    EXPECT_EQ(args.points.length(), 3);
    EXPECT_EQ(args.polylines.size(), 1u);
    EXPECT_EQ(args.polylines[0].size(), 3u);
}

// Authored: no reference test exists in itwinjs-core for PolylineFlags numeric values;
//           enum order verified 1:1 against core/frontend/src/render/PolylineArgs.ts.
TEST(PolylineFlags, Values)
{
    EXPECT_EQ(static_cast<int>(PolylineFlags::None), 0);
    EXPECT_EQ(static_cast<int>(PolylineFlags::Edge), 1);
    EXPECT_EQ(static_cast<int>(PolylineFlags::Silhouette), 2);
    EXPECT_EQ(static_cast<int>(PolylineFlags::Planar), 4);
    EXPECT_EQ(static_cast<int>(PolylineFlags::Undefined), 128);
}

// Authored: reference test core/common/src/test/OctEncodedNormal.test.ts exists but
//           covers fromVector round-trip, not this trivial type check; type from
//           core/common/src/OctEncodedNormal.ts.
TEST(OctEncodedNormal, TypeCheck)
{
    OctEncodedNormal normal = 0;
    EXPECT_EQ(normal, 0u);
}

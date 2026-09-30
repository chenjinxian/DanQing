// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — imdl surface material JSON parse tests (M-M(1))
// Ported from: itwinjs-core core/frontend/src/common/imdl/ImdlSchema.ts
//              (:86-110 SurfaceMaterialParams / RenderMaterialJson) +
//              ParseImdlDocument.ts:467-475 (convertMaterial string|inline).
// Authored: no direct reference test for the JSON layer (reference exercises
// it through recorded fixtures); the field values below mirror the real
// housemodel-v1 tile `-b-2-0-0-0-1` renderMaterials["0x40"] probe taken
// 2026-09-30 — the first collected model whose saved-view tiles carry
// renderMaterials.
#include "tile/TilesetJson.h"

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqRender::tilejson;

static char const* kSceneJson = R"({
  "renderMaterials": {
    "0x40": {
      "diffuseColor": [1, 1, 1],
      "specularColor": [1, 1, 1],
      "specularExponent": 0.3861,
      "transparency": 0,
      "diffuse": 0,
      "specular": 0,
      "reflect": 0,
      "reflectColor": [1, 1, 1],
      "refract": 1,
      "shadows": false,
      "ambient": 1
    }
  },
  "meshes": {
    "Mesh_Root": {
      "primitives": [
        {
          "type": 0,
          "isPlanar": false,
          "material": "Material0",
          "surface": {
            "indices": "bvindices0Surface",
            "type": 1,
            "material": "0x40"
          },
          "vertices": { "bufferView": "bvVertex0", "count": 4, "numRgbaPerVertex": 4 }
        },
        {
          "type": 0,
          "isPlanar": false,
          "surface": {
            "indices": "bvindices1Surface",
            "type": 1,
            "material": {
              "alpha": 0.5,
              "diffuse": { "color": [0.25, 0.5, 0.75], "weight": 0.7 },
              "specular": { "color": [0.1, 0.2, 0.3], "weight": 0.9, "exponent": 22.0 }
            }
          },
          "vertices": { "bufferView": "bvVertex1", "count": 4, "numRgbaPerVertex": 4 }
        }
      ]
    }
  }
})";

// renderMaterials table: full field extraction (housemodel 0x40 values).
TEST(ImdlMaterialParse, RenderMaterialsTableFields)
{
    auto doc = parseJsonDocument(kSceneJson);
    ASSERT_TRUE(doc);
    auto table = parseImdlRenderMaterials(*doc);
    ASSERT_EQ(table.size(), 1u);
    auto it = table.find("0x40");
    ASSERT_NE(it, table.end());
    ASSERT_TRUE(it->second.valid);
    ASSERT_TRUE(it->second.diffuseWeight && *it->second.diffuseWeight == 0.0f);
    ASSERT_TRUE(it->second.specularWeight && *it->second.specularWeight == 0.0f);
    ASSERT_TRUE(it->second.specularExponent);
    EXPECT_FLOAT_EQ(*it->second.specularExponent, 0.3861f);
    ASSERT_TRUE(it->second.transparency && *it->second.transparency == 0.0f);
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(it->second.diffuseColor[i]);
        EXPECT_FLOAT_EQ(*it->second.diffuseColor[i], 1.0f);
        ASSERT_TRUE(it->second.specularColor[i]);
        EXPECT_FLOAT_EQ(*it->second.specularColor[i], 1.0f);
    }
}

// Primitive surface.material: string key resolves against the table; inline
// SurfaceMaterialParams parse from the nested diffuse/specular objects.
TEST(ImdlMaterialParse, SurfaceMaterialStringKeyAndInline)
{
    auto doc = parseJsonDocument(kSceneJson);
    ASSERT_TRUE(doc);
    auto prims = parseImdlMeshPrimitives(*doc);
    ASSERT_EQ(prims.size(), 2u);

    // Primitive 0: string key "0x40".
    ASSERT_TRUE(prims[0].surface.material);
    EXPECT_FALSE(prims[0].surface.material->isInline);
    EXPECT_EQ(prims[0].surface.material->key, "0x40");

    // Primitive 1: inline params.
    ASSERT_TRUE(prims[1].surface.material);
    ASSERT_TRUE(prims[1].surface.material->isInline);
    auto const& inlineMat = *prims[1].surface.material;
    ASSERT_TRUE(inlineMat.alpha);
    EXPECT_FLOAT_EQ(*inlineMat.alpha, 0.5f);
    ASSERT_TRUE(inlineMat.diffuseWeight);
    EXPECT_FLOAT_EQ(*inlineMat.diffuseWeight, 0.7f);
    ASSERT_TRUE(inlineMat.specularWeight);
    EXPECT_FLOAT_EQ(*inlineMat.specularWeight, 0.9f);
    ASSERT_TRUE(inlineMat.specularExponent);
    EXPECT_FLOAT_EQ(*inlineMat.specularExponent, 22.0f);
    ASSERT_TRUE(inlineMat.diffuseColor[0] && inlineMat.diffuseColor[1] && inlineMat.diffuseColor[2]);
    EXPECT_FLOAT_EQ(*inlineMat.diffuseColor[1], 0.5f);
    ASSERT_TRUE(inlineMat.specularColor[0] && inlineMat.specularColor[1] && inlineMat.specularColor[2]);
    EXPECT_FLOAT_EQ(*inlineMat.specularColor[2], 0.3f);
}

// Missing table + absent key parse to empty (consumer falls to Material.default,
// the reference getMaterial-undefined semantics).
TEST(ImdlMaterialParse, MissingTableYieldsEmpty)
{
    auto doc = parseJsonDocument(R"({"meshes":{"M":{"primitives":[{"type":0,"surface":{"indices":"bv","type":1,"material":"nope"}}]}}})");
    ASSERT_TRUE(doc);
    auto table = parseImdlRenderMaterials(*doc);
    EXPECT_TRUE(table.empty());
    auto prims = parseImdlMeshPrimitives(*doc);
    ASSERT_EQ(prims.size(), 1u);
    ASSERT_TRUE(prims[0].surface.material);
    EXPECT_FALSE(prims[0].surface.material->isInline);
    EXPECT_EQ(prims[0].surface.material->key, "nope");
}

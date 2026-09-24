// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — RenderMaterial and RenderTexture unit tests
//
// Authored: no reference tests exist in itwinjs-core for RenderMaterial/RenderTexture
#include <dqCommon/RenderMaterial.h>
#include <dqCommon/RenderTexture.h>

#include <gtest/gtest.h>

using namespace dqCommon;

// ---------------------------------------------------------------------------
// RenderMaterial
// ---------------------------------------------------------------------------

// Concrete implementation for testing
class TestMaterial : public RenderMaterial {
public:
    TestMaterial() = default;
    TestMaterial(const std::string& k, const TextureMapping* tm = nullptr)
        : RenderMaterial(k, tm ? std::optional<TextureMapping>(*tm) : std::nullopt) {}
};

// Authored: no reference tests exist in itwinjs-core for RenderMaterial/RenderTexture
TEST(RenderMaterialTest, DefaultConstruction)
{
    TestMaterial mat;
    EXPECT_TRUE(mat.key.empty());
    EXPECT_FALSE(mat.hasTexture());
}

// Authored: no reference tests exist in itwinjs-core for RenderMaterial/RenderTexture
TEST(RenderMaterialTest, WithKey)
{
    TestMaterial mat("material_001");
    EXPECT_EQ(mat.key, "material_001");
    EXPECT_FALSE(mat.hasTexture());
}

// Authored: no reference tests exist in itwinjs-core for RenderMaterial/RenderTexture
TEST(RenderMaterialTest, WithTextureMapping)
{
    TextureMappingParams params;
    TextureMapping tm(params);
    TestMaterial mat("mat_with_tex", &tm);
    EXPECT_TRUE(mat.hasTexture());
}

// Authored: no reference tests exist in itwinjs-core for RenderMaterial/RenderTexture
TEST(RenderMaterialTest, Compare)
{
    TestMaterial a("alpha");
    TestMaterial b("beta");
    EXPECT_LT(a.compare(b), 0);
    EXPECT_GT(b.compare(a), 0);
    EXPECT_EQ(a.compare(a), 0);
}

// ---------------------------------------------------------------------------
// RenderTexture
// ---------------------------------------------------------------------------

// Concrete implementation for testing
class TestTexture : public RenderTexture {
public:
    explicit TestTexture(Type t = Type::Normal, int bytes = 0)
        : RenderTexture(t), m_bytes(bytes) {}
    int getBytesUsed() const noexcept override { return m_bytes; }
private:
    int m_bytes;
};

// Authored: no reference tests exist in itwinjs-core for RenderMaterial/RenderTexture
TEST(RenderTextureTest, DefaultConstruction)
{
    TestTexture tex;
    EXPECT_EQ(tex.type, RenderTexture::Type::Normal);
    EXPECT_FALSE(tex.isTileSection());
    EXPECT_FALSE(tex.isGlyph());
    EXPECT_FALSE(tex.isSkyBox());
}

// Authored: no reference tests exist in itwinjs-core for RenderMaterial/RenderTexture
TEST(RenderTextureTest, TileSection)
{
    TestTexture tex(RenderTexture::Type::TileSection, 1024);
    EXPECT_TRUE(tex.isTileSection());
    EXPECT_EQ(tex.getBytesUsed(), 1024);
}

// Authored: no reference tests exist in itwinjs-core for RenderMaterial/RenderTexture
TEST(RenderTextureTest, Glyph)
{
    TestTexture tex(RenderTexture::Type::Glyph);
    EXPECT_TRUE(tex.isGlyph());
}

// Authored: no reference tests exist in itwinjs-core for RenderMaterial/RenderTexture
TEST(RenderTextureTest, SkyBox)
{
    TestTexture tex(RenderTexture::Type::SkyBox);
    EXPECT_TRUE(tex.isSkyBox());
}

// Authored: no reference tests exist in itwinjs-core for RenderMaterial/RenderTexture
TEST(RenderTextureTest, FilteredTileSection)
{
    TestTexture tex(RenderTexture::Type::FilteredTileSection);
    EXPECT_TRUE(tex.isTileSection());
}

// Authored: no reference tests exist in itwinjs-core for RenderMaterial/RenderTexture
TEST(RenderTextureTest, Compare)
{
    TestTexture a(RenderTexture::Type::Normal);
    TestTexture b(RenderTexture::Type::Glyph);
    EXPECT_LT(a.compare(b), 0);
    EXPECT_GT(b.compare(a), 0);
    EXPECT_EQ(a.compare(a), 0);
}

// Authored: no reference tests exist in itwinjs-core for RenderMaterial/RenderTexture
TEST(RenderTextureTest, ThematicGradient)
{
    TestTexture tex(RenderTexture::Type::ThematicGradient);
    EXPECT_EQ(tex.type, RenderTexture::Type::ThematicGradient);
}

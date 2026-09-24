// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — TextureMapping unit tests
//
// Ported from: itwinjs-core core/common/src/test/TextureMapping.test.ts
//              describe("TextureMapping.Params")
#include <dqCommon/TextureMapping.h>
#include <dqCommon/RenderTexture.h>

#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqBase;

namespace {
// Concrete RenderTexture subclass for testing.
class TestTexture : public RenderTexture {
public:
    explicit TestTexture(Type t = Type::Normal, uint64_t id = 0)
        : RenderTexture(t), m_id(id) {}
    uint64_t Id() const noexcept { return m_id; }
private:
    uint64_t m_id = 0;
};
}  // namespace

// Ported from: itwinjs-core core/common/src/test/TextureMapping.test.ts
//              describe("TextureMapping.Params") > constructor
//              it("applies default values when no props provided")
TEST(TextureMappingParamsTest, DefaultsAppliedWhenNoProps)
{
    TextureMappingParams params;
    EXPECT_EQ(params.mode, TextureMappingMode::Parametric);
    EXPECT_DOUBLE_EQ(params.weight, 1.0);
    EXPECT_FALSE(params.worldMapping);
    EXPECT_FALSE(params.useConstantLod);

    // Constant LOD params should have defaults (ref: 4096 * 1024 * 1024)
    EXPECT_EQ(params.constantLodParams.repetitions, 1);
    EXPECT_DOUBLE_EQ(params.constantLodParams.offsetX, 0.0);
    EXPECT_DOUBLE_EQ(params.constantLodParams.offsetY, 0.0);
    EXPECT_DOUBLE_EQ(params.constantLodParams.minDistClamp, 1.0);
    EXPECT_DOUBLE_EQ(params.constantLodParams.maxDistClamp, 4096.0 * 1024.0 * 1024.0);
}

// Ported from: itwinjs-core TextureMapping.test.ts
//              it("applies default constant LOD values when useConstantLod is true but no constantLodProps provided")
TEST(TextureMappingParamsTest, DefaultsWhenUseConstantLodWithoutProps)
{
    TextureMappingProps props;
    props.useConstantLod = true;
    TextureMappingParams params(props);
    EXPECT_TRUE(params.useConstantLod);
    EXPECT_EQ(params.constantLodParams.repetitions, 1);
    EXPECT_DOUBLE_EQ(params.constantLodParams.offsetX, 0.0);
    EXPECT_DOUBLE_EQ(params.constantLodParams.offsetY, 0.0);
    EXPECT_DOUBLE_EQ(params.constantLodParams.minDistClamp, 1.0);
    EXPECT_DOUBLE_EQ(params.constantLodParams.maxDistClamp, 4096.0 * 1024.0 * 1024.0);
}

// Ported from: itwinjs-core TextureMapping.test.ts
//              it("applies default constant LOD values for missing properties in constantLodProps")
TEST(TextureMappingParamsTest, DefaultsForMissingConstantLodProps)
{
    TextureMappingProps props;
    props.useConstantLod = true;
    props.constantLodProps = ConstantLodParamProps{};
    props.constantLodProps->repetitions = 5;
    TextureMappingParams params(props);
    EXPECT_EQ(params.constantLodParams.repetitions, 5);
    EXPECT_DOUBLE_EQ(params.constantLodParams.offsetX, 0.0);
    EXPECT_DOUBLE_EQ(params.constantLodParams.offsetY, 0.0);
    EXPECT_DOUBLE_EQ(params.constantLodParams.minDistClamp, 1.0);
    EXPECT_DOUBLE_EQ(params.constantLodParams.maxDistClamp, 4096.0 * 1024.0 * 1024.0);
}

// Ported from: itwinjs-core TextureMapping.test.ts
//              it("uses provided constant LOD values when all are specified")
TEST(TextureMappingParamsTest, UsesProvidedConstantLodValues)
{
    TextureMappingProps props;
    props.useConstantLod = true;
    props.constantLodProps = ConstantLodParamProps{};
    props.constantLodProps->repetitions = 2;
    props.constantLodProps->offsetX = 10.0;
    props.constantLodProps->offsetY = 20.0;
    props.constantLodProps->minDistClamp = 100.0;
    props.constantLodProps->maxDistClamp = 5000.0;
    TextureMappingParams params(props);
    EXPECT_EQ(params.constantLodParams.repetitions, 2);
    EXPECT_DOUBLE_EQ(params.constantLodParams.offsetX, 10.0);
    EXPECT_DOUBLE_EQ(params.constantLodParams.offsetY, 20.0);
    EXPECT_DOUBLE_EQ(params.constantLodParams.minDistClamp, 100.0);
    EXPECT_DOUBLE_EQ(params.constantLodParams.maxDistClamp, 5000.0);
}

// Authored: compare must include constantLodParams (ref compare logic line 230-232)
TEST(TextureMappingParamsTest, CompareIncludesConstantLodParams)
{
    TextureMappingParams a, b;
    EXPECT_EQ(a.compare(b), 0);
    b.constantLodParams.repetitions = 3;
    EXPECT_NE(a.compare(b), 0);
}

// Authored: TextureMapping.texture field + 2-arg constructor (ref line 40, 48)
TEST(TextureMappingTest, HoldsTextureAndParams)
{
    RefPtr<RenderTexture> tex(new TestTexture(RenderTexture::Type::Normal, 42ULL));
    TextureMappingParams params;
    params.weight = 0.5;
    TextureMapping tm(tex, params);
    EXPECT_EQ(tm.texture.Get(), tex.Get());
    EXPECT_DOUBLE_EQ(tm.params.weight, 0.5);
}

// Authored: NormalMapParams construction + defaults (ref interface lines 16-27)
TEST(NormalMapParamsTest, defaults)
{
    NormalMapParams nmp;
    EXPECT_FALSE(nmp.normalMap);
    EXPECT_FALSE(nmp.greenUp);
    EXPECT_FALSE(nmp.scale.has_value());
    EXPECT_FALSE(nmp.useConstantLod);
}

// Authored: TextureMapping.compare must compare texture + normalMapParams (ref 63-69)
TEST(TextureMappingTest, CompareIncludesTextureAndNormalMap)
{
    RefPtr<RenderTexture> t1(new TestTexture(RenderTexture::Type::Normal, 1ULL));
    RefPtr<RenderTexture> t2(new TestTexture(RenderTexture::Type::Normal, 2ULL));
    TextureMappingParams params;
    TextureMapping a(t1, params);
    TextureMapping b(t2, params);
    // Different textures (by type, both normal, but identity differs) → equal per RenderTexture::Compare
    // (RenderTexture::Compare only compares type). Confirm 0 here.
    EXPECT_EQ(a.compare(b), 0);

    // Setting normalMapParams on one but not the other → differs
    a.normalMapParams = NormalMapParams{};
    a.normalMapParams->greenUp = true;
    EXPECT_NE(a.compare(b), 0);
}

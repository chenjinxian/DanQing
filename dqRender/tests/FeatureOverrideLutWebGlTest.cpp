// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — webgl FeatureOverrides surface on FeatureOverrideLUT
// Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
//              isUniform / getUniformOverrides / lutParams / anyOverridden
#include <dqBase/DqId.h>

#include <dqCommon/FeatureTable.h>
#include <dqCommon/OvrFlags.h>
#include <dqCommon/PackedFeatureTable.h>

#include "render/FeatureOverrideLUT.h"
#include "render/Uniforms.h"

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqCommon;
using namespace dqBase;

namespace {
// Build a uniform (1-feature) LUT with a known override (out-param: LUT is non-copyable).
void setupUniformLut(FeatureOverrideLUT& lut, FeatureOverrideData const& data)
{
    FeatureTable ft(1, DqId{1});
    ft.insert(Feature(DqId{42}));
    PackedFeatureTable packed = PackedFeatureTable::pack(ft);
    lut.initialize(packed);
    lut.setFeatureOverride(0, data);
}
}  // namespace

// A 1-feature LUT is the uniform case (3 texels x 1 row).
// Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
//              TEST(FeatureOverrideLutWebGlTest, IsUniformForSingleFeature)
TEST(FeatureOverrideLutWebGlTest, IsUniformForSingleFeature)
{
    FeatureOverrideLUT lut;
    setupUniformLut(lut, FeatureOverrideData{});
    EXPECT_TRUE(lut.isUniform());

    float lp[2] = {0.0f, 0.0f};
    lut.getLutParams(lp);
    EXPECT_FLOAT_EQ(lp[0], 3.0f);  // 3 texels wide
    EXPECT_FLOAT_EQ(lp[1], 1.0f);  // 1 row
}

// getUniformOverrides returns the full 12-byte (3 RGBA texel) feature record;
// the uniform shader path (BatchUniforms) reads texels 0-1 (bytes [0..7]):
//   [0]=OvrFlags low, [1]=OvrFlags16 high, [2]=lineCode, [3]=lineWeight,
//   [4]=r, [5]=g, [6]=b, [7]=alpha.  (texel 2 [8..11]=line rgb/alpha unused here.)
// Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
//              TEST(FeatureOverrideLutWebGlTest, UniformOverrideEncoding)
TEST(FeatureOverrideLutWebGlTest, UniformOverrideEncoding)
{
    FeatureOverrideData d;
    d.flags = OvrFlag::Rgb;
    d.r = 10; d.g = 20; d.b = 30; d.alpha = 200;

    FeatureOverrideLUT lut;
    setupUniformLut(lut, d);
    ASSERT_TRUE(lut.isUniform());

    uint8_t const* uo = lut.getUniformOverrides();
    ASSERT_NE(uo, nullptr);
    // uo[0] low byte carries OvrFlag::Rgb (=2).
    EXPECT_NE(uo[0] & static_cast<uint8_t>(OvrFlag::Rgb), 0u);
    // uo[4..6] = rgb; uo[7] = alpha (the BatchUniforms bind layout).
    EXPECT_EQ(uo[4], 10);
    EXPECT_EQ(uo[5], 20);
    EXPECT_EQ(uo[6], 30);
    EXPECT_EQ(uo[7], 200);
}

// anyOverridden reflects whether a feature carries a non-None flag.
// Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
//              TEST(FeatureOverrideLutWebGlTest, anyOverridden)
TEST(FeatureOverrideLutWebGlTest, anyOverridden)
{
    FeatureOverrideLUT lutNone;
    setupUniformLut(lutNone, FeatureOverrideData{});  // flags default None
    EXPECT_FALSE(lutNone.anyOverridden());

    FeatureOverrideData overridden;
    overridden.flags = OvrFlag::Rgb;
    FeatureOverrideLUT lutOvr;
    setupUniformLut(lutOvr, overridden);
    EXPECT_TRUE(lutOvr.anyOverridden());
}

// Alpha flag is read from the same low byte (BatchUniforms.bindUniformTransparencyOverride).
// Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
//              TEST(FeatureOverrideLutWebGlTest, AlphaFlagEncoding)
TEST(FeatureOverrideLutWebGlTest, AlphaFlagEncoding)
{
    FeatureOverrideData d;
    d.flags = OvrFlag::Alpha;
    d.alpha = 128;
    FeatureOverrideLUT lut;
    setupUniformLut(lut, d);
    uint8_t const* uo = lut.getUniformOverrides();
    EXPECT_NE(uo[0] & static_cast<uint8_t>(OvrFlag::Alpha), 0u);
    EXPECT_EQ(uo[7], 128);
}

// --- BatchUniforms bind methods (read the LUT uniform surface) ---

// bindUniformColorOverride uploads the overridden rgb, or the (-1,-1,-1) sentinel.
// Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
//              TEST(FeatureOverrideLutWebGlTest, BatchUniformsColorBind)
TEST(FeatureOverrideLutWebGlTest, BatchUniformsColorBind)
{
    FeatureOverrideData d;
    d.flags = OvrFlag::Rgb;
    d.r = 255; d.g = 0; d.b = 0;
    FeatureOverrideLUT lut;
    setupUniformLut(lut, d);

    BatchUniforms batch;
    batch.setOverrides(&lut);
    UniformHandle h;
    batch.bindUniformColorOverride(h);
    EXPECT_FLOAT_EQ(h.getData()[0], 1.0f);
    EXPECT_FLOAT_EQ(h.getData()[1], 0.0f);
    EXPECT_FLOAT_EQ(h.getData()[2], 0.0f);

    // No overrides set -> sentinel.
    BatchUniforms bare;
    bare.setOverrides(nullptr);
    UniformHandle h2;
    bare.bindUniformColorOverride(h2);
    EXPECT_FLOAT_EQ(h2.getData()[0], -1.0f);
}

// bindUniformTransparencyOverride uploads alpha or -1.
// Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
//              TEST(FeatureOverrideLutWebGlTest, BatchUniformsTransparencyBind)
TEST(FeatureOverrideLutWebGlTest, BatchUniformsTransparencyBind)
{
    FeatureOverrideData d;
    d.flags = OvrFlag::Alpha;
    d.alpha = 128;
    FeatureOverrideLUT lut;
    setupUniformLut(lut, d);

    BatchUniforms batch;
    batch.setOverrides(&lut);
    UniformHandle h;
    batch.bindUniformTransparencyOverride(h);
    EXPECT_NEAR(h.getData()[0], 128.0f / 255.0f, 1e-5);
}

// bindLUTParams uploads {width, height}.
// Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
//              TEST(FeatureOverrideLutWebGlTest, BatchUniformsLutParamsBind)
TEST(FeatureOverrideLutWebGlTest, BatchUniformsLutParamsBind)
{
    FeatureOverrideLUT lut;
    setupUniformLut(lut, FeatureOverrideData{});
    BatchUniforms batch;
    batch.setOverrides(&lut);
    UniformHandle h;
    batch.bindLUTParams(h);
    EXPECT_FLOAT_EQ(h.getData()[0], 3.0f);
    EXPECT_FLOAT_EQ(h.getData()[1], 1.0f);
}

// bindUniformSymbologyFlags uploads the EmphasisFlags bitmask derived from the
// uniform override encoding, or 0 if no overrides.
// Ported from: itwinjs-core BatchUniforms.bindUniformSymbologyFlags +
//               FeatureOverrides.updateUniformSymbologyFlags (line 71-92)
TEST(FeatureOverrideLutWebGlTest, BatchUniformsSymbologyFlagsFlashed)
{
    FeatureOverrideData d;
    d.flags = OvrFlag::Flashed;
    FeatureOverrideLUT lut;
    setupUniformLut(lut, d);
    BatchUniforms batch;
    batch.setOverrides(&lut);
    UniformHandle h;
    batch.bindUniformSymbologyFlags(h);
    EXPECT_FLOAT_EQ(h.getData()[0], 4.0f);  // EmphasisFlags::Flashed
}

// Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
//              TEST(FeatureOverrideLutWebGlTest, BatchUniformsSymbologyFlagsNonLocatable)
TEST(FeatureOverrideLutWebGlTest, BatchUniformsSymbologyFlagsNonLocatable)
{
    FeatureOverrideData d;
    d.flags = OvrFlag::NonLocatable;
    FeatureOverrideLUT lut;
    setupUniformLut(lut, d);
    BatchUniforms batch;
    batch.setOverrides(&lut);
    UniformHandle h;
    batch.bindUniformSymbologyFlags(h);
    EXPECT_FLOAT_EQ(h.getData()[0], 8.0f);  // EmphasisFlags::NonLocatable
}

// Flashed + NonLocatable combine on the low byte.
// Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
//              TEST(FeatureOverrideLutWebGlTest, BatchUniformsSymbologyFlagsCombinedLowByte)
TEST(FeatureOverrideLutWebGlTest, BatchUniformsSymbologyFlagsCombinedLowByte)
{
    FeatureOverrideData d;
    d.flags = OvrFlag::Flashed | OvrFlag::NonLocatable;
    FeatureOverrideLUT lut;
    setupUniformLut(lut, d);
    BatchUniforms batch;
    batch.setOverrides(&lut);
    UniformHandle h;
    batch.bindUniformSymbologyFlags(h);
    EXPECT_FLOAT_EQ(h.getData()[0], 12.0f);  // Flashed(4) | NonLocatable(8)
}

// Hilite comes from the high byte (flags16), gated on anyHilited().
// Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
//              TEST(FeatureOverrideLutWebGlTest, BatchUniformsSymbologyFlagsHilite)
TEST(FeatureOverrideLutWebGlTest, BatchUniformsSymbologyFlagsHilite)
{
    FeatureOverrideData d;
    d.flags16 = OvrFlags16::Visibility | OvrFlags16::Hilited;
    FeatureOverrideLUT lut;
    setupUniformLut(lut, d);
    BatchUniforms batch;
    batch.setOverrides(&lut);
    UniformHandle h;
    batch.bindUniformSymbologyFlags(h);
    EXPECT_FLOAT_EQ(h.getData()[0], 1.0f);  // EmphasisFlags::Hilite
}

// Hilite + Emphasized combine on the high byte (both gated on anyHilited).
// Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
//              TEST(FeatureOverrideLutWebGlTest, BatchUniformsSymbologyFlagsHiliteAndEmphasized)
TEST(FeatureOverrideLutWebGlTest, BatchUniformsSymbologyFlagsHiliteAndEmphasized)
{
    FeatureOverrideData d;
    d.flags16 = OvrFlags16::Visibility | OvrFlags16::Hilited | OvrFlags16::Emphasized;
    FeatureOverrideLUT lut;
    setupUniformLut(lut, d);
    BatchUniforms batch;
    batch.setOverrides(&lut);
    UniformHandle h;
    batch.bindUniformSymbologyFlags(h);
    EXPECT_FLOAT_EQ(h.getData()[0], 3.0f);  // EmphasisFlags::Hilite(1) | Emphasized(2)
}

// No overrides -> 0.
// Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
//              TEST(FeatureOverrideLutWebGlTest, BatchUniformsSymbologyFlagsNoOverrides)
TEST(FeatureOverrideLutWebGlTest, BatchUniformsSymbologyFlagsNoOverrides)
{
    BatchUniforms batch;
    batch.setOverrides(nullptr);
    UniformHandle h;
    batch.bindUniformSymbologyFlags(h);
    EXPECT_FLOAT_EQ(h.getData()[0], 0.0f);
}

// bindNumThematicSensors: with sensors set, uploads the sensor count; with none,
// leaves the uniform untouched (no-op). Ported from BatchUniforms.bindNumThematicSensors.
TEST(FeatureOverrideLutWebGlTest, BatchUniformsNumThematicSensors)
{
    std::vector<ThematicDisplaySensor> sensors = {
        ThematicDisplaySensor::fromJSON(0.0, 0.0, 0.0, 1.0),
        ThematicDisplaySensor::fromJSON(1.0, 1.0, 1.0, 2.0),
        ThematicDisplaySensor::fromJSON(2.0, 2.0, 2.0, 3.0),
    };
    ThematicSensors ts = ThematicSensors::create(sensors, dqGeom::Transform::CreateIdentity());
    ASSERT_EQ(ts.numSensors(), 3u);

    // ThematicSensors.bindNumSensors uploads the count.
    UniformHandle h0;
    ts.bindNumSensors(h0);
    EXPECT_EQ(h0.getIntData()[0], 3);

    // BatchUniforms.bindNumThematicSensors forwards to the sensors.
    BatchUniforms batch;
    batch.setSensors(&ts);
    UniformHandle h1;
    batch.bindNumThematicSensors(h1);
    EXPECT_EQ(h1.getIntData()[0], 3);

    // No sensors -> no-op (uniform left at default 0).
    BatchUniforms bare;
    bare.setSensors(nullptr);
    UniformHandle h2;
    bare.bindNumThematicSensors(h2);
    EXPECT_EQ(h2.getIntData()[0], 0);
}

// setCurrentBatch assigns a batch ID, encodes it, and picks feature mode.
// Ported from: itwinjs-core BatchUniforms.setCurrentBatch (line 51-92)
TEST(FeatureOverrideLutWebGlTest, BatchUniformsSetCurrentBatch)
{
    Batch batch(1);
    BatchState state;
    BatchUniforms bu;
    bu.setCurrentBatch(batch, state);

    // First batch gets a non-zero ID; no overrides -> Pick mode (1).
    EXPECT_NE(bu.getBatchId(), 0u);
    EXPECT_EQ(bu.getFeatureMode(), uint8_t(1));  // Pick

    // The batchId is encoded as one vec4 (4 floats) for the shader.
    UniformHandle h;
    bu.bindBatchId(h);
    EXPECT_EQ(h.getType(), UniformHandle::UniformType::Vec4);

    // clearCurrentBatch resets to None.
    bu.clearCurrentBatch(state);
    EXPECT_EQ(bu.getBatchId(), 0u);
    EXPECT_EQ(bu.getFeatureMode(), uint8_t(0));  // None
}

// bindLUT sets the sampler uniform to (unit - GL::TextureUnit::Zero).
// The texture-bind half (system.bindTexture2d) is GL-only and live-verified;
// here we test only the setUniform1i value path (system = nullptr).
// Ported from: itwinjs-core Texture.bindSampler() (line 474-479)
//               + FeatureOverrides.bindLUT() (line 447-452)
TEST(FeatureOverrideLutWebGlTest, BatchUniformsLutBind)
{
    FeatureOverrideLUT lut;
    setupUniformLut(lut, FeatureOverrideData{});

    BatchUniforms batch;
    batch.setOverrides(&lut);
    UniformHandle h;
    // Pass nullptr system — skips the GL texture-bind, tests only sampler value.
    batch.bindLUT(h, nullptr, static_cast<uint32_t>(GL::TextureUnit::FeatureSymbology));
    // FeatureSymbology = GL::TextureUnit::One = 0x84C1; Zero = 0x84C0; index = 1.
    EXPECT_EQ(h.getIntData()[0], 1);
}

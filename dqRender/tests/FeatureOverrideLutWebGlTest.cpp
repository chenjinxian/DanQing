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
    // dqCommon::ThematicDisplaySensor（M-S 归位：dqRender 原地复刻的同名
    // struct 删——参考 ThematicSensors.ts 自 core-common 导入同型）。
    std::vector<dqCommon::ThematicDisplaySensor> sensors;
    {
        auto mk = [](double x, double y, double z, double v) {
            dqCommon::ThematicDisplaySensor s;
            s.position = dqGeom::Point3d::From(x, y, z);
            s.value = v;
            return s;
        };
        sensors.push_back(mk(0.0, 0.0, 0.0, 1.0));
        sensors.push_back(mk(1.0, 1.0, 1.0, 2.0));
        sensors.push_back(mk(2.0, 2.0, 2.0, 3.0));
    }
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

// --- M-J(2)：override 归属门（hover 高亮构件颜色丢失修复的机制锁） ---

// anyOverridden covers the HIGH byte of the override word: a Hilited-only row
// (flags16.Hilited, no low-byte flag) counts as overridden — the reference
// counts the full OvrFlags word on BOTH the build path (:279-280) and the
// flash/hilite update path (:329-331), so selection-hilite keeps the batch's
// Overrides variant active. The prior form scanned only the low byte.
// Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
//              buildLookupTable (:279-280) + updateFlashedAndHilited (:329-331)
// Authored: no reference test isolates anyOverridden's high-byte half
//           (FeatureOverrides.test.ts drives it through full update flows).
TEST(FeatureOverrideLutWebGlTest, anyOverriddenCoversHighByte)
{
    // Hilited-only row: flags16 = Hilited (low byte None).
    FeatureOverrideLUT lut;
    FeatureTable ft(1, DqId{1});
    ft.insert(Feature(DqId{7}));
    lut.initialize(PackedFeatureTable::pack(ft));
    ASSERT_FALSE(lut.anyOverridden());  // all-defaults rows are not overridden
    lut.setFeatureHilited(0, true);
    EXPECT_TRUE(lut.anyOverridden())
        << "hilited-only row must count as overridden (reference counts the "
           "full 16-bit override word) — otherwise selection hilite loses the "
           "Overrides variant";

    // Flash-only row (low byte) still counts.
    FeatureOverrideLUT lutFlash;
    FeatureTable ft2(1, DqId{1});
    ft2.insert(Feature(DqId{8}));
    lutFlash.initialize(PackedFeatureTable::pack(ft2));
    lutFlash.setFeatureFlashed(0, true);
    EXPECT_TRUE(lutFlash.anyOverridden());
}

// The always-on "row is visible" marker (DanQing's inverted-Visibility
// convention — OvrFlags16.h:48, FeatureOverrideLUT.cpp:52) does NOT count as
// an override: an all-defaults LUT must stay inactive, or every batched tile
// renders the Overrides variant from its first draw (the prior gate's form).
// EQUIVALENCE: 参考源 = FeatureOverrides.buildLookupTable :279-280（Visibility
// 位在参考是"隐藏"标记，仅隐藏行携带并计入 nOverridden）；发散 = DanQing 把
// 该位反极性用作"可见"行标记（每行携带）；验证法 = 本锁（全默认表恒 false）
// + FeatureOverrideLutWebGlTest.anyOverriddenCoversHighByte（真实覆盖位恒 true）。
// Authored: no reference test exists for the marker-polarity equivalence.
TEST(FeatureOverrideLutWebGlTest, anyOverriddenNotFiredByVisibilityMarker)
{
    FeatureOverrideLUT lut;
    setupUniformLut(lut, FeatureOverrideData{});  // flags16 = Visibility marker only
    EXPECT_FALSE(lut.anyOverridden())
        << "the always-on visible-row marker must not count as an override";

    // After a flash add+remove cycle (updateFeatureStates's full recompute),
    // rows return to the marker-only state — still not overridden.
    lut.setFeatureFlashed(0, true);
    ASSERT_TRUE(lut.anyOverridden());
    lut.setFeatureFlashed(0, false);
    EXPECT_FALSE(lut.anyOverridden());
}

// A batch WITHOUT a feature table has no override LUT: getOrCreateFeature-
// OverrideLUT returns nullptr and hasFeatureOverrides stays false. The prior
// form conjured a dead LUT object for a tableless batch — hasFeatureOverrides
// reported true while the draw path bound no texture for it, so the Overrides
// variant would sample whatever LUT the previous batch left on the sampler
// unit (cross-batch override contamination — TD-28③).
// Ported from: itwinjs-core FeatureOverrides.initFromMap (:397-410) — the
//              overrides object is built from the batch's feature table only
//              (`assert(0 < nFeatures)`, :399).
// Authored: no reference test exists for the tableless-batch form (the
//           reference's Batch constructor requires a feature table).
TEST(FeatureOverrideLutWebGlTest, TablelessBatchHasNoOverrideLut)
{
    Batch batch(1);  // featureCount=1, no feature table
    EXPECT_FALSE(batch.hasFeatureOverrides());
    EXPECT_EQ(batch.getOrCreateFeatureOverrideLUT(), nullptr);
    EXPECT_EQ(batch.getFeatureOverrideLUT(), nullptr);

    // The lazy feature-state update is a no-op for a tableless batch (the
    // reference's per-batch overrides are defined by the table).
    std::vector<uint32_t> const hiliteIds = {42u};
    batch.updateFeatureStates(hiliteIds, 42u);
    EXPECT_EQ(batch.getFeatureOverrideLUT(), nullptr);
}

// BatchUniforms.setCurrentBatch resolves the ACTIVE override set exactly as
// the reference's _setCurrentBatch (:74): LUT present AND anyOverridden —
// else nullptr (FeatureMode falls back to Pick/None at :84-89, covered by
// BatchUniformsSetCurrentBatch above).
// Ported from: itwinjs-core internal/render/webgl/BatchUniforms.ts:74
//              TEST(FeatureOverrideLutWebGlTest, BatchUniformsSetCurrentBatch)
TEST(FeatureOverrideLutWebGlTest, SetCurrentBatchActiveOverridesRequireAnyOverridden)
{
    // Batch WITH a table, no overridden features: LUT exists after the lazy
    // update but the active set stays null (Pick mode, not Overrides).
    auto table = std::make_unique<FeatureTable>(2, DqId{1});
    table->insert(Feature(DqId{42}));
    table->insert(Feature(DqId{43}));
    Batch batch(2, std::move(table));
    BatchState state;
    BatchUniforms bu;
    bu.setCurrentBatch(batch, state);
    ASSERT_NE(batch.getFeatureOverrideLUT(), nullptr);
    EXPECT_EQ(bu.getFeatureMode(), uint8_t(1));  // Pick — not Overrides
    bu.clearCurrentBatch(state);

    // Flash one feature -> the set activates (Overrides mode = 2).
    std::vector<uint32_t> const none;
    batch.updateFeatureStates(none, 42u);
    bu.setCurrentBatch(batch, state);
    EXPECT_EQ(bu.getFeatureMode(), uint8_t(2));  // Overrides
    bu.clearCurrentBatch(state);

    // Hilite-only (no flash, no rgb) also activates — full-word scan.
    batch.updateFeatureStates({42u}, 0u);
    bu.setCurrentBatch(batch, state);
    EXPECT_EQ(bu.getFeatureMode(), uint8_t(2));  // Overrides
    bu.clearCurrentBatch(state);
}

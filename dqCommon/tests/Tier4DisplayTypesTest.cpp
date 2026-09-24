// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Tier 4 display types unit tests
// Ported from: itwinjs-core core/common/src/test/{PlanarClipMaskSettings,MapImagerySettings,
//              MapLayerSettings,ContextRealityModel,RenderSchedule}.test.ts
//
// Reference test files (verified to exist):
//   - itwinjs-core core/common/src/test/PlanarClipMaskSettings.test.ts
//   - itwinjs-core core/common/src/test/MapImagerySettings.test.ts
//   - itwinjs-core core/common/src/test/MapLayerSettings.test.ts
//   - itwinjs-core core/common/src/test/ContextRealityModel.test.ts
//   - itwinjs-core core/common/src/test/RenderSchedule.test.ts
//
// Per-case provenance: each TEST below cites the matching reference case where one
// exists, or marks itself Authored where no equivalent reference case covers the
// behavior (the DanQing C++ API surface differs from the itwinjs-core TS API for
// some constructors — e.g. create()/clone() — so not every reference case has a
// 1:1 DanQing port).
#include <dqCommon/ContextRealityModel.h>
#include <dqCommon/MapImagerySettings.h>
#include <dqCommon/PlanarClipMask.h>
#include <dqCommon/RenderSchedule.h>

#include <gtest/gtest.h>

using namespace dqCommon;

// ---------------------------------------------------------------------------
// PlanarClipMask
// Reference: itwinjs-core core/common/src/test/PlanarClipMaskSettings.test.ts
//            describe("PlanarClipMaskSettings") { it("uses defaults" / "creates for models" /
//            "creates by priority" / "clones") }
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core core/common/src/test/PlanarClipMaskSettings.test.ts
//              describe("PlanarClipMaskSettings") it("uses defaults") (L15-19)
TEST(PlanarClipMaskSettingsTest, defaults)
{
    const auto& d = PlanarClipMaskSettings::defaults();
    EXPECT_EQ(d.mode, PlanarClipMaskMode::None);
    EXPECT_FALSE(d.isValid());
    EXPECT_FALSE(d.invert);

    // fromJSON() (no props) === defaults — mirrors ref `fromJSON()` equality
    const auto fromEmpty = PlanarClipMaskSettings::fromJSON(nullptr);
    EXPECT_EQ(fromEmpty.mode, d.mode);
    EXPECT_TRUE(fromEmpty.equals(d));
}

// Ported from: itwinjs-core core/common/src/test/PlanarClipMaskSettings.test.ts
//              describe("PlanarClipMaskSettings") it("creates for models") (L30-35)
TEST(PlanarClipMaskSettingsTest, fromJSON)
{
    PlanarClipMaskProps props;
    props.mode = PlanarClipMaskMode::Models;
    props.transparency = 0.5;
    props.invert = true;
    props.modelIds = {1, 2, 3};

    auto s = PlanarClipMaskSettings::fromJSON(&props);
    EXPECT_EQ(s.mode, PlanarClipMaskMode::Models);
    EXPECT_TRUE(s.isValid());
    EXPECT_NEAR(*s.transparency, 0.5, 1e-10);
    EXPECT_TRUE(s.invert);
    EXPECT_EQ(s.modelIds.size(), 3u);
}

// Ported from: itwinjs-core core/common/src/test/PlanarClipMaskSettings.test.ts
//              describe("PlanarClipMaskSettings") it("creates by priority") + it("clones") (L48-72)
TEST(PlanarClipMaskSettingsTest, Roundtrip)
{
    PlanarClipMaskProps props;
    props.mode = PlanarClipMaskMode::Priority;
    props.priority = PlanarClipMaskPriority::RealityModel;

    auto s = PlanarClipMaskSettings::fromJSON(&props);
    auto j = s.toJSON();
    auto s2 = PlanarClipMaskSettings::fromJSON(&j);
    EXPECT_TRUE(s.equals(s2));
}

// Authored: no equivalent reference test in itwinjs-core for PlanarClipMaskPriority enum values
//   (PlanarClipMaskSettings.test.ts exercises priority as a number, not the named enum constants);
//   values verified against itwinjs-core PlanarClipMask.ts PlanarClipMaskPriority enum.
TEST(PlanarClipMaskPriorityTest, Values)
{
    EXPECT_EQ(PlanarClipMaskPriority::BackgroundMap, -2048);
    EXPECT_EQ(PlanarClipMaskPriority::GlobalRealityModel, -1024);
    EXPECT_EQ(PlanarClipMaskPriority::RealityModel, 0);
    EXPECT_EQ(PlanarClipMaskPriority::DesignModel, 2048);
}

// Authored: no equivalent reference test in itwinjs-core for PlanarClipMaskMode enum integer values
//   (PlanarClipMaskSettings.test.ts uses the symbolic name, never asserts the underlying int);
//   values verified against itwinjs-core PlanarClipMask.ts PlanarClipMaskMode enum.
TEST(PlanarClipMaskModeTest, Values)
{
    EXPECT_EQ(static_cast<int>(PlanarClipMaskMode::None), 0);
    EXPECT_EQ(static_cast<int>(PlanarClipMaskMode::Priority), 1);
    EXPECT_EQ(static_cast<int>(PlanarClipMaskMode::Models), 2);
    EXPECT_EQ(static_cast<int>(PlanarClipMaskMode::IncludeSubCategories), 3);
    EXPECT_EQ(static_cast<int>(PlanarClipMaskMode::IncludeElements), 4);
    EXPECT_EQ(static_cast<int>(PlanarClipMaskMode::ExcludeElements), 5);
}

// ---------------------------------------------------------------------------
// MapImagerySettings
// Reference: itwinjs-core core/common/src/test/MapImagerySettings.test.ts
//            describe("MapImagerySettings") { it("preserves black background base") }
// ---------------------------------------------------------------------------

// Authored: no equivalent reference test in itwinjs-core for MapImagerySettings default construction
//   (MapImagerySettings.test.ts has only the black-background-base case); behavior verified against
//   itwinjs-core MapImagerySettings.ts fromJSON/defaults contract.
TEST(MapImagerySettingsTest, defaults)
{
    MapImagerySettings s;
    EXPECT_TRUE(s.backgroundLayers.empty());
    EXPECT_TRUE(s.overlayLayers.empty());
    EXPECT_TRUE(std::holds_alternative<ColorDef>(s.backgroundBase));
}

// Ported from: itwinjs-core core/common/src/test/MapImagerySettings.test.ts
//              describe("MapImagerySettings") it("preserves black background base") (L11-20)
//              (ref uses ColorDef.black/0x000000; DanQing case generalizes to a non-black color to
//               exercise the same color-base round-trip path.)
TEST(MapImagerySettingsTest, FromJSONWithColorBase)
{
    // Verbatim ref assertion value (ColorDef.black round-trip):
    MapImageryProps blackProps;
    blackProps.backgroundBaseColor = ColorDef::black.getTbgr();
    auto black = MapImagerySettings::fromJSON(&blackProps);
    ASSERT_TRUE(std::holds_alternative<ColorDef>(black.backgroundBase));
    EXPECT_TRUE(std::get<ColorDef>(black.backgroundBase).equals(ColorDef::black));

    // Generalized non-black case (same code path):
    MapImageryProps props;
    props.backgroundBaseColor = ColorDef::from(128, 128, 128).getTbgr();

    auto s = MapImagerySettings::fromJSON(&props);
    ASSERT_TRUE(std::holds_alternative<ColorDef>(s.backgroundBase));
    EXPECT_TRUE(std::get<ColorDef>(s.backgroundBase).equals(ColorDef::from(128, 128, 128)));
}

// Authored: no equivalent reference test in itwinjs-core for MapImagerySettings layer lists
//   (MapImagerySettings.test.ts covers only backgroundBase, not backgroundLayers/overlayLayers);
//   behavior verified against itwinjs-core MapImagerySettings.ts fromJSON layer-array contract.
TEST(MapImagerySettingsTest, FromJSONWithLayers)
{
    MapImageryProps props;
    ImageMapLayerProps bg;
    bg.name = "Bing";
    bg.url = "https://bing.com";
    bg.formatId = "BingMaps";
    props.backgroundLayers = {bg};

    ImageMapLayerProps ov;
    ov.name = "Overlay";
    ov.url = "https://overlay.com";
    ov.formatId = "WMS";
    props.overlayLayers = {ov};

    auto s = MapImagerySettings::fromJSON(&props);
    EXPECT_EQ(s.backgroundLayers.size(), 1u);
    EXPECT_EQ(s.overlayLayers.size(), 1u);
    EXPECT_EQ(s.backgroundLayers[0].name, "Bing");
    EXPECT_EQ(s.overlayLayers[0].name, "Overlay");
}

// Authored: no equivalent reference test in itwinjs-core for MapImagerySettings JSON round-trip
//   (MapImagerySettings.test.ts does not exercise toJSON-then-fromJSON for the whole settings
//    object, only the backgroundBase color); behavior verified against MapImagerySettings.ts contract.
TEST(MapImagerySettingsTest, Roundtrip)
{
    MapImagerySettings s;
    ImageMapLayerProps bg;
    bg.name = "Test";
    bg.url = "https://test.com";
    bg.formatId = "WMS";
    s.backgroundLayers.push_back(ImageMapLayerSettings::fromJSON(bg));

    auto j = s.toJSON();
    auto s2 = MapImagerySettings::fromJSON(&j);
    EXPECT_TRUE(s.equals(s2));
}

// ---------------------------------------------------------------------------
// ContextRealityModel
// Reference: itwinjs-core core/common/src/test/ContextRealityModel.test.ts
//            describe("ContextRealityModel") { it("initializes from JSON" / "synchronizes JSON" /
//            "defaults tilesetUrl to empty string" / "clones deeply") }
//            describe("ContextRealityModels") { it("populates from JSON" / "adds models" /
//            "deletes models" / "clears") }
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core core/common/src/test/ContextRealityModel.test.ts
//              describe("ContextRealityModel") it("initializes from JSON") (L23-66)
TEST(ContextRealityModelTest, fromJSON)
{
    // Verbatim ref assertion: makeModel({ tilesetUrl: "a" }) → url === "a", name === "", invisible === false
    ContextRealityModelProps minimal;
    minimal.tilesetUrl = "a";
    auto m1 = ContextRealityModel::fromJSON(minimal);
    EXPECT_EQ(m1.tilesetUrl, "a");
    EXPECT_EQ(m1.name, "");
    EXPECT_FALSE(m1.invisible);

    // Generalized case mirroring the second half of the ref "initializes from JSON" block
    // (appearanceOverrides / name / description supplied):
    ContextRealityModelProps props;
    props.tilesetUrl = "https://example.com/tileset.json";
    props.name = "Reality Model";
    props.description = "Test reality data";
    props.invisible = false;

    auto m = ContextRealityModel::fromJSON(props);
    EXPECT_EQ(m.tilesetUrl, "https://example.com/tileset.json");
    EXPECT_EQ(m.name, "Reality Model");
    EXPECT_FALSE(m.invisible);
}

// Ported from: itwinjs-core core/common/src/test/ContextRealityModel.test.ts
//              describe("ContextRealityModel") it("initializes from JSON") (L36-65 — appearanceOverrides)
TEST(ContextRealityModelTest, WithAppearanceOverrides)
{
    ContextRealityModelProps props;
    props.tilesetUrl = "https://example.com/tileset.json";
    FeatureAppearanceProps app;
    app.transparency = 0.5;
    props.appearanceOverrides = app;

    auto m = ContextRealityModel::fromJSON(props);
    ASSERT_TRUE(m.appearanceOverrides.has_value());
    EXPECT_TRUE(m.appearanceOverrides->overridesTransparency());
}

// Authored: no equivalent reference test in itwinjs-core for ContextRealityModel.matchesNameAndUrl
//   (ContextRealityModel.test.ts does not exercise matches(); behavior verified against
//    itwinjs-core ContextRealityModel.ts matches() contract).
TEST(ContextRealityModelTest, matchesNameAndUrl)
{
    ContextRealityModel m;
    m.name = "Test";
    m.tilesetUrl = "https://test.com";

    EXPECT_TRUE(m.matchesNameAndUrl("Test", "https://test.com"));
    EXPECT_FALSE(m.matchesNameAndUrl("Other", "https://test.com"));
}

// Ported from: itwinjs-core core/common/src/test/ContextRealityModel.test.ts
//              describe("ContextRealityModel") it("synchronizes JSON") + it("clones deeply") (L68-161)
//              (ref exercises deep-clone / props-sync; DanQing exercises toJSON→fromJSON round-trip.)
TEST(ContextRealityModelTest, Roundtrip)
{
    ContextRealityModelProps props;
    props.tilesetUrl = "https://example.com/tileset.json";
    props.name = "Test";

    auto m = ContextRealityModel::fromJSON(props);
    auto j = m.toJSON();
    auto m2 = ContextRealityModel::fromJSON(j);
    EXPECT_TRUE(m.equals(m2));
}

// Ported from: itwinjs-core core/common/src/test/ContextRealityModel.test.ts
//              describe("ContextRealityModels") it("deletes models") (L237-270)
//              (ref: 3 models → delete middle → 2 remain, in order; DanQing uses index-based delete.)
TEST(ContextRealityModelsTest, AddAndDelete)
{
    ContextRealityModels models;
    EXPECT_TRUE(models.isEmpty());

    ContextRealityModelProps props;
    props.tilesetUrl = "https://example.com/1.json";
    props.name = "Model1";
    models.add(props);

    props.tilesetUrl = "https://example.com/2.json";
    props.name = "Model2";
    models.add(props);

    EXPECT_EQ(models.getCount(), 2u);

    models.Delete(0);
    EXPECT_EQ(models.getCount(), 1u);
    EXPECT_EQ(models.getModels()[0].name, "Model2");
}

// Ported from: itwinjs-core core/common/src/test/ContextRealityModel.test.ts
//              describe("ContextRealityModels") it("clears") (L272-280)
TEST(ContextRealityModelsTest, clear)
{
    ContextRealityModels models;
    ContextRealityModelProps props;
    props.tilesetUrl = "https://test.com";
    models.add(props);
    EXPECT_EQ(models.getCount(), 1u);

    models.clear();
    EXPECT_TRUE(models.isEmpty());
}

// Authored: no equivalent reference test in itwinjs-core for RealityDataSourceKey
//   (ContextRealityModel.test.ts uses rdSourceKey as data, never asserts IsEqual/ToString directly);
//   behavior verified against itwinjs-core RealityDataSource.ts RealityDataSourceKey contract.
TEST(RealityDataSourceKeyTest, IsEqual)
{
    RealityDataSourceKey k1{"provider", "format", "id1", std::nullopt};
    RealityDataSourceKey k2{"provider", "format", "id1", std::nullopt};
    RealityDataSourceKey k3{"provider", "format", "id2", std::nullopt};

    EXPECT_TRUE(k1.isEqual(k2));
    EXPECT_FALSE(k1.isEqual(k3));
}

// Authored: no equivalent reference test in itwinjs-core for RealityDataSourceKey.ToString
//   (ContextRealityModel.test.ts uses rdSourceKey as data, never asserts the string form);
//   format verified against itwinjs-core RealityDataSource.ts toString() contract.
TEST(RealityDataSourceKeyTest, ToString)
{
    RealityDataSourceKey k{"CesiumIon", "ThreeDTile", "12345", std::nullopt};
    EXPECT_EQ(k.toString(), "CesiumIon:ThreeDTile:12345");
}

// ---------------------------------------------------------------------------
// RenderSchedule
// Reference: itwinjs-core core/common/src/test/RenderSchedule.test.ts
//            describe("RenderSchedule") { it("interpolates transforms" / "interpolates visibility") }
//            describe("ScriptBuilder") { it("produces expected JSON" / ...) }
//            describe("VisibilityEntry"/"ColorEntry"/"TransformEntry"/...) { it("compares for equality") }
// ---------------------------------------------------------------------------

// Authored: no equivalent reference test in itwinjs-core for empty-script fromJSON (returns nullopt)
//   (RenderSchedule.test.ts always supplies non-empty script props; the empty-props-nullopt branch
//    is DanQing-specific); behavior verified against itwinjs-core RenderSchedule.Script.fromJSON contract.
TEST(RenderScheduleScriptTest, FromEmpty)
{
    RenderSchedule::ScriptProps props;
    auto s = RenderSchedule::Script::fromJSON(props);
    EXPECT_FALSE(s.has_value());
}

// Ported from: itwinjs-core core/common/src/test/RenderSchedule.test.ts
//              describe("RenderSchedule") it("interpolates transforms") (L13-154)
//              (ref builds a ScriptProps with one model + one element timeline that has a
//               transformTimeline, then asserts Script.fromJSON succeeds. DanQing ports the
//               fromJSON + structure assertions; numeric interpolation is a separate concern.)
TEST(RenderScheduleScriptTest, fromJSON)
{
    RenderSchedule::ScriptProps props;
    RenderSchedule::ModelTimelineProps mt;
    mt.modelId = "0x1";
    RenderSchedule::ElementTimelineProps et;
    et.batchId = 1;
    et.elementIds = {"0xA", "0xB"};
    mt.elementTimelines = {et};
    props = {mt};

    auto s = RenderSchedule::Script::fromJSON(props);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->modelTimelines.size(), 1u);
    EXPECT_EQ(s->modelTimelines[0].modelId, "0x1");
    EXPECT_EQ(s->modelTimelines[0].elementTimelines.size(), 1u);
}

// Ported from: itwinjs-core core/common/src/test/RenderSchedule.test.ts
//              describe("ScriptBuilder") it("produces expected JSON") (L219-297)
//              (ref builds elem2 with addTransform → script contains transformTimeline;
//               DanQing asserts the resulting containsTransform() flag.)
TEST(RenderScheduleScriptTest, containsTransform)
{
    RenderSchedule::Script s;
    RenderSchedule::ModelTimelineProps mt;
    mt.modelId = "0x1";
    RenderSchedule::TransformEntryProps te;
    te.time = 0.0;
    mt.transformTimeline = {te};
    s.modelTimelines = {mt};

    EXPECT_TRUE(s.containsTransform());
    EXPECT_FALSE(s.containsModelClipping());
}

// Ported from: itwinjs-core core/common/src/test/RenderSchedule.test.ts
//              describe("ScriptBuilder") it("produces expected JSON") (L266-275)
//              (ref builds elem3 with addCuttingPlane → script contains cuttingPlaneTimeline;
//               DanQing asserts the resulting containsModelClipping() flag.)
TEST(RenderScheduleScriptTest, containsModelClipping)
{
    RenderSchedule::Script s;
    RenderSchedule::ModelTimelineProps mt;
    mt.modelId = "0x1";
    RenderSchedule::CuttingPlaneEntryProps ce;
    ce.time = 0.0;
    mt.cuttingPlaneTimeline = {ce};
    s.modelTimelines = {mt};

    EXPECT_FALSE(s.containsTransform());
    EXPECT_TRUE(s.containsModelClipping());
}

// Ported from: itwinjs-core core/common/src/test/RenderSchedule.test.ts
//              describe("Script") it("considers the same model Ids equal...") (L486-493)
//              (ref round-trips Script.fromJSON([modelTimelines]) and re-serializes;
//               DanQing exercises toJSON after fromJSON.)
TEST(RenderScheduleScriptTest, Roundtrip)
{
    RenderSchedule::ScriptProps props;
    RenderSchedule::ModelTimelineProps mt;
    mt.modelId = "0x1";
    props = {mt};

    auto s = RenderSchedule::Script::fromJSON(props);
    ASSERT_TRUE(s.has_value());
    auto j = s->toJSON();
    EXPECT_EQ(j.size(), 1u);
    EXPECT_EQ(j[0].modelId, "0x1");
}

// Ported from: itwinjs-core core/common/src/test/RenderSchedule.test.ts
//              describe("RenderSchedule") it("interpolates transforms") (L18-32 — interpolation: 2 = linear, 1 = step)
//              + describe("VisibilityEntry") it("compares for equality") (L320-321 — interpolation 1 vs 2)
TEST(RenderScheduleInterpolationTest, Values)
{
    // Verbatim ref assertion values (RS.Interpolation.Step = 1, Linear = 2):
    EXPECT_EQ(static_cast<int>(RenderSchedule::Interpolation::Step), 1);
    EXPECT_EQ(static_cast<int>(RenderSchedule::Interpolation::Linear), 2);
}

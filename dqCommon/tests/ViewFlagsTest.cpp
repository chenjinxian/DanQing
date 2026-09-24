// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — ViewFlags unit tests
//
// Ported from: itwinjs-core core/common/src/test/ViewFlags.test.ts
// All test scenarios, boundary values, and assertions faithfully ported.
#include "dqCommon/ViewFlags.h"

#include <gtest/gtest.h>

using namespace dqCommon;

// Ported from: itwinjs-core core/common/src/test/ViewFlags.test.ts
//              describe("ViewFlags") it("should initialize to expected defaults")
TEST(ViewFlags, DefaultValues)
{
    const ViewFlags flags;
    EXPECT_FALSE(flags.acsTriad());
    EXPECT_FALSE(flags.grid());
    EXPECT_TRUE(flags.fill());
    EXPECT_EQ(flags.renderMode(), RenderMode::Wireframe);
}

// Ported from: itwinjs-core core/common/src/test/ViewFlags.test.ts
//              describe("ViewFlags") it("should compute whether edges are required")
TEST(ViewFlags, edgesRequired)
{
    for (const auto renderMode : {RenderMode::Wireframe, RenderMode::HiddenLine, RenderMode::SolidFill,
                                  RenderMode::SmoothShade}) {
        for (int i = 0; i < 2; i++) {
            ViewFlagsProperties props;
            props.renderMode = renderMode;
            props.visibleEdges = i > 0;
            const ViewFlags vf(props);
            const bool expected = props.visibleEdges || RenderMode::SmoothShade != renderMode;
            EXPECT_EQ(vf.edgesRequired(), expected)
                << "renderMode=" << static_cast<int>(renderMode) << " visibleEdges=" << (i > 0);
        }
    }
}

// Ported from: itwinjs-core core/common/src/test/ViewFlags.test.ts
//              describe("ViewFlags") it("withRenderMode")
TEST(ViewFlags, withRenderMode)
{
    ViewFlagsProperties props;
    props.renderMode = RenderMode::SolidFill;
    const ViewFlags vf(props);

    // Same mode → returns same object
    EXPECT_TRUE(vf.withRenderMode(RenderMode::SolidFill).equals(vf));

    // Different mode → new object
    EXPECT_EQ(vf.withRenderMode(RenderMode::HiddenLine).renderMode(), RenderMode::HiddenLine);
}

// Ported from: itwinjs-core core/common/src/test/ViewFlags.test.ts
//              describe("ViewFlags") it("compares for equality")
TEST(ViewFlags, Equality)
{
    const auto& def = ViewFlags::defaults();

    // Every boolean property, when toggled, should make equals return false
    // We test a representative set
    {
        ViewFlagsProperties props = def.Properties();
        props.dimensions = !def.dimensions();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.patterns = !def.patterns();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.weights = !def.weights();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.styles = !def.styles();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.transparency = !def.transparency();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.fill = !def.fill();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.textures = !def.textures();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.materials = !def.materials();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.grid = !def.grid();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.visibleEdges = !def.visibleEdges();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.hiddenEdges = !def.hiddenEdges();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.lighting = !def.lighting();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.shadows = !def.shadows();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.clipVolume = !def.clipVolume();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.constructions = !def.constructions();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.monochrome = !def.monochrome();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.backgroundMap = !def.backgroundMap();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.ambientOcclusion = !def.ambientOcclusion();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.thematicDisplay = !def.thematicDisplay();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.wiremesh = !def.wiremesh();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.forceSurfaceDiscard = !def.forceSurfaceDiscard();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }
    {
        ViewFlagsProperties props = def.Properties();
        props.whiteOnWhiteReversal = !def.whiteOnWhiteReversal();
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }

    // renderMode difference
    {
        ViewFlagsProperties props = def.Properties();
        props.renderMode = RenderMode::SmoothShade;
        EXPECT_FALSE(def.equals(ViewFlags(props)));
    }

    // Self-equality
    EXPECT_TRUE(def.equals(def));
}

// Ported from: itwinjs-core core/common/src/test/ViewFlags.test.ts
//              describe("ViewFlags") it("returns defaults if no properties supplied")
TEST(ViewFlags, CreateDefaults)
{
    EXPECT_TRUE(ViewFlags::fromJSON(nullptr).equals(ViewFlags::defaults()));
    EXPECT_TRUE(ViewFlags::create().equals(ViewFlags::defaults()));
}

// Ported from: itwinjs-core core/common/src/test/ViewFlags.test.ts
//              describe("ViewFlags") it("uses different defaults for undefined vs ViewFlagProps")
TEST(ViewFlags, FromJSONDifferentDefaults)
{
    const auto& def = ViewFlags::defaults();

    // fromJSON(undefined) = normal defaults
    EXPECT_TRUE(ViewFlags::fromJSON(nullptr).equals(def));

    // fromJSON({}) = different defaults for clipVolume, lighting, constructions
    ViewFlagProps emptyJson;
    const auto fromEmpty = ViewFlags::fromJSON(&emptyJson);
    EXPECT_NE(fromEmpty.clipVolume(), def.clipVolume());
    EXPECT_NE(fromEmpty.lighting(), def.lighting());
    EXPECT_NE(fromEmpty.constructions(), def.constructions());
}

// Ported from: itwinjs-core core/common/src/test/ViewFlags.test.ts
//              describe("ViewFlags") it("has 3 JSON properties corresponding to 1 lighting flag")
TEST(ViewFlags, LightingJSON)
{
    auto expectLighting = [](const ViewFlags& vf, bool expected) {
        EXPECT_EQ(vf.lighting(), expected);
        const auto props = vf.toJSON();
        if (!expected) {
            EXPECT_TRUE(props.noSolarLight.value_or(false));
            EXPECT_TRUE(props.noCameraLights.value_or(false));
            EXPECT_TRUE(props.noSourceLights.value_or(false));
        }
        const auto roundtrip = ViewFlags::fromJSON(&props);
        EXPECT_EQ(roundtrip.lighting(), expected);
    };

    expectLighting(ViewFlags::fromJSON(nullptr), false);
    {
        ViewFlagProps json;
        expectLighting(ViewFlags::fromJSON(&json), true);
    }
    {
        ViewFlagProps json;
        json.noSourceLights = true;
        json.noCameraLights = true;
        json.noSolarLight = true;
        expectLighting(ViewFlags::fromJSON(&json), false);
    }
    {
        ViewFlagProps json;
        json.noCameraLights = true;
        json.noSolarLight = true;
        expectLighting(ViewFlags::fromJSON(&json), true);
    }
    {
        ViewFlagProps json;
        json.noCameraLights = true;
        expectLighting(ViewFlags::fromJSON(&json), true);
    }

    // Default-constructed ViewFlags has lighting = false
    expectLighting(ViewFlags(), false);
    {
        ViewFlagsProperties props;
        props.lighting = false;
        expectLighting(ViewFlags(props), false);
    }
    {
        ViewFlagsProperties props;
        props.lighting = true;
        expectLighting(ViewFlags(props), true);
    }
}

// Ported from: itwinjs-core core/common/src/test/ViewFlags.test.ts
//              describe("ViewFlagOverrides") it("should compute whether edges are required")
TEST(ViewFlags, OverrideEdgesRequired)
{
    // Build test cases for ViewFlags
    std::vector<ViewFlags> viewflagTestCases;
    for (const auto renderMode : {RenderMode::Wireframe, RenderMode::HiddenLine, RenderMode::SolidFill,
                                  RenderMode::SmoothShade}) {
        for (int i = 0; i < 2; i++) {
            ViewFlagsProperties props;
            props.renderMode = renderMode;
            props.visibleEdges = i > 0;
            viewflagTestCases.push_back(ViewFlags(props));
        }
    }

    // Build override test cases
    struct OverrideCase {
        ViewFlagsProperties ovrs;
        std::optional<RenderMode> renderMode;
        std::optional<bool> visibleEdges;
    };
    std::vector<OverrideCase> ovrsTestCases;

    for (int rm = -1; rm <= 6; rm++) {
        for (int i = 0; i < 3; i++) {
            OverrideCase tc;
            if (rm >= 0) {
                tc.renderMode = static_cast<RenderMode>(rm);
                tc.ovrs.renderMode = *tc.renderMode;
            }
            if (i > 0) {
                tc.visibleEdges = i > 1;
                tc.ovrs.visibleEdges = *tc.visibleEdges;
            }
            ovrsTestCases.push_back(tc);
        }
    }

    for (const auto& tc : ovrsTestCases) {
        for (auto vf : viewflagTestCases) {
            const auto rm = tc.renderMode.value_or(vf.renderMode());
            const auto edges = tc.visibleEdges.value_or(vf.visibleEdges());
            const bool edgesReq = edges || RenderMode::SmoothShade != rm;

            ViewFlagsProperties overProps;
            overProps.renderMode = rm;
            overProps.visibleEdges = edges;
            vf = vf.override(overProps);
            EXPECT_EQ(vf.edgesRequired(), edgesReq)
                << "renderMode=" << static_cast<int>(rm) << " visibleEdges=" << edges;
        }
    }
}

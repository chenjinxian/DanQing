// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/test/render/primitives/DisplayParams.test.ts
// DanQing dqRender — DisplayParams factory + equality tests.
//
// 保真依据（CLAUDE.md §5）：逐 TEST 移植 DisplayParams.test.ts。场景/断言全部来自参考；
// 仅适配类型（TS class→C++ class、ColorDef.withAlpha/getTransparency、enum class 命名）。
#include <gtest/gtest.h>

#include "render/DisplayParams.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/GraphicParams.h>
#include <dqCommon/LinePixels.h>

using namespace dqRender;
using dqCommon::ColorDef;
using dqCommon::GraphicParams;
using dqCommon::LinePixels;

// Ported from: DisplayParams.test.ts describe("DisplayParams creation tests") — "should create mesh DisplayParams..."
TEST(DisplayParamsTest, CreateForMeshIsMesh) {
    GraphicParams gf;
    DisplayParams dp = DisplayParams::createForMesh(gf, false);
    EXPECT_EQ(dp.type(), DisplayParams::Type::Mesh);
}

// Ported from: DisplayParams.test.ts "should create linear DisplayParams..."
TEST(DisplayParamsTest, CreateForLinearIsLinear) {
    GraphicParams gf;
    DisplayParams dp = DisplayParams::createForLinear(gf);
    EXPECT_EQ(dp.type(), DisplayParams::Type::Linear);
}

// Ported from: DisplayParams.test.ts "should create text DisplayParams..."
TEST(DisplayParamsTest, CreateForTextIsText) {
    GraphicParams gf;
    DisplayParams dp = DisplayParams::createForText(gf);
    EXPECT_EQ(dp.type(), DisplayParams::Type::Text);
}

// Ported from: DisplayParams.test.ts "two DisplayParams created from two default GraphicParams should be equal"
TEST(DisplayParamsTest, DefaultsAreEqual) {
    GraphicParams gf0, gf1;
    EXPECT_TRUE(DisplayParams::createForMesh(gf0, false).equals(DisplayParams::createForMesh(gf1, false)));
    EXPECT_TRUE(DisplayParams::createForText(gf0).equals(DisplayParams::createForText(gf1)));
    EXPECT_TRUE(DisplayParams::createForLinear(gf0).equals(DisplayParams::createForLinear(gf1)));
}

// Ported from: DisplayParams.test.ts "two DisplayParams created with different colors should be non-equal"
TEST(DisplayParamsTest, DifferentColorsNotEqual) {
    GraphicParams gf0, gf1;
    gf0.lineColor = ColorDef::white;
    gf1.lineColor = ColorDef::black;
    EXPECT_FALSE(DisplayParams::createForMesh(gf0, false).equals(DisplayParams::createForMesh(gf1, false)));
    EXPECT_FALSE(DisplayParams::createForText(gf0).equals(DisplayParams::createForText(gf1)));
    EXPECT_FALSE(DisplayParams::createForLinear(gf0).equals(DisplayParams::createForLinear(gf1)));
}

// Ported from: DisplayParams.test.ts "two DisplayParams created with different colors (same alpha) should be
// equal if merge-comparing". compareForMerge only tests whether colors have/lack transparency, not the
// color values themselves — so same-alpha different-hue colors merge-compare equal.
TEST(DisplayParamsTest, DifferentColorsSameAlphaMergeEqual) {
    ColorDef cd0 = ColorDef::white.withAlpha(64);
    ColorDef cd1 = ColorDef::black.withAlpha(64);
    GraphicParams gf0, gf1;
    gf0.lineColor = cd0;
    gf1.lineColor = cd1;
    EXPECT_TRUE(DisplayParams::createForMesh(gf0, false).equals(
        DisplayParams::createForMesh(gf1, false), ComparePurpose::Merge));
}

// Ported from: DisplayParams.test.ts "two DisplayParams created with different types should be non-equal"
TEST(DisplayParamsTest, DifferentTypesNotEqual) {
    GraphicParams gf;
    EXPECT_FALSE(DisplayParams::createForLinear(gf).equals(DisplayParams::createForMesh(gf, false)));
}

// Ported from: DisplayParams.test.ts "two DisplayParams created with different symbology should be non-equal"
TEST(DisplayParamsTest, DifferentSymbologyNotEqual) {
    GraphicParams gf0 = GraphicParams::fromSymbology(ColorDef::white, ColorDef::black, 8, LinePixels::Solid);
    GraphicParams gf1 = GraphicParams::fromSymbology(ColorDef::white, ColorDef::black, 3, LinePixels::HiddenLine);
    EXPECT_FALSE(DisplayParams::createForMesh(gf0, false).equals(DisplayParams::createForMesh(gf1, false)));
}

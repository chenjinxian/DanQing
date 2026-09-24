// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — RgbColor unit tests
//
// Ported from: itwinjs-core core/common/src/test/RgbColor.test.ts
// All test scenarios, boundary values, and assertions faithfully ported.
#include "dqCommon/RgbColor.h"

#include <gtest/gtest.h>

using namespace dqCommon;

// Ported from: itwinjs-core core/common/src/test/RgbColor.test.ts
//              describe("RgbColor") it("converts to hex string")
TEST(RgbColor, toHexString)
{
    EXPECT_EQ(RgbColor(0, 255, 127).toHexString(), "#00ff7f");
    EXPECT_EQ(RgbColor(255, 254, 1).toHexString(), "#fffe01");
    EXPECT_EQ(RgbColor(15, 32, 138).toHexString(), "#0f208a");
}

// Ported from: itwinjs-core core/common/src/test/RgbColor.test.ts
//              describe("RgbColor") it("should convert from ColorDef")
TEST(RgbColor, fromColorDef)
{
    const auto colorDef = ColorDef::from(100, 200, 50);
    const auto rgb = RgbColor::fromColorDef(colorDef);
    EXPECT_EQ(rgb.r, 100);
    EXPECT_EQ(rgb.g, 200);
    EXPECT_EQ(rgb.b, 50);
}

// Ported from: itwinjs-core core/common/src/test/RgbColor.test.ts
//              describe("RgbColor") it("should convert to ColorDef")
TEST(RgbColor, toColorDef)
{
    const auto rgb = RgbColor(100, 200, 50);
    const auto colorDef = rgb.toColorDef();
    const auto c = colorDef.getColors();
    EXPECT_EQ(c.r, 100);
    EXPECT_EQ(c.g, 200);
    EXPECT_EQ(c.b, 50);
    EXPECT_EQ(c.t, 0);  // default transparency
}

// Ported from: itwinjs-core core/common/src/test/RgbColor.test.ts
//              describe("RgbColor") it("should compare equality")
TEST(RgbColor, Equality)
{
    const auto a = RgbColor(10, 20, 30);
    const auto b = RgbColor(10, 20, 30);
    const auto c = RgbColor(10, 20, 31);
    EXPECT_TRUE(a.equals(b));
    EXPECT_FALSE(a.equals(c));
}

// Ported from: itwinjs-core core/common/src/test/RgbColor.test.ts
//              describe("RgbColor") it("should compare")
TEST(RgbColor, CompareTo)
{
    const auto a = RgbColor(10, 20, 30);
    const auto b = RgbColor(10, 20, 30);
    const auto c = RgbColor(11, 20, 30);
    const auto d = RgbColor(10, 21, 30);
    EXPECT_EQ(a.compareTo(b), 0);
    EXPECT_LT(a.compareTo(c), 0);
    EXPECT_GT(c.compareTo(a), 0);
    EXPECT_LT(a.compareTo(d), 0);
}

// Ported from: itwinjs-core core/common/src/test/RgbColor.test.ts
//              describe("RgbColor") it("should create from JSON")
TEST(RgbColor, fromJSON)
{
    const RgbColorProps json = {100, 150, 200};
    const auto rgb = RgbColor::fromJSON(&json);
    EXPECT_EQ(rgb.r, 100);
    EXPECT_EQ(rgb.g, 150);
    EXPECT_EQ(rgb.b, 200);

    // null → white
    const auto white = RgbColor::fromJSON(nullptr);
    EXPECT_EQ(white.r, 255);
    EXPECT_EQ(white.g, 255);
    EXPECT_EQ(white.b, 255);
}

// Ported from: itwinjs-core core/common/src/test/RgbColor.test.ts
//              describe("RgbColor") it("should clamp values")
TEST(RgbColor, Clamping)
{
    // Values outside [0,255] should be clamped
    const auto rgb = RgbColor(-10, 300, 128);
    EXPECT_EQ(rgb.r, 0);
    EXPECT_EQ(rgb.g, 255);
    EXPECT_EQ(rgb.b, 128);
}

// AmbientOcclusionTest — AmbientOcclusion.Settings 数据面锁（M-T T-a）。
//
// Ported from: itwinjs-core core/common/src/test/AmbientOcclusion.test.ts
//              （describe "AmbientOcclusion.Settings" 三例 1:1——toJSON 默认
//               省略 / fromJSON 默认值 / 全字段 round-trip）。
#include <gtest/gtest.h>

#include <dqCommon/AmbientOcclusion.h>

using namespace dqCommon;

// Ported from: AmbientOcclusion.test.ts — "toJSON() should return defaults as
//              undefined"（省略语义：值==默认的字段不写出）。
TEST(AmbientOcclusionSettingsTest, ToJsonOmitsDefaults)
{
    auto const props = AmbientOcclusion::Settings::fromJSON().toJSON();
    EXPECT_FALSE(props.bias.has_value());
    EXPECT_FALSE(props.blurDelta.has_value());
    EXPECT_FALSE(props.blurSigma.has_value());
    EXPECT_FALSE(props.blurTexelStepSize.has_value());
    EXPECT_FALSE(props.intensity.has_value());
    EXPECT_FALSE(props.maxDistance.has_value());
    EXPECT_FALSE(props.texelStepSize.has_value());
    EXPECT_FALSE(props.zLengthCap.has_value());
}

// Ported from: AmbientOcclusion.test.ts — "toJSON() should return proper
//              default values"（fromJSON 缺席=默认值——**代码值钉死**：
//  intensity=1.0/texelStepSize=1[参考注释文档写 2.0/1.95 系陈旧——
//  AmbientOcclusion.ts:60-61 的 _defaultIntensity/_defaultTexelStepSize
//  代码为准]）。
TEST(AmbientOcclusionSettingsTest, FromJsonDefaultsMatchCodeDefaults)
{
    auto const s = AmbientOcclusion::Settings::fromJSON();
    auto const& d = AmbientOcclusion::Settings::defaults();
    EXPECT_DOUBLE_EQ(s.bias, d.bias);
    EXPECT_DOUBLE_EQ(s.bias, 0.25);
    EXPECT_DOUBLE_EQ(s.blurDelta, d.blurDelta);
    EXPECT_DOUBLE_EQ(s.blurSigma, d.blurSigma);
    EXPECT_DOUBLE_EQ(s.blurSigma, 2.0);
    EXPECT_DOUBLE_EQ(s.blurTexelStepSize, d.blurTexelStepSize);
    EXPECT_DOUBLE_EQ(s.intensity, d.intensity);
    EXPECT_DOUBLE_EQ(s.intensity, 1.0);
    EXPECT_DOUBLE_EQ(s.maxDistance, d.maxDistance);
    EXPECT_DOUBLE_EQ(s.maxDistance, 10000.0);
    EXPECT_DOUBLE_EQ(s.texelStepSize, d.texelStepSize);
    EXPECT_DOUBLE_EQ(s.texelStepSize, 1.0);
    EXPECT_DOUBLE_EQ(s.zLengthCap, d.zLengthCap);
    EXPECT_DOUBLE_EQ(s.zLengthCap, 0.0025);
}

// Ported from: AmbientOcclusion.test.ts — "should round trip proper values"
//              （全字段非默认→fromJSON 逐字段就位 + toJSON 回写逐项同值）。
TEST(AmbientOcclusionSettingsTest, RoundTripProperValues)
{
    AmbientOcclusion::Props props;
    props.bias = 0.1;
    props.zLengthCap = 0.1;
    props.maxDistance = 5000.0;
    props.intensity = 5.0;
    props.texelStepSize = 2.0;
    props.blurDelta = 1.5;
    props.blurSigma = 2.5;
    props.blurTexelStepSize = 2.0;

    auto const s = AmbientOcclusion::Settings::fromJSON(&props);
    EXPECT_DOUBLE_EQ(s.bias, *props.bias);
    EXPECT_DOUBLE_EQ(s.blurDelta, *props.blurDelta);
    EXPECT_DOUBLE_EQ(s.blurSigma, *props.blurSigma);
    EXPECT_DOUBLE_EQ(s.blurTexelStepSize, *props.blurTexelStepSize);
    EXPECT_DOUBLE_EQ(s.intensity, *props.intensity);
    EXPECT_DOUBLE_EQ(s.maxDistance, *props.maxDistance);
    EXPECT_DOUBLE_EQ(s.texelStepSize, *props.texelStepSize);
    EXPECT_DOUBLE_EQ(s.zLengthCap, *props.zLengthCap);

    auto const out = s.toJSON();
    ASSERT_TRUE(out.bias.has_value());
    EXPECT_DOUBLE_EQ(*out.bias, *props.bias);
    ASSERT_TRUE(out.blurDelta.has_value());
    EXPECT_DOUBLE_EQ(*out.blurDelta, *props.blurDelta);
    ASSERT_TRUE(out.blurSigma.has_value());
    EXPECT_DOUBLE_EQ(*out.blurSigma, *props.blurSigma);
    ASSERT_TRUE(out.blurTexelStepSize.has_value());
    EXPECT_DOUBLE_EQ(*out.blurTexelStepSize, *props.blurTexelStepSize);
    ASSERT_TRUE(out.intensity.has_value());
    EXPECT_DOUBLE_EQ(*out.intensity, *props.intensity);
    ASSERT_TRUE(out.maxDistance.has_value());
    EXPECT_DOUBLE_EQ(*out.maxDistance, *props.maxDistance);
    ASSERT_TRUE(out.texelStepSize.has_value());
    EXPECT_DOUBLE_EQ(*out.texelStepSize, *props.texelStepSize);
    ASSERT_TRUE(out.zLengthCap.has_value());
    EXPECT_DOUBLE_EQ(*out.zLengthCap, *props.zLengthCap);
}

// SPDX-License-Identifier: Apache-2.0
// dqCommon tests — Solar calculations
// Ported from: itwinjs-core core/common/src/test/SolarCalculate.test.ts
//              describe("Solar Calculations") / it("should compare as expected")
// C++ 适配：JS Date 字符串 → Unix epoch 毫秒常量（Python datetime 预算，
// 容差域内等价——参考断言 az/el ±0.05°、日出日落 ±61s）。

#include <gtest/gtest.h>

#include <dqCommon/Cartographic.h>
#include <dqCommon/SolarCalculate.h>

#include <cmath>

namespace {

void testSolarAngleCalculation(dqCommon::Cartographic const& location, double dateMs,
                               double expectedAzimuth, double expectedElevation,
                               double expectedSunriseMs = -1.0, double expectedSunsetMs = -1.0)
{
    auto const azEl = dqCommon::calculateSolarAngles(dateMs, location);
    EXPECT_LT(std::abs(azEl.azimuth - expectedAzimuth), 0.05) << "azimuth mismatch";
    EXPECT_LT(std::abs(azEl.elevation - expectedElevation), 0.05) << "elevation mismatch";
    if (expectedSunriseMs >= 0.0) {
        double const sunrise = dqCommon::calculateSunriseOrSunset(dateMs, location, true);
        EXPECT_LT(std::abs(expectedSunriseMs - sunrise), 61.0 * 1000.0) << "sunrise mismatch";
    }
    if (expectedSunsetMs >= 0.0) {
        double const sunset = dqCommon::calculateSunriseOrSunset(dateMs, location, false);
        EXPECT_LT(std::abs(expectedSunsetMs - sunset), 61.0 * 1000.0) << "sunset mismatch";
    }
}

}  // namespace

// Ported from: itwinjs-core SolarCalculate.test.ts
//              it("should compare as expected")
TEST(SolarCalculateTest, ComparesAsExpected)
{
    auto const philadelphia =
        dqCommon::Cartographic::fromDegrees(-75.17035, 39.954927, 0.0);
    // May 03 2019 12:00:00 GMT -0500 → 2019-05-03T17:00:00Z
    testSolarAngleCalculation(philadelphia, 1556902800000.0, 181.42, 65.78,
                              1556877540000.0,  // 04:59 GMT-0500
                              1556927820000.0); // 18:57 GMT-0500
    // Sep 03 2019 12:00:00 GMT -0500 → 2019-09-03T17:00:00Z
    testSolarAngleCalculation(philadelphia, 1567530000000.0, 179.96, 57.53,
                              1567506600000.0,  // 05:30 GMT-0500
                              1567553340000.0); // 18:29 GMT-0500
    // Feb 03 2019 12:00:00 GMT -0500 → 2019-02-03T17:00:00Z
    testSolarAngleCalculation(philadelphia, 1549213200000.0, 175.84, 33.51,
                              1549195620000.0,  // 07:07 GMT-0500
                              1549232520000.0); // 17:22 GMT-0500

    auto const algeria = dqCommon::Cartographic::fromDegrees(2.54882812, 27.761329, 0.0);
    testSolarAngleCalculation(algeria, 1556884800000.0, 194.96, 77.53);  // May 03 12:00 GMT+0000
    testSolarAngleCalculation(algeria, 1567512000000.0, 187.66, 69.64);  // Sep 03 12:00 GMT+0000
    testSolarAngleCalculation(algeria, 1549195200000.0, 178.76, 45.72);  // Feb 03 12:00 GMT+0000

    auto const melbourne = dqCommon::Cartographic::fromDegrees(145.371093, -37.8575, 0.0);
    // May 03 2019 12:00:00 GMT +1000 → 2019-05-03T02:00:00Z
    testSolarAngleCalculation(melbourne, 1556848800000.0, 4.64, 36.49,
                              1556830860000.0,  // 07:01 GMT+1000 (May 02 21:01Z)
                              1556868600000.0); // 17:30 GMT+1000
    // Sep 03 2019 12:00:00 GMT +1000 → 2019-09-03T02:00:00Z
    testSolarAngleCalculation(melbourne, 1567476000000.0, 6.27, 44.26,
                              1567456680000.0,  // 06:38 GMT+1000 (Sep 02 20:38Z)
                              1567497480000.0); // 17:58 GMT+1000
    // Feb 03 2019 12:00:00 GMT +1000 → 2019-02-03T02:00:00Z
    testSolarAngleCalculation(melbourne, 1549159200000.0, 20.69, 67.6,
                              1549136040000.0,  // 05:34 GMT+1000 (Feb 02 19:34Z)
                              1549186200000.0); // 19:30 GMT+1000
}

// Ported from: itwinjs-core SolarCalculate.ts calculateSolarDirection
//              (:181-186——角度→方向向量的组合面；SolarCalculate.test.ts 未
//              直接断言方向向量，此处按参考公式补链锁）。
TEST(SolarCalculateTest, SolarDirectionFromAnglesMatchesAngleComposition)
{
    // azimuth 0°/elevation 90°（太阳在天顶）→ 方向 (0,0,-1)。
    auto const zenith = dqCommon::calculateSolarDirectionFromAngles(0.0, 90.0);
    EXPECT_NEAR(zenith.x, 0.0, 1e-10);
    EXPECT_NEAR(zenith.y, 0.0, 1e-10);
    EXPECT_NEAR(zenith.z, -1.0, 1e-10);

    // azimuth 180°/elevation 0° → 方向 (+1, 0, 0)（-sin(180°)=0... 核公式：
    // x=-sin(az)cos(el) → -0*1=0？sin(180°)=0 → x=0；y=-cos(180°)cos(0)=+1；
    // z=-sin(0)=0 → (0,1,0)）。
    auto const horizon = dqCommon::calculateSolarDirectionFromAngles(180.0, 0.0);
    EXPECT_NEAR(horizon.x, 0.0, 1e-10);
    EXPECT_NEAR(horizon.y, 1.0, 1e-10);
    EXPECT_NEAR(horizon.z, 0.0, 1e-10);
}

// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Solar calculation helpers
// Ported from: itwinjs-core core/common/src/SolarCalculate.ts
//
// NOAA solcalc 移植（https://www.esrl.noaa.gov/gmd/grad/solcalc/）。
// M-U U-c：全量归位（M-S 面仅 calculateSolarDirectionFromAngles——
// ThematicDisplayEditor defaultSettings 消费面；本件补
// calculateSolarAngles/calculateSolarDirection[date+location]/
// sunrise-sunset 族——DTA Timeline.ts SolarTimelineProvider 消费面）。
// C++ 适配：JS Date → Unix epoch 毫秒 double（solar.timePoint 数据面
// 同单位——DisplayStyleSettings lights.solar.timePoint 的 ms 语义）。
#pragma once

#include "Cartographic.h"
#include "Export.h"
#include "DqCommon.h"

#include <dqGeom/Angle.h>
#include <dqGeom/Vector3d.h>

#include <cmath>

BEGIN_DQ_COMMON_NAMESPACE

namespace detail {

// Code below loosely translated from https://www.esrl.noaa.gov/gmd/grad/solcalc/
// Ported from: itwinjs-core SolarCalculate.ts calcTimeJulianCent (:15-18)
inline double calcTimeJulianCent(double jd) noexcept
{
    return (jd - 2451545.0) / 36525.0;
}

// Ported from: itwinjs-core SolarCalculate.ts radToDeg (:20-22)
inline double radToDeg(double angleRad) noexcept
{
    return 180.0 * angleRad / M_PI;
}

// Ported from: itwinjs-core SolarCalculate.ts degToRad (:24-26)
inline double degToRad(double angleDeg) noexcept
{
    return M_PI * angleDeg / 180.0;
}

// Ported from: itwinjs-core SolarCalculate.ts calcGeomMeanLongSun (:28-37)
inline double calcGeomMeanLongSun(double t) noexcept
{
    double L0 = 280.46646 + t * (36000.76983 + t * 0.0003032);
    while (L0 > 360.0)
        L0 -= 360.0;
    while (L0 < 0.0)
        L0 += 360.0;
    return L0;  // in degrees
}

// Ported from: itwinjs-core SolarCalculate.ts calcGeomMeanAnomalySun (:39-42)
inline double calcGeomMeanAnomalySun(double t) noexcept
{
    return 357.52911 + t * (35999.05029 - 0.0001537 * t);  // in degrees
}

// Ported from: itwinjs-core SolarCalculate.ts calcEccentricityEarthOrbit (:44-47)
inline double calcEccentricityEarthOrbit(double t) noexcept
{
    return 0.016708634 - t * (0.000042037 + 0.0000001267 * t);  // unitless
}

// Ported from: itwinjs-core SolarCalculate.ts calcSunEqOfCenter (:49-57)
inline double calcSunEqOfCenter(double t) noexcept
{
    double const m = calcGeomMeanAnomalySun(t);
    double const mrad = degToRad(m);
    double const sinm = std::sin(mrad);
    double const sin2m = std::sin(mrad + mrad);
    double const sin3m = std::sin(mrad + mrad + mrad);
    return sinm * (1.914602 - t * (0.004817 + 0.000014 * t)) +
           sin2m * (0.019993 - 0.000101 * t) + sin3m * 0.000289;  // in degrees
}

// Ported from: itwinjs-core SolarCalculate.ts calcSunTrueLong (:59-63)
inline double calcSunTrueLong(double t) noexcept
{
    return calcGeomMeanLongSun(t) + calcSunEqOfCenter(t);  // in degrees
}

// Ported from: itwinjs-core SolarCalculate.ts calcSunApparentLong (:65-70)
inline double calcSunApparentLong(double t) noexcept
{
    double const o = calcSunTrueLong(t);
    double const omega = 125.04 - 1934.136 * t;
    return o - 0.00569 - 0.00478 * std::sin(degToRad(omega));  // in degrees
}

// Ported from: itwinjs-core SolarCalculate.ts calcMeanObliquityOfEcliptic (:73-78)
inline double calcMeanObliquityOfEcliptic(double t) noexcept
{
    double const seconds = 21.448 - t * (46.8150 + t * (0.00059 - t * 0.001813));
    return 23.0 + (26.0 + (seconds / 60.0)) / 60.0;  // in degrees
}

// Ported from: itwinjs-core SolarCalculate.ts calcObliquityCorrection (:80-85)
inline double calcObliquityCorrection(double t) noexcept
{
    double const e0 = calcMeanObliquityOfEcliptic(t);
    double const omega = 125.04 - 1934.136 * t;
    return e0 + 0.00256 * std::cos(degToRad(omega));  // in degrees
}

// Ported from: itwinjs-core SolarCalculate.ts calcSunDeclination (:87-94)
inline double calcSunDeclination(double t) noexcept
{
    double const e = calcObliquityCorrection(t);
    double const lambda = calcSunApparentLong(t);

    double const sint = std::sin(degToRad(e)) * std::sin(degToRad(lambda));
    return radToDeg(std::asin(sint));  // in degrees
}

// Ported from: itwinjs-core SolarCalculate.ts calcEquationOfTime (:96-112)
inline double calcEquationOfTime(double t) noexcept
{
    double const epsilon = calcObliquityCorrection(t);
    double const l0 = calcGeomMeanLongSun(t);
    double const e = calcEccentricityEarthOrbit(t);
    double const m = calcGeomMeanAnomalySun(t);

    double y = std::tan(degToRad(epsilon) / 2.0);
    y *= y;

    double const sin2l0 = std::sin(2.0 * degToRad(l0));
    double const sinm = std::sin(degToRad(m));
    double const cos2l0 = std::cos(2.0 * degToRad(l0));
    double const sin4l0 = std::sin(4.0 * degToRad(l0));
    double const sin2m = std::sin(2.0 * degToRad(m));

    double const eTime =
        y * sin2l0 - 2.0 * e * sinm + 4.0 * e * y * sinm * cos2l0 - 0.5 * y * y * sin4l0 -
        1.25 * e * e * sin2m;
    return radToDeg(eTime) * 4.0;  // in minutes of time
}

struct AzEl {
    double azimuth;
    double elevation;
};

// Ported from: itwinjs-core SolarCalculate.ts calcAzEl (:114-160)
inline AzEl calcAzEl(double t, double localTime, double latitude, double longitude,
                     double zone) noexcept
{
    double const eqTime = calcEquationOfTime(t);
    double const theta = calcSunDeclination(t);
    double const solarTimeFix = eqTime + 4.0 * longitude - 60.0 * zone;
    double trueSolarTime = localTime + solarTimeFix;
    while (trueSolarTime > 1440.0)
        trueSolarTime -= 1440.0;

    double hourAngle = trueSolarTime / 4.0 - 180.0;
    if (hourAngle < -180.0)
        hourAngle += 360.0;

    double const haRad = degToRad(hourAngle);
    double csz = std::sin(degToRad(latitude)) * std::sin(degToRad(theta)) +
                 std::cos(degToRad(latitude)) * std::cos(degToRad(theta)) * std::cos(haRad);
    if (csz > 1.0)
        csz = 1.0;
    else if (csz < -1.0)
        csz = -1.0;
    double const zenith = radToDeg(std::acos(csz));
    double const azDenom =
        std::cos(degToRad(latitude)) * std::sin(degToRad(zenith));
    double azimuth;
    if (std::abs(azDenom) > 0.001) {
        double azRad =
            ((std::sin(degToRad(latitude)) * std::cos(degToRad(zenith))) -
             std::sin(degToRad(theta))) /
            azDenom;
        if (std::abs(azRad) > 1.0)
            azRad = azRad < 0 ? -1.0 : 1.0;
        azimuth = 180.0 - radToDeg(std::acos(azRad));
        if (hourAngle > 0.0)
            azimuth = -azimuth;
    } else {
        azimuth = latitude > 0.0 ? 180.0 : 0.0;
    }
    if (azimuth < 0.0)
        azimuth += 360.0;
    return {azimuth, 90.0 - zenith};
}

// Ported from: itwinjs-core SolarCalculate.ts calculateJulianDay (:162-164)
inline double calculateJulianDay(double dateMs) noexcept
{
    return std::floor(dateMs / 86400000.0) + 2440587.5;
}

// Ported from: itwinjs-core SolarCalculate.ts dateFromUtcMinutes (:199-206)
inline double dateFromUtcMinutes(double dateMs, double utcMinutes) noexcept
{
    double const utcHours = std::floor(utcMinutes / 60.0);
    double const ms = (dateMs - std::fmod(dateMs, 86400000.0)) +  // day 部分
                      utcHours * 3600000.0 +
                      std::floor(0.5 + utcMinutes - 60.0 * utcHours) * 60000.0;
    return ms;
}

// Ported from: itwinjs-core SolarCalculate.ts calcSunriseUtcMinutes (:208-218)
inline double calcSunriseUtcMinutes(bool rise, double lat, double longitude,
                                    double jDay) noexcept
{
    double const t = calcTimeJulianCent(jDay);
    double const eqTime = calcEquationOfTime(t);
    double const solarDec = calcSunDeclination(t);
    double const latRad = degToRad(lat);
    double const sdRad = degToRad(solarDec);
    double const hAarg = (std::cos(degToRad(90.833)) / (std::cos(latRad) * std::cos(sdRad)) -
                          std::tan(latRad) * std::tan(sdRad));
    double const hourAngle = std::acos(hAarg);
    double const delta = longitude + radToDeg(rise ? hourAngle : -hourAngle);
    return 720.0 - (4.0 * delta) - eqTime;  // in UTC minutes
}

}  // namespace detail

// Solar azimuth/elevation angles (degrees) at a given date/time and location.
// Ported from: itwinjs-core calculateSolarAngles (SolarCalculate.ts:166-179)
struct SolarAngles {
    double azimuth;
    double elevation;
};

inline SolarAngles calculateSolarAngles(double dateMs, Cartographic const& location) noexcept
{
    double const jDay = detail::calculateJulianDay(dateMs);
    double const latitude = location.getLatitudeDegrees();
    double const longitude = location.getLongitudeDegrees();
    double const utcMinutes = std::fmod(dateMs, 86400000.0) / 60000.0;
    double const zone = std::floor(0.5 + longitude / 15.0);  // date.getTimeZoneOffset mixes in DST
    double const localMinutes = utcMinutes + zone * 60.0;
    double const jTotal = jDay + utcMinutes / 1440.0;
    double const T = detail::calcTimeJulianCent(jTotal);
    auto const azEl = detail::calcAzEl(T, localMinutes, latitude, longitude, zone);
    return {azEl.azimuth, azEl.elevation};
}

// Calculate solar direction corresponding to the given azimuth and elevation
// (altitude) angles in degrees.
// Ported from: itwinjs-core calculateSolarDirectionFromAngles
// (SolarCalculate.ts:191-197)。§3.4 适配：参考的 {azimuth,elevation} 对象
// 形参 → 两 double。
inline dqGeom::Vector3d calculateSolarDirectionFromAngles(double azimuth, double elevation)
{
    const double az = dqGeom::Angle::DegreesToRadians(azimuth);
    const double el = dqGeom::Angle::DegreesToRadians(elevation);
    const double cosElevation = std::cos(el);
    const double sinElevation = std::sin(el);
    return dqGeom::Vector3d::From(
        -std::sin(az) * cosElevation, -std::cos(az) * cosElevation, -sinElevation);
}

// Solar direction based at a given date/time and cartographic location.
// Ported from: itwinjs-core calculateSolarDirection (SolarCalculate.ts:181-186)
inline dqGeom::Vector3d calculateSolarDirection(double dateMs, Cartographic const& location)
{
    auto const azEl = calculateSolarAngles(dateMs, location);
    return calculateSolarDirectionFromAngles(azEl.azimuth, azEl.elevation);
}

// Calculate solar sunrise or sunset for a given day and cartographic location.
// Returns the epoch milliseconds of the sunrise (sunrise=true) or sunset.
// Ported from: itwinjs-core calculateSunriseOrSunset (SolarCalculate.ts:223-229)
inline double calculateSunriseOrSunset(double dateMs, Cartographic const& location,
                                       bool sunrise) noexcept
{
    double const jDay = detail::calculateJulianDay(dateMs);
    double const longitude = location.getLongitudeDegrees();
    double const latitude = location.getLatitudeDegrees();
    double const utcMinutes =
        detail::calcSunriseUtcMinutes(sunrise, latitude, longitude, jDay);
    // 日落系迭代精化（参考 :228  sunset 分支二次调用——首日近似 utcMinutes
    // 推进 Julian 日）。
    return sunrise
               ? detail::dateFromUtcMinutes(dateMs, utcMinutes)
               : detail::dateFromUtcMinutes(
                     dateMs, detail::calcSunriseUtcMinutes(sunrise, latitude, longitude,
                                                           jDay + utcMinutes / 1440.0));
}

END_DQ_COMMON_NAMESPACE

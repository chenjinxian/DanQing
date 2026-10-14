// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp tests — M-V V-a CameraPaths 数据层（Keyframe/CameraPath）
//
// Authored: no reference test exists in itwinjs-core / display-test-app for
//           CameraPaths（无 .test.ts；行为锚 = CameraPaths.ts 逐行：
//           Keyframe.interpolate :67-90 / CameraPath.simplifies :160-184 /
//           getKeyframeAtTime :191-203 / getKeyframeRangeAtTime :210-227 /
//           fromJSON/toJSON :92-114/:229-246）。
//
// 覆盖面：
//  - Keyframe::Interpolate 全字段插值 + lensAngle 双 undefined 怪癖（:68-73）+
//    extents 怪癖（:75-80——this 在场 other 缺席时取 other[undefined]，与
//    lensAngle 分支取 this 不对称，参考原样锁）。
//  - CameraPath::GetKeyframeAtTime 四分支（经公开面：空→nullopt[参考 throw]/
//    早于首帧→首帧原样/中间→插值/晚于末帧→末帧原样）+ timeDifference==0 除零门。
//  - SortKeyframes 升序。
//  - Simplifies 四条件逐阈删减（插值可重建→删；location/target/lensAngle/
//    extents 任一超阈或 sameCameraType=false→留）。
//  - JSON round-trip（lensAngle=度数 number[Angle.toJSON()→degrees]；
//    undefined 字段省略[JSON.stringify 语义]；%.17g 精确守恒）。

#include <gtest/gtest.h>

#include "Gui/CameraPaths.h"

#include <dqApp/tile/DumpTileTreeProps.h>  // dumpjson

#include <cmath>
#include <string>

using namespace Gui;
using dqGeom::Angle;
using dqGeom::Point3d;
using dqGeom::Vector3d;

namespace {

constexpr double kTol = 1.0e-9;

Keyframe makePerspectiveFrame(double time, double lx, double tx, double lensDeg)
{
    Keyframe k;
    k.time = time;
    k.location = Point3d::From(lx, 0.0, 0.0);
    k.target = Point3d::From(tx, 0.0, 0.0);
    k.up = Vector3d::From(0.0, 0.0, 1.0);
    k.lensAngle = Angle::FromDegrees(lensDeg);
    k.extents = std::nullopt;
    return k;
}

Keyframe makeOrthoFrame(double time, double lx, double tx, double extentX)
{
    Keyframe k;
    k.time = time;
    k.location = Point3d::From(lx, 0.0, 0.0);
    k.target = Point3d::From(tx, 0.0, 0.0);
    k.up = Vector3d::From(0.0, 0.0, 1.0);
    k.lensAngle = std::nullopt;
    k.extents = Vector3d::From(extentX, extentX * 0.5, 1.0);
    return k;
}

dqApp::dumpjson::JsonValue parse(std::string const& text)
{
    auto doc = dqApp::dumpjson::parseJsonDocument(text);
    EXPECT_TRUE(doc.has_value());
    return doc.has_value() ? *doc : dqApp::dumpjson::JsonValue{};
}

}  // namespace

// --- Keyframe::Interpolate（CameraPaths.ts:67-90） ---

TEST(CameraPathsTest, KeyframeInterpolateBothDefinedInterpolatesAllFields)
{
    const Keyframe a = makePerspectiveFrame(1000.0, 0.0, 10.0, 60.0);
    Keyframe b = makePerspectiveFrame(3000.0, 10.0, 20.0, 90.0);
    b.up = Vector3d::From(0.0, 1.0, 0.0);

    const Keyframe mid = a.Interpolate(0.5, b);
    EXPECT_DOUBLE_EQ(mid.time, 2000.0);
    EXPECT_DOUBLE_EQ(mid.location.x, 5.0);
    EXPECT_DOUBLE_EQ(mid.target.x, 15.0);
    EXPECT_NEAR(mid.up.y, 0.5, kTol);
    ASSERT_TRUE(mid.lensAngle.has_value());
    EXPECT_NEAR(mid.lensAngle->Degrees(), 75.0, kTol);
    EXPECT_FALSE(mid.extents.has_value());
}

TEST(CameraPathsTest, KeyframeInterpolateLensAngleUndefinedFallsBackThreeWays)
{
    const Keyframe persp = makePerspectiveFrame(0.0, 0.0, 10.0, 60.0);
    const Keyframe ortho = makeOrthoFrame(0.0, 0.0, 10.0, 100.0);

    // this 缺席 → 取 other（:69-70）
    const Keyframe r1 = ortho.Interpolate(0.5, persp);
    ASSERT_TRUE(r1.lensAngle.has_value());
    EXPECT_NEAR(r1.lensAngle->Degrees(), 60.0, kTol);  // 全值，不插值

    // this 在场 + other 缺席 → 取 this（:72——lensAngle 分支的回落）
    const Keyframe r2 = persp.Interpolate(0.5, ortho);
    ASSERT_TRUE(r2.lensAngle.has_value());
    EXPECT_NEAR(r2.lensAngle->Degrees(), 60.0, kTol);

    // 双双缺席 → 缺席
    const Keyframe r3 = ortho.Interpolate(0.5, ortho);
    EXPECT_FALSE(r3.lensAngle.has_value());
}

TEST(CameraPathsTest, KeyframeInterpolateExtentsUndefinedTakesOtherSide)
{
    const Keyframe persp = makePerspectiveFrame(0.0, 0.0, 10.0, 60.0);
    const Keyframe ortho = makeOrthoFrame(0.0, 0.0, 10.0, 100.0);

    // this 缺席 → 取 other（:76-77）
    const Keyframe r1 = persp.Interpolate(0.5, ortho);
    ASSERT_TRUE(r1.extents.has_value());
    EXPECT_DOUBLE_EQ(r1.extents->x, 100.0);

    // 参考怪癖（:79）：this 在场 + other 缺席 → 取 **other**（=undefined）——
    // 与 lensAngle 分支回落 this 不对称，疑似参考复制粘贴，§0 原样锁定。
    const Keyframe r2 = ortho.Interpolate(0.5, persp);
    EXPECT_FALSE(r2.extents.has_value());

    // 双双在场 → 插值（:79 中段）
    const Keyframe o1 = makeOrthoFrame(0.0, 0.0, 10.0, 100.0);
    const Keyframe o2 = makeOrthoFrame(0.0, 0.0, 10.0, 200.0);
    const Keyframe r3 = o1.Interpolate(0.5, o2);
    ASSERT_TRUE(r3.extents.has_value());
    EXPECT_DOUBLE_EQ(r3.extents->x, 150.0);
    EXPECT_DOUBLE_EQ(r3.extents->y, 75.0);
}

// --- CameraPath::GetKeyframeAtTime（:191-203 + :210-227 四分支） ---

TEST(CameraPathsTest, GetKeyframeAtTimeEmptyReturnsNullopt)
{
    CameraPath path = CameraPath::CreateEmpty();
    // 参考 throw new Error("Selected path has no keyframes.")（:212）→ C++ nullopt
    EXPECT_FALSE(path.GetKeyframeAtTime(0.0).has_value());
}

TEST(CameraPathsTest, GetKeyframeAtTimeBeforeFirstReturnsFirstVerbatim)
{
    CameraPath path;
    path.keyframes.push_back(makePerspectiveFrame(1000.0, 0.0, 10.0, 60.0));
    path.keyframes.push_back(makePerspectiveFrame(2000.0, 20.0, 30.0, 90.0));
    // 早于首帧 → {before:首帧} 直通（:215-217——不外推）
    const auto k = path.GetKeyframeAtTime(500.0);
    ASSERT_TRUE(k.has_value());
    EXPECT_DOUBLE_EQ(k->time, 1000.0);
    EXPECT_DOUBLE_EQ(k->location.x, 0.0);
    EXPECT_NEAR(k->lensAngle->Degrees(), 60.0, kTol);
}

TEST(CameraPathsTest, GetKeyframeAtTimeMiddleInterpolates)
{
    CameraPath path;
    path.keyframes.push_back(makePerspectiveFrame(1000.0, 0.0, 10.0, 60.0));
    path.keyframes.push_back(makePerspectiveFrame(3000.0, 20.0, 30.0, 90.0));
    const auto k = path.GetKeyframeAtTime(2000.0);
    ASSERT_TRUE(k.has_value());
    EXPECT_DOUBLE_EQ(k->time, 2000.0);
    EXPECT_DOUBLE_EQ(k->location.x, 10.0);
    EXPECT_NEAR(k->lensAngle->Degrees(), 75.0, kTol);
}

TEST(CameraPathsTest, GetKeyframeAtTimeAfterLastReturnsLastVerbatim)
{
    CameraPath path;
    path.keyframes.push_back(makePerspectiveFrame(1000.0, 0.0, 10.0, 60.0));
    path.keyframes.push_back(makePerspectiveFrame(2000.0, 20.0, 30.0, 90.0));
    const auto k = path.GetKeyframeAtTime(9999.0);
    ASSERT_TRUE(k.has_value());
    EXPECT_DOUBLE_EQ(k->time, 2000.0);
    EXPECT_DOUBLE_EQ(k->location.x, 20.0);
}

TEST(CameraPathsTest, GetKeyframeAtTimeSameTimeFramesFractionZero)
{
    CameraPath path;
    path.keyframes.push_back(makePerspectiveFrame(1000.0, 0.0, 10.0, 60.0));
    path.keyframes.push_back(makePerspectiveFrame(1000.0, 20.0, 30.0, 90.0));
    // timeDifference==0 → fraction=0.0（:200 除零门）→ 取 before 原样
    const auto k = path.GetKeyframeAtTime(1000.0);
    ASSERT_TRUE(k.has_value());
    EXPECT_DOUBLE_EQ(k->location.x, 20.0);  // 时刻同戳：循环先命中第二帧为 after（:220-223）
}

TEST(CameraPathsTest, SortKeyframesOrdersByTime)
{
    CameraPath path;
    path.keyframes.push_back(makePerspectiveFrame(3000.0, 0.0, 10.0, 60.0));
    path.keyframes.push_back(makePerspectiveFrame(1000.0, 0.0, 10.0, 60.0));
    path.keyframes.push_back(makePerspectiveFrame(2000.0, 0.0, 10.0, 60.0));
    path.SortKeyframes();
    ASSERT_EQ(path.keyframes.size(), 3u);
    EXPECT_DOUBLE_EQ(path.keyframes[0].time, 1000.0);
    EXPECT_DOUBLE_EQ(path.keyframes[1].time, 2000.0);
    EXPECT_DOUBLE_EQ(path.keyframes[2].time, 3000.0);
}

// --- CameraPath::Simplifies（:160-184；stop() 实参 0.005/1°/0.01——:577） ---

TEST(CameraPathsTest, SimplifiesRemovesInterpolableMiddleFrame)
{
    // 共线等距三帧（中点可被前后插值精确重建）→ 中帧删
    CameraPath path;
    path.keyframes.push_back(makePerspectiveFrame(0.0, 0.0, 100.0, 60.0));
    path.keyframes.push_back(makePerspectiveFrame(1000.0, 1.0, 101.0, 60.0));
    path.keyframes.push_back(makePerspectiveFrame(2000.0, 2.0, 102.0, 60.0));
    path.Simplifies(0.005, Angle::FromDegrees(1.0), 0.01);
    ASSERT_EQ(path.keyframes.size(), 2u);
    EXPECT_DOUBLE_EQ(path.keyframes[0].time, 0.0);
    EXPECT_DOUBLE_EQ(path.keyframes[1].time, 2000.0);
}

TEST(CameraPathsTest, SimplifiesKeepsFrameWhenLocationDeviates)
{
    CameraPath path;
    path.keyframes.push_back(makePerspectiveFrame(0.0, 0.0, 100.0, 60.0));
    Keyframe mid = makePerspectiveFrame(1000.0, 1.0, 101.0, 60.0);
    mid.location.y = 0.02;  // 偏离插值 0.02 > distanceThreshold 0.005
    path.keyframes.push_back(mid);
    path.keyframes.push_back(makePerspectiveFrame(2000.0, 2.0, 102.0, 60.0));
    path.Simplifies(0.005, Angle::FromDegrees(1.0), 0.01);
    EXPECT_EQ(path.keyframes.size(), 3u);
}

TEST(CameraPathsTest, SimplifiesKeepsFrameWhenLensAngleDeviates)
{
    CameraPath path;
    path.keyframes.push_back(makePerspectiveFrame(0.0, 0.0, 100.0, 60.0));
    path.keyframes.push_back(makePerspectiveFrame(1000.0, 1.0, 101.0, 62.0));  // 偏 2° > 1°
    path.keyframes.push_back(makePerspectiveFrame(2000.0, 2.0, 102.0, 60.0));
    path.Simplifies(0.005, Angle::FromDegrees(1.0), 0.01);
    EXPECT_EQ(path.keyframes.size(), 3u);
}

TEST(CameraPathsTest, SimplifiesKeepsFrameWhenCameraTypeDiffers)
{
    CameraPath path;
    path.keyframes.push_back(makePerspectiveFrame(0.0, 0.0, 100.0, 60.0));
    path.keyframes.push_back(makeOrthoFrame(1000.0, 1.0, 101.0, 100.0));  // 正交夹透视
    path.keyframes.push_back(makePerspectiveFrame(2000.0, 2.0, 102.0, 60.0));
    // sameCameraType=false → 留（:168/:174 门）
    path.Simplifies(0.005, Angle::FromDegrees(1.0), 0.01);
    EXPECT_EQ(path.keyframes.size(), 3u);
}

TEST(CameraPathsTest, SimplifiesKeepsFrameWhenExtentsDeviate)
{
    CameraPath path;
    path.keyframes.push_back(makeOrthoFrame(0.0, 0.0, 100.0, 100.0));
    path.keyframes.push_back(makeOrthoFrame(1000.0, 1.0, 101.0, 100.05));  // extents 偏 0.05 > 0.01
    path.keyframes.push_back(makeOrthoFrame(2000.0, 2.0, 102.0, 100.0));
    path.Simplifies(0.005, Angle::FromDegrees(1.0), 0.01);
    EXPECT_EQ(path.keyframes.size(), 3u);
}

TEST(CameraPathsTest, SimplifiesSweepsAllRemovableInOnePass)
{
    // 5 共线帧 → while 循环 splice 不递增 i 的语义（:179-183）删至 2
    CameraPath path;
    for (int i = 0; i < 5; ++i)
        path.keyframes.push_back(makePerspectiveFrame(1000.0 * i, 1.0 * i, 100.0 + i, 60.0));
    path.Simplifies(0.005, Angle::FromDegrees(1.0), 0.01);
    ASSERT_EQ(path.keyframes.size(), 2u);
    EXPECT_DOUBLE_EQ(path.keyframes[0].time, 0.0);
    EXPECT_DOUBLE_EQ(path.keyframes[1].time, 4000.0);
}

// --- JSON 对偶（:92-114 / :229-246） ---

TEST(CameraPathsTest, KeyframeJsonRoundTripPerspective)
{
    const Keyframe src = makePerspectiveFrame(1234.5, 1.25, 10.5, 60.5);
    std::string json;
    src.ToJSON(json);
    const auto doc = parse(json);
    const Keyframe dst = Keyframe::FromJSON(&doc);
    EXPECT_DOUBLE_EQ(dst.time, src.time);
    EXPECT_DOUBLE_EQ(dst.location.x, src.location.x);
    EXPECT_DOUBLE_EQ(dst.target.x, src.target.x);
    EXPECT_DOUBLE_EQ(dst.up.z, src.up.z);
    ASSERT_TRUE(dst.lensAngle.has_value());
    EXPECT_DOUBLE_EQ(dst.lensAngle->Radians(), src.lensAngle->Radians());
    EXPECT_FALSE(dst.extents.has_value());
}

TEST(CameraPathsTest, KeyframeJsonRoundTripOrthographic)
{
    const Keyframe src = makeOrthoFrame(999.0, -3.5, 7.75, 123.456);
    std::string json;
    src.ToJSON(json);
    const auto doc = parse(json);
    const Keyframe dst = Keyframe::FromJSON(&doc);
    EXPECT_DOUBLE_EQ(dst.time, src.time);
    EXPECT_FALSE(dst.lensAngle.has_value());
    ASSERT_TRUE(dst.extents.has_value());
    EXPECT_DOUBLE_EQ(dst.extents->x, src.extents->x);
    EXPECT_DOUBLE_EQ(dst.extents->y, src.extents->y);
}

TEST(CameraPathsTest, KeyframeJsonOmitsUndefinedFields)
{
    const Keyframe src = makePerspectiveFrame(0.0, 0.0, 10.0, 60.0);
    std::string json;
    src.ToJSON(json);
    // JSON.stringify 省略 undefined 属性（:111-112）→ 文本无该键
    EXPECT_EQ(json.find("extents"), std::string::npos);
    EXPECT_NE(json.find("lensAngle"), std::string::npos);
}

TEST(CameraPathsTest, KeyframeJsonLensAngleIsDegreesNumber)
{
    Keyframe src;
    src.time = 0.0;
    src.lensAngle = Angle::FromDegrees(60.0);
    std::string json;
    src.ToJSON(json);
    // Angle.toJSON() → this.degrees（Angle.ts:156-158——度数 number 形态）。
    // 提取 "lensAngle": 后的数值——度数≈60（弧度形态会是 ≈1.047）。
    const size_t pos = json.find("\"lensAngle\":");
    ASSERT_NE(pos, std::string::npos);
    const double emitted = std::stod(json.substr(pos + 12));
    EXPECT_NEAR(emitted, 60.0, 1.0e-9);
    EXPECT_GT(std::fabs(emitted - 1.0471975512), 1.0);  // 非弧度
}

TEST(CameraPathsTest, KeyframeFromJsonNullReturnsZero)
{
    const Keyframe k = Keyframe::FromJSON(nullptr);  // 参考 :93-94 createZero
    EXPECT_DOUBLE_EQ(k.time, 0.0);
    EXPECT_DOUBLE_EQ(k.location.x, 0.0);
    EXPECT_FALSE(k.lensAngle.has_value());
    EXPECT_FALSE(k.extents.has_value());
}

TEST(CameraPathsTest, CameraPathJsonRoundTripPreservesAllFields)
{
    CameraPath path;
    path.name = "walkthrough A";
    path.duration = 2500.0;
    path.keyframes.push_back(makePerspectiveFrame(0.0, 0.0, 10.0, 60.0));
    path.keyframes.push_back(makeOrthoFrame(2500.0, 5.0, 15.0, 88.5));

    std::string json;
    path.ToJSON(json);
    const auto doc = parse(json);
    const CameraPath dst = CameraPath::FromJSON(&doc);
    EXPECT_EQ(dst.name, path.name);
    EXPECT_DOUBLE_EQ(dst.duration, path.duration);
    ASSERT_EQ(dst.keyframes.size(), 2u);
    EXPECT_TRUE(dst.keyframes[0].lensAngle.has_value());
    EXPECT_FALSE(dst.keyframes[0].extents.has_value());
    EXPECT_FALSE(dst.keyframes[1].lensAngle.has_value());
    ASSERT_TRUE(dst.keyframes[1].extents.has_value());
    EXPECT_DOUBLE_EQ(dst.keyframes[1].extents->x, 88.5);
}

TEST(CameraPathsTest, CameraPathFromJsonNullReturnsEmpty)
{
    const CameraPath path = CameraPath::FromJSON(nullptr);  // 参考 :230-231 createEmpty
    EXPECT_EQ(path.name, "");
    EXPECT_DOUBLE_EQ(path.duration, 0.0);
    EXPECT_TRUE(path.keyframes.empty());
}

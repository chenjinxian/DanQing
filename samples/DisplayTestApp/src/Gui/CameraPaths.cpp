// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Camera Paths 数据层实现
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/CameraPaths.ts
#include "CameraPaths.h"

#include <dqApp/tile/DumpTileTreeProps.h>  // dumpjson

#include <cmath>
#include <cstdio>

namespace Gui {

namespace {

// 数值 %.17g round-trip（M-O(2) I9 先例——DumpIModelConnection.cpp:661）
void appendNumber(std::string& out, double v)
{
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    out += buf;
}

// {x,y,z} 点/向量对象（Point3d.toJSON/Vector3d.toJSON 的发出形态——geometry-core
// XYZ.toJSON 为 {x,y,z} 对象）
void appendXyz(std::string& out, double x, double y, double z)
{
    out += "{\"x\":";
    appendNumber(out, x);
    out += ",\"y\":";
    appendNumber(out, y);
    out += ",\"z\":";
    appendNumber(out, z);
    out += '}';
}

// 解析 {x,y,z}（本实现唯一发出形态；geometry-core fromJSON 的数组等其余形态
// 不落——EQUIVALENCE：round-trip 消费面自产自销）
dqGeom::Point3d pointFromXyz(const dqApp::dumpjson::JsonValue* json)
{
    if (json == nullptr)
        return dqGeom::Point3d::FromZero();
    const auto* x = json->find("x");
    const auto* y = json->find("y");
    const auto* z = json->find("z");
    return dqGeom::Point3d::From(
        x != nullptr ? x->number : 0.0,
        y != nullptr ? y->number : 0.0,
        z != nullptr ? z->number : 0.0);
}

// Ported from: itwinjs-core geometry-core Angle.setFromJSON (Angle.ts:131-145——
//   number=degrees / {degrees} / {_degrees} / {radians} / {_radians})
dqGeom::Angle angleFromJson(const dqApp::dumpjson::JsonValue* json)
{
    if (json == nullptr)
        return dqGeom::Angle();
    if (json->type == dqApp::dumpjson::JsonValue::Type::Number)
        return dqGeom::Angle::FromDegrees(json->number);
    if (const auto* v = json->find("degrees"))
        return dqGeom::Angle::FromDegrees(v->number);
    if (const auto* v = json->find("_degrees"))
        return dqGeom::Angle::FromDegrees(v->number);
    if (const auto* v = json->find("radians"))
        return dqGeom::Angle::FromRadians(v->number);
    if (const auto* v = json->find("_radians"))
        return dqGeom::Angle::FromRadians(v->number);
    return dqGeom::Angle();
}

}  // namespace

// --- Keyframe（CameraPaths.ts:21-115） ---

Keyframe Keyframe::CreateZero()
{
    // :60-62
    Keyframe k;
    k.time = 0.0;
    k.location = dqGeom::Point3d::FromZero();
    k.target = dqGeom::Point3d::FromZero();
    k.up = dqGeom::Vector3d::FromZero();
    k.lensAngle = std::nullopt;
    k.extents = std::nullopt;
    return k;
}

Keyframe Keyframe::Interpolate(double fraction, const Keyframe& other) const
{
    // :67-90
    std::optional<dqGeom::Angle> lensAngleResult;
    if (!lensAngle.has_value()) {
        lensAngleResult = other.lensAngle;                        // :69-70
    } else {
        lensAngleResult = other.lensAngle.has_value()
            ? dqGeom::Angle::CreateInterpolate(*lensAngle, fraction, *other.lensAngle)
            : lensAngle;                                          // :72
    }

    std::optional<dqGeom::Vector3d> extentsResult;
    if (!extents.has_value()) {
        extentsResult = other.extents;                            // :76-77
    } else {
        // 参考怪癖原样（:79）：other 缺席的回落是 **other.extents**（=undefined），
        // 不是 this.extents——与 lensAngle 分支不对称。
        extentsResult = other.extents.has_value()
            ? std::optional<dqGeom::Vector3d>(dqGeom::Vector3d::From(
                  extents->x + fraction * (other.extents->x - extents->x),
                  extents->y + fraction * (other.extents->y - extents->y),
                  extents->z + fraction * (other.extents->z - extents->z)))
            : other.extents;
    }

    Keyframe result;
    result.time = time + fraction * (other.time - time);          // :83
    result.location = dqGeom::Point3d::FromInterpolate(location, fraction, other.location);  // :84
    result.target = dqGeom::Point3d::FromInterpolate(target, fraction, other.target);        // :85
    result.up = dqGeom::Vector3d::FromInterpolate(up, fraction, other.up);                   // :86
    result.lensAngle = lensAngleResult;
    result.extents = extentsResult;
    return result;
}

Keyframe Keyframe::FromJSON(const dqApp::dumpjson::JsonValue* json)
{
    // :92-103
    if (json == nullptr)
        return CreateZero();

    Keyframe k;
    if (const auto* v = json->find("time"))
        k.time = v->number;
    k.location = pointFromXyz(json->find("location"));
    k.target = pointFromXyz(json->find("target"));
    if (const auto* v = json->find("up")) {
        const dqGeom::Point3d p = pointFromXyz(v);
        k.up = dqGeom::Vector3d::From(p.x, p.y, p.z);
    }
    if (const auto* v = json->find("lensAngle"))                  // :101
        k.lensAngle = angleFromJson(v);
    if (const auto* v = json->find("extents")) {                  // :102
        const dqGeom::Point3d p = pointFromXyz(v);
        k.extents = dqGeom::Vector3d::From(p.x, p.y, p.z);
    }
    return k;
}

void Keyframe::ToJSON(std::string& out) const
{
    // :105-114——undefined 字段省略（JSON.stringify 语义）
    out += "{\"time\":";
    appendNumber(out, time);
    out += ",\"location\":";
    appendXyz(out, location.x, location.y, location.z);
    out += ",\"target\":";
    appendXyz(out, target.x, target.y, target.z);
    out += ",\"up\":";
    appendXyz(out, up.x, up.y, up.z);
    if (lensAngle.has_value()) {
        out += ",\"lensAngle\":";
        appendNumber(out, lensAngle->Degrees());  // Angle.toJSON()=degrees（Angle.ts:156-158）
    }
    if (extents.has_value()) {
        out += ",\"extents\":";
        appendXyz(out, extents->x, extents->y, extents->z);
    }
    out += '}';
}

// --- CameraPath（CameraPaths.ts:123-247） ---

CameraPath::CameraPath(std::string pathName, double pathDuration, std::vector<Keyframe> frames)
    : name(std::move(pathName)), duration(pathDuration), keyframes(std::move(frames))
{
    // :136-139——带帧构造即排序
    SortKeyframes();
}

CameraPath CameraPath::CreateEmpty()
{
    // :146-148
    return CameraPath("", 0.0, {});
}

void CameraPath::SortKeyframes()
{
    // :153-155——time 升序
    std::sort(keyframes.begin(), keyframes.end(),
              [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
}

void CameraPath::Simplifies(double distanceThreshold, dqGeom::Angle lensAngleThreshold,
                            double extentThreshold)
{
    // :160-184
    size_t i = 1;
    while (i + 1 < keyframes.size()) {
        const Keyframe& before = keyframes[i - 1];
        const Keyframe& current = keyframes[i];
        const Keyframe& after = keyframes[i + 1];
        const double fraction = (current.time - before.time) / (after.time - before.time);  // :164
        const Keyframe interpolated = before.Interpolate(fraction, after);                  // :165

        // :168-172
        const bool sameCameraType = interpolated.lensAngle.has_value() == current.lensAngle.has_value();
        const double locationDistance = interpolated.location.Distance(current.location);
        const double targetDistance = interpolated.target.Distance(current.target);
        const double lensAngleDistance =
            (!interpolated.lensAngle.has_value() || !sameCameraType) ? 0.0
            : std::fabs(interpolated.lensAngle->Degrees() - current.lensAngle->Degrees());
        const double extentDistance =
            (interpolated.extents.has_value() && current.extents.has_value())
            ? interpolated.extents->Distance(*current.extents) : 0.0;

        // :174-183
        if (sameCameraType
            && locationDistance < distanceThreshold
            && targetDistance < distanceThreshold
            && lensAngleDistance < lensAngleThreshold.Degrees()
            && extentDistance < extentThreshold) {
            keyframes.erase(keyframes.begin() + static_cast<ptrdiff_t>(i));  // splice(i,1)
        } else {
            ++i;
        }
    }
}

std::optional<Keyframe> CameraPath::GetKeyframeAtTime(double time) const
{
    // :191-203 + getKeyframeRangeAtTime :210-227（内联）
    if (keyframes.empty())
        return std::nullopt;  // :212 throw → nullopt（EQUIVALENCE 见头注）

    // :215-217——单帧或早于首帧
    if (keyframes.size() == 1 || time < keyframes.front().time)
        return keyframes.front();

    // :220-223——中间逐对
    for (size_t i = 1; i < keyframes.size(); ++i) {
        if (time < keyframes[i].time) {
            const Keyframe& before = keyframes[i - 1];
            const Keyframe& after = keyframes[i];
            const double timeDifference = after.time - before.time;
            const double fraction = timeDifference != 0.0
                ? (time - before.time) / timeDifference : 0.0;  // :199-200 除零门
            return before.Interpolate(fraction, after);
        }
    }

    // :226——晚于末帧
    return keyframes.back();
}

CameraPath CameraPath::FromJSON(const dqApp::dumpjson::JsonValue* json)
{
    // :229-238
    if (json == nullptr || json->type != dqApp::dumpjson::JsonValue::Type::Object)
        return CreateEmpty();

    CameraPath path;
    if (const auto* v = json->find("name"))
        path.name = v->str;
    if (const auto* v = json->find("duration"))
        path.duration = v->number;
    if (const auto* v = json->find("keyframes")) {
        for (const auto& kf : v->arr)
            path.keyframes.push_back(Keyframe::FromJSON(&kf));
    }
    return path;
}

void CameraPath::ToJSON(std::string& out) const
{
    // :240-246——name 的 JSON 字符串转义（\" 与 \\ 两形态——本面板产生的名字
    // 经 UI 校验无控制字符）
    out += "{\"name\":\"";
    for (char c : name) {
        if (c == '"' || c == '\\')
            out += '\\';
        out += c;
    }
    out += "\",\"duration\":";
    appendNumber(out, duration);
    out += ",\"keyframes\":[";
    bool first = true;
    for (const auto& kf : keyframes) {
        if (!first)
            out += ',';
        first = false;
        kf.ToJSON(out);
    }
    out += "]}";
}

}  // namespace Gui

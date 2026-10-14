// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Camera Paths 数据层（Keyframe/CameraPath）
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/CameraPaths.ts
//   （Keyframe :21-115 + CameraPath :123-247；面板/泵见 V-c/V-d 同文件续接）
//
// JSON 形态（参考 KeyframeProps/CameraPathProps）：
//   Keyframe: {time, location:{x,y,z}, target:{x,y,z}, up:{x,y,z},
//              lensAngle?: number(degrees——Angle.toJSON()=this.degrees,
//              Angle.ts toJSON :156-158), extents?: {x,y,z}}
//   undefined 字段省略（JSON.stringify 语义）；数值 %.17g round-trip
//   （M-O(2) I9 先例）。
// 解析面：dumpjson（dqApp/tile/DumpTileTreeProps.h——SavedViewsPanel 同族消费）。
#pragma once

#include <dqGeom/Angle.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>

#include <optional>
#include <string>
#include <vector>

namespace dqApp::dumpjson { struct JsonValue; }

namespace Gui {

// Ported from: CameraPaths.ts Keyframe (:21-115)。TS public 字段 → struct 公开成员
//   （§3.2 props 先例=TimelineEntryProps）；clone() 在 C++ 值语义下=拷贝（:49-58）。
struct Keyframe {
    double time = 0.0;                          // ms（:22-23）
    dqGeom::Point3d location;                   // 相机位置（:26）
    dqGeom::Point3d target;                     // 注视点（:29）
    dqGeom::Vector3d up;                        // 上向（:32）
    std::optional<dqGeom::Angle> lensAngle;     // undefined=正交相机（:35）
    std::optional<dqGeom::Vector3d> extents;    // undefined=透视相机（:38）

    // :60-62
    static Keyframe CreateZero();
    // :67-90——lensAngle：this 缺席→other，否则 other 缺席→this，双双在场→
    //   Angle.createInterpolate；**extents 怪癖原样**（:75-80）：this 缺席→other，
    //   否则 other 缺席→**other（=undefined）**——与 lensAngle 分支回落 this
    //   不对称（疑似参考复制粘贴，§0 原样锁定）。
    Keyframe Interpolate(double fraction, const Keyframe& other) const;
    // :92-103——json 缺席/null → CreateZero（:93-94）
    static Keyframe FromJSON(const dqApp::dumpjson::JsonValue* json);
    // :105-114——out 追加对象文本（不含外层逗号）；缺席字段省略。
    void ToJSON(std::string& out) const;
};

// Ported from: CameraPaths.ts CameraPath (:123-247)
class CameraPath {
public:
    std::string name;
    double duration = 0.0;                      // ms
    // 参考 get keyframes() 直接暴露可变数组（:128-130——simplifies/sort 经它
    // 原地变异）→ 公开成员。
    std::vector<Keyframe> keyframes;

    CameraPath() = default;
    // :132-140——keyframes 在场时构造即排序
    CameraPath(std::string pathName, double pathDuration, std::vector<Keyframe> frames);

    // :146-148
    static CameraPath CreateEmpty();
    // :153-155——time 升序
    void SortKeyframes();
    // :160-184——while i=1..n-2：fraction 重建插值，四条件全过（sameCameraType 门
    //   + location/target 距离阈 + lensAngle 度数阈 + extents 距离阈）则 splice
    //   删除（i 不递增），否则 i++。
    void Simplifies(double distanceThreshold, dqGeom::Angle lensAngleThreshold,
                    double extentThreshold);
    // :191-203（getKeyframeRangeAtTime :210-227 内联）——空表 → nullopt
    //   （EQUIVALENCE：参考 throw new Error("Selected path has no keyframes.")
    //   :212 → C++ 禁异常，nullopt=throw 路径；调用面[playAnimationFrame]由
    //   UI 互斥保证非空）。
    std::optional<Keyframe> GetKeyframeAtTime(double time) const;
    // :229-238——json 缺席/非对象 → CreateEmpty（:230-231）
    static CameraPath FromJSON(const dqApp::dumpjson::JsonValue* json);
    // :240-246——out 追加对象文本。
    void ToJSON(std::string& out) const;
};

}  // namespace Gui

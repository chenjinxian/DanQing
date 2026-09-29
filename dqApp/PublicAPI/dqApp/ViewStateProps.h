// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewStateProps carriers (view persistence format)
// Ported from: itwinjs-core core/common/src/ViewStateProps.ts
//              (+ core/common/src/ViewDefinitionProps.ts / ModelSelectorProps /
//              CategorySelectorProps / DisplayStyleProps / Camera.ts CameraProps)
//
// 移植面 = 打开链消费面子集（M-H Task 3——saved ViewState 全字段应用判据
// 的渲染相关字段；ViewState3d 构造 ViewState.ts:1497-1515 + SpatialViewState.
// createFromProps SpatialViewState.ts:90-95 + Views.getViewList 的 ViewSpec
// 映射 IModelConnection.ts:1520-1524）：
//   - ViewDefinitionProps（3d 段）：classFullName/id/code.value/description/
//     isPrivate/cameraOn/origin/extents/angles/camera；
//   - CategorySelectorProps.categories / ModelSelectorProps.models /
//     DisplayStyleProps.jsonProperties.styles.viewflags；
//   - DisplayStyleProps.jsonProperties.styles.hline（M-I(4)——visible/hidden
//     边色/宽/图案 + transThreshold；消费链 = DisplayStyle3dSettings ctor
//     DisplayStyleSettings.ts:1104 → RenderPlan.ts:124 → EdgeSettings）。
// 登记未移植（载体归对应特性落地时补）：
//   - jsonProperties.viewDetails（ViewDetails——acs/gridOrient 等，ViewState
//     无 ViewDetails 载体）；
//   - displayStyleProps.jsonProperties.styles 的其余段（environment sky/ground
//     色与 display、sceneLights sunDir/ambient、excludedElements、
//     scheduleScript——DisplayStyleSettings 有 environment 载体但
//     JSON→载体映射不在本任务判据面；sceneLights 无载体）；
//   - 2d 视图段（sheetProps/sheetSize/modelExtents/baseModelId——DanQing 无
//     2d 视图）；
//   - angles 的 AngleProps 对象形（{radians/degrees}——dump 实态为 number
//     度数形；camera.lens 同）。
#pragma once

#include "Export.h"

#include <dqBase/DqId.h>
#include <dqCommon/HiddenLine.h>  // HiddenLineSettingsProps（styles.hline）
#include <dqCommon/ViewFlags.h>  // ViewFlagProps
#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>

#include <optional>
#include <string>
#include <vector>

namespace dqApp {

// ---------------------------------------------------------------------------
// CameraProps — Ported from: itwinjs-core CameraProps（core/common Camera.ts
// 构造消费面 :66-76——lens/focusDist/eye；lens 的 number 形态 = degrees，
// Angle.fromJSON）。
// ---------------------------------------------------------------------------
struct CameraProps {
    dqGeom::Point3d eye = {0.0, 0.0, 0.0};  // ← CameraProps.eye
    double focusDist = 0.0;                 // ← CameraProps.focusDist
    double lensDegrees = 0.0;               // ← CameraProps.lens（number=degrees）
};

// ---------------------------------------------------------------------------
// ViewDefinitionProps — Ported from: itwinjs-core ViewDefinitionProps +
// ViewDefinition3dProps（ViewState3d 构造消费面，ViewState.ts:1497-1515）。
// angles 段（YawPitchRollProps，:1502 经 YawPitchRollAngles.fromJSON）以
// 度数三字段承载——hasAngles=false 表示 JSON 无 angles 段（参考 fromJSON
// undefined → 全 0，语义等价于三 0，但载体保留判别便于测试钉死缺失面）。
// ---------------------------------------------------------------------------
struct ViewDefinitionProps {
    std::string classFullName;      // ← EntityProps.classFullName
    dqBase::DqId id;                // ← EntityProps.id（DqId() = Id64.invalid）
    std::string codeValue;          // ← ViewDefinitionProps.code.value
    std::string description;        // ← ViewDefinitionProps.description（ViewState ctor :303）
    bool isPrivate = false;         // ← ViewDefinitionProps.isPrivate（:304）
    // --- ViewDefinition3dProps（:1499-1504 消费面） ---
    bool cameraOn = false;          // ← cameraOn（:1499 JsonUtils.asBool）
    dqGeom::Point3d origin = {0.0, 0.0, 0.0};   // ← origin（:1500）
    dqGeom::Vector3d extents = {0.0, 0.0, 0.0}; // ← extents（:1501）
    bool hasAngles = false;         // JSON angles 段存在判别（见文件头登记）
    double yawDegrees = 0.0;        // ← angles.yaw（YawPitchRollProps，number=degrees）
    double pitchDegrees = 0.0;      // ← angles.pitch
    double rollDegrees = 0.0;       // ← angles.roll
    CameraProps camera;             // ← camera（:1504 new Camera(props.camera)）
};

// ---------------------------------------------------------------------------
// CategorySelectorProps — Ported from: itwinjs-core CategorySelectorProps
//（CategorySelectorState 构造消费面——categories 数组）。
// ---------------------------------------------------------------------------
struct CategorySelectorProps {
    dqBase::DqId id;
    std::vector<dqBase::DqId> categories;  // ← CategorySelectorProps.categories
};

// ---------------------------------------------------------------------------
// ModelSelectorProps — Ported from: itwinjs-core ModelSelectorProps
//（ModelSelectorState 构造消费面——models 数组）。
// ---------------------------------------------------------------------------
struct ModelSelectorProps {
    dqBase::DqId id;
    std::vector<dqBase::DqId> models;  // ← ModelSelectorProps.models
};

// ---------------------------------------------------------------------------
// DisplayStyleProps — Ported from: itwinjs-core DisplayStyleProps。
// 消费面 = jsonProperties.styles.viewflags（DisplayStyle3dState 的 ViewFlags
// 应用——ViewFlags.fromJSON ViewFlags.ts:471-511）+ styles.hline（M-I(4)——
// DisplayStyle3dSettings ctor DisplayStyleSettings.ts:1104 的
// HiddenLine.Settings.fromJSON 段）；styles 其余段登记未移植（见文件头）。
// ---------------------------------------------------------------------------
struct DisplayStyleProps {
    dqBase::DqId id;
    std::optional<dqCommon::ViewFlagProps> viewflags;  // ← styles.viewflags
    // ← styles.hline（HiddenLine.SettingsProps——visible/hidden/transThreshold；
    // dump 实态：visible={color:0,ovrColor:true,pattern:0,width:1} = 黑边覆盖）。
    std::optional<dqCommon::HiddenLineSettingsProps> hline;
};

// ---------------------------------------------------------------------------
// ViewStateProps — Ported from: itwinjs-core ViewStateProps
//（core/common/src/ViewStateProps.ts——getViewStateData RPC 的返回形态；
// modelSelectorProps 仅空间视图携带，:optional）。
// ---------------------------------------------------------------------------
struct ViewStateProps {
    ViewDefinitionProps viewDefinitionProps;                  // ← viewDefinitionProps
    CategorySelectorProps categorySelectorProps;              // ← categorySelectorProps
    DisplayStyleProps displayStyleProps;                      // ← displayStyleProps
    std::optional<ModelSelectorProps> modelSelectorProps;     // ← modelSelectorProps?
};

}  // namespace dqApp

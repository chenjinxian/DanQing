// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — text annotation leader geometry
// Ported from: itwinjs-core core/backend/src/annotations/LeaderGeometry.ts
//
// 参考的 CurveCurve.intersectionXYZPairs / closestPoint / collectCurvePrimitives
// 为 dqGeom 未移植面——三附件臂按采样 EQUIVALENCE 承载（§11.10 登记）：
// - Nearest：帧曲线均匀采样（64 点/曲线）取最近——与解析最近点差 ≤ 弦差
//   （帧形全为小尺度解析曲线，宿主展示容差内）。
// - KeyPoint：FrameGeometryResult.curves 直接索引（免泛型遍历——曲线集封闭）。
// - TextPoint：投影线与帧采样点的最近前向交点（解析线-弧交点 TODO 随
//   CurveCurve 落地复核）。
// TODO: 解析 CurveCurve 交点（dqGeom 移植缺口）落地后三臂回参考形。
#pragma once

#include "DqCommon.h"
#include "FrameGeometry.h"
#include "TextAnnotation.h"
#include "TextBlockLayout.h"

#include <dqGeom/LineString3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>

#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

class GeometryStreamBuilder;

// Arguments to createTerminatorGeometry.
// Ported from: itwinjs-core CreateTerminatorGeometryArgs (:141-156)
struct CreateTerminatorGeometryArgs {
    GeometryStreamBuilder* builder = nullptr;
    dqGeom::Point3d point;                // terminator placement point
    dqGeom::Vector3d dir;                 // leader line direction
    GeometryAppearanceProps params;       // terminator geometry params
    TextStyleSettings textStyleSettings;  // terminator shape/dimensions
    double textHeight = 1.0;              // scale for terminator dimensions
    bool isArrow = true;                  // false → target-point shape
};

// Creates the geometry for a terminator at the end of a leader line.
// Ported from: itwinjs-core createTerminatorGeometry (:164-285)
DQ_COMMON_EXPORT bool createTerminatorGeometry(CreateTerminatorGeometryArgs& args);

// Computes the elbow direction for a leader（附件点对帧左/右近侧判定 + 相切
// 抑制——切向经采样邻点方向 EQUIVALENCE，文件头登记）。
// Ported from: itwinjs-core computeElbowDirection (:299-327)
DQ_COMMON_EXPORT std::optional<dqGeom::Vector3d> computeElbowDirection(
    dqGeom::Point3d const& attachmentPoint, FrameGeometryResult const& frameCurve,
    double elbowLength);

// Computes the leader attachment point on the annotation frame（三模式）。
// Ported from: itwinjs-core computeLeaderAttachmentPoint (:344-390)
DQ_COMMON_EXPORT std::optional<dqGeom::Point3d> computeLeaderAttachmentPoint(
    TextAnnotationLeader const& leader, FrameGeometryResult const& frameCurve,
    TextBlockLayout const& textLayout, dqGeom::Transform const& transform);

// Constructs and appends leader lines + terminators for a text annotation.
// Ported from: itwinjs-core appendLeadersToBuilder (:32-135)
DQ_COMMON_EXPORT bool appendLeadersToBuilder(
    GeometryStreamBuilder& builder, std::vector<TextAnnotationLeader> const& leaders,
    TextBlockLayout const& layout, dqGeom::Transform const& transform,
    GeometryAppearanceProps const& params, TextStyleResolver const& textStyleResolver,
    double scaleFactor);

END_DQ_COMMON_NAMESPACE

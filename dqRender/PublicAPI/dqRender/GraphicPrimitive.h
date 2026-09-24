// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/render/GraphicPrimitive.ts
// DanQing dqRender — GraphicPrimitive (12-variant union supplied to GraphicBuilder.addPrimitive)
//
// 保真依据：逐类型移植 GraphicPrimitive.ts。TS union-of-interfaces → C++ std::variant of 12 struct。
// 判别由 variant.index() 承担（TS 的 `type` 字符串字面量仅作运行时 narrow 标记，C++ 静态类型已区分，
// 无对应运行时需求）。GraphicPrimitive2d 基接口（仅 zDepth）保留为基 struct，3 个 2d 变体
// （GraphicLineString2d/GraphicPointString2d/GraphicShape2d）1:1 `extends` 继承之；GraphicArc2d 参考
// 不继承 GraphicPrimitive2d，自带 zDepth（1:1 保留）。几何对象（Arc3d/Path/Loop/Polyface/SolidPrimitive）
// 均为 RefCounted（GeometryQuery 子类），变体持 RefPtr<const T> 强引用（对齐 TS GC 引用语义 + §9 禁裸指针）。
#pragma once

#include <dqBase/RefCounted.h>
#include <dqGeom/Arc3d.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Path.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Polyface.h>
#include <dqGeom/SolidPrimitive.h>

#include <variant>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Base interface for a 2d GraphicPrimitive (1:1 GraphicPrimitive2d): the z value in local
// coordinates to use for each point. Concrete 2d variants inherit this.
struct GraphicPrimitive2d {
    double zDepth = 0.0; // 1:1 GraphicPrimitive2d.zDepth
};

// 1:1 GraphicLineString — see GraphicBuilder.addLineString.
struct GraphicLineString { std::vector<dqGeom::Point3d> points; };

// 1:1 GraphicLineString2d extends GraphicPrimitive2d — see GraphicBuilder.addLineString2d.
struct GraphicLineString2d : GraphicPrimitive2d { std::vector<dqGeom::Point2d> points; };

// 1:1 GraphicPointString — see GraphicBuilder.addPointString.
struct GraphicPointString { std::vector<dqGeom::Point3d> points; };

// 1:1 GraphicPointString2d extends GraphicPrimitive2d — see GraphicBuilder.addPointString2d.
struct GraphicPointString2d : GraphicPrimitive2d { std::vector<dqGeom::Point2d> points; };

// 1:1 GraphicShape (closed 3d planar region) — see GraphicBuilder.addShape.
struct GraphicShape { std::vector<dqGeom::Point3d> points; };

// 1:1 GraphicShape2d extends GraphicPrimitive2d (closed 2d region) — see GraphicBuilder.addShape2d.
struct GraphicShape2d : GraphicPrimitive2d { std::vector<dqGeom::Point2d> points; };

// 1:1 GraphicArc (3d open arc or closed ellipse). isEllipse/filled optional → default false,
// mirroring TS `true === primitive.isEllipse` undefined-coercion.
struct GraphicArc {
    dqBase::RefPtr<const dqGeom::Arc3d> arc;
    bool isEllipse = false;
    bool filled = false;
};

// 1:1 GraphicArc2d (2d open arc / closed ellipse). Reference does NOT extend GraphicPrimitive2d;
// it carries its own zDepth.
struct GraphicArc2d {
    dqBase::RefPtr<const dqGeom::Arc3d> arc;
    bool isEllipse = false;
    bool filled = false;
    double zDepth = 0.0;
};

// 1:1 GraphicPath (3d open path) — see GraphicBuilder.addPath.
struct GraphicPath { dqBase::RefPtr<const dqGeom::Path> path; };

// 1:1 GraphicLoop (3d planar region) — see GraphicBuilder.addLoop.
struct GraphicLoop { dqBase::RefPtr<const dqGeom::Loop> loop; };

// 1:1 GraphicPolyface (mesh). filled optional → default false.
struct GraphicPolyface {
    dqBase::RefPtr<const dqGeom::Polyface> polyface;
    bool filled = false;
};

// 1:1 GraphicSolidPrimitive — see GraphicBuilder.addSolidPrimitive.
struct GraphicSolidPrimitive { dqBase::RefPtr<const dqGeom::SolidPrimitive> solidPrimitive; };

// Union of all graphic primitives that can be supplied to GraphicBuilder.addPrimitive.
// Each variant corresponds to one of GraphicBuilder's addXXX methods (1:1 GraphicPrimitive).
using GraphicPrimitive = std::variant<
    GraphicLineString,
    GraphicLineString2d,
    GraphicPointString,
    GraphicPointString2d,
    GraphicShape,
    GraphicShape2d,
    GraphicArc,
    GraphicArc2d,
    GraphicPath,
    GraphicLoop,
    GraphicPolyface,
    GraphicSolidPrimitive
>;

END_DQ_RENDER_NAMESPACE

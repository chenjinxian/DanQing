// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/GeometryPrimitives.ts
// DanQing dqRender — Geometry (abstract accumulator record) + 6 concrete subclasses + factories.
//
// 保真依据：逐类移植 GeometryPrimitives.ts。Geometry 持 transform/tileRange/displayParams/feature；
// 6 子类（PrimitivePointString/LineString/Loop/Path/PolyfaceGeometry + SolidPrimitiveGeometry）覆写
// _getPolyfaces/_getStrokes。getPolyfaces/getStrokes 建 StrokeOptions（CreateForFacets/Curves + chordTol
// + needParams/needNormals）后分发。PolyfaceGeometry/SolidPrimitiveGeometry ctor eager bake transform
// （CloneTransformed(identity-or-tf).StaticCast）。PrimitiveLoopGeometry._getPolyfaces 的 SweepContour
// 路径为 Phase-N TODO，fallback 用 CloneStroked+AddPolygon（单环平面区域）。@internal。
#pragma once

#include "DisplayParams.h"
#include "Polyface.h"
#include "Strokes.h"

#include <dqBase/RefCounted.h>
#include <dqCommon/FeatureTable.h>
#include <dqGeom/CurveCollection.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Path.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/PolyfaceBuilder.h>
#include <dqGeom/PolyfaceQuery.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/SolidPrimitive.h>
#include <dqGeom/StrokeOptions.h>
#include <dqGeom/Transform.h>

#include <memory>
#include <optional>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 PrimitiveGeometryType = Loop | Path | IndexedPolyface | SolidPrimitive (type alias; C++ no-op).

// 1:1 Geometry (GeometryPrimitives.ts:22-84) — abstract accumulator record.
class Geometry {
public:
    virtual ~Geometry() = default;

    Geometry(const dqGeom::Transform& transform, const dqGeom::Range3d& tileRange,
             const DisplayParams& displayParams, std::optional<dqCommon::Feature> feature)
        : m_transform(transform), m_tileRange(tileRange), m_displayParams(displayParams), m_feature(std::move(feature)) {}

    // --- Static factories (1:1 Geometry.createFromXxx) ---
    static std::unique_ptr<Geometry> createFromPointString(const std::vector<dqGeom::Point3d>& pts,
        const dqGeom::Transform& tf, const dqGeom::Range3d& tileRange, const DisplayParams& params,
        std::optional<dqCommon::Feature> feature);
    static std::unique_ptr<Geometry> createFromLineString(const std::vector<dqGeom::Point3d>& pts,
        const dqGeom::Transform& tf, const dqGeom::Range3d& tileRange, const DisplayParams& params,
        std::optional<dqCommon::Feature> feature);
    static std::unique_ptr<Geometry> createFromLoop(const dqBase::RefPtr<dqGeom::Loop>& loop,
        const dqGeom::Transform& tf, const dqGeom::Range3d& tileRange, const DisplayParams& params,
        bool disjoint, std::optional<dqCommon::Feature> feature);
    static std::unique_ptr<Geometry> createFromSolidPrimitive(const dqBase::RefPtr<dqGeom::SolidPrimitive>& primitive,
        const dqGeom::Transform& tf, const dqGeom::Range3d& tileRange, const DisplayParams& params,
        std::optional<dqCommon::Feature> feature);
    static std::unique_ptr<Geometry> createFromPath(const dqBase::RefPtr<dqGeom::Path>& path,
        const dqGeom::Transform& tf, const dqGeom::Range3d& tileRange, const DisplayParams& params,
        bool disjoint, std::optional<dqCommon::Feature> feature);
    static std::unique_ptr<Geometry> createFromPolyface(const dqBase::RefPtr<dqGeom::IndexedPolyface>& ipf,
        const dqGeom::Transform& tf, const dqGeom::Range3d& tileRange, const DisplayParams& params,
        std::optional<dqCommon::Feature> feature);

    // 1:1 getPolyfaces(tolerance) / getStrokes(tolerance).
    std::optional<PolyfacePrimitiveList> getPolyfaces(double tolerance) const;
    std::optional<StrokesPrimitiveList> getStrokes(double tolerance) const;

    bool hasTexture() const { return m_displayParams.isTextured(); }
    bool doDecimate() const { return false; }
    bool doVertexCluster() const { return true; }
    // 1:1 Geometry.part() → undefined (no associated part). DanQing: returns nullptr.
    const void* part() const noexcept { return nullptr; }

    const dqGeom::Transform& transform() const noexcept { return m_transform; }
    const dqGeom::Range3d& tileRange() const noexcept { return m_tileRange; }
    const DisplayParams& displayParams() const noexcept { return m_displayParams; }
    const std::optional<dqCommon::Feature>& feature() const noexcept { return m_feature; }

protected:
    virtual std::optional<PolyfacePrimitiveList> _getPolyfaces(const dqGeom::StrokeOptions& facetOptions) const = 0;
    virtual std::optional<StrokesPrimitiveList> _getStrokes(const dqGeom::StrokeOptions& facetOptions) const = 0;

    // 登记 DEViation（2026-09-14，ACS 盘深缩放消失修复）：世界 chordTol → 记录局部
    // 空间的容差换算（÷ placement 最大轴缩放）。理由见 GeometryPrimitives.cpp 实现
    // 注释（参考以世界容差 stroke 局部几何，屏幕恒定尺寸装饰深缩放端 stroke 爆点
    // → 世界弦长塌到焊接容差下 → 填充全灭；DTA 空白连接不画 ACS triad，无参考
    // 可见行为可对齐）。
    double localChordTolerance(double worldTolerance) const;

    dqGeom::Transform m_transform;
    dqGeom::Range3d m_tileRange;
    DisplayParams m_displayParams;
    std::optional<dqCommon::Feature> m_feature;
};

// 1:1 PrimitivePathGeometry (GeometryPrimitives.ts:87-130).
class PrimitivePathGeometry : public Geometry {
public:
    PrimitivePathGeometry(const dqBase::RefPtr<dqGeom::Path>& path, const dqGeom::Transform& tf,
        const dqGeom::Range3d& range, const DisplayParams& params, bool isDisjoint,
        std::optional<dqCommon::Feature> feature)
        : Geometry(tf, range, params, std::move(feature)), m_path(path), m_isDisjoint(isDisjoint) {}

    static std::optional<StrokesPrimitiveList> getStrokesForLoopOrPath(const dqGeom::CurveChain& loopOrPath,
        const dqGeom::StrokeOptions& facetOptions, const DisplayParams& params, bool isDisjoint,
        const dqGeom::Transform& transform);

protected:
    std::optional<PolyfacePrimitiveList> _getPolyfaces(const dqGeom::StrokeOptions&) const override { return std::nullopt; }
    std::optional<StrokesPrimitiveList> _getStrokes(const dqGeom::StrokeOptions& facetOptions) const override;

private:
    static void collectCurveStrokes(StrokesPrimitivePointLists& strksPts, const dqGeom::CurveChain& loopOrPath,
        const dqGeom::StrokeOptions& facetOptions, const dqGeom::Transform& trans);

    dqBase::RefPtr<dqGeom::Path> m_path;
    bool m_isDisjoint;
};

// 1:1 PrimitivePointStringGeometry (GeometryPrimitives.ts:133-156).
class PrimitivePointStringGeometry : public Geometry {
public:
    PrimitivePointStringGeometry(std::vector<dqGeom::Point3d> pts, const dqGeom::Transform& tf,
        const dqGeom::Range3d& range, const DisplayParams& params, std::optional<dqCommon::Feature> feature)
        : Geometry(tf, range, params, std::move(feature)), m_pts(std::move(pts)) {}

protected:
    std::optional<PolyfacePrimitiveList> _getPolyfaces(const dqGeom::StrokeOptions&) const override { return std::nullopt; }
    std::optional<StrokesPrimitiveList> _getStrokes(const dqGeom::StrokeOptions&) const override;

private:
    std::vector<dqGeom::Point3d> m_pts;
};

// 1:1 PrimitiveLineStringGeometry (GeometryPrimitives.ts:159-182).
class PrimitiveLineStringGeometry : public Geometry {
public:
    PrimitiveLineStringGeometry(std::vector<dqGeom::Point3d> pts, const dqGeom::Transform& tf,
        const dqGeom::Range3d& range, const DisplayParams& params, std::optional<dqCommon::Feature> feature)
        : Geometry(tf, range, params, std::move(feature)), m_pts(std::move(pts)) {}

protected:
    std::optional<PolyfacePrimitiveList> _getPolyfaces(const dqGeom::StrokeOptions&) const override { return std::nullopt; }
    std::optional<StrokesPrimitiveList> _getStrokes(const dqGeom::StrokeOptions&) const override;

private:
    std::vector<dqGeom::Point3d> m_pts;
};

// 1:1 PrimitiveLoopGeometry (GeometryPrimitives.ts:185-217).
class PrimitiveLoopGeometry : public Geometry {
public:
    PrimitiveLoopGeometry(const dqBase::RefPtr<dqGeom::Loop>& loop, const dqGeom::Transform& tf,
        const dqGeom::Range3d& range, const DisplayParams& params, bool isDisjoint,
        std::optional<dqCommon::Feature> feature)
        : Geometry(tf, range, params, std::move(feature)), m_loop(loop), m_isDisjoint(isDisjoint) {}

protected:
    std::optional<PolyfacePrimitiveList> _getPolyfaces(const dqGeom::StrokeOptions& facetOptions) const override;
    std::optional<StrokesPrimitiveList> _getStrokes(const dqGeom::StrokeOptions& facetOptions) const override {
        return PrimitivePathGeometry::getStrokesForLoopOrPath(*m_loop, facetOptions, m_displayParams, m_isDisjoint, m_transform);
    }

private:
    dqBase::RefPtr<dqGeom::Loop> m_loop;
    bool m_isDisjoint;
};

// 1:1 PrimitivePolyfaceGeometry (GeometryPrimitives.ts:220-251) — ctor bakes transform into the polyface.
class PrimitivePolyfaceGeometry : public Geometry {
public:
    PrimitivePolyfaceGeometry(const dqBase::RefPtr<dqGeom::IndexedPolyface>& polyface, const dqGeom::Transform& tf,
        const dqGeom::Range3d& range, const DisplayParams& params, std::optional<dqCommon::Feature> feature);

    const dqBase::RefPtr<dqGeom::IndexedPolyface>& polyface() const noexcept { return m_polyface; }

protected:
    std::optional<PolyfacePrimitiveList> _getPolyfaces(const dqGeom::StrokeOptions& facetOptions) const override;
    std::optional<StrokesPrimitiveList> _getStrokes(const dqGeom::StrokeOptions&) const override { return std::nullopt; }

private:
    dqBase::RefPtr<dqGeom::IndexedPolyface> m_polyface;
};

// 1:1 SolidPrimitiveGeometry (GeometryPrimitives.ts:253-269) — ctor bakes transform into the solid.
class SolidPrimitiveGeometry : public Geometry {
public:
    SolidPrimitiveGeometry(const dqBase::RefPtr<dqGeom::SolidPrimitive>& primitive, const dqGeom::Transform& tf,
        const dqGeom::Range3d& range, const DisplayParams& params, std::optional<dqCommon::Feature> feature);

protected:
    std::optional<StrokesPrimitiveList> _getStrokes(const dqGeom::StrokeOptions&) const override { return std::nullopt; }
    std::optional<PolyfacePrimitiveList> _getPolyfaces(const dqGeom::StrokeOptions& opts) const override;

private:
    dqBase::RefPtr<dqGeom::SolidPrimitive> m_primitive;
};

END_DQ_RENDER_NAMESPACE

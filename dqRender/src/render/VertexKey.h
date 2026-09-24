// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/VertexKey.ts
// DanQing dqRender — VertexKeyProps / VertexKey / VertexMap (vertex dedup for MeshBuilder; @internal)
//
// 保真依据：逐类型移植 VertexKey.ts。VertexKeyProps{position,fillColor,normal?,uvParam?,feature?}；
// VertexKey 持同名字段 + create/equals/compare；VertexMap = IndexMap<VertexKey>（compare 带 tolerance，
// ctor 捕获 tolerance），insertKey(props,onInsert)/arePositionsAlmostEqual/comparePositions。
// compareWithTolerance/compareFeatures 为 core-bentley 同名 helper 的 Authored 内联复刻
// （dqBase 未暴露；语义 1:1）。@internal；仅 MeshBuilder.addVertex 去重消费。
#pragma once

#include <dqBase/IndexMap.h>
#include <dqCommon/FeatureTable.h>
#include <dqCommon/OctEncodedNormal.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>

#include <cstdint>
#include <functional>
#include <optional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 VertexKeyProps (VertexKey.ts:14-20) — input shape for VertexKey/VertexMap/insertion.
struct VertexKeyProps {
    dqGeom::Point3d position;
    uint32_t fillColor = 0;
    std::optional<dqCommon::OctEncodedNormal> normal;
    std::optional<dqGeom::Point2d> uvParam;
    std::optional<dqCommon::Feature> feature;
};

// 1:1 VertexKey (VertexKey.ts:38-108) — a dedup key for one mesh vertex.
class VertexKey {
public:
    VertexKey(dqGeom::Point3d position, uint32_t fillColor,
              std::optional<dqCommon::OctEncodedNormal> normal = std::nullopt,
              std::optional<dqGeom::Point2d> uvParam = std::nullopt,
              std::optional<dqCommon::Feature> feature = std::nullopt)
        : m_position(position)
        , m_fillColor(fillColor)
        , m_normal(normal)
        , m_uvParam(uvParam)
        , m_feature(feature)
    {
    }

    // 1:1 VertexKey.create(props).
    static VertexKey create(const VertexKeyProps& props)
    {
        return VertexKey(props.position, props.fillColor, props.normal, props.uvParam, props.feature);
    }

    // 1:1 VertexKey.equals(rhs, tolerance).
    bool equals(const VertexKey& rhs, const dqGeom::Point3d& tolerance) const;
    // 1:1 VertexKey.compare(rhs, tolerance) — tiered ordered comparison for IndexMap.
    int compare(const VertexKey& rhs, const dqGeom::Point3d& tolerance) const;

    // Field accessors (1:1 readonly fields; consumers read via VertexKeyProps).
    const dqGeom::Point3d& position() const noexcept { return m_position; }
    uint32_t fillColor() const noexcept { return m_fillColor; }
    const std::optional<dqCommon::OctEncodedNormal>& normal() const noexcept { return m_normal; }
    const std::optional<dqGeom::Point2d>& uvParam() const noexcept { return m_uvParam; }
    const std::optional<dqCommon::Feature>& feature() const noexcept { return m_feature; }

    // 1:1 structural compatibility: VertexKey is consumed where VertexKeyProps is expected
    // (Mesh.addVertex). TS achieves this structurally; DanQing exposes an explicit props view.
    VertexKeyProps props() const
    {
        return VertexKeyProps{m_position, m_fillColor, m_normal, m_uvParam, m_feature};
    }

private:
    dqGeom::Point3d m_position;
    uint32_t m_fillColor = 0;
    std::optional<dqCommon::OctEncodedNormal> m_normal;
    std::optional<dqGeom::Point2d> m_uvParam;
    std::optional<dqCommon::Feature> m_feature;
};

// 1:1 VertexMap (VertexKey.ts:111-130) — IndexMap<VertexKey> with a position-tolerance-aware compare.
class VertexMap : public dqBase::IndexMap<VertexKey> {
public:
    // 1:1 VertexMap constructor: super((lhs,rhs) => lhs.compare(rhs, tolerance)).
    explicit VertexMap(const dqGeom::Point3d& tolerance)
        : dqBase::IndexMap<VertexKey>([tolerance](const VertexKey& a, const VertexKey& b) {
            return a.compare(b, tolerance);
        })
        , m_tolerance(tolerance)
    {
    }

    // 1:1 VertexMap.insertKey(props, onInsert?) — insert by props, invoke onInsert only when the key
    // is newly inserted. DanQing IndexMap::insert has no onInsert hook, so check-then-insert here.
    // Index consistency: map-index == mesh-vertex-index holds because every new key inserts exactly
    // one map entry AND (via onInsert) exactly one mesh vertex, in step (invariant maintained per call).
    int insertKey(const VertexKeyProps& props, const std::function<void(const VertexKey&)>& onInsert = nullptr)
    {
        const VertexKey key = VertexKey::create(props);
        const int existing = indexOf(key);
        if (existing >= 0)
            return existing;
        if (onInsert)
            onInsert(key);
        return dqBase::IndexMap<VertexKey>::insert(key);
    }

    // 1:1 VertexMap.arePositionsAlmostEqual(p0, p1).
    bool arePositionsAlmostEqual(const VertexKeyProps& p0, const VertexKeyProps& p1) const
    {
        return 0 == comparePositions(p0, p1);
    }

    // 1:1 VertexMap.comparePositions(p0, p1).
    int comparePositions(const VertexKeyProps& p0, const VertexKeyProps& p1) const;

private:
    dqGeom::Point3d m_tolerance;
};

END_DQ_RENDER_NAMESPACE

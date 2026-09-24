// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/MeshBuilderMap.ts
// DanQing dqRender — MeshBuilderMap (maps Geometry→MeshBuilder by symbology key; @internal)
//
// 保真依据：逐方法移植 MeshBuilderMap.ts。extends Dictionary<Key,MeshBuilder>（DanQing std::map<Key,
// unique_ptr<MeshBuilder>>，sorted for deterministic output；MeshBuilder move-only 故 unique_ptr 持有）。
// ctor 计算 vertexTolerance/facetAreaTolerance（tolerance * ToleranceRatio.vertex/facetArea），pickable 时
// 建 FeatureTable(2048*1024, modelId)。createFromGeometries/loadGeometry/loadPolyfacePrimitiveList/
// loadIndexedPolyface/loadStrokePrimitiveList/loadStrokesPrimitive/getBuilder/getKey/getBuilderFromKey/toMeshes。
// Key.compare（order→type→isPlanar→hasNormals→params.compareForMerge）。
//
// 命名：dqRender/src/render 内部全 camelCase（§3.3 + 与 DisplayParams/GeometryPrimitives/MeshPrimitives
// 既有约定一致）。
//
// DEFERRED：feature-table 跨 mesh 共享（参考 JS 按引用共享 this.features；DanQing §9 禁 shared_ptr 且
// FeatureTable 非.RefCounted → D′ 在 pickable 时按值拷贝 m_features 入每个 Mesh.Props，已知限制：feature
// 索引非全局。D′ 主路径无 pickable → features 恒 nullopt，不触发。pickable/batch 全局索引待 PackedFeatureTable
// port 时重构）。@internal；仅 GeometryAccumulator 消费。
#pragma once

#include "GeometryList.h"
#include "GeometryPrimitives.h"
#include "MeshBuilder.h"
#include "MeshPrimitives.h"
#include "Primitives.h"
#include "Strokes.h"

#include <dqCommon/FeatureTable.h>
#include <dqCommon/TextureMapping.h>
#include <dqBase/DqId.h>
#include <dqGeom/Range3d.h>

#include <map>
#include <memory>
#include <optional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 MeshBuilderMap pickable shape ({ isVolumeClassifier?, modelId? } | undefined).
struct MeshBuilderMapPickable {
    bool isVolumeClassifier = false;
    dqBase::DqId modelId;
};

// 1:1 MeshBuilderMap (MeshBuilderMap.ts:23-171).
class MeshBuilderMap {
public:
    // 1:1 MeshBuilderMap.Key (MeshBuilderMap.ts:175-212).
    class Key {
    public:
        Key(DisplayParams params, MeshPrimitiveType type, bool hasNormals, bool isPlanar)
            : m_params(std::move(params))
            , m_type(type)
            , m_hasNormals(hasNormals)
            , m_isPlanar(isPlanar)
        {
        }

        int order() const noexcept { return m_order; }
        void setOrder(int o) noexcept { m_order = o; }

        const DisplayParams& params() const noexcept { return m_params; }
        MeshPrimitiveType type() const noexcept { return m_type; }
        bool hasNormals() const noexcept { return m_hasNormals; }
        bool isPlanar() const noexcept { return m_isPlanar; }

        // 1:1 Key.compare(rhs).
        int compare(const Key& rhs) const;
        // 1:1 Key.equals(rhs).
        bool equals(const Key& rhs) const { return 0 == compare(rhs); }

        // 1:1 Key.createFromMesh(mesh).
        static Key createFromMesh(const Mesh& mesh)
        {
            return Key(mesh.displayParams(), mesh.type(), !mesh.normals().empty(), mesh.isPlanar());
        }

    private:
        int m_order = 0;
        DisplayParams m_params;
        MeshPrimitiveType m_type = MeshPrimitiveType::Mesh;
        bool m_hasNormals = false;
        bool m_isPlanar = false;
    };

    // 1:1 MeshBuilderMap constructor (MeshBuilderMap.ts:34-46).
    MeshBuilderMap(double tolerance, const dqGeom::Range3d& range, bool is2d, GeometryOptions options,
                   const std::optional<MeshBuilderMapPickable>& pickable);

    // 1:1 MeshBuilderMap.createFromGeometries (MeshBuilderMap.ts:48-55).
    static std::unique_ptr<MeshBuilderMap> createFromGeometries(GeometryList& geometries, double tolerance,
            const dqGeom::Range3d& range, bool is2d, GeometryOptions options,
            const std::optional<MeshBuilderMapPickable>& pickable);

    // 1:1 MeshBuilderMap.toMeshes (MeshBuilderMap.ts:57-64).
    MeshList toMeshes();

    // 1:1 MeshBuilderMap.loadGeometry (MeshBuilderMap.ts:70-73).
    void loadGeometry(Geometry& geom);

    // 1:1 MeshBuilderMap.loadPolyfacePrimitiveList (MeshBuilderMap.ts:79-85).
    void loadPolyfacePrimitiveList(Geometry& geom);
    // 1:1 MeshBuilderMap.loadIndexedPolyface (MeshBuilderMap.ts:91-103).
    void loadIndexedPolyface(const PolyfacePrimitive& polyface, const std::optional<dqCommon::Feature>& feature);

    // 1:1 MeshBuilderMap.loadStrokePrimitiveList (MeshBuilderMap.ts:109-115).
    void loadStrokePrimitiveList(Geometry& geom);
    // 1:1 MeshBuilderMap.loadStrokesPrimitive (MeshBuilderMap.ts:121-127).
    void loadStrokesPrimitive(const StrokesPrimitive& strokePrimitive,
                              const std::optional<dqCommon::Feature>& feature);

    // 1:1 MeshBuilderMap.getBuilder (MeshBuilderMap.ts:129-146).
    MeshBuilder& getBuilder(const DisplayParams& displayParams, MeshPrimitiveType type, bool hasNormals,
                            bool isPlanar);
    // 1:1 MeshBuilderMap.getKey (MeshBuilderMap.ts:148-155).
    Key getKey(const DisplayParams& displayParams, MeshPrimitiveType type, bool hasNormals, bool isPlanar);
    // 1:1 MeshBuilderMap.getBuilderFromKey (MeshBuilderMap.ts:163-170).
    MeshBuilder& getBuilderFromKey(const Key& key, MeshBuilder::Props props);

    // --- Accessors (1:1 readonly fields) ---
    size_t size() const noexcept { return m_map.size(); }  // 1:1 Dictionary.size
    const dqGeom::Range3d& range() const noexcept { return m_range; }
    double vertexTolerance() const noexcept { return m_vertexTolerance; }
    double facetAreaTolerance() const noexcept { return m_facetAreaTolerance; }
    double tolerance() const noexcept { return m_tolerance; }
    bool is2d() const noexcept { return m_is2d; }
    const std::optional<dqCommon::FeatureTable>& features() const noexcept { return m_features; }
    const GeometryOptions& options() const noexcept { return m_options; }

private:
    struct KeyCompare {
        bool operator()(const Key& a, const Key& b) const { return a.compare(b) < 0; }
    };

    // Dictionary<Key, MeshBuilder> — unique_ptr because MeshBuilder is move-only (created via factory).
    std::map<Key, std::unique_ptr<MeshBuilder>, KeyCompare> m_map;

    dqGeom::Range3d m_range;
    double m_tolerance = 0.0;
    double m_vertexTolerance = 0.0;
    double m_facetAreaTolerance = 0.0;
    bool m_is2d = false;
    GeometryOptions m_options;
    std::optional<dqCommon::FeatureTable> m_features;
    bool m_isVolumeClassifier = false;
    int m_keyOrder = 0;
};

END_DQ_RENDER_NAMESPACE

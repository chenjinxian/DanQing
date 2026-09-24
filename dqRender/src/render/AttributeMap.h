// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Attribute map (name to shader location mapping)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/AttributeMap.ts
//
// Maps named attributes (a_pos, a_norm, a_uvParam, etc.) to their
// locations in a shader program.  Per-technique attribute maps are
// maintained for both instanced and non-instanced rendering.
//
// Instance attributes (a_instanceMatrixRow0/1/2, a_instanceOverrides,
// a_instanceRgba, a_featureId, a_patternX, a_patternY) are appended
// after the technique's base attributes when instanced=true.
#pragma once

#include "TechniqueImpl.h"
// 参考实现（AttributeMap.ts:9）从 ShaderBuilder import VariableType：
//   import { VariableType } from "./ShaderBuilder";
// 此前本文件重定义了一份同名但值序不同的 VariableType（Float 首位 vs 参考
// Boolean 首位）——与 ShaderBuilder.h 构成 ODR 双定义（同名枚举不同值，跨 TU
// 静默值重映射）。ShaderBuilder.h 为唯一权威定义。
#include "ShaderBuilder.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// AttributeDetails — details of a single attribute
// (Ported from: itwinjs-core AttributeMap.ts AttributeDetails, line 16-21)
// ---------------------------------------------------------------------------
struct AttributeDetails {
    uint32_t location = 0;
    VariableType type = VariableType::Vec3;
};

// ---------------------------------------------------------------------------
// AttributeMapEntry — per-technique attribute map
// (Ported from: itwinjs-core AttributeMap.ts AttributeMapEntry, line 25-53)
//
// Holds both uninstanced and instanced attribute maps.
// Instance attributes are appended after the technique's base attributes.
// ---------------------------------------------------------------------------
class AttributeMapEntry {
public:
    struct AttributeInfo {
        std::string name;
        uint32_t location;
        VariableType type;
    };

    AttributeMapEntry() = default;
    explicit AttributeMapEntry(std::vector<AttributeInfo> attributes);

    /// Get the uninstanced attribute map.
    std::unordered_map<std::string, AttributeDetails> const& getUninstanced() const { return m_uninstanced; }

    /// Get the instanced attribute map.
    std::unordered_map<std::string, AttributeDetails> const& getInstanced() const { return m_instanced; }

    /// Find an attribute by name.
    AttributeDetails const* find(std::string const& name, bool instanced) const;

private:
    std::unordered_map<std::string, AttributeDetails> m_uninstanced;
    std::unordered_map<std::string, AttributeDetails> m_instanced;
};

// ---------------------------------------------------------------------------
// AttributeMap — global attribute map singleton
// (Ported from: itwinjs-core AttributeMap.ts, line 60-133)
//
// Provides static access to technique-specific attribute maps.
// The singleton is lazily initialized on first access.
// ---------------------------------------------------------------------------
class AttributeMap {
public:
    /// Find an attribute by name, technique, and instancing mode.
    /// Ported from: itwinjs-core AttributeMap.ts line 128-130
    static AttributeDetails const* findAttribute(
        std::string const& name,
        TechniqueId techniqueId,
        bool instanced);

    /// Find the attribute map for a technique.
    /// Ported from: itwinjs-core AttributeMap.ts line 116-126
    static AttributeMapEntry const& findAttributeMap(
        TechniqueId techniqueId,
        bool instanced);

private:
    AttributeMap();
    static AttributeMap const& getInstance();

    std::unordered_map<TechniqueId, AttributeMapEntry> m_entries;
    AttributeMapEntry m_defaultEntry;
};

END_DQ_RENDER_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — AttributeMap implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/AttributeMap.ts
#include "AttributeMap.h"

BEGIN_DQ_RENDER_NAMESPACE

// ===========================================================================
// AttributeMapEntry
// (Ported from: itwinjs-core AttributeMap.ts AttributeMapEntry, line 25-53)
// ===========================================================================

AttributeMapEntry::AttributeMapEntry(std::vector<AttributeInfo> attributes)
{
    // Populate uninstanced map with the technique's base attributes.
    for (auto const& attr : attributes) {
        AttributeDetails details;
        details.location = attr.location;
        details.type = attr.type;
        m_uninstanced[attr.name] = details;
        m_instanced[attr.name] = details;
    }

    // Instance attributes are appended after the technique's base attributes.
    // Ported from: itwinjs-core AttributeMap.ts line 36-51
    struct InstanceAttr {
        std::string name;
        VariableType type;
    };
    static const InstanceAttr kInstanceAttrs[] = {
        {"a_instanceMatrixRow0", VariableType::Vec4},
        {"a_instanceMatrixRow1", VariableType::Vec4},
        {"a_instanceMatrixRow2", VariableType::Vec4},
        {"a_instanceOverrides", VariableType::Vec4},
        {"a_instanceRgba",     VariableType::Vec4},
        {"a_featureId",        VariableType::Vec3},
        {"a_patternX",         VariableType::Float},
        {"a_patternY",         VariableType::Float},
    };

    uint32_t location = static_cast<uint32_t>(attributes.size());
    for (auto const& attr : kInstanceAttrs) {
        AttributeDetails details;
        details.location = location;
        details.type = attr.type;
        m_instanced[attr.name] = details;
        ++location;
    }
}

AttributeDetails const* AttributeMapEntry::find(std::string const& name, bool instanced) const
{
    auto const& map = instanced ? m_instanced : m_uninstanced;
    auto it = map.find(name);
    return it != map.end() ? &it->second : nullptr;
}

// ===========================================================================
// AttributeMap — global singleton
// (Ported from: itwinjs-core AttributeMap.ts, line 60-133)
// ===========================================================================

AttributeMap::AttributeMap()
{
    // Default: position-only (used when techniqueId is not mapped)
    // Ported from: itwinjs-core AttributeMap.ts line 64
    m_defaultEntry = AttributeMapEntry({
        {"a_pos", 0, VariableType::Vec3},
    });

    // Sky sphere (gradient and texture)
    // Ported from: itwinjs-core AttributeMap.ts line 65-68
    AttributeMapEntry skySphere({
        {"a_pos",      0, VariableType::Vec3},
        {"a_worldPos", 1, VariableType::Vec3},
    });

    // Polyline
    // Ported from: itwinjs-core AttributeMap.ts line 69-74
    AttributeMapEntry polyline({
        {"a_pos",       0, VariableType::Vec3},
        {"a_prevIndex", 1, VariableType::Vec3},
        {"a_nextIndex", 2, VariableType::Vec3},
        {"a_param",     3, VariableType::Float},
    });

    // Edge
    // Ported from: itwinjs-core AttributeMap.ts line 75-78
    AttributeMapEntry edge({
        {"a_pos",                    0, VariableType::Vec3},
        {"a_endPointAndQuadIndices", 1, VariableType::Vec4},
    });

    // Silhouette edge
    // Ported from: itwinjs-core AttributeMap.ts line 79-83
    AttributeMapEntry silhouette({
        {"a_pos",                    0, VariableType::Vec3},
        {"a_endPointAndQuadIndices", 1, VariableType::Vec4},
        {"a_normals",                2, VariableType::Vec4},
    });

    // point cloud
    // Ported from: itwinjs-core AttributeMap.ts line 84-87
    AttributeMapEntry pointCloud({
        {"a_pos",   0, VariableType::Vec3},
        {"a_color", 1, VariableType::Vec3},
    });

    // Reality mesh
    // Ported from: itwinjs-core AttributeMap.ts line 88-92
    AttributeMapEntry realityMesh({
        {"a_pos",     0, VariableType::Vec3},
        {"a_norm",    1, VariableType::Vec2},
        {"a_uvParam", 2, VariableType::Vec2},
    });

    // Planar grid
    // Ported from: itwinjs-core AttributeMap.ts line 93-96
    AttributeMapEntry planarGrid({
        {"a_pos",     0, VariableType::Vec3},
        {"a_uvParam", 1, VariableType::Vec2},
    });

    // Screen points (volume classification copy Z)
    // Ported from: itwinjs-core AttributeMap.ts line 98-100
    AttributeMapEntry screenPoints({
        {"a_pos", 0, VariableType::Vec2},
    });

    // Register all technique-specific maps.
    // Ported from: itwinjs-core AttributeMap.ts line 102-113
    m_entries[TechniqueId::SkySphereGradient] = skySphere;
    m_entries[TechniqueId::SkySphereTexture] = skySphere;
    m_entries[TechniqueId::Polyline] = polyline;
    m_entries[TechniqueId::Edge] = edge;
    m_entries[TechniqueId::SilhouetteEdge] = silhouette;
    m_entries[TechniqueId::PointCloud] = pointCloud;
    m_entries[TechniqueId::VolClassCopyZ] = screenPoints;
    m_entries[TechniqueId::RealityMesh] = realityMesh;
    m_entries[TechniqueId::PlanarGrid] = planarGrid;
}

AttributeMap const& AttributeMap::getInstance()
{
    static AttributeMap const sInstance;
    return sInstance;
}

AttributeMapEntry const& AttributeMap::findAttributeMap(
    TechniqueId techniqueId,
    bool /*instanced*/)
{
    auto const& self = getInstance();
    auto it = self.m_entries.find(techniqueId);
    if (it != self.m_entries.end()) {
        return it->second;
    }
    // Default entry for unmapped techniques.
    // Ported from: itwinjs-core AttributeMap.ts line 117-123
    return self.m_defaultEntry;
}

AttributeDetails const* AttributeMap::findAttribute(
    std::string const& name,
    TechniqueId techniqueId,
    bool instanced)
{
    return findAttributeMap(techniqueId, instanced).find(name, instanced);
}

END_DQ_RENDER_NAMESPACE

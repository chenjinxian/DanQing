// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Texture mapping parameters
//
// Ported from: itwinjs-core core/common/src/TextureMapping.ts
// Describes how to map a texture image onto a surface.
#pragma once

#include "Export.h"
#include "MaterialProps.h"
#include "RenderTexture.h"
#include "DqCommon.h"

#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <dqBase/RefCounted.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

class TextureMapping;
class TextureMappingParams;

// A 2D point in texture space (x=u, y=v).
// Ported from: itwinjs-core core-geometry Point2d (surface used by TextureMapping.computeUVParams)
struct TextureUVPoint {
    double x = 0.0;
    double y = 0.0;
    TextureUVPoint() noexcept = default;
    TextureUVPoint(double x_, double y_) noexcept : x(x_), y(y_) {}
};

// A 2x3 matrix for mapping texture to surface.
// Ported from: itwinjs-core TextureMapping.Trans2x3
class DQ_COMMON_EXPORT TextureTrans2x3 {
public:
    dqGeom::Transform transform;

    // Construct from 2x3 matrix components.
    // Ported from: itwinjs-core TextureMapping.Trans2x3 constructor
    TextureTrans2x3(double m00 = 1, double m01 = 0, double originX = 0,
                    double m10 = 0, double m11 = 1, double originY = 0);

    // identity transform.
    static const TextureTrans2x3& identity() noexcept;

    // An [OrderedComparator] that compares this Trans2x3 against `other`.
    // Ported from: itwinjs-core TextureMapping.Trans2x3.compare()
    int compare(const TextureTrans2x3& other) const noexcept;
};

// Properties used to construct a ConstantLodParams.
// Ported from: itwinjs-core TextureMapping.ConstantLodParamProps
struct DQ_COMMON_EXPORT ConstantLodParamProps {
    int repetitions = 1;
    double offsetX = 0.0;
    double offsetY = 0.0;
    double minDistClamp = 1.0;
    double maxDistClamp = 4096.0 * 1024.0 * 1024.0;
};

// Parameters to define constant level-of-detail mapping.
// Ported from: itwinjs-core TextureMapping.ConstantLodParams
struct ConstantLodParams {
    int repetitions = 1;
    double offsetX = 0.0;
    double offsetY = 0.0;
    double minDistClamp = 1.0;
    double maxDistClamp = 4096.0 * 1024.0 * 1024.0;
};

// Defines normal map parameters.
// Ported from: itwinjs-core TextureMapping.NormalMapParams (interface lines 16-27)
struct DQ_COMMON_EXPORT NormalMapParams {
    // The texture to use as a normal map (non-owning; §5.1). If null, the pattern-map texture is used as a normal map.
    RenderTexture* normalMap = nullptr;
    // True if the Y component stored in the green channel should be negated. By default positive Y points downward.
    bool greenUp = false;
    // Scale factor by which to multiply the components of the extracted normal.
    std::optional<double> scale;
    // True to use constant LOD texture mapping for the normal-map texture.
    bool useConstantLod = false;
};

// Properties used to construct TextureMappingParams.
// Ported from: itwinjs-core TextureMapping.ParamProps (interface lines 174-192)
struct DQ_COMMON_EXPORT TextureMappingProps {
    std::optional<TextureTrans2x3> textureMat2x3;
    std::optional<double> textureWeight;
    std::optional<TextureMappingMode> mapMode;
    std::optional<bool> worldMapping;
    std::optional<bool> useConstantLod;
    std::optional<ConstantLodParamProps> constantLodProps;
};

// Texture mapping parameters.
// Ported from: itwinjs-core TextureMapping.Params
class DQ_COMMON_EXPORT TextureMappingParams {
public:
    TextureTrans2x3 textureMatrix;
    double weight = 1.0;
    TextureMappingMode mode = TextureMappingMode::Parametric;
    bool worldMapping = false;
    bool useConstantLod = false;
    ConstantLodParams constantLodParams;

    TextureMappingParams() = default;

    // Construct from props (applies ref defaults for unspecified fields).
    // Ported from: itwinjs-core TextureMapping.Params constructor
    explicit TextureMappingParams(const TextureMappingProps& props);

    // An [OrderedComparator] that compares these Params against `other`.
    // Ported from: itwinjs-core TextureMapping.Params.compare()
    int compare(const TextureMappingParams& other) const noexcept;

    // Compute texture coordinates for a polyface visitor.
    // Ported from: itwinjs-core TextureMapping.Params.computeUVParams()
    std::optional<std::vector<TextureUVPoint>> computeUVParams(
        class PolyfaceVisitor& visitor,
        const dqGeom::Transform& localToWorld) const;
    std::optional<std::vector<TextureUVPoint>> computeUVParams(class PolyfaceVisitor& visitor) const;
};

// PolyfaceVisitor — narrow abstract interface used by TextureMapping to read one facet of a mesh.
// Ported from: itwinjs-core core/geometry/src/polyface/Polyface.ts (PolyfaceVisitor interface, narrowed surface)
//               core/geometry/src/polyface/IndexedPolyfaceVisitor.ts (implementation reference)
// NOTE: The concrete implementation lives in dqGeom (out of scope for this port). This abstract
//       interface in dqCommon matches the ref's `PolyfaceVisitor` parameter type of computeUVParams.
class PolyfaceVisitor {
public:
    virtual ~PolyfaceVisitor() = default;

    // Number of edges (vertices) in the currently-loaded facet.
    virtual int numEdgesThisFacet() const noexcept = 0;

    // Vertex point (3D) at facet-relative index i.
    virtual dqGeom::Point3d point(int i) const = 0;

    // Vertex normal at facet-relative index i; nullopt if the mesh lacks normals.
    virtual std::optional<dqGeom::Vector3d> normal(int i) const = 0;

    // The shared normal index of the facet's first three vertices, used by planar-mode fast path
    // (ref line 252: `normalIndices[0] !== normalIndices[1]`). Returns nullopt if mesh lacks normals.
    virtual std::optional<int> normalIndex(int i) const = 0;

    // Distance parameter (UV-style) at facet-relative index i; nullopt if face data is absent.
    virtual std::optional<TextureUVPoint> tryGetDistanceParameter(int i) const = 0;

    // Normalized parameter (0..1) at facet-relative index i; nullopt if face data is absent.
    virtual std::optional<TextureUVPoint> tryGetNormalizedParameter(int i) const = 0;

    // The raw UV parameter stored on the vertex at facet-relative index i (ref: visitor.getParam).
    virtual TextureUVPoint getParam(int i) const = 0;
};

// Describes how to map a texture onto a surface.
// Ported from: itwinjs-core core/common/src/TextureMapping.ts (class TextureMapping, lines 38-71)
class DQ_COMMON_EXPORT TextureMapping {
public:
    using Mode = TextureMappingMode;
    using Trans2x3 = TextureTrans2x3;
    using Params = TextureMappingParams;

    // The texture to be mapped to the surface. (ref line 40)
    dqBase::RefPtr<RenderTexture> texture;
    // Parameters describing how the texture is mapped to the surface. (ref line 46)
    Params params;
    // Optional parameters for normal mapping. (ref line 44)
    std::optional<NormalMapParams> normalMapParams;

    // Construct from texture + params. (ref constructor lines 48-51)
    TextureMapping(dqBase::RefPtr<RenderTexture> tx, const Params& p)
        : texture(std::move(tx)), params(p) {}

    // Backward-compatible 1-arg constructor (preserved for existing consumers that wrap a
    // TextureMapping in std::optional without a texture handle).
    explicit TextureMapping(const Params& p) : texture(nullptr), params(p) {}
    TextureMapping() = default;

    // Compute texture coordinates for a polyface. (ref lines 53-60)
    std::optional<std::vector<TextureUVPoint>> computeUVParams(
        PolyfaceVisitor& visitor,
        const dqGeom::Transform& localToWorld) const
    {
        return params.computeUVParams(visitor, localToWorld);
    }
    std::optional<std::vector<TextureUVPoint>> computeUVParams(PolyfaceVisitor& visitor) const
    {
        return params.computeUVParams(visitor);
    }

    // An [OrderedComparator] that compares this mapping against `other`. (ref lines 63-70)
    int compare(const TextureMapping& other) const noexcept;
};

END_DQ_COMMON_NAMESPACE

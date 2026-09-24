// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — TextureMapping implementation
//
// Ported from: itwinjs-core core/common/src/TextureMapping.ts
#include "dqCommon/TextureMapping.h"
#include "dqCommon/RenderTexture.h"

#include <cmath>

BEGIN_DQ_COMMON_NAMESPACE

using namespace dqGeom;

// Ported from: itwinjs-core TextureMapping.Trans2x3 constructor (lines 112-116)
TextureTrans2x3::TextureTrans2x3(double m00, double m01, double originX,
                                  double m10, double m11, double originY)
{
    const auto origin = Point3d::From(originX, originY, 0.0);
    const auto matrix = Matrix3d::CreateRowValues(
        m00, m01, 0.0,
        m10, m11, 0.0,
        0.0, 0.0, 1.0);
    transform = Transform(origin, matrix);
}

// Ported from: itwinjs-core TextureMapping.Trans2x3.identity (line 119)
static const TextureTrans2x3 s_identity{};

const TextureTrans2x3& TextureTrans2x3::identity() noexcept
{
    return s_identity;
}

// Ported from: itwinjs-core TextureMapping.Trans2x3.compare() (lines 122-140)
int TextureTrans2x3::compare(const TextureTrans2x3& other) const noexcept
{
    if (this == &other)
        return 0;

    const auto& a = transform.GetMatrix();
    const auto& b = other.transform.GetMatrix();
    const auto& ao = transform.GetOrigin();
    const auto& bo = other.transform.GetOrigin();

    if (ao.x != bo.x)
        return ao.x < bo.x ? -1 : 1;
    if (ao.y != bo.y)
        return ao.y < bo.y ? -1 : 1;

    // row-major: coffs[0..8] = m00,m01,m02, m10,m11,m12, m20,m21,m22
    for (const int i : {0, 1, 3, 4}) {
        if (a.coffs[i] != b.coffs[i])
            return a.coffs[i] < b.coffs[i] ? -1 : 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// TextureMappingParams
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core TextureMapping.Params constructor (lines 210-222)
TextureMappingParams::TextureMappingParams(const TextureMappingProps& props)
{
    textureMatrix = props.textureMat2x3.value_or(TextureTrans2x3::identity());
    weight = props.textureWeight.value_or(1.0);
    mode = props.mapMode.value_or(TextureMappingMode::Parametric);
    worldMapping = props.worldMapping.value_or(false);
    useConstantLod = props.useConstantLod.value_or(false);
    if (props.constantLodProps) {
        constantLodParams.repetitions = props.constantLodProps->repetitions;
        constantLodParams.offsetX = props.constantLodProps->offsetX;
        constantLodParams.offsetY = props.constantLodProps->offsetY;
        constantLodParams.minDistClamp = props.constantLodProps->minDistClamp;
        constantLodParams.maxDistClamp = props.constantLodProps->maxDistClamp;
    }
}

// Ported from: itwinjs-core TextureMapping.Params.compare() (lines 225-233)
int TextureMappingParams::compare(const TextureMappingParams& other) const noexcept
{
    if (this == &other)
        return 0;
    if (weight != other.weight)
        return weight < other.weight ? -1 : 1;
    if (mode != other.mode)
        return static_cast<int>(mode) < static_cast<int>(other.mode) ? -1 : 1;
    if (worldMapping != other.worldMapping)
        return worldMapping ? 1 : -1;
    if (useConstantLod != other.useConstantLod)
        return useConstantLod ? 1 : -1;

    if (int d = textureMatrix.compare(other.textureMatrix); d != 0)
        return d;

    // compareConstantLodParams (ref lines 168-171)
    if (constantLodParams.repetitions != other.constantLodParams.repetitions)
        return constantLodParams.repetitions < other.constantLodParams.repetitions ? -1 : 1;
    if (constantLodParams.offsetX != other.constantLodParams.offsetX)
        return constantLodParams.offsetX < other.constantLodParams.offsetX ? -1 : 1;
    if (constantLodParams.offsetY != other.constantLodParams.offsetY)
        return constantLodParams.offsetY < other.constantLodParams.offsetY ? -1 : 1;
    if (constantLodParams.minDistClamp != other.constantLodParams.minDistClamp)
        return constantLodParams.minDistClamp < other.constantLodParams.minDistClamp ? -1 : 1;
    if (constantLodParams.maxDistClamp != other.constantLodParams.maxDistClamp)
        return constantLodParams.maxDistClamp < other.constantLodParams.maxDistClamp ? -1 : 1;
    return 0;
}

// Apply uvTransform to a 2D parameter. (ref Point2d.multiplyPoint2d equivalent)
static TextureUVPoint MultiplyUV(const Transform& t, const TextureUVPoint& p)
{
    const auto& m = t.GetMatrix();
    const auto& o = t.GetOrigin();
    // Transform applied to (p.x, p.y, 0):
    return {m.coffs[0] * p.x + m.coffs[1] * p.y + o.x,
            m.coffs[3] * p.x + m.coffs[4] * p.y + o.y};
}

// Ported from: itwinjs-core TextureMapping.Params.computeParametricUVParams (lines 265-280)
static std::vector<TextureUVPoint> ComputeParametricUVParams(
    PolyfaceVisitor& visitor, const Transform& uvTransform, bool isRelativeUnits)
{
    std::vector<TextureUVPoint> params;
    const int numEdges = visitor.numEdgesThisFacet();
    params.reserve(static_cast<size_t>(numEdges));
    for (int i = 0; i < numEdges; ++i) {
        TextureUVPoint param{0.0, 0.0};
        bool have = false;
        if (!isRelativeUnits) {
            auto dp = visitor.tryGetDistanceParameter(i);
            if (dp) { param = *dp; have = true; }
        }
        if (!have) {
            auto np = visitor.tryGetNormalizedParameter(i);
            if (np) { param = *np; }
            else { param = visitor.getParam(i); }
        }
        params.push_back(MultiplyUV(uvTransform, param));
    }
    return params;
}

// Ported from: itwinjs-core TextureMapping.Params.computePlanarUVParams (lines 283-328)
static std::optional<std::vector<TextureUVPoint>> ComputePlanarUVParams(
    PolyfaceVisitor& visitor, const Transform& uvTransform)
{
    std::vector<TextureUVPoint> params;
    Vector3d normal;
    auto n0 = visitor.normal(0);
    if (!n0) {
        // ref: points[0].crossProductToPoints(points[1], points[2]) == (p1-p0) x (p2-p0)
        const Point3d p0 = visitor.point(0);
        const Point3d p1 = visitor.point(1);
        const Point3d p2 = visitor.point(2);
        const Vector3d a{p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
        const Vector3d b{p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
        normal = Vector3d::FromCrossProduct(a, b);
    } else {
        normal = *n0;
    }

    const double normalMag = normal.Magnitude();
    if (normalMag <= 0.0)
        return std::nullopt;
    normal.x /= normalMag;
    normal.y /= normalMag;
    normal.z /= normalMag;

    // Flipping normal puts us in a planar coordinate system consistent with MicroStation.
    normal.x = -normal.x; normal.y = -normal.y; normal.z = -normal.z;

    // sideVector = (normal.y, -normal.x, 0)
    Vector3d sideVector{normal.y, -normal.x, 0.0};

    const double magnitude = sideVector.Magnitude();
    // sideVector.normalize(sideVector) (ref) — tolerated if failed; replaced below
    if (magnitude > 0.0) {
        sideVector.x /= magnitude;
        sideVector.y /= magnitude;
        sideVector.z /= magnitude;
    }

    if (magnitude < 1e-3) {
        normal.x = 0.0; normal.y = 0.0; normal.z = -1.0;
        sideVector.x = 1.0; sideVector.y = 0.0; sideVector.z = 0.0;
    }

    // upVector = sideVector x normal; ref: normalize, undefined → return undefined
    Vector3d upVector = Vector3d::FromCrossProduct(sideVector, normal);
    const double upMag = upVector.Magnitude();
    if (upMag <= 0.0)
        return std::nullopt;
    upVector.x /= upMag;
    upVector.y /= upMag;
    upVector.z /= upMag;

    const int numEdges = visitor.numEdgesThisFacet();
    params.reserve(static_cast<size_t>(numEdges));
    for (int i = 0; i < numEdges; ++i) {
        const Point3d p = visitor.point(i);
        const Vector3d v{p.x, p.y, p.z};
        const double u = v.x * sideVector.x + v.y * sideVector.y + v.z * sideVector.z;
        const double w = v.x * upVector.x + v.y * upVector.y + v.z * upVector.z;
        params.push_back(MultiplyUV(uvTransform, {u, w}));
    }
    return params;
}

// Ported from: itwinjs-core TextureMapping.Params.computeElevationDrapeUVParams (lines 331-344)
static std::vector<TextureUVPoint> ComputeElevationDrapeUVParams(
    PolyfaceVisitor& visitor, const Transform& uvTransform, const Transform* localToWorld)
{
    std::vector<TextureUVPoint> params;
    const int numEdges = visitor.numEdgesThisFacet();
    params.reserve(static_cast<size_t>(numEdges));
    for (int i = 0; i < numEdges; ++i) {
        Point3d p = visitor.point(i);
        if (localToWorld) {
            p = localToWorld->MultiplyPoint3d(p);
        }
        params.push_back(MultiplyUV(uvTransform, {p.x, p.y}));
    }
    return params;
}

// Ported from: itwinjs-core TextureMapping.Params.computeUVParams (lines 240-262)
std::optional<std::vector<TextureUVPoint>> TextureMappingParams::computeUVParams(
    PolyfaceVisitor& visitor, const Transform& localToWorld) const
{
    const Transform& uv = textureMatrix.transform;
    switch (mode) {
        default:  // Fall through to parametric in default case
        case TextureMappingMode::Parametric:
            return ComputeParametricUVParams(visitor, uv, !worldMapping);
        case TextureMappingMode::Planar: {
            // Ignore planar mode unless world-mapping (master/sub units) and facet is planar.
            auto ni0 = visitor.normalIndex(0);
            auto ni1 = visitor.normalIndex(1);
            auto ni2 = visitor.normalIndex(2);
            const bool planar = ni0 && ni1 && ni2 && (*ni0 == *ni1) && (*ni0 == *ni2);
            if (!worldMapping || !planar) {
                return ComputeParametricUVParams(visitor, uv, !worldMapping);
            }
            return ComputePlanarUVParams(visitor, uv);
        }
        case TextureMappingMode::ElevationDrape:
            return ComputeElevationDrapeUVParams(visitor, uv, &localToWorld);
    }
}

std::optional<std::vector<TextureUVPoint>> TextureMappingParams::computeUVParams(
    PolyfaceVisitor& visitor) const
{
    return computeUVParams(visitor, Transform::CreateIdentity());
}

// ---------------------------------------------------------------------------
// TextureMapping
// ---------------------------------------------------------------------------

namespace {
// comparePossiblyUndefined for normalMap (RenderTexture) — ref line 69
// (raw non-owning pointers, §5.1; normalMap is a non-owning back-reference)
int CompareTexturePtr(const RenderTexture* a, const RenderTexture* b) noexcept
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return a->compare(*b);
}

// compareNormalMapParams (ref lines 29-33)
int CompareNormalMapParams(const NormalMapParams& lhs, const NormalMapParams& rhs) noexcept
{
    if (int d = CompareTexturePtr(lhs.normalMap, rhs.normalMap); d != 0) return d;
    if (lhs.greenUp != rhs.greenUp) return lhs.greenUp ? 1 : -1;
    // compareNumbersOrUndefined(scale)
    if (!lhs.scale.has_value() && !rhs.scale.has_value()) return 0;
    if (!lhs.scale.has_value()) return -1;
    if (!rhs.scale.has_value()) return 1;
    if (*lhs.scale != *rhs.scale) return *lhs.scale < *rhs.scale ? -1 : 1;
    if (lhs.useConstantLod != rhs.useConstantLod) return lhs.useConstantLod ? 1 : -1;
    return 0;
}
}  // namespace

// Ported from: itwinjs-core TextureMapping.compare() (lines 63-70)
int TextureMapping::compare(const TextureMapping& other) const noexcept
{
    if (this == &other)
        return 0;

    // texture comparison
    if (!texture && other.texture) return -1;
    if (texture && !other.texture) return 1;
    if (texture && other.texture) {
        if (int d = texture->compare(*other.texture); d != 0) return d;
    }

    if (int d = params.compare(other.params); d != 0) return d;

    // comparePossiblyUndefined(compareNormalMapParams, this.normalMapParams, other.normalMapParams)
    if (!normalMapParams.has_value() && !other.normalMapParams.has_value()) return 0;
    if (!normalMapParams.has_value()) return -1;
    if (!other.normalMapParams.has_value()) return 1;
    return CompareNormalMapParams(*normalMapParams, *other.normalMapParams);
}

END_DQ_COMMON_NAMESPACE

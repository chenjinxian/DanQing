// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Viewport quad geometry implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/CachedGeometry.ts
#include "ViewportQuadGeometry.h"
#include "CompositeTechniques.h"
#include "dqRender/rhi/DriverEnums.h"

#include <cmath>

#include "gl/GL.h"  // glDisable (sky pass force-disables depth test + cull)

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// ComputeSkySphereWorldPosAndEye — see header. Npc corner indices: 0=LBR,
// 1=RBR, 2=LTR, 3=RTR, 4=LBF, 5=RBF, 6=LTF, 7=RTF (Npc.h, ported from itwinjs).
// ---------------------------------------------------------------------------
void ComputeSkySphereWorldPosAndEye(dqCommon::Frustum const& f,
                                    float worldPos[12], float worldEye[3]) noexcept
{
    // 4 mid-depth WORLD corners (non-globe path, CachedGeometry.ts:583-596):
    // each = midpoint of its Rear and Front corners.
    auto const& lbr = f.getCorner(0);  // LeftBottomRear
    auto const& rbr = f.getCorner(1);  // RightBottomRear
    auto const& ltr = f.getCorner(2);  // LeftTopRear
    auto const& rtr = f.getCorner(3);  // RightTopRear
    auto const& lbf = f.getCorner(4);  // LeftBottomFront
    auto const& rbf = f.getCorner(5);  // RightBottomFront
    auto const& ltf = f.getCorner(6);  // LeftTopFront
    auto const& rtf = f.getCorner(7);  // RightTopFront

    dqGeom::Point3d const lb = dqGeom::Point3d::FromInterpolate(lbr, 0.5, lbf);
    dqGeom::Point3d const rb = dqGeom::Point3d::FromInterpolate(rbr, 0.5, rbf);
    dqGeom::Point3d const rt = dqGeom::Point3d::FromInterpolate(rtr, 0.5, rtf);
    dqGeom::Point3d const lt = dqGeom::Point3d::FromInterpolate(ltr, 0.5, ltf);

    worldPos[0] = static_cast<float>(lb.x);
    worldPos[1] = static_cast<float>(lb.y);
    worldPos[2] = static_cast<float>(lb.z);
    worldPos[3] = static_cast<float>(rb.x);
    worldPos[4] = static_cast<float>(rb.y);
    worldPos[5] = static_cast<float>(rb.z);
    worldPos[6] = static_cast<float>(rt.x);
    worldPos[7] = static_cast<float>(rt.y);
    worldPos[8] = static_cast<float>(rt.z);
    worldPos[9] = static_cast<float>(lt.x);
    worldPos[10] = static_cast<float>(lt.y);
    worldPos[11] = static_cast<float>(lt.z);

    // u_worldEye — orthographic pseudo-camera (SkySphere.ts:251-264).
    // delta = LeftBottomFront - LeftBottomRear (rear→front); diagonal = |LBR→RTR|;
    // focalLength = diagonal / (2·atan(22.5°)); zScale = max(focalLength/|delta|, 1.000001);
    // worldEye = rearCenter + delta·zScale, rearCenter = (LBR + RTR)/2.
    // NB: the reference uses Math.ATAN (SkySphere.ts:255 `Math.atan(pseudoCameraHalfAngle *
    // Angle.radiansPerDegree)`), not tan — ported verbatim (reference is spec).
    dqGeom::Vector3d const delta = dqGeom::Vector3d::FromStartEnd(lbr, lbf);
    double const diagonal = lbr.Distance(rtr);
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kPseudoCameraHalfAngleDeg = 22.5;
    double const focalLength = diagonal / (2.0 * std::atan(kPseudoCameraHalfAngleDeg * kPi / 180.0));
    double const deltaMag = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    double zScale = (deltaMag > 1e-12) ? focalLength / deltaMag : 1.000001;
    if (zScale < 1.000001)
        zScale = 1.000001;  // keep worldEye in front of the rear plane (prevent divide blowup)
    dqGeom::Point3d const rearCenter = dqGeom::Point3d::From(
        (lbr.x + rtr.x) * 0.5, (lbr.y + rtr.y) * 0.5, (lbr.z + rtr.z) * 0.5);
    worldEye[0] = static_cast<float>(rearCenter.x + delta.x * zScale);
    worldEye[1] = static_cast<float>(rearCenter.y + delta.y * zScale);
    worldEye[2] = static_cast<float>(rearCenter.z + delta.z * zScale);
}

// ---------------------------------------------------------------------------
// ComputeSkySphereWorldPosAndEye (globe-mode 重载)
// Ported from: itwinjs-core CachedGeometry.ts _setPointsFromFrustum 的
//              globe 分支（:597-651）+ SkySphere.ts u_worldEye（:237-265）。
// ---------------------------------------------------------------------------
void ComputeSkySphereWorldPosAndEye(dqCommon::Frustum const& f,
                                    float worldPos[12], float worldEye[3],
                                    SkySphereGlobeParams const& globe) noexcept
{
    if (!globe.isGlobeMode3D) {
        ComputeSkySphereWorldPosAndEye(f, worldPos, worldEye);
        return;
    }

    auto const& lbr = f.getCorner(0);  // LeftBottomRear
    auto const& rbr = f.getCorner(1);  // RightBottomRear
    auto const& rtr = f.getCorner(3);  // RightTopRear
    auto const& lbf = f.getCorner(4);  // LeftBottomFront

    // 真视锥的 4 个中深角点（CachedGeometry.ts:580-582 同式）——globe 分支只取
    // lb/rb/rt 三点；lt 在分支末重组（故这里先取 lb/rb/rt 即可）。
    dqGeom::Point3d const lb = dqGeom::Point3d::FromInterpolate(lbr, 0.5, lbf);
    dqGeom::Point3d const rb = dqGeom::Point3d::FromInterpolate(rbr, 0.5, f.getCorner(5));
    dqGeom::Point3d const rt = dqGeom::Point3d::FromInterpolate(rtr, 0.5, f.getCorner(7));

    // :598-604 — fCenter（真视锥中心）+ upScreen/rightScreen 与半宽半高。
    dqGeom::Point3d fCenter = dqGeom::Point3d::FromInterpolate(lb, 0.5, rt);
    auto upScreen = dqGeom::Vector3d::FromStartEnd(rb, rt);
    auto rightScreen = dqGeom::Vector3d::FromStartEnd(lb, rb);
    double const halfWidth = upScreen.Magnitude() * 0.5;
    double const halfHeight = rightScreen.Magnitude() * 0.5;
    upScreen.Normalize();
    rightScreen.Normalize();

    // :608-609 — projUp/projRt：global-up 向真视锥 up/right 的投影分量。
    double const projUp = globe.upVector.DotProduct(upScreen);
    double const projRt = globe.upVector.DotProduct(rightScreen);

    // camPos（:611-627）：透视用真实相机位（:612-617），正交用 22.5° 伪相机（:618-626）。
    dqGeom::Point3d camPos;
    if (globe.perspective) {
        // :612-617 — camPos = LBR + (LBF-LBR) · 1/(1-planFraction)
        double const scale = 1.0 / (1.0 - globe.planFraction);
        camPos = dqGeom::Point3d::From(
            lbr.x + (lbf.x - lbr.x) * scale,
            lbr.y + (lbf.y - lbr.y) * scale,
            lbr.z + (lbf.z - lbr.z) * scale);
    } else {
        // :618-626 — 与非 globe 路径同式（SkySphere.ts:251-264；Math.atan 保真）。
        dqGeom::Vector3d const delta = dqGeom::Vector3d::FromStartEnd(lbr, lbf);
        double const diagonal = lbr.Distance(rtr);
        constexpr double kPi = 3.14159265358979323846;
        constexpr double kPseudoCameraHalfAngleDeg = 22.5;
        double const focalLength = diagonal / (2.0 * std::atan(kPseudoCameraHalfAngleDeg * kPi / 180.0));
        double const deltaMag = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        double zScale = (deltaMag > 1e-12) ? focalLength / deltaMag : 1.000001;
        if (zScale < 1.000001)
            zScale = 1.000001;
        camPos = dqGeom::Point3d::From(
            (lbr.x + rtr.x) * 0.5 + delta.x * zScale,
            (lbr.y + rtr.y) * 0.5 + delta.y * zScale,
            (lbr.z + rtr.z) * 0.5 + delta.z * zScale);
    }

    // u_worldEye = camPos（u_worldEye 的供值与 globe 与否无关，SkySphere.ts:237-265）。
    worldEye[0] = static_cast<float>(camPos.x);
    worldEye[1] = static_cast<float>(camPos.y);
    worldEye[2] = static_cast<float>(camPos.z);

    // :628-631 — camDist + 固定 camDir=(0,1,0)：fCenter 重定位到 camPos + camDir·camDist。
    double const camDist = camPos.Distance(fCenter);
    fCenter = dqGeom::Point3d::From(camPos.x, camPos.y + camDist, camPos.z);

    // :633-636 — upScreen = normalize(projRt, 0, projUp)；rightScreen = upScreen × camDir。
    // 参考语义（Vector3d.normalizeInPlace / normalizeWithLength，Point3dVector3d.ts:970）：零向量
    // 保持为零（safeDivideOrNull(0)→原样），**不得**用 dqGeom::Normalize 的 (1,0,0) 兜底——
    // 非地理定位（upVector=unitZ）+ Top/Bottom 视图下 (projRt,0,projUp) 恰为 0，参考于此
    // 退化为单点 → 全屏均匀色（itwinjs DTA 的 Top/Bottom 均匀浅蓝正是此情形）。
    auto up2 = dqGeom::Vector3d::From(projRt, 0.0, projUp);
    {
        double const m = up2.Magnitude();
        if (m > 1.0e-12) {
            double const inv = 1.0 / m;
            up2 = dqGeom::Vector3d::From(up2.x * inv, up2.y * inv, up2.z * inv);
        }  // else: 保持零向量（参考行为）
    }
    // up2 × (0,1,0) = (uy·0-uz·1, uz·0-ux·0, ux·1-uy·0) = (-up2.z, 0, up2.x)
    auto right2 = dqGeom::Vector3d::From(-up2.z, 0.0, up2.x);

    // :637-638 — 半高半宽缩放。
    up2 = dqGeom::Vector3d::From(up2.x * halfHeight, up2.y * halfHeight, up2.z * halfHeight);
    right2 = dqGeom::Vector3d::From(right2.x * halfWidth, right2.y * halfWidth, right2.z * halfWidth);

    // :639-650 — 4 角点：fCenter ± right2 ± up2（LeftBottom, RightBottom, RightTop, LeftTop）。
    worldPos[0]  = static_cast<float>(fCenter.x - right2.x - up2.x);
    worldPos[1]  = static_cast<float>(fCenter.y - right2.y - up2.y);
    worldPos[2]  = static_cast<float>(fCenter.z - right2.z - up2.z);
    worldPos[3]  = static_cast<float>(fCenter.x + right2.x - up2.x);
    worldPos[4]  = static_cast<float>(fCenter.y + right2.y - up2.y);
    worldPos[5]  = static_cast<float>(fCenter.z + right2.z - up2.z);
    worldPos[6]  = static_cast<float>(fCenter.x + right2.x + up2.x);
    worldPos[7]  = static_cast<float>(fCenter.y + right2.y + up2.y);
    worldPos[8]  = static_cast<float>(fCenter.z + right2.z + up2.z);
    worldPos[9]  = static_cast<float>(fCenter.x - right2.x + up2.x);
    worldPos[10] = static_cast<float>(fCenter.y - right2.y + up2.y);
    worldPos[11] = static_cast<float>(fCenter.z - right2.z + up2.z);
}

// ===========================================================================
// ViewportQuad helper — builds the full-screen quad vertices + indices
// Ported from: itwinjs-core CachedGeometry.ts ViewportQuad (line 456-498)
// ===========================================================================

namespace {

// quantize a float in [-1, 1] to uint16 [0, 65535].
// Matches QParams3d.fromNormalizedRange() quantization.
inline uint16_t quantizeNormalized(float v) {
    return static_cast<uint16_t>((v * 0.5f + 0.5f) * 65535.0f + 0.5f);
}

// CPU-side mesh data for the full-screen quad.
// Ported from: itwinjs-core CachedGeometry.ts ViewportQuad constructor
struct ViewportQuadMeshData {
    // 4 vertices × 3 components × uint16 = 24 bytes
    std::array<uint16_t, 12> positions = {
        // Vertex 0: (-1, -1, 0) bottom-left
        quantizeNormalized(-1.0f), quantizeNormalized(-1.0f), quantizeNormalized(0.0f),
        // Vertex 1: ( 1, -1, 0) bottom-right
        quantizeNormalized( 1.0f), quantizeNormalized(-1.0f), quantizeNormalized(0.0f),
        // Vertex 2: ( 1,  1, 0) top-right
        quantizeNormalized( 1.0f), quantizeNormalized( 1.0f), quantizeNormalized(0.0f),
        // Vertex 3: (-1,  1, 0) top-left
        quantizeNormalized(-1.0f), quantizeNormalized( 1.0f), quantizeNormalized(0.0f),
    };

    // 6 indices × uint32 = 24 bytes
    std::array<uint32_t, 6> indices = {0, 1, 2, 0, 2, 3};

    // Quantization params: origin = (-1,-1,0), scale = (2/65535, 2/65535, 2/65535)
    std::array<float, 3> qOrigin = {-1.0f, -1.0f, 0.0f};
    std::array<float, 3> qScale = {2.0f / 65535.0f, 2.0f / 65535.0f, 2.0f / 65535.0f};
};

static ViewportQuadMeshData const sViewportQuadMesh;

// CPU-side mesh data for the skybox cube.
// Ported from: itwinjs-core CachedGeometry.ts SkyBoxQuads constructor (line 297-356)
// 36 vertices (6 faces × 2 triangles × 3 vertices), no index buffer.
struct SkyBoxMeshData {
    // 8 unique corner positions
    static constexpr float corners[8][3] = {
        {-1,  1,  1}, { 1,  1,  1}, {-1, -1,  1}, { 1, -1,  1},
        {-1,  1, -1}, { 1,  1, -1}, {-1, -1, -1}, { 1, -1, -1},
    };

    // 36 vertex indices into corners[], forming 12 triangles (6 faces × 2)
    // Ported from: itwinjs-core CachedGeometry.ts SkyBoxQuads face winding
    static constexpr uint8_t faceIndices[36] = {
        // Back face (Z=+1)
        0, 1, 2,  1, 3, 2,
        // Front face (Z=-1)
        4, 5, 6,  5, 7, 6,
        // Top face (Y=+1)
        4, 5, 1,  4, 0, 1,
        // Bottom face (Y=-1)
        2, 3, 6,  3, 7, 6,
        // Left face (X=-1)
        0, 4, 2,  4, 6, 2,
        // Right face (X=+1)
        1, 5, 3,  5, 7, 3,
    };

    // Quantized positions: 36 vertices × 3 components × uint16
    std::array<uint16_t, 36 * 3> positions;

    SkyBoxMeshData() {
        for (int i = 0; i < 36; ++i) {
            auto const* c = corners[faceIndices[i]];
            positions[i * 3 + 0] = quantizeNormalized(c[0]);
            positions[i * 3 + 1] = quantizeNormalized(c[1]);
            positions[i * 3 + 2] = quantizeNormalized(c[2]);
        }
    }

    std::array<float, 3> qOrigin = {-1.0f, -1.0f, -1.0f};
    std::array<float, 3> qScale = {2.0f / 65535.0f, 2.0f / 65535.0f, 2.0f / 65535.0f};
};

static SkyBoxMeshData const sSkyBoxMesh;

}  // anonymous namespace

// ===========================================================================
// ViewportQuadGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 503-522)
// ===========================================================================

ViewportQuadGeometry::ViewportQuadGeometry(IndexedGeometryParams params, TechniqueId techniqueId)
    : IndexedGeometry(std::move(params))
    , m_techniqueId(techniqueId)
{
}

ViewportQuadGeometry* ViewportQuadGeometry::create(TechniqueId techniqueId) {
    // Ported from: itwinjs-core CachedGeometry.ts line 510-513
    // Uses the shared viewport quad mesh (CPU-side data).
    // GPU resources are created lazily on first draw().
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new ViewportQuadGeometry(std::move(params), techniqueId);
}

// ===========================================================================
// TexturedViewportQuadGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 527-546)
// ===========================================================================

TexturedViewportQuadGeometry::TexturedViewportQuadGeometry(
    IndexedGeometryParams params,
    TechniqueId techniqueId,
    std::vector<rhi::TextureHandle> textures)
    : ViewportQuadGeometry(std::move(params), techniqueId)
    , m_textures(std::move(textures))
{
}

TexturedViewportQuadGeometry* TexturedViewportQuadGeometry::create(
    TechniqueId techniqueId,
    std::vector<rhi::TextureHandle> textures) {
    // Ported from: itwinjs-core CachedGeometry.ts line 539-546
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new TexturedViewportQuadGeometry(std::move(params), techniqueId, std::move(textures));
}

// ===========================================================================
// SkySphereViewportQuadGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 551-735)
// ===========================================================================

SkySphereViewportQuadGeometry::SkySphereViewportQuadGeometry(
    IndexedGeometryParams params,
    RenderSkySphereParams const& skybox)
    : ViewportQuadGeometry(std::move(params), TechniqueId::SkySphereTexture)
    , m_zOffset(skybox.zOffset)
    , m_rotation(skybox.rotation)
    , m_skyTexture(skybox.texture)
{
    m_typeAndExponents = {0.0f, 1.0f, 1.0f};
    // For texture skybox, colors are zeroed.
    m_colors = {};
}

SkySphereViewportQuadGeometry::SkySphereViewportQuadGeometry(
    IndexedGeometryParams params,
    RenderSkyGradientParams const& skybox)
    : ViewportQuadGeometry(std::move(params), TechniqueId::SkySphereGradient)
    , m_zOffset(skybox.zOffset)
{
    // Ported from: itwinjs-core CachedGeometry.ts:686-716 (gradient branch).
    // zenith/nadir always come from the SkyGradient; the twoColor flag selects
    // 2-color (typeAndExponents={-1,4,4}, sky/ground=0) vs 4-color
    // (typeAndExponents={1, skyExp, groundExp}, sky/ground from the gradient).
    auto const& gradient = skybox.gradient;
    auto toRgb = [](dqCommon::ColorDef c, std::array<float, 3>& out) {
        auto const cc = dqCommon::ColorDef::getColors(c.getTbgr());
        out[0] = static_cast<float>(cc.r) / 255.0f;
        out[1] = static_cast<float>(cc.g) / 255.0f;
        out[2] = static_cast<float>(cc.b) / 255.0f;
    };

    toRgb(gradient.zenithColor, m_colors.zenith);  // CachedGeometry.ts:689-691
    toRgb(gradient.nadirColor, m_colors.nadir);     // CachedGeometry.ts:692-694

    if (gradient.twoColor) {                        // CachedGeometry.ts:696-705
        m_typeAndExponents = {-1.0f, 4.0f, 4.0f};
        m_colors.sky = {0.0f, 0.0f, 0.0f};
        m_colors.ground = {0.0f, 0.0f, 0.0f};
    } else {                                        // CachedGeometry.ts:706-715
        m_typeAndExponents = {1.0f,
                              static_cast<float>(gradient.skyExponent),
                              static_cast<float>(gradient.groundExponent)};
        toRgb(gradient.skyColor, m_colors.sky);
        toRgb(gradient.groundColor, m_colors.ground);
    }

    // Ported from: itwinjs-core SkySphere.ts:145-180. When the background map is
    // on, the map-facing sky stops (ground/nadir) are replaced by skyColor (4-color)
    // or zenithColor (2-color ground) - the lower hemisphere is painted sky-blue so
    // a top-down blank-connection view reads flat light-blue (142,205,255), not the
    // nadir dark-green. (globeMode3D modulation by (1-globalViewTransition) is a
    // sub-global no-op for the blank view - Phase 4.)
    if (skybox.backgroundMapOn) {
        if (gradient.twoColor) {
            m_colors.ground = m_colors.zenith;       // SkySphere.ts:166-167 (2-color ground -> zenith)
        } else {
            m_colors.ground = m_colors.sky;           // SkySphere.ts:152-153 (4-color ground -> sky)
            m_colors.nadir = m_colors.sky;            // SkySphere.ts:176-177 (4-color nadir -> sky)
        }
    }
}

SkySphereViewportQuadGeometry* SkySphereViewportQuadGeometry::createGeometry(
    RenderSkySphereParams const& skybox) {
    // Ported from: itwinjs-core CachedGeometry.ts line 720-723
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new SkySphereViewportQuadGeometry(std::move(params), skybox);
}

SkySphereViewportQuadGeometry* SkySphereViewportQuadGeometry::createGeometry(
    RenderSkyGradientParams const& skybox) {
    // Ported from: itwinjs-core CachedGeometry.ts line 725-728
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new SkySphereViewportQuadGeometry(std::move(params), skybox);
}

void SkySphereViewportQuadGeometry::setWorldPosAndEye(float const worldPos[12],
                                                       float const worldEye[3]) noexcept
{
    for (int i = 0; i < 12; ++i)
        m_worldPos[i] = worldPos[i];
    for (int i = 0; i < 3; ++i)
        m_worldEye[i] = worldEye[i];
}

void SkySphereViewportQuadGeometry::draw(rhi::Driver& driver) {
    // Lazily upload the full-screen quad on first draw (the viewport-quad
    // hierarchy's "lazy GPU upload" was otherwise unimplemented → null primitive
    // → sky never rendered). Ported from: itwinjs-core CachedGeometry.ts
    // ViewportQuad (line 503-522) + SkySphereViewportQuadGeometry worldPos buffer.
    if (!m_skyUploaded) {
        m_skyUploaded = true;
        // a_position (location 0): four NDC corners (±1, ±1, 0). The gradient
        // shader forces gl_Position.z to the far plane via the `.xyww` trick.
        // Ordered for TRIANGLE_STRIP.
        static float const kPositions[4 * 3] = {
            -1.0f, -1.0f, 0.0f,
             1.0f, -1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f, 0.0f,
        };
        // Two buffers: 0 = a_position (static NDC), 1 = a_worldPos (per-frame
        // world corners, drives the gradient's elevation angle). Mirrors
        // itwinjs SkySphereViewportQuadGeometry's separate worldPos buffer.
        rhi::AttributeArray attrs = {};
        attrs[0].buffer = 0;
        attrs[0].offset = 0;
        attrs[0].type = rhi::ElementType::FLOAT3;
        attrs[1].buffer = 1;
        attrs[1].offset = 0;
        attrs[1].type = rhi::ElementType::FLOAT3;
        m_skyVbih = driver.createVertexBufferInfo(2, 2, attrs);
        m_skyVbh = driver.createVertexBuffer(4, m_skyVbih);

        m_skyVbo = driver.createBufferObject(
            sizeof(kPositions), rhi::BufferObjectBinding::VERTEX, rhi::BufferUsage::STATIC);
        rhi::BufferDescriptor vboData(kPositions, sizeof(kPositions));
        driver.updateBufferObject(m_skyVbo, std::move(vboData), 0);
        driver.setVertexBufferObject(m_skyVbh, 0, m_skyVbo);

        m_skyWorldVbo = driver.createBufferObject(
            static_cast<uint32_t>(m_worldPos.size() * sizeof(float)),
            rhi::BufferObjectBinding::VERTEX, rhi::BufferUsage::DYNAMIC);
        driver.setVertexBufferObject(m_skyVbh, 1, m_skyWorldVbo);

        m_skyIbh = driver.createIndexBuffer(rhi::ElementType::USHORT, 0, rhi::BufferUsage::STATIC);
        m_skyPrimitive = driver.createRenderPrimitive(
            m_skyVbh, m_skyIbh, rhi::PrimitiveType::TRIANGLE_STRIP);
    }
    // Update a_worldPos each draw — the camera frustum (hence m_worldPos) changes
    // as the view moves. In-place buffer update (DYNAMIC); handles are stable.
    if (m_skyWorldVbo) {
        rhi::BufferDescriptor worldData(m_worldPos.data(), m_worldPos.size() * sizeof(float));
        driver.updateBufferObject(m_skyWorldVbo, std::move(worldData), 0);
    }
    if (m_skyPrimitive) {
        // SkySphereGradient uses pos.xyww -> gl_Position.z = w = 1 (far plane).
        // With depthTest on + GL_LESS + depth cleared to 1.0, every sky fragment
        // fails -> white. itwinjs draws the sky depth-off. Force-disable depth
        // test + cull RIGHT before drawArrays (the render-state tracker is
        // desync'd, so pass-level applyRenderState's diff leaves them on).
        // 2026-09-16 修复：此前裸关 GL_DEPTH_TEST 后从不恢复——天空画在不透明
        // pass 之前，后续 opaque pass 的 applyRenderState diff 误判"深度仍开"
        // 跳过 glEnable → 整个不透明 pass 深度测试实际失效（纯画序）。glTF 立方
        // 体是首个"视线重叠不透明体"场景：顶视图最后画的 Z− 面画序胜出，贴图呈
        // 水平镜像（用户实测 vs DTA 左右反转）；X/Y 面对布局相同故侧视图看似正
        // 确。状态中立：保存/恢复（GLCanvasContext::flush 同款，参考机制 =
        // itwinjs 经 RenderState 走状态跟踪器）。
        GLboolean const savedDepthTest = glIsEnabled(GL_DEPTH_TEST);
        GLboolean const savedCullFace = glIsEnabled(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        driver.bindRenderPrimitive(m_skyPrimitive);
        driver.drawArrays(0, 4, 1);
        if (savedDepthTest == GL_TRUE) glEnable(GL_DEPTH_TEST);
        if (savedCullFace == GL_TRUE) glEnable(GL_CULL_FACE);
    }

}

// ===========================================================================
// AmbientOcclusionGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 740-758)
// ===========================================================================

AmbientOcclusionGeometry::AmbientOcclusionGeometry(
    IndexedGeometryParams params,
    rhi::TextureHandle depthAndOrder,
    rhi::TextureHandle depth)
    : TexturedViewportQuadGeometry(std::move(params), TechniqueId::AmbientOcclusion,
                                    {depth, depthAndOrder})
{
}

AmbientOcclusionGeometry* AmbientOcclusionGeometry::createGeometry(
    rhi::TextureHandle depthAndOrder, rhi::TextureHandle depth) {
    // Ported from: itwinjs-core CachedGeometry.ts line 741-744
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new AmbientOcclusionGeometry(std::move(params), depthAndOrder, depth);
}

// ===========================================================================
// BlurGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 767-789)
// ===========================================================================

BlurGeometry::BlurGeometry(
    IndexedGeometryParams params,
    std::vector<rhi::TextureHandle> textures,
    float blurDirX, float blurDirY,
    BlurType blurType)
    : TexturedViewportQuadGeometry(
          std::move(params),
          blurType == BlurType::NoTest ? TechniqueId::Blur : TechniqueId::BlurTestOrder,
          std::move(textures))
    , m_blurDirX(blurDirX)
    , m_blurDirY(blurDirY)
{
}

BlurGeometry* BlurGeometry::createGeometry(
    rhi::TextureHandle texToBlur,
    rhi::TextureHandle depthAndOrder,
    rhi::TextureHandle depthAndOrderHidden,
    float blurDirX, float blurDirY,
    BlurType blurType) {
    // Ported from: itwinjs-core CachedGeometry.ts line 770-778
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    std::vector<rhi::TextureHandle> textures = {texToBlur, depthAndOrder, depthAndOrderHidden};
    return new BlurGeometry(std::move(params), std::move(textures),
                            blurDirX, blurDirY, blurType);
}

// ===========================================================================
// EDLCalcBasicGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 792-812)
// ===========================================================================

EDLCalcBasicGeometry::EDLCalcBasicGeometry(
    IndexedGeometryParams params,
    rhi::TextureHandle colorBuffer,
    rhi::TextureHandle depthBuffer,
    float width, float height)
    : TexturedViewportQuadGeometry(std::move(params), TechniqueId::EDLCalcBasic,
                                    {colorBuffer, depthBuffer})
{
    m_texInfo[0] = 1.0f / width;
    m_texInfo[1] = 1.0f / height;
    m_texInfo[2] = 1.0f;
}

EDLCalcBasicGeometry* EDLCalcBasicGeometry::createGeometry(
    rhi::TextureHandle colorBuffer, rhi::TextureHandle depthBuffer,
    float width, float height) {
    // Ported from: itwinjs-core CachedGeometry.ts line 795-798
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new EDLCalcBasicGeometry(std::move(params), colorBuffer, depthBuffer, width, height);
}

// ===========================================================================
// EDLCalcFullGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 815-835)
// ===========================================================================

EDLCalcFullGeometry::EDLCalcFullGeometry(
    IndexedGeometryParams params,
    rhi::TextureHandle colorBuffer,
    rhi::TextureHandle depthBuffer,
    float scale, float width, float height)
    : TexturedViewportQuadGeometry(std::move(params), TechniqueId::EDLCalcFull,
                                    {colorBuffer, depthBuffer})
{
    m_texInfo[0] = 1.0f / width;
    m_texInfo[1] = 1.0f / height;
    m_texInfo[2] = scale;
}

EDLCalcFullGeometry* EDLCalcFullGeometry::createGeometry(
    rhi::TextureHandle colorBuffer, rhi::TextureHandle depthBuffer,
    float scale, float width, float height) {
    // Ported from: itwinjs-core CachedGeometry.ts line 818-821
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new EDLCalcFullGeometry(std::move(params), colorBuffer, depthBuffer,
                                   scale, width, height);
}

// ===========================================================================
// EDLFilterGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 838-858)
// ===========================================================================

EDLFilterGeometry::EDLFilterGeometry(
    IndexedGeometryParams params,
    rhi::TextureHandle colorBuffer,
    rhi::TextureHandle depthBuffer,
    float scale, float width, float height)
    : TexturedViewportQuadGeometry(std::move(params), TechniqueId::EDLFilter,
                                    {colorBuffer, depthBuffer})
{
    m_texInfo[0] = 1.0f / width;
    m_texInfo[1] = 1.0f / height;
    m_texInfo[2] = scale;
}

EDLFilterGeometry* EDLFilterGeometry::createGeometry(
    rhi::TextureHandle colorBuffer, rhi::TextureHandle depthBuffer,
    float scale, float width, float height) {
    // Ported from: itwinjs-core CachedGeometry.ts line 841-844
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new EDLFilterGeometry(std::move(params), colorBuffer, depthBuffer,
                                 scale, width, height);
}

// ===========================================================================
// EDLMixGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 861-878)
// ===========================================================================

EDLMixGeometry::EDLMixGeometry(
    IndexedGeometryParams params,
    rhi::TextureHandle color1,
    rhi::TextureHandle color2,
    rhi::TextureHandle color4)
    : TexturedViewportQuadGeometry(std::move(params), TechniqueId::EDLMix,
                                    {color1, color2, color4})
{
}

EDLMixGeometry* EDLMixGeometry::createGeometry(
    rhi::TextureHandle color1,
    rhi::TextureHandle color2,
    rhi::TextureHandle color4) {
    // Ported from: itwinjs-core CachedGeometry.ts line 863-866
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new EDLMixGeometry(std::move(params), color1, color2, color4);
}

// ===========================================================================
// EVSMGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 881-899)
// ===========================================================================

EVSMGeometry::EVSMGeometry(
    IndexedGeometryParams params,
    rhi::TextureHandle depthBuffer,
    float width, float height)
    : TexturedViewportQuadGeometry(std::move(params), TechniqueId::EVSMFromDepth,
                                    {depthBuffer})
{
    m_stepSize[0] = 1.0f / width;
    m_stepSize[1] = 1.0f / height;
}

EVSMGeometry* EVSMGeometry::createGeometry(
    rhi::TextureHandle depthBuffer, float width, float height) {
    // Ported from: itwinjs-core CachedGeometry.ts line 884-887
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new EVSMGeometry(std::move(params), depthBuffer, width, height);
}

// ===========================================================================
// CompositeGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 904-940)
// ===========================================================================

CompositeGeometry::CompositeGeometry(
    IndexedGeometryParams params,
    std::vector<rhi::TextureHandle> textures)
    : TexturedViewportQuadGeometry(std::move(params), TechniqueId::CompositeHilite,
                                    std::move(textures))
{
}

// Ported from: itwinjs-core TechniqueId.ts computeCompositeTechniqueId()
// Maps CompositeFlags to the appropriate composite TechniqueId.
static TechniqueId computeCompositeTechniqueIdLocal(CompositeFlags flags) {
    bool const translucent = (static_cast<uint8_t>(flags) & static_cast<uint8_t>(CompositeFlags::Translucent)) != 0;
    bool const hilite = (static_cast<uint8_t>(flags) & static_cast<uint8_t>(CompositeFlags::Hilite)) != 0;
    bool const occlusion = (static_cast<uint8_t>(flags) & static_cast<uint8_t>(CompositeFlags::AmbientOcclusion)) != 0;

    if (translucent) {
        if (hilite) {
            return occlusion ? TechniqueId::CompositeAll : TechniqueId::CompositeHiliteAndTranslucent;
        }
        return occlusion ? TechniqueId::CompositeTranslucentAndOcclusion : TechniqueId::CompositeTranslucent;
    }
    if (hilite) {
        return occlusion ? TechniqueId::CompositeHiliteAndOcclusion : TechniqueId::CompositeHilite;
    }
    return occlusion ? TechniqueId::CompositeOcclusion : TechniqueId::CompositeHilite;
}

void CompositeGeometry::update(CompositeFlags flags) {
    // Ported from: itwinjs-core CachedGeometry.ts line 930-934
    m_techniqueId = computeCompositeTechniqueIdLocal(flags);
}

CompositeGeometry* CompositeGeometry::createGeometry(
    rhi::TextureHandle opaque,
    rhi::TextureHandle accum,
    rhi::TextureHandle reveal,
    rhi::TextureHandle hilite) {
    // Ported from: itwinjs-core CachedGeometry.ts line 905-908
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    std::vector<rhi::TextureHandle> textures = {opaque, accum, reveal, hilite};
    return new CompositeGeometry(std::move(params), std::move(textures));
}

// ===========================================================================
// CopyPickBufferGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 945-961)
// ===========================================================================

CopyPickBufferGeometry::CopyPickBufferGeometry(
    IndexedGeometryParams params,
    rhi::TextureHandle featureId,
    rhi::TextureHandle depthAndOrder)
    : TexturedViewportQuadGeometry(std::move(params), TechniqueId::CopyPickBuffers,
                                    {featureId, depthAndOrder})
{
}

CopyPickBufferGeometry* CopyPickBufferGeometry::createGeometry(
    rhi::TextureHandle featureId,
    rhi::TextureHandle depthAndOrder) {
    // Ported from: itwinjs-core CachedGeometry.ts line 947-950
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new CopyPickBufferGeometry(std::move(params), featureId, depthAndOrder);
}

// ===========================================================================
// CombineTexturesGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 962-978)
// ===========================================================================

CombineTexturesGeometry::CombineTexturesGeometry(
    IndexedGeometryParams params,
    rhi::TextureHandle texture0,
    rhi::TextureHandle texture1)
    : TexturedViewportQuadGeometry(std::move(params), TechniqueId::CombineTextures,
                                    {texture0, texture1})
{
}

CombineTexturesGeometry* CombineTexturesGeometry::createGeometry(
    rhi::TextureHandle texture0,
    rhi::TextureHandle texture1) {
    // Ported from: itwinjs-core CachedGeometry.ts line 963-966
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new CombineTexturesGeometry(std::move(params), texture0, texture1);
}

// ===========================================================================
// Combine3TexturesGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 980-997)
// ===========================================================================

Combine3TexturesGeometry::Combine3TexturesGeometry(
    IndexedGeometryParams params,
    rhi::TextureHandle texture0,
    rhi::TextureHandle texture1,
    rhi::TextureHandle texture2)
    : TexturedViewportQuadGeometry(std::move(params), TechniqueId::Combine3Textures,
                                    {texture0, texture1, texture2})
{
}

Combine3TexturesGeometry* Combine3TexturesGeometry::createGeometry(
    rhi::TextureHandle texture0,
    rhi::TextureHandle texture1,
    rhi::TextureHandle texture2) {
    // Ported from: itwinjs-core CachedGeometry.ts line 981-984
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new Combine3TexturesGeometry(std::move(params), texture0, texture1, texture2);
}

// ===========================================================================
// SingleTexturedViewportQuadGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 1000-1016)
// ===========================================================================

SingleTexturedViewportQuadGeometry::SingleTexturedViewportQuadGeometry(
    IndexedGeometryParams params,
    rhi::TextureHandle texture,
    TechniqueId techniqueId)
    : TexturedViewportQuadGeometry(std::move(params), techniqueId, {texture})
{
}

SingleTexturedViewportQuadGeometry* SingleTexturedViewportQuadGeometry::createGeometry(
    rhi::TextureHandle texture, TechniqueId techniqueId) {
    // Ported from: itwinjs-core CachedGeometry.ts line 1001-1004
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new SingleTexturedViewportQuadGeometry(std::move(params), texture, techniqueId);
}

// ===========================================================================
// VolumeClassifierGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 1026-1039)
// ===========================================================================

VolumeClassifierGeometry::VolumeClassifierGeometry(
    IndexedGeometryParams params,
    rhi::TextureHandle texture)
    : SingleTexturedViewportQuadGeometry(std::move(params), texture,
                                          TechniqueId::VolClassSetBlend)
{
}

VolumeClassifierGeometry* VolumeClassifierGeometry::createVCGeometry(
    rhi::TextureHandle texture) {
    // Ported from: itwinjs-core CachedGeometry.ts line 1029-1032
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        static_cast<uint32_t>(sViewportQuadMesh.indices.size())
    );
    return new VolumeClassifierGeometry(std::move(params), texture);
}

// ===========================================================================
// ScreenPointsGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 1044-1120)
// ===========================================================================

ScreenPointsGeometry::ScreenPointsGeometry(
    std::vector<float> positions,
    uint32_t numPoints,
    rhi::TextureHandle zTexture)
    : m_positions(std::move(positions))
    , m_numPoints(numPoints)
    , m_zTexture(zTexture)
{
}

void ScreenPointsGeometry::draw(rhi::Driver& driver) {
    if (m_primitive) {
        driver.bindRenderPrimitive(m_primitive);
        driver.drawArrays(0, m_numPoints, 0);
    }
}

void ScreenPointsGeometry::collectStatistics(RenderMemory::Statistics& stats) const {
    stats.addPointString(
                    static_cast<uint64_t>(m_positions.size() * sizeof(float)));
}

ScreenPointsGeometry* ScreenPointsGeometry::createGeometry(
    uint32_t width, uint32_t height, rhi::TextureHandle depth) {
    // Ported from: itwinjs-core CachedGeometry.ts line 1076-1095
    // Creates a grid of points covering the viewport for volume classification copy Z.
    const float pixWidth = 2.0f / static_cast<float>(width);
    const float pixHeight = 2.0f / static_cast<float>(height);
    const float startX = pixWidth * 0.5f - 1.0f;
    const float startY = pixHeight * 0.5f - 1.0f;

    const uint32_t numPoints = width * height;
    std::vector<float> positions;
    positions.reserve(numPoints * 2);

    float ptY = startY;
    for (uint32_t y = 0; y < height; ++y) {
        float ptX = startX;
        for (uint32_t x = 0; x < width; ++x) {
            positions.push_back(ptX);
            positions.push_back(ptY);
            ptX += pixWidth;
        }
        ptY += pixHeight;
    }

    return new ScreenPointsGeometry(std::move(positions), numPoints, depth);
}

// ===========================================================================
// SkyBoxQuadsGeometry
// (Ported from: itwinjs-core CachedGeometry.ts line 410-451)
// ===========================================================================

SkyBoxQuadsGeometry::SkyBoxQuadsGeometry(
    IndexedGeometryParams params,
    rhi::TextureHandle cubeTexture)
    : m_params(std::move(params))
    , m_cubeTexture(cubeTexture)
{
}

void SkyBoxQuadsGeometry::draw(rhi::Driver& driver) {
    // Ported from: itwinjs-core CachedGeometry.ts line 435-439
    // Skybox uses drawArrays (36 vertices, no index buffer).
    if (m_params.getPrimitive()) {
        driver.bindRenderPrimitive(m_params.getPrimitive());
        driver.drawArrays(0, 36, 0);
    }
}

SkyBoxQuadsGeometry* SkyBoxQuadsGeometry::create(rhi::TextureHandle cubeTexture) {
    // Ported from: itwinjs-core CachedGeometry.ts line 422-425
    // Skybox uses drawArrays with 36 vertices (no index buffer).
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        36  // drawArrays, no index buffer
    );
    return new SkyBoxQuadsGeometry(std::move(params), cubeTexture);
}

// ===========================================================================
// PolylineBuffers
// (Ported from: itwinjs-core CachedGeometry.ts line 1123-1177)
// ===========================================================================

PolylineBuffers::PolylineBuffers(
    rhi::RenderPrimitiveHandle primitive,
    rhi::BufferObjectHandle indices,
    rhi::BufferObjectHandle prevIndices,
    rhi::BufferObjectHandle nextIndicesAndParams,
    uint32_t numIndices)
    : m_primitive(primitive)
    , m_indices(indices)
    , m_prevIndices(prevIndices)
    , m_nextIndicesAndParams(nextIndicesAndParams)
    , m_numIndices(numIndices)
{
}

void PolylineBuffers::collectStatistics(RenderMemory::Statistics& stats) const {
    // Ported from: itwinjs-core CachedGeometry.ts line 1160-1161
    // Approximate sizes — actual buffer sizes would need Driver queries.
    stats.addPolyline(
                    static_cast<uint64_t>(m_numIndices * 3));  // indices
    stats.addPolyline(
                    static_cast<uint64_t>(m_numIndices * 3));  // prevIndices
    stats.addPolyline(
                    static_cast<uint64_t>(m_numIndices * 4));  // nextIndicesAndParams
}

PolylineBuffers* PolylineBuffers::create(
    rhi::Driver& driver,
    uint8_t const* indicesData, uint32_t indicesSize,
    uint8_t const* prevIndicesData, uint32_t prevIndicesSize,
    uint8_t const* nextIndicesAndParamsData, uint32_t nextIndicesAndParamsSize) {
    // Ported from: itwinjs-core CachedGeometry.ts line 1153-1157
    // Placeholder — actual GPU resource creation requires Driver.
    (void)driver;
    (void)indicesData;
    (void)indicesSize;
    (void)prevIndicesData;
    (void)prevIndicesSize;
    (void)nextIndicesAndParamsData;
    (void)nextIndicesAndParamsSize;
    return nullptr;
}

END_DQ_RENDER_NAMESPACE

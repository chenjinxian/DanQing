// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — PlanarGrid GPU geometry implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/PlanarGrid.ts
#include "PlanarGridGraphic.h"

#include <algorithm>
#include <cmath>

BEGIN_DQ_RENDER_NAMESPACE

// Ported from: itwinjs-core PlanarGridGeometry.create (PlanarGrid.ts:52-111).
// Pure geometry: the frustum∩grid-plane convex polygon + per-vertex grid-line UVs +
// fan triangulation. No GL. Quantization (QPoint2dList/QPoint3dList) and the
// far-from-origin branch are deferred (TODO); UVs are float - result-faithful for the
// blank-connection grid (precision only matters for far-from-origin reality models).
std::optional<PlanarGridPolygon> BuildPlanarGridPolygon(
    dqCommon::Frustum const& frustum, PlanarGridProps const& grid)
{
    // grid plane: through grid.origin with normal rMatrix.rowZ().
    auto plane = dqGeom::Plane3dByOriginAndUnitNormal::create(grid.origin, grid.rMatrix.RowZ());
    if (!plane)
        return std::nullopt;
    auto polygon = frustum.GetIntersectionWithPlane(*plane);
    if (!polygon || polygon->size() < 3)
        return std::nullopt;

    dqGeom::Vector3d const xAxis = grid.rMatrix.RowX();
    dqGeom::Vector3d const yAxis = grid.rMatrix.RowY();
    double const xOrigin = xAxis.DotProduct(grid.origin);
    double const yOrigin = yAxis.DotProduct(grid.origin);
    double const spacingX = (grid.spacing.x != 0.0) ? grid.spacing.x : 1.0;
    double const spacingY = (grid.spacing.y != 0.0) ? grid.spacing.y : 1.0;

    PlanarGridPolygon out;
    out.positions.reserve(polygon->size());
    std::vector<dqGeom::Point2d> rawUvs;
    rawUvs.reserve(polygon->size());
    for (auto const& p : *polygon) {
        out.positions.push_back(p);
        rawUvs.push_back(dqGeom::Point2d{(xAxis.DotProduct(p) - xOrigin) / spacingX,
                                         (yAxis.DotProduct(p) - yOrigin) / spacingY});
    }

    // Ported from: itwinjs-core PlanarGrid.ts:70-77 — QPoint2dList.fromPoints(params)
    // + `qParams.params.origin.x/y % gridsPerRef`。
    // UV 量化：顶点属性存 [0,1] 均匀量化值（数学等价参考的 short2 normalized），
    // 解码 uniform u_qTexCoordParams = (origin.xy, extent.xy)——shader 侧
    // unquantize2d 已移植。裸大值 float UV（±1000 级）的插值是相对精度，
    // fwidth/fract 的相位随视口/缩放伪随机翻转（实测：视口宽 1080→1100 时
    // Grid 消失；缩放 ~20 次消失 ~30 次恢复——同机制），量化后 [0,1] 插值
    // 精度均匀，相位稳定。
    //
    // 量化基准与解码基准是两个不同的 origin（参考的顺序，PlanarGrid.ts:72-75）：
    // ① QPoint2dList.fromPoints 先以 **range lower（minX/minY）** 量化——
    //    q=(uv-min)·65535/extent ∈[0,65535] 全覆盖；
    // ② 之后才对 uniform 的 params.origin 取 `min % gridsPerRef`——解码基准
    //    平移 gridsPerRef 的整数倍，fract 相位不变（mult∈{1,1/gridsPerRef} 均
    //    为整周期），量化参数数值小而稳定。
    // 此前把 ② 的取模 origin 同时用于 ① 的量化——量化区间 [fmod(min), +extent]
    // 与数据 [min,max] 错位，min 侧被 clamp 成 q=0、UV 覆盖只剩一半（v_texCoord
    // 跨度减半 → 线间距加倍、负半无相位），2026-09-14 初始网格间距 bug 根因。
    double const gridsPerRef = std::max(1.0, grid.gridsPerRef);
    double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300;
    for (auto const& uv : rawUvs) {
        minX = std::min(minX, uv.x); maxX = std::max(maxX, uv.x);
        minY = std::min(minY, uv.y); maxY = std::max(maxY, uv.y);
    }
    double const originX = std::fmod(minX, gridsPerRef);
    double const originY = std::fmod(minY, gridsPerRef);
    double const extentX = maxX - minX;
    double const extentY = maxY - minY;
    // Quantization.quantize（QPoint.ts:42-44）：floor(clamp((pos-origin)*scale,0,65535))，
    // 数学等价的 [0,1] 归一（short-normalized 精度相同，float 属性免 RHI 改造）。
    constexpr double kRangeScale16 = 65535.0;  // Quantization.rangeScale16
    out.qTexCoordParams = {static_cast<float>(originX), static_cast<float>(originY),
                           static_cast<float>(extentX), static_cast<float>(extentY)};
    for (auto const& uv : rawUvs) {
        // ① 量化基准 = range lower（minX/minY），非取模 origin。
        double const qx = extentX > 0.0
            ? std::floor(std::max(0.0, std::min(kRangeScale16, 0.5 + (uv.x - minX) * (kRangeScale16 / extentX)))) / kRangeScale16
            : 0.0;
        double const qy = extentY > 0.0
            ? std::floor(std::max(0.0, std::min(kRangeScale16, 0.5 + (uv.y - minY) * (kRangeScale16 / extentY)))) / kRangeScale16
            : 0.0;
        out.uvs.push_back(dqGeom::Point2d{qx, qy});
    }

    // Fan triangulation: (0, i+1, i+2) for i in 0..nTriangles-1 (PlanarGrid.ts:85-90).
    size_t const nTriangles = out.positions.size() - 2;
    out.indices.reserve(3 * nTriangles);
    for (size_t i = 0; i < nTriangles; ++i) {
        out.indices.push_back(0);
        out.indices.push_back(static_cast<uint32_t>(i + 1));
        out.indices.push_back(static_cast<uint32_t>(i + 2));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Construction — build geometry and upload to GPU
// ---------------------------------------------------------------------------
PlanarGridGraphic::PlanarGridGraphic(rhi::Driver& driver, float extent, int gridLines)
    : m_driver(driver)
{
    // Default grid props: origin (0,0,0), identity rotation (ground plane z=0), spacing
    // sized to ~20 cells per half-extent, reference lines every 10, black lines (visible
    // on the blank view's white background). Ported from: itwinjs-core display-test-app
    // Grid.ts (createPlanarGrid props); the polygon is built from a ground-area frustum.
    m_props.origin = dqGeom::Point3d::From(0.0, 0.0, 0.0);
    m_props.rMatrix = dqGeom::Matrix3d::CreateIdentity();
    double const spacing = (extent > 0.0f) ? (2.0 * static_cast<double>(extent) / 20.0) : 1.0;
    m_props.spacing = dqGeom::Point2d{spacing, spacing};
    m_props.gridsPerRef = 10.0;
    m_props.color = dqCommon::ColorDef::black;
    (void)gridLines;  // gridsPerRef drives the reference-line period; gridLines is unused
                      // in the faithful procedural grid (the polygon is the frustum∩plane).
    buildGeometry(extent, gridLines);
    uploadToGpu(driver);
}

// Faithful per-frame ctor: the polygon is the live view frustum intersected with
// the grid plane (props.origin / props.rMatrix.rowZ()). The caller rebuilds this
// whenever the frustum changes so the infinite grid plane tracks the camera.
// Ported from: itwinjs-core PlanarGridGeometry.create (PlanarGrid.ts:52-111).
PlanarGridGraphic::PlanarGridGraphic(rhi::Driver& driver,
                                     dqCommon::Frustum const& frustum,
                                     PlanarGridProps const& props)
    : m_props(props)
    , m_driver(driver)
{
    buildGeometryFromFrustum(frustum);
    uploadToGpu(driver);
}

PlanarGridGraphic::~PlanarGridGraphic()
{
    if (m_primitive) m_driver.destroyRenderPrimitive(m_primitive);
    if (m_vbo) m_driver.destroyBufferObject(m_vbo);
    if (m_vbh) m_driver.destroyVertexBuffer(m_vbh);
    if (m_ibh) m_driver.destroyIndexBuffer(m_ibh);
    if (m_vbih) m_driver.destroyVertexBufferInfo(m_vbih);
}

// ---------------------------------------------------------------------------
// Build the frustum∩plane polygon into m_vertices (position + uv) + m_indices (fan).
// Ported from: itwinjs-core PlanarGrid.ts (PlanarGridGeometry.create via
//               BuildPlanarGridPolygon). A ground-area frustum the z=0 plane intersects
//               yields the visible grid quad.
// ---------------------------------------------------------------------------
void PlanarGridGraphic::buildGeometry(float extent, int /*gridLines*/)
{
    // Synthetic ground-area frustum the z=0 plane intersects:
    // [-extent,extent]^3 -> z=0 cross-section. (Legacy one-time build; the faithful
    // path passes the live view frustum via the (driver, frustum, props) ctor.)
    dqCommon::Frustum const frustum = dqCommon::Frustum::fromRange(
        dqGeom::Range3d(-extent, -extent, -extent, extent, extent, extent));
    buildGeometryFromFrustum(frustum);
}

// Build the frustum∩grid-plane polygon (position+uv) + fan indices from `frustum`
// using m_props. Shared by both ctors. Ported from: itwinjs-core
// PlanarGridGeometry.create (PlanarGrid.ts:52-111) via BuildPlanarGridPolygon.
void PlanarGridGraphic::buildGeometryFromFrustum(dqCommon::Frustum const& frustum)
{
    m_vertices.clear();
    m_indices.clear();

    auto polygon = BuildPlanarGridPolygon(frustum, m_props);
    if (!polygon) {
        m_vertexCount = 0;
        m_indexCount = 0;
        return;
    }
    m_vertices.reserve(polygon->positions.size());
    for (size_t i = 0; i < polygon->positions.size(); ++i) {
        m_vertices.push_back({{static_cast<float>(polygon->positions[i].x),
                               static_cast<float>(polygon->positions[i].y),
                               static_cast<float>(polygon->positions[i].z)},
                              {static_cast<float>(polygon->uvs[i].x),
                               static_cast<float>(polygon->uvs[i].y)}});
    }
    m_indices = std::move(polygon->indices);
    m_qTexCoordParams = polygon->qTexCoordParams;  // 量化解码参数（每帧随多边形）
    m_vertexCount = static_cast<uint32_t>(m_vertices.size());
    m_indexCount = static_cast<uint32_t>(m_indices.size());
}

// ---------------------------------------------------------------------------
// Upload geometry to GPU via RHI Driver.
// Handles are created ONCE (first valid upload) at kMaxGridVerts capacity and
// reused across rebuilds — updateFrustum() re-uploads data in place instead of
// recreating handles. This avoids the RHI state-tracker churn that per-frame
// handle create/destroy caused (DanQing's tracker misbehaves under rapid churn,
// unlike raw WebGL where itwinjs recreates PlanarGridGeometry each frame).
// ---------------------------------------------------------------------------
void PlanarGridGraphic::uploadToGpu(rhi::Driver& driver)
{
    if (m_vertexCount == 0) return;
    // Guard: never exceed allocated capacity (defensive; observed max is 8 verts).
    if (m_vertexCount > kMaxGridVerts || m_indexCount > kMaxGridIndices) return;

    if (!m_primitive) {
        // First valid upload: allocate handles at max capacity (stable thereafter).
        rhi::AttributeArray attrs = {};
        attrs[0].buffer = 0;
        attrs[0].offset = 0;
        attrs[0].type = rhi::ElementType::FLOAT3;  // position
        attrs[1].buffer = 0;
        attrs[1].offset = 12;  // after vec3 (12 bytes)
        attrs[1].type = rhi::ElementType::FLOAT2;  // uv

        m_vbih = driver.createVertexBufferInfo(1, 2, attrs);
        m_vbh = driver.createVertexBuffer(kMaxGridVerts, m_vbih);
        m_vbo = driver.createBufferObject(
            static_cast<uint32_t>(kMaxGridVerts * sizeof(Vertex)),
            rhi::BufferObjectBinding::VERTEX,
            rhi::BufferUsage::STATIC);
        driver.setVertexBufferObject(m_vbh, 0, m_vbo);
        m_ibh = driver.createIndexBuffer(rhi::ElementType::UINT, kMaxGridIndices,
                                         rhi::BufferUsage::STATIC);
        m_primitive = driver.createRenderPrimitive(m_vbh, m_ibh, rhi::PrimitiveType::TRIANGLES);
    }

    // Update geometry data in place (a subset of max capacity). draw() uses
    // m_indexCount, so only the valid prefix of the buffers is drawn.
    rhi::BufferDescriptor vboData(m_vertices.data(), m_vertexCount * sizeof(Vertex));
    driver.updateBufferObject(m_vbo, std::move(vboData), 0);
    rhi::BufferDescriptor iboData(m_indices.data(), m_indexCount * sizeof(uint32_t));
    driver.updateIndexBuffer(m_ibh, std::move(iboData), 0);
}

// Rebuild the polygon from a new view frustum, reusing GPU handles (in-place).
// Ported from: itwinjs-core ViewContext.drawStandardGrid re-gathering the grid
//               decoration each frame (ViewContext.ts:348).
void PlanarGridGraphic::updateFrustum(dqCommon::Frustum const& frustum,
                                      PlanarGridProps const& props)
{
    m_props = props;
    buildGeometryFromFrustum(frustum);  // rebuilds m_vertices/m_indices + counts
    uploadToGpu(m_driver);              // in-place update (handles reused)
}

// ---------------------------------------------------------------------------
// Set MVP matrix
// ---------------------------------------------------------------------------
void PlanarGridGraphic::setMvpMatrix(float const* mvp16) noexcept
{
    for (int i = 0; i < 16; ++i) {
        m_mvpMatrix[i] = mvp16[i];
    }
}

// ---------------------------------------------------------------------------
// Draw — bind primitive and issue draw call
// (Ported from: itwinjs-core CachedGeometry.ts draw)
// ---------------------------------------------------------------------------
void PlanarGridGraphic::draw(rhi::Driver& driver)
{
    if (!m_primitive) return;
    if (m_indexCount == 0) return;

    driver.bindRenderPrimitive(m_primitive);
    driver.draw2(0, m_indexCount, 1);
}

// Ported from: itwinjs-core glsl/PlanarGrid.ts u_gridColor (color/255).
std::array<float, 3> PlanarGridGraphic::getGridColorRgb() const noexcept
{
    dqCommon::ColorComponents const c = dqCommon::ColorDef::getColors(m_props.color.getTbgr());
    return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f};
}

// Ported from: itwinjs-core glsl/PlanarGrid.ts u_gridProps =
// (gridsPerRef, 1-planeTransparency, 1-lineTransparency, 1-refTransparency).
std::array<float, 4> PlanarGridGraphic::getGridProps() const noexcept
{
    PlanarGridTransparency const t = m_props.transparency.value_or(PlanarGridTransparency{});
    return {static_cast<float>(m_props.gridsPerRef),
            1.0f - t.planeTransparency,
            1.0f - t.lineTransparency,
            1.0f - t.refTransparency};
}

void PlanarGridGraphic::collectStatistics(RenderMemory::Statistics& stats) const
{
    // Ported from: itwinjs-core PlanarGrid.ts collectStatistics
    stats.addSurface(
                    static_cast<uint64_t>(m_vertexCount * sizeof(Vertex)));
    stats.addSurface(
                    static_cast<uint64_t>(m_indices.size() * sizeof(uint32_t)));
}

END_DQ_RENDER_NAMESPACE

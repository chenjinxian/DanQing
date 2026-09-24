// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GLCanvasContext implementation (CanvasContext 的 GL 栅格化后端)
//
// Rendering path: filament libs/filagui/src/ImGuiHelper.cpp
// (processImGuiCommands :196-320 — packed pos/uv/color vertex + one TRIANGLES
// primitive per draw command + texture-sampled blending; createVertexBuffer
// :326-342 — the vertex layout). The CanvasContext API + drawDecoration
// semantics are the itwinjs-core contract (CanvasDecoration.ts:12-62).
#include "GLCanvasContext.h"
#include "shader/Canvas2dShaders.h"

#include <algorithm>
#include <cmath>

#include "gl/GL.h"  // glIsEnabled/glGetBooleanv/glGetIntegerv + glDisable/glDepthMask/
                    // glEnable/glBlendFunc/glBlendFuncSeparate/glLineWidth（力置 overlay
                    // 态前保存、绘制后恢复——flush 保持 GL 状态中立）

BEGIN_DQ_RENDER_NAMESPACE

namespace {

// 1×1 white RGBA texture — the untextured-geometry fallback (ImGuiHelper's
// material-without-texture path, :310-314: no texture → the default material
// samples the font atlas; untextured shapes here sample white so the vertex
// color alone drives the output).
constexpr uint8_t kWhitePixel[4] = {255, 255, 255, 255};

dqCommon::ColorComponents toComponents(dqCommon::ColorDef c) { return c.getColors(); }

}  // namespace

GLCanvasContext::GLCanvasContext()
{
    // Lazy compile on first use() (ShaderProgram 的 compile-on-demand，
    // ShaderProgram.ts)。描述名 "Canvas2dList"。
    m_program.setSource(kCanvas2dListVert, kCanvas2dListFrag, "Canvas2dList");
}

// ---------------------------------------------------------------------------
// Transform stack (translate-only 2D affine)
// HTML canvas save/restore stacks the full transform + styles; the ported
// CanvasContext subset has translate only, and the reference's drawDecoration
// bodies set their style explicitly each call — the stack covers the transform.
// ---------------------------------------------------------------------------
void GLCanvasContext::save()
{
    m_translateStack.emplace_back(m_tx, m_ty);
}

void GLCanvasContext::restore()
{
    if (m_translateStack.empty())
        return;
    m_tx = m_translateStack.back().first;
    m_ty = m_translateStack.back().second;
    m_translateStack.pop_back();
}

void GLCanvasContext::translate(double x, double y)
{
    m_tx += x;
    m_ty += y;
}

// ---------------------------------------------------------------------------
// Path recording (HTML canvas path semantics, CPU-side)
// ---------------------------------------------------------------------------
void GLCanvasContext::beginPath()
{
    m_pathPoints.clear();
    m_hasCursor = false;
}

void GLCanvasContext::moveTo(double x, double y)
{
    // HTML: moveTo starts a new subpath — no segment is emitted (the break is
    // carried on the point itself so one beginPath can hold multiple subpaths,
    // e.g. drawCross 的两条独立臂，ViewTool.ts:981-984/993-995)。
    m_pathPoints.push_back({x + m_tx, y + m_ty, true});
    m_hasCursor = true;
}

void GLCanvasContext::lineTo(double x, double y)
{
    // HTML: lineTo with no current point acts as moveTo.
    if (!m_hasCursor) {
        moveTo(x, y);
        return;
    }
    m_pathPoints.push_back({x + m_tx, y + m_ty, false});
}

void GLCanvasContext::stroke()
{
    // HTML: stroke() rasterizes ALL subpaths since beginPath under the CURRENT
    // style and does NOT clear the path. The ported path is one polyline;
    // beginPath resets it. Degenerate (<2 pts) strokes produce nothing.
    if (m_pathPoints.size() < 2)
        return;
    strokePath();
}

void GLCanvasContext::fill()
{
    // HTML: fill() closes the path implicitly and fills it (nonzero winding).
    // The ported paths are convex (locate circle, rects) — fan triangulation is
    // exact for them; concave paths land as a registered gap.
    if (m_pathPoints.size() < 3)
        return;
    fillPath();
}

void GLCanvasContext::arc(double x, double y, double radius,
                          double startAngle, double endAngle)
{
    if (radius <= 0.0)
        return;
    double const span = endAngle - startAngle;
    if (span == 0.0)
        return;
    constexpr double kStep = 7.5 * 3.14159265358979323846 / 180.0;   // 7.5°/segment
    int const segments = std::max(8, static_cast<int>(std::ceil(std::abs(span) / kStep)));
    for (int i = 0; i <= segments; ++i) {
        double const a = startAngle + span * (static_cast<double>(i) / segments);
        double const px = x + radius * std::cos(a);
        double const py = y + radius * std::sin(a);
        if (i == 0)
            moveTo(px, py);
        else
            lineTo(px, py);
    }
}

void GLCanvasContext::setStrokeStyle(dqCommon::ColorDef color)
{
    auto const cc = toComponents(color);
    m_strokeColor[0] = static_cast<float>(cc.r) / 255.0f;
    m_strokeColor[1] = static_cast<float>(cc.g) / 255.0f;
    m_strokeColor[2] = static_cast<float>(cc.b) / 255.0f;
    m_strokeColor[3] = static_cast<float>(255 - cc.t) / 255.0f;
}

void GLCanvasContext::setFillStyle(dqCommon::ColorDef color)
{
    auto const cc = toComponents(color);
    m_fillColor[0] = static_cast<float>(cc.r) / 255.0f;
    m_fillColor[1] = static_cast<float>(cc.g) / 255.0f;
    m_fillColor[2] = static_cast<float>(cc.b) / 255.0f;
    m_fillColor[3] = static_cast<float>(255 - cc.t) / 255.0f;
}

void GLCanvasContext::setGlobalAlpha(double alpha)
{
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;
    m_globalAlpha = alpha;
}

void GLCanvasContext::setLineWidth(double w)
{
    if (w <= 0.0)
        return;
    m_lineWidth = w;
}

void GLCanvasContext::drawImage(rhi::TextureHandle texture, uint32_t width, uint32_t height,
                                double dx, double dy)
{
    if (texture == rhi::TextureHandle{} || width == 0 || height == 0)
        return;   // sprite 未装载（Sprites.ts:53-56 load 失败 isLoaded=false → 不画）
    float const x = static_cast<float>(dx + m_tx);
    float const y = static_cast<float>(dy + m_ty);
    float const w = static_cast<float>(width);
    float const h = static_cast<float>(height);
    // drawImage's alpha comes from globalAlpha (Sprites.ts:132); the vertex
    // color is white so the texture carries the image.
    float const white[4] = {1.0f, 1.0f, 1.0f, static_cast<float>(m_globalAlpha)};
    // Command tint stays white — the image carries its own color; alpha rides
    // the vertex color (Sprites.ts:132 globalAlpha).
    emitTexturedQuad(texture, x, y, w, h, white);
}

// ---------------------------------------------------------------------------
// Path → packed draw list
// ---------------------------------------------------------------------------
// Push one packed vertex; returns its index (uint16 — the draw list's index
// type; ImDrawIdx is 16-bit by default).
uint16_t GLCanvasContext::pushVertex(float x, float y, float u, float v, float const rgba[4])
{
    Vertex vtx{};
    vtx.x = x; vtx.y = y; vtx.u = u; vtx.v = v;
    vtx.r = static_cast<uint8_t>(std::lround(rgba[0] * 255.0f));
    vtx.g = static_cast<uint8_t>(std::lround(rgba[1] * 255.0f));
    vtx.b = static_cast<uint8_t>(std::lround(rgba[2] * 255.0f));
    vtx.a = static_cast<uint8_t>(std::lround(rgba[3] * 255.0f));
    m_vertices.push_back(vtx);
    return static_cast<uint16_t>(m_vertices.size() - 1);
}

void GLCanvasContext::strokePath()
{
    // Sweep the polyline into a triangle strip of width m_lineWidth, miter
    // joins. Each segment emits two vertices (±half-width normal); the strip
    // is cut per segment into independent quads (two triangles) so disjoint
    // segments don't connect.
    double const halfW = m_lineWidth * 0.5;
    float color[4] = { m_strokeColor[0], m_strokeColor[1], m_strokeColor[2],
                       m_strokeColor[3] * static_cast<float>(m_globalAlpha) };

    uint32_t const firstIndex = static_cast<uint32_t>(m_indices.size());
    uint32_t indexCount = 0;

    size_t const n = m_pathPoints.size();
    for (size_t i = 0; i + 1 < n; ++i) {
        // HTML 子路径断点：moveTo 起的点不与上一点连线（跨子路径不发射线段）。
        if (m_pathPoints[i + 1].newSubpath)
            continue;
        double const x0 = m_pathPoints[i].x,  y0 = m_pathPoints[i].y;
        double const x1 = m_pathPoints[i+1].x, y1 = m_pathPoints[i+1].y;
        double const dx = x1 - x0, dy = y1 - y0;
        double const len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-9)
            continue;
        double const nx = -dy / len, ny = dx / len;   // unit normal
        float const hw = static_cast<float>(halfW);
        // Quad corners: (p0 - n·hw)(p1 - n·hw)(p1 + n·hw)(p0 + n·hw).
        uint16_t const a = pushVertex(static_cast<float>(x0 - nx * hw), static_cast<float>(y0 - ny * hw), 0, 0, color);
        uint16_t const b = pushVertex(static_cast<float>(x1 - nx * hw), static_cast<float>(y1 - ny * hw), 0, 0, color);
        uint16_t const c = pushVertex(static_cast<float>(x1 + nx * hw), static_cast<float>(y1 + ny * hw), 0, 0, color);
        uint16_t const d = pushVertex(static_cast<float>(x0 + nx * hw), static_cast<float>(y0 + ny * hw), 0, 0, color);
        m_indices.push_back(a); m_indices.push_back(b); m_indices.push_back(c);
        m_indices.push_back(a); m_indices.push_back(c); m_indices.push_back(d);
        indexCount += 6;
    }
    if (indexCount > 0)
        m_drawCommands.push_back({ firstIndex, indexCount, {}, {1,1,1,1} });
}

void GLCanvasContext::fillPath()
{
    // Fan triangulation per subpath (convex paths — the locate circle; the
    // implicit close edge connects the last point back to the subpath start).
    // HTML: fill() closes and fills EVERY subpath since beginPath.
    float color[4] = { m_fillColor[0], m_fillColor[1], m_fillColor[2],
                       m_fillColor[3] * static_cast<float>(m_globalAlpha) };

    size_t const n = m_pathPoints.size();
    size_t subStart = 0;
    while (subStart < n) {
        // 找本子路径终点（下一个 newSubpath 或末尾）。
        size_t subEnd = subStart + 1;
        while (subEnd < n && !m_pathPoints[subEnd].newSubpath)
            ++subEnd;

        size_t const count = subEnd - subStart;
        if (count >= 3) {   // 退化（<3 点）子路径不产生可见填充（HTML 同）
            uint16_t const v0 = pushVertex(static_cast<float>(m_pathPoints[subStart].x),
                                           static_cast<float>(m_pathPoints[subStart].y), 0, 0, color);
            uint32_t const firstIndex = static_cast<uint32_t>(m_indices.size());
            uint32_t indexCount = 0;
            // 扇形化 + 隐式闭合边（末点回连起点）——凸子路径精确。
            // i 从 1 到 count-1：vj 在末步绕回子路径起点（不复制点）。
            for (size_t i = 1; i < count; ++i) {
                size_t const j = (i + 1 == count) ? 0 : i + 1;
                uint16_t const vi = pushVertex(static_cast<float>(m_pathPoints[subStart + i].x),
                                               static_cast<float>(m_pathPoints[subStart + i].y), 0, 0, color);
                uint16_t const vj = pushVertex(static_cast<float>(m_pathPoints[subStart + j].x),
                                               static_cast<float>(m_pathPoints[subStart + j].y), 0, 0, color);
                m_indices.push_back(v0); m_indices.push_back(vi); m_indices.push_back(vj);
                indexCount += 3;
            }
            if (indexCount > 0)
                m_drawCommands.push_back({ firstIndex, indexCount, {}, {1,1,1,1} });
        }
        subStart = subEnd;
    }
}

void GLCanvasContext::emitTexturedQuad(rhi::TextureHandle texture, float x, float y,
                                       float w, float h, float const color[4])
{
    uint32_t const firstIndex = static_cast<uint32_t>(m_indices.size());
    uint16_t const a = pushVertex(x,     y,     0, 0, color);
    uint16_t const b = pushVertex(x + w, y,     1, 0, color);
    uint16_t const c = pushVertex(x + w, y + h, 1, 1, color);
    uint16_t const d = pushVertex(x,     y + h, 0, 1, color);
    m_indices.push_back(a); m_indices.push_back(b); m_indices.push_back(c);
    m_indices.push_back(a); m_indices.push_back(c); m_indices.push_back(d);
    m_drawCommands.push_back({ firstIndex, 6, texture, {color[0], color[1], color[2], color[3]} });
}

// ---------------------------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------------------------
void GLCanvasContext::dispose(rhi::Driver& driver)
{
    if (m_primitive) driver.destroyRenderPrimitive(m_primitive);
    if (m_ibh) driver.destroyIndexBuffer(m_ibh);
    if (m_vbo) driver.destroyBufferObject(m_vbo);
    if (m_vbh) driver.destroyVertexBuffer(m_vbh);
    if (m_vbih) driver.destroyVertexBufferInfo(m_vbih);
    if (m_whiteTexture) driver.destroyTexture(m_whiteTexture);
    if (m_program.isValid())
        driver.destroyProgram(m_program.getHandle());
    m_primitive = {};
    m_ibh = {};
    m_vbo = {};
    m_vbh = {};
    m_vbih = {};
    m_whiteTexture = {};
    m_gpuVertexCapacity = 0;
    m_gpuIndexCapacity = 0;
}

void GLCanvasContext::beginFrame()
{
    m_vertices.clear();
    m_indices.clear();
    m_drawCommands.clear();
    m_pathPoints.clear();
    m_hasCursor = false;
    m_tx = 0.0;
    m_ty = 0.0;
    m_translateStack.clear();
    // Style intentionally NOT reset — a real canvas context persists strokeStyle/
    // fillStyle/lineWidth/globalAlpha across frames (Target.ts:1408 的 _2dCanvas
    // 跨帧存活)。
}

void GLCanvasContext::flush(rhi::Driver& driver, float viewportWidth, float viewportHeight,
                            float devicePixelRatio)
{
    if (!hasContent())
        return;

    uint32_t const vertexCount = static_cast<uint32_t>(m_vertices.size());
    uint32_t const indexCount = static_cast<uint32_t>(m_indices.size());

    // --- GPU buffers (capacity grows on demand; DYNAMIC per-frame content) ---
    // One packed VBO (pos/uv/color interleaved — ImGuiHelper.cpp:326-342) + one
    // IBH + one TRIANGLES primitive; the draw commands index into them.
    if (vertexCount > m_gpuVertexCapacity || indexCount > m_gpuIndexCapacity) {
        if (m_primitive) driver.destroyRenderPrimitive(m_primitive);
        if (m_ibh) driver.destroyIndexBuffer(m_ibh);
        if (m_vbo) driver.destroyBufferObject(m_vbo);
        if (m_vbh) driver.destroyVertexBuffer(m_vbh);
        if (m_vbih) driver.destroyVertexBufferInfo(m_vbih);
        m_primitive = {};
        m_ibh = {};
        m_vbo = {};
        m_vbh = {};
        m_vbih = {};

        m_gpuVertexCapacity = vertexCount < 1024u ? 1024u : vertexCount * 2u;
        m_gpuIndexCapacity = indexCount < 2048u ? 2048u : indexCount * 2u;

        rhi::AttributeArray attrs = {};
        // ImDrawVert layout (ImGuiHelper.cpp:333-340): pos FLOAT2 @0,
        // uv FLOAT2 @8, color UBYTE4 (normalized) @16 — 20 bytes/vertex.
        attrs[0].buffer = 0;
        attrs[0].offset = 0;
        attrs[0].type = rhi::ElementType::FLOAT2;   // a_position (location 0)
        attrs[1].buffer = 0;
        attrs[1].offset = 8;
        attrs[1].type = rhi::ElementType::FLOAT2;   // a_uv (location 1)
        attrs[2].buffer = 0;
        attrs[2].offset = 16;
        attrs[2].type = rhi::ElementType::UBYTE4;   // a_color (location 2)
        m_vbih = driver.createVertexBufferInfo(1, 3, attrs);
        m_vbh = driver.createVertexBuffer(m_gpuVertexCapacity, m_vbih);
        m_vbo = driver.createBufferObject(m_gpuVertexCapacity * sizeof(Vertex),
                                          rhi::BufferObjectBinding::VERTEX,
                                          rhi::BufferUsage::DYNAMIC);
        driver.setVertexBufferObject(m_vbh, 0, m_vbo);
        m_ibh = driver.createIndexBuffer(rhi::ElementType::USHORT, m_gpuIndexCapacity,
                                         rhi::BufferUsage::DYNAMIC);
        m_primitive = driver.createRenderPrimitive(m_vbh, m_ibh, rhi::PrimitiveType::TRIANGLES);
    }

    // White 1×1 fallback texture (untextured draw commands sample it so the
    // vertex color alone drives the output — ImGuiHelper :310-314 equivalent).
    if (!m_whiteTexture) {
        m_whiteTexture = driver.createTexture(rhi::SamplerType::SAMPLER_2D, 1,
                                              rhi::TextureFormat::RGBA8, 1, 1, 1,
                                              rhi::TextureUsage::DEFAULT);
        rhi::PixelBufferDescriptor pbd(kWhitePixel, sizeof(kWhitePixel), GL_RGBA, GL_UNSIGNED_BYTE);
        driver.setTextureData(m_whiteTexture, 0, 0, 0, 0, 1, 1, 1, std::move(pbd));
    }

    // Upload device-px content (view px × devicePixelRatio —
    // ImGuiHelper.cpp:245-249 io.DisplayFramebufferScale).
    if (devicePixelRatio == 1.0f) {
        rhi::BufferDescriptor vdata(m_vertices.data(), m_vertices.size() * sizeof(Vertex));
        driver.updateBufferObject(m_vbo, std::move(vdata), 0);
    } else {
        std::vector<Vertex> device(m_vertices.size());
        for (size_t i = 0; i < m_vertices.size(); ++i) {
            device[i] = m_vertices[i];
            device[i].x *= devicePixelRatio;
            device[i].y *= devicePixelRatio;
        }
        rhi::BufferDescriptor vdata(device.data(), device.size() * sizeof(Vertex));
        driver.updateBufferObject(m_vbo, std::move(vdata), 0);
    }
    rhi::BufferDescriptor idata(m_indices.data(), m_indices.size() * sizeof(uint16_t));
    driver.updateIndexBuffer(m_ibh, std::move(idata), 0);

    // --- Render state: force the overlay state (TargetImpl._overlayRenderState:
    // depthMask=false, blend=true, blendFunc(ONE, ONE_MINUS_SRC_ALPHA); depthTest
    // off). Save the touched physical state first and restore it after the pass:
    // flush() must be GL-state-neutral.
    GLboolean const savedDepthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean savedDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &savedDepthMask);
    GLboolean const savedBlend = glIsEnabled(GL_BLEND);
    GLint savedBlendSrcRgb = GL_ONE, savedBlendDstRgb = GL_ZERO;
    GLint savedBlendSrcAlpha = GL_ONE, savedBlendDstAlpha = GL_ZERO;
    glGetIntegerv(GL_BLEND_SRC_RGB, &savedBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &savedBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &savedBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &savedBlendDstAlpha);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    float const viewport[2] = {viewportWidth, viewportHeight};
    for (auto const& cmd : m_drawCommands) {
        ShaderProgramParams params;
        params.setVec2("u_viewport", viewport);
        params.setInt("u_texture", 0);
        if (!m_program.use(driver, params))
            break;   // compile/link failure — skip remaining commands
        m_program.uploadUniforms(driver, params);
        driver.bindTexture(0, cmd.texture != rhi::TextureHandle{} ? cmd.texture : m_whiteTexture);
        driver.bindRenderPrimitive(m_primitive);
        driver.draw2(cmd.firstIndex, cmd.indexCount, 1);
        m_program.endUse(driver);
    }

    // Restore the pre-flush physical state (state neutrality, see above).
    if (savedDepthTest == GL_TRUE) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glDepthMask(savedDepthMask);
    if (savedBlend == GL_TRUE) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendFuncSeparate(static_cast<GLenum>(savedBlendSrcRgb),
                        static_cast<GLenum>(savedBlendDstRgb),
                        static_cast<GLenum>(savedBlendSrcAlpha),
                        static_cast<GLenum>(savedBlendDstAlpha));
    glLineWidth(1.0f);
}

END_DQ_RENDER_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GLCanvasContext (CanvasContext 的 GL 栅格化后端)
//
// Ported from: itwinjs-core core/frontend/src/render/CanvasDecoration.ts:12-62
//              (CanvasContext contract) + the drawing environment at
//              internal/render/webgl/Target.ts:1408-1439 (drawOverlayDecorations).
//
// APPROVED DEVIATION (预批准兜底，原 spec 2026-09-11-windowarea-look-design §2.4；
// spec 已随 2026-09-24 历史文档清理删除，偏差登记以本注释为准): the reference
// rasterizes into an HTML 2D canvas; DanQing records the
// stroked paths CPU-side and rasterizes them as GL_LINES at the end of the frame
// (the QPainter alien-widget backend hung the native WGL driver — 2x
// LiveKernelEvent 141 TDR). Same CanvasContext API, same drawDecoration bodies.
//
// HTML canvas 2D semantics preserved CPU-side:
//   - save/restore stack the current transform (the CanvasContext subset has only
//     translate; it composes additively).
//   - beginPath resets the current path; moveTo starts a new subpath (no segment);
//     lineTo appends a segment from the current point (lineTo with no current
//     point acts as moveTo); stroke commits ALL subpaths since beginPath under
//     the CURRENT style and does NOT clear the path.
#pragma once

#include "ShaderProgramImpl.h"

#include <dqRender/CanvasDecoration.h>
#include <dqRender/rhi/Driver.h>
#include <dqRender/rhi/Handle.h>

#include <cstdint>
#include <utility>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// GLCanvasContext — records CanvasContext operations, rasterizes as textured
// triangles. Ported from: filament libs/filagui/src/ImGuiHelper.cpp
// (processImGuiCommands :196-320 + createVertexBuffer :326-342) — packed
// pos/uv/color vertex + one TRIANGLES primitive per draw command + scissor +
// texture-sampled fragment blending. The CanvasContext API is the itwinjs-core
// CanvasRenderingContext2D contract (CanvasDecoration.ts:12-62); the rendering
// path below is the filament/ImGui draw-list mechanism.
// ---------------------------------------------------------------------------
class GLCanvasContext final : public CanvasContext {
public:
    GLCanvasContext();
    ~GLCanvasContext() override = default;

    GLCanvasContext(GLCanvasContext const&) = delete;
    GLCanvasContext& operator=(GLCanvasContext const&) = delete;

    // --- CanvasContext (HTML canvas 2D API subset — CanvasDecoration.h) ---
    void save() override;
    void restore() override;
    void translate(double x, double y) override;
    void beginPath() override;
    void moveTo(double x, double y) override;
    void lineTo(double x, double y) override;
    void stroke() override;
    void setStrokeStyle(dqCommon::ColorDef color) override;
    void setLineWidth(double w) override;
    void setFillStyle(dqCommon::ColorDef color) override;
    void setGlobalAlpha(double alpha) override;
    void arc(double x, double y, double radius, double startAngle, double endAngle) override;
    void fill() override;
    void drawImage(rhi::TextureHandle texture, uint32_t width, uint32_t height,
                   double dx, double dy) override;

    // --- Frame lifecycle (TargetImpl::drawCanvasDecorations drives these) ---

    // Reset the recorded draw list for a new frame (the reference's canvas
    // clears every frame; the ImGui draw list likewise rebuilds per frame —
    // ImGui::NewFrame).
    void beginFrame();

    bool hasContent() const noexcept { return !m_drawCommands.empty(); }

    // Rasterize the recorded draw list into the currently-bound render pass
    // (the caller opens/closes the pass on the composited frame FBO).
    // viewportWidth/Height are DEVICE pixels (the FBO extent); recorded
    // coordinates are view (logical) pixels — scaled by devicePixelRatio at
    // upload (ImGuiHelper.cpp:245-249 io.DisplayFramebufferScale).
    void flush(rhi::Driver& driver, float viewportWidth, float viewportHeight,
               float devicePixelRatio);

    // Destroy all GPU resources (lazy-created in flush: VBO/VBH/VBIH/IBH, the
    // primitive's VAO, and the program). Invoked from ~TargetImpl.
    void dispose(rhi::Driver& driver);

private:
    // One recorded draw command (ImDrawCmd equivalent — ImGuiHelper.cpp:269-321
    // assembles one filament Primitive per ImDrawCmd). vertex/index ranges
    // index into the frame's packed buffers; texture is the per-command
    // texture (or the white fallback for untextured geometry, mirroring
    // ImGuiHelper's material-without-texture path :310-314).
    struct DrawCommand {
        uint32_t firstIndex = 0;      // index offset into m_indices
        uint32_t indexCount = 0;
        rhi::TextureHandle texture;   // untextured → {} (white 1×1 fallback at flush)
        float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};  // flat tint (non-premultiplied RGBA)
    };

    // Packed draw-list buffers (ImGuiHelper populateVertexData :333-336).
    // Vertex layout: x, y (FLOAT2 view px), u, v (FLOAT2), rgba (4×UNORM8 —
    // UBYTE4 normalized, createVertexBuffer :335-340).
    struct Vertex { float x, y, u, v; uint8_t r, g, b, a; };
    std::vector<Vertex> m_vertices;
    std::vector<uint16_t> m_indices;   // ImDrawIdx is 16-bit by default
    std::vector<DrawCommand> m_drawCommands;

    // --- CPU-side path recording (CanvasDecoration semantics) ----------------
    // The current path is a polyline of points (beginPath resets it; moveTo
    // starts a subpath; lineTo appends; arc tessellates into the same list).
    // stroke()/fill() consume it into packed vertex/index + a DrawCommand.
    // 路径点：逻辑 px（平移已应用）。newSubpath = HTML canvas 的 moveTo 断子路径
    // 语义（moveTo 起新子路径、不产生线段）——十字装饰 moveTo/lineTo 两条独立
    // 臂靠它断开（2026-09-20 用户实测：跨子路径误连出右上斜线）。
    struct PathPoint { double x; double y; bool newSubpath; };
    std::vector<PathPoint> m_pathPoints;
    bool m_hasCursor = false;

    // Transform stack (translate-only 2D affine — composes additively)
    std::vector<std::pair<double, double>> m_translateStack;
    double m_tx = 0.0;
    double m_ty = 0.0;

    // Current style (persists across frames like a real canvas ctx).
    float m_strokeColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float m_fillColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    double m_globalAlpha = 1.0;
    double m_lineWidth = 1.0;

    // GPU resources (lazy — created on first flush; capacity grows on demand).
    // One program (kCanvas2dVert/kCanvas2dTexFrag), one VBO/IBH, one primitive
    // (TRIANGLES); per-frame content is re-uploaded. The white 1×1 texture is
    // the untextured fallback (ImGuiHelper's material-without-texture path).
    ShaderProgram m_program;
    uint32_t m_gpuVertexCapacity = 0;
    uint32_t m_gpuIndexCapacity = 0;
    rhi::VertexBufferInfoHandle m_vbih;
    rhi::VertexBufferHandle m_vbh;
    rhi::BufferObjectHandle m_vbo;
    rhi::IndexBufferHandle m_ibh;
    rhi::RenderPrimitiveHandle m_primitive;
    rhi::TextureHandle m_whiteTexture;

    // --- Path → packed buffers (per-op rasterization) ------------------------
    // stroke: the polyline is swept into a triangle strip of width
    // m_lineWidth (miter-joined; the locate circle / WindowArea rects are the
    // exercised shapes). fill: the closed polyline is triangulated as a fan
    // (the recorded shapes are convex; concave paths land as a registered gap).
    void strokePath();
    void fillPath();
    void emitTexturedQuad(rhi::TextureHandle texture, float x, float y, float w, float h,
                          float const color[4]);

    // Push one packed vertex; returns its index (uint16 — the draw list's
    // index type; ImDrawIdx is 16-bit by default).
    uint16_t pushVertex(float x, float y, float u, float v, float const rgba[4]);
};

END_DQ_RENDER_NAMESPACE

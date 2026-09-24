// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Canvas2d shader sources (2D canvas decoration GL rasterizer)
//
// APPROVED DEVIATION (预批准兜底，原 spec 2026-09-11-windowarea-look-design §2.4；
// spec 已随 2026-09-24 历史文档清理删除，偏差登记以本注释为准): the reference
// (itwinjs-core) rasterizes CanvasDecoration strokes into
// an HTML 2D canvas via CanvasRenderingContext2D (OnScreenTarget._2dCanvas,
// Target.ts:1408-1439) — no GLSL exists in the reference for this. DanQing's GL
// backend rasterizes the same stroked paths as GL_LINES inside the GL frame.
// Uniform/attribute naming follows sibling shaders (u_viewport — vec2 device-px
// viewport size, SceneCompositorImpl polyline path; u_color — RGBA stroke color).
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Canvas2d vertex shader
// ---------------------------------------------------------------------------
// Authored: GL 后端专属（参考无对应 GLSL——参考栅格化在 HTML 2D canvas）。
// a_position: vec2 视口像素坐标（设备像素；原点左上，y 向下——CanvasContext
// 契约坐标系）。u_viewport: 设备像素视口尺寸。像素 → 裁剪空间（y 翻转）。
static char const* kCanvas2dVert = R"glsl(
#version 410 core

layout(location = 0) in vec2 a_position;   // device px, origin top-left, y down

uniform vec2 u_viewport;                   // device px viewport size

void main()
{
    vec2 ndc = vec2(a_position.x / u_viewport.x * 2.0 - 1.0,
                    1.0 - a_position.y / u_viewport.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// Canvas2d fragment shader
// ---------------------------------------------------------------------------
// Authored: GL 后端专属。u_color 为非预乘 RGBA 描边色；输出预乘
// (rgb*a, a) 以匹配 TargetImpl 的 overlay render state 混合
// (ONE, ONE_MINUS_SRC_ALPHA——Target.ts _overlayRenderState)。
static char const* kCanvas2dFrag = R"glsl(
#version 410 core

uniform vec4 u_color;                      // stroke color (non-premultiplied)

out vec4 fragColor;

void main()
{
    fragColor = vec4(u_color.rgb * u_color.a, u_color.a);
}
)glsl";

// ---------------------------------------------------------------------------
// Canvas2d sprite (drawImage) shaders
// ---------------------------------------------------------------------------
// Authored: GL 后端专属（参考的 drawImage 栅格化在 HTML 2D canvas，
// Sprites.ts:131-135 ctx.drawImage(sprite.image, -offset.x, -offset.y)）。
// 顶点 = 2D canvas 像素坐标 + uv；片元 = straight-alpha 纹理采样转预乘，
// 匹配 overlay render state (ONE, ONE_MINUS_SRC_ALPHA)。
static char const* kCanvas2dSpriteVert = R"glsl(
#version 410 core

layout(location = 0) in vec2 a_position;   // device px, origin top-left, y down
layout(location = 1) in vec2 a_uv;         // texture coordinate

uniform vec2 u_viewport;                   // device px viewport size

out vec2 v_uv;

void main()
{
    vec2 ndc = vec2(a_position.x / u_viewport.x * 2.0 - 1.0,
                    1.0 - a_position.y / u_viewport.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_uv = a_uv;
}
)glsl";

static char const* kCanvas2dSpriteFrag = R"glsl(
#version 410 core

uniform sampler2D u_texture;               // straight-alpha RGBA

in vec2 v_uv;

out vec4 fragColor;

void main()
{
    vec4 t = texture(u_texture, v_uv);
    fragColor = vec4(t.rgb * t.a, t.a);    // straight → premultiplied
}
)glsl";

// ---------------------------------------------------------------------------
// Canvas2d unified draw-list shaders (packed pos/uv/color vertex —
// ImGuiHelper.cpp createVertexBuffer :326-342 layout).
// ---------------------------------------------------------------------------
// Authored: GL 后端专属。顶点布局 = a_position(FLOAT2) + a_uv(FLOAT2) +
// a_color(UBYTE4 normalized)；片元 = 纹理采样 × 顶点色（未打纹理时 u_texture
// 绑 1×1 白图，颜色全走顶点色——ImGuiHelper 的无纹理材质路径 :310-314 等价）。
// straight-alpha 纹理采样 × 顶点色后转预乘（overlay 混合 ONE/1-SRC_ALPHA）。
static char const* kCanvas2dListVert = R"glsl(
#version 410 core

layout(location = 0) in vec2 a_position;   // device px, origin top-left, y down
layout(location = 1) in vec2 a_uv;         // texture coordinate
layout(location = 2) in vec4 a_color;      // vertex color (normalized UBYTE4)

uniform vec2 u_viewport;                   // device px viewport size

out vec2 v_uv;
out vec4 v_color;

void main()
{
    vec2 ndc = vec2(a_position.x / u_viewport.x * 2.0 - 1.0,
                    1.0 - a_position.y / u_viewport.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_uv = a_uv;
    v_color = a_color;
}
)glsl";

static char const* kCanvas2dListFrag = R"glsl(
#version 410 core

uniform sampler2D u_texture;               // straight-alpha RGBA (or white fallback)

in vec2 v_uv;
in vec4 v_color;

out vec4 fragColor;

void main()
{
    vec4 t = texture(u_texture, v_uv) * v_color;
    fragColor = vec4(t.rgb * t.a, t.a);    // straight → premultiplied
}
)glsl";

END_DQ_RENDER_NAMESPACE

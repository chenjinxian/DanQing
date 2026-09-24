// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — CanvasDecoration + CanvasContext (2D overlay decoration contract)
//
// Ported from: itwinjs-core core/frontend/src/render/CanvasDecoration.ts:12-62
//              (CanvasDecoration interface) + the CanvasRenderingContext2D subset
//              implied by its drawDecoration(ctx) contract and the drawing
//              environment at internal/render/webgl/Target.ts:1403-1424
//              (drawOverlayDecorations: save/restore wrap + position translate).
//
// CanvasDecoration is the HTML 2D-canvas decoration mechanism mapped onto DanQing:
// tools/decorators add CanvasDecoration entries via DecorateContext::AddCanvasDecoration;
// the viewport's 2D overlay layer (OnScreenTarget._2dCanvas equivalent) invokes
// drawDecoration once per frame per entry, wrapped in save/restore with an
// optional position translate (Target.ts:1415-1423 semantics).
#pragma once

#include "Export.h"

#include <dqCommon/ColorDef.h>
#include <dqGeom/Point2d.h>
#include <dqRender/rhi/Handle.h>

#include <functional>
#include <optional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// CanvasContext — HTML CanvasRenderingContext2D 子集（CanvasDecoration.ts 的
// drawDecoration(ctx: CanvasRenderingContext2D) 契约）。
//
// 方法名 1:1 保留 HTML canvas 2D API（camelCase）；栅格化后端由渲染目标提供
// （现行后端 = dqRender 的 GLCanvasContext——GL 帧内 GL_LINES 栅格化，spec §2.4
// 预批准兜底；原 QPainter overlay 后端在原生 WGL 视口上导致 GPU TDR，已移除）。
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT CanvasContext {
public:
    virtual ~CanvasContext() = default;

    virtual void save() = 0;        // ← ctx.save()
    virtual void restore() = 0;     // ← ctx.restore()
    virtual void translate(double x, double y) = 0;  // ← ctx.translate(x, y)
    virtual void beginPath() = 0;   // ← ctx.beginPath()
    virtual void moveTo(double x, double y) = 0;     // ← ctx.moveTo(x, y)
    virtual void lineTo(double x, double y) = 0;     // ← ctx.lineTo(x, y)
    virtual void stroke() = 0;      // ← ctx.stroke()
    // ← ctx.strokeStyle（HTML canvas 接受 CSS 颜色串；DanQing 以 ColorDef 承载——
    //   参考 WindowAreaTool 的 black/white 选择即 ColorDef 比较，ViewTool.ts:3722）。
    virtual void setStrokeStyle(dqCommon::ColorDef color) = 0;
    virtual void setLineWidth(double w) = 0;         // ← ctx.lineWidth

    // ← ctx.fillStyle — locate 光圈填充（Viewport.ts:3768 "rgba(255,255,255,.2)"）。
    //   alpha 经 ColorDef 的 t 分量承载（0=不透明，与 dqRender 惯例一致）。
    virtual void setFillStyle(dqCommon::ColorDef color) = 0;

    // ← ctx.globalAlpha — sprite 透明叠加（SpriteLocation.activate 的 alpha 参数，
    //   Sprites.ts:108/132）。范围 [0,1]；默认 1。
    virtual void setGlobalAlpha(double alpha) = 0;

    // ← ctx.arc(x, y, radius, startAngle, endAngle) — 中心园/弧路径
    //   （Viewport.ts:3770/3776 locate 光圈；canvas 语义：向当前路径追加圆形子路径）。
    virtual void arc(double x, double y, double radius, double startAngle, double endAngle) = 0;

    // ← ctx.fill() — 填充当前路径（locate 光圈先 fill 后 stroke，Viewport.ts:3771-3772）。
    virtual void fill() = 0;

    // ← ctx.drawImage(image, dx, dy) — sprite 装饰路径（Sprites.ts:131-135:
    //   ctx.drawImage(sprite.image, -sprite.offset.x, -sprite.offset.y)，即以
    //   CanvasDecoration.position 为图像中心）。DanQing 的图像承载 = 已上传的
    //   rhi 纹理句柄（调用方经 RenderSystem.createTexture 创建——同 glTF
    //   baseColor 路径）+ 纹理自然尺寸（HTMLImageElement.naturalWidth/Height，
    //   Sprites.ts:52）。
    virtual void drawImage(rhi::TextureHandle texture, uint32_t width, uint32_t height,
                           double dx, double dy) = 0;
};

// ---------------------------------------------------------------------------
// CanvasDecoration — drawn onto the 2D canvas on top of a Viewport.
// Ported from: itwinjs-core CanvasDecoration (CanvasDecoration.ts:18-62)。
//
// W4 scope（参考成员的移植边界）：
//   - drawDecoration / position / pick 字段保留（pick 可空——W4 不实现鼠标反馈；
//     onMouseEnter/onMouseLeave/onMouseMove/propagateMouseMove/onMouseButton/onWheel/
//     decorationCursor 未移植——TODO：随有 pick 需求的装饰落地，CanvasDecoration.ts:38-61）。
// ---------------------------------------------------------------------------
struct CanvasDecoration {
    // Optional view coordinates position of this overlay decoration. If present,
    // ctx.translate is called with this point before drawDecoration.
    // ← CanvasDecoration.ts:28-31 (position?: XAndY)
    std::optional<dqGeom::Point2d> position;

    // Required draw method — called every time a frame is rendered. The context
    // TRANSFORM is saved before and restored after this call (Target.ts:1416/1422),
    // so implementers need not save/restore it themselves. Styles
    // (strokeStyle/lineWidth) are NOT wrapped — they persist into subsequent
    // decorations by design, so set them explicitly on every call (all ported
    // drawDecoration bodies do).
    // ← CanvasDecoration.ts:20-26 (drawDecoration(ctx))
    std::function<void(CanvasContext&)> drawDecoration;

    // Optional hit-test for mouse feedback (not wired in W4 — overlay is
    // mouse-transparent). ← CanvasDecoration.ts:32-37 (pick?(pt))
    std::function<bool(dqGeom::Point2d)> pick;
};

END_DQ_RENDER_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — DecorateContext for collecting decorations
//
// Ported from: itwinjs-core core/frontend/src/ViewContext.ts DecorateContext
// The mechanism by which decorators produce decorations. The Viewport creates
// a DecorateContext during CollectDecorations(), and decorators call methods
// on it to add graphics.
#pragma once

#include "Export.h"

#include <dqRender/Decorations.h>
#include <dqRender/RenderGraphic.h>

#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>

#include <vector>

namespace dqApp {

class DecorationsCache;
class IDecorator;
class Viewport;
struct CachedDecoration;

// ---------------------------------------------------------------------------
// DecorateContext — collects decorations from decorators
// Ported from: itwinjs-core DecorateContext (ViewContext.ts:171)
//
// Usage:
//   1. Viewport creates DecorateContext during CollectDecorations()
//   2. For each decorator, calls context.AddFromDecorator(decorator)
//   3. The decorator's Decorate(context) method calls context.AddDecoration()
//   4. Decorations are routed to the appropriate list in the Decorations object
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT DecorateContext {
public:
    // Create a DecorateContext for the given viewport.
    // @param viewport The viewport being decorated.
    // @param decorations The output Decorations container.
    // @param cache The viewport's DecorationsCache（Viewport.ts:3173
    //        _decorationCache；DecorateContext 构造 ViewContext.ts:182-186）。
    DecorateContext(Viewport& viewport, dqRender::Decorations& decorations,
                    DecorationsCache& cache);

    // add a decoration graphic to the appropriate list based on GraphicType.
    // ← DecorateContext.addDecoration(type, decoration)
    // Decorators call this from their Decorate() method.
    // 参考（ViewContext.ts:274-279）：cacheable decorator 的产出追加进
    // DecorationsCache（参考包 GraphicOwner——TS GC 所有权间接层；DanQing 缓存直接
    // 持有图形，clear 时 delete 等价 dispose），装饰列表持同一指针（非 owning）。
    void AddDecoration(dqRender::GraphicType type, dqRender::RenderGraphic* graphic);

    // Add a CanvasDecoration to be drawn in this context's Viewport (2D overlay layer).
    // Ported from: itwinjs-core DecorateContext.addCanvasDecoration
    //              (ViewContext.ts:312-325)。
    // 顺序语义（:320-324）：atFront 或列表为空 → push 尾部；否则 unshift 头部
    // （空表时 unshift==push，故 `atFront ? push_back : insert(begin)` 行为等价）。
    void AddCanvasDecoration(dqRender::CanvasDecoration decoration, bool atFront = false);

    // Set the skybox graphic.
    // ← DecorateContext.setSkyBox(graphic)
    void SetSkyBox(dqRender::RenderGraphic* graphic);

    // Set the view background graphic.
    // ← DecorateContext.setViewBackground(graphic)
    void SetViewBackground(dqRender::RenderGraphic* graphic);

    // Invoke a decorator's Decorate() method and collect its output.
    // ← DecorateContext.addFromDecorator(decorator)（ViewContext.ts:218-235）
    // 缓存语义：decorator->UseCachedDecorations() 且缓存命中 → RestoreCache
    // 直接重放不再 Decorate；未命中 → 置 m_curCacheableDecorator 后调
    // Decorate()，其间 AddDecoration 的产出追加进缓存（DecorationsCache.add）。
    void AddFromDecorator(IDecorator* decorator);

    // Draw the standard view grid as a WorldDecoration planar grid.
    // Ported from: itwinjs-core DecorateContext.drawStandardGrid
    //              (ViewContext.ts:341-352)。
    // @param gridOrigin grid plane origin
    // @param rMatrix grid orientation (rowX/rowY in-plane, rowZ normal)
    // @param spacing line spacing in X/Y (world units)
    // @param gridsPerRef reference-line period (0 = no reference lines)
    // @param isoGrid isometric grid — 参考实参固定 false 且实现忽略（_isoGrid）
    // @param fixedRepetitions 参考实现忽略（_fixedRepetitions），按签名 1:1 保留
    void DrawStandardGrid(dqGeom::Point3d const& gridOrigin, dqGeom::Matrix3d const& rMatrix,
                          dqGeom::Point2d const& spacing, double gridsPerRef,
                          bool isoGrid = false, dqGeom::Point2d const* fixedRepetitions = nullptr);

    // Get the viewport being decorated.
    Viewport& GetViewport() const { return m_viewport; }

private:
    // 重放缓存装饰（ViewContext.ts:238-252 restoreCache）。
    void RestoreCache(std::vector<CachedDecoration> const& cached);

    Viewport& m_viewport;
    dqRender::Decorations& m_decorations;
    DecorationsCache& m_cache;                       // ← ViewContext.ts:173 _cache
    IDecorator* m_curCacheableDecorator = nullptr;   // ← ViewContext.ts:174 _curCacheableDecorator
};

}  // namespace dqApp

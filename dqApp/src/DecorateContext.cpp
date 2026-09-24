// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — DecorateContext implementation
// Ported from: itwinjs-core core/frontend/src/ViewContext.ts DecorateContext
#include "dqApp/DecorateContext.h"
#include "dqApp/DecorationsCache.h"
#include "dqApp/Decorator.h"
#include "dqApp/Viewport.h"

#include <dqRender/PlanarGridProps.h>
#include <dqRender/RenderSystem.h>

#include <utility>

namespace dqApp {

DecorateContext::DecorateContext(Viewport& viewport, dqRender::Decorations& decorations,
                                 DecorationsCache& cache)
    : m_viewport(viewport)
    , m_decorations(decorations)
    , m_cache(cache)
{
}

// Ported from: itwinjs-core DecorateContext.addDecoration (ViewContext.ts:274-306)。
// :275-279 参考把 cacheable decorator 的图形产出包进 GraphicOwner 再入缓存——
// 那是 TS GC 语义下的所有权间接层。DanQing 的 DecorationsCache 直接持有图形本身
// （clear 时 delete，等价于参考的 dispose）；装饰列表持有同一裸指针（非 owning，
// 每次 CollectDecorations 前清空，生命周期由缓存保证）。不包 owner 的另一原因：
// DanQing 公开 RenderGraphicOwner 非内部 Graphic，入列表会被渲染路径
// static_cast<Graphic*> 误解析（参考的 GraphicOwner 是内部 Graphic 不存在此问题）。
void DecorateContext::AddDecoration(dqRender::GraphicType type, dqRender::RenderGraphic* graphic)
{
    if (!graphic) return;

    if (m_curCacheableDecorator) {
        CachedDecoration entry;
        entry.type = CachedDecoration::Type::Graphic;
        entry.graphic = graphic;  // 缓存持有（clear 时 delete）
        entry.graphicType = static_cast<uint32_t>(type);
        m_cache.add(m_curCacheableDecorator, std::move(entry));
    }

    m_decorations.add(type, graphic);
}

// Ported from: itwinjs-core DecorateContext.addCanvasDecoration (ViewContext.ts:312-325).
void DecorateContext::AddCanvasDecoration(dqRender::CanvasDecoration decoration, bool atFront)
{
    // :314-315 — _curCacheableDecorator 的 canvas 装饰缓存（DecorationsCache）未移植
    // （DanQing DecorationsCache 只缓存 cacheable decorator 的图形装饰）；TODO 随
    // CachedDecoration 机制落地。
    // :317-318 — canvasDecorations 惰性建表：C++ 成员恒在，无需判空。
    auto& list = m_decorations.canvasDecorations;
    // :320-324 — atFront 或空表 → push 尾部；否则 unshift 头部。
    if (list.empty() || atFront)
        list.push_back(std::move(decoration));
    else
        list.insert(list.begin(), std::move(decoration));
}

void DecorateContext::SetSkyBox(dqRender::RenderGraphic* graphic)
{
    m_decorations.skyBox = graphic;
}

void DecorateContext::SetViewBackground(dqRender::RenderGraphic* graphic)
{
    m_decorations.viewBackground = graphic;
}

// Ported from: itwinjs-core DecorateContext.addFromDecorator (ViewContext.ts:218-235)。
void DecorateContext::AddFromDecorator(IDecorator* decorator)
{
    if (!decorator) return;
    // 参考 :219 assert(undefined === this._curCacheableDecorator) —— 不嵌套。
    if (decorator->UseCachedDecorations()) {              // :221
        if (auto const* cached = m_cache.get(decorator)) {  // :222-226 命中 → 重放
            RestoreCache(*cached);
            return;
        }
        m_curCacheableDecorator = decorator;              // :228 未命中 → 记录
    }

    decorator->Decorate(*this);                           // :231

    m_curCacheableDecorator = nullptr;                    // :233 finally 清
}

// Ported from: itwinjs-core DecorateContext.restoreCache (ViewContext.ts:238-252)。
// DanQing CachedDecoration 只有 Graphic 一种类型（canvas/html 未移植）。
void DecorateContext::RestoreCache(std::vector<CachedDecoration> const& cached)
{
    for (auto const& entry : cached) {
        if (entry.type == CachedDecoration::Type::Graphic && entry.graphic) {
            // :241-243 — 同一 graphicOwner 重新入列（不重新入缓存）。
            m_decorations.add(static_cast<dqRender::GraphicType>(entry.graphicType),
                              entry.graphic);
        }
    }
}

// Ported from: itwinjs-core DecorateContext.drawStandardGrid (ViewContext.ts:341-352)。
void DecorateContext::DrawStandardGrid(dqGeom::Point3d const& gridOrigin,
                                       dqGeom::Matrix3d const& rMatrix,
                                       dqGeom::Point2d const& spacing, double gridsPerRef,
                                       bool /*isoGrid*/, dqGeom::Point2d const* /*fixedRepetitions*/)
{
    // :344-345 — if (vp.viewingGlobe) return。DanQing 无 globe 视图（blank connection
    // 恒非 viewingGlobe）——参考早退分支不可达，注释保留。
    dqRender::PlanarGridProps props;
    props.origin = gridOrigin;
    props.rMatrix = rMatrix;
    props.spacing = spacing;
    props.gridsPerRef = gridsPerRef;
    props.color = m_viewport.getContrastToBackgroundColor();  // :347

    // :348 — renderSystem.createPlanarGrid(vp.getFrustum(), props)。
    // vp.getFrustum() 默认 (world, adjustedBox=true) → GetFrustum(true)。
    auto* planarGrid = m_viewport.createPlanarGrid(m_viewport.getFrustum(true), props);
    if (planarGrid) {                                         // :349-351
        AddDecoration(dqRender::GraphicType::WorldDecoration, planarGrid);
    }
}

}  // namespace dqApp

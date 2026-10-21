// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Decorations cache for viewport decorators
// Ported from: itwinjs-core core/frontend/src/DecorationsCache.ts
//
// Caches decorations produced by decorators to avoid rebuilding every frame.
// Decorations are preserved until the viewport scene is invalidated.
#pragma once

#include "Export.h"

#include <dqRender/CanvasDecoration.h>  // M-X X-b1：canvas 变体（ViewContext.ts:314-315）
#include <dqRender/RenderGraphic.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace dqApp {

class IDecorator;
class Marker;  // M-X 修复轮 1：removeCanvasDecorationsForMarker 形参

// Cached decoration entry.
// Ported from: itwinjs-core DecorationsCache.ts CachedDecoration
//（参考为判别联合 {graphic} | {canvas,atFront} | {html}——html 变体随 XE6
// 不移植；C++ 平铺为单 struct + Type 判别，图形与画布字段分区）。
struct CachedDecoration {
    enum class Type : uint8_t {
        Graphic = 0,
        Canvas = 1,   // M-X X-b1——ViewContext.ts:16-18 的 "canvas" 变体
    };

    Type type = Type::Graphic;
    dqRender::RenderGraphic* graphic = nullptr;  // owned
    uint32_t graphicType = 0;  // normal/world/worldOverlay/viewOverlay
    // Canvas 变体字段——值拷贝（参考缓存同一 JS 对象；DanQing 拷贝的函子捕获
    // 同一 Marker*，悬停身份经 XE7 缝等价保持）。非 owning（Marker 归
    // MarkerSet/demo 所有）。
    dqRender::CanvasDecoration canvasDecoration;
    bool atFront = false;
};

// Decorations cache — maps decorators to their cached decorations.
// Ported from: itwinjs-core DecorationsCache.ts
class DQ_APP_EXPORT DecorationsCache {
public:
    DecorationsCache() = default;
    ~DecorationsCache();

    // Get cached decorations for a decorator. Returns nullptr if not cached.
    // Ported from: itwinjs-core DecorationsCache.get()
    std::vector<CachedDecoration> const* get(IDecorator const* decorator) const;

    // Add a decoration to the list of cached decorations for the decorator.
    // Ported from: itwinjs-core DecorationsCache.add() — no-op unless the
    // decorator is cacheable (UseCachedDecorations).
    void add(IDecorator* decorator, CachedDecoration decoration);

    // Store decorations for a decorator.
    // Ported from: itwinjs-core DecorationsCache.set()
    void set(IDecorator* decorator, std::vector<CachedDecoration> decorations);

    // Check if a decorator has cached decorations.
    bool has(IDecorator const* decorator) const;

    // Clear all cached decorations.
    // Ported from: itwinjs-core DecorationsCache.clear()
    void clear();

    // Remove cached decorations for a specific decorator.
    void remove(IDecorator const* decorator);

    // 清扫所有 decorator 桶中捕获指定 marker 的 Canvas 缓存条目（XE14 扩展
    // 清扫——修复轮 1 Important-1：~Marker 生命周期缝的缓存维。C++ 显式
    // 所有权适配，参考无对应物[GC]——Authored: no reference equivalent）。
    // 不触发任何回调（条目函子捕获的死指针不解引用——纯容器摘除）。
    void removeCanvasDecorationsForMarker(Marker const& marker);

private:
    std::unordered_map<IDecorator*, std::vector<CachedDecoration>> m_cache;
};

}  // namespace dqApp

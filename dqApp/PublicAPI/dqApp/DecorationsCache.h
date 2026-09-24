// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Decorations cache for viewport decorators
// Ported from: itwinjs-core core/frontend/src/DecorationsCache.ts
//
// Caches decorations produced by decorators to avoid rebuilding every frame.
// Decorations are preserved until the viewport scene is invalidated.
#pragma once

#include "Export.h"

#include <dqRender/RenderGraphic.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace dqApp {

class IDecorator;

// Cached decoration entry.
// Ported from: itwinjs-core DecorationsCache.ts CachedDecoration
struct CachedDecoration {
    enum class Type : uint8_t {
        Graphic = 0,
    };

    Type type = Type::Graphic;
    dqRender::RenderGraphic* graphic = nullptr;  // owned
    uint32_t graphicType = 0;  // normal/world/worldOverlay/viewOverlay
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

private:
    std::unordered_map<IDecorator*, std::vector<CachedDecoration>> m_cache;
};

}  // namespace dqApp

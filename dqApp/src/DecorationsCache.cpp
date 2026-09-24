// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — DecorationsCache implementation
// Ported from: itwinjs-core core/frontend/src/DecorationsCache.ts
#include "dqApp/DecorationsCache.h"
#include "dqApp/Decorator.h"

namespace dqApp {

DecorationsCache::~DecorationsCache()
{
    clear();
}

std::vector<CachedDecoration> const* DecorationsCache::get(IDecorator const* decorator) const
{
    auto it = m_cache.find(const_cast<IDecorator*>(decorator));
    if (it == m_cache.end()) return nullptr;
    return &it->second;
}

void DecorationsCache::set(IDecorator* decorator, std::vector<CachedDecoration> decorations)
{
    // Clear any existing cached decorations for this decorator.
    remove(decorator);
    m_cache[decorator] = std::move(decorations);
}

// Ported from: itwinjs-core DecorationsCache.add (DecorationsCache.ts:52-62)。
void DecorationsCache::add(IDecorator* decorator, CachedDecoration decoration)
{
    // 参考：assert(useCachedDecorations)；非 cacheable 直接不缓存（:53-55）。
    if (!decorator || !decorator->UseCachedDecorations())
        return;

    auto& list = m_cache[decorator];  // 惰性建表（:57-59）
    list.push_back(std::move(decoration));
}

bool DecorationsCache::has(IDecorator const* decorator) const
{
    return m_cache.find(const_cast<IDecorator*>(decorator)) != m_cache.end();
}

void DecorationsCache::clear()
{
    // Release all owned graphics.
    for (auto& [decorator, decorations] : m_cache) {
        for (auto& entry : decorations) {
            delete entry.graphic;
            entry.graphic = nullptr;
        }
    }
    m_cache.clear();
}

void DecorationsCache::remove(IDecorator const* decorator)
{
    auto it = m_cache.find(const_cast<IDecorator*>(decorator));
    if (it == m_cache.end()) return;

    for (auto& entry : it->second) {
        delete entry.graphic;
        entry.graphic = nullptr;
    }
    m_cache.erase(it);
}

}  // namespace dqApp

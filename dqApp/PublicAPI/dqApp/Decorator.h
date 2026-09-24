// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Decorator interface for viewport decorations
// Ported from: itwinjs-core core/frontend/src/ViewManager.ts Decorator interface
//
// Decorators own graphics and render them to the viewport. Each decorator
// implements Decorate() to add graphics via the DecorateContext, and can
// implement TestDecorationHit() to claim ownership of hit feature IDs.
#pragma once

#include "Export.h"
#include "DecorateContext.h"

#include <QString>

#include <cstdint>

namespace dqApp {

// ---------------------------------------------------------------------------
// IDecorator — interface for viewport decorations
// Ported from: itwinjs-core ViewManager.ts Decorator (line 23)
//
// Decorators are registered with ViewManager::AddDecorator(). During each
// frame, the Viewport calls DecorateContext::AddFromDecorator() for each
// registered decorator, which invokes the Decorate() method.
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT IDecorator {
public:
    virtual ~IDecorator() = default;

    /// If true, the viewport caches the most-recently-created decorations for
    /// this decorator and only invokes Decorate() when its cache is empty
    /// (the cache is cleared on Viewport::InvalidateScene).
    /// Ported from: itwinjs-core ViewportDecorator.useCachedDecorations (Viewport.ts:89).
    virtual bool UseCachedDecorations() const { return false; }

    /// add decorations to the context.
    /// Called by the Viewport during CollectDecorations() via DecorateContext.
    /// Implementations should call context.AddDecoration(type, graphic) to
    /// add their graphics.
    /// ← itwinjs-core Decorator.decorate(context)
    /// @param context The decoration context to add graphics to.
    virtual void Decorate(DecorateContext& context) = 0;

    /// Test if this decorator owns the given feature ID.
    /// @param featureId The feature ID from a pick query.
    /// @return true if this decorator claims the hit.
    virtual bool TestDecorationHit(uint32_t featureId) const = 0;

    /// Get a tooltip for the hit decoration.
    /// @param featureId The feature ID from a pick query.
    /// @return Tooltip text.
    virtual QString GetDecorationToolTip(uint32_t featureId) const = 0;
};

}  // namespace dqApp

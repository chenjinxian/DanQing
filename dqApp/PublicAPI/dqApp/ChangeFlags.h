// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ChangeFlags for viewport invalidation
//
// Ported from: itwinjs-core core/frontend/src/ChangeFlags.ts
// Bitmask flags that track what has changed in a Viewport, driving
// partial reconstruction during renderFrame().
#pragma once

#include "Export.h"

#include <cstdint>

namespace dqApp {

// Bitmask flags for viewport changes.
// Ported from: itwinjs-core ChangeFlag enum
enum class ChangeFlag : uint32_t {
    None                    = 0,
    alwaysDrawn             = 1 << 0,  // Always-drawn element set changed
    neverDrawn              = 1 << 1,  // Never-drawn element set changed
    ViewedCategories        = 1 << 2,  // Displayed categories changed
    ViewedModels            = 1 << 3,  // Displayed models changed
    DisplayStyle            = 1 << 4,  // DisplayStyleState or ViewFlags changed
    FeatureOverrideProvider = 1 << 5,  // Feature override provider changed
    ViewedCategoriesPerModel = 1 << 6, // Per-model category visibility changed
    ViewState               = 1 << 7,  // ViewState replaced via changeView
    All                     = 0x0fffffff,
};

// Bitwise operators for ChangeFlag
constexpr ChangeFlag operator|(ChangeFlag a, ChangeFlag b)
{
    return static_cast<ChangeFlag>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr ChangeFlag operator&(ChangeFlag a, ChangeFlag b)
{
    return static_cast<ChangeFlag>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

constexpr ChangeFlag operator~(ChangeFlag a)
{
    return static_cast<ChangeFlag>(~static_cast<uint32_t>(a));
}

constexpr ChangeFlag& operator|=(ChangeFlag& a, ChangeFlag b)
{
    return a = a | b;
}

constexpr bool HasFlag(ChangeFlag flags, ChangeFlag flag)
{
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

// All symbology-related flags (everything except ViewedModels and ViewState).
// ← itwinjs-core ChangeFlag.Overrides
constexpr ChangeFlag kOverridesFlags = ChangeFlag::All & ~(ChangeFlag::ViewedModels | ChangeFlag::ViewState);

// Initial state flags.
// ← itwinjs-core ChangeFlag.Initial
constexpr ChangeFlag kInitialFlags = ChangeFlag::ViewedCategories | ChangeFlag::ViewedModels | ChangeFlag::DisplayStyle;

// Read-only change flags.
// Ported from: itwinjs-core ChangeFlags
class DQ_APP_EXPORT ChangeFlags {
public:
    ChangeFlags() : m_flags(ChangeFlag::None) {}
    explicit ChangeFlags(ChangeFlag flags) : m_flags(flags) {}

    bool HasChanges() const { return m_flags != ChangeFlag::None; }

    // Are any symbology-related flags set?
    // ← itwinjs-core ChangeFlags.areFeatureOverridesDirty
    bool AreFeatureOverridesDirty() const
    {
        return HasFlag(m_flags, kOverridesFlags);
    }

    bool alwaysDrawn() const { return HasFlag(m_flags, ChangeFlag::alwaysDrawn); }
    bool neverDrawn() const { return HasFlag(m_flags, ChangeFlag::neverDrawn); }
    bool ViewedCategories() const { return HasFlag(m_flags, ChangeFlag::ViewedCategories); }
    bool ViewedModels() const { return HasFlag(m_flags, ChangeFlag::ViewedModels); }
    bool DisplayStyle() const { return HasFlag(m_flags, ChangeFlag::DisplayStyle); }
    bool FeatureOverrideProvider() const { return HasFlag(m_flags, ChangeFlag::FeatureOverrideProvider); }
    bool ViewedCategoriesPerModel() const { return HasFlag(m_flags, ChangeFlag::ViewedCategoriesPerModel); }
    bool ViewState() const { return HasFlag(m_flags, ChangeFlag::ViewState); }

protected:
    ChangeFlag m_flags;
};

// Mutable change flags with setter methods.
// Ported from: itwinjs-core MutableChangeFlags
class DQ_APP_EXPORT MutableChangeFlags : public ChangeFlags {
public:
    MutableChangeFlags() = default;
    explicit MutableChangeFlags(ChangeFlag flags) : ChangeFlags(flags) {}

    void setAlwaysDrawn() { m_flags |= ChangeFlag::alwaysDrawn; }
    void setNeverDrawn() { m_flags |= ChangeFlag::neverDrawn; }
    void SetViewedCategories() { m_flags |= ChangeFlag::ViewedCategories; }
    void SetViewedModels() { m_flags |= ChangeFlag::ViewedModels; }
    void SetDisplayStyle() { m_flags |= ChangeFlag::DisplayStyle; }
    void SetFeatureOverrideProvider() { m_flags |= ChangeFlag::FeatureOverrideProvider; }
    void SetViewedCategoriesPerModel() { m_flags |= ChangeFlag::ViewedCategoriesPerModel; }
    void SetViewState() { m_flags |= ChangeFlag::ViewState; }

    // Mark feature overrides as dirty (triggers recomputation in Step 9)
    // ← itwinjs-core: setFeatureOverrideProviderChanged()
    void SetFeatureOverridesDirty() { m_flags |= ChangeFlag::FeatureOverrideProvider; }

    // clear specified flags (default: all).
    void clear(ChangeFlag flags = ChangeFlag::All)
    {
        m_flags = m_flags & ~flags;
    }
};

}  // namespace dqApp

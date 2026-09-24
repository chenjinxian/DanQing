// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — QuantityFormatter (unit formatting)
// Ported from: itwinjs-core core/frontend/src/quantity-formatting/QuantityFormatter.ts
#pragma once

#include "Export.h"
#include <string>

namespace dqApp {

// ---------------------------------------------------------------------------
// QuantityFormatter — formats quantities with units for display
// Ported from: itwinjs-core QuantityFormatter.ts
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT QuantityFormatter {
public:
    QuantityFormatter() = default;
    virtual ~QuantityFormatter() = default;

    // Called after Application startup.
    // Ported from: itwinjs-core QuantityFormatter.onInitialized()
    virtual void onInitialized() {}

    // Get the active unit system.
    // Ported from: itwinjs-core QuantityFormatter.activeUnitSystem
    std::string const& getActiveUnitSystem() const noexcept { return m_activeUnitSystem; }
    void setActiveUnitSystem(std::string const& system) { m_activeUnitSystem = system; }

private:
    // Ported from: itwinjs-core QuantityFormatter.ts:407
    //              (_activeUnitSystem: UnitSystemKey = "imperial")
    std::string m_activeUnitSystem = "imperial";
};

}  // namespace dqApp

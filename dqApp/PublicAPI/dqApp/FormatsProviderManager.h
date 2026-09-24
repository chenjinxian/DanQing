// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — FormatsProviderManager (format provider management)
// Ported from: itwinjs-core core/frontend/src/quantity-formatting/QuantityFormatter.ts:362
//              (class FormatsProviderManager implements FormatsProvider)
#pragma once

#include "Export.h"

namespace dqApp {

// ---------------------------------------------------------------------------
// FormatsProviderManager — manages format providers for quantity display
// Ported from: itwinjs-core core/frontend/src/quantity-formatting/QuantityFormatter.ts:362
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT FormatsProviderManager {
public:
    FormatsProviderManager() = default;
    ~FormatsProviderManager() = default;
};

}  // namespace dqApp

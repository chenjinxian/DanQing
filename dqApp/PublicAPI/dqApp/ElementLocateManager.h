// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ElementLocateManager (element hit-testing)
// Ported from: itwinjs-core core/frontend/src/ElementLocateManager.ts
#pragma once

#include "Export.h"

namespace dqApp {

class Viewport;
struct ButtonEvent;

// ---------------------------------------------------------------------------
// ElementLocateManager — manages element hit-testing and locate operations
// Ported from: itwinjs-core ElementLocateManager.ts
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ElementLocateManager {
public:
    ElementLocateManager() = default;
    virtual ~ElementLocateManager() = default;

    // Called after Application startup.
    // Ported from: itwinjs-core ElementLocateManager.onInitialized()
    virtual void onInitialized() {}

    // Whether external iModels are allowed for locate.
    // Ported from: itwinjs-core ElementLocateManager.options.allowExternalIModels
    bool allowExternalIModels = false;
};

}  // namespace dqApp

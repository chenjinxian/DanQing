// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — UiAdmin (UI administration)
// Ported from: itwinjs-core ui/appui-abstract/src/appui-abstract/UiAdmin.ts
#pragma once

#include "Export.h"

namespace dqApp {

// ---------------------------------------------------------------------------
// UiAdmin — abstract UI administration interface
// Ported from: itwinjs-core ui/appui-abstract/src/appui-abstract/UiAdmin.ts
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT UiAdmin {
public:
    UiAdmin() = default;
    virtual ~UiAdmin() = default;

    // Called after Application startup.
    // Ported from: itwinjs-core UiAdmin.onInitialized()
    virtual void onInitialized() {}
};

}  // namespace dqApp

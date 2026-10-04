// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Cesium 陈列馆入口工具实现
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/EmptyExample.ts
#include "CesiumExampleTool.h"
#include "CesiumDecorator.h"

#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>

namespace Gui {

// 已挂装饰器（参考模块级单例——EmptyExample 的 decorator 生命周期挂在
// iModel.onClose；DanQing 以静态指针承载 toggle 态）。
namespace {
CesiumDecorator* s_decorator = nullptr;
}

// Ported from: EmptyExample.ts:14-48.
bool CesiumExampleTool::run()
{
    if (s_decorator != nullptr) {
        s_decorator->stop();
        s_decorator = nullptr;
        return true;
    }
    auto* vp = dqApp::Application::Get().GetViewManager().GetActiveViewport();
    if (vp == nullptr || vp->GetIModel() == nullptr)
        return false;
    s_decorator = CesiumDecorator::start(vp->GetIModel());
    return s_decorator != nullptr;
}

}  // namespace Gui

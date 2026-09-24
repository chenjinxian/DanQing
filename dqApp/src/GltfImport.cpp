// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts:149-221
#include "dqApp/GltfImport.h"
#include "dqApp/Application.h"
#include "dqApp/GltfDecoration.h"
#include "dqApp/Viewport.h"
#include "dqApp/ViewState.h"
#include "dqApp/ViewManager.h"

#include <QString>

#include <atomic>

namespace dqApp {

uint32_t NextGltfPickableId() {
    static std::atomic<uint32_t> s_counter{ 0 };
    return 0x80000000u | (s_counter.fetch_add(1, std::memory_order_relaxed) + 1);
}

std::unique_ptr<GltfDecoration> InstallGltfDecoration(
    Viewport& vp, std::unique_ptr<dqRender::GltfScene> scene, std::string const& name)
{
    // ← itwinjs-core GltfDecoration.ts:165-174 readGltfTemplate -> null guard
    if (!scene || scene->meshes.empty())
        return nullptr;

    // ← itwinjs-core GltfDecoration.ts:162-164 pickableOptions { id, modelId }
    //   modelId must differ from id; GltfDecoration encodes that internally.
    auto decoration = std::make_unique<GltfDecoration>(NextGltfPickableId(),
                                                       QString::fromStdString(name));

    // ← itwinjs-core GltfDecoration.ts:182-204 createGraphicFromTemplate + createGraphicOwner.
    //   GltfDecoration::SetScene builds the RenderGraphic + owner via the viewport's GL driver.
    decoration->SetScene(std::move(scene), vp);

    // ← itwinjs-core GltfDecoration.ts:208 IModelApp.viewManager.addDecorator(decorator)
    Application::Get().GetViewManager().AddDecorator(decoration.get());

    // ← itwinjs-core GltfDecoration.ts:210-215 graphic.unionRange(range); vp.view.lookAtVolume(range, aspect)
    auto* view = vp.GetView();
    if (auto* view3d = view ? view->AsViewState3d() : nullptr) {
        view3d->LookAtVolume(decoration->GetScene()->bounds);
        // 仅等价于参考 vp.synchWithView(...)（GltfDecoration.ts:213-214）的失效
        // 半边（invalidate / rebuild transform chain）；撤销记录半边（synchWithView
        // 的 saveViewUndo）未接。
        // TODO: loadGltf 接入时改为 vp.synchWithView()（参考 GltfDecoration.ts:213-214
        //       vp.synchWithView({ animateFrustumChange: true })）。
        vp.InvalidateController();
    }

    return decoration;  // ownership transferred to caller (must DropDecorator before destroy)
}

}  // namespace dqApp

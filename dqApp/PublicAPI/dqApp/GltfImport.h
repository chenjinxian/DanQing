// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts:149-221
//              (GltfDecorationTool.run — load → addDecorator → lookAtVolume → onClose cleanup)
//
// Installs a loaded glTF scene as a pickable GltfDecoration on a viewport and fits the
// view to it. Mirrors the reference's run() body (readGltfTemplate → createGraphicOwner →
// addDecorator → lookAtVolume).
#pragma once

#include "Export.h"

#include <cstdint>
#include <memory>
#include <string>

namespace dqRender { struct GltfScene; }
namespace dqApp {
class Viewport;
class GltfDecoration;

// Next reserved-range feature id for a glTF decoration pickable.
// Ported from: itwinjs-core GltfDecoration.ts:162 iModel.transientIds.getNext()
//              (transientIds not yet ported — Authored stand-in using a reserved
//               high range so glTF pick ids never collide with real element ids.)
DQ_APP_EXPORT uint32_t NextGltfPickableId();

// Install `scene` as a pickable GltfDecoration on `vp`: build the graphic, register the
// decorator with the ViewManager, and fit the viewport's view to scene->bounds.
// Returns ownership of the decoration to the caller (who must DropDecorator before
// destroying it). Returns nullptr if scene is null or has no meshes.
DQ_APP_EXPORT std::unique_ptr<GltfDecoration> InstallGltfDecoration(
    Viewport& vp, std::unique_ptr<dqRender::GltfScene> scene, std::string const& name);

}  // namespace dqApp

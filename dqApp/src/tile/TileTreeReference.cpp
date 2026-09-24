// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — tile tree reference implementation
// Ported from: itwinjs-core core/frontend/src/tile/TileTreeReference.ts
#include "dqApp/tile/TileTreeReference.h"

#include "dqApp/tile/SceneContext.h"

BEGIN_DQ_APP_NAMESPACE

void TileTreeReference::addToScene(SceneContext& context)
{
    // Ported from: TileTreeReference.addToScene (:71-75 → createDrawArgs →
    // draw). TileTree::draw collects the selected graphics into args.graphics
    // and the selection into ready/requested — both flow into the context
    // (the reference's TileGraphicType routing — BackgroundMap/Overlay —
    // lands together with those consumers; everything is Scene type today).
    auto args = createDrawArgs(context);
    if (!args)
        return;
    draw(*args);
    for (auto* graphic : args->graphics)
        context.outputGraphic(graphic);
    context.collectSelection(args->readyTiles, args->requestedTiles);
}

std::unique_ptr<dqRender::TileDrawArgs> TileTreeReference::createDrawArgs(SceneContext& context)
{
    // Ported from: TileTreeReference.createDrawArgs (:156-176 — owner.load,
    // then a per-reference TileDrawArgs from the viewport-level context).
    auto* tree = getTreeOwner().load();
    if (!tree)
        return nullptr;

    auto args = std::make_unique<dqRender::TileDrawArgs>(context.viewportDrawArgs());
    args->treeToWorld = computeTransform(*tree);
    return args;
}

END_DQ_APP_NAMESPACE

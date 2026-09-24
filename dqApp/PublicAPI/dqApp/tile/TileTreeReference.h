// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — tile tree reference (view-side seam into the scene)
// Ported from: itwinjs-core core/frontend/src/tile/TileTreeReference.ts
//
// DanQing adaptations (registered):
//  - Tooltip / map-feature-info / decorate / attributions / shadow-cast /
//    planar-clip-mask / geometry-collection surfaces omitted: they depend on
//    HitDetail, MapLayerFeatureInfo, DecorateContext etc. which DanQing has
//    not ported. Each lands with its owning feature.
//  - TileDrawArgs construction: the reference builds a fully-populated
//    TileDrawArgs (context/now/location/viewFlagOverrides/clipVolume/...,
//    TileTreeReference.ts:156-176); DanQing's TileDrawArgs carries the subset
//    the consumer supports today, filled by SceneContext + computeTransform.
#pragma once

#include "../Export.h"
#include "DisclosedTileTreeSet.h"
#include "TileTreeOwner.h"

#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>
#include <dqRender/tile/TileDrawArgs.h>
#include <dqRender/tile/TileTree.h>

#include <memory>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

class SceneContext;

// Describes the type of graphics produced by a TileTreeReference.
// Ported from: itwinjs-core TileGraphicType (TileTreeReference.ts:26-33).
enum class TileGraphicType : uint8_t {
    BackgroundMap = 0,  // rendered behind all other geometry without depth
    Scene = 1,          // rendered with normal scene graphics
    Overlay = 2,        // rendered in front of all other geometry
};

// A reference to a TileTree suitable for drawing within a Viewport. The
// reference does not *own* its tile tree — it refers to it by way of the
// tree's TileTreeOwner. The specific tree referenced may change based on
// viewport state. Typically associated with a ViewState, DisplayStyle, or
// Viewport; see TiledGraphicsProvider to supply custom references.
// Ported from: itwinjs-core TileTreeReference (TileTreeReference.ts:46-341).
class DQ_APP_EXPORT TileTreeReference {
public:
    virtual ~TileTreeReference() = default;

    /// The owner of the currently-referenced TileTree. Do not store a direct
    /// reference to it — it may change or become disposed at any time.
    /// Ported from: TileTreeReference.treeOwner (TileTreeReference.ts:48).
    virtual TileTreeOwner& getTreeOwner() = 0;

    /// Force a new tree owner / tile tree to be created for this reference.
    /// Ported from: TileTreeReference.resetTreeOwner (TileTreeReference.ts:58).
    virtual void resetTreeOwner() {}

    /// Disclose ALL TileTrees used by this reference (including auxiliary
    /// trees). Any tree NOT disclosed becomes a candidate for purging.
    /// Ported from: TileTreeReference.discloseTileTrees (:64-68).
    virtual void discloseTileTrees(DisclosedTileTreeSet& trees)
    {
        auto* tree = getTreeOwner().getTileTree();
        if (tree)
            trees.add(tree);
    }

    /// Adds this reference's graphics to the scene. By default invokes draw.
    /// Ported from: TileTreeReference.addToScene (:71-75).
    virtual void addToScene(SceneContext& context);

    /// Adds this reference's graphics by invoking TileTree.draw on the
    /// referenced tree, if loaded.
    /// Ported from: TileTreeReference.draw (:78-80).
    virtual void draw(dqRender::TileDrawArgs& args)
    {
        auto* tree = getTreeOwner().getTileTree();
        if (tree)
            tree->draw(args);
    }

    /// Unions this reference's range with `union` for fitting a viewport to
    /// its contents.
    /// Ported from: TileTreeReference.unionFitRange (:115-119).
    virtual void unionFitRange(dqGeom::Range3d& range)
    {
        dqGeom::Range3d const contentRange = computeWorldContentRange();
        if (!contentRange.isNull())
            range.ExtendRange(contentRange);
    }

    /// Record graphics memory consumed by this reference.
    /// Ported from: TileTreeReference.collectStatistics (:122-126).
    virtual void collectStatistics(dqRender::RenderMemory::Statistics& stats)
    {
        auto* tree = getTreeOwner().getTileTree();
        if (tree)
            tree->collectStatistics(stats);
    }

    /// Return true if the tile tree is fully loaded and ready to draw.
    /// NotLoaded/Loading → false; NotFound → true (we tried and failed);
    /// Loaded → _isLoadingComplete.
    /// Ported from: TileTreeReference.isLoadingComplete (:133-143 —
    /// non-const: the treeOwner property has reference semantics).
    bool isLoadingComplete()
    {
        switch (getTreeOwner().getLoadStatus()) {
            case dqRender::TileTreeLoadStatus::NotLoaded:
            case dqRender::TileTreeLoadStatus::Loading:
                return false;
            case dqRender::TileTreeLoadStatus::NotFound:
                return true;
            case dqRender::TileTreeLoadStatus::Loaded:
                return isLoadingCompleteOverride();
        }
        return true;
    }

    /// Create context for drawing the tile tree, if it is ready for drawing.
    /// Returns nullptr if, e.g., the tree is not yet loaded.
    /// Ported from: TileTreeReference.createDrawArgs (:156-176).
    virtual std::unique_ptr<dqRender::TileDrawArgs> createDrawArgs(SceneContext& context);

    /// Compute a transform from this reference's coordinate space to the
    /// iModel's coordinate space.
    /// Ported from: TileTreeReference.computeTransform (:199-201).
    virtual dqGeom::Transform computeTransform(dqRender::TileTree& tree)
    {
        return tree.getIModelTransform();
    }

    /// Compute the range of this tree's contents in world coordinates.
    /// Ported from: TileTreeReference.computeWorldContentRange (:206-213).
    virtual dqGeom::Range3d computeWorldContentRange()
    {
        dqGeom::Range3d range;
        auto* tree = getTreeOwner().getTileTree();
        if (tree && tree->getRootTile() && !tree->getRootTile()->getRange().isNull())
            range = computeTransform(*tree).MultiplyRange(tree->getRootTile()->getRange());
        return range;
    }

protected:
    /// Override if additional asynchronous loading is required after the tree
    /// loads. Ported from: TileTreeReference._isLoadingComplete (:148-150).
    virtual bool isLoadingCompleteOverride() const { return true; }
};

END_DQ_APP_NAMESPACE

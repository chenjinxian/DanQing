// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Scene graph nodes
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Graphic.ts
//
// The scene graph is a tree of Graphic nodes.  Each node knows how to add
// DrawCommands to the RenderCommands buffer.
#pragma once

#include "RenderCommands.h"
#include "dqRender/RenderGraphic.h"

#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>

#include <cstdint>
#include <memory>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class CachedGeometry;
class Primitive;

// ---------------------------------------------------------------------------
// Graphic — abstract scene graph node
// Ported from: itwinjs-core Graphic.ts Graphic (line 34-40)
//
// Inherits from public RenderGraphic so internal graphics can be stored
// in the public Scene::foreground/background/overlay lists.
// ---------------------------------------------------------------------------
class Graphic : public RenderGraphic {
public:
    virtual ~Graphic() = default;

    /// add draw commands for this graphic to the command buffer.
    virtual void addCommands(RenderCommands& commands) = 0;

    /// add hilite draw commands for this graphic.
    /// Ported from: itwinjs-core Graphic.ts addHiliteCommands()
    /// Default: asserts false (must be overridden by pickable graphics).
    virtual void addHiliteCommands(RenderCommands& commands, RenderPass pass);

    /// Collect render memory statistics.
    /// Ported from: itwinjs-core Graphic.ts collectStatistics()
    virtual void collectStatistics(/* RenderMemory::Statistics& stats */) const {}

    /// Extend the range by this graphic's bounds.
    /// Ported from: itwinjs-core Graphic.ts unionRange()
    void unionRange(dqGeom::Range3d& /*range*/) const override {}

    /// Return the Primitive if this is a Primitive, else nullptr.
    /// Ported from: itwinjs-core Graphic.ts toPrimitive()
    virtual Primitive* toPrimitive() { return nullptr; }

    /// Whether this graphic is pickable (should appear in pick buffer).
    /// Ported from: itwinjs-core Graphic.ts isPickable
    virtual bool isPickable() const { return false; }
};

// ---------------------------------------------------------------------------
// Primitive — leaf node wrapping CachedGeometry
// Ported from: itwinjs-core Primitive.ts (line 25-133)
// ---------------------------------------------------------------------------
class Primitive : public Graphic {
public:
    explicit Primitive(CachedGeometry* geometry, bool ownsGeometry = true)
        : m_geometry(geometry), m_ownsGeometry(ownsGeometry) {}

    ~Primitive() override;

    void addCommands(RenderCommands& commands) override;
    void addHiliteCommands(RenderCommands& commands, RenderPass pass) override;
    Primitive* toPrimitive() override { return this; }
    bool isPickable() const override { return false; }

    CachedGeometry* getGeometry() const noexcept { return m_geometry; }

    // Accessors — ported from: itwinjs-core Primitive.ts
    bool hasFeatures() const;
    bool hasAnimation() const;
    bool isInstanced() const;
    bool isLit() const;
    bool isEdge() const;
    uint8_t getRenderOrder() const;
    TechniqueId getTechniqueId() const;

private:
    CachedGeometry* m_geometry;
    bool m_ownsGeometry = true;
};

// ---------------------------------------------------------------------------
// GraphicsArray — holds an array of child graphics
// Ported from: itwinjs-core Graphic.ts GraphicsArray (line 502-539)
// ---------------------------------------------------------------------------
class GraphicsArray : public Graphic {
public:
    GraphicsArray() = default;

    void add(std::unique_ptr<Graphic> graphic) {
        m_children.push_back(std::move(graphic));
    }

    /// add a non-owning graphic (used for temporary wrappers like WorldDecorations).
    /// The caller must ensure the graphic outlives this array.
    void addNonOwning(Graphic* graphic) {
        m_nonOwningChildren.push_back(graphic);
    }

    size_t size() const noexcept { return m_children.size(); }

    void addCommands(RenderCommands& commands) override {
        for (auto& child : m_children)
            child->addCommands(commands);
        for (auto* child : m_nonOwningChildren)
            child->addCommands(commands);
    }

    void addHiliteCommands(RenderCommands& commands, RenderPass pass) override {
        for (auto& child : m_children)
            child->addHiliteCommands(commands, pass);
        for (auto* child : m_nonOwningChildren)
            child->addHiliteCommands(commands, pass);
    }

    void collectStatistics(/* RenderMemory::Statistics& stats */) const override {
        for (auto& child : m_children)
            child->collectStatistics(/* stats */);
        for (auto* child : m_nonOwningChildren)
            child->collectStatistics(/* stats */);
    }

    void unionRange(dqGeom::Range3d& range) const override {
        for (auto& child : m_children)
            child->unionRange(range);
        for (auto* child : m_nonOwningChildren)
            child->unionRange(range);
    }

    bool isPickable() const override {
        for (auto& child : m_children)
            if (child->isPickable()) return true;
        for (auto* child : m_nonOwningChildren)
            if (child->isPickable()) return true;
        return false;
    }

private:
    std::vector<std::unique_ptr<Graphic>> m_children;
    std::vector<Graphic*> m_nonOwningChildren;
};

// ---------------------------------------------------------------------------
// Branch — transform/symbology branch
// Ported from: itwinjs-core Graphic.ts Branch (line 337-436)
//
// Wraps a GraphicBranch (which holds entries/children) with a local-to-world
// transform and optional view flag overrides.
// ---------------------------------------------------------------------------
class Branch : public Graphic {
public:
    Branch() = default;

    void setChild(std::unique_ptr<Graphic> child) { m_child = std::move(child); }

    /// Get the child graphic (may be nullptr).
    Graphic* getChild() const noexcept { return m_child.get(); }

    // Set the transform for this branch (model-view and model-view-projection)
    void setTransform(float const* mv16, float const* mvp16) {
        for (int i = 0; i < 16; ++i) {
            m_mv[i] = mv16[i];
            m_mvp[i] = mvp16[i];
        }
        m_hasTransform = true;
    }

    // Set view flags for this branch
    void setViewFlags(BranchViewFlags const& flags) {
        m_viewFlags = flags;
        m_hasViewFlags = true;
    }

    // Set animation node ID for this branch
    // Ported from: itwinjs-core Graphic.ts Branch.animationNodeId
    void setAnimationNodeId(int32_t nodeId) {
        m_animationNodeId = nodeId;
        m_hasAnimationNodeId = true;
    }

    // Set group node ID for this branch
    // Ported from: itwinjs-core Graphic.ts Branch.groupNodeId
    void setGroupNodeId(int32_t nodeId) {
        m_groupNodeId = nodeId;
    }

    void addCommands(RenderCommands& commands) override;
    void addHiliteCommands(RenderCommands& commands, RenderPass pass) override;
    void unionRange(dqGeom::Range3d& range) const override;
    bool isPickable() const override;
    void collectStatistics(/* RenderMemory::Statistics& stats */) const override;

    // Accessors
    float const* getMv() const noexcept { return m_mv; }
    float const* getMvp() const noexcept { return m_mvp; }

    // Local-to-world (model) transform, carried separately from the combined
    // mv/mvp so BranchState.fromBranch can compose it through the stack.
    // Ported from: itwinjs-core Graphic.ts Branch.localToWorldTransform (line 339)
    dqGeom::Transform const& getLocalToWorld() const noexcept { return m_localToWorld; }
    void setLocalToWorld(dqGeom::Transform const& t) noexcept { m_localToWorld = t; }

    bool hasViewFlags() const noexcept { return m_hasViewFlags; }
    BranchViewFlags const& getViewFlags() const noexcept { return m_viewFlags; }
    bool hasAnimationNodeId() const noexcept { return m_hasAnimationNodeId; }
    int32_t getAnimationNodeId() const noexcept { return m_animationNodeId; }

private:
    // Check if commands should be added based on animation node compatibility.
    // Ported from: itwinjs-core Graphic.ts Branch.shouldAddCommands()
    bool shouldAddCommands() const;

    std::unique_ptr<Graphic> m_child;
    float m_mv[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float m_mvp[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    // Ported from: itwinjs-core Graphic.ts Branch.localToWorldTransform (line 339)
    dqGeom::Transform m_localToWorld;  // default: identity
    BranchViewFlags m_viewFlags;
    int32_t m_animationNodeId = -1;
    int32_t m_groupNodeId = -1;
    bool m_hasTransform = false;
    bool m_hasViewFlags = false;
    bool m_hasAnimationNodeId = false;
};

// ---------------------------------------------------------------------------
// AnimationTransformBranch — wraps a graphic with an animation node ID
// Ported from: itwinjs-core Graphic.ts AnimationTransformBranch (line 439-481)
// ---------------------------------------------------------------------------
class AnimationTransformBranch : public Graphic {
public:
    AnimationTransformBranch(std::unique_ptr<Graphic> graphic, uint32_t nodeId)
        : m_graphic(std::move(graphic)), m_nodeId(nodeId) {}

    void addCommands(RenderCommands& commands) override;
    void addHiliteCommands(RenderCommands& commands, RenderPass pass) override;

    void unionRange(dqGeom::Range3d& range) const override {
        if (m_graphic) m_graphic->unionRange(range);
    }

    bool isPickable() const override {
        return m_graphic ? m_graphic->isPickable() : false;
    }

private:
    std::unique_ptr<Graphic> m_graphic;
    uint32_t m_nodeId;
};

// ---------------------------------------------------------------------------
// WorldDecorations — branch for world-space decorations
// Ported from: itwinjs-core Graphic.ts WorldDecorations (line 484-500)
// ---------------------------------------------------------------------------
class WorldDecorations : public Branch {
public:
    WorldDecorations();
    void init(std::vector<std::unique_ptr<Graphic>> decorations);
};

END_DQ_RENDER_NAMESPACE

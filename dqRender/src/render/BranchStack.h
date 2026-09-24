// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Branch stack and branch state
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BranchStack.ts
//              + BranchState.ts
//
// Maintains a stack of BranchState objects.  Each push multiplies the current
// transform and replaces view flags; pop restores.
// This is the core scene graph traversal mechanism.
#pragma once

#include "TechniqueImpl.h"
#include "ViewFlags.h"
#include "EdgeSettings.h"

#include <dqGeom/Transform.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class Branch;
class ClipVolume;
class PlanarClassifier;
class TextureDrape;

// ---------------------------------------------------------------------------
// BranchState — state for a single branch level
// Ported from: itwinjs-core BranchState.ts BranchStateOptions + BranchState
//
// Contains all rendering state that changes as the scene graph is traversed:
// transform, view flags, symbology overrides, clip volume, classifiers, etc.
// ---------------------------------------------------------------------------
class BranchState {
public:
    BranchState();

    /// Construct with explicit options.
    BranchState(ViewFlags const& viewFlags,
                std::array<float, 16> const& mv,
                std::array<float, 16> const& mvp,
                bool is3d = true);

    // --- Accessors (Ported from: itwinjs-core BranchState getters) ---

    ViewFlags const& getViewFlags() const noexcept { return m_viewFlags; }
    void setViewFlags(ViewFlags const& vf);

    std::array<float, 16> const& getMv() const noexcept { return m_mv; }
    std::array<float, 16> const& getMvp() const noexcept { return m_mvp; }

    /// Model (local-to-world) transform.
    /// Ported from: itwinjs-core BranchState.transform
    /// Carried separately from the combined mv so BranchUniforms.update can
    /// compute mv = view * model at draw time (reference semantics).
    dqGeom::Transform const& getLocalToWorld() const noexcept { return m_localToWorld; }
    void setLocalToWorld(dqGeom::Transform const& t) noexcept { m_localToWorld = t; }

    TechniqueFlags const& getTechniqueFlags() const noexcept { return m_techniqueFlags; }

    ClipVolume* getClipVolume() const noexcept { return m_clipVolume; }
    void setClipVolume(ClipVolume* vol) noexcept { m_clipVolume = vol; }

    PlanarClassifier* getPlanarClassifier() const noexcept { return m_planarClassifier; }
    void setPlanarClassifier(PlanarClassifier* pc) noexcept { m_planarClassifier = pc; }

    std::vector<PlanarClassifier*> const& getSecondaryClassifiers() const noexcept { return m_secondaryClassifiers; }
    void setSecondaryClassifiers(std::vector<PlanarClassifier*> const& sc) { m_secondaryClassifiers = sc; }

    TextureDrape* getTextureDrape() const noexcept { return m_textureDrape; }
    void setTextureDrape(TextureDrape* td) noexcept { m_textureDrape = td; }

    EdgeSettings const& getEdgeSettings() const noexcept { return m_edgeSettings; }
    EdgeSettings& getEdgeSettings() noexcept { return m_edgeSettings; }
    void setEdgeSettings(EdgeSettings const& es) { m_edgeSettings = es; }

    bool is3d() const noexcept { return m_is3d; }
    void setIs3d(bool v) noexcept { m_is3d = v; }

    struct FrustumScale { float x = 1.0f; float y = 1.0f; };
    FrustumScale const& getFrustumScale() const noexcept { return m_frustumScale; }
    void setFrustumScale(FrustumScale const& s) noexcept { m_frustumScale = s; }

    bool isForceViewCoords() const noexcept { return m_forceViewCoords; }
    void setForceViewCoords(bool v) noexcept { m_forceViewCoords = v; }

    int32_t getGroupNodeId() const noexcept { return m_groupNodeId; }
    void setGroupNodeId(int32_t id) noexcept { m_groupNodeId = id; }

    bool isDisableClipStyle() const noexcept { return m_disableClipStyle; }
    void setDisableClipStyle(bool v) noexcept { m_disableClipStyle = v; }

    // Ported from: itwinjs-core BranchState options
    // These properties are part of the platform layer and will be
    // added when the corresponding types are ported:
    //   - appearanceProvider: FeatureAppearance provider for symbology
    //   - realityModelDisplaySettings: reality model rendering config
    //   - contourLine: contour line rendering settings
    //   - symbologyOverrides: FeatureSymbology.Overrides
    //   - iModel, transformFromIModel, viewAttachmentId: multi-iModel support

    // --- Methods (Ported from: itwinjs-core BranchState methods) ---

    /// Update view flags and related settings for a new render plan.
    /// Ported from: itwinjs-core BranchState.changeRenderPlan()
    void changeRenderPlan(ViewFlags const& viewFlags, bool is3d);

    /// Create a BranchState suitable for rendering decorations.
    /// Ported from: itwinjs-core BranchState.createForDecorations()
    static BranchState createForDecorations();

    /// Create a copy with forceViewCoords=true.
    /// Ported from: itwinjs-core BranchState.withViewCoords()
    BranchState withViewCoords() const;

    /// Create a BranchState from a parent state and a Branch node.
    /// Properties not specified by the Branch are inherited from prev.
    /// Ported from: itwinjs-core BranchState.fromBranch()
    static BranchState fromBranch(BranchState const& prev, Branch const& branch);

private:
    friend class BranchStack;  // BranchStack needs direct access for push/pushTransform
    void recomputeTechniqueFlags();

    // Transform (Ported from: itwinjs-core BranchStateOptions.transform)
    std::array<float, 16> m_mv  = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    std::array<float, 16> m_mvp = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    // Model (local-to-world) transform — Ported from: itwinjs-core BranchState.transform
    dqGeom::Transform m_localToWorld;  // default: identity

    // View flags (Ported from: itwinjs-core BranchStateOptions.viewFlags)
    ViewFlags m_viewFlags;
    TechniqueFlags m_techniqueFlags;  // Pre-computed from viewFlags

    // Clip volume (Ported from: itwinjs-core BranchStateOptions.clipVolume)
    ClipVolume* m_clipVolume = nullptr;

    // Planar classifiers (Ported from: itwinjs-core BranchStateOptions.planarClassifier)
    PlanarClassifier* m_planarClassifier = nullptr;
    std::vector<PlanarClassifier*> m_secondaryClassifiers;

    // Texture drape (Ported from: itwinjs-core BranchStateOptions.textureDrape)
    TextureDrape* m_textureDrape = nullptr;

    // Edge settings (Ported from: itwinjs-core BranchStateOptions.edgeSettings)
    EdgeSettings m_edgeSettings;

    // 3D flag (Ported from: itwinjs-core BranchStateOptions.is3d)
    bool m_is3d = true;

    // Frustum scale (Ported from: itwinjs-core BranchStateOptions.frustumScale)
    FrustumScale m_frustumScale;

    // Force view coords (Ported from: itwinjs-core BranchStateOptions.forceViewCoords)
    bool m_forceViewCoords = false;

    // Group node ID (Ported from: itwinjs-core BranchStateOptions.groupNodeId)
    int32_t m_groupNodeId = -1;

    // Disable clip style (Ported from: itwinjs-core BranchStateOptions.disableClipStyle)
    bool m_disableClipStyle = false;
};

// ---------------------------------------------------------------------------
// BranchStack — manages the branch state stack
// Ported from: itwinjs-core BranchStack.ts
//
// The stack starts with a single default BranchState.  As the scene graph
// is traversed, states are pushed (from branches) and popped.  The top
// state applies to all primitives being rendered.
// ---------------------------------------------------------------------------
class BranchStack {
public:
    /// Construct with a default initial state.
    /// Ported from: itwinjs-core BranchStack constructor
    BranchStack();

    /// Push a new branch state by combining the current state with a Branch.
    /// Ported from: itwinjs-core BranchStack.pushBranch()
    void pushBranch(Branch const& branch);

    /// Push an entire BranchState (used by PushStateCommand for decorations).
    /// Ported from: itwinjs-core BranchStack.pushState()
    void pushState(BranchState const& state);

    /// Push with transform and view flags (convenience for tests/legacy code).
    void push(std::array<float, 16> const& branchMv,
              std::array<float, 16> const& branchMvp,
              ViewFlags const& flags);

    /// Push with raw float pointers (convenience overload).
    void push(float const* mv16, float const* mvp16, ViewFlags const& flags)
    {
        std::array<float, 16> mv, mvp;
        for (int i = 0; i < 16; ++i) {
            mv[i] = mv16[i];
            mvp[i] = mvp16[i];
        }
        push(mv, mvp, flags);
    }

    /// Push with transform only (view flags inherited from parent).
    void pushTransform(std::array<float, 16> const& branchMv,
                       std::array<float, 16> const& branchMvp);

    /// Push with raw float pointers (convenience overload).
    void pushTransform(float const* mv16, float const* mvp16)
    {
        std::array<float, 16> mv, mvp;
        for (int i = 0; i < 16; ++i) {
            mv[i] = mv16[i];
            mvp[i] = mvp16[i];
        }
        pushTransform(mv, mvp);
    }

    /// Push with view flags only (transform inherited from parent).
    void pushViewFlags(ViewFlags const& flags);

    /// Pop the top branch state.
    /// Ported from: itwinjs-core BranchStack.pop()
    void pop();

    /// Get the current (top) branch state.
    /// Ported from: itwinjs-core BranchStack.top
    BranchState const& getTop() const;
    BranchState& getTop();

    /// Get the bottom (initial) branch state.
    /// Ported from: itwinjs-core BranchStack.bottom
    BranchState const& getBottom() const;

    /// Get the stack depth.
    /// Ported from: itwinjs-core BranchStack.length
    size_t getDepth() const noexcept { return m_stack.size(); }

    /// Check if the stack is empty.
    /// Ported from: itwinjs-core BranchStack.empty
    bool isEmpty() const noexcept { return m_stack.empty(); }

    /// Update the render plan on the bottom state.
    /// Ported from: itwinjs-core BranchStack.changeRenderPlan()
    void changeRenderPlan(ViewFlags const& viewFlags, bool is3d);

    /// Seed the bottom (base) state's view matrices: mv = view, mvp = proj·view.
    /// The reference composes u_mv = frustum.viewMatrix · model at draw time
    /// (BranchUniforms.update, BranchUniforms.ts:215-226); DanQing's dispatch uploads
    /// the branch-stack matrices directly, so the view enters through the base
    /// state instead — every branch push then yields mv = view·model and
    /// mvp = proj·view·model (reference-equivalent uniforms).
    void seedBaseTransforms(std::array<float, 16> const& mv, std::array<float, 16> const& mvp)
    {
        if (m_stack.empty()) {
            clear();
        }
        m_stack[0].m_mv = mv;
        m_stack[0].m_mvp = mvp;
    }

    /// Reset to a single default state.
    void clear();

    // --- Convenience accessors (for backward compatibility) ---
    std::array<float, 16> const& getCurrentMv() const { return getTop().getMv(); }
    std::array<float, 16> const& getCurrentMvp() const { return getTop().getMvp(); }

    /// Reset to the base state at `depth` (frame boundary cleanup).
    /// drawFrame pushes a viewport-transform state above the base and must unwind to
    /// exactly that depth after rendering — plain pop() only removes one level, so
    /// commands that pushed without popping (e.g. WorldOverlay decorations under the
    /// stack-driven overlay pass) would leak states into the next frame.
    void unwindToDepth(size_t depth)
    {
        while (m_stack.size() > depth)
            m_stack.pop_back();
    }
    ViewFlags const& getCurrentViewFlags() const { return getTop().getViewFlags(); }
    TechniqueFlags const& getCurrentTechniqueFlags() const { return getTop().getTechniqueFlags(); }

private:
    std::vector<BranchState> m_stack;
};

END_DQ_RENDER_NAMESPACE

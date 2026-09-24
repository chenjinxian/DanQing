// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Branch stack implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BranchStack.ts
//              + BranchState.ts
#include "BranchStack.h"
#include "Graphic.h"

#include <cassert>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Helper: 4×4 column-major matrix multiply (result = a * b)
// ---------------------------------------------------------------------------
static std::array<float, 16> multiplyMatrix(std::array<float, 16> const& a,
                                             std::array<float, 16> const& b)
{
    std::array<float, 16> r = {};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a[k * 4 + row] * b[col * 4 + k];
            }
            r[col * 4 + row] = sum;
        }
    }
    return r;
}

// ---------------------------------------------------------------------------
// Helper: compute TechniqueFlags from ViewFlags
// Ported from: itwinjs-core TechniqueFlags.ts init() (lines 81-125)
// ---------------------------------------------------------------------------
static TechniqueFlags computeTechniqueFlags(ViewFlags const& vf)
{
    TechniqueFlags flags;

    switch (vf.renderMode) {
        case RenderMode::Wireframe:
            flags.isEdgeTestNeeded = false;
            break;
        case RenderMode::HiddenLine:
        case RenderMode::SolidFill:
            flags.isEdgeTestNeeded = true;
            break;
        case RenderMode::SmoothShade:
            flags.isEdgeTestNeeded = vf.visibleEdges || vf.ambientOcclusion;
            break;
    }

    if (vf.forceSurfaceDiscard) {
        flags.isEdgeTestNeeded = false;
    }

    flags.isShadowable = vf.shadows;
    flags.isWiremesh = vf.wiremesh;

    return flags;
}

// ===========================================================================
// BranchState
// ===========================================================================

BranchState::BranchState()
    : m_viewFlags()
    , m_techniqueFlags(computeTechniqueFlags(m_viewFlags))
{
}

BranchState::BranchState(ViewFlags const& viewFlags,
                          std::array<float, 16> const& mv,
                          std::array<float, 16> const& mvp,
                          bool is3d)
    : m_mv(mv)
    , m_mvp(mvp)
    , m_viewFlags(viewFlags)
    , m_techniqueFlags(computeTechniqueFlags(viewFlags))
    , m_is3d(is3d)
{
}

void BranchState::setViewFlags(ViewFlags const& vf)
{
    m_viewFlags = vf;
    recomputeTechniqueFlags();
}

void BranchState::recomputeTechniqueFlags()
{
    m_techniqueFlags = computeTechniqueFlags(m_viewFlags);
}

// ---------------------------------------------------------------------------
// changeRenderPlan — update view flags and 3D mode
// Ported from: itwinjs-core BranchState.changeRenderPlan()
// ---------------------------------------------------------------------------
void BranchState::changeRenderPlan(ViewFlags const& viewFlags, bool is3d)
{
    setViewFlags(viewFlags);
    m_is3d = is3d;
    m_edgeSettings = EdgeSettings();  // Reset edge settings
}

// ---------------------------------------------------------------------------
// createForDecorations — state for decoration rendering
// Ported from: itwinjs-core BranchState.createForDecorations()
// ---------------------------------------------------------------------------
BranchState BranchState::createForDecorations()
{
    ViewFlags vf;
    vf.renderMode = RenderMode::SmoothShade;
    vf.lighting = false;
    vf.whiteOnWhiteReversal = false;

    BranchState state;
    state.setViewFlags(vf);
    state.m_is3d = true;
    // Symbology overrides with ignoreSubCategory = true
    // Ported from: itwinjs-core BranchState.createForDecorations()
    // Symbology overrides with ignoreSubCategory = true.
    // Requires FeatureSymbology::Overrides to be wired into BranchState.
    // state.m_symbologyOverrides.setIgnoreSubCategory(true);
    return state;
}

// ---------------------------------------------------------------------------
// withViewCoords — copy with forceViewCoords=true
// Ported from: itwinjs-core BranchState.withViewCoords()
// ---------------------------------------------------------------------------
BranchState BranchState::withViewCoords() const
{
    BranchState copy = *this;
    copy.m_forceViewCoords = true;
    return copy;
}

// ---------------------------------------------------------------------------
// fromBranch — create state from parent + branch
// Ported from: itwinjs-core BranchState.fromBranch()
// ---------------------------------------------------------------------------
BranchState BranchState::fromBranch(BranchState const& prev, Branch const& branch)
{
    BranchState state;

    // Convert raw float pointers to arrays for matrix multiply.
    auto toArray = [](float const* p) -> std::array<float, 16> {
        std::array<float, 16> a;
        for (int i = 0; i < 16; ++i) a[i] = p[i];
        return a;
    };

    // Transform: multiply parent with branch
    state.m_mv = multiplyMatrix(prev.m_mv, toArray(branch.getMv()));
    state.m_mvp = multiplyMatrix(prev.m_mvp, toArray(branch.getMvp()));

    // Model (local-to-world) transform: compose parent with the branch's own.
    // Ported from: itwinjs-core BranchState.fromBranch (BranchState.ts line 104):
    //   transform: prev.transform.multiplyTransformTransform(branch.localToWorldTransform)
    state.m_localToWorld = prev.m_localToWorld.MultiplyTransform(branch.getLocalToWorld());

    // View flags: branch overrides if specified, otherwise inherit
    if (branch.hasViewFlags()) {
        state.setViewFlags(branch.getViewFlags());
    } else {
        state.setViewFlags(prev.m_viewFlags);
    }

    // Inherit properties from parent
    state.m_clipVolume = prev.m_clipVolume;
    state.m_planarClassifier = prev.m_planarClassifier;
    state.m_secondaryClassifiers = prev.m_secondaryClassifiers;
    state.m_textureDrape = prev.m_textureDrape;
    state.m_edgeSettings = prev.m_edgeSettings;
    state.m_is3d = prev.m_is3d;
    state.m_frustumScale = prev.m_frustumScale;
    state.m_forceViewCoords = prev.m_forceViewCoords;
    state.m_groupNodeId = prev.m_groupNodeId;
    state.m_disableClipStyle = prev.m_disableClipStyle;

    // Ported from: itwinjs-core BranchState.fromBranch() (line 101-125)
    // The branch can override: clipVolume, planarClassifier, textureDrape,
    // edgeSettings, is3d, frustumScale, groupNodeId, disableClipStyle.
    // These properties are inherited from parent (done above).
    // Branch-level overrides require adding these properties to the Branch class.
    // Currently Branch only has: transform, viewFlags, animationNodeId, groupNodeId.

    return state;
}

// ===========================================================================
// BranchStack
// ===========================================================================

// ---------------------------------------------------------------------------
// Constructor — initialize with default state
// Ported from: itwinjs-core BranchStack constructor
// ---------------------------------------------------------------------------
BranchStack::BranchStack()
{
    m_stack.push_back(BranchState());
}

// ---------------------------------------------------------------------------
// pushBranch — push state derived from a Branch
// Ported from: itwinjs-core BranchStack.pushBranch()
// ---------------------------------------------------------------------------
void BranchStack::pushBranch(Branch const& branch)
{
    assert(!m_stack.empty());
    m_stack.push_back(BranchState::fromBranch(m_stack.back(), branch));
}

// ---------------------------------------------------------------------------
// pushState — push an explicit BranchState
// Ported from: itwinjs-core BranchStack.pushState()
// ---------------------------------------------------------------------------
void BranchStack::pushState(BranchState const& state)
{
    m_stack.push_back(state);
}

// ---------------------------------------------------------------------------
// push — push with transform and view flags (convenience)
// ---------------------------------------------------------------------------
void BranchStack::push(std::array<float, 16> const& branchMv,
                        std::array<float, 16> const& branchMvp,
                        ViewFlags const& flags)
{
    BranchState state;
    state.m_mv = multiplyMatrix(m_stack.back().getMv(), branchMv);
    state.m_mvp = multiplyMatrix(m_stack.back().getMvp(), branchMvp);
    state.setViewFlags(flags);
    // Inherit other properties
    state.m_clipVolume = m_stack.back().m_clipVolume;
    state.m_planarClassifier = m_stack.back().m_planarClassifier;
    state.m_textureDrape = m_stack.back().m_textureDrape;
    state.m_edgeSettings = m_stack.back().m_edgeSettings;
    state.m_is3d = m_stack.back().m_is3d;
    state.m_frustumScale = m_stack.back().m_frustumScale;
    state.m_forceViewCoords = m_stack.back().m_forceViewCoords;
    m_stack.push_back(state);
}

// ---------------------------------------------------------------------------
// pushTransform — push with transform only (view flags inherited)
// ---------------------------------------------------------------------------
void BranchStack::pushTransform(std::array<float, 16> const& branchMv,
                                 std::array<float, 16> const& branchMvp)
{
    BranchState state;
    state.m_mv = multiplyMatrix(m_stack.back().getMv(), branchMv);
    state.m_mvp = multiplyMatrix(m_stack.back().getMvp(), branchMvp);
    state.setViewFlags(m_stack.back().getViewFlags());
    // Inherit all other properties
    state.m_clipVolume = m_stack.back().m_clipVolume;
    state.m_planarClassifier = m_stack.back().m_planarClassifier;
    state.m_textureDrape = m_stack.back().m_textureDrape;
    state.m_edgeSettings = m_stack.back().m_edgeSettings;
    state.m_is3d = m_stack.back().m_is3d;
    state.m_frustumScale = m_stack.back().m_frustumScale;
    state.m_forceViewCoords = m_stack.back().m_forceViewCoords;
    m_stack.push_back(state);
}

// ---------------------------------------------------------------------------
// pushViewFlags — push with view flags only (transform inherited)
// ---------------------------------------------------------------------------
void BranchStack::pushViewFlags(ViewFlags const& flags)
{
    BranchState state;
    state.m_mv = m_stack.back().getMv();
    state.m_mvp = m_stack.back().getMvp();
    state.setViewFlags(flags);
    // Inherit all other properties
    state.m_clipVolume = m_stack.back().m_clipVolume;
    state.m_planarClassifier = m_stack.back().m_planarClassifier;
    state.m_textureDrape = m_stack.back().m_textureDrape;
    state.m_edgeSettings = m_stack.back().m_edgeSettings;
    state.m_is3d = m_stack.back().m_is3d;
    state.m_frustumScale = m_stack.back().m_frustumScale;
    state.m_forceViewCoords = m_stack.back().m_forceViewCoords;
    m_stack.push_back(state);
}

// ---------------------------------------------------------------------------
// pop
// Ported from: itwinjs-core BranchStack.pop()
// ---------------------------------------------------------------------------
void BranchStack::pop()
{
    assert(!m_stack.empty());
    if (m_stack.size() > 1) {  // Never pop the bottom state
        m_stack.pop_back();
    }
}

// ---------------------------------------------------------------------------
// getTop
// Ported from: itwinjs-core BranchStack.top
// ---------------------------------------------------------------------------
BranchState const& BranchStack::getTop() const
{
    assert(!m_stack.empty());
    return m_stack.back();
}

BranchState& BranchStack::getTop()
{
    assert(!m_stack.empty());
    return m_stack.back();
}

// ---------------------------------------------------------------------------
// getBottom
// Ported from: itwinjs-core BranchStack.bottom
// ---------------------------------------------------------------------------
BranchState const& BranchStack::getBottom() const
{
    assert(!m_stack.empty());
    return m_stack.front();
}

// ---------------------------------------------------------------------------
// changeRenderPlan
// Ported from: itwinjs-core BranchStack.changeRenderPlan()
// ---------------------------------------------------------------------------
void BranchStack::changeRenderPlan(ViewFlags const& viewFlags, bool is3d)
{
    assert(m_stack.size() == 1);
    m_stack.front().changeRenderPlan(viewFlags, is3d);
}

// ---------------------------------------------------------------------------
// clear — reset to single default state
// ---------------------------------------------------------------------------
void BranchStack::clear()
{
    m_stack.clear();
    m_stack.push_back(BranchState());
}

END_DQ_RENDER_NAMESPACE

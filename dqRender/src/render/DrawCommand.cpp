// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — DrawCommand implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/DrawCommand.ts
#include "DrawCommand.h"
#include "CachedGeometry.h"
#include "BranchStack.h"
#include "Batch.h"
#include "ClipStack.h"
#include "ClipVolume.h"
#include "TechniqueImpl.h"
#include "ShaderProgramExecutor.h"
#include "TargetImpl.h"
#include "SceneCompositorImpl.h"
#include "Graphic.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// PrimitiveCommand::execute — draw a geometry primitive
// Ported from: itwinjs-core DrawCommand.ts PrimitiveCommand.execute()
//
// Flow:
//   1. Get technique ID from the cached geometry
//   2. Build TechniqueFlags from target state
//   3. Get shader program from technique
//   4. Set program on executor
//   5. Draw the primitive via compositor
// ---------------------------------------------------------------------------
void PrimitiveCommand::execute(ShaderProgramExecutor& exec)
{
    if (!m_geometry) return;

    auto& target = exec.getTarget();
    auto techniqueId = m_geometry->getTechniqueId();

    // Build technique flags from current target state
    // Ported from: itwinjs-core DrawCommand.ts PrimitiveCommand.execute()
    TechniqueFlags flags;
    bool isInstanced = m_geometry->isInstanced();
    bool isAnimated = m_geometry->hasAnimation();
    bool usesQuantized = m_geometry->usesQuantizedPositions();
    auto const& vf = target.getCurrentViewFlags();
    bool wiremesh = vf.wiremesh &&
                    (techniqueId == TechniqueId::Surface || techniqueId == TechniqueId::RealityMesh);

    // Determine feature mode (None for normal rendering, Pick for readPixels)
    FeatureMode featureMode = target.isReadPixelsInProgress()
        ? FeatureMode::Pick : FeatureMode::None;

    // Check if geometry has a planar classifier
    bool classified = target.getPlanarClassifier() != nullptr;

    // Check if thematic display is wanted
    bool thematic = target.wantThematicDisplay() &&
                    m_geometry->supportsThematicDisplay();

    // Check if atmosphere is wanted
    bool enableAtmosphere = target.wantAtmosphere();

    // Check if shadow map is ready (for shadowable geometry)
    // Ported from: itwinjs-core PrimitiveCommand.execute() shadowable check
    bool shadowable = false;
    if (target.getShadowMap() && target.getShadowMap()->isEnabled) {
        // Only surface and reality mesh techniques support shadows
        shadowable = (techniqueId == TechniqueId::Surface ||
                      techniqueId == TechniqueId::RealityMesh) &&
                     vf.shadows && !thematic;
    }

    // Count clip planes from clip volume
    uint8_t numClipPlanes = 0;
    if (target.getClipVolume()) {
        numClipPlanes = 1;  // Simplified — actual count from clip planes
    }

    flags.init(exec.getRenderPass(),
               featureMode,
               numClipPlanes,
               isInstanced,
               isAnimated,
               classified,
               shadowable,
               thematic,
               wiremesh,
               usesQuantized ? PositionType::Quantized : PositionType::Unquantized,
               enableAtmosphere,
               target.is3d(),
               static_cast<int>(vf.renderMode),
               vf.visibleEdges,
               vf.forceSurfaceDiscard,
               target.wantAmbientOcclusion());

    // Get shader program from technique
    auto& techniques = target.getTechniques();
    auto* technique = techniques.getTechnique(techniqueId);
    if (!technique) return;

    auto* program = technique->getShader(flags);
    if (!program) return;

    // Set program and draw
    exec.setProgram(program);
    m_geometry->draw(target.getDriver());
}

// ---------------------------------------------------------------------------
// PushBranchCommand::execute — push branch onto stack
// Ported from: itwinjs-core DrawCommand.ts PushBranchCommand.execute()
// ---------------------------------------------------------------------------
void PushBranchCommand::execute(ShaderProgramExecutor& exec)
{
    auto& target = exec.getTarget();
    auto& branchStack = target.getBranchStack();
    // Create a BranchState from the stored transform and view flags
    // The BranchStack.pushState() handles the state composition
    std::array<float, 16> mv, mvp;
    for (int i = 0; i < 16; ++i) {
        mv[i] = m_mv[i];
        mvp[i] = m_mvp[i];
    }
    BranchState state(m_viewFlags, mv, mvp, target.is3d());
    branchStack.pushState(state);
}

// ---------------------------------------------------------------------------
// PopBranchCommand::execute — pop branch from stack
// Ported from: itwinjs-core DrawCommand.ts PopBranchCommand.execute()
// ---------------------------------------------------------------------------
void PopBranchCommand::execute(ShaderProgramExecutor& exec)
{
    auto& target = exec.getTarget();
    target.getBranchStack().pop();
}

// ---------------------------------------------------------------------------
// PushBatchCommand::execute — push batch onto batch state
// Ported from: itwinjs-core DrawCommand.ts PushBatchCommand.execute()
// ---------------------------------------------------------------------------
void PushBatchCommand::execute(ShaderProgramExecutor& /*exec*/)
{
    // BatchState.push() is called during scene graph traversal,
    // not during command execution. The batchId is set via setContext().
}

// ---------------------------------------------------------------------------
// PopBatchCommand::execute — pop batch from batch state
// Ported from: itwinjs-core DrawCommand.ts PopBatchCommand.execute()
// ---------------------------------------------------------------------------
void PopBatchCommand::execute(ShaderProgramExecutor& exec)
{
    auto& target = exec.getTarget();
    target.getBatchState().reset();
}

// ---------------------------------------------------------------------------
// PushClipCommand::execute — push clip volume onto clip stack
// Ported from: itwinjs-core DrawCommand.ts PushClipCommand.execute()
// ---------------------------------------------------------------------------
void PushClipCommand::execute(ShaderProgramExecutor& exec)
{
    if (m_clipVolume) {
        auto& target = exec.getTarget();
        auto& compositor = static_cast<SceneCompositor&>(target.getCompositor());
        compositor.getClipStack().push(*m_clipVolume);
    }
}

// ---------------------------------------------------------------------------
// PopClipCommand::execute — pop clip volume from clip stack
// Ported from: itwinjs-core DrawCommand.ts PopClipCommand.execute()
// ---------------------------------------------------------------------------
void PopClipCommand::execute(ShaderProgramExecutor& exec)
{
    auto& target = exec.getTarget();
    auto& compositor = static_cast<SceneCompositor&>(target.getCompositor());
    compositor.getClipStack().pop();
}

// ---------------------------------------------------------------------------
// PushStateCommand::execute — push a BranchState onto the target's BranchStack
// Ported from: itwinjs-core DrawCommand.ts PushStateCommand.execute()
// ---------------------------------------------------------------------------
void PushStateCommand::execute(ShaderProgramExecutor& exec)
{
    auto& target = exec.getTarget();
    target.getBranchStack().pushState(m_state);
}

END_DQ_RENDER_NAMESPACE

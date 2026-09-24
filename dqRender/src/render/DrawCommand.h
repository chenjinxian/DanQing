// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Draw command types
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/DrawCommand.ts
//
// Draw commands are produced by traversing the scene graph and sorted into
// render passes.  Each command either pushes/pops state or draws a primitive.
#pragma once

#include <cstdint>
#include "ViewFlags.h"  // Must be before namespace opening (includes dqCommon headers)

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class CachedGeometry;
class ShaderProgramExecutor;
class BranchState;  // Forward declaration (defined in BranchStack.h)

// ---------------------------------------------------------------------------
// DrawCommand opcodes
// (Ported from: itwinjs-core DrawCommand.ts)
// ---------------------------------------------------------------------------
enum class DrawCommandType : uint8_t {
    Primitive,
    PushBranch,
    PopBranch,
    PushBatch,
    PopBatch,
    PushClip,
    PopClip,
    PushState,  // Ported from: itwinjs-core DrawCommand.ts DrawOpCode.PushState
};

// ---------------------------------------------------------------------------
// DrawCommand — a single rendering command
// ---------------------------------------------------------------------------
class DrawCommand {
public:
    explicit DrawCommand(DrawCommandType type) : m_type(type) {}
    virtual ~DrawCommand() = default;

    DrawCommandType getType() const noexcept { return m_type; }

    /// Execute this command (replaces: command.execute(executor) in itwinjs).
    virtual void execute(ShaderProgramExecutor& executor) = 0;

private:
    DrawCommandType m_type;
};

// ---------------------------------------------------------------------------
// PrimitiveCommand — draws a geometry primitive
// ---------------------------------------------------------------------------
class PrimitiveCommand : public DrawCommand {
public:
    explicit PrimitiveCommand(CachedGeometry* geometry)
        : DrawCommand(DrawCommandType::Primitive), m_geometry(geometry) {}

    CachedGeometry* getGeometry() const noexcept { return m_geometry; }

    /// Set centroid position for depth sorting.
    void setCentroid(float x, float y, float z) noexcept
    {
        m_centroid[0] = x; m_centroid[1] = y; m_centroid[2] = z;
    }
    float const* getCentroid() const noexcept { return m_centroid; }

    void execute(ShaderProgramExecutor& executor) override;

private:
    CachedGeometry* m_geometry;
    float m_centroid[3] = {0, 0, 0};
};

// BranchViewFlags is now an alias for ViewFlags (defined in ViewFlags.h).
// Ported from: itwinjs-core core/common/src/ViewFlags.ts ViewFlagsProperties
using BranchViewFlags = ViewFlags;

// ---------------------------------------------------------------------------
// PushBranchCommand / PopBranchCommand — transform stack
// ---------------------------------------------------------------------------
class PushBranchCommand : public DrawCommand {
public:
    PushBranchCommand() : DrawCommand(DrawCommandType::PushBranch) {}

    // Set the model-view and model-view-projection matrices for this branch
    void setTransform(float const* mv16, float const* mvp16)
    {
        for (int i = 0; i < 16; ++i) {
            m_mv[i] = mv16[i];
            m_mvp[i] = mvp16[i];
        }
    }

    float const* getMv() const noexcept { return m_mv; }
    float const* getMvp() const noexcept { return m_mvp; }

    // Set view flags for this branch
    void setViewFlags(BranchViewFlags const& flags) { m_viewFlags = flags; m_hasViewFlags = true; }
    BranchViewFlags const& getViewFlags() const noexcept { return m_viewFlags; }
    bool hasViewFlags() const noexcept { return m_hasViewFlags; }

    void execute(ShaderProgramExecutor& executor) override;

private:
    float m_mv[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float m_mvp[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    BranchViewFlags m_viewFlags;
    bool m_hasViewFlags = false;
};

class PopBranchCommand : public DrawCommand {
public:
    PopBranchCommand() : DrawCommand(DrawCommandType::PopBranch) {}
    void execute(ShaderProgramExecutor& executor) override;
};

// ---------------------------------------------------------------------------
// PushBatchCommand / PopBatchCommand — feature batch state
// (Ported from: itwinjs-core DrawCommand.ts PushBatch/PopBatch)
// ---------------------------------------------------------------------------
class PushBatchCommand : public DrawCommand {
public:
    explicit PushBatchCommand(uint32_t batchId)
        : DrawCommand(DrawCommandType::PushBatch), m_batchId(batchId) {}

    uint32_t getBatchId() const noexcept { return m_batchId; }
    void execute(ShaderProgramExecutor& executor) override;

private:
    uint32_t m_batchId;
};

class PopBatchCommand : public DrawCommand {
public:
    PopBatchCommand() : DrawCommand(DrawCommandType::PopBatch) {}
    void execute(ShaderProgramExecutor& executor) override;
};

// ---------------------------------------------------------------------------
// PushClipCommand / PopClipCommand — clip volume state
// (Ported from: itwinjs-core DrawCommand.ts PushClip/PopClip)
// ---------------------------------------------------------------------------
class ClipVolume;

class PushClipCommand : public DrawCommand {
public:
    PushClipCommand() : DrawCommand(DrawCommandType::PushClip) {}
    explicit PushClipCommand(ClipVolume* vol)
        : DrawCommand(DrawCommandType::PushClip), m_clipVolume(vol) {}

    ClipVolume* getClipVolume() const noexcept { return m_clipVolume; }

    void execute(ShaderProgramExecutor& executor) override;

private:
    ClipVolume* m_clipVolume = nullptr;
};

class PopClipCommand : public DrawCommand {
public:
    PopClipCommand() : DrawCommand(DrawCommandType::PopClip) {}
    void execute(ShaderProgramExecutor& executor) override;
};

// ---------------------------------------------------------------------------
// PushStateCommand — push a BranchState onto the stack
// (Ported from: itwinjs-core DrawCommand.ts PushStateCommand)
//
// Used for decorations and other contexts where we want to push a synthetic
// BranchState (not derived from a Branch node).  PopBranchCommand is used
// to pop it (same as itwinjs-core).
// ---------------------------------------------------------------------------
class PushStateCommand : public DrawCommand {
public:
    explicit PushStateCommand(BranchState const& state)
        : DrawCommand(DrawCommandType::PushState), m_state(state) {}

    BranchState const& getState() const noexcept { return m_state; }
    void execute(ShaderProgramExecutor& executor) override;

private:
    BranchState const& m_state;
};

END_DQ_RENDER_NAMESPACE

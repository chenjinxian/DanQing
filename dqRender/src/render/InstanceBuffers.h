// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Instance buffers (per-instance GPU data)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/InstancedGeometry.ts
//
// Manages per-instance data for GPU instanced rendering:
// - Transform matrices (3×4 per instance, stored as 3 vec4 rows)
// - Feature IDs (optional, for per-instance picking)
// - Symbology overrides (optional, for per-instance color/weight)
//
// Contains: InstanceData, InstanceBuffers, PatternBuffers
#pragma once

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"
#include "dqRender/RenderMemory.h"

#include <dqGeom/Transform.h>

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Forward declarations
enum class TechniqueId : int32_t;

// ---------------------------------------------------------------------------
// InstanceData — base class for per-instance data with RTC transforms
// (Ported from: itwinjs-core InstancedGeometry.ts InstanceData, line 31-63)
//
// Manages relative-to-center (RTC) transforms for instanced rendering.
// The RTC center is subtracted from world positions to avoid floating-point
// precision issues far from the origin.
// ---------------------------------------------------------------------------
class InstanceData {
public:
    virtual ~InstanceData() = default;

    uint32_t getNumInstances() const noexcept { return m_numInstances; }

    /// Get the RTC model transform (rtcCenter * modelMatrix).
    /// Recomputes if the model matrix has changed.
    /// Ported from: itwinjs-core InstanceData.getRtcModelTransform
    void getRtcModelTransform(float const* modelMatrix16, float* out16) const;

    /// Get the RTC model transform = modelMatrix * rtcOnlyTransform, in Transform space.
    /// Recomputes only when modelMatrix changes (isAlmostEqual change detection).
    /// Ported from: itwinjs-core InstanceData.getRtcModelTransform (line 46-53)
    dqGeom::Transform getRtcModelTransform(dqGeom::Transform const& modelMatrix) const;

    /// Get the RTC-only transform (just the translation by rtcCenter).
    /// Ported from: itwinjs-core InstanceData.getRtcOnlyTransform
    void getRtcOnlyTransform(float* out16) const;

    /// Get the pattern feature ID (default: {0,0,0}).
    /// Ported from: itwinjs-core InstanceData.patternFeatureId
    virtual float const* getPatternFeatureId() const;

protected:
    InstanceData(uint32_t numInstances, float rtcCenterX, float rtcCenterY, float rtcCenterZ);

private:
    uint32_t m_numInstances = 0;
    float m_rtcCenter[3] = {0.0f, 0.0f, 0.0f};
    mutable float m_rtcModelTransform[16] = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
    };
    mutable float m_lastModelMatrix[16] = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
    };

    // Transform-space RTC cache (Ported from: itwinjs-core InstanceData line 34-38).
    // _rtcOnlyTransform = createTranslation(rtcCenter); _rtcModelTransform cached.
    dqGeom::Transform m_rtcOnlyTransform;           // createTranslation(rtcCenter)
    mutable dqGeom::Transform m_rtcModelTransformT;  // cached: modelMatrix * rtcOnly
    mutable dqGeom::Transform m_lastModelMatrixT;    // last modelMatrix for change detection
};

// ---------------------------------------------------------------------------
// InstanceBuffers — manages per-instance GPU buffers
// (Ported from: itwinjs-core InstancedGeometry.ts InstanceBuffers, line 156-267)
//
// Holds transform, feature ID, and symbology override buffers for instanced
// rendering.  Computes the bounding range of all instances.
// ---------------------------------------------------------------------------
class InstanceBuffers : public InstanceData {
public:
    InstanceBuffers(uint32_t count,
                     rhi::BufferObjectHandle transforms,
                     float rtcCenterX, float rtcCenterY, float rtcCenterZ,
                     rhi::BufferObjectHandle featureIds = {},
                     rhi::BufferObjectHandle symbology = {});
    ~InstanceBuffers() override = default;

    InstanceBuffers(InstanceBuffers const&) = delete;
    InstanceBuffers& operator=(InstanceBuffers const&) = delete;

    // --- Buffer accessors ---
    rhi::BufferObjectHandle getTransforms() const noexcept { return m_transforms; }
    rhi::BufferObjectHandle getFeatureIds() const noexcept { return m_featureIds; }
    rhi::BufferObjectHandle getSymbology() const noexcept { return m_symbology; }

    /// Whether this instance batch has per-instance feature IDs.
    bool hasFeatures() const noexcept { return static_cast<bool>(m_featureIds); }

    /// Whether the buffers are valid.
    bool isValid() const noexcept { return getNumInstances() > 0 && static_cast<bool>(m_transforms); }

    // --- Range ---
    struct Range3d {
        float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
        float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
    };
    Range3d const& getRange() const noexcept { return m_range; }

    // --- Statistics ---
    void collectStatistics(RenderMemory::Statistics& stats) const;

    // --- Static helpers ---

    /// Create buffer parameters for instance transform attributes.
    /// Ported from: itwinjs-core InstanceBuffers.createTransformBufferParameters
    /// Returns 3 BufferParameters for a_instanceMatrixRow0/1/2.
    struct TransformBufferParams {
        uint32_t locations[3] = {0, 0, 0};
        uint32_t stride = 0;
        uint32_t offsets[3] = {0, 0, 0};
    };
    static TransformBufferParams getTransformBufferParams(TechniqueId techId);

    /// Compute the bounding range of all instances.
    /// Ported from: itwinjs-core InstanceBuffers.computeRange
    static Range3d computeRange(float const* reprMin, float const* reprMax,
                                 float const* transforms, uint32_t numFloats,
                                 float const* rtcCenter);

    // --- Factory ---
    /// Create from raw transform data.
    static InstanceBuffers* create(
        rhi::Driver& driver, uint32_t count,
        float const* transforms, float const* transformCenter,
        float const* featureIds = nullptr, float const* symbology = nullptr);

private:
    rhi::BufferObjectHandle m_transforms;
    rhi::BufferObjectHandle m_featureIds;
    rhi::BufferObjectHandle m_symbology;
    Range3d m_range;
};

// ---------------------------------------------------------------------------
// PatternBuffers — per-instance data for area patterns
// (Ported from: itwinjs-core InstancedGeometry.ts PatternBuffers, line 270-342)
//
// Area patterns use XY offsets instead of full transforms.  The pattern
// parameters control spacing, scale, and origin.
// ---------------------------------------------------------------------------
class PatternBuffers : public InstanceData {
public:
    struct PatternTransforms {
        float orgTransform[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        float localToModel[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        float symbolToLocal[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        float origin[2] = {0.0f, 0.0f};
    };

    PatternBuffers(uint32_t count,
                    rhi::BufferObjectHandle offsets,
                    float const* patternParams4,
                    float const* origin2,
                    float const* orgTransform16,
                    float const* localToModel16,
                    float const* symbolToLocal16,
                    uint32_t featureId = 0,
                    bool hasFeatureId = false);
    ~PatternBuffers() override = default;

    // --- Buffer access ---
    rhi::BufferObjectHandle getOffsets() const noexcept { return m_offsets; }

    // --- Pattern parameters ---
    /// [isAreaPattern, spacingX, spacingY, scale]
    float const* getPatternParams() const noexcept { return m_patternParams; }

    /// Pattern transforms for the vertex shader.
    PatternTransforms const& getPatternTransforms() const noexcept { return m_transforms; }

    /// Feature ID for the entire pattern.
    bool hasFeatureId() const noexcept { return m_hasFeatureId; }
    float const* getPatternFeatureId() const override;

    // --- Range ---
    InstanceBuffers::Range3d const& getRange() const noexcept { return m_range; }

    // --- Statistics ---
    void collectStatistics(RenderMemory::Statistics& stats) const;

    // --- Factory ---
    static PatternBuffers* create(
        rhi::Driver& driver,
        uint32_t count, float const* xyOffsets,
        float spacingX, float spacingY, float scale,
        float originX, float originY,
        float const* orgTransform16,
        float const* patternToModel16,
        float const* symbolTranslation16,
        uint32_t featureId = 0);

private:
    rhi::BufferObjectHandle m_offsets;
    float m_patternParams[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    PatternTransforms m_transforms;
    float m_featureId[3] = {0.0f, 0.0f, 0.0f};
    bool m_hasFeatureId = false;
    InstanceBuffers::Range3d m_range;
};

END_DQ_RENDER_NAMESPACE

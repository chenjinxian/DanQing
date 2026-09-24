// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Target-level uniform aggregator
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/TargetUniforms.ts
//
// Aggregates the per-Target uniform subsystems. Modules already extracted to
// dedicated 1:1 files are included here; the remaining few (StyleUniforms,
// BatchUniforms, BranchUniforms) are still stubs pending extraction.
#pragma once

#include "FrustumUniforms.h"
#include "HiliteUniforms.h"
#include "LightingUniforms.h"
#include "StyleUniforms.h"
#include "Sync.h"
#include "ViewRectUniforms.h"

#include "Batch.h"  // Batch + BatchState (for setCurrentBatch)
#include "InstancedGeometry.h"  // InstancedGeometry (for BranchUniforms.update instanced path)
#include "ThematicSensors.h"  // ThematicSensors (for bindNumThematicSensors)
#include "gl/RenderFlags.h"  // EmphasisFlags (for bindUniformSymbologyFlags)

#include "UniformHandle.h"
#include "FeatureOverrideLUT.h"
#include "dqRender/rhi/DriverEnums.h"

#include <dqCommon/OvrFlags.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class RenderSystemImpl;  // forward-declare for bindLUT (avoids circular include)

// ---------------------------------------------------------------------------
// BatchUniforms — per-batch uniforms  [STUB — pending 1:1 extraction]
// (Ported from: itwinjs-core BatchUniforms.ts — not yet faithfully ported;
//  the real implementation requires Target/BatchState/FeatureOverrides wiring)
// ---------------------------------------------------------------------------
class BatchUniforms {
public:
    void setBatchId(uint32_t id)
    {
        m_batchId = id;
        // Encode as 4 bytes for shader
        m_batchIdBytes[0] = static_cast<float>(id & 0xFF);
        m_batchIdBytes[1] = static_cast<float>((id >> 8) & 0xFF);
        m_batchIdBytes[2] = static_cast<float>((id >> 16) & 0xFF);
        m_batchIdBytes[3] = static_cast<float>((id >> 24) & 0xFF);
    }

    void setFeatureMode(uint8_t mode) noexcept { m_featureMode = mode; }

    void bindBatchId(UniformHandle& uniform) const
    {
        uniform.setUniform4fv(m_batchIdBytes);
    }

    // --- Uniform override binds (faithful ports from BatchUniforms.ts 156-201) ---
    // These read the 8-byte uniform encoding exposed by FeatureOverrideLUT.

    /// Set the feature-override LUT backing the uniform binds.
    void setOverrides(FeatureOverrideLUT const* ovr) noexcept { m_overrides = ovr; }

    // vec3: overridden rgb, or (-1,-1,-1) sentinel.
    // Ported from: itwinjs-core BatchUniforms.bindUniformColorOverride
    void bindUniformColorOverride(UniformHandle& uniform) const
    {
        static float const kNoOverride[3] = {-1.0f, -1.0f, -1.0f};
        if (nullptr == m_overrides) {
            uniform.setUniform3fv(kNoOverride);
            return;
        }
        uint8_t const* uo = m_overrides->getUniformOverrides();
        if (uo[0] & static_cast<uint8_t>(dqCommon::OvrFlag::Rgb)) {
            float rgb[3] = {uo[4] / 255.0f, uo[5] / 255.0f, uo[6] / 255.0f};
            uniform.setUniform3fv(rgb);
        } else {
            uniform.setUniform3fv(kNoOverride);
        }
    }

    // float: overridden alpha, or -1 sentinel.
    // Ported from: itwinjs-core BatchUniforms.bindUniformTransparencyOverride
    void bindUniformTransparencyOverride(UniformHandle& uniform) const
    {
        if (nullptr != m_overrides) {
            uint8_t const* uo = m_overrides->getUniformOverrides();
            if (uo[0] & static_cast<uint8_t>(dqCommon::OvrFlag::Alpha)) {
                uniform.setUniform1f(uo[7] / 255.0f);
                return;
            }
        }
        uniform.setUniform1f(-1.0f);
    }

    // int: 1 if the current batch is non-locatable (and not ignored).
    // Ported from: itwinjs-core BatchUniforms.bindUniformNonLocatable
    void bindUniformNonLocatable(UniformHandle& uniform, bool ignoreNonLocatable) const
    {
        int nonLocatable = 0;
        if (!ignoreNonLocatable && nullptr != m_overrides) {
            uint8_t const* uo = m_overrides->getUniformOverrides();
            nonLocatable = (uo[0] & static_cast<uint8_t>(dqCommon::OvrFlag::NonLocatable)) ? 1 : 0;
        }
        uniform.setUniform1i(nonLocatable);
    }

    // vec2: LUT {width, height} params.
    // Ported from: itwinjs-core BatchUniforms.bindLUTParams
    void bindLUTParams(UniformHandle& uniform) const
    {
        if (nullptr != m_overrides) {
            float lp[2];
            m_overrides->getLutParams(lp);
            uniform.setUniform2fv(lp);
        }
    }

    /// float: uniform symbology flags (EmphasisFlags bitmask), or 0 if no overrides.
    /// Ported from: itwinjs-core BatchUniforms.bindUniformSymbologyFlags +
    ///               FeatureOverrides.updateUniformSymbologyFlags (line 71-92)
    void bindUniformSymbologyFlags(UniformHandle& uniform) const
    {
        if (nullptr == m_overrides) {
            uniform.setUniform1f(0.0f);
            return;
        }
        // The uniform symbology flags only apply to the single-feature (uniform)
        // case; a multi-feature LUT is consumed via the texture, not this uniform.
        GL::EmphasisFlags flags = GL::EmphasisFlags::None;
        if (m_overrides->isUniform()) {
            uint8_t const* uo = m_overrides->getUniformOverrides();
            // Low byte (OvrFlag): Flashed / NonLocatable.
            if (uo[0] & static_cast<uint8_t>(dqCommon::OvrFlag::Flashed))
                flags = flags | GL::EmphasisFlags::Flashed;
            if (uo[0] & static_cast<uint8_t>(dqCommon::OvrFlag::NonLocatable))
                flags = flags | GL::EmphasisFlags::NonLocatable;
            // High byte (OvrFlags16): Hilite / Emphasized, gated on anyHilited.
            if (m_overrides->anyHilited()) {
                if (uo[1] & static_cast<uint8_t>(dqCommon::OvrFlags16::Hilited))
                    flags = flags | GL::EmphasisFlags::Hilite;
                if (uo[1] & static_cast<uint8_t>(dqCommon::OvrFlags16::Emphasized))
                    flags = flags | GL::EmphasisFlags::Emphasized;
            }
        }
        uniform.setUniform1f(static_cast<float>(static_cast<uint8_t>(flags)));
    }

    /// Set the per-batch thematic sensors (nullptr -> bindNumThematicSensors no-ops).
    /// Ported from: itwinjs-core BatchUniforms._sensors (set in _setCurrentBatch;
    ///  wiring through setCurrentBatch depends on Batch.getThematicSensors +
    ///  Target.wantThematicSensors/plan.thematic, not yet present).
    void setSensors(ThematicSensors const* sensors) noexcept { m_sensors = sensors; }

    /// Bind the number of thematic sensors (no-op if none).
    /// Ported from: itwinjs-core BatchUniforms.bindNumThematicSensors()
    void bindNumThematicSensors(UniformHandle& uniform) const
    {
        if (nullptr != m_sensors)
            m_sensors->bindNumSensors(uniform);
    }

    /// Bind the feature-override LUT texture + sampler uniform.
    /// Ported from: itwinjs-core FeatureOverrides.bindLUT() + Texture.bindSampler()
    ///               (line 447-452 + 474-479). Two halves: (1) bind the GPU texture
    ///               to the driver unit, (2) set the sampler uniform to the 0-based
    ///               unit index.  The `system` pointer is nullable for GL-free tests
    ///               (live binding lambdas always pass &target->getSystem()).
    /// Definition is out-of-line in BatchUniforms.cpp (needs RenderSystemImpl).
    void bindLUT(UniformHandle& uniform, RenderSystemImpl* system, uint32_t unit) const;

    // TODO: bindThematicSensors (GPU texture bind),
    //       bindContourLUT/bindContourLUTWidth — these need GPU texture binding +
    //       the _contours member plumbed through setCurrentBatch (depend on a
    //       Contours class + Batch.getThematicSensors/getContours + Target state,
    //       not yet present).

    /// Set the current batch: assign batchId, resolve overrides + feature mode.
    /// Ported from: itwinjs-core BatchUniforms.setCurrentBatch() (line 51-92)
    /// TODO: thematic sensors + contours (need ThematicSensors/Contours wiring).
    void setCurrentBatch(Batch& batch, BatchState& state)
    {
        state.assignBatchId(batch);  // assign unique batch ID (reference push does this internally)
        state.push(batch);
        uint32_t const batchId = state.getCurrentBatchId();
        setBatchId(batchId);

        FeatureOverrideLUT& lut = batch.getOrCreateFeatureOverrideLUT();
        m_overrides = lut.anyOverridden() ? &lut : nullptr;
        // FeatureMode: Overrides if any override active, else Pick if batched, else None.
        m_featureMode = (nullptr != m_overrides) ? uint8_t(2)
                      : (0 != batchId)        ? uint8_t(1)
                                              : uint8_t(0);
    }

    /// Clear the current batch.
    /// Ported from: itwinjs-core BatchUniforms.clearCurrentBatch()
    void clearCurrentBatch(BatchState& state)
    {
        state.pop();
        m_overrides = nullptr;
        m_featureMode = 0;
        setBatchId(0);
    }

    uint32_t getBatchId() const noexcept { return m_batchId; }
    uint8_t getFeatureMode() const noexcept { return m_featureMode; }

private:
    uint32_t m_batchId = 0;
    float m_batchIdBytes[4] = {0, 0, 0, 0};
    uint8_t m_featureMode = 0;  // 0=None, 1=Pick, 2=Overrides
    FeatureOverrideLUT const* m_overrides = nullptr;
    ThematicSensors const* m_sensors = nullptr;  // Ported from BatchUniforms._sensors
};

// ---------------------------------------------------------------------------
// BranchUniforms — per-branch uniforms
// (Ported from: itwinjs-core BranchUniforms.ts update() line 181-257)
// Inherits SyncTarget to enable change-tracking for uniform uploads.
// ---------------------------------------------------------------------------
class BranchUniforms : public SyncTarget {
public:
    // UpdateParams — per-call inputs to update()
    // (Ported from: itwinjs-core BranchUniforms.update() reads these from the
    //  geometry (asInstanced, viewIndependentOrigin, asSurface.mesh) and from
    //  this._target (currentTransform, devicePixelRatio, wantThematicDisplay,
    //  uniforms.batch.wantContourLines). They are bundled here so update() can
    //  stay a pure computation, avoiding a TargetImpl <-> Uniforms.h circular
    //  include; the caller extracts them from the geometry/target.)
    struct UpdateParams {
        bool wantThematic = false;                       // target.wantThematicDisplay
        bool isViewCoords = false;                       // bindModelView*(uniform, geom, isViewCoords)
        InstancedGeometry const* instancedGeom = nullptr; // geometry.asInstanced (nullptr = not instanced)
        dqGeom::Point3d const* viewIndependentOrigin = nullptr;  // geometry.viewIndependentOrigin
        double devicePixelRatio = 1.0;                   // target.devicePixelRatio (viewCoords path)
        bool wantContourLines = false;                   // target.uniforms.batch.wantContourLines
        bool hasConstantLodVParams = false;              // geometry.asSurface?.mesh.constantLodVParams
    };

    void setModelViewMatrix(float const* mv)
    {
        for (int i = 0; i < 16; ++i) m_mv[i] = mv[i];
        desync();
    }

    void setModelViewProjectionMatrix(float const* mvp)
    {
        for (int i = 0; i < 16; ++i) m_mvp[i] = mvp[i];
        desync();
    }

    void setModelMatrix(float const* m)
    {
        for (int i = 0; i < 16; ++i) m_model[i] = m[i];
        desync();
    }

    void bindModelViewMatrix(UniformHandle& uniform) const
    {
        uniform.setUniform4fv(m_mv, 4);
    }

    void bindModelViewProjectionMatrix(UniformHandle& uniform) const
    {
        uniform.setUniform4fv(m_mvp, 4);
    }

    void bindModelMatrix(UniformHandle& uniform) const
    {
        uniform.setUniform4fv(m_model, 4);
    }

    float const* getModelViewMatrix() const noexcept { return m_mv; }
    float const* getModelViewProjectionMatrix() const noexcept { return m_mvp; }
    float const* getModelMatrix() const noexcept { return m_model; }
    float const* getViewMatrix3() const noexcept { return m_view; }

    /// Compute mv/mvp/m32/v32 from the target's model, view, and projection.
    /// Ported from: itwinjs-core BranchUniforms.update() (line 181-257).
    /// Sync-token caching (reference lines 182-199) is deferred — it is a perf
    /// optimization that does not change computed values; TODO port when wiring
    /// bind*() into the live draw path (needs the SyncToken system).
    void update(dqGeom::Transform const& model,
                dqGeom::Transform const& view,
                dqGeom::Matrix4d const& projection,
                UpdateParams const& params)
    {
        bool const instanced = (nullptr != params.instancedGeom);
        dqGeom::Point3d const* const vio = params.viewIndependentOrigin;

        dqGeom::Transform mv;  // model-view (Transform space)
        if (params.isViewCoords) {
            // Zero out Z column of the model matrix ("silly clipping tools").
            // Ported from: itwinjs-core BranchUniforms.update line 205-206.
            mv = model;
            mv.matrix.coffs[2] = mv.matrix.coffs[5] = mv.matrix.coffs[8] = 0.0;
            if (instanced)
                mv = params.instancedGeom->getRtcModelTransform(mv);

            // Scale based on device-pixel ratio about the origin.
            // Ported from: itwinjs-core BranchUniforms.update line 210-213.
            dqGeom::Transform const scaleView = dqGeom::Transform::CreateScaleAboutPoint(
                dqGeom::Point3d::FromZero(), params.devicePixelRatio);
            mv = scaleView.MultiplyTransform(mv);
        } else {
            dqGeom::Transform const& viewMatrix = view;  // frustum.viewMatrix
            if (instanced) {
                // For instanced geometry the "model view" is really a transform from
                // center of the instanced range to view; the shader applies the
                // per-instance transform. Ported from line 217-227.
                if (nullptr != vio) {
                    dqGeom::Matrix3d viewToWorldRot;
                    viewMatrix.matrix.Inverse(viewToWorldRot);
                    dqGeom::Transform const rotateAboutOrigin =
                        dqGeom::Transform::CreateFixedPointAndMatrix(*vio, viewToWorldRot);
                    dqGeom::Transform const viModelMatrix =
                        rotateAboutOrigin.MultiplyTransform(params.instancedGeom->getRtcModelTransform(model));
                    mv = viewMatrix.MultiplyTransform(viModelMatrix);
                } else {
                    mv = viewMatrix.MultiplyTransform(params.instancedGeom->getRtcModelTransform(model));
                }
            } else {
                // Ported from: itwinjs-core BranchUniforms.update line 228-237.
                if (nullptr != vio) {
                    dqGeom::Matrix3d viewToWorldRot;
                    viewMatrix.matrix.Inverse(viewToWorldRot);
                    dqGeom::Transform const rotateAboutOrigin =
                        dqGeom::Transform::CreateFixedPointAndMatrix(*vio, viewToWorldRot);
                    dqGeom::Transform const viModelMatrix =
                        rotateAboutOrigin.MultiplyTransform(model);
                    mv = viewMatrix.MultiplyTransform(viModelMatrix);
                } else {
                    mv = viewMatrix.MultiplyTransform(model);
                }
            }
        }

        // m32/v32 population (thematic uses model + view.matrix; contour/constant-lod
        // uses model only). Ported from: itwinjs-core BranchUniforms.update line 240-245.
        if (params.wantThematic) {
            Matrix4 const m32 = Matrix4::fromTransform(model);
            std::memcpy(m_model, m32.data, sizeof(m_model));
            Matrix3 const v32 = Matrix3::fromMatrix3d(view.matrix);
            std::memcpy(m_view, v32.data, sizeof(m_view));
        } else if (params.wantContourLines || params.hasConstantLodVParams) {
            Matrix4 const m32 = Matrix4::fromTransform(model);
            std::memcpy(m_model, m32.data, sizeof(m_model));
        }

        // mv -> mv32. Ported from: itwinjs-core BranchUniforms.update line 247-248.
        Matrix4 const mv32 = Matrix4::fromTransform(mv);
        std::memcpy(m_mv, mv32.data, sizeof(m_mv));

        // mvp = projection * mv (skip for instanced — not used by the shader).
        // Ported from: itwinjs-core BranchUniforms.update line 251-254.
        if (!instanced) {
            dqGeom::Matrix4d const mvp = projection.MultiplyMatrixMatrix(dqGeom::Matrix4d::CreateTransform(mv));
            Matrix4 const mvp32 = Matrix4::fromMatrix4d(mvp);
            std::memcpy(m_mvp, mvp32.data, sizeof(m_mvp));
        }
    }

    /// Convenience overload: common case (non-viewCoords, non-instanced, non-VIO,
    /// no contour). Equivalent to UpdateParams{wantThematic}.
    void update(dqGeom::Transform const& model,
                dqGeom::Transform const& view,
                dqGeom::Matrix4d const& projection,
                bool wantThematic)
    {
        UpdateParams p;
        p.wantThematic = wantThematic;
        update(model, view, projection, p);
    }

    // mat3: world-to-view rotation (thematic).
    // Ported from: itwinjs-core BranchUniforms.bindWorldToViewNTransform()
    void bindWorldToViewNTransform(UniformHandle& uniform) const
    {
        uniform.setMatrix3(m_view);
    }

private:
    float m_mv[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float m_mvp[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float m_model[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float m_view[9] = {1,0,0, 0,1,0, 0,0,1};
};

// ---------------------------------------------------------------------------
// SunDirection — sun direction uniform, transformed into view space
// Ported from: itwinjs-core TargetUniforms.SunDirection
//
// The sun direction is supplied to the shader in view coordinates, so it must
// be recomputed whenever the frustum (view matrix) changes. If no world-space
// sun direction is supplied, a default view-space direction is used.
// ---------------------------------------------------------------------------
class SunDirection {
public:
    SunDirection() noexcept
        : m_viewDir(dqGeom::Vector3d::From(kDefaultX, kDefaultY, kDefaultZ))
    {
        storeViewDir();
    }

    /// Set the world-space sun direction (nullptr -> use default view-space dir).
    /// Ported from: itwinjs-core SunDirection.update()
    void update(dqGeom::Vector3d const* sunDir) noexcept
    {
        bool const haveWorldDir = (nullptr != sunDir);
        if (haveWorldDir != m_haveWorldDir
            || (sunDir != nullptr && !sunDir->IsEqual(m_worldDir))) {
            m_haveWorldDir = haveWorldDir;
            if (nullptr != sunDir) {
                m_worldDir = *sunDir;
                m_worldDir.Normalize();
            }
            m_updated = true;
        }
    }

    /// Recompute the view-space direction from the frustum view matrix and bind it.
    /// Ported from: itwinjs-core SunDirection.bind()
    void bind(UniformHandle& uniform, FrustumUniforms const& frustum) noexcept
    {
        if (m_updated) {
            if (m_haveWorldDir) {
                m_viewDir = frustum.getViewMatrix().matrix.MultiplyVector(m_worldDir);
                m_viewDir.x = -m_viewDir.x;  // viewDir.negate()
                m_viewDir.y = -m_viewDir.y;
                m_viewDir.z = -m_viewDir.z;
            } else {
                m_viewDir = dqGeom::Vector3d::From(kDefaultX, kDefaultY, kDefaultZ);
            }
            m_viewDir.Normalize();
            storeViewDir();
            m_updated = false;
        }
        uniform.setUniform3fv(m_viewDir32);
    }

    /// View-space sun direction (the 3 floats bind() would upload). For the
    /// legacy-map upload path (compositor frame-constant uniforms) — same value
    /// the ProgramUniform binding supplies on the binding-dispatch path.
    float const* getSunDirView() const noexcept { return m_viewDir32; }

private:
    void storeViewDir() noexcept
    {
        m_viewDir32[0] = static_cast<float>(m_viewDir.x);
        m_viewDir32[1] = static_cast<float>(m_viewDir.y);
        m_viewDir32[2] = static_cast<float>(m_viewDir.z);
    }

    // Reference default: new Vector3d(0.272166, 0.680414, 0.680414)
    static constexpr double kDefaultX = 0.272166;
    static constexpr double kDefaultY = 0.680414;
    static constexpr double kDefaultZ = 0.680414;

    bool m_haveWorldDir = false;
    dqGeom::Vector3d m_worldDir = dqGeom::Vector3d::From(0.0, 0.0, 1.0);
    dqGeom::Vector3d m_viewDir;
    float m_viewDir32[3] = {static_cast<float>(kDefaultX),
                           static_cast<float>(kDefaultY),
                           static_cast<float>(kDefaultZ)};
    bool m_updated = true;
};

// ---------------------------------------------------------------------------
// TargetUniforms — aggregates all target-level uniform state
// (Ported from: itwinjs-core TargetUniforms.ts)
//
// frustum/viewRect/hilite/lights/style are the faithful 1:1 implementations.
// branch/batch remain stubs pending extraction.
// TODO: PixelWidthFactor (needs branch.top.frustumScale); full updateRenderPlan
// (needs plan.hiliteSettings/emphasisSettings/lights — absent from the public
// RenderPlan stub); thematic/contours/shadow/realityModel/atmosphere members.
// ---------------------------------------------------------------------------
class TargetUniforms {
public:
    FrustumUniforms frustum;
    ViewRectUniforms viewRect;
    LightingUniforms lights;
    StyleUniforms style;
    HiliteUniforms hilite;
    BranchUniforms branch;
    BatchUniforms batch;

    /// Get the projection matrix (viewRect for view-coords, frustum otherwise).
    /// Ported from: itwinjs-core TargetUniforms.getProjectionMatrix()
    dqGeom::Matrix4d const& getProjectionMatrix(bool forViewCoords) const noexcept
    {
        return forViewCoords ? viewRect.getProjectionMatrix() : frustum.getProjectionMatrix();
    }

    /// Get the float32 projection matrix.
    /// Ported from: itwinjs-core TargetUniforms.getProjectionMatrix32()
    Matrix4 const& getProjectionMatrix32(bool forViewCoords) const noexcept
    {
        return forViewCoords ? viewRect.getProjectionMatrix32() : frustum.getProjectionMatrix32();
    }

    /// Bind the projection matrix.
    /// Ported from: itwinjs-core TargetUniforms.bindProjectionMatrix()
    void bindProjectionMatrix(UniformHandle& uniform, bool forViewCoords) const
    {
        if (forViewCoords)
            viewRect.bindProjectionMatrix(uniform);
        else
            frustum.bindProjectionMatrix(uniform);
    }

    /// Bind the view-space sun direction.
    /// Ported from: itwinjs-core TargetUniforms.bindSunDirection()
    void bindSunDirection(UniformHandle& uniform) noexcept
    {
        m_sunDirection.bind(uniform, frustum);
    }

    /// Update the world-space sun direction.
    void setSunDirection(dqGeom::Vector3d const* sunDir) noexcept { m_sunDirection.update(sunDir); }

    /// View-space sun direction (u_sunDir value; see SunDirection::getSunDirView).
    float const* getSunDirView() const noexcept { return m_sunDirection.getSunDirView(); }

    /// Update per-frame uniforms from a render plan.
    /// Ported from: itwinjs-core TargetUniforms.updateRenderPlan()
    /// lights.update(plan.lights) wired (RenderPlan now carries a typed
    /// dqCommon::LightSettings). hilite.update(...) still TODO.
    void updateRenderPlan(RenderPlan const& plan) noexcept
    {
        style.update(plan);
        // Lights (u_lightSettings[16]) — packed from the plan's typed LightSettings.
        // Ported from: itwinjs-core TargetUniforms.updateRenderPlan lights.update.
        lights.update(plan.lights);
        // Sun direction (world space). Ported from: itwinjs-core TargetUniforms.ts
        // :177-186 — a world-space sun dir is used only when shadows are enabled
        // or the solar light is alwaysEnabled; otherwise SunDirection falls back
        // to the default VIEW-space direction (0.272166, 0.680414, 0.680414), so
        // unshadowed scenes still get directional lighting.
        bool const useSunDir = plan.viewFlags.shadows() || plan.lights.solar.alwaysEnabled;
        if (useSunDir) {
            dqGeom::Vector3d const sunDir(plan.lights.solar.direction);
            setSunDirection(&sunDir);
        } else {
            setSunDirection(nullptr);
        }
    }

private:
    SunDirection m_sunDirection;
};

END_DQ_RENDER_NAMESPACE

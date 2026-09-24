// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Thematic display sensor geometry
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ThematicSensors.ts
//
// Maintains a floating-point texture representing a list of thematic
// sensors. Each sensor occupies one texel row (4 floats: x, y, z, value).
// The texture is updated when the view matrix changes, transforming
// sensor positions to view space.
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "UniformHandle.h"
#include "dqGeom/Point3d.h"
#include "dqGeom/Range3d.h"
#include "dqGeom/Transform.h"

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

using dqGeom::Point3d;
using dqGeom::Range3d;
using dqGeom::Transform;

// ---------------------------------------------------------------------------
// ThematicDisplaySensor — a single sensor position + value
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core core-common ThematicDisplaySensor
struct ThematicDisplaySensor {
    Point3d position = Point3d::FromZero();
    double value = 0.0;

    static ThematicDisplaySensor fromJSON(double px, double py, double pz, double val) {
        ThematicDisplaySensor s;
        s.position = Point3d::From(px, py, pz);
        s.value = val;
        return s;
    }
};

// ---------------------------------------------------------------------------
// ThematicSensors — floating-point texture of sensor data
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ThematicSensors.ts
//
// The texture layout is a 1-by-N float RGBA texture where N = numSensors.
// Each texel stores (position.x, position.y, position.z, value).
// Positions are transformed to view space before writing.
class ThematicSensors {
public:
    ThematicSensors() = default;

    // Non-copyable, movable
    ThematicSensors(const ThematicSensors&) = delete;
    ThematicSensors& operator=(const ThematicSensors&) = delete;
    ThematicSensors(ThematicSensors&& other) noexcept;
    ThematicSensors& operator=(ThematicSensors&& other) noexcept;

    ~ThematicSensors() = default;

    // create a ThematicSensors from a list of sensors.
    // viewMatrix is the current view transform used to transform sensor positions.
    static ThematicSensors create(
        const std::vector<ThematicDisplaySensor>& sensors,
        const Transform& viewMatrix);

    // Update the sensor data if the view matrix has changed.
    void update(const Transform& viewMatrix);

    // Access the raw float texture data (4 floats per sensor: x, y, z, value).
    const float* data() const { return m_data.data(); }
    float* dataMut() { return m_data.data(); }
    std::size_t byteSize() const { return m_data.size() * sizeof(float); }

    // Number of sensors in the texture.
    std::size_t numSensors() const { return m_sensors.size(); }

    /// Bind the sensor count to a uniform.
    /// Ported from: itwinjs-core ThematicSensors.bindNumSensors()
    void bindNumSensors(UniformHandle& uniform) const
    {
        uniform.setUniform1i(static_cast<int>(numSensors()));
    }

    // Texture dimensions: width=1, height=numSensors.
    int textureWidth() const { return 1; }
    int textureHeight() const { return static_cast<int>(m_sensors.size()); }

    // Whether the texture data has been modified since last upload.
    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }

    // Whether this object is empty (no sensors).
    bool isEmpty() const { return m_sensors.empty(); }

    // Filter sensors by range and distance cutoff.
    static std::vector<ThematicDisplaySensor> accumulateSensorsInRange(
        const std::vector<ThematicDisplaySensor>& sensors,
        const Range3d& range,
        const Transform& transform,
        double distanceCutoff);

private:
    void appendFloat(float value);
    void appendValues(double a, double b, double c, double d);
    void reset();
    void advance(std::size_t numBytes);
    void updateTextureData();

    std::vector<ThematicDisplaySensor> m_sensors;
    std::vector<float> m_data;
    Transform m_viewMatrix = Transform::CreateIdentity();
    std::size_t m_curPos = 0;
    bool m_dirty = false;
};

END_DQ_RENDER_NAMESPACE

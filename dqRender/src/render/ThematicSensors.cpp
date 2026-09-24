// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Thematic display sensor geometry implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ThematicSensors.ts
//
// Maintains a floating-point texture representing a list of thematic
// sensors. Each sensor occupies one texel row (4 floats: x, y, z, value).

#include "ThematicSensors.h"

#include <algorithm>
#include <cstring>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Move operations
// ---------------------------------------------------------------------------
ThematicSensors::ThematicSensors(ThematicSensors&& other) noexcept
    : m_sensors(std::move(other.m_sensors))
    , m_data(std::move(other.m_data))
    , m_viewMatrix(other.m_viewMatrix)
    , m_curPos(other.m_curPos)
    , m_dirty(other.m_dirty)
{
    other.m_curPos = 0;
    other.m_dirty = false;
}

ThematicSensors& ThematicSensors::operator=(ThematicSensors&& other) noexcept
{
    if (this != &other) {
        m_sensors = std::move(other.m_sensors);
        m_data = std::move(other.m_data);
        m_viewMatrix = other.m_viewMatrix;
        m_curPos = other.m_curPos;
        m_dirty = other.m_dirty;
        other.m_curPos = 0;
        other.m_dirty = false;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// create — build sensor texture from sensor list and initial view matrix
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ThematicSensors.ts create
ThematicSensors ThematicSensors::create(
    const std::vector<ThematicDisplaySensor>& sensors,
    const Transform& viewMatrix)
{
    ThematicSensors ts;
    ts.m_sensors = sensors;
    ts.m_data.resize(sensors.size() * 4, 0.0f);
    ts.m_viewMatrix = viewMatrix;
    ts.updateTextureData();
    return ts;
}

// ---------------------------------------------------------------------------
// Update — re-transform and re-pack sensor data if view matrix changed
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ThematicSensors.ts update
void ThematicSensors::update(const Transform& viewMatrix)
{
    // Simple check: compare matrix elements to avoid full re-upload.
    // The reference uses isAlmostEqual; we use a direct comparison since
    // Transform does not yet have that method.
    if (std::memcmp(&m_viewMatrix, &viewMatrix, sizeof(Transform)) == 0)
        return;

    m_viewMatrix = viewMatrix;
    updateTextureData();
}

// ---------------------------------------------------------------------------
// accumulateSensorsInRange — filter sensors by range proximity
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ThematicSensors.ts _accumulateSensorsInRange
std::vector<ThematicDisplaySensor> ThematicSensors::accumulateSensorsInRange(
    const std::vector<ThematicDisplaySensor>& sensors,
    const Range3d& range,
    const Transform& transform,
    double distanceCutoff)
{
    std::vector<ThematicDisplaySensor> result;

    Range3d transformedRange = transform.MultiplyRange(range);

    for (const auto& sensor : sensors) {
        if (distanceCutoff <= 0.0) {
            result.push_back(sensor);
            continue;
        }

        // Check if sensor position is within distance cutoff of the transformed range.
        // Ported from: itwinjs-core ThematicSensors.ts _sensorRadiusAffectsRange
        double distance = transformedRange.DistanceToPoint(sensor.position);

        if (distance <= distanceCutoff) {
            result.push_back(sensor);
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ThematicSensors::appendFloat(float value)
{
    if (m_curPos + sizeof(float) <= m_data.size() * sizeof(float)) {
        std::memcpy(reinterpret_cast<char*>(m_data.data()) + m_curPos, &value, sizeof(float));
    }
    advance(sizeof(float));
}

void ThematicSensors::appendValues(double a, double b, double c, double d)
{
    appendFloat(static_cast<float>(a));
    appendFloat(static_cast<float>(b));
    appendFloat(static_cast<float>(c));
    appendFloat(static_cast<float>(d));
}

void ThematicSensors::reset()
{
    m_curPos = 0;
}

void ThematicSensors::advance(std::size_t numBytes)
{
    m_curPos += numBytes;
}

void ThematicSensors::updateTextureData()
{
    m_data.resize(m_sensors.size() * 4, 0.0f);
    reset();

    for (const auto& sensor : m_sensors) {
        // Transform position to view space
        Point3d viewPos = m_viewMatrix.MultiplyPoint3d(sensor.position);
        appendValues(viewPos.x, viewPos.y, viewPos.z, sensor.value);
    }

    m_dirty = true;
}

END_DQ_RENDER_NAMESPACE

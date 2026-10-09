// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Thematic display sensor geometry implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ThematicSensors.ts
//
// Maintains a floating-point texture representing a list of thematic
// sensors. Each sensor occupies one texel row (4 floats: x, y, z, value).

#include "ThematicSensors.h"

#include "rhi/opengl/GlLoader.h"  // [THM] 探针回读（DANQING_THM_TRACE——CLIPDUMP 先例）

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Move operations
// ---------------------------------------------------------------------------
ThematicSensors::ThematicSensors(ThematicSensors&& other) noexcept
    : m_sensors(std::move(other.m_sensors))
    , m_data(std::move(other.m_data))
    , m_texture(other.m_texture)
    , m_viewMatrix(other.m_viewMatrix)
    , m_curPos(other.m_curPos)
    , m_dirty(other.m_dirty)
{
    other.m_texture = {};
    other.m_curPos = 0;
    other.m_dirty = false;
}

ThematicSensors& ThematicSensors::operator=(ThematicSensors&& other) noexcept
{
    if (this != &other) {
        m_sensors = std::move(other.m_sensors);
        m_data = std::move(other.m_data);
        m_texture = other.m_texture;
        m_viewMatrix = other.m_viewMatrix;
        m_curPos = other.m_curPos;
        m_dirty = other.m_dirty;
        other.m_texture = {};
        other.m_curPos = 0;
        other.m_dirty = false;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// create — build sensor texture from sensor list（视空间打包 + GPU 上传）
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ThematicSensors.ts create/createFloat/_update
//              （:51-66——createFloat 后立即 _update(frustum.viewMatrix)：
//              打包视空间位 + 上传）
ThematicSensors ThematicSensors::create(
    const std::vector<ThematicDisplaySensor>& sensors,
    const Transform& viewMatrix,
    rhi::Driver* driver)
{
    ThematicSensors ts;
    ts.m_sensors = sensors;
    ts.m_data.resize(sensors.size() * 4, 0.0f);
    ts.m_viewMatrix = viewMatrix;
    ts.updateTextureData();
    ts.uploadTexture(driver);
    return ts;
}

// ---------------------------------------------------------------------------
// update — per-frame lazy refresh（viewMatrix 变则重打包+重传）
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ThematicSensors.ts update（:95-99——
//              isAlmostEqual 惰性门）+ _update（:82-93——replaceTextureData）
void ThematicSensors::update(const Transform& viewMatrix, rhi::Driver* driver)
{
    if (m_viewMatrix.IsAlmostEqual(viewMatrix))
        return;
    m_viewMatrix = viewMatrix;
    updateTextureData();
    uploadTexture(driver, /*replace=*/true);
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
        // 视空间位（ThematicSensors.ts:82-93——viewMatrix.multiplyPoint3d 全点
        // 变换[含平移]：消费点 distance(v_eyeSpace, sensor) 的 v_eyeSpace 为
        // 视空间[实测归位——"世界帧"登记被 FRAGDBG=10 证伪]）。
        auto const pos = m_viewMatrix.MultiplyPoint3d(sensor.position);
        appendValues(pos.x, pos.y, pos.z, sensor.value);
    }

    m_dirty = true;
}

// ---------------------------------------------------------------------------
// uploadTexture — 1×N RGBA32F 列向浮点纹理（ThematicSensors.ts createForData
// 形态：1×numSensors / Format.Rgba[浮点] / ClampToEdge）+ NEAREST 数据纹理
// 过滤（ClipStack.cpp:203 先例——0x2600）。replace=true 时按参考
// replaceTextureData 语义就地重传（不重建纹理对象）。
// ---------------------------------------------------------------------------
void ThematicSensors::uploadTexture(rhi::Driver* driver, bool replace)
{
    if (!driver || m_data.empty()) {
        m_texture = {};
        return;
    }

    if (replace && m_texture.isValid()) {
        // replaceTextureData（Texture2DHandle.replaceTextureData——同尺寸
        // 重传）。客户端格式 = GL_RGBA/GL_FLOAT（ClipStack.cpp:206-212 先例）。
        rhi::PixelBufferDescriptor pbd(
            m_data.data(), m_data.size() * sizeof(float),
            0x1908 /* GL_RGBA */, 0x1406 /* GL_FLOAT */,
            0, 1, 0, 0, 1, static_cast<uint32_t>(m_sensors.size()), 1);
        driver->setTextureData(m_texture.getRhiHandle(), 0, 0, 0, 0,
                               1, static_cast<uint32_t>(m_sensors.size()), 1,
                               std::move(pbd));
        return;
    }

    m_texture = TextureHandle::create2D(
        *driver,
        1,
        static_cast<uint32_t>(m_sensors.size()),
        rhi::TextureFormat::RGBA32F,
        m_data.data(),
        static_cast<uint32_t>(m_data.size() * sizeof(float)));

    if (m_texture.isValid())
        driver->setTextureFilters(m_texture.getRhiHandle(), 0x2600, 0x2600);  // NEAREST

    // [THM] 探针（DANQING_THM_TRACE=1，§13.1 族）：上传后回读首两行浮点——
    // 与 CPU 打包值对拍（M-S S-e IDW 传感器链取证；CLIPDUMP 先例）。
    if (std::getenv("DANQING_THM_TRACE") && m_texture.isValid()) {
        driver->bindTexture(0, m_texture.getRhiHandle());
        float rows[8] = {-9, -9, -9, -9, -9, -9, -9, -9};
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, rows);
        float const* c = m_data.data();
        size_t const n = m_data.size();
        printf("[THM] sensor tex gpuR0=(%g,%g,%g,%g) gpuR1=(%g,%g,%g,%g) cpuR0=(%g,%g,%g,%g) cpuR1=(%g,%g,%g,%g)\n",
               rows[0], rows[1], rows[2], rows[3], rows[4], rows[5], rows[6], rows[7],
               n > 0 ? c[0] : -9.f, n > 1 ? c[1] : -9.f, n > 2 ? c[2] : -9.f, n > 3 ? c[3] : -9.f,
               n > 4 ? c[4] : -9.f, n > 5 ? c[5] : -9.f, n > 6 ? c[6] : -9.f, n > 7 ? c[7] : -9.f);
        fflush(stdout);
    }
}

END_DQ_RENDER_NAMESPACE

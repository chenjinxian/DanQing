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

#include "TextureHandle.h"  // rhi::TextureHandle（GPU 上传——M-S S-e）
#include "UniformHandle.h"
#include "dqGeom/Point3d.h"
#include "dqGeom/Range3d.h"
#include "dqGeom/Transform.h"

#include <dqCommon/ThematicDisplay.h>  // ThematicDisplaySensor（参考 ThematicSensors.ts
                                       //  自 core-common 导入——M-S 归位：本文件原
                                       //  地复刻的同名 struct 删[重复类型源]）

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

using dqGeom::Point3d;
using dqGeom::Range3d;
using dqGeom::Transform;
using dqCommon::ThematicDisplaySensor;

// ---------------------------------------------------------------------------
// ThematicSensors — floating-point texture of sensor data
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ThematicSensors.ts
//
// The texture layout is a 1-by-N float RGBA texture where N = numSensors.
// Each texel stores (position.x, position.y, position.z, value).
//
// **视空间打包（参考语义，M-S S-e 实测归位）**：参考 _update 把传感器位置经
// frustum.viewMatrix 变换到**视空间**（ThematicSensors.ts:82-93——消费点
// distance(v_eyeSpace, sensor) 的 v_eyeSpace 为视空间）。S-d 的"E1 世界帧"
// 登记经实测**证伪**（DANQING_THM_FRAGDBG=10 直读 v_eyeSpace=视空间坐标场、
// mode=1 法线场 roofTop≈(0,0.82,0.58)=R·ẑ——DanQing 着色器与参考同构视帧），
// 传感器逐帧视变换与 update 惰性门（viewMatrix isAlmostEqual）1:1 归位。
class ThematicSensors {
public:
    ThematicSensors() = default;

    // Non-copyable, movable
    ThematicSensors(const ThematicSensors&) = delete;
    ThematicSensors& operator=(const ThematicSensors&) = delete;
    ThematicSensors(ThematicSensors&& other) noexcept;
    ThematicSensors& operator=(ThematicSensors&& other) noexcept;

    ~ThematicSensors() = default;

    // create a ThematicSensors from a list of sensors（视空间打包 + GPU 上传）。
    // Ported from: itwinjs-core ThematicSensors.create（:51-66——accumulate 后
    //  createFloat + 立即 _update(frustum.viewMatrix)）。
    // @param sensors    世界位传感器集（源数据恒世界域——update 重打包的底本）
    // @param viewMatrix 当前视矩阵（world→view——打包其 eye-space 位置）
    // @param driver     纹理创建驱动（NullDriver 可——句柄无效则 getTexture 空）。
    static ThematicSensors create(
        const std::vector<ThematicDisplaySensor>& sensors,
        const Transform& viewMatrix,
        rhi::Driver* driver = nullptr);

    /// Per-frame refresh（ThematicSensors.ts update :95-99——viewMatrix
    ///  isAlmostEqual 惰性门：视角未变不重打包/不重传；变则 _update 重打包
    ///  eye-space + replaceTextureData 重传）。
    /// Ported from: itwinjs-core ThematicSensors.update()
    void update(const Transform& viewMatrix, rhi::Driver* driver = nullptr);

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

    /// The GPU texture handle (1×N RGBA32F；无驱动创建时为空）。
    /// Ported from: itwinjs-core ThematicSensors.texture（createForData 形态）。
    rhi::TextureHandle getTexture() const noexcept
    {
        return m_texture.isValid() ? m_texture.getRhiHandle() : rhi::TextureHandle{};
    }

    // Texture dimensions: width=1, height=numSensors.
    int textureWidth() const { return 1; }
    int textureHeight() const { return static_cast<int>(m_sensors.size()); }

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
    void updateTextureData();          // _update 的打包半（视空间——m_viewMatrix）
    void uploadTexture(rhi::Driver* driver, bool replace = false);

    std::vector<ThematicDisplaySensor> m_sensors;  // 世界位源（参考 _sensors）
    std::vector<float> m_data;                      // 视空间打包（参考 _texture.data）
    TextureHandle m_texture;  // 渲染层包装（create2D 产出——getRhiHandle 供绑定）
    Transform m_viewMatrix = Transform::CreateIdentity();  // 打包用视矩阵（参考 _viewMatrix——惰性门判据）
    std::size_t m_curPos = 0;
    bool m_dirty = false;
};

END_DQ_RENDER_NAMESPACE

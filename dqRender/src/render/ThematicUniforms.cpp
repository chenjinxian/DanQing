// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Thematic display uniforms
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ThematicUniforms.ts

#include "ThematicUniforms.h"

#include "TargetImpl.h"   // update(target) 的完全体（getPlanThematic/getUniforms/wantThematicSensors/getDriver）
#include "FrustumUniforms.h"

#include <dqGeom/Angle.h>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// ThematicUniforms::update — 参考 :90-152 逐行
// ---------------------------------------------------------------------------
void ThematicUniforms::update(TargetImpl& target)
{
    auto const* planThematic = target.getPlanThematic();
    auto const& viewMatrix = target.getUniforms().frustum.getViewMatrix();

    // 快路径（:93-106）：设置在且等值 + 纹理在 → 仅刷新传感器 eye-space 后
    // 返回。EQUIVALENCE（§11.10，同下 axis/sunDirection 注）：Slope 轴与
    // HillShade 太阳向的视矩阵刷新臂在 DanQing 的世界帧着色结构下无需执行
    // （轴/太阳向恒世界空间，不随相机变——见下 E1 注）。传感器纹理的
    // eye-space 变换保留（S-e 落地时再评——其 eye-space 消费点在 v_eyeSpace）。
    if (m_thematicDisplay && planThematic && m_thematicDisplay->equals(*planThematic) && m_texture.isValid()) {
        if (m_sensors)
            m_sensors->update(viewMatrix);
        return;
    }

    desync();

    // :110-113——plan.thematic 缺席即清态（关闭路径）。
    if (planThematic)
        m_thematicDisplay = *planThematic;
    else
        m_thematicDisplay.reset();
    m_texture = {};
    if (!m_thematicDisplay)
        return;

    auto const& td = *m_thematicDisplay;

    // :115-121——Slope 的 range 以度输入、弧度上传。
    if (dqCommon::ThematicDisplayMode::Slope == td.displayMode) {
        m_range[0] = static_cast<float>(dqGeom::Angle::DegreesToRadians(td.range.low));
        m_range[1] = static_cast<float>(dqGeom::Angle::DegreesToRadians(td.range.high));
    } else {
        m_range[0] = static_cast<float>(td.range.low);
        m_range[1] = static_cast<float>(td.range.high);
    }

    m_colorMix = static_cast<float>(td.gradientSettings.colorMix);

    // :125 轴 + :127-128 太阳向——**EQUIVALENCE（§11.10，E1）**：
    // 参考源 = ThematicUniforms._updateAxis/:97-103 与 _updateSunDirection
    //（:73-88/:127-128——Slope 轴经 frustum.viewMatrix 变换、HillShade 太阳向
    //  变换+negate+normalize；其语义前提 = 参考的 g_normal 为**视空间**法线
    // [u_nmx 与 u_mv 同源的视空间体系]）。
    // 发散 = DanQing 着色器为**世界帧结构**（M-P P-D clip 既有登记同族：
    //  v_eyeSpace = 栈底 u_mv·p = 世界坐标[相机仅经 u_mvp 进入]；
    //  u_normalMatrix = transpose(inverse(栈底 mv)) = 恒等旋转 →
    //  v_n/g_normal = **世界法线**[非实例变体]；实例变体 g_nmx 自 g_mv
    //  计算——u_instanced_modelView 同为世界域[M-M(1) 注记面]，全变体一致）。
    // 本式 = 旋转不变量下的等价重表达：世界法线 × 世界轴 = 参考的
    // 视法线 × 视轴（点积逐位同）——故 axis 恒 normalize（无变换）、
    // sunDirection 仅 HillShade 取负+归一（negate/normalize 与旋转可交换，
    // 世界空间同义）。
    // 验证法 = Slope 墙面白/屋面暗 + HillShade 阴阳面像素锁（S-d E2E）。
    updateAxis(td.axis);
    if (dqCommon::ThematicDisplayMode::HillShade == td.displayMode)
        updateSunDirection(td.sunDirection);

    // :130
    m_marginColor = FloatRgba::fromHex(
        td.gradientSettings.marginColor.getRgb(),
        static_cast<uint8_t>(td.gradientSettings.marginColor.getAlpha()));

    // :132
    m_displayMode[0] = static_cast<float>(td.displayMode);

    // :134-140——fragSettings = (gradientMode, distanceCutoff, stepCount 钳,
    // multiplyGradientAlpha)。
    m_fragSettings[0] = static_cast<float>(td.gradientSettings.mode);
    m_fragSettings[1] = static_cast<float>(td.sensorSettings.distanceCutoff);
    m_fragSettings[2] = static_cast<float>(
        std::min(td.gradientSettings.stepCount, m_gradientDimension));
    m_fragSettings[3] = (td.gradientSettings.transparencyMode ==
        dqCommon::ThematicGradientTransparencyMode::SurfaceOnly) ? 0.0f : 1.0f;

    // :142-147——wantSensors 且无 cutoff → 全局共享传感器纹理。
    //（GPU 上传随 S-e；本步先持 CPU 数据面 + 计数。参考无 else 臂——模式
    // 切离 IDW 时传感器态滞留但惰性[shader 仅 IDW 分支读 u_numSensors，
    // s_sensorSampler 绑定受 wantThematicSensors 门]；1:1 保留该形态。）
    if (target.wantThematicSensors() && !(m_fragSettings[1] > 0.0f)) {
        m_numSensors = static_cast<int>(td.sensorSettings.sensors.size());
        m_sensors.reset();
        m_sensors = ThematicSensors::create(td.sensorSettings.sensors, viewMatrix);
    }

    // :149-151——渐变纹理重建。
    createGradientTexture(target.getDriver());
}

// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ThematicUniforms._updateAxis（:73-79——恒
// normalize；DanQing 世界帧结构下无视矩阵变换——E1 EQUIVALENCE 见 update）。
// ---------------------------------------------------------------------------
void ThematicUniforms::updateAxis(dqGeom::Vector3d const& axis)
{
    dqGeom::Vector3d tAxis = axis;
    tAxis.Normalize();
    m_axis[0] = static_cast<float>(tAxis.x);
    m_axis[1] = static_cast<float>(tAxis.y);
    m_axis[2] = static_cast<float>(tAxis.z);
}

// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ThematicUniforms._updateSunDirection（:81-88——
// negate + normalize；DanQing 世界帧结构下无视矩阵变换）。
// ---------------------------------------------------------------------------
void ThematicUniforms::updateSunDirection(dqGeom::Vector3d const& sunDir)
{
    auto v = sunDir;
    v.Negate();
    v.Normalize();
    m_sunDirection[0] = static_cast<float>(v.x);
    m_sunDirection[1] = static_cast<float>(v.y);
    m_sunDirection[2] = static_cast<float>(v.z);
}

// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ThematicUniforms.update() :149-151 的纹理创建半
// ---------------------------------------------------------------------------
void ThematicUniforms::createGradientTexture(rhi::Driver& driver)
{
    if (!m_thematicDisplay.has_value()) return;

    // Gradient.Symb.createThematic（Gradient.ts:140-158）。
    auto symb = dqCommon::GradientSymb::createThematic(m_thematicDisplay->gradientSettings);

    // getThematicImageForRenderer（Gradient.ts:307-359——**1×N 列向** RGBA8：
    // Smooth→N=maxDimension，Stepped 族→N=stepCount；M-S 前此处误用
    // produceImage 行向纹理[G18——shader 采样 vec2(0.0,ndx) 恒中首 texel]）。
    auto image = symb.getThematicImageForRenderer(m_gradientDimension);
    if (!image.has_value()) return;

    auto const& img = image.value();
    const uint32_t height = static_cast<uint32_t>(img.getHeight());

    m_texture = TextureHandle::create2D(
        driver, 1, height,
        rhi::TextureFormat::RGBA8,
        img.data.data(),
        static_cast<uint32_t>(img.data.size()));

    // ThematicGradient 型（Texture.ts:293-309）：无 mipmap + MIN/MAG 皆
    // NEAREST（0x2600——ClipStack.cpp:203 数据纹理先例）+ ClampToEdge
    //（create2D 默认）。
    if (m_texture.isValid())
        driver.setTextureFilters(m_texture.getRhiHandle(), 0x2600, 0x2600);

    // TEMP-DIAG（M-S S-d——渐变纹理内容取证：首/中/尾 texel 应为
    // BlueRed 的 蓝(0)/绿(0.5)/红(1) 端）。
    static bool const s_thmDump = getenv("DANQING_THM_TRACE") != nullptr;
    if (s_thmDump) {
        auto const& d = img.data;
        auto px = [&](size_t row) {
            return std::string("(") + std::to_string(d[row * 4 + 0]) + "," +
                   std::to_string(d[row * 4 + 1]) + "," + std::to_string(d[row * 4 + 2]) + ")";
        };
        printf("[THM] gradient texture 1x%u: row0=%s rowMid=%s rowLast=%s\n",
               height, px(0).c_str(), px(height / 2).c_str(), px(height - 1).c_str());
        fflush(stdout);
    }
}

END_DQ_RENDER_NAMESPACE

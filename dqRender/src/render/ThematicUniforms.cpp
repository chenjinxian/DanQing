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

    // 快路径（:93-106）：设置在且等值 + 纹理在 → 逐帧刷新视域臂后返回——
    // 传感器 eye-space 重打包（update 的 isAlmostEqual 惰性门）、Slope 轴 /
    // HillShade 太阳向随视矩阵重变换+desync。（S-d 的"E1 世界帧"登记实测证伪
    // 后归位——v_eyeSpace/g_normal 皆视空间[FRAGDBG=10/1 直读实证：roofTop
    // 法线 (0.09,0.83,0.61)=R_iso·ẑ]，参考的视变换臂为必需而非可舍。）
    if (m_thematicDisplay && planThematic && m_thematicDisplay->equals(*planThematic) && m_texture.isValid()) {
        if (m_sensors)
            m_sensors->update(viewMatrix, &target.getDriver());

        // :100-105——Slope 轴 / HillShade 太阳向逐帧随视矩阵刷新 + desync
        //（参考 desync(this)——绑定侧 sync 去重为性能优化，DanQing bind* 恒
        // 上传的既有 EQUIVALENCE 下 desync 仅推进 syncKey[观测面]）。
        if (dqCommon::ThematicDisplayMode::Slope == m_thematicDisplay->displayMode) {
            updateAxis(m_thematicDisplay->axis, &viewMatrix);
            desync();
        } else if (dqCommon::ThematicDisplayMode::HillShade == m_thematicDisplay->displayMode) {
            updateSunDirection(m_thematicDisplay->sunDirection, viewMatrix);
            desync();
        }

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

    // :125 轴（Slope 臂经视矩阵变换）+ :127-128 太阳向（HillShade 臂变换
    // +negate+normalize）——参考语义逐行归位（E1 登记撤销：实测 DanQing 着色
    // 器为视空间结构，与参考同帧）。
    updateAxis(td.axis,
               dqCommon::ThematicDisplayMode::Slope == td.displayMode ? &viewMatrix : nullptr);
    if (dqCommon::ThematicDisplayMode::HillShade == td.displayMode)
        updateSunDirection(td.sunDirection, viewMatrix);

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

    // :142-147——wantSensors 且无 cutoff → 全局共享传感器纹理（创建即按当前
    // 视矩阵打包 eye-space——参考 create(target, nullRange) 内 _update）。
    //（参考无 else 臂——模式切离 IDW 时传感器态滞留但惰性[shader 仅 IDW 分支读
    //  u_numSensors，s_sensorSampler 绑定受 wantThematicSensors 门]；
    //  1:1 保留该形态。）
    if (target.wantThematicSensors() && !(m_fragSettings[1] > 0.0f)) {
        m_numSensors = static_cast<int>(td.sensorSettings.sensors.size());
        m_sensors.reset();
        m_sensors = ThematicSensors::create(td.sensorSettings.sensors, viewMatrix,
                                            &target.getDriver());
    }

    // :149-151——渐变纹理重建。
    createGradientTexture(target.getDriver());
}

// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ThematicUniforms._updateAxis（:73-79——viewMatrix
// 在场则 multiplyVector 变换（方向变换——旋转不含平移）后恒 normalize）。
// ---------------------------------------------------------------------------
void ThematicUniforms::updateAxis(dqGeom::Vector3d const& axis,
                                  dqGeom::Transform const* viewMatrix)
{
    dqGeom::Vector3d tAxis = viewMatrix ? viewMatrix->MultiplyVector(axis) : axis;
    tAxis.Normalize();
    m_axis[0] = static_cast<float>(tAxis.x);
    m_axis[1] = static_cast<float>(tAxis.y);
    m_axis[2] = static_cast<float>(tAxis.z);
}

// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ThematicUniforms._updateSunDirection（:81-88——
// viewMatrix.multiplyVector + negate + normalize）。
// ---------------------------------------------------------------------------
void ThematicUniforms::updateSunDirection(dqGeom::Vector3d const& sunDir,
                                          dqGeom::Transform const& viewMatrix)
{
    auto v = viewMatrix.MultiplyVector(sunDir);
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

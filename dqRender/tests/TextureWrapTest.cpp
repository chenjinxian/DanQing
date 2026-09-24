// SPDX-License-Identifier: Apache-2.0
// dqRender — texture wrap-mode regression (glTF atlas textures).
//
// Repro（用户 2026-09-15 DanQing vs DTA 并排对比）: BoxTextured 是图集纹理——6 面 UV
// 各占 U=[0,1]..[5,6] 一格；glTF 规范默认 wrapS/wrapT=REPEAT。DanQing RHI 硬编码
// ClampToEdge（OpenGLDriver.cpp:1478）→ U>1 的 5 面全部塌缩到右边缘 texel（≈纯色），
// 仅 U∈[0,1] 一面显示纹理；参考 DTA 6 面全纹理。
//
// 参考机制（逐字）: itwinjs-core Texture.ts:300 getImageProperties —
//   wrapMode = (RenderTexture.Type.Normal === type) ? Repeat : ClampToEdge
// 上传于 Texture.ts:87-88 texParameteri(TEXTURE_WRAP_S/T, params.wrapMode)。
// Authored: no reference test exists in itwinjs-core for wrap-mode dispatch; 行为锚定
//           实现 Texture.ts:293-309 + :87-88。
#include "NullDriver.h"

#include "rhi/DriverBase.h"
#include "rhi/HandleAllocator.h"
#include "render/OpenGLRenderSystem.h"
#include "render/TextureHandle.h"

#include <dqRender/CreateTextureArgs.h>
#include <dqRender/rhi/Handle.h>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using namespace dqRender;

namespace {
// GL wrap constants (GL_REPEAT / GL_CLAMP_TO_EDGE) as raw values, matching what
// the driver-level interface receives.
uint32_t const kGlRepeat = 0x2901u;
uint32_t const kGlClampToEdge = 0x812Fu;

// Records wrap requests reaching the driver (setTextureWrapMode) and hands out a
// real texture handle so TextureHandle::create2D proceeds past the validity check.
class WrapRecordingDriver : public rhi::NullDriver {
public:
    rhi::TextureHandle createTexture(rhi::SamplerType, uint8_t, rhi::TextureFormat,
                                     uint32_t, uint32_t, uint32_t, rhi::TextureUsage) noexcept override {
        ++m_created;
        return m_handleAllocator.allocate<rhi::HwTexture>();
    }

    void setTextureWrapMode(rhi::TextureHandle, uint32_t wrapS, uint32_t wrapT) noexcept override {
        ++m_wrapCalls;
        m_lastWrapS = wrapS;
        m_lastWrapT = wrapT;
    }

    int m_created = 0;
    int m_wrapCalls = 0;
    uint32_t m_lastWrapS = 0;
    uint32_t m_lastWrapT = 0;

private:
    rhi::HandleAllocator m_handleAllocator;
};
}  // namespace

// Normal 类型（glTF baseColor 的默认类型）必须请求 REPEAT —— 参考 Texture.ts:300。
TEST(TextureWrapTest, NormalTextureGetsRepeatWrap)
{
    auto driverOwner = std::make_unique<WrapRecordingDriver>();
    auto& driver = *driverOwner;
    OpenGLRenderSystem system{ std::move(driverOwner) };

    dqCommon::ImageBuffer image{ std::vector<uint8_t>{ 255, 0, 0, 255 }, dqCommon::ImageBufferFormat::Rgba, 1 };
    dqRender::CreateTextureArgs args{};
    args.type = dqRender::RenderTexture::Type::Normal;  // glTF 默认路径
    args.imageBuffer = std::move(image);

    (void)system.createTexture(args);
    ASSERT_GE(driver.m_created, 1) << "texture was never created";
    EXPECT_GE(driver.m_wrapCalls, 1) << "wrap mode was never sent to the driver";
    EXPECT_EQ(driver.m_lastWrapS, kGlRepeat) << "WRAP_S must be GL_REPEAT for Normal";
    EXPECT_EQ(driver.m_lastWrapT, kGlRepeat) << "WRAP_T must be GL_REPEAT for Normal";
}

// 非 Normal 类型保持 ClampToEdge —— 参考 Texture.ts:300 else 分支。
TEST(TextureWrapTest, TileSectionTextureGetsClampWrap)
{
    auto driverOwner = std::make_unique<WrapRecordingDriver>();
    auto& driver = *driverOwner;
    OpenGLRenderSystem system{ std::move(driverOwner) };

    dqCommon::ImageBuffer image{ std::vector<uint8_t>{ 255, 0, 0, 255 }, dqCommon::ImageBufferFormat::Rgba, 1 };
    dqRender::CreateTextureArgs args{};
    args.type = dqRender::RenderTexture::Type::TileSection;
    args.imageBuffer = std::move(image);

    (void)system.createTexture(args);
    ASSERT_GE(driver.m_created, 1);
    EXPECT_GE(driver.m_wrapCalls, 1);
    EXPECT_EQ(driver.m_lastWrapS, kGlClampToEdge) << "WRAP_S must be GL_CLAMP_TO_EDGE for non-Normal";
    EXPECT_EQ(driver.m_lastWrapT, kGlClampToEdge);
}

// create2D 缺省 wrap = ClampToEdge（既有调用点行为不变）。
TEST(TextureWrapTest, DefaultCreate2DClamps)
{
    WrapRecordingDriver driver;
    uint8_t const pixel[4] = { 255, 0, 0, 255 };
    TextureHandle tex = TextureHandle::create2D(driver, 1, 1, rhi::TextureFormat::RGBA8,
                                                pixel, sizeof(pixel));
    (void)tex;
    ASSERT_GE(driver.m_created, 1);
    EXPECT_GE(driver.m_wrapCalls, 1);
    EXPECT_EQ(driver.m_lastWrapS, kGlClampToEdge);
    EXPECT_EQ(driver.m_lastWrapT, kGlClampToEdge);
}

// 显式 Repeat 经 create2D 直达驱动。
TEST(TextureWrapTest, ExplicitRepeatReachesDriver)
{
    WrapRecordingDriver driver;
    uint8_t const pixel[4] = { 255, 0, 0, 255 };
    TextureHandle tex = TextureHandle::create2D(driver, 1, 1, rhi::TextureFormat::RGBA8,
                                                pixel, sizeof(pixel),
                                                GL::Texture::WrapMode::Repeat);
    (void)tex;
    ASSERT_GE(driver.m_created, 1);
    EXPECT_GE(driver.m_wrapCalls, 1);
    EXPECT_EQ(driver.m_lastWrapS, kGlRepeat);
    EXPECT_EQ(driver.m_lastWrapT, kGlRepeat);
}

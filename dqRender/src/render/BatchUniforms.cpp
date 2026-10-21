// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — BatchUniforms out-of-line definitions
//
// bindLUT requires the full RenderSystemImpl definition (for bindTexture2d),
// which cannot be included in Uniforms.h without a circular dependency.
// Ported from: itwinjs-core FeatureOverrides.bindLUT() (line 447-452)
//               + Texture.bindSampler() (line 474-479)
#include "Uniforms.h"
#include "Contours.h"
#include "RenderSystemImpl.h"
#include "TargetImpl.h"

BEGIN_DQ_RENDER_NAMESPACE

void BatchUniforms::bindLUT(UniformHandle& uniform, RenderSystemImpl* system,
                             uint32_t unit) const
{
    if (m_overrides) {
        rhi::TextureHandle tex = m_overrides->getTextureHandle();
        if (tex && system)
            system->bindTexture2d(unit, tex);
    }
    uniform.setUniform1i(static_cast<int>(unit - static_cast<uint32_t>(GL::TextureUnit::Zero)));
}

// ---------------------------------------------------------------------------
// Contours（M-X X-a3）
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core BatchUniforms.wantContourLines（BatchUniforms.ts
//   :110-113——`target.currentBranch.contourLine` 在场 && displayContours &&
//   groups.length>0）。currentBranch=栈顶（Target.ts:253/:269 读取面）。
bool BatchUniforms::wantContourLines() const noexcept
{
    if (nullptr == m_target)
        return false;
    auto const* contours = m_target->getCurrentContours();
    return nullptr != contours && contours->displayContours && !contours->groups.empty();
}

// Ported from: itwinjs-core BatchUniforms.bindContourLUT（:115-119——
//   "can't use sync() here because a different texture may have been assigned
//   to the desired texture unit"；DanQing 面=sampler 单元索引恒写[GPU 纹理绑
//   在绘制环——EQUIVALENCE 见 Contours.h getTexture 注]）。
void BatchUniforms::bindContourLUT(UniformHandle& uniform) const
{
    // TextureUnit.Contours - TextureUnit.Zero（RenderFlags.h:169 的 0x84CE 相对
    // 单元 14）——Contours.ts:123-124 的防 loopback 恒写。
    uniform.setUniform1i(static_cast<int>(GL::TextureUnit::Contours)
                         - static_cast<int>(GL::TextureUnit::Zero));
}

// Ported from: itwinjs-core BatchUniforms.bindContourLUTWidth（:121-128——
//   _contours 缺席写 0[着色器早退门]；DanQing 无 sync 缓存[逐帧恒写面]）。
void BatchUniforms::bindContourLUTWidth(UniformHandle& uniform) const
{
    if (nullptr == m_contours)
        uniform.setUniform1ui(0);
    else
        m_contours->bindContourLUTWidth(uniform);
}

END_DQ_RENDER_NAMESPACE

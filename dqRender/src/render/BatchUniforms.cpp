// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — BatchUniforms out-of-line definitions
//
// bindLUT requires the full RenderSystemImpl definition (for bindTexture2d),
// which cannot be included in Uniforms.h without a circular dependency.
// Ported from: itwinjs-core FeatureOverrides.bindLUT() (line 447-452)
//               + Texture.bindSampler() (line 474-479)
#include "Uniforms.h"
#include "RenderSystemImpl.h"

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

END_DQ_RENDER_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — copy color buffer shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/CopyColor.ts
//
// GLSL functions for copying the color buffer to a texture. Two variants:
// CopyAlpha preserves alpha values as-is; NoAlpha premultiplies the
// background alpha into background-color pixels and sets all other pixels
// to full opacity.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// CopyColor — computeColor (with alpha)
// ---------------------------------------------------------------------------
// Copies color texture directly, preserving alpha.
// Ported from: itwinjs-core CopyColor.ts computeColor
static char const* kCopyColorComputeColor = R"glsl(
return TEXTURE(u_color, v_texCoord);
)glsl";

// ---------------------------------------------------------------------------
// CopyColor — computeColor (no alpha)
// ---------------------------------------------------------------------------
// Transparent background color will not have premultiplied alpha — multiply
// it when copying. Set all other pixels opaque.
// Ported from: itwinjs-core CopyColor.ts computeColorNoAlpha
static char const* kCopyColorComputeColorNoAlpha = R"glsl(
  vec4 color = TEXTURE(u_color, v_texCoord);
  if (color == u_bgColor)
    return vec4(color.rgb * color.a, color.a);
  else
    return vec4(color.rgb, 1.0);
)glsl";

// ---------------------------------------------------------------------------
// CopyColor — complete fragment shader (with alpha)
// ---------------------------------------------------------------------------
static char const* kCopyColorFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_color;
out vec4 fragColor;
void main() {
    fragColor = texture(u_color, v_texCoord);
}
)glsl";

// ---------------------------------------------------------------------------
// CopyColorNoAlpha — complete fragment shader (no alpha)
// ---------------------------------------------------------------------------
static char const* kCopyColorNoAlphaFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_color;
uniform vec4 u_bgColor;
out vec4 fragColor;
void main() {
    vec4 color = texture(u_color, v_texCoord);
    if (color == u_bgColor)
        fragColor = vec4(color.rgb * color.a, color.a);
    else
        fragColor = vec4(color.rgb, 1.0);
}
)glsl";

END_DQ_RENDER_NAMESPACE

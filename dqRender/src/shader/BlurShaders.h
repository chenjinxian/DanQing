// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Blur shader sources（AO 的 X/Y 两向高斯模糊）
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Blur.ts
//              （123 行全量——computeBlur 高斯 7 步 + testRenderOrder 臂）。
//
// 两变体（参考 BlurType：NoTest=0 / TestOrder=1——TechniqueId.Blur/
// BlurTestOrder 对应物）：NoTest=纯高斯；TestOrder=先按 pick 纹理的 order
// 通道跳过线/边/轮廓像素（返回 1.0=不遮蔽）。
#pragma once

#include <string_view>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// 全屏四边形顶点着色器（同 AO 面——ViewportQuad 形；与既有 quad 缓冲的
// vec2 a_position 布局一致[compositeOit 同]）。
inline constexpr std::string_view kBlurVert = R"glsl(
#version 410 core
layout(location = 0) in vec2 a_position;
void main() { gl_Position = vec4(a_position, 0.0, 1.0); }
)glsl";

// ---------------------------------------------------------------------------
// computeBlur 主体（Blur.ts:24-56——一维高斯：delta/sigma/texelStepSize 三参
// [u_blurSettings=(blurDelta,blurSigma,blurTexelStepSize)，Blur.ts:106-115
//  绑定位——参考注：该设置借 AO 参数族]）。
// uniforms：u_textureToBlur=单元 0 / u_blurDir=(1,0)|(0,1) / u_viewport。
// ---------------------------------------------------------------------------
inline constexpr std::string_view kBlurCommonPrefix = R"glsl(
#version 410 core

uniform sampler2D u_textureToBlur;
uniform vec2 u_blurDir;
uniform vec3 u_blurSettings;   // (blurDelta, blurSigma, blurTexelStepSize)
uniform vec2 u_viewport;

out vec4 FragColor;

// windowCoordsToTexCoords（Fragment.ts）。
vec2 windowCoordsToTexCoords(vec2 wc) { return wc / u_viewport; }

vec4 computeBlur(vec2 tc) {
  float delta = u_blurSettings.x;
  float sigma = u_blurSettings.y;
  float texelStepSize = u_blurSettings.z;

  vec2 step = texelStepSize / u_viewport;

  vec3 gaussian;
  const float twoPi = 6.283185307179586;
  gaussian.x = 1.0 / (sqrt(twoPi) * sigma);
  gaussian.y = exp((-0.5 * delta * delta) / (sigma * sigma));
  gaussian.z = gaussian.y * gaussian.y;

  vec4 origColor = texture(u_textureToBlur, tc);
  vec4 result = origColor * gaussian.x;
  for (int i = 1; i < 8; i++) {
    gaussian.xy *= gaussian.yz;

    vec2 offset = float(i) * u_blurDir * step;
    vec2 tcMinusOffset = tc - offset;
    vec2 tcPlusOffset = tc + offset;

    result += texture(u_textureToBlur, tcMinusOffset) * gaussian.x;
    result += texture(u_textureToBlur, tcPlusOffset) * gaussian.x;
  }

  return result;
}
)glsl";

// testRenderOrder 臂（Blur.ts:58-71——当前像素为线/边/轮廓则跳过模糊）。
// 须与 kBlurCommonPrefix 串接后置于 main 前；本段自带 u_pickDepthAndOrder。
inline constexpr std::string_view kBlurTestOrderPrefix = R"glsl(
uniform sampler2D u_pickDepthAndOrder;

const float kRenderOrder_Linear = 5.0;
const float kRenderOrder_Silhouette = 7.0;   // RenderOrder.Silhouette（RenderFlags.h:192——**非** 6）
const float kRenderOrder_PlanarBit = 8.0;

// 返回 true=跳过（返回 vec4(1.0)——Blur.ts:69）。
bool testRenderOrderSkip(vec2 rotc) {
  vec4 pdo = texture(u_pickDepthAndOrder, rotc);
  float order = floor(pdo.x * 16.0 + 0.5);
  if (order >= kRenderOrder_PlanarBit)
    order = order - kRenderOrder_PlanarBit;
  return order >= kRenderOrder_Linear && order <= kRenderOrder_Silhouette;
}
)glsl";

// NoTest 变体（TechniqueId.Blur——纯高斯）。
inline constexpr std::string_view kBlurFrag = R"glsl(
void main() {
  vec2 tc = windowCoordsToTexCoords(gl_FragCoord.xy);
  FragColor = computeBlur(tc);
}
)glsl";

// TestOrder 变体（TechniqueId.BlurTestOrder——先跳线边再高斯）。
// 串接形态：kBlurTestOrderPrefix + kBlurCommonPrefix + 本段（prefix 顺序=
// 参考 frag.set(ComputeBaseColor, testRenderOrder + computeBlur) 的
// 串接——函数须先声明，故 prefix 在 common 前）。
inline constexpr std::string_view kBlurTestOrderFrag = R"glsl(
void main() {
  vec2 tc = windowCoordsToTexCoords(gl_FragCoord.xy);
  if (testRenderOrderSkip(tc)) {
    FragColor = vec4(1.0);
    return;
  }
  FragColor = computeBlur(tc);
}
)glsl";

END_DQ_RENDER_NAMESPACE

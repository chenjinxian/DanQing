// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Ambient occlusion shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/AmbientOcclusion.ts
//
// HBAO 式屏空间 AO 的 **PB 变体**（参考 `_shouldUseDB()=supportsLogZBuffer`
// 的另一臂——PB=读 pick 深度纹理[depthAndOrder]，自含于拾取链已锁的既有
// 纹理；M-T T-c 裁决：DanQing FBO 深度附件虽为可采样纹理[DB 物理可行]，
// PB 先行、DB 臂随深度纹理消费面登记）。
//
// M-T T-d：本文件全文替换早期桩版（旧版把 renderOrder 做成**逐 draw 的**
// uniform——参考实为**逐像素**读 pick 纹理的 order 通道[readDepthAndOrder(tc)
// .x]；且缺噪声旋转/距离淡出等段——桩件与参考语义背离，整体重写）。
#pragma once

#include <string_view>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// 全屏四边形顶点着色器（参考 createViewportQuadBuilder(true) 的顶点面——
// ViewportQuad.ts；DanQing 全屏 quad 惯用形[compositeOit 同]）。
// ---------------------------------------------------------------------------
inline constexpr std::string_view kAmbientOcclusionVert = R"glsl(
#version 410 core
layout(location = 0) in vec2 a_position;
void main() { gl_Position = vec4(a_position, 0.0, 1.0); }
)glsl";

// ---------------------------------------------------------------------------
// AO 片元着色器（PB 变体）——参考 createAmbientOcclusionProgram 的
// shouldUseDB==false 臂全量（:60-128 主循环 + :130-200 辅助函数 +
// 前缀 computeAmbientOcclusionPrefixPB）。
//
//  uniforms（参考绑定位）：u_pickDepthAndOrder=单元 0 / u_noise=单元 1 /
//  u_frustum=(near,far,type) / u_frustumPlanes=(top,bottom,left,right) /
//  u_viewport=(w,h) / u_invProj / u_hbaoSettings=(bias,zLengthCap,intensity,
//  texelStepSize) / u_maxDistance。
// ---------------------------------------------------------------------------
inline constexpr std::string_view kAmbientOcclusionFrag = R"glsl(
#version 410 core

uniform sampler2D u_pickDepthAndOrder;
uniform sampler2D u_noise;
uniform vec3 u_frustum;          // (near, far, frustumType)——addFrustum
uniform vec4 u_frustumPlanes;    // (top, bottom, left, right)
uniform vec2 u_viewport;         // (w, h)——addViewport
uniform mat4 u_invProj;
uniform vec4 u_hbaoSettings;     // (bias, zLengthCap, intensity, texelStepSize)
uniform float u_maxDistance;

out vec4 FragColor;

// 参考常量（RenderOrder——FeatureSymbologyShaders.h:217-221 同值）。
const float kFrustumType_Perspective = 2.0;
const float kRenderOrder_LitSurface = 4.0;
const float kRenderOrder_Linear = 5.0;
const float kRenderOrder_PlanarBit = 8.0;

// windowCoordsToTexCoords（Fragment.ts addWindowToTexCoords）。
vec2 windowCoordsToTexCoords(vec2 wc) { return wc / u_viewport; }

// decodeDepthRgb（Decode.ts:37-39）。
float decodeDepthRgb(vec3 rgb) { return dot(rgb, vec3(1.0, 1.0 / 255.0, 1.0 / 65025.0)); }

// readDepthAndOrder（FeatureSymbology.ts:395-401——pick 纹理 RGBA8：
// order=x*16 打包、深度 RGB 打包在 yzw）。
vec2 readDepthAndOrder(vec2 tc) {
  vec4 pdo = texture(u_pickDepthAndOrder, tc);
  float order = floor(pdo.x * 16.0 + 0.5);
  return vec2(order, decodeDepthRgb(pdo.yzw));
}

// ── PB 臂三件套（参考 :216-240）────────────────────────────────
// computeNonLinearDepthPB：线性深度→非线性（mix(far, near, linear)）。
float computeNonLinearDepth(float linearDepth) {
  return mix(u_frustum.y, u_frustum.x, linearDepth);
}
// readDepthPB：pick 纹理读深度（y 通道的打包线性深度——decode 后）。
float readDepth(vec2 tc) {
  return readDepthAndOrder(tc).y;
}
// 参考 PB 臂的 `addDefine("unfinalizeLinearDepth", "")`——对象宏空展开：
// `unfinalizeLinearDepth(db)` → `(db)`（PB 的 db 已是线性深度，恒等）。
#define unfinalizeLinearDepth

// computePositionFromDepth（:130-143——透视经 u_invProj 反投影 /
// 正交经 frustumPlanes 线性插值）。
vec4 computePositionFromDepth(vec2 tc, float nonLinearDepth) {
  if (kFrustumType_Perspective == u_frustum.z) {
    vec2 xy = vec2((tc.x * 2.0 - 1.0), ((1.0 - tc.y) * 2.0 - 1.0));
    vec4 posEC = u_invProj * vec4(xy, nonLinearDepth, 1.0);
    posEC = posEC / posEC.w;
    return posEC;
  } else {
    float top = u_frustumPlanes.x;
    float bottom = u_frustumPlanes.y;
    float left = u_frustumPlanes.z;
    float right = u_frustumPlanes.w;
    return vec4(mix(left, right, tc.x), mix(bottom, top, tc.y), nonLinearDepth, 1.0);
  }
}

// computeNormalFromDepth（:147-168——四邻域差分取短边叉积）。
vec3 computeNormalFromDepth(vec3 viewPos, vec2 tc, vec2 pixelSize) {
  float nonLinearDepthU = computeNonLinearDepth(readDepth(tc - vec2(0.0, pixelSize.y)));
  float nonLinearDepthD = computeNonLinearDepth(readDepth(tc + vec2(0.0, pixelSize.y)));
  float nonLinearDepthL = computeNonLinearDepth(readDepth(tc - vec2(pixelSize.x, 0.0)));
  float nonLinearDepthR = computeNonLinearDepth(readDepth(tc + vec2(pixelSize.x, 0.0)));

  vec3 viewPosUp = computePositionFromDepth(tc - vec2(0.0, pixelSize.y), nonLinearDepthU).xyz;
  vec3 viewPosDown = computePositionFromDepth(tc + vec2(0.0, pixelSize.y), nonLinearDepthD).xyz;
  vec3 viewPosLeft = computePositionFromDepth(tc - vec2(pixelSize.x, 0.0), nonLinearDepthL).xyz;
  vec3 viewPosRight = computePositionFromDepth(tc + vec2(pixelSize.x, 0.0), nonLinearDepthR).xyz;

  vec3 up = viewPos.xyz - viewPosUp.xyz;
  vec3 down = viewPosDown.xyz - viewPos.xyz;
  vec3 left = viewPos.xyz - viewPosLeft.xyz;
  vec3 right = viewPosRight.xyz - viewPos.xyz;

  vec3 dx = length(left) < length(right) ? left : right;
  vec3 dy = length(up) < length(down) ? up : down;

  return normalize(cross(dy, dx));
}

// 主体（:44-128——computeAmbientOcclusionPrefixPB 内联后接
// computeAmbientOcclusion 原文；assignFragColor=输出）。
void main() {
  // computeAmbientOcclusionPrefixPB
  vec2 tc = windowCoordsToTexCoords(gl_FragCoord.xy);
  vec2 depthAndOrder = readDepthAndOrder(tc);
  float db = depthAndOrder.y;

  // computeAmbientOcclusion
  depthAndOrder.y = unfinalizeLinearDepth(db);   // PB 宏展开 → (db)
  float order = depthAndOrder.x;
  if (order >= kRenderOrder_PlanarBit)
    order = order - kRenderOrder_PlanarBit;

  if (order < kRenderOrder_LitSurface || order == kRenderOrder_Linear) {
    FragColor = vec4(1.0);
    return;
  }

  // NB: linearDepth: 1 == near, 0 == far
  float linearDepth = depthAndOrder.y;
  float nonLinearDepth = computeNonLinearDepth(db);
  if (nonLinearDepth > u_maxDistance) {
    FragColor = vec4(1.0);
    return;
  }

  vec3 viewPos = computePositionFromDepth(tc, nonLinearDepth).xyz;

  vec2 pixelSize = 1.0 / u_viewport;
  vec3 viewNormal = computeNormalFromDepth(viewPos, tc, pixelSize);

  vec2 sampleDirection = vec2(1.0, 0.0);
  float gapAngle = 90.0 * 0.017453292519943295; // radians per degree

  // 噪声旋转：4×4 噪声纹理平铺（u_viewport/4 平铺率）——(TEXTURE(...).rgb
  // +1)/2 的 [0,1] 重映射[参考注释：噪声字节是 -1..1 的编码——Luminance
  // 纹理读出已归一]。
  vec3 noiseVec = (texture(u_noise, tc * vec2(u_viewport.x / 4.0, u_viewport.y / 4.0)).rgb + 1.0) / 2.0;

  float bias = u_hbaoSettings.x;
  float zLengthCap = u_hbaoSettings.y;
  float intensity = u_hbaoSettings.z;
  float texelStepSize = clamp(u_hbaoSettings.w * linearDepth, 1.0, u_hbaoSettings.w);

  float tOcclusion = 0.0;

  // loop for each direction
  for (int i = 0; i < 4; i++) {
    float newGapAngle = gapAngle * (float(i) + noiseVec.x);
    float cosVal = cos(newGapAngle);
    float sinVal = sin(newGapAngle);

    // rotate sampling direction
    vec2 rotatedSampleDirection = vec2(cosVal * sampleDirection.x - sinVal * sampleDirection.y, sinVal * sampleDirection.x + cosVal * sampleDirection.y);
    float curOcclusion = 0.0;
    float curStepSize = texelStepSize;

    // loop for each step
    for (int j = 0; j < 6; j++) {
      vec2 directionWithStep = vec2(rotatedSampleDirection.x * curStepSize * pixelSize.x, rotatedSampleDirection.y * curStepSize * pixelSize.y);
      vec2 newCoords = directionWithStep + tc;

      // do not repeat around the depth texture
      if (newCoords.x > 1.0 || newCoords.y > 1.0 || newCoords.x < 0.0 || newCoords.y < 0.0) {
          break;
      }

      db = readDepth(newCoords);
      float curLinearDepth = unfinalizeLinearDepth(db);
      float curNonLinearDepth = computeNonLinearDepth(db);
      vec3 curViewPos = computePositionFromDepth(newCoords, curNonLinearDepth).xyz;
      vec3 diffVec = curViewPos.xyz - viewPos.xyz;
      float zLength = abs(curLinearDepth - linearDepth);

      float dotVal = clamp(dot(viewNormal, normalize(diffVec)), 0.0, 1.0);
      float weight = smoothstep(0.0, 1.0, zLengthCap / zLength);

      if (dotVal < bias) {
          dotVal = 0.0;
      }

      curOcclusion = max(curOcclusion, dotVal * weight);
      curStepSize += texelStepSize;
    }
    tOcclusion += curOcclusion;
  }

  float distanceFadeFactor = kFrustumType_Perspective == u_frustum.z ? 1.0 - pow(clamp(nonLinearDepth / u_maxDistance, 0.0, 1.0), 4.0) : 1.0;
  tOcclusion *= distanceFadeFactor;

  tOcclusion /= 4.0;
  tOcclusion = 1.0 - clamp(tOcclusion, 0.0, 1.0);
  tOcclusion = pow(tOcclusion, intensity);

  FragColor = vec4(tOcclusion, tOcclusion, tOcclusion, 1.0);
}
)glsl";

END_DQ_RENDER_NAMESPACE

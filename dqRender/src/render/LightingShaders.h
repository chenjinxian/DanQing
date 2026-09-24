// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Lighting shader module
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Lighting.ts
//
// Computes directional + ambient + hemisphere + fresnel lighting in the
// ApplyLighting fragment component. Depends on Common.addFrustum.
#pragma once

#include "CommonShaders.h"  // addFrustum
#include "ShaderBindings.h"  // wireSunDirection, wireLightSettings, wireUpVector
#include "ShaderBuilder.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Reference GLSL fragments (exact ports from Lighting.ts)
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Lighting.ts computeDirectionalLighting
inline constexpr char const* kComputeDirectionalLighting = R"(
void computeDirectionalLight (inout float diffuse, inout float specular, vec3 normal, vec3 toEye, vec3 lightDir, float lightIntensity, float specularExponent) {
  diffuse += lightIntensity * max(dot(normal, lightDir), 0.0);
  vec3 toReflectedLight = normalize(reflect(lightDir, normal));
  float specularDot = max(dot(toReflectedLight, toEye), 0.0001);
  // NB: If specularDot and specularExponent are both zero, 0^0 done below can return NaN.  Must make sure specularDot is larger than zero (hence 0.0001 or greater, as ensured above).
  specular += lightIntensity * pow(specularDot, specularExponent);
}
)";

// Ported from: itwinjs-core Lighting.ts applyLighting (mat_weights: x=diffuse y=specular)
inline constexpr char const* kApplyLighting = R"(
  if (baseColor.a <= 0.0 || !u_surfaceFlags[kSurfaceBitIndex_ApplyLighting])
    return baseColor;

  // Extract surface properties
  vec3 rgb = baseColor.rgb;
  vec3 toEye = kFrustumType_Perspective == u_frustum.z ? normalize(v_eyeSpace.xyz) : vec3(0.0, 0.0, -1.0);

  // Extract material properties
  float diffuseWeight = mat_weights.x;
  float specularWeight = mat_weights.y * u_lightSettings[13];
  float specularExponent = mat_specular.a;
  vec3 specularColor = mat_specular.rgb;
  const float ambientWeight = 1.0; // Ignore MicroStation's ambient weights - usually bogus.

  // Compute directional lights
  const vec3 portraitDir = vec3(-0.7071, 0.0, 0.7071);
  float portraitIntensity = u_lightSettings[12];
  float sunIntensity = u_lightSettings[0];

  float directionalDiffuseIntensity = 0.0;
  float directionalSpecularIntensity = 0.0;
  computeDirectionalLight(directionalDiffuseIntensity, directionalSpecularIntensity, g_normal, toEye, u_sunDir, sunIntensity, specularExponent);
  computeDirectionalLight(directionalDiffuseIntensity, directionalSpecularIntensity, g_normal ,toEye, portraitDir, portraitIntensity, specularExponent);

  const float directionalFudge = 0.92; // leftover from old lighting implementation
  vec3 diffuseAccum = directionalDiffuseIntensity * diffuseWeight * rgb * directionalFudge; // directional light is white.
  vec3 specularAccum = directionalSpecularIntensity * specularWeight * specularColor;

  // Compute ambient light
  float ambientIntensity = u_lightSettings[4];
  vec3 ambientColor = vec3(u_lightSettings[1], u_lightSettings[2], u_lightSettings[3]);
  if (ambientColor.r + ambientColor.g + ambientColor.b == 0.0)
    ambientColor = rgb;

  diffuseAccum += ambientIntensity * ambientWeight * ambientColor;

  // Compute hemisphere lights
  vec3 ground = vec3(u_lightSettings[5], u_lightSettings[6], u_lightSettings[7]);
  vec3 sky = vec3(u_lightSettings[8], u_lightSettings[9], u_lightSettings[10]);
  float hemiIntensity = u_lightSettings[11];

  //  diffuse
  float hemiDot = dot(g_normal, u_upVector);
  float hemiDiffuseWeight = 0.5 * hemiDot + 0.5;
  vec3 hemiColor = mix(ground, sky, hemiDiffuseWeight);
  diffuseAccum += hemiIntensity * hemiColor * rgb;

  //  sky specular
  vec3 reflectSky = normalize(reflect(u_upVector, g_normal));
  float skyDot = max(dot(reflectSky, toEye), 0.0001);
  float hemiSpecWeight = hemiIntensity * pow(skyDot, specularExponent);

  //  ground specular
  vec3 reflectGround = normalize(reflect(-u_upVector, g_normal));
  float groundDot = max(dot(reflectGround, toEye), 0.0001);
  hemiSpecWeight += hemiIntensity * pow(groundDot, specularExponent);

  specularAccum += hemiSpecWeight * specularColor * hemiColor;

  vec3 litColor = diffuseAccum + specularAccum;

  // Apply fresnel reflection.
  float fresnelIntensity = u_lightSettings[15];
  if (0.0 != fresnelIntensity) {
    float fresnel = -dot(toEye, g_normal);
    if (fresnelIntensity < 0.0) {
      fresnelIntensity = abs(fresnelIntensity);
      fresnel = 1.0 - fresnel;
    }

    fresnel = clamp(1.0 - fresnel, 0.0, 1.0);
    litColor = litColor * (1.0 + fresnelIntensity * fresnel);
  }

  // clamp while preserving hue.
  float maxIntensity = max(litColor.r, max(litColor.g, litColor.b));
  float numCel = u_lightSettings[14];
  if (numCel > 0.0)
    baseColor.rgb = baseColor.rgb * ceil(maxIntensity * numCel) / numCel;
  else
    baseColor.rgb = litColor / max(1.0, maxIntensity);

  return baseColor;
)";

// ---------------------------------------------------------------------------
// Wiring (exact port of Lighting.ts addLighting)
//
// NOTE: addMaterial() sets up the mat_* variables used by applyLighting.
// Uniform VALUE bindings (u_sunDir, u_lightSettings, u_upVector) are registered
// with binding=nullptr per convention.
// ---------------------------------------------------------------------------

/// Wire lighting: addFrustum + computeDirectionalLighting function + ApplyLighting
/// fragment component + u_sunDir (high precision) / u_lightSettings[16] / u_upVector.
/// Ported from: itwinjs-core Lighting.ts addLighting()
inline void addLighting(ProgramBuilder& builder)
{
    addFrustum(builder);

    ShaderBuilder& frag = builder.getFragmentBuilder();
    frag.addFunction(std::string(kComputeDirectionalLighting));
    // Function-call convention: slot holds the body (ends in `return baseColor;`);
    // buildFragmentMain wraps it as `vec4 applyLighting(vec4 baseColor)`.
    // Ported from: itwinjs-core Lighting.ts applyLighting.
    frag.setFragmentComponent(FragmentShaderComponent::ApplyLighting,
                              std::string(kApplyLighting));
    wireSunDirection(frag);
    wireLightSettings(frag);
    wireUpVector(frag);
}

END_DQ_RENDER_NAMESPACE

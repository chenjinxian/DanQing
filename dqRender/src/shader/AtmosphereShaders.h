// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Atmosphere shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Atmosphere.ts
//
// Rayleigh atmospheric scattering implementation. Uses ray-ellipsoid
// intersection, density sampling via exponential falloff, optical depth
// integration via trapezoid rule, and HDR tone mapping.
//
// The effect is compositional: these functions are added to existing
// surface/skybox shaders via ShaderBuilder (not standalone programs).
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Atmosphere constants
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core AtmosphereUniforms.ts MAX_SAMPLE_POINTS = 40
static char const* kAtmosphereConstants = R"glsl(
const float MAX_FLOAT = 3.402823466e+38;
const int MAX_SAMPLE_POINTS = 40;
)glsl";

// ---------------------------------------------------------------------------
// computeRayDir — compute viewing ray direction from eye-space position
// ---------------------------------------------------------------------------
// For perspective camera, normalizes the eye-space position.
// For orthographic, uses a fixed forward direction.
// Ported from: itwinjs-core Atmosphere.ts computeRayDir
static char const* kAtmosphereComputeRayDir = R"glsl(
vec3 computeRayDir(vec3 eyeSpace) {
  bool isCameraEnabled = u_frustum.z == 2.0;
  return isCameraEnabled ? normalize(eyeSpace) : vec3(0.0, 0.0, -1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// computeSceneDepthDefault — compute scene depth for non-sky geometry
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core Atmosphere.ts computeSceneDepthDefault
static char const* kAtmosphereComputeSceneDepthDefault = R"glsl(
float computeSceneDepth(vec3 eyeSpace) {
  bool isCameraEnabled = u_frustum.z == 2.0;
  return isCameraEnabled ? length(eyeSpace) : -eyeSpace.z;
}
)glsl";

// ---------------------------------------------------------------------------
// computeSceneDepthSky — compute scene depth for skybox (infinite)
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core Atmosphere.ts computeSceneDepthSky
static char const* kAtmosphereComputeSceneDepthSky = R"glsl(
float computeSceneDepth(vec3 eyeSpace) {
  return MAX_FLOAT;
}
)glsl";

// ---------------------------------------------------------------------------
// computeRayOrigin — compute ray origin from eye-space position
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core Atmosphere.ts computeRayOrigin
static char const* kAtmosphereComputeRayOrigin = R"glsl(
vec3 computeRayOrigin(vec3 eyeSpace) {
  bool isCameraEnabled = u_frustum.z == 2.0;
  return isCameraEnabled ? vec3(0.0) : vec3(eyeSpace.xy, 0.0);
}
)glsl";

// ---------------------------------------------------------------------------
// raySphere — ray-sphere intersection
// ---------------------------------------------------------------------------
// Returns vec2(distanceToNear, distanceThroughSphere).
// Adapted from https://math.stackexchange.com/questions/1939423
// Ported from: itwinjs-core Atmosphere.ts raySphere
static char const* kAtmosphereRaySphere = R"glsl(
vec2 raySphere(vec3 sphereCenter, float sphereRadius, vec3 rayOrigin, vec3 rayDir) {
  float a = 1.0;
  vec3 offset = rayOrigin - sphereCenter;
  float b = 2.0 * dot(offset, rayDir);
  float c = dot(offset, offset) - sphereRadius * sphereRadius;
  float discriminant = b * b - 4.0 * a * c;

  if (discriminant <= 0.0) {
    return vec2(MAX_FLOAT, 0.0);
  }

  float s = sqrt(discriminant);
  float firstRoot = (-b - s) / (2.0 * a);
  float secondRoot = (-b + s) / (2.0 * a);
  if (firstRoot <= 0.0 && secondRoot <= 0.0) {
    return vec2(MAX_FLOAT, 0.0);
  }
  float distanceToSphereNear = max(0.0, firstRoot);
  float distanceToSphereFar = secondRoot;
  return vec2(distanceToSphereNear, distanceToSphereFar - distanceToSphereNear);
}
)glsl";

// ---------------------------------------------------------------------------
// rayEllipsoidIntersection — ray-ellipsoid intersection
// ---------------------------------------------------------------------------
// Transforms coordinates so the ellipsoid is a unit sphere, intersects,
// then transforms distances back to ellipsoid space.
// Ported from: itwinjs-core Atmosphere.ts rayEllipsoidIntersection
static char const* kAtmosphereRayEllipsoidIntersection = R"glsl(
vec2 rayEllipsoidIntersection(
  vec3 ellipsoidCenter,
  vec3 rayOrigin,
  vec3 rayDir,
  mat4 inverseScaleInverseRotationMatrix4,
  mat4 ellipsoidScaleMatrix4
) {
  mat3 inverseScaleInverseRotationMatrix = mat3(inverseScaleInverseRotationMatrix4);
  mat3 ellipsoidScaleMatrix = mat3(ellipsoidScaleMatrix4);
  vec3 rayOriginFromEllipsoid = rayOrigin - ellipsoidCenter;
  vec3 rayOriginFromAxisAlignedUnitSphere = inverseScaleInverseRotationMatrix * rayOriginFromEllipsoid;
  vec3 rayDirFromAxisAlignedUnitSphere = normalize(inverseScaleInverseRotationMatrix * rayDir);

  vec2 intersectionInfo = raySphere(vec3(0.0), 1.0, rayOriginFromAxisAlignedUnitSphere, rayDirFromAxisAlignedUnitSphere);

  float distanceToEllipsoidNear = length(ellipsoidScaleMatrix * rayDirFromAxisAlignedUnitSphere * intersectionInfo[0]);
  float distanceThroughEllipsoid = length(ellipsoidScaleMatrix * rayDirFromAxisAlignedUnitSphere * intersectionInfo[1]);
  return vec2(distanceToEllipsoidNear, distanceThroughEllipsoid);
}
)glsl";

// ---------------------------------------------------------------------------
// densityAtPoint — atmospheric density at a point
// ---------------------------------------------------------------------------
// Returns density in [0,1] based on altitude between max density threshold
// and atmosphere radius. Density decreases exponentially, modulated by
// densityFalloff coefficient.
// Ported from: itwinjs-core Atmosphere.ts densityAtPoint
static char const* kAtmosphereDensityAtPoint = R"glsl(
float densityAtPoint(vec3 point, vec3 earthCenter, float atmosphereRadiusScaleFactor, float atmosphereMaxDensityThresholdScaleFactor, float densityFalloff) {
  vec3 pointFromEarthCenter = mat3(u_inverseEarthScaleInverseRotationMatrix) * (point - earthCenter);

  if (length(pointFromEarthCenter) <= atmosphereMaxDensityThresholdScaleFactor) {
    return 1.0;
  }
  else if (length(pointFromEarthCenter) >= atmosphereRadiusScaleFactor) {
    return 0.0;
  }

  float atmosphereDistanceFromMaxDensityThreshold = atmosphereRadiusScaleFactor - atmosphereMaxDensityThresholdScaleFactor;
  float samplePointDistanceFromMaxDensityThreshold = length(pointFromEarthCenter) - atmosphereMaxDensityThresholdScaleFactor;
  float heightFrom0to1 = samplePointDistanceFromMaxDensityThreshold / atmosphereDistanceFromMaxDensityThreshold;
  float result = exp(-heightFrom0to1 * densityFalloff) * (1.0 - heightFrom0to1);

  return result;
}
)glsl";

// ---------------------------------------------------------------------------
// opticalDepth — optical depth integration via trapezoid rule
// ---------------------------------------------------------------------------
// Integrates atmospheric density along a ray segment. Uses trapezoid
// rule with numSamplePoints discrete samples.
// Ported from: itwinjs-core Atmosphere.ts opticalDepth
static char const* kAtmosphereOpticalDepth = R"glsl(
float opticalDepth(vec3 rayOrigin, vec3 rayDir, float rayLength, int numSamplePoints, vec3 earthCenter, float atmosphereRadiusScaleFactor, float atmosphereMaxDensityThresholdScaleFactor, float densityFalloff) {
  if (numSamplePoints <= 1) {
    return densityAtPoint(rayOrigin, earthCenter, atmosphereRadiusScaleFactor, atmosphereMaxDensityThresholdScaleFactor, densityFalloff) * rayLength;
  }

  int numPartitions = numSamplePoints - 1;
  float stepSize = rayLength / float(numPartitions);
  vec3 samplePointA = rayOrigin;
  vec3 samplePointB = rayOrigin + (rayDir * stepSize);
  float samplePointADensity = densityAtPoint(samplePointA, earthCenter, atmosphereRadiusScaleFactor, atmosphereMaxDensityThresholdScaleFactor, densityFalloff);
  float trapezoidRuleSum = 0.0;

  for (int i = 1; i <= numPartitions; i++) {
    float samplePointBDensity = densityAtPoint(samplePointB, earthCenter, atmosphereRadiusScaleFactor, atmosphereMaxDensityThresholdScaleFactor, densityFalloff);

    trapezoidRuleSum += samplePointADensity + samplePointBDensity;
    samplePointADensity = samplePointBDensity;
    samplePointB += rayDir * stepSize;
  }

  float opticalDepth = trapezoidRuleSum * stepSize / 2.0;
  return opticalDepth;
}
)glsl";

// ---------------------------------------------------------------------------
// calculateReflectedLightIntensity — surface scattering light intensity
// ---------------------------------------------------------------------------
// Computes the intensity of light directly reflected toward the camera
// by a surface, using wavelength-specific scattering with uniform
// interpolation to reduce excessive red at sunset.
// Ported from: itwinjs-core Atmosphere.ts calculateReflectedLightIntensity
static char const* kAtmosphereCalculateReflectedLightIntensity = R"glsl(
vec3 calculateReflectedLightIntensity(float opticalDepth, vec3 scatteringCoefficients) {
    float averageScatteringValue = (scatteringCoefficients.x + scatteringCoefficients.y + scatteringCoefficients.z) / 3.0;
    vec3 equalScatteringByWavelength = vec3(averageScatteringValue);
    vec3 scatteringStrength = mix(equalScatteringByWavelength, scatteringCoefficients, 0.5);
    vec3 outScatteredLight = opticalDepth * scatteringStrength;

    vec3 sunlightColor = vec3(1.0, 0.95, 0.925);
    vec3 reflectedLightIntensity = sunlightColor * exp(-outScatteredLight);
    return reflectedLightIntensity;
}
)glsl";

// ---------------------------------------------------------------------------
// computeAtmosphericScattering — main scattering computation
// ---------------------------------------------------------------------------
// Computes in-scattering and surface scattering along the view ray.
// Returns mat3 where column 0 = scattered light color, column 1 = reflected
// light intensity, column 2 = unused (zero).
// Ported from: itwinjs-core Atmosphere.ts computeAtmosphericScatteringFromScratch
static char const* kAtmosphereComputeScattering = R"glsl(
mat3 computeAtmosphericScattering(bool isSkyBox) {
  mat3 emptyResult = mat3(vec3(0.0), vec3(1.0), vec3(0.0));
  vec3 rayDir = computeRayDir(v_eyeSpace);
  vec3 rayOrigin = computeRayOrigin(v_eyeSpace);
  float sceneDepth = computeSceneDepth(v_eyeSpace);
  float diameterOfEarthAtPole = u_earthScaleMatrix[2][2];
  vec3 earthCenter = vec3(u_atmosphereData[2]);

  vec2 earthHitInfo = rayEllipsoidIntersection(earthCenter, rayOrigin, rayDir, u_inverseEarthScaleInverseRotationMatrix, u_earthScaleMatrix);
  vec2 atmosphereHitInfo = rayEllipsoidIntersection(earthCenter, rayOrigin, rayDir, u_inverseAtmosphereScaleInverseRotationMatrix, u_atmosphereScaleMatrix);

  float distanceThroughAtmosphere = min(
    atmosphereHitInfo[1],
    min(sceneDepth, earthHitInfo[0] - atmosphereHitInfo[0])
  );

  if (distanceThroughAtmosphere <= 0.0) {
    return emptyResult;
  }

  float ignoreDistanceThreshold = diameterOfEarthAtPole * 0.15;
  bool ignoreRaycastsIntersectingEarth = isSkyBox;
  if (ignoreRaycastsIntersectingEarth && earthHitInfo[1] > ignoreDistanceThreshold) {
    return emptyResult;
  }

  int numPartitions = int(u_atmosphereData[1][0]) - 1;
  if (numPartitions <= 0) {
    return emptyResult;
  }

  float stepSize = distanceThroughAtmosphere / float(numPartitions);
  vec3 step = rayDir * stepSize;
  vec3 firstPointInAtmosphere = rayDir * atmosphereHitInfo[0] + rayOrigin;
  vec3 scatterPoint = firstPointInAtmosphere;

  float atmosphereRadiusScaleFactor = u_atmosphereData[0][0];
  float atmosphereMaxDensityThresholdScaleFactor = u_atmosphereData[0][1];
  float densityFalloff = u_atmosphereData[0][2];
  vec3 scatteringCoefficients = vec3(u_atmosphereData[3]);

  float opticalDepthFromRayOriginToSamplePoints[MAX_SAMPLE_POINTS];
  opticalDepthFromRayOriginToSamplePoints[0] = 0.0;

  vec3 lightScatteredTowardsCamera = vec3(0.0);
  float opticalDepthFromSunToCameraThroughLastSamplePoint = 0.0;

  for (int i = 1; i <= numPartitions; i++) {
    float opticalDepthForCurrentPartition = opticalDepth(scatterPoint, rayDir, stepSize, 2, earthCenter, atmosphereRadiusScaleFactor, atmosphereMaxDensityThresholdScaleFactor, densityFalloff);
    opticalDepthFromRayOriginToSamplePoints[i] = opticalDepthForCurrentPartition + opticalDepthFromRayOriginToSamplePoints[i-1];

    vec2 sunRayAtmosphereHitInfo = rayEllipsoidIntersection(earthCenter, scatterPoint, u_sunDir, u_inverseAtmosphereScaleInverseRotationMatrix, u_atmosphereScaleMatrix);
    int numSunRaySamples = int(u_atmosphereData[1][1]);
    float sunRayOpticalDepthToScatterPoint = opticalDepth(scatterPoint, u_sunDir, sunRayAtmosphereHitInfo[1], numSunRaySamples, earthCenter, atmosphereRadiusScaleFactor, atmosphereMaxDensityThresholdScaleFactor, densityFalloff);

    float totalOpticalDepthFromSunToCamera = (sunRayOpticalDepthToScatterPoint + opticalDepthFromRayOriginToSamplePoints[i]) / diameterOfEarthAtPole;
    float averageDensityAcrossPartition = opticalDepthForCurrentPartition / stepSize;
    vec3 outScatteredLight = scatteringCoefficients * totalOpticalDepthFromSunToCamera;

    lightScatteredTowardsCamera += averageDensityAcrossPartition * exp(-outScatteredLight);

    opticalDepthFromSunToCameraThroughLastSamplePoint = totalOpticalDepthFromSunToCamera;
    scatterPoint += step;
  }

  float stepSizeByEarthDiameter = (stepSize / diameterOfEarthAtPole);
  vec3 totalLightScatteredTowardsCamera = scatteringCoefficients * stepSizeByEarthDiameter * lightScatteredTowardsCamera;

  vec3 reflectedLightIntensity = isSkyBox ? vec3(1.0) : calculateReflectedLightIntensity(opticalDepthFromSunToCameraThroughLastSamplePoint, scatteringCoefficients);

  return mat3(totalLightScatteredTowardsCamera, reflectedLightIntensity, vec3(0.0));
}
)glsl";

// ---------------------------------------------------------------------------
// applyHdr — high dynamic range tone mapping
// ---------------------------------------------------------------------------
// Compresses over-exposed colors using an exponential curve that preserves
// relative color intensity.
// Ported from: itwinjs-core Atmosphere.ts applyHdr
static char const* kAtmosphereApplyHdr = R"glsl(
vec3 applyHdr(vec3 color) {
  float exposure = u_exposure;
  vec3 colorWithHdr = 1.0 - exp(-exposure * color);
  return colorWithHdr;
}
)glsl";

// ---------------------------------------------------------------------------
// Atmosphere uniforms (included by other shaders via ShaderBuilder)
// ---------------------------------------------------------------------------
// Uniforms:
//   u_atmosphereData (mat4) — atmosphere parameters packed into matrix
//   u_sunDir (vec3) — normalized sun direction (high precision)
//   u_atmosphereScaleMatrix (mat3) — atmosphere ellipsoid scale
//   u_inverseAtmosphereScaleInverseRotationMatrix (mat3) — inverse rotation+scale
//   u_inverseEarthScaleInverseRotationMatrix (mat3) — inverse earth rotation+scale
//   u_earthScaleMatrix (mat3) — earth ellipsoid scale
//   u_frustum (vec3) — {near, far, cameraType}
//   u_exposure (float) — HDR exposure (high precision)
static char const* kAtmosphereUniforms = R"glsl(
uniform mat4 u_atmosphereData;
uniform vec3 u_sunDir;
uniform mat3 u_atmosphereScaleMatrix;
uniform mat3 u_inverseAtmosphereScaleInverseRotationMatrix;
uniform mat3 u_inverseEarthScaleInverseRotationMatrix;
uniform mat3 u_earthScaleMatrix;
uniform vec3 u_frustum;
uniform float u_exposure;
)glsl";

END_DQ_RENDER_NAMESPACE

// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — point cloud shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/PointCloud.ts
//
// Renders point clouds as GL_POINTS with configurable size.
// Supports pixel/meter sizing, BGR color format, and square/circular points.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// PointCloud vertex shader
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core PointCloud.ts computePosition + computeColor
// Supports pixel-vs-meter sizing via u_pointCloudSettings.x flag.
static char const* kPointCloudVert = R"glsl(
#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;

uniform mat4 u_mvp;
uniform mat4 u_viewportTransformation;
uniform vec4 u_pointCloudSettings;  // x=size/mode, y=minSize, z=maxSize, w=squarePoints
uniform vec2 u_pointCloud;          // x=voxelSize, y=colorIsBgr

out vec4 v_color;

void main()
{
    gl_PointSize = 1.0;
    vec4 pos = u_mvp * vec4(a_position, 1.0);

    if (u_pointCloudSettings.x > 0.0) {
        // Size is specified in pixels
        gl_PointSize = u_pointCloudSettings.x;
    } else if (pos.w > 0.0) {
        // Point size is in meters (voxel size)
        // Convert voxel size in meters into pixel size
        mat4 toView = u_viewportTransformation * u_mvp;
        float scale = length(toView[0].xyz);
        gl_PointSize = -u_pointCloudSettings.x * clamp(u_pointCloud.x * scale / pos.w, u_pointCloudSettings.y, u_pointCloudSettings.z);
    }

    gl_Position = pos;

    // BGR color format support
    // Ported from: itwinjs-core PointCloud.ts computeColor
    v_color = u_pointCloud.y == 1.0 ? vec4(a_color.b, a_color.g, a_color.r, 1.0) : vec4(a_color.rgb, 1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// PointCloud fragment shader
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core PointCloud.ts roundPointDiscard + computeBaseColor
// Supports square points (u_pointCloudSettings.w == 1.0) and circular points.
static char const* kPointCloudFrag = R"glsl(
#version 410 core

in vec4 v_color;
uniform vec4 u_pointCloudSettings;  // w = squarePoints flag

out vec4 fragColor;

void main()
{
    // Square point option: skip circular discard
    // Ported from: itwinjs-core PointCloud.ts roundPointDiscard
    if (u_pointCloudSettings.w != 1.0) {
        vec2 pointXY = (2.0 * gl_PointCoord - 1.0);
        if (dot(pointXY, pointXY) > 1.0) discard;
    }

    fragColor = v_color;
}
)glsl";

// ---------------------------------------------------------------------------
// PointString vertex shader (same as PointCloud but with line strip)
// ---------------------------------------------------------------------------
static char const* kPointStringVert = kPointCloudVert;
static char const* kPointStringFrag = kPointCloudFrag;

END_DQ_RENDER_NAMESPACE

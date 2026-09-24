// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Sky GLSL shader builders
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/
//              SkyBox.ts, SkySphere.ts, ScreenSpaceEffect.ts
//
// Shader builders for sky rendering (cubemap, gradient, texture).
#pragma once

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// SkyBox cubemap shader
// (Ported from: itwinjs-core glsl/SkyBox.ts)
// ---------------------------------------------------------------------------
inline std::string buildSkyBoxVertexShader()
{
    std::string src;
    src += "#version 410 core\n\n";
    src += "layout(location = 0) in vec3 a_position;\n";
    src += "uniform mat4 u_mvp;\n";
    src += "\nout vec3 v_texCoord;\n";
    src += "\nvoid main()\n{\n";
    src += "    v_texCoord = a_position;\n";
    src += "    vec4 pos = u_mvp * vec4(a_position, 0.0);\n";
    src += "    gl_Position = pos.xyww;  // depth = 1.0 (far plane)\n";
    src += "}\n";
    return src;
}

inline std::string buildSkyBoxFragmentShader()
{
    std::string src;
    src += "#version 410 core\n\n";
    src += "in vec3 v_texCoord;\n";
    src += "uniform samplerCube s_skybox;\n";
    src += "\nout vec4 fragColor;\n";
    src += "\nvoid main()\n{\n";
    src += "    fragColor = texture(s_skybox, v_texCoord);\n";
    src += "}\n";
    return src;
}

// ---------------------------------------------------------------------------
// SkySphere gradient shader
// (Ported from: itwinjs-core glsl/SkySphere.ts)
// ---------------------------------------------------------------------------
inline std::string buildSkySphereGradientVertexShader()
{
    std::string src;
    src += "#version 410 core\n\n";
    src += "layout(location = 0) in vec2 a_position;\n";
    src += "layout(location = 1) in vec2 a_texCoord;\n";
    src += "uniform mat4 u_mvp;\n";
    src += "uniform vec3 u_skyZenithColor;\n";
    src += "uniform vec3 u_skyColor;\n";
    src += "uniform vec3 u_skyGroundColor;\n";
    src += "uniform vec3 u_skyNadirColor;\n";
    src += "\nout vec2 v_texCoord;\n";
    src += "out vec3 v_skyColor;\n";
    src += "\nvoid main()\n{\n";
    src += "    gl_Position = vec4(a_position, 0.999, 1.0);\n";
    src += "    v_texCoord = a_texCoord;\n";
    src += "\n";
    src += "    // Compute sky gradient based on elevation angle\n";
    src += "    float elevation = a_texCoord.y;  // 0=bottom, 1=top\n";
    src += "    if (elevation > 0.5) {\n";
    src += "        float t = (elevation - 0.5) * 2.0;\n";
    src += "        v_skyColor = mix(u_skyColor, u_skyZenithColor, t);\n";
    src += "    } else {\n";
    src += "        float t = (0.5 - elevation) * 2.0;\n";
    src += "        v_skyColor = mix(u_skyColor, u_skyGroundColor, t);\n";
    src += "    }\n";
    src += "}\n";
    return src;
}

inline std::string buildSkySphereGradientFragmentShader()
{
    std::string src;
    src += "#version 410 core\n\n";
    src += "in vec2 v_texCoord;\n";
    src += "in vec3 v_skyColor;\n";
    src += "\nout vec4 fragColor;\n";
    src += "\nvoid main()\n{\n";
    src += "    fragColor = vec4(v_skyColor, 1.0);\n";
    src += "}\n";
    return src;
}

// ---------------------------------------------------------------------------
// SkySphere texture shader (equirectangular)
// (Ported from: itwinjs-core glsl/SkySphere.ts)
// ---------------------------------------------------------------------------
inline std::string buildSkySphereTextureVertexShader()
{
    std::string src;
    src += "#version 410 core\n\n";
    src += "layout(location = 0) in vec2 a_position;\n";
    src += "layout(location = 1) in vec2 a_texCoord;\n";
    src += "uniform mat4 u_mvp;\n";
    src += "\nout vec2 v_texCoord;\n";
    src += "\nvoid main()\n{\n";
    src += "    gl_Position = vec4(a_position, 0.999, 1.0);\n";
    src += "    v_texCoord = a_texCoord;\n";
    src += "}\n";
    return src;
}

inline std::string buildSkySphereTextureFragmentShader()
{
    std::string src;
    src += "#version 410 core\n\n";
    src += "in vec2 v_texCoord;\n";
    src += "uniform sampler2D s_skyTexture;\n";
    src += "\nout vec4 fragColor;\n";
    src += "\nvoid main()\n{\n";
    src += "    fragColor = texture(s_skyTexture, v_texCoord);\n";
    src += "}\n";
    return src;
}

// ---------------------------------------------------------------------------
// Screen space effect template
// (Ported from: itwinjs-core glsl/ScreenSpaceEffect.ts)
// ---------------------------------------------------------------------------
inline std::string buildScreenSpaceEffectVertexShader()
{
    // Reuses fullscreen quad vertex shader
    std::string src;
    src += "#version 410 core\n\n";
    src += "layout(location = 0) in vec2 a_position;\n";
    src += "layout(location = 1) in vec2 a_texCoord;\n";
    src += "\nout vec2 v_texCoord;\n";
    src += "\nvoid main()\n{\n";
    src += "    gl_Position = vec4(a_position, 0.0, 1.0);\n";
    src += "    v_texCoord = a_texCoord;\n";
    src += "}\n";
    return src;
}

inline std::string buildScreenSpaceEffectFragmentShader(std::string const& effectCode)
{
    std::string src;
    src += "#version 410 core\n\n";
    src += "in vec2 v_texCoord;\n";
    src += "uniform sampler2D s_inputTexture;\n";
    src += "\nout vec4 fragColor;\n";
    src += "\n";
    src += effectCode;  // User-defined effect code
    src += "\nvoid main()\n{\n";
    src += "    vec4 inputColor = texture(s_inputTexture, v_texCoord);\n";
    src += "    fragColor = applyEffect(inputColor, v_texCoord);\n";
    src += "}\n";
    return src;
}

END_DQ_RENDER_NAMESPACE

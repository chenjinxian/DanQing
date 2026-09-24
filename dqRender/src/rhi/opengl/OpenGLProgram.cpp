// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OpenGL program compilation and linking
// Ported from: filament backend/src/opengl/OpenGLProgram.cpp
#include "OpenGLProgram.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

OpenGLProgram::~OpenGLProgram()
{
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

bool OpenGLProgram::compile(char const* vertSource, char const* fragSource,
                            std::string const& programName,
                            std::vector<std::pair<std::string, uint8_t>> const& attributeLocations)
{
    // Compile vertex shader
    GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &vertSource, nullptr);
    // TEMP-DIAG（贴图 U 翻转 saga）：dump 含 a_texCoord 的顶点着色器源码。
    if (getenv("DANQING_SHADER_DUMP") && vertSource && strstr(vertSource, "a_texCoord")) {
        static int n = 0;
        char path[128];
        snprintf(path, sizeof(path), "build/vshader-%s-%d.glsl", programName.c_str(), n++);
        FILE* f = fopen(path, "wb");
        if (f) { fputs(vertSource, f); fclose(f); }
        printf("[SHDUMP] %s\n", path);
    }
    glCompileShader(vertShader);

    GLint status = 0;
    glGetShaderiv(vertShader, GL_COMPILE_STATUS, &status);
    if (!status) {
        GLint logLen = 0;
        glGetShaderiv(vertShader, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(logLen + 1);
        glGetShaderInfoLog(vertShader, logLen, nullptr, log.data());
        std::cerr << "Vertex shader compile error (" << programName << "):\n"
                  << log.data() << "\n";
        glDeleteShader(vertShader);
        return false;
    }

    // Compile fragment shader
    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &fragSource, nullptr);
    // TEMP-DIAG（U 翻转 saga）：dump 含 v_texCoord 的片段着色器源码。
    if (getenv("DANQING_SHADER_DUMP") && fragSource && strstr(fragSource, "v_texCoord")) {
        static int n = 0;
        char path[128];
        snprintf(path, sizeof(path), "build/fshader-%d-%s.glsl", n++, programName.c_str());
        FILE* f = fopen(path, "wb");
        if (f) { fputs(fragSource, f); fclose(f); }
        printf("[SHDUMP-F] fshader-%d.glsl\n", n - 1);
    }
    // TEMP-DIAG（拾取 saga）：dump 顶点着色器源码（Pick 变体的 v_featureId 赋值核对）。
    if (getenv("DANQING_SHADER_DUMP") && vertSource && strstr(vertSource, "v_featureId")) {
        static int m = 0;
        char vpath[128];
        snprintf(vpath, sizeof(vpath), "build/vshader-%d-%s.glsl", m++, programName.c_str());
        FILE* vf = fopen(vpath, "wb");
        if (vf) { fputs(vertSource, vf); fclose(vf); }
        printf("[SHDUMP-V] vshader-%d.glsl\n", m - 1);
    }
    glCompileShader(fragShader);

    glGetShaderiv(fragShader, GL_COMPILE_STATUS, &status);
    if (!status) {
        GLint logLen = 0;
        glGetShaderiv(fragShader, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(logLen + 1);
        glGetShaderInfoLog(fragShader, logLen, nullptr, log.data());
        std::cerr << "Fragment shader compile error (" << programName << "):\n"
                  << log.data() << "\n";
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
        return false;
    }

    // Link program
    m_program = glCreateProgram();
    glAttachShader(m_program, vertShader);
    glAttachShader(m_program, fragShader);

    // Bind explicit attribute locations BEFORE linking (glBindAttribLocation must
    // precede glLinkProgram to take effect). Without this, GL auto-assigns
    // locations and geometry VAO slots misalign with shader inputs.
    // Ported from: filament backend OpenGLProgram (explicit attrib binding).
#pragma warning(suppress : 4458) // 结构化绑定 name 遮蔽同名成员，仅 MSVC /W4 提示
    for (auto const& [name, loc] : attributeLocations)
        glBindAttribLocation(m_program, loc, name.c_str());

    glLinkProgram(m_program);

    glGetProgramiv(m_program, GL_LINK_STATUS, &status);
    if (!status) {
        GLint logLen = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(logLen + 1);
        glGetProgramInfoLog(m_program, logLen, nullptr, log.data());
        std::cerr << "Program link error (" << programName << "):\n"
                  << log.data() << "\n";
        glDeleteProgram(m_program);
        m_program = 0;
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
        return false;
    }

    // Clean up shaders (they're retained by the program)
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    name = programName;
    return true;
}

void OpenGLProgram::use() noexcept
{
    if (m_program) {
        glUseProgram(m_program);
    }
}

GLint OpenGLProgram::getUniformLocation(char const* uniformName) noexcept
{
    auto it = m_uniformCache.find(uniformName);
    if (it != m_uniformCache.end()) {
        return it->second;
    }
    GLint loc = glGetUniformLocation(m_program, uniformName);
    m_uniformCache[uniformName] = loc;
    return loc;
}

GLint OpenGLProgram::getAttribLocation(char const* attribName) noexcept
{
    auto it = m_attribCache.find(attribName);
    if (it != m_attribCache.end()) {
        return it->second;
    }
    GLint loc = glGetAttribLocation(m_program, attribName);
    m_attribCache[attribName] = loc;
    return loc;
}

}  // namespace rhi
END_DQ_RENDER_NAMESPACE

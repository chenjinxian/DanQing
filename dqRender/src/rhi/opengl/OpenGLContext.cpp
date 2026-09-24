// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OpenGL context initialization
// Ported from: filament backend/src/opengl/OpenGLContext.cpp
#include "OpenGLContext.h"

#include <cstring>

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

void OpenGLContext::initialize()
{
    // Query GL version
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &minorVersion);

    vendor = reinterpret_cast<char const*>(glGetString(GL_VENDOR));
    renderer = reinterpret_cast<char const*>(glGetString(GL_RENDERER));
    version = reinterpret_cast<char const*>(glGetString(GL_VERSION));
    shaderVersion = reinterpret_cast<char const*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    // Query GL limits
    glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gets.maxAnisotropy);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &gets.maxCombinedTextureImageUnits);
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &gets.maxDrawBuffers);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &gets.maxRenderbufferSize);
    glGetIntegerv(GL_MAX_SAMPLES, &gets.maxSamples);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &gets.maxTextureImageUnits);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gets.maxTextureSize);
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &gets.maxCubeMapTextureSize);
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &gets.max3DTextureSize);
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &gets.maxArrayTextureLayers);
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &gets.maxUniformBlockSize);
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &gets.maxUniformBufferBindings);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &gets.uniformBufferOffsetAlignment);

    // Check extensions
    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    for (GLint i = 0; i < numExtensions; ++i) {
#pragma warning(suppress : 4458) // 局部 ext 与 filament 参考同名（OpenGLContext 扩展探测），成员访问已 this-> 限定
        char const* ext = reinterpret_cast<char const*>(glGetStringi(GL_EXTENSIONS, i));
        if (!ext) continue;
        if (std::strcmp(ext, "GL_EXT_clip_control") == 0) this->ext.EXTClipControl = true;
        if (std::strcmp(ext, "GL_EXT_disjoint_timer_query") == 0) this->ext.EXTDisjointTimerQuery = true;
        if (std::strcmp(ext, "GL_KHR_parallel_shader_compile") == 0) this->ext.KHRParallelShaderCompile = true;
        if (std::strcmp(ext, "GL_EXT_texture_filter_anisotropic") == 0) this->ext.EXTTextureFilterAnisotropic = true;
        if (std::strcmp(ext, "GL_ARB_draw_buffers") == 0) this->ext.ARBDrawBuffers = true;
        if (std::strcmp(ext, "GL_ARB_texture_float") == 0) this->ext.ARBTextureFloat = true;
        if (std::strcmp(ext, "GL_ARB_half_float_vertex") == 0) this->ext.ARBHalfFloatVertex = true;
    }
}

}  // namespace rhi
END_DQ_RENDER_NAMESPACE

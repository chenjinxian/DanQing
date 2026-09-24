// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OpenGL context and capability detection
// Ported from: filament backend/src/opengl/OpenGLContext.h
//
// Stores all GL state queries and extension/bug detection results.
// Immutable after construction.
#pragma once

#include "Gl.h"
#include "dqRender/rhi/DriverEnums.h"

#include <array>
#include <bitset>
#include <cstdint>
#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// ---------------------------------------------------------------------------
// OpenGLContext — GL capabilities and extension queries
// ---------------------------------------------------------------------------
class OpenGLContext {
public:
    OpenGLContext() noexcept = default;
    void initialize();

    // --- GL version ---
    GLint majorVersion = 0;
    GLint minorVersion = 0;
    std::string vendor;
    std::string renderer;
    std::string version;
    std::string shaderVersion;

    // --- GL limits ---
    struct Gets {
        GLint maxAnisotropy = 1;
        GLint maxCombinedTextureImageUnits = 16;
        GLint maxDrawBuffers = 4;
        GLint maxRenderbufferSize = 4096;
        GLint maxSamples = 1;
        GLint maxTextureImageUnits = 16;
        GLint maxTextureSize = 2048;
        GLint maxCubeMapTextureSize = 2048;
        GLint max3DTextureSize = 256;
        GLint maxArrayTextureLayers = 256;
        GLint maxUniformBlockSize = 16384;
        GLint maxUniformBufferBindings = 36;
        GLint uniformBufferOffsetAlignment = 256;
    } gets;

    // --- Extensions ---
    struct Extensions {
        bool EXTClipControl = false;
        bool EXTDisjointTimerQuery = false;
        bool KHRParallelShaderCompile = false;
        bool EXTTextureFilterAnisotropic = false;
        bool ARBDrawBuffers = false;
        bool ARBTextureFloat = false;
        bool ARBHalfFloatVertex = false;
    } ext;

    // --- Bugs / Workarounds ---
    struct Bugs {
        bool disableGlFlush = false;
        bool vaoDoesntStoreElementArrayBufferBinding = false;
    } bugs;

    // --- RenderPrimitive (VAO state tracking) ---
    struct RenderPrimitive {
        GLuint vao = 0;
        GLuint elementArray = 0;
        GLenum indicesType = GL_UNSIGNED_SHORT;
        uint8_t indicesShift = 1;  // 1 for 16-bit, 2 for 32-bit
    };

    // --- State cache helpers ---
    bool isAtLeastGL(uint8_t major, uint8_t minor) const noexcept
    {
        return majorVersion > major || (majorVersion == major && minorVersion >= minor);
    }
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE

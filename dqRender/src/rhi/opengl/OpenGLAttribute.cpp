// SPDX-License-Identifier: Apache-2.0
// Ported from: filament backend/src/opengl/OpenGLDriver.cpp
//              (vertex-attribute ElementType → GL size/format wiring, extracted
//               for unit testability — see OpenGLDriver.cpp bindRenderPrimitive)
//
// The two switches below are transcribed VERBATIM from the current
// OpenGLDriver.cpp bindRenderPrimitive switches (size switch and type switch),
// then extended with UBYTE/UBYTE2/UBYTE3 cases that were previously missing
// (only UBYTE4 was wired). All other ElementType cases — including those that
// fall through to `default` (e.g. BYTE/SHORT/USHORT/INT/HALF/HALF3, and UINT
// in the type switch) — keep their CURRENT mapping unchanged.
#include "rhi/opengl/OpenGLAttribute.h"

#include "rhi/opengl/Gl.h"  // GL_UNSIGNED_BYTE / GL_FLOAT / GL_HALF_FLOAT / GL_SHORT

namespace dqRender::gl {

// Mirrors the size switch in OpenGLDriver.cpp bindRenderPrimitive
// (stride computation). Cases transcribed verbatim; UBYTE/UBYTE2/UBYTE3 added.
int elementSize(rhi::ElementType type) noexcept {
    uint32_t sz = 0;
    switch (type) {
        case rhi::ElementType::FLOAT:   sz = 4;  break;
        case rhi::ElementType::FLOAT2:  sz = 8;  break;
        case rhi::ElementType::FLOAT3:  sz = 12; break;
        case rhi::ElementType::FLOAT4:  sz = 16; break;
        case rhi::ElementType::HALF2:   sz = 4;  break;
        case rhi::ElementType::HALF4:   sz = 8;  break;
        case rhi::ElementType::UBYTE:   sz = 1;  break;  // ADDED (Polyline a_param)
        case rhi::ElementType::UBYTE2:  sz = 2;  break;  // ADDED
        case rhi::ElementType::UBYTE3:  sz = 3;  break;  // ADDED (Polyline a_pos / a_prevIndex / a_nextIndex)
        case rhi::ElementType::UBYTE4:  sz = 4;  break;
        case rhi::ElementType::SHORT2:  sz = 4;  break;
        case rhi::ElementType::SHORT4:  sz = 8;  break;
        case rhi::ElementType::UINT:    sz = 4;  break;  // uint32 featureId (PolyfaceGraphic Vertex)
        default:                        sz = 16; break;
    }
    return static_cast<int>(sz);
}

// Mirrors the type switch in OpenGLDriver.cpp bindRenderPrimitive
// (glVertexAttribPointer arguments). Cases transcribed verbatim;
// UBYTE/UBYTE2/UBYTE3 added. normalized stays GL_FALSE (handled by the caller
// — the driver hardcodes GL_FALSE at the call site, which is exactly the
// faithful value for these byte attrs).
void elementFormat(rhi::ElementType type, int& outGlType, int& outGlSize) noexcept {
    GLenum glType = GL_FLOAT;
    GLint  glSize = 4;
    switch (type) {
        case rhi::ElementType::FLOAT:   glType = GL_FLOAT;         glSize = 1; break;
        case rhi::ElementType::FLOAT2:  glType = GL_FLOAT;         glSize = 2; break;
        case rhi::ElementType::FLOAT3:  glType = GL_FLOAT;         glSize = 3; break;
        case rhi::ElementType::FLOAT4:  glType = GL_FLOAT;         glSize = 4; break;
        case rhi::ElementType::HALF2:   glType = GL_HALF_FLOAT;    glSize = 2; break;
        case rhi::ElementType::HALF4:   glType = GL_HALF_FLOAT;    glSize = 4; break;
        case rhi::ElementType::UBYTE:   glType = GL_UNSIGNED_BYTE; glSize = 1; break;  // ADDED (Polyline a_param)
        case rhi::ElementType::UBYTE2:  glType = GL_UNSIGNED_BYTE; glSize = 2; break;  // ADDED
        case rhi::ElementType::UBYTE3:  glType = GL_UNSIGNED_BYTE; glSize = 3; break;  // ADDED (Polyline a_pos / a_prevIndex / a_nextIndex)
        case rhi::ElementType::UBYTE4:  glType = GL_UNSIGNED_BYTE; glSize = 4; break;
        case rhi::ElementType::SHORT2:  glType = GL_SHORT;         glSize = 2; break;
        case rhi::ElementType::SHORT4:  glType = GL_SHORT;         glSize = 4; break;
        default:                        glType = GL_FLOAT;         glSize = 4; break;
    }
    outGlType = static_cast<int>(glType);
    outGlSize = static_cast<int>(glSize);
}

}  // namespace dqRender::gl

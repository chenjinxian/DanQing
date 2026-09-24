// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GL constant namespace
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/GL.ts
//
// Type-safe OpenGL constant mapping, matching itwinjs-core's GL namespace exactly.
// WebGL2 constants map 1:1 to OpenGL 4.1 Core constants (same numeric values).
#pragma once

// Reuse the same GL header as the RHI layer to avoid "gl.h and gl3.h are both included" warning.
#include "rhi/opengl/Gl.h"

namespace GL {

// --- BlendEquation ---
// Ported from: itwinjs-core GL.ts BlendEquation
enum class BlendEquation : GLenum {
    add = GL_FUNC_ADD,
    Subtract = GL_FUNC_SUBTRACT,
    ReverseSubtract = GL_FUNC_REVERSE_SUBTRACT,
    Default = add,
};

// --- BlendFactor ---
// Ported from: itwinjs-core GL.ts BlendFactor
enum class BlendFactor : GLenum {
    Zero = GL_ZERO,
    One = GL_ONE,
    SrcColor = GL_SRC_COLOR,
    OneMinusSrcColor = GL_ONE_MINUS_SRC_COLOR,
    DstColor = GL_DST_COLOR,
    OneMinusDstColor = GL_ONE_MINUS_DST_COLOR,
    SrcAlpha = GL_SRC_ALPHA,
    OneMinusSrcAlpha = GL_ONE_MINUS_SRC_ALPHA,
    DstAlpha = GL_DST_ALPHA,
    OneMinusDstAlpha = GL_ONE_MINUS_DST_ALPHA,
    ConstColor = GL_CONSTANT_COLOR,
    OneMinusConstColor = GL_ONE_MINUS_CONSTANT_COLOR,
    ConstAlpha = GL_CONSTANT_ALPHA,
    OneMinusConstAlpha = GL_ONE_MINUS_CONSTANT_ALPHA,
    AlphaSaturate = GL_SRC_ALPHA_SATURATE,
    DefaultSrc = One,
    DefaultDst = Zero,
};

// --- Buffer ---
// Ported from: itwinjs-core GL.ts Buffer
namespace Buffer {
    enum class Target : GLenum {
        ArrayBuffer = GL_ARRAY_BUFFER,
        ElementArrayBuffer = GL_ELEMENT_ARRAY_BUFFER,
    };

    enum class Binding : GLenum {
        ArrayBuffer = GL_ARRAY_BUFFER_BINDING,
        ElementArrayBuffer = GL_ELEMENT_ARRAY_BUFFER_BINDING,
    };

    enum class Parameter : GLenum {
        Size = GL_BUFFER_SIZE,
        Usage = GL_BUFFER_USAGE,
    };

    enum class Usage : GLenum {
        DynamicDraw = GL_DYNAMIC_DRAW,
        StaticDraw = GL_STATIC_DRAW,
        StreamDraw = GL_STREAM_DRAW,
    };
}  // namespace Buffer

// --- StencilOperation ---
// Ported from: itwinjs-core GL.ts StencilOperation
enum class StencilOperation : GLenum {
    Keep = GL_KEEP,
    Zero = GL_ZERO,
    Replace = GL_REPLACE,
    Incr = GL_INCR,
    IncrWrap = GL_INCR_WRAP,
    Decr = GL_DECR,
    DecrWrap = GL_DECR_WRAP,
    Invert = GL_INVERT,
    Default = Keep,
};

// --- StencilFunction ---
// Ported from: itwinjs-core GL.ts StencilFunction
enum class StencilFunction : GLenum {
    Never = GL_NEVER,
    Less = GL_LESS,
    LEqual = GL_LEQUAL,
    Greater = GL_GREATER,
    GEqual = GL_GEQUAL,
    Equal = GL_EQUAL,
    NotEqual = GL_NOTEQUAL,
    Always = GL_ALWAYS,
    Default = Always,
};

// --- CullFace ---
// Ported from: itwinjs-core GL.ts CullFace
enum class CullFace : GLenum {
    Front = GL_FRONT,
    Back = GL_BACK,
    FrontAndBack = GL_FRONT_AND_BACK,
    Default = Back,
};

// --- DataType ---
// Ported from: itwinjs-core GL.ts DataType
enum class DataType : GLenum {
    Byte = GL_BYTE,
    Short = GL_SHORT,
    UnsignedByte = GL_UNSIGNED_BYTE,
    UnsignedShort = GL_UNSIGNED_SHORT,
    UnsignedInt = GL_UNSIGNED_INT,
    Float = GL_FLOAT,
    HalfFloat = GL_HALF_FLOAT,
};

// --- FrontFace ---
// Ported from: itwinjs-core GL.ts FrontFace
enum class FrontFace : GLenum {
    CounterClockwise = GL_CCW,
    Clockwise = GL_CW,
    Default = CounterClockwise,
};

// --- DepthFunc ---
// Ported from: itwinjs-core GL.ts DepthFunc
enum class DepthFunc : GLenum {
    Never = GL_NEVER,
    Less = GL_LESS,
    Equal = GL_EQUAL,
    LessOrEqual = GL_LEQUAL,
    Greater = GL_GREATER,
    NotEqual = GL_NOTEQUAL,
    GreaterOrEqual = GL_GEQUAL,
    Always = GL_ALWAYS,
    Default = LessOrEqual,
};

// --- Capability ---
// Ported from: itwinjs-core GL.ts Capability
enum class Capability : GLenum {
    Blend = GL_BLEND,
    BlendColor = GL_BLEND_COLOR,
    BlendEquationAlpha = GL_BLEND_EQUATION_ALPHA,
    BlendEquationRGB = GL_BLEND_EQUATION_RGB,
    BlendSrcAlpha = GL_BLEND_SRC_ALPHA,
    BlendSrcRgb = GL_BLEND_SRC_RGB,
    BlendDstAlpha = GL_BLEND_DST_ALPHA,
    BlendDstRgb = GL_BLEND_DST_RGB,
    CullFace = GL_CULL_FACE,
    CullFaceMode = GL_CULL_FACE_MODE,
    DepthFunc = GL_DEPTH_FUNC,
    DepthTest = GL_DEPTH_TEST,
    DepthWriteMask = GL_DEPTH_WRITEMASK,
    FrontFace = GL_FRONT_FACE,
    StencilFrontFunc = GL_STENCIL_FUNC,
    StencilFrontRef = GL_STENCIL_REF,
    StencilFrontValueMask = GL_STENCIL_VALUE_MASK,
    StencilFrontWriteMask = GL_STENCIL_WRITEMASK,
    StencilFrontOpFail = GL_STENCIL_FAIL,
    StencilFrontOpZFail = GL_STENCIL_PASS_DEPTH_FAIL,
    StencilFrontOpZPass = GL_STENCIL_PASS_DEPTH_PASS,
    StencilBackFunc = GL_STENCIL_BACK_FUNC,
    StencilBackRef = GL_STENCIL_BACK_REF,
    StencilBackValueMask = GL_STENCIL_BACK_VALUE_MASK,
    StencilBackWriteMask = GL_STENCIL_BACK_WRITEMASK,
    StencilBackOpFail = GL_STENCIL_BACK_FAIL,
    StencilBackOpZFail = GL_STENCIL_BACK_PASS_DEPTH_FAIL,
    StencilBackOpZPass = GL_STENCIL_BACK_PASS_DEPTH_PASS,
    StencilTest = GL_STENCIL_TEST,
    StencilWriteMask = GL_STENCIL_WRITEMASK,
};

// --- Texture ---
// Ported from: itwinjs-core GL.ts Texture
namespace Texture {
    enum class Target : GLenum {
        TwoDee = GL_TEXTURE_2D,
        CubeMap = GL_TEXTURE_CUBE_MAP,
        CubeMapPositiveX = GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        CubeMapNegativeX = GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        CubeMapPositiveY = GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        CubeMapNegativeY = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        CubeMapPositiveZ = GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        CubeMapNegativeZ = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
    };

    // GL_LUMINANCE is removed in OpenGL 3 core profile; define manually.
    static constexpr GLenum GL_LUMINANCE_COMPAT = 0x1909;

    enum class Format : GLenum {
        Rgb = GL_RGB,
        Rgba = GL_RGBA,
        DepthStencil = GL_DEPTH_STENCIL,
        Luminance = GL_LUMINANCE_COMPAT,
        DepthComponent = GL_DEPTH_COMPONENT,
        RedInteger = GL_RED_INTEGER,
    };

    enum class DataType : GLenum {
        Float = GL_FLOAT,
        UnsignedByte = GL_UNSIGNED_BYTE,
        UnsignedInt = GL_UNSIGNED_INT,
    };

    enum class WrapMode : GLenum {
        Repeat = GL_REPEAT,
        MirroredRepeat = GL_MIRRORED_REPEAT,
        ClampToEdge = GL_CLAMP_TO_EDGE,
    };
}  // namespace Texture

// --- ShaderType ---
// Ported from: itwinjs-core GL.ts ShaderType
enum class ShaderType : GLenum {
    Fragment = GL_FRAGMENT_SHADER,
    Vertex = GL_VERTEX_SHADER,
};

// --- ShaderParameter ---
// Ported from: itwinjs-core GL.ts ShaderParameter
enum class ShaderParameter : GLenum {
    CompileStatus = GL_COMPILE_STATUS,
};

// --- ProgramParameter ---
// Ported from: itwinjs-core GL.ts ProgramParameter
enum class ProgramParameter : GLenum {
    LinkStatus = GL_LINK_STATUS,
    ActiveUniforms = GL_ACTIVE_UNIFORMS,
};

// --- PrimitiveType ---
// Ported from: itwinjs-core GL.ts PrimitiveType
enum class PrimitiveType : GLenum {
    Points = GL_POINTS,
    Lines = GL_LINES,
    Triangles = GL_TRIANGLES,
};

// --- RenderBuffer constants ---
// Ported from: itwinjs-core GL.ts RenderBuffer
// Renamed to RenderBufferTarget to avoid conflict with RenderBuffer class.
namespace RenderBufferTarget {
    constexpr GLenum TARGET = GL_RENDERBUFFER;

    enum class Format : GLenum {
        DepthComponent16 = GL_DEPTH_COMPONENT16,
    };
}  // namespace RenderBufferTarget

// --- FrameBuffer constants ---
// Ported from: itwinjs-core GL.ts FrameBuffer
// Renamed to FrameBufferTarget to avoid conflict with FrameBuffer class.
namespace FrameBufferTarget {
    constexpr GLenum TARGET = GL_FRAMEBUFFER;

    enum class Status : GLenum {
        Complete = GL_FRAMEBUFFER_COMPLETE,
        IncompleteAttachment = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
        IncompleteMissingAttachment = GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT,
    };
}  // namespace FrameBufferTarget

// --- BufferBit ---
// Ported from: itwinjs-core GL.ts BufferBit
enum class BufferBit : GLbitfield {
    Color = GL_COLOR_BUFFER_BIT,
    Depth = GL_DEPTH_BUFFER_BIT,
    Stencil = GL_STENCIL_BUFFER_BIT,
};

// --- MultiSampling ---
// Ported from: itwinjs-core GL.ts MultiSampling
namespace MultiSampling {
    enum class Filter : GLenum {
        Nearest = GL_NEAREST,
        Linear = GL_LINEAR,
    };
}  // namespace MultiSampling

// --- Top-level constants ---
// Ported from: itwinjs-core GL.ts POLYGON_OFFSET_FILL
constexpr GLenum POLYGON_OFFSET_FILL = GL_POLYGON_OFFSET_FILL;

// --- Extension constants ---
// GL_QUERY_RESULT and GL_EXTENSIONS are defined by platform gl3.h.
// GL_TIME_ELAPSED_EXT may not be defined on all platforms; polyfill.
#ifndef GL_TIME_ELAPSED_EXT
#define GL_TIME_ELAPSED_EXT 0x8BF5
#endif

}  // namespace GL

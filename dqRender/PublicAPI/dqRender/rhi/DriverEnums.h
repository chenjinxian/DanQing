// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RHI enumerations and value types
// Ported from: filament backend/include/backend/DriverEnums.h
//
// All enumerations for the RHI layer.  Names and values match Filament exactly;
// only the namespace is changed from filament::backend to dqRender::rhi.
#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr uint8_t MAX_VERTEX_ATTRIBUTE_COUNT = 16;
static constexpr uint8_t MAX_SAMPLER_COUNT = 62;
static constexpr uint8_t MAX_VERTEX_BUFFER_COUNT = 16;
static constexpr uint8_t MAX_SSBO_COUNT = 4;
static constexpr uint8_t MAX_DESCRIPTOR_SET_COUNT = 4;
static constexpr uint8_t MAX_DESCRIPTOR_COUNT = 64;
static constexpr uint8_t MAX_PUSH_CONSTANT_COUNT = 32;
static constexpr uint8_t MAX_COLOR_ATTACHMENT_COUNT = 8;

// ---------------------------------------------------------------------------
// Shader
// ---------------------------------------------------------------------------
enum class ShaderStage : uint8_t {
    VERTEX = 0,
    FRAGMENT = 1,
    COMPUTE = 2,
};

enum class ShaderStageFlags : uint8_t {
    NONE = 0x0,
    VERTEX = 0x1,
    FRAGMENT = 0x2,
    COMPUTE = 0x4,
    ALL = VERTEX | FRAGMENT | COMPUTE,
};

inline ShaderStageFlags operator|(ShaderStageFlags a, ShaderStageFlags b) noexcept
{
    return static_cast<ShaderStageFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

enum class ShaderLanguage : uint8_t {
    ESSL1,
    ESSL3,
    SPIRV,
    MSL,
    METAL_LIBRARY,
    WGSL,
};

// ---------------------------------------------------------------------------
// Element / Vertex types
// ---------------------------------------------------------------------------
enum class ElementType : uint8_t {
    BYTE,
    BYTE2,
    BYTE3,
    BYTE4,
    UBYTE,
    UBYTE2,
    UBYTE3,
    UBYTE4,
    SHORT,
    SHORT2,
    SHORT3,
    SHORT4,
    USHORT,
    USHORT2,
    USHORT3,
    USHORT4,
    INT,
    UINT,
    FLOAT,
    FLOAT2,
    FLOAT3,
    FLOAT4,
    HALF,
    HALF2,
    HALF3,
    HALF4,
};

// ---------------------------------------------------------------------------
// Primitive type
// ---------------------------------------------------------------------------
enum class PrimitiveType : uint8_t {
    POINTS,
    LINES,
    LINE_STRIP,
    TRIANGLES,
    TRIANGLE_STRIP,
};

// ---------------------------------------------------------------------------
// Buffer usage
// ---------------------------------------------------------------------------
enum class BufferUsage : uint8_t {
    STATIC = 0,
    DYNAMIC = 1,
};

enum class BufferObjectBinding : uint8_t {
    VERTEX,
    UNIFORM,
    SHADER_STORAGE,
};

// ---------------------------------------------------------------------------
// Texture
// ---------------------------------------------------------------------------
enum class SamplerType : uint8_t {
    SAMPLER_2D,
    SAMPLER_2D_ARRAY,
    SAMPLER_CUBEMAP,
    SAMPLER_EXTERNAL,
    SAMPLER_3D,
    SAMPLER_CUBEMAP_ARRAY,
};

enum class TextureFormat : uint16_t {
    // Values match Filament backend/DriverEnums.h exactly.

    // 8-bits per element
    R8, R8_SNORM, R8UI, R8I, STENCIL8, // [0 - 4]

    // 16-bits per element
    R16F, R16UI, R16I, // [5 - 7]
    RG8, RG8_SNORM, RG8UI, RG8I, // [8 - 11]
    RGB565, // [12]
    RGB9_E5, // 9995 is actually 32 bpp but it's here for historical reasons. [13]
    RGB5_A1, // [14]
    RGBA4, // [15]
    DEPTH16, // [16]

    // 24-bits per element
    RGB8, SRGB8, RGB8_SNORM, RGB8UI, RGB8I, // [17 - 21]
    DEPTH24, // [22]

    // 32-bits per element
    R32F, R32UI, R32I, // [23 - 25]
    RG16F, RG16UI, RG16I, // [26 - 28]
    R11F_G11F_B10F, // [29]
    RGBA8, SRGB8_A8, RGBA8_SNORM, // [30 - 32]
    UNUSED, // used to be rgbm [33]
    RGB10_A2, RGBA8UI, RGBA8I, // [34 - 36]
    DEPTH32F, DEPTH24_STENCIL8, DEPTH32F_STENCIL8, // [37 - 39]

    // 48-bits per element
    RGB16F, RGB16UI, RGB16I, // [40 - 42]

    // 64-bits per element
    RG32F, RG32UI, RG32I, // [43 - 45]
    RGBA16F, RGBA16UI, RGBA16I, // [46 - 48]

    // 96-bits per element
    RGB32F, RGB32UI, RGB32I, // [49 - 51]

    // 128-bits per element
    RGBA32F, RGBA32UI, RGBA32I, // [52 - 54]

    // BGRA8 — not in Filament, added for Metal/Vulkan compatibility
    BGRA8 = 55,

    // Compressed formats (Phase 0: omitted for brevity)
    // Full list: see Filament TextureFormat [55 - 100+]
};

enum class TextureUsage : uint16_t {
    NONE = 0x0,
    COLOR_ATTACHMENT = 0x1,
    DEPTH_ATTACHMENT = 0x2,
    STENCIL_ATTACHMENT = 0x4,
    UPLOADABLE = 0x8,
    SAMPLEABLE = 0x10,
    SUBPASS_INPUT = 0x20,
    BLIT_SRC = 0x40,
    BLIT_DST = 0x80,
    PROTECTED = 0x100,
    GEN_MIPMAPPABLE = 0x200,
    DEFAULT = UPLOADABLE | SAMPLEABLE,
};

inline TextureUsage operator|(TextureUsage a, TextureUsage b) noexcept
{
    return static_cast<TextureUsage>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline TextureUsage operator&(TextureUsage a, TextureUsage b) noexcept
{
    return static_cast<TextureUsage>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

// ---------------------------------------------------------------------------
// Sampler parameters
// ---------------------------------------------------------------------------
enum class SamplerMagFilter : uint8_t {
    NEAREST,
    LINEAR,
};

enum class SamplerMinFilter : uint8_t {
    NEAREST,
    LINEAR,
    NEAREST_MIPMAP_NEAREST,
    LINEAR_MIPMAP_NEAREST,
    NEAREST_MIPMAP_LINEAR,
    LINEAR_MIPMAP_LINEAR,
};

enum class SamplerWrapMode : uint8_t {
    CLAMP_TO_EDGE,   // 0 — Filament: clamp-to-edge
    REPEAT,          // 1 — Filament: repeat
    MIRRORED_REPEAT, // 2 — Filament: mirrored-repeat
};

enum class SamplerCompareMode : uint8_t {
    NONE,
    COMPARE_TO_TEXTURE,
};

enum class SamplerCompareFunc : uint8_t {
    LEQUAL,
    GEQUAL,
    LESS,
    GREATER,
    EQUAL,
    NOTEQUAL,
    ALWAYS,
    NEVER,
};

struct SamplerParams {
    SamplerMagFilter filterMag : 1;
    SamplerMinFilter filterMin : 3;
    SamplerWrapMode wrapS : 2;
    SamplerWrapMode wrapT : 2;
    SamplerWrapMode wrapR : 2;
    uint8_t anisotropyLog2 : 3;
    SamplerCompareMode compareMode : 1;
    SamplerCompareFunc compareFunc : 3;

    bool operator==(SamplerParams const& rhs) const noexcept
    {
        return memcmp(this, &rhs, sizeof(SamplerParams)) == 0;
    }
};

// Size depends on compiler bit-field packing; don't assert exact size.

// ---------------------------------------------------------------------------
// Blend
// ---------------------------------------------------------------------------
enum class BlendEquation : uint8_t {
    ADD,
    SUBTRACT,
    REVERSE_SUBTRACT,
    MIN,
    MAX,
};

enum class BlendFunction : uint8_t {
    ZERO,
    ONE,
    SRC_COLOR,
    ONE_MINUS_SRC_COLOR,
    DST_COLOR,
    ONE_MINUS_DST_COLOR,
    SRC_ALPHA,
    ONE_MINUS_SRC_ALPHA,
    DST_ALPHA,
    ONE_MINUS_DST_ALPHA,
    SRC_ALPHA_SATURATE,
};

// ---------------------------------------------------------------------------
// Culling
// ---------------------------------------------------------------------------
enum class CullingMode : uint8_t {
    NONE,
    FRONT,
    BACK,
    FRONT_AND_BACK,
};

// ---------------------------------------------------------------------------
// Depth
// ---------------------------------------------------------------------------
// DepthFunc uses the same values as SamplerCompareFunc (Filament convention).
using DepthFunc = SamplerCompareFunc;

// ---------------------------------------------------------------------------
// Stencil
// ---------------------------------------------------------------------------
enum class StencilOperation : uint8_t {
    KEEP,
    ZERO,
    REPLACE,
    INCR,
    INCR_WRAP,
    DECR,
    DECR_WRAP,
    INVERT,
};

enum class StencilFunction : uint8_t {
    NEVER,
    LESS,
    LEQUAL,
    GREATER,
    GEQUAL,
    EQUAL,
    NOTEQUAL,
    ALWAYS,
};

// ---------------------------------------------------------------------------
// RasterState — bit-packed into 4 bytes
// ---------------------------------------------------------------------------
struct RasterState {
    CullingMode culling : 2;
    BlendEquation blendEquationRGB : 3;
    BlendEquation blendEquationAlpha : 3;
    BlendFunction blendFunctionSrcRGB : 4;
    BlendFunction blendFunctionSrcAlpha : 4;
    BlendFunction blendFunctionDstRGB : 4;
    BlendFunction blendFunctionDstAlpha : 4;
    bool depthWrite : 1;
    DepthFunc depthFunc : 3;
    bool colorWrite : 1;
    bool alphaToCoverage : 1;
    bool inverseFrontFaces : 1;
    bool depthClamp : 1;

    RasterState() noexcept
        : culling(CullingMode::BACK)
        , blendEquationRGB(BlendEquation::ADD)
        , blendEquationAlpha(BlendEquation::ADD)
        , blendFunctionSrcRGB(BlendFunction::ONE)
        , blendFunctionSrcAlpha(BlendFunction::ONE)
        , blendFunctionDstRGB(BlendFunction::ZERO)
        , blendFunctionDstAlpha(BlendFunction::ZERO)
        , depthWrite(true)
        , depthFunc(DepthFunc::LEQUAL)
        , colorWrite(true)
        , alphaToCoverage(false)
        , inverseFrontFaces(false)
        , depthClamp(false)
    {}
};

static_assert(sizeof(RasterState) == 4, "RasterState must be 4 bytes");

// ---------------------------------------------------------------------------
// StencilState
// ---------------------------------------------------------------------------
struct StencilState {
    struct StencilOps {
        StencilFunction function : 3;
        StencilOperation stencilFail : 3;
        StencilOperation depthFail : 3;
        StencilOperation stencilDepthPass : 3;
    };

    StencilOps front;
    StencilOps back;
    uint8_t readMask;
    uint8_t writeMask;
    uint8_t ref;
    uint8_t padding;

    StencilState() noexcept
        : front{StencilFunction::ALWAYS, StencilOperation::KEEP,
                StencilOperation::KEEP, StencilOperation::KEEP}
        , back{StencilFunction::ALWAYS, StencilOperation::KEEP,
               StencilOperation::KEEP, StencilOperation::KEEP}
        , readMask(0xff)
        , writeMask(0xff)
        , ref(0)
        , padding(0)
    {}
};

// Size depends on compiler bit-field packing; don't assert exact size.

// ---------------------------------------------------------------------------
// PolygonOffset
// ---------------------------------------------------------------------------
struct PolygonOffset {
    float slope = 0.0f;
    float constant = 0.0f;
};

// ---------------------------------------------------------------------------
// Viewport
// ---------------------------------------------------------------------------
struct Viewport {
    int32_t left;
    int32_t bottom;
    uint32_t width;
    uint32_t height;
};

// ---------------------------------------------------------------------------
// DepthRange
// ---------------------------------------------------------------------------
struct DepthRange {
    float near = 0.0f;
    float far = 1.0f;
};

// ---------------------------------------------------------------------------
// Clear values
// ---------------------------------------------------------------------------
union ClearColorValue {
    float f[4];
    int32_t i[4];
    uint32_t u[4];

    ClearColorValue() noexcept { f[0] = f[1] = f[2] = f[3] = 0.0f; }
};

// ---------------------------------------------------------------------------
// RenderPass parameters
// ---------------------------------------------------------------------------
enum class TargetBufferFlags : uint32_t {
    NONE = 0x0u,
    COLOR0 = 0x00000001u,
    COLOR1 = 0x00000002u,
    COLOR2 = 0x00000004u,
    COLOR3 = 0x00000008u,
    COLOR4 = 0x00000010u,
    COLOR5 = 0x00000020u,
    COLOR6 = 0x00000040u,
    COLOR7 = 0x00000080u,
    COLOR = COLOR0,
    COLOR_ALL = COLOR0 | COLOR1 | COLOR2 | COLOR3 | COLOR4 | COLOR5 | COLOR6 | COLOR7,
    DEPTH   = 0x10000000u,
    STENCIL = 0x20000000u,
    DEPTH_AND_STENCIL = DEPTH | STENCIL,
    ALL = COLOR_ALL | DEPTH | STENCIL,
};

inline TargetBufferFlags operator|(TargetBufferFlags a, TargetBufferFlags b) noexcept
{
    return static_cast<TargetBufferFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline TargetBufferFlags operator&(TargetBufferFlags a, TargetBufferFlags b) noexcept
{
    return static_cast<TargetBufferFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

struct RenderPassFlags {
    TargetBufferFlags clear = TargetBufferFlags::NONE;
    TargetBufferFlags discardStart = TargetBufferFlags::NONE;
    TargetBufferFlags discardEnd = TargetBufferFlags::NONE;
};

struct RenderPassParams {
    RenderPassFlags flags;
    Viewport viewport = {0, 0, 0, 0};
    DepthRange depthRange = {0.0f, 1.0f};
    ClearColorValue clearColor;
    double clearDepth = 1.0;
    uint32_t clearStencil = 0;
    uint16_t subpassMask = 0;
    uint16_t readOnlyDepthStencil = 0;

    // Per-attachment clear colors (for OIT MRT)
    // When perAttachmentClearCount > 0, each attachment is cleared with its own color
    uint8_t perAttachmentClearCount = 0;
    std::array<ClearColorValue, MAX_COLOR_ATTACHMENT_COUNT> perAttachmentClearColors;
};

// ---------------------------------------------------------------------------
// Attribute (for VertexBufferInfo)
// ---------------------------------------------------------------------------
enum class Attribute : uint8_t {
    POSITION,
    TANGENT,
    COLOR,
    COLOR_1,
    UV0,
    UV1,
    BONE_INDICES,
    BONE_WEIGHTS,
    CUSTOM0,
    CUSTOM1,
    CUSTOM2,
    CUSTOM3,
    CUSTOM4,
    CUSTOM5,
    CUSTOM6,
    CUSTOM7,
};

struct AttributeDesc {
    uint8_t buffer = 0;
    uint8_t offset = 0;
    ElementType type = ElementType::HALF4;
    uint8_t flags = 0;  // 0 = normalized
};

using AttributeArray = AttributeDesc[MAX_VERTEX_ATTRIBUTE_COUNT];

// ---------------------------------------------------------------------------
// Descriptor types
// ---------------------------------------------------------------------------
using descriptor_set_t = uint8_t;
using descriptor_binding_t = uint8_t;

enum class DescriptorType : uint8_t {
    SAMPLER_2D_FLOAT,
    SAMPLER_2D_INT,
    SAMPLER_2D_UINT,
    SAMPLER_2D_DEPTH,
    SAMPLER_2D_ARRAY_FLOAT,
    SAMPLER_2D_ARRAY_INT,
    SAMPLER_2D_ARRAY_UINT,
    SAMPLER_2D_ARRAY_DEPTH,
    SAMPLER_CUBE_FLOAT,
    SAMPLER_CUBE_INT,
    SAMPLER_CUBE_UINT,
    SAMPLER_CUBE_DEPTH,
    SAMPLER_CUBE_ARRAY_FLOAT,
    SAMPLER_CUBE_ARRAY_INT,
    SAMPLER_CUBE_ARRAY_UINT,
    SAMPLER_CUBE_ARRAY_DEPTH,
    SAMPLER_3D_FLOAT,
    SAMPLER_3D_INT,
    SAMPLER_3D_UINT,
    UNIFORM_BUFFER,
    SHADER_STORAGE_BUFFER,
    INPUT_ATTACHMENT,
};

// ---------------------------------------------------------------------------
// Fence
// ---------------------------------------------------------------------------
enum class FenceStatus : uint8_t {
    ERROR,
    CONDITION_SATISFIED,
    TIMEOUT_EXPIRED,
};

// ---------------------------------------------------------------------------
// Timer query
// ---------------------------------------------------------------------------
enum class TimerQueryResult : uint8_t {
    ERROR,
    NOT_READY,
    AVAILABLE,
};

// ---------------------------------------------------------------------------
// Workaround flags
// ---------------------------------------------------------------------------
enum class Workaround : uint8_t {
    SPLIT_EASU,
    ALLOW_READ_ONLY_ANCILLARY_FEEDBACK_LOOP,
    ADRENO_UNIFORM_ARRAY_CRASH,
    METAL_STATIC_TEXTURE_TARGET_ERROR,
    DISABLE_BLIT_INTO_TEXTURE_ARRAY,
    POWER_VR_SHADER_WORKAROUNDS,
    DISABLE_DEPTH_PRECACHE_FOR_DEFAULT_MATERIAL,
    EMULATE_SRGB_SWAPCHAIN,
};

// ---------------------------------------------------------------------------
// MapBufferAccess
// ---------------------------------------------------------------------------
enum class MapBufferAccess : uint8_t {
    READ = 0x1,
    WRITE = 0x2,
    READ_WRITE = READ | WRITE,
};

using MapBufferAccessFlags = uint8_t;

// ---------------------------------------------------------------------------
// Shader model
// ---------------------------------------------------------------------------
enum class ShaderModel : uint8_t {
    UNKNOWN = 0,
    GL_ES_20,   // OpenGL ES 2.0 (WebGL 1)
    GL_ES_30,   // OpenGL ES 3.0 (WebGL 2)
    GL_CORE_33, // OpenGL 3.3 (desktop minimum)
    GL_CORE_41, // OpenGL 4.1 (macOS max)
    GL_CORE_45, // OpenGL 4.5
    GL_CORE_46, // OpenGL 4.6
    VULKAN_10,
    METAL_20,
};

// ---------------------------------------------------------------------------
// Texture swizzle
// ---------------------------------------------------------------------------
enum class TextureSwizzle : uint8_t {
    SUBSTITUTE_ZERO,
    SUBSTITUTE_ONE,
    R,
    G,
    B,
    A,
};

// ---------------------------------------------------------------------------
// Feature level
// ---------------------------------------------------------------------------
enum class FeatureLevel : uint8_t {
    FEATURE_LEVEL_0,  // Basic features
    FEATURE_LEVEL_1,  // + Instancing, texture arrays
    FEATURE_LEVEL_2,  // + Compute, SSBO
    FEATURE_LEVEL_3,  // + Mesh shaders, RT
};

// ---------------------------------------------------------------------------
// Descriptor set layout
// ---------------------------------------------------------------------------
struct DescriptorSetLayoutBinding {
    descriptor_binding_t binding = 0;
    DescriptorType type = DescriptorType::UNIFORM_BUFFER;
    ShaderStageFlags stageFlags = ShaderStageFlags::ALL;
    uint16_t count = 1;
};

struct DescriptorSetLayout {
    static constexpr uint8_t MAX_BINDINGS = 16;
    uint8_t bindingCount = 0;
    DescriptorSetLayoutBinding bindings[MAX_BINDINGS] = {};
};

// ---------------------------------------------------------------------------
// Sampler params (extended)
// ---------------------------------------------------------------------------
// SamplerParams already defined above (line 242); no changes needed.

// ---------------------------------------------------------------------------
// Texture type
// ---------------------------------------------------------------------------
enum class TextureType : uint8_t {
    INVALID,
    TEXTURE_2D,
    TEXTURE_2D_ARRAY,
    CUBEMAP,
    TEXTURE_3D,
    CUBEMAP_ARRAY,
};

// ---------------------------------------------------------------------------
// Pixel data format / type
// ---------------------------------------------------------------------------
enum class PixelDataFormat : uint8_t {
    R,
    RG,
    RGB,
    RGBA,
    DEPTH_COMPONENT,
    DEPTH_STENCIL,
    STENCIL_INDEX,
};

enum class PixelDataType : uint8_t {
    UBYTE,
    BYTE,
    USHORT,
    SHORT,
    UINT,
    INT,
    FLOAT,
    UBYTE_3_3_2,
    USHORT_5_6_5,
    UBYTE_4_4_4_4,
    USHORT_4_4_4_4,
    USHORT_5_5_5_1,
    UINT_2_10_10_10_REV,
    UINT_10F_11F_11F_REV,
    UINT_5_9_9_9_REV,
};

// ---------------------------------------------------------------------------
// Backend
// ---------------------------------------------------------------------------
enum class Backend : uint8_t {
    DEFAULT,
    OPENGL,
    VULKAN,
    METAL,
    WEBGPU,
    NOOP,
};

// ---------------------------------------------------------------------------
// Compiler priority queue
// ---------------------------------------------------------------------------
enum class CompilerPriorityQueue : uint8_t {
    HIGH,
    LOW,
};

// ---------------------------------------------------------------------------
// Stencil face
// ---------------------------------------------------------------------------
enum class StencilFace : uint8_t {
    FRONT,
    BACK,
    FRONT_AND_BACK,
};

// ---------------------------------------------------------------------------
// Stream type
// ---------------------------------------------------------------------------
enum class StreamType : uint8_t {
    NATIVE,
    ACQUIRED,
};

// ---------------------------------------------------------------------------
// Sampler format
// ---------------------------------------------------------------------------
enum class SamplerFormat : uint8_t {
    INT,
    UINT,
    FLOAT,
    SHADOW,
};

// ---------------------------------------------------------------------------
// Pipeline / binding constants
// ---------------------------------------------------------------------------
static constexpr uint8_t PIPELINE_STAGE_COUNT = 2;        // vertex + fragment
static constexpr uint8_t CONFIG_UNIFORM_BINDING_COUNT = 4;
static constexpr uint8_t CONFIG_SAMPLER_BINDING_COUNT = 16;
static constexpr uint32_t FENCE_WAIT_FOR_EVER = 0xFFFFFFFFu;

// ---------------------------------------------------------------------------
// Texture format classification helpers
// ---------------------------------------------------------------------------
constexpr bool isDepthFormat(TextureFormat format) noexcept
{
    switch (format) {
        case TextureFormat::DEPTH16:
        case TextureFormat::DEPTH24:
        case TextureFormat::DEPTH32F:
        case TextureFormat::DEPTH24_STENCIL8:
        case TextureFormat::DEPTH32F_STENCIL8:
            return true;
        default:
            return false;
    }
}

constexpr bool isStencilFormat(TextureFormat format) noexcept
{
    switch (format) {
        case TextureFormat::STENCIL8:
        case TextureFormat::DEPTH24_STENCIL8:
        case TextureFormat::DEPTH32F_STENCIL8:
            return true;
        default:
            return false;
    }
}

constexpr bool isColorFormat(TextureFormat format) noexcept
{
    return !isDepthFormat(format) && !isStencilFormat(format);
}

constexpr bool isUnsignedIntFormat(TextureFormat format) noexcept
{
    switch (format) {
        case TextureFormat::R8UI:
        case TextureFormat::RG8UI:
        case TextureFormat::RGB8UI:
        case TextureFormat::RGBA8UI:
        case TextureFormat::R16UI:
        case TextureFormat::RG16UI:
        case TextureFormat::RGB16UI:
        case TextureFormat::RGBA16UI:
        case TextureFormat::R32UI:
        case TextureFormat::RG32UI:
        case TextureFormat::RGB32UI:
        case TextureFormat::RGBA32UI:
            return true;
        default:
            return false;
    }
}

constexpr bool isSignedIntFormat(TextureFormat format) noexcept
{
    switch (format) {
        case TextureFormat::R8I:
        case TextureFormat::RG8I:
        case TextureFormat::RGB8I:
        case TextureFormat::RGBA8I:
        case TextureFormat::R16I:
        case TextureFormat::RG16I:
        case TextureFormat::RGB16I:
        case TextureFormat::RGBA16I:
        case TextureFormat::R32I:
        case TextureFormat::RG32I:
        case TextureFormat::RGB32I:
        case TextureFormat::RGBA32I:
            return true;
        default:
            return false;
    }
}

constexpr bool isFp32ColorFormat(TextureFormat format) noexcept
{
    switch (format) {
        case TextureFormat::R32F:
        case TextureFormat::RG32F:
        case TextureFormat::RGB32F:
        case TextureFormat::RGBA32F:
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// Frame timestamps
// ---------------------------------------------------------------------------
struct FrameTimestamps {
    int64_t desiredPresentTime = 0;
    int64_t actualPresentTime = 0;
    int64_t renderCompleteTime = 0;
    int64_t displayCompleteTime = 0;
};

struct CompositorTiming {
    int64_t vsyncPeriodNanos = 0;
    int64_t desiredPresentTimeNanos = 0;
    int64_t presentMarginNanos = 0;
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE

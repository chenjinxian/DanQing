// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Hardware resource structs
// Ported from: filament backend/src/DriverBase.h
//
// All GPU resource structs inherit from HwBase.  The Driver allocates these
// via HandleAllocator and resolves them from typed Handles.
#pragma once

#include "dqRender/rhi/DriverEnums.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>
#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// ---------------------------------------------------------------------------
// HwBase — base for all GPU resource structs
// ---------------------------------------------------------------------------
struct HwBase {
    HwBase() noexcept = default;
    ~HwBase() noexcept = default;
    HwBase(HwBase const&) = delete;
    HwBase& operator=(HwBase const&) = delete;
};

// ---------------------------------------------------------------------------
// HwVertexBufferInfo — vertex buffer layout descriptor
// ---------------------------------------------------------------------------
struct HwVertexBufferInfo : public HwBase {
    uint8_t bufferCount = 0;
    uint8_t attributeCount = 0;
    AttributeArray attributes = {};
};

// ---------------------------------------------------------------------------
// HwVertexBuffer — vertex buffer (one or more buffer objects)
// ---------------------------------------------------------------------------
struct HwVertexBuffer : public HwBase {
    uint32_t vertexCount = 0;
    Handle<HwVertexBufferInfo> vbih;
};

// ---------------------------------------------------------------------------
// HwBufferObject — generic GPU buffer (vertex data, uniform, SSBO)
// ---------------------------------------------------------------------------
struct HwBufferObject : public HwBase {
    uint32_t byteCount = 0;
    BufferObjectBinding bindingType = BufferObjectBinding::VERTEX;
    BufferUsage usage = BufferUsage::STATIC;
};

// ---------------------------------------------------------------------------
// HwIndexBuffer — index buffer
// ---------------------------------------------------------------------------
struct HwIndexBuffer : public HwBase {
    uint32_t count = 0;
    ElementType elementType = ElementType::USHORT;
    BufferUsage usage = BufferUsage::STATIC;
};

// ---------------------------------------------------------------------------
// HwRenderPrimitive — combines vertex + index buffer for drawing
// ---------------------------------------------------------------------------
struct HwRenderPrimitive : public HwBase {
    PrimitiveType type = PrimitiveType::TRIANGLES;
};

// ---------------------------------------------------------------------------
// HwProgram — compiled GPU shader program
// ---------------------------------------------------------------------------
struct HwProgram : public HwBase {
    std::string name;
};

// ---------------------------------------------------------------------------
// HwTexture — GPU texture
// ---------------------------------------------------------------------------
struct HwTexture : public HwBase {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 0;
    SamplerType target = SamplerType::SAMPLER_2D;
    uint8_t levels = 1;
    uint8_t samples = 1;
    TextureFormat format = TextureFormat::RGBA8;
    TextureUsage usage = TextureUsage::DEFAULT;
};

// ---------------------------------------------------------------------------
// HwRenderTarget — framebuffer
// ---------------------------------------------------------------------------
struct HwRenderTarget : public HwBase {
    uint32_t width = 0;
    uint32_t height = 0;
};

// ---------------------------------------------------------------------------
// HwSwapChain — platform swap chain
// ---------------------------------------------------------------------------
struct HwSwapChain : public HwBase {
    void* nativeWindow = nullptr;
};

// ---------------------------------------------------------------------------
// HwFence — GPU synchronization fence
// ---------------------------------------------------------------------------
struct HwFence : public HwBase {};

// ---------------------------------------------------------------------------
// HwDescriptorSetLayout / HwDescriptorSet
// ---------------------------------------------------------------------------
struct HwDescriptorSetLayout : public HwBase {};
struct HwDescriptorSet : public HwBase {};

// ---------------------------------------------------------------------------
// HwSync — GPU synchronization primitive
// ---------------------------------------------------------------------------
struct HwSync : public HwBase {};

// ---------------------------------------------------------------------------
// HwTimerQuery — GPU timer query
// ---------------------------------------------------------------------------
struct HwTimerQuery : public HwBase {};

// ---------------------------------------------------------------------------
// HwStream — external image stream
// ---------------------------------------------------------------------------
struct HwStream : public HwBase {};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE

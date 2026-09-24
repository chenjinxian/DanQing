// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — VBO/VAO management
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/AttributeBuffers.ts
//
// Manages vertex buffer objects and vertex array objects (VAOs).
//
// Integration point: itwinjs's BuffersContainer held a WebGLVertexArrayObject
// and called gl.bindVertexArray()/gl.vertexAttribPointer().  In the RHI
// approach, this maps to createVertexBufferInfo + createRenderPrimitive +
// bindRenderPrimitive.
#pragma once

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"
#include "dqCommon/QPoint.h"

#include <array>
#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// BufferParameters — vertex attribute descriptor
// ---------------------------------------------------------------------------
struct BufferParameters {
    uint32_t location;     // attribute location
    uint32_t size;         // components per vertex (1-4)
    rhi::ElementType type; // data type
    bool normalized = false;
    uint32_t stride = 0;
    uint32_t offset = 0;
    bool glInstanced = false;  // Ported from: itwinjs-core AttributeBuffers.ts BufferParameters.glInstanced
};

// ---------------------------------------------------------------------------
// BuffersContainer — manages VBO/VAO (maps to RenderPrimitive in RHI)
// (Ported from: itwinjs-core AttributeBuffers.ts BuffersContainer)
// ---------------------------------------------------------------------------
class BuffersContainer {
public:
    BuffersContainer() = default;
    ~BuffersContainer() = default;

    /// add a vertex buffer with attribute parameters.
    void addBuffer(rhi::Driver& driver, void const* data, uint32_t dataSize,
                   std::vector<BufferParameters> const& params);

    /// Set the index buffer.
    void setIndexBuffer(rhi::Driver& driver, void const* data, uint32_t dataSize,
                        uint32_t indexCount, rhi::ElementType elementType);

    /// Create the render primitive (combines vertex + index buffers).
    void finalize(rhi::Driver& driver, rhi::PrimitiveType primType = rhi::PrimitiveType::TRIANGLES);

    /// Bind for rendering (calls driver.bindRenderPrimitive).
    void bind(rhi::Driver& driver);

    /// Get the render primitive handle.
    rhi::RenderPrimitiveHandle getPrimitiveHandle() const noexcept { return m_primitive; }

    uint32_t getNumIndices() const noexcept { return m_numIndices; }

private:
    rhi::VertexBufferInfoHandle m_vertexBufferInfo;
    rhi::VertexBufferHandle m_vertexBuffer;
    rhi::IndexBufferHandle m_indexBuffer;
    rhi::BufferObjectHandle m_bufferObject;
    rhi::RenderPrimitiveHandle m_primitive;
    uint32_t m_numIndices = 0;
    bool m_finalized = false;
};

// ---------------------------------------------------------------------------
// Quantization helpers
// Ported from: itwinjs-core AttributeBuffers.ts setScale, qparams2dToArray,
//              qorigin3dToArray, qscale3dToArray, qparams3dToArray
// ---------------------------------------------------------------------------

inline void setScale(uint32_t index, float value, std::array<float, 3>& arr) {
    arr[index] = (0.0f != value) ? (1.0f / value) : value;
}

/// Converts 3d quantization params to a pair of float arrays.
/// Ported from: itwinjs-core AttributeBuffers.ts qparams3dToArray
inline std::pair<std::array<float, 3>, std::array<float, 3>> qparams3dToArray(dqCommon::QParams3d const& params) {
    std::array<float, 3> origin = {
        static_cast<float>(params.origin.x),
        static_cast<float>(params.origin.y),
        static_cast<float>(params.origin.z)
    };
    std::array<float, 3> scale = {};
    setScale(0, static_cast<float>(params.scale.x), scale);
    setScale(1, static_cast<float>(params.scale.y), scale);
    setScale(2, static_cast<float>(params.scale.z), scale);
    return {origin, scale};
}

/// Converts 2d quantization params to a float array.
/// Ported from: itwinjs-core AttributeBuffers.ts qparams2dToArray
inline std::array<float, 4> qparams2dToArray(dqCommon::QParams2d const& params) {
    std::array<float, 4> arr = {};
    arr[0] = static_cast<float>(params.origin.x);
    arr[1] = static_cast<float>(params.origin.y);
    arr[2] = (0.0f != static_cast<float>(params.scale.x)) ? (1.0f / static_cast<float>(params.scale.x)) : 0.0f;
    arr[3] = (0.0f != static_cast<float>(params.scale.y)) ? (1.0f / static_cast<float>(params.scale.y)) : 0.0f;
    return arr;
}

// ---------------------------------------------------------------------------
// QBufferHandle3d — quantized 3d vertex buffer
// Ported from: itwinjs-core AttributeBuffers.ts QBufferHandle3d
// ---------------------------------------------------------------------------
class QBufferHandle3d {
public:
    /// The quantization origin in x, y, and z
    std::array<float, 3> origin;
    /// The quantization scale in x, y, and z
    std::array<float, 3> scale;

    explicit QBufferHandle3d(dqCommon::QParams3d const& qParams) {
        auto [o, s] = qparams3dToArray(qParams);
        origin = o;
        scale = s;
    }

    static QBufferHandle3d create(dqCommon::QParams3d const& qParams) {
        return QBufferHandle3d(qParams);
    }
};

// ---------------------------------------------------------------------------
// QBufferHandle2d — quantized 2d vertex buffer
// Ported from: itwinjs-core AttributeBuffers.ts QBufferHandle2d
// ---------------------------------------------------------------------------
class QBufferHandle2d {
public:
    std::array<float, 4> params;

    explicit QBufferHandle2d(dqCommon::QParams2d const& qParams)
        : params(qparams2dToArray(qParams))
    {
    }

    static QBufferHandle2d create(dqCommon::QParams2d const& qParams) {
        return QBufferHandle2d(qParams);
    }
};

END_DQ_RENDER_NAMESPACE

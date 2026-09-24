// SPDX-License-Identifier: Apache-2.0
// Authored: DanQing test infrastructure — a recording rhi::Driver for verifying GPU-upload logic.
//
// MockDriver derives NullDriver and overrides only the rhi::Driver methods that MeshRenderGeometry::
// create(driver, Mesh) calls, recording their arguments. Tests then assert the recorded call sequence
// (vertex count, index count, PrimitiveType per mesh type) — runtime-verifying the upload contract
// WITHOUT a real GL context. Portable; runs in ctest.
//
// Handle correlation: createRenderPrimitive(vbh, ibh, pt) is emitted once per primitive, immediately
// after that primitive's createIndexBuffer (see MeshGraphic.cpp createIbo lambda). So recorded
// primitive i corresponds to indexBuffer i by call order — primitiveIndexCount(i) == indexCountAt(i).
#pragma once

#include "NullDriver.h"

#include <cstddef>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

class MockDriver : public NullDriver {
public:
    // --- Recorded overrides (the 7 methods MeshRenderGeometry::create exercises) ---
    VertexBufferHandle createVertexBuffer(uint32_t vertexCount, VertexBufferInfoHandle) noexcept override
    {
        m_vertexCounts.push_back(vertexCount);
        return {};
    }
    IndexBufferHandle createIndexBuffer(ElementType, uint32_t indexCount, BufferUsage) noexcept override
    {
        m_indexCounts.push_back(indexCount);
        return {};
    }
    RenderPrimitiveHandle createRenderPrimitive(VertexBufferHandle, IndexBufferHandle, PrimitiveType pt) noexcept override
    {
        m_primitiveTypes.push_back(pt);
        return {};
    }
    BufferObjectHandle createBufferObject(uint32_t, BufferObjectBinding, BufferUsage) noexcept override
    {
        ++m_bufferObjectCount;
        return {};
    }
    void updateBufferObject(BufferObjectHandle, BufferDescriptor&&, uint32_t) noexcept override
    {
        ++m_bufferObjectUpdateCount;
    }
    void updateIndexBuffer(IndexBufferHandle, BufferDescriptor&&, uint32_t) noexcept override
    {
        ++m_indexBufferUpdateCount;
    }
    VertexBufferInfoHandle createVertexBufferInfo(uint8_t, uint8_t, AttributeArray const&) noexcept override
    {
        ++m_vertexBufferInfoCount;
        return {};
    }
    void setVertexBufferObject(VertexBufferHandle, uint32_t, BufferObjectHandle) noexcept override
    {
        ++m_setVboCount;
    }

    // --- Accessors ---
    size_t vertexBufferCount() const noexcept { return m_vertexCounts.size(); }
    uint32_t lastVertexCount() const noexcept { return m_vertexCounts.empty() ? 0 : m_vertexCounts.back(); }

    size_t indexBufferCount() const noexcept { return m_indexCounts.size(); }
    uint32_t indexCountAt(size_t i) const noexcept { return m_indexCounts.at(i); }

    size_t primitiveCount() const noexcept { return m_primitiveTypes.size(); }
    PrimitiveType primitiveTypeAt(size_t i) const noexcept { return m_primitiveTypes.at(i); }
    // createRenderPrimitive is emitted once per primitive, right after that primitive's createIndexBuffer.
    uint32_t primitiveIndexCount(size_t i) const noexcept { return m_indexCounts.at(i); }

    size_t bufferObjectCount() const noexcept { return m_bufferObjectCount; }
    size_t bufferObjectUpdateCount() const noexcept { return m_bufferObjectUpdateCount; }
    size_t indexBufferUpdateCount() const noexcept { return m_indexBufferUpdateCount; }
    size_t vertexBufferInfoCount() const noexcept { return m_vertexBufferInfoCount; }
    size_t setVboCount() const noexcept { return m_setVboCount; }

private:
    std::vector<uint32_t> m_vertexCounts;
    std::vector<uint32_t> m_indexCounts;
    std::vector<PrimitiveType> m_primitiveTypes;
    size_t m_bufferObjectCount = 0;
    size_t m_bufferObjectUpdateCount = 0;
    size_t m_indexBufferUpdateCount = 0;
    size_t m_vertexBufferInfoCount = 0;
    size_t m_setVboCount = 0;
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE

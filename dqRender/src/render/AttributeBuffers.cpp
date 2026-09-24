// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — AttributeBuffers implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/AttributeBuffers.ts
#include "AttributeBuffers.h"

BEGIN_DQ_RENDER_NAMESPACE

void BuffersContainer::addBuffer(rhi::Driver& driver, void const* data, uint32_t dataSize,
                                 std::vector<BufferParameters> const& params)
{
    if (!data || dataSize == 0 || params.empty()) return;

    // Create buffer object for vertex data
    m_bufferObject = driver.createBufferObject(
        dataSize, rhi::BufferObjectBinding::VERTEX, rhi::BufferUsage::STATIC);

    // Upload data
    rhi::BufferDescriptor desc(data, dataSize);
    driver.updateBufferObject(m_bufferObject, std::move(desc), 0);

    // Create vertex buffer info from parameters
    rhi::AttributeArray attrs = {};
    uint8_t attrCount = 0;
    for (auto const& p : params) {
        if (p.location < rhi::MAX_VERTEX_ATTRIBUTE_COUNT) {
            attrs[p.location].buffer = 0;
            attrs[p.location].offset = static_cast<uint8_t>(p.offset);
            attrs[p.location].type = p.type;
            attrCount = std::max(attrCount, static_cast<uint8_t>(p.location + 1));
        }
    }

    m_vertexBufferInfo = driver.createVertexBufferInfo(1, attrCount, attrs);
    m_vertexBuffer = driver.createVertexBuffer(0, m_vertexBufferInfo);  // vertexCount set later

    // Bind buffer object to vertex buffer
    driver.setVertexBufferObject(m_vertexBuffer, 0, m_bufferObject);
}

void BuffersContainer::setIndexBuffer(rhi::Driver& driver, void const* data, uint32_t dataSize,
                                      uint32_t indexCount, rhi::ElementType elementType)
{
    if (!data || dataSize == 0) return;

    m_indexBuffer = driver.createIndexBuffer(elementType, indexCount, rhi::BufferUsage::STATIC);

    rhi::BufferDescriptor desc(data, dataSize);
    driver.updateBufferObject(
        m_indexBuffer.getId() != rhi::HandleBase::nullid
            ? rhi::BufferObjectHandle{}  // IndexBuffer has its own upload path
            : rhi::BufferObjectHandle{},
        std::move(desc), 0);

    m_numIndices = indexCount;
}

void BuffersContainer::finalize(rhi::Driver& driver, rhi::PrimitiveType primType)
{
    if (m_vertexBuffer && m_indexBuffer) {
        m_primitive = driver.createRenderPrimitive(m_vertexBuffer, m_indexBuffer, primType);
        m_finalized = true;
    }
}

void BuffersContainer::bind(rhi::Driver& driver)
{
    if (m_primitive) {
        driver.bindRenderPrimitive(m_primitive);
    }
}

END_DQ_RENDER_NAMESPACE

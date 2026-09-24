// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — UniformBlockLayout implementation
// Ported from: filament UniformBufferBlock + itwinjs-core uniform binding
#include "UniformBlockLayout.h"

#include <cstring>

BEGIN_DQ_RENDER_NAMESPACE

void UniformBlockLayout::addMatrix4(char const* name)
{
    m_size = alignTo(m_size, 16);
    UniformBlockField field;
    field.name = name;
    field.offset = m_size;
    field.size = 64;  // 4x4 float = 64 bytes
    field.arrayCount = 1;
    m_fields[name] = field;
    m_size += 64;
}

void UniformBlockLayout::addVec4(char const* name)
{
    m_size = alignTo(m_size, 16);
    UniformBlockField field;
    field.name = name;
    field.offset = m_size;
    field.size = 16;  // 4 float = 16 bytes
    field.arrayCount = 1;
    m_fields[name] = field;
    m_size += 16;
}

void UniformBlockLayout::addVec3(char const* name)
{
    // In std140, vec3 is aligned to 16 bytes but only uses 12 bytes.
    m_size = alignTo(m_size, 16);
    UniformBlockField field;
    field.name = name;
    field.offset = m_size;
    field.size = 12;  // 3 float = 12 bytes (padded to 16 in std140)
    field.arrayCount = 1;
    m_fields[name] = field;
    m_size += 16;  // Padded to 16 for std140 alignment
}

void UniformBlockLayout::addVec2(char const* name)
{
    m_size = alignTo(m_size, 8);
    UniformBlockField field;
    field.name = name;
    field.offset = m_size;
    field.size = 8;  // 2 float = 8 bytes
    field.arrayCount = 1;
    m_fields[name] = field;
    m_size += 8;
}

void UniformBlockLayout::addFloat(char const* name)
{
    m_size = alignTo(m_size, 4);
    UniformBlockField field;
    field.name = name;
    field.offset = m_size;
    field.size = 4;  // 1 float = 4 bytes
    field.arrayCount = 1;
    m_fields[name] = field;
    m_size += 4;
}

void UniformBlockLayout::addInt(char const* name)
{
    m_size = alignTo(m_size, 4);
    UniformBlockField field;
    field.name = name;
    field.offset = m_size;
    field.size = 4;  // 1 int = 4 bytes
    field.arrayCount = 1;
    m_fields[name] = field;
    m_size += 4;
}

UniformBlockField const* UniformBlockLayout::getField(char const* name) const
{
    auto it = m_fields.find(name);
    return it != m_fields.end() ? &it->second : nullptr;
}

void UniformBlockLayout::pack(ShaderProgramParams const& params, uint8_t* buffer) const
{
    for (auto const& [name, uv] : params.getUniforms()) {
        auto it = m_fields.find(name);
        if (it == m_fields.end())
            continue;

        auto const& field = it->second;

        switch (uv.type) {
            case UniformValue::Type::Mat4:
                std::memcpy(buffer + field.offset, uv.floatData.data(), 64);
                break;
            case UniformValue::Type::Vec4:
                std::memcpy(buffer + field.offset, uv.floatData.data(), 16);
                break;
            case UniformValue::Type::Vec3:
                std::memcpy(buffer + field.offset, uv.floatData.data(), 12);
                break;
            case UniformValue::Type::Vec2:
                std::memcpy(buffer + field.offset, uv.floatData.data(), 8);
                break;
            case UniformValue::Type::Float:
                std::memcpy(buffer + field.offset, uv.floatData.data(),
                           uv.arrayCount * sizeof(float));
                break;
            case UniformValue::Type::Int:
                std::memcpy(buffer + field.offset, &uv.intData, sizeof(int));
                break;
        }
    }
}

END_DQ_RENDER_NAMESPACE

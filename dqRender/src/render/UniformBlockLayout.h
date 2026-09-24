// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Uniform block layout for UBO packing
// Ported from: filament UniformBufferBlock + itwinjs-core uniform binding
//
// Defines the std140 layout for packing uniforms into a UBO.
// Provides methods to pack ShaderProgramParams into a flat buffer.
#pragma once

#include "ShaderProgramImpl.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// UniformBlockField — describes a single uniform in a UBO
// ---------------------------------------------------------------------------
struct UniformBlockField {
    std::string name;
    uint32_t offset = 0;     // Byte offset in the UBO
    uint32_t size = 0;       // Size in bytes
    uint32_t arrayCount = 1; // Array element count
};

// ---------------------------------------------------------------------------
// UniformBlockLayout — defines the std140 layout for a uniform block
// ---------------------------------------------------------------------------
class UniformBlockLayout {
public:
    UniformBlockLayout() = default;

    /// add a mat4 field (64 bytes, aligned to 16).
    void addMatrix4(char const* name);

    /// add a vec4 field (16 bytes, aligned to 16).
    void addVec4(char const* name);

    /// add a vec3 field (12 bytes, padded to 16 in std140).
    void addVec3(char const* name);

    /// add a vec2 field (8 bytes, aligned to 8).
    void addVec2(char const* name);

    /// add a float field (4 bytes, aligned to 4).
    void addFloat(char const* name);

    /// add an int field (4 bytes, aligned to 4).
    void addInt(char const* name);

    /// Get the total size of the uniform block (rounded up to 16-byte alignment).
    uint32_t getSize() const { return m_size; }

    /// Get the field map (name → field descriptor).
    std::unordered_map<std::string, UniformBlockField> const& getFields() const { return m_fields; }

    /// Check if a field exists.
    bool hasField(char const* name) const { return m_fields.find(name) != m_fields.end(); }

    /// Get a field by name. Returns nullptr if not found.
    UniformBlockField const* getField(char const* name) const;

    /// pack ShaderProgramParams into a buffer according to this layout.
    /// @param params Source uniform values
    /// @param buffer Destination buffer (must be at least getSize() bytes)
    void pack(ShaderProgramParams const& params, uint8_t* buffer) const;

private:
    uint32_t alignTo(uint32_t offset, uint32_t alignment) const
    {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    std::unordered_map<std::string, UniformBlockField> m_fields;
    uint32_t m_size = 0;
};

END_DQ_RENDER_NAMESPACE

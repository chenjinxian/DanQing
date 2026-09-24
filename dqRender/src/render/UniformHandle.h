// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Uniform value caching and binding
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/UniformHandle.ts
//
// Caches uniform values to avoid redundant GL calls.  Each uniform tracks its
// type and current data, only issuing the GL call when the value changes.
// The handle owns its GL uniform location (resolved at compile from the linked
// program) and every dirty set* dispatches directly to glUniform* — the 1:1
// reference behavior (UniformHandle.ts:90-138; the ambient TS WebGL context
// maps to zogl/OpenGL calls on the current context).
//
// The type discriminator (UniformType) resolves the mat3 vs vec3×3 ambiguity:
// both are 9 floats, but mat3 needs glUniformMatrix3fv while vec3[3] needs
// glUniform3fv.  The type tag ensures dirty-checking detects type changes.
#pragma once

#include "rhi/opengl/Gl.h"  // GLint/GLuint + glUniform* dispatch (current context)

#include <array>
#include <cstdint>
#include <cstring>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// UniformHandle — cached uniform value + its GL location
// Ported from: itwinjs-core UniformHandle.ts
//
// Type discriminator matches itwinjs-core DataType const enum (line 16-28).
// Used for dirty-checking: type change OR data change → GL upload.
// ---------------------------------------------------------------------------
class UniformHandle {
public:
    // Ported from: itwinjs-core UniformHandle.ts DataType (line 16-28)
    // File-private in TS; exposed here as a nested enum for GL dispatch.
    enum class UniformType : uint8_t {
        Undefined,
        Mat3,
        Mat4,
        Float,
        FloatArray,
        Vec2,
        Vec3,
        Vec4,
        Int,
        IntArray,
        Uint,
    };

    /// Default: no location (GL-free/test path) — set* only caches, never
    /// dispatches.  Mirrors the reference's null-location no-op semantics.
    UniformHandle() = default;

    /// Resolve the uniform's location from the linked GL program.
    /// Ported from: itwinjs-core UniformHandle.create() (line 41-55)
    ///
    /// A missing uniform (location -1) maps the reference's logError +
    /// null-location no-op (`uniform*(null, ...)` is ignored by WebGL) to a
    /// silent skip: set* never dispatches for this handle.
    static UniformHandle create(GLuint glProgram, char const* name) noexcept
    {
        GLint location = -1;
        if (glProgram != 0)
            location = glGetUniformLocation(glProgram, name);
        return UniformHandle(location);
    }

    /// Set a mat4 uniform.  Returns true if the value changed.
    /// Ported from: itwinjs-core UniformHandle.setMatrix4() (line 95-98)
    bool setMatrix4(float const* data) noexcept
    {
        if (m_type == UniformType::Mat4 && std::memcmp(m_data.data(), data, 16 * sizeof(float)) == 0)
            return false;
        m_type = UniformType::Mat4;
        std::memcpy(m_data.data(), data, 16 * sizeof(float));
        if (m_location >= 0)
            glUniformMatrix4fv(m_location, 1, GL_FALSE, data);
        return true;
    }

    /// Set a mat3 uniform.  Returns true if the value changed.
    /// Ported from: itwinjs-core UniformHandle.setMatrix3() (line 90-93)
    bool setMatrix3(float const* data) noexcept
    {
        if (m_type == UniformType::Mat3 && std::memcmp(m_data.data(), data, 9 * sizeof(float)) == 0)
            return false;
        m_type = UniformType::Mat3;
        std::memcpy(m_data.data(), data, 9 * sizeof(float));
        if (m_location >= 0)
            glUniformMatrix3fv(m_location, 1, GL_FALSE, data);
        return true;
    }

    /// Set a vec4 uniform (or vec4 array).
    /// Ported from: itwinjs-core UniformHandle.setUniform4fv() (line 120-123)
    bool setUniform4fv(float const* data, size_t count = 1) noexcept
    {
        size_t sz = 4 * count;
        if (m_type == UniformType::Vec4 && m_floatCount == sz &&
            std::memcmp(m_data.data(), data, sz * sizeof(float)) == 0)
            return false;
        m_type = UniformType::Vec4;
        m_floatCount = sz;
        std::memcpy(m_data.data(), data, sz * sizeof(float));
        if (m_location >= 0)
            glUniform4fv(m_location, static_cast<GLsizei>(count), data);
        return true;
    }

    /// Set a vec3 uniform (or vec3 array).
    /// Ported from: itwinjs-core UniformHandle.setUniform3fv() (line 115-118)
    bool setUniform3fv(float const* data, size_t count = 1) noexcept
    {
        size_t sz = 3 * count;
        if (m_type == UniformType::Vec3 && m_floatCount == sz &&
            std::memcmp(m_data.data(), data, sz * sizeof(float)) == 0)
            return false;
        m_type = UniformType::Vec3;
        m_floatCount = sz;
        std::memcpy(m_data.data(), data, sz * sizeof(float));
        if (m_location >= 0)
            glUniform3fv(m_location, static_cast<GLsizei>(count), data);
        return true;
    }

    /// Set a vec2 uniform (or vec2 array).
    /// Ported from: itwinjs-core UniformHandle.setUniform2fv() (line 110-113)
    bool setUniform2fv(float const* data, size_t count = 1) noexcept
    {
        size_t sz = 2 * count;
        if (m_type == UniformType::Vec2 && m_floatCount == sz &&
            std::memcmp(m_data.data(), data, sz * sizeof(float)) == 0)
            return false;
        m_type = UniformType::Vec2;
        m_floatCount = sz;
        std::memcpy(m_data.data(), data, sz * sizeof(float));
        if (m_location >= 0)
            glUniform2fv(m_location, static_cast<GLsizei>(count), data);
        return true;
    }

    /// Set a float uniform (or float array).
    /// Ported from: itwinjs-core UniformHandle.setUniform1fv() (line 105-108)
    bool setUniform1fv(float const* data, size_t count = 1) noexcept
    {
        size_t sz = count;
        if (m_type == UniformType::FloatArray && m_floatCount == sz &&
            std::memcmp(m_data.data(), data, sz * sizeof(float)) == 0)
            return false;
        m_type = UniformType::FloatArray;
        m_floatCount = sz;
        std::memcpy(m_data.data(), data, sz * sizeof(float));
        if (m_location >= 0)
            glUniform1fv(m_location, static_cast<GLsizei>(count), data);
        return true;
    }

    /// Set an int uniform.
    /// Ported from: itwinjs-core UniformHandle.setUniform1i() (line 125-128)
    bool setUniform1i(int value) noexcept
    {
        if (m_type == UniformType::Int && m_dataInt[0] == value) return false;
        m_type = UniformType::Int;
        m_dataInt[0] = value;
        if (m_location >= 0)
            glUniform1i(m_location, value);
        return true;
    }

    /// Set an int-array uniform (e.g. u_surfaceFlags[12]).
    /// Ported from: itwinjs-core UniformHandle.setUniform1iv() (line 100-103)
    bool setUniform1iv(int const* data, size_t count) noexcept
    {
        if (m_type == UniformType::IntArray && m_intCount == count &&
            std::memcmp(m_dataInt.data(), data, count * sizeof(int)) == 0)
            return false;
        m_type = UniformType::IntArray;
        m_intCount = count;
        std::memcpy(m_dataInt.data(), data, count * sizeof(int));
        if (m_location >= 0)
            glUniform1iv(m_location, static_cast<GLsizei>(count), data);
        return true;
    }

    /// Set a float uniform (scalar).
    /// Ported from: itwinjs-core UniformHandle.setUniform1f() (line 130-133)
    bool setUniform1f(float value) noexcept
    {
        if (m_type == UniformType::Float && m_data[0] == value) return false;
        m_type = UniformType::Float;
        m_data[0] = value;
        if (m_location >= 0)
            glUniform1f(m_location, value);
        return true;
    }

    /// Set a uint uniform.
    /// Ported from: itwinjs-core UniformHandle.setUniform1ui() (line 135-138)
    bool setUniform1ui(unsigned int value) noexcept
    {
        if (m_type == UniformType::Uint && m_dataUint[0] == value) return false;
        m_type = UniformType::Uint;
        m_dataUint[0] = value;
        if (m_location >= 0)
            glUniform1ui(m_location, value);
        return true;
    }

    float const* getData() const noexcept { return m_data.data(); }
    int const* getIntData() const noexcept { return m_dataInt.data(); }
    UniformType getType() const noexcept { return m_type; }

private:
    // Ported from: itwinjs-core UniformHandle constructor (line 39)
    explicit UniformHandle(GLint location) noexcept : m_location(location) {}

    GLint m_location = -1;       // -1 = unresolved/missing (never dispatches)
    UniformType m_type = UniformType::Undefined;
    size_t m_floatCount = 0;   // for Vec2/Vec3/Vec4/FloatArray array counts
    size_t m_intCount = 0;     // for IntArray count
    std::array<float, 64> m_data = {};
    std::array<int, 16> m_dataInt = {};
    std::array<unsigned int, 1> m_dataUint = {};
};

END_DQ_RENDER_NAMESPACE

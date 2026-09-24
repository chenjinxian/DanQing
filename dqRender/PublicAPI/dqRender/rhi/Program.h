// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GPU program builder
// Ported from: filament backend/include/backend/Program.h
//
// A builder object that carries everything needed to create a GPU shader
// program: shader sources, descriptor bindings, push constants, etc.
// Uses a fluent API pattern.
#pragma once

#include "DriverEnums.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// ---------------------------------------------------------------------------
// Shader source blob
// ---------------------------------------------------------------------------
using ShaderBlob = std::vector<uint8_t>;

// ---------------------------------------------------------------------------
// Program — shader program builder
// ---------------------------------------------------------------------------
class Program {
public:
    static constexpr size_t SHADER_STAGE_COUNT = 3;  // vertex, fragment, compute

    Program() = default;

    // --- Fluent builder API (matching Filament exactly) ---

    /// Set shader source for a stage.
    Program& shader(ShaderStage stage, void const* data, size_t size)
    {
        auto& blob = m_shaderSource[static_cast<size_t>(stage)];
        blob.assign(static_cast<uint8_t const*>(data),
                    static_cast<uint8_t const*>(data) + size);
        return *this;
    }

    /// Set shader source from a string.
    Program& shader(ShaderStage stage, std::string const& source)
    {
        return shader(stage, source.data(), source.size());
    }

    /// Set the shader language.
    Program& shaderLanguage(ShaderLanguage language) noexcept
    {
        m_shaderLanguage = language;
        return *this;
    }

    /// Set the program name (for debugging).
    Program& name(std::string const& programName)
    {
        m_name = programName;
        return *this;
    }

    /// Set a cache ID for shader binary caching.
    Program& cacheId(uint64_t id) noexcept
    {
        m_cacheId = id;
        return *this;
    }

    /// Set an explicit attribute location. The driver binds these via
    /// glBindAttribLocation BEFORE glLinkProgram so geometry VAO slots line up
    /// with shader inputs deterministically (no reliance on GL auto-assignment).
    /// Ported from: filament backend Program.attributeLocation().
    Program& attributeLocation(std::string name, uint8_t location)
    {
        m_attributeLocations.emplace_back(std::move(name), location);
        return *this;
    }

    // --- Accessors ---

    ShaderBlob const& getShaderSource(ShaderStage stage) const noexcept
    {
        return m_shaderSource[static_cast<size_t>(stage)];
    }

    ShaderLanguage getShaderLanguage() const noexcept { return m_shaderLanguage; }
    std::string const& getName() const noexcept { return m_name; }
    uint64_t getCacheId() const noexcept { return m_cacheId; }
    std::vector<std::pair<std::string, uint8_t>> const& getAttributeLocations() const noexcept
    {
        return m_attributeLocations;
    }

private:
    std::array<ShaderBlob, SHADER_STAGE_COUNT> m_shaderSource;
    ShaderLanguage m_shaderLanguage = ShaderLanguage::ESSL3;
    std::string m_name;
    uint64_t m_cacheId = 0;
    std::vector<std::pair<std::string, uint8_t>> m_attributeLocations;
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE

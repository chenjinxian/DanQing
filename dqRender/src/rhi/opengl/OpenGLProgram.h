// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OpenGL linked program + uniform/sampler table
// Ported from: filament backend/src/opengl/OpenGLProgram.h
//
// Extends HwProgram with the actual GL program name and uniform location cache.
#pragma once

#include "Gl.h"
#include "rhi/DriverBase.h"
#include "dqRender/rhi/DriverEnums.h"
#include "dqRender/rhi/Handle.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// ---------------------------------------------------------------------------
// OpenGLProgram — linked GL program with uniform locations
// ---------------------------------------------------------------------------
class OpenGLProgram : public HwProgram {
public:
    OpenGLProgram() noexcept = default;
    ~OpenGLProgram();

    OpenGLProgram(OpenGLProgram const&) = delete;
    OpenGLProgram& operator=(OpenGLProgram const&) = delete;

    /// Compile and link the program from source strings.
    /// @param attributeLocations  name→location pairs bound via glBindAttribLocation
    ///         BEFORE glLinkProgram (so VAO slots line up with shader inputs).
    /// @return true on success.
    bool compile(char const* vertSource, char const* fragSource,
                 std::string const& programName,
                 std::vector<std::pair<std::string, uint8_t>> const& attributeLocations = {});

    /// Bind the program for use.
    void use() noexcept;

    /// Get the uniform location (cached).
    GLint getUniformLocation(char const* name) noexcept;

    /// Get the attribute location.
    GLint getAttribLocation(char const* name) noexcept;

    GLuint getProgram() const noexcept { return m_program; }
    bool isValid() const noexcept { return m_program != 0; }

private:
    GLuint m_program = 0;
    std::unordered_map<std::string, GLint> m_uniformCache;
    std::unordered_map<std::string, GLint> m_attribCache;
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE

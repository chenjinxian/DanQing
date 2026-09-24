// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GLSL compile+link test harness (macOS / CGL).
// Authored: no reference test exists in itwinjs-core for offline GLSL compile
//           verification (itwinjs compiles live in a browser via WebGL).
//           This harness creates an offscreen GL 4.1 core context via CGL and
//           compiles+links vert+frag sources, returning the GL info logs so
//           GoogleTest can assert the assembled GLSL actually compiles+links
//           (CLAUDE.md §5 test fidelity, §6 conformance — verify, don't assert
//           on substrings). Used by SurfaceCompileTest and reusable for any
//           shader-builder output.
//
// macOS-only: CGL lives in OpenGL.framework; the dqRenderTest executable
// already links ${OPENGL_FRAMEWORK} + ${COCOA_FRAMEWORK} (dqRender/CMakeLists.txt).
// On non-Apple builds this header compiles to a no-op harness that reports
// contextAvailable=false (tests GTEST_SKIP).
#pragma once

#if defined(__APPLE__)

// macOS marks OpenGL/CGL as deprecated (since 10.14) and -Werror is on
// project-wide.  GL_SILENCE_DEPRECATION is Apple's sanctioned macro to mute
// those warnings; it must be defined before the GL headers are included.
#define GL_SILENCE_DEPRECATION 1

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>

#include <string>
#include <vector>

namespace dqRender {
namespace test {

// Outcome of one compile+link attempt.
struct GlslCompileResult {
    bool contextAvailable = false;  // false ⇒ no GL context; caller should SKIP
    bool vertCompiled = false;
    bool fragCompiled = false;
    bool linked = false;
    std::string vertLog;
    std::string fragLog;
    std::string linkLog;
};

namespace detail {

// Lazily create one offscreen GL 4.1 core context for the test process and
// return it.  A current context is required for glCreateShader/glCompileShader.
// The context is intentionally leaked (test-only, lives for the process).
inline CGLContextObj ensureContext()
{
    static CGLContextObj sCtx = nullptr;
    static bool sTried = false;
    if (sTried) return sCtx;
    sTried = true;

    CGLPixelFormatAttribute attrs[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core,
        kCGLPFAAllowOfflineRenderers,
        (CGLPixelFormatAttribute)0
    };
    CGLPixelFormatObj pix = nullptr;
    GLint npix = 0;
    if (kCGLNoError != CGLChoosePixelFormat(attrs, &pix, &npix) || !pix)
        return nullptr;
    if (kCGLNoError != CGLCreateContext(pix, nullptr, &sCtx) || !sCtx)
        sCtx = nullptr;
    CGLDestroyPixelFormat(pix);
    return sCtx;
}

inline std::string getShaderLog(GLuint shader)
{
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len <= 0) return {};
    std::vector<char> buf(static_cast<size_t>(len));
    glGetShaderInfoLog(shader, len, nullptr, buf.data());
    return std::string(buf.data());
}

inline std::string getProgramLog(GLuint prog)
{
    GLint len = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    if (len <= 0) return {};
    std::vector<char> buf(static_cast<size_t>(len));
    glGetProgramInfoLog(prog, len, nullptr, buf.data());
    return std::string(buf.data());
}

inline bool compileStage(GLenum type, std::string const& src, std::string& log)
{
    GLuint shader = glCreateShader(type);
    char const* srcs = src.c_str();
    GLint lengths[1] = { static_cast<GLint>(src.size()) };
    glShaderSource(shader, 1, &srcs, lengths);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    log = getShaderLog(shader);
    return shader != 0 && ok == GL_TRUE;
}

}  // namespace detail

// Compile + link a vert/frag pair.  Makes the offscreen context current.
inline GlslCompileResult compileAndLinkProgram(std::string const& vert,
                                                std::string const& frag)
{
    GlslCompileResult r;
    CGLContextObj ctx = detail::ensureContext();
    if (!ctx) {
        r.contextAvailable = false;
        return r;
    }
    r.contextAvailable = true;
    CGLSetCurrentContext(ctx);

    r.vertCompiled = detail::compileStage(GL_VERTEX_SHADER, vert, r.vertLog);
    r.fragCompiled = detail::compileStage(GL_FRAGMENT_SHADER, frag, r.fragLog);

    if (!r.vertCompiled || !r.fragCompiled)
        return r;  // link cannot succeed if either stage failed to compile

    GLuint prog = glCreateProgram();
    // Rebuild shaders to attach (compileStage deletes its shader object).
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    {
        char const* s = vert.c_str(); GLint l = static_cast<GLint>(vert.size());
        glShaderSource(vs, 1, &s, &l); glCompileShader(vs);
    }
    {
        char const* s = frag.c_str(); GLint l = static_cast<GLint>(frag.size());
        glShaderSource(fs, 1, &s, &l); glCompileShader(fs);
    }
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    r.linked = (ok == GL_TRUE);
    r.linkLog = detail::getProgramLog(prog);
    glDeleteProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return r;
}

}  // namespace test
}  // namespace dqRender

#else  // !__APPLE__

#include <string>

namespace dqRender {
namespace test {

struct GlslCompileResult {
    bool contextAvailable = false;
    bool vertCompiled = false;
    bool fragCompiled = false;
    bool linked = false;
    std::string vertLog;
    std::string fragLog;
    std::string linkLog;
};

inline GlslCompileResult compileAndLinkProgram(std::string const&,
                                                std::string const&)
{
    return {};  // no offscreen GL on this platform; tests SKIP
}

}  // namespace test
}  // namespace dqRender

#endif  // __APPLE__

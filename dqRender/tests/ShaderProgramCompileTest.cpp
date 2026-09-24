// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ShaderProgram compile data-flow regression test (GL-free)
//
// Authored: itwinjs-core validates shader compile/link on a live GL context. This
// test isolates the DATA-FLOW root cause of the macOS Apple-GL "Compiled vertex/
// fragment shader was corrupt" link failure (ShaderProgram::compile must pass BOTH
// vertex + fragment sources to createProgram inside ONE rhi::Program) without a GL
// context. The rhi::Program holds sources in a per-stage array (m_shaderSource[
// SHADER_STAGE_COUNT]); a single Program must carry every stage (filament builds
// one Program per material). The bug was createProgram being called with a
// vertex-only Program, leaving the fragment stage empty -> no-main fragment link.
#include "NullDriver.h"

#include "render/ShaderProgramImpl.h"

#include <gtest/gtest.h>

#include <string>

using namespace dqRender;

namespace {
// A NullDriver that records the rhi::Program each createProgram receives, so the
// test can assert which shader stages were delivered — WITHOUT compiling GL.
class RecordingProgramDriver : public rhi::NullDriver {
public:
    rhi::ProgramHandle createProgram(rhi::Program&& program) noexcept override {
        m_lastVertSize = program.getShaderSource(rhi::ShaderStage::VERTEX).size();
        m_lastFragSize = program.getShaderSource(rhi::ShaderStage::FRAGMENT).size();
        ++m_createProgramCount;
        return {};  // null handle: compile() reports Failure, but createProgram was called.
    }

    std::size_t m_lastVertSize = 0;
    std::size_t m_lastFragSize = 0;
    int m_createProgramCount = 0;
};
}  // namespace

// Root-cause regression for the shader-link "corrupt" failure.
// Before the fix, compileShader() invoked createProgram only on the VERTEX stage
// (with a vertex-only Program); the fragment source was discarded -> OpenGLProgram
// linked an empty no-main fragment shader -> "corrupt". After the fix, compile()
// builds one Program carrying both stages, so createProgram receives both.
TEST(ShaderProgramCompile, PassesBothVertexAndFragmentSourcesToCreateProgram)
{
    RecordingProgramDriver driver;
    ShaderProgram prog;
    std::string const vert = "attribute vec2 a_pos; void main(){ gl_Position = vec4(a_pos,0.0,1.0); }";
    std::string const frag = "precision mediump float; void main(){ gl_FragColor = vec4(1.0); }";

    // Null handle -> compile returns Failure; we assert on what createProgram received.
    EXPECT_NE(prog.compile(driver, vert, frag, "link-root-cause-test"), CompileStatus::Success);

    EXPECT_EQ(driver.m_createProgramCount, 1);     // exactly one createProgram call
    EXPECT_EQ(driver.m_lastVertSize, vert.size()); // vertex source delivered
    EXPECT_EQ(driver.m_lastFragSize, frag.size()); // FAILS before fix (0) — the root cause
}

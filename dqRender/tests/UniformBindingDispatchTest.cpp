// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — uniform 绑定派发探针（真 GL 上下文 + readPixels 像素断言）
//
// Authored: no reference test exists in itwinjs-core for uniform-binding GL dispatch
// (参考 UniformHandle.ts:90-138 经环境 WebGL context 直接派发，浏览器测试无隔离
//  该机制的用例；按 §5(g) 渲染/窗口行为回归授权自写像素级回归)。
//
// 探针机制（TD-15 RED 锁）：
//   注册 ProgramUniform/GraphicUniform 绑定 → 绑定回调 u.setUniform1f(源值)
//   → 全屏三角片元色 = vec4(u_probe,0,0,1) → 改源值重绑 → readPixels 断言
//   红通道随绑定值变化。
//   RED（修复前）：UniformHandle 只写缓存不上屏 → u_probe 恒为 GL 默认 0 → 黑。
//   GREEN（修复后）：set* dirty 时直接 glUniform* → 红通道 = 0.25→64 / 1.0→255。
// 复现配方：无 legacy uploadUniforms 调用——探针隔离纯绑定路径（名值映射路径
// 由 ShaderProgramCompileTest/GLCanvasContext 覆盖）。
#include "render/ShaderProgramImpl.h"  // ShaderProgram, UniformHandle, ShaderProgramParams
#include "render/DrawParams.h"         // DrawParams（GraphicUniform 探针载荷）
#include "platform/PlatformFactory.h"  // createPlatform
#include "rhi/opengl/GlLoader.h"       // Windows: dqgl::init 运行时符号装载

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>

using namespace dqRender;

#if defined(_WIN32)
    #include <windows.h>
#endif

namespace {

// 离屏 GL 环境（RenderSmokeTest.OffscreenRenderEnv 的最小化：探针不需要
// RenderSystem/Techniques——ShaderProgram 只需 rhi::Driver）。
struct ProbeEnv {
    std::unique_ptr<rhi::OpenGLPlatform> platform;
    std::unique_ptr<rhi::Driver> driver;

    bool init() {
#if defined(_WIN32)
        if (!dqgl::init())
            return false;
#endif
        platform.reset(rhi::createPlatform());
        if (!platform)
            return false;
        rhi::DriverConfig config;
        driver.reset(platform->createDriver(nullptr, config));
        return driver != nullptr;
    }
};

// 全屏三角（gl_VertexID 展开，无顶点缓冲）：(0,0)/(2,0)/(0,2) → NDC 覆盖全屏。
char const* kProbeVert = R"glsl(#version 410 core
void main() {
    vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";

// 片元色 = 绑定 uniform 值本身（红通道 = u_probe）。
char const* kProbeFrag = R"glsl(#version 410 core
uniform float u_probe;
out vec4 FragColor;
void main() { FragColor = vec4(u_probe, 0.0, 0.0, 1.0); }
)glsl";

// 16x16 RGBA8 FBO + 中心像素红通道回读。
struct ProbeFbo {
    GLuint fbo = 0, tex = 0, vao = 0;

    void init() {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glGenVertexArrays(1, &vao);  // core profile 必须绑一个 VAO（无属性）
        glBindVertexArray(vao);
        glViewport(0, 0, 16, 16);
    }

    uint8_t readCenterRed() {
        uint8_t px[4] = {};
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        return px[0];
    }

    void destroy() {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);
        glDeleteVertexArrays(1, &vao);
    }
};

}  // namespace

// ProgramUniform 绑定派发：use() 时绑定回调写入的值必须到达 GL。
// 断言信息量 ≥ 失败自由度：两个不同绑定值 → 两个不同像素值（0→64→255），
// 同时锁定"派发了"与"dirty 后重派发"两个行为。
TEST(UniformBindingDispatch, ProgramUniformBindingDispatchesToGl)
{
    ProbeEnv env;
    ASSERT_TRUE(env.init()) << "offscreen GL environment unavailable";

    ProbeFbo fbo;
    fbo.init();

    float probeValue = 0.25f;
    ShaderProgram prog;
    prog.setSource(kProbeVert, kProbeFrag, "uniform-binding-dispatch-probe");
    prog.addProgramUniform("u_probe", [&probeValue](UniformHandle& u, ShaderProgramParams const&) {
        u.setUniform1f(probeValue);
    });

    // 空名值映射——探针不走 legacy uploadUniforms。
    ShaderProgramParams params;

    // 帧 1：绑定值 0.25 → 红通道 ≈ 64
    ASSERT_TRUE(prog.use(*env.driver, params));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    EXPECT_NEAR(fbo.readCenterRed(), 64, 2)
        << "ProgramUniform binding value 0.25 did not reach GL (u_probe stayed at default)";

    // 帧 2：改源值 1.0 → endUse+use 重绑 → 红通道 = 255
    prog.endUse(*env.driver);
    probeValue = 1.0f;
    ASSERT_TRUE(prog.use(*env.driver, params));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    EXPECT_NEAR(fbo.readCenterRed(), 255, 2)
        << "ProgramUniform binding value 1.0 did not re-dispatch (dirty update missing)";

    fbo.destroy();
}

// GraphicUniform 绑定派发：draw() 时绑定回调写入的值必须到达 GL（同程序
// 连续两次 draw，值变 → dirty → 重派发）。
TEST(UniformBindingDispatch, GraphicUniformBindingDispatchesToGl)
{
    ProbeEnv env;
    ASSERT_TRUE(env.init()) << "offscreen GL environment unavailable";

    ProbeFbo fbo;
    fbo.init();

    float probeValue = 0.25f;
    ShaderProgram prog;
    prog.setSource(kProbeVert, kProbeFrag, "uniform-binding-dispatch-probe");
    prog.addGraphicUniform("u_probe", [&probeValue](UniformHandle& u, DrawParams const&) {
        u.setUniform1f(probeValue);
    });

    ShaderProgramParams params;
    DrawParams drawParams;

    ASSERT_TRUE(prog.use(*env.driver, params));

    // 绘 1：绑定值 0.25 → 红通道 ≈ 64
    prog.draw(drawParams);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    EXPECT_NEAR(fbo.readCenterRed(), 64, 2)
        << "GraphicUniform binding value 0.25 did not reach GL (u_probe stayed at default)";

    // 绘 2：改源值 1.0 → draw 重绑 → 红通道 = 255
    probeValue = 1.0f;
    prog.draw(drawParams);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    EXPECT_NEAR(fbo.readCenterRed(), 255, 2)
        << "GraphicUniform binding value 1.0 did not re-dispatch (dirty update missing)";

    fbo.destroy();
}

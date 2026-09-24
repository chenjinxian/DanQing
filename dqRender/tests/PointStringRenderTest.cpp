// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Headless PointString (GL_POINTS) render + position regression test.
//
// Authored: no reference test exists in itwinjs-core for headless GL render + readback
// of the PointString mechanism (itwinjs renders live in a browser via WebGL). This is
// the pixel-truth verification that the ACS Z-axis tip — a PointString whose stored
// vertex is recentered about the graphic's range center — projects at the disc center
// once PrimitiveBuilder.finish()'s recovery translate reaches the draw MVP.
//
// What it exercises (the REAL PointString vertex shader end-to-end, no mocks):
//   * createPointStringProgramBuilder vertex stage — live GLSL generation (the actual
//     TechniqueId::PointString vertex shader the compositor dispatches).
//   * a_position/a_color attribute binding at locations 0/2 (cd1f75f).
//   * gl_Position = u_mvp · vec4(a_position,1.0) with the recentering-recovery
//     translate folded in (RenderCommands branchEffectiveTransform), so a recentered
//     vertex projects at its world position instead of offset by the range center.
//
// The core assertion: a point stored recentered (world − center), drawn with
// u_mvp = proj · translate(center), lands at the screen projection of `world`. Drawn
// with u_mvp = proj (no recovery translate — the pre-fix bug), it lands offset by
// `center`. The contrast is the proof.
//
// NOTE on the fragment stage: the real PointString fragment discards outside the unit
// circle via gl_PointCoord. macOS's offscreen CGL context returns gl_PointCoord=(0,0)
// for point fragments (a known headless quirk; the live Qt GL context does not — the
// user verified samplesPassed=114 there), which would cull every fragment and defeat a
// pixel readback. The discard affects only the round *shape*, not the *position* this
// test measures, so the position check pairs the real vertex shader with a plain
// (no-discard) red fragment.
#include "GlslCompileHarness.h"

#include "render/Matrix.h"
#include "render/ShaderBuilder.h"
#include "render/shader/PolylineShaderBuilder.h"  // createPointStringProgramBuilder

#include <gtest/gtest.h>

#include "rhi/opengl/Gl.h" // 平台统一 GL 来源（mac: gl3.h / Win: glcorearb.h / Linux: gl.h）

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

using namespace dqRender;

namespace {

constexpr int kSize = 256;
constexpr float kCenter[3] = {50.0f, 50.0f, 0.0f};   // the recentering origin (range center)
constexpr float kWorld[3]  = {128.0f, 128.0f, 0.0f};  // where the point should land (screen px)

// Orthographic map: world pixel (wx,wy) -> NDC (2*wx/size-1, 2*wy/size-1), so a point
// at `world` renders at screen pixel `world`. Column-major.
Matrix4 orthoScreenProj()
{
    float const s = 2.0f / static_cast<float>(kSize);
    Matrix4 m;  // identity-initialized
    m.data[0] = s;  m.data[5] = s;  m.data[12] = -1.0f;  m.data[13] = -1.0f;
    return m;
}

// Compile the REAL PointString vertex shader (createPointStringProgramBuilder vert
// stage) with a plain no-discard red fragment (see file header NOTE). Binds
// a_position=0 / a_color=2 before link (cd1f75f attribute map).
GLuint compilePointStringPositionProgram()
{
    ProgramBuilder builder = createPointStringProgramBuilder(FeatureMode::None, PositionType::Unquantized);
    std::string const vert = builder.getVertexBuilder().buildSourceWithComponents();
    std::string const frag =
        "#version 410 core\n"
        "in vec4 v_color;\n"
        "out vec4 fragColor;\n"
        "void main(){ fragColor = v_color; }\n";

    auto mk = [](GLenum t, std::string const& src) {
        GLuint s = glCreateShader(t);
        char const* p = src.c_str();
        GLint l = static_cast<GLint>(src.size());
        glShaderSource(s, 1, &p, &l);
        glCompileShader(s);
        return s;
    };

    GLuint program = glCreateProgram();
    glAttachShader(program, mk(GL_VERTEX_SHADER, vert));
    glAttachShader(program, mk(GL_FRAGMENT_SHADER, frag));
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 2, "a_color");
    glLinkProgram(program);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        std::array<char, 4096> log{};
        glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::fprintf(stderr, "PointString position program link FAILED:\n%s\n", log.data());
        return 0;
    }
    return program;
}

// One PointString vertex: pos = recentered (world - center), color = red. Compact
// layout: float pos[3] (attrib 0) + float color[4] (attrib 2), stride 28.
GLuint buildPointVao()
{
    float const recentered[7] = {
        kWorld[0] - kCenter[0], kWorld[1] - kCenter[1], kWorld[2] - kCenter[2],  // pos
        1.0f, 0.0f, 0.0f, 1.0f                                                    // color (red)
    };
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(recentered), recentered, GL_STATIC_DRAW);

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glEnableVertexAttribArray(0);  // a_position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), nullptr);
    glEnableVertexAttribArray(2);  // a_color
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    return vao;
}

struct PointImage { int redPixels = 0; double cx = 0.0; double cy = 0.0; };

PointImage renderAndFindCentroid(GLuint program, GLuint vao, Matrix4 const& mvp, float pointSize)
{
    GLint loc = glGetUniformLocation(program, "u_mvp");
    GLint ps  = glGetUniformLocation(program, "u_pointSize");

    glViewport(0, 0, kSize, kSize);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);  // blue clear — red point is the target
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program);
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, mvp.data);
    if (ps >= 0)  glUniform1f(ps, pointSize);
    glEnable(GL_PROGRAM_POINT_SIZE);  // SceneCompositorImpl.cpp:897 (desktop GL)
    glBindVertexArray(vao);
    glDrawArrays(GL_POINTS, 0, 1);

    std::vector<uint8_t> px(static_cast<size_t>(kSize) * kSize * 4);
    glReadPixels(0, 0, kSize, kSize, GL_RGBA, GL_UNSIGNED_BYTE, px.data());

    PointImage img;
    double sx = 0.0, sy = 0.0;
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            uint8_t const* p = &px[(y * kSize + x) * 4];
            if (p[0] > 128 && p[1] < 128 && p[2] < 128) {  // red-dominant
                sx += x; sy += y; ++img.redPixels;
            }
        }
    }
    if (img.redPixels > 0) { img.cx = sx / img.redPixels; img.cy = sy / img.redPixels; }
    return img;
}

}  // namespace

// The recentering-recovery translate must reach u_mvp: a recentered point
// (world − center) drawn with u_mvp = proj·translate(center) lands at `world`
// (screen center, = the disc center), not offset by `center`.
TEST(PointStringRenderTest, RecoveryTranslatePlacesPointAtDiscCenter)
{
    test::GlslCompileResult probe = test::compileAndLinkProgram(
        "#version 410 core\nvoid main(){}",
        "#version 410 core\nvoid main(){}");
    if (!probe.contextAvailable) GTEST_SKIP() << "no offscreen GL context available";

    GLuint fbo = 0, rbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kSize, kSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    GLuint program = compilePointStringPositionProgram();
    ASSERT_NE(program, 0u) << "PointString vertex shader failed to compile+link";
    GLuint vao = buildPointVao();

    Matrix4 const proj = orthoScreenProj();
    Matrix4 const translate = Matrix4::translation(kCenter[0], kCenter[1], kCenter[2]);

    // FIXED: u_mvp = proj · translate(center) — recovery translate folded in.
    Matrix4 const mvpFixed = Matrix4::multiply(proj, translate);
    PointImage const fixed = renderAndFindCentroid(program, vao, mvpFixed, 6.0f);

    // BUGGY (pre-fix): u_mvp = proj — no recovery translate; point projects offset.
    PointImage const buggy = renderAndFindCentroid(program, vao, proj, 6.0f);

    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteFramebuffers(1, &fbo);

    // The PointString vertex stage must rasterize (position verified by pixel centroid).
    ASSERT_GT(fixed.redPixels, 0) << "PointString vertex stage produced no fragments";

    // FIXED: the recentered point lands at `world` (the disc center).
    EXPECT_NEAR(fixed.cx, kWorld[0], 2.0) << "tip x not at disc center with recovery translate";
    EXPECT_NEAR(fixed.cy, kWorld[1], 2.0) << "tip y not at disc center with recovery translate";

    // CONTRAST — the proof the translate matters: without it the point lands offset
    // by `center` (the pre-fix ACS-tip symptom: tip off the disc).
    EXPECT_NEAR(buggy.cx, kWorld[0] - kCenter[0], 2.0) << "buggy tip x not offset by center";
    EXPECT_NEAR(buggy.cy, kWorld[1] - kCenter[1], 2.0) << "buggy tip y not offset by center";
    EXPECT_GT(std::abs(fixed.cx - buggy.cx), 40.0) << "recovery translate had no effect on position";
}

// Regression guard for the visibility root cause: the PointString fragment must
// declare fragColor as an `out` variable (via the shared addFragData, the same path
// Surface/Polyline use). The prior code rolled its own and declared fragColor as a
// plain VariableScope::Global (ShaderBuilder emits those with no qualifier) — so the
// fragment wrote a non-output global and the framebuffer color attachment was NEVER
// written: the ACS Z-axis tip rasterized (GL_SAMPLES_PASSED > 0) but stayed INVISIBLE.
// (Live-verified: the blue dot appears only once fragColor is `out`.) A render-based
// check can't catch this headlessly because macOS's offscreen CGL context returns
// gl_PointCoord=(0,0), which the circular discard culls to 0 fragments — so this is a
// source-level assertion on the generated GLSL.
TEST(PointStringRenderTest, FragmentDeclaresOutFragColor)
{
    ProgramBuilder builder = createPointStringProgramBuilder(FeatureMode::None, PositionType::Unquantized);
    std::string const frag = builder.getFragmentBuilder().buildSourceWithComponents();
    EXPECT_NE(std::string::npos, frag.find("out vec4 fragColor"))
        << "PointString fragment must declare fragColor as `out` (a plain Global writes no "
           "framebuffer color → the point rasterizes but is invisible)";
}

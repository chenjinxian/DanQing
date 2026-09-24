// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Headless Polyline thick-line render regression test.
//
// Authored: no reference test exists in itwinjs-core for headless GL render +
// readback of the Polyline thick-line mechanism. itwinjs renders live in a
// browser via WebGL; this test renders through a headless offscreen GL context
// (CGL) + FBO + glReadPixels to OBJECTIVELY prove the weight-N polyline renders
// at ~N px width (vs the pre-port 1px-everywhere behavior where glLineWidth was
// never applied). The whole point of the faithful Polyline port (Tasks 1-5)
// was making the ACS triad's weight-2 label glyphs render at 2 px — this test
// is the permanent regression gate for that mechanism.
//
// What it exercises (the REAL Polyline technique end-to-end, no mocks):
//   * PolylineVariantCompiler::buildProgram  — live GLSL program generation
//   * VertexTableBuilder::buildFromPolylines — transposed-SoA LUT byte packing
//   * PolylineTesselator::tesselate          — 24-bit corner buffer emission
//   * u_vertLUT sampling in the vertex shader (g_vertLutData0..5 pre-read +
//     on-demand samplePosition for prev/next)
//   * buildComputePosition miter/square math + adjustWidth
//   * u_lineWeight driving the per-pixel displacement (dist = weight/2)
//
// Path: a 2-point horizontal polyline (mirrors one ACS "X" diagonal —
// AcsTriadDecorator.cpp:109 `b.addLineString(d0, 2)` with setSymbology weight 2)
// is rendered once at weight=1 and once at weight=2 through the same FBO.
// glReadPixels counts red pixels in the rendered band. The CORE ASSERTION is
// weight-2 covers strictly MORE pixels than weight-1 (the line is measurably
// wider); the IDEAL outcome is weight-2 ≈ 2x weight-1 pixel count, with the
// vertical span going 1 → 2 rows.

#include "GlslCompileHarness.h"

#include "render/PolylineTesselator.h"
#include "render/PolylineVariantCompiler.h"
#include "render/SurfaceGeometry.h"      // PolylineGeometry
#include "render/VertexLutTexture.h"
#include "render/VertexTableBuilder.h"
#include "rhi/opengl/OpenGLDriver.h"     // real OpenGLDriver (live path)
#include "dqRender/rhi/OpenGLPlatform.h"
#include "dqRender/rhi/Platform.h"       // DriverConfig
#include "dqRender/rhi/DriverEnums.h"    // ElementType, AttributeArray, BufferObjectBinding, ...
#include "dqRender/rhi/BufferDescriptor.h"

#include <dqCommon/ColorDef.h>
#include <dqGeom/Point3d.h>

#include <gtest/gtest.h>

#include "rhi/opengl/Gl.h" // 平台统一 GL 来源（mac: gl3.h / Win: glcorearb.h / Linux: gl.h）；非 Apple 下 harness 为 no-op，测试 SKIP

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace dqRender;
using dqCommon::ColorDef;
using dqGeom::Point3d;

namespace {

// Headless GL context + FBO state kept across the two weight passes so the
// expensive context + shader compile happens once. Mirrors the SurfaceCompile
// harness pattern (SurfaceCompileTest.cpp: HeadlessCubeViaSurfaceVertexShader).
struct RenderEnv {
    GLuint fbo = 0;
    GLuint rbo = 0;
    GLuint program = 0;
    int viewportW = 256;
    int viewportH = 256;
    bool ok = false;
};

// (1) Compile the REAL Polyline variant shader (Unquantized / FeatureMode::None
// — the ACS variant). PolylineVariantCompiler is what the live SceneCompositor
// dispatches for TechniqueId::Polyline, so this is the actual app shader.
GLuint compilePolylineProgram() {
    PolylineVariantCompiler compiler;
    TechniqueFlags flags;
    flags.positionType = PositionType::Unquantized;   // ACS = unquantized LUT
    flags.featureMode = FeatureMode::None;            // no feature overrides
    ShaderProgram prog;
    compiler.buildProgram(prog, flags);
    std::string const vert = prog.getVertSource();
    std::string const frag = prog.getFragSource();

    auto mk = [](GLenum t, std::string const& src) {
        GLuint s = glCreateShader(t);
        char const* p = src.c_str();
        GLint l = static_cast<GLint>(src.size());
        glShaderSource(s, 1, &p, &l);
        glCompileShader(s);
        return s;
    };

    // Bind attribute locations BEFORE link to match PolylineVariantCompiler's
    // setAttributeMap (a_pos/a_prevIndex/a_nextIndex/a_param at 0/1/2/3) —
    // PolylineCompileTest.PolylineVariantCompilerAttributeLocationsBoundExplicitly
    // verifies the same map.
    GLuint program = glCreateProgram();
    glAttachShader(program, mk(GL_VERTEX_SHADER, vert));
    glAttachShader(program, mk(GL_FRAGMENT_SHADER, frag));
    glBindAttribLocation(program, 0, "a_pos");
    glBindAttribLocation(program, 1, "a_prevIndex");
    glBindAttribLocation(program, 2, "a_nextIndex");
    glBindAttribLocation(program, 3, "a_param");
    glLinkProgram(program);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(static_cast<size_t>(logLen + 1), 0);
        glGetProgramInfoLog(program, logLen, nullptr, log.data());
        std::fprintf(stderr, "Polyline program link FAILED:\n%s\n", log.data());
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

// (2) Build + upload the LUT for a polyline. Returns the GL texture handle +
// the u_vertParams = (texWidth, texHeight, numRgbaPerVert, numVertices) the
// shader needs. The texture uses the BuiltVertexTable's faithful dims (NOT
// VertexLutTexture::create's recomputed pow-2 dims — we test the LUT bytes
// directly to keep the path raw and dependency-free).
struct LutBundle {
    GLuint texture = 0;
    float vertParams[4] = {0, 0, 0, 0};
};
LutBundle buildAndUploadLut(std::vector<Point3d> const& points, ColorDef color) {
    BuiltVertexTable vt = VertexTableBuilder::buildFromPolylines(
        points.data(), static_cast<uint32_t>(points.size()), color);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // vt.data has width*height*4 bytes (RGBA8). Upload tightly; no padding needed
    // because vt.width*vt.height == numVertices*numRgbaPerVert by construction.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 static_cast<GLsizei>(vt.width), static_cast<GLsizei>(vt.height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, vt.data.data());

    LutBundle b;
    b.texture = tex;
    b.vertParams[0] = static_cast<float>(vt.width);
    b.vertParams[1] = static_cast<float>(vt.height);
    b.vertParams[2] = static_cast<float>(vt.numRgbaPerVertex);
    b.vertParams[3] = static_cast<float>(vt.numVertices);
    return b;
}

// (3) Build + upload the corner VAO from PolylineTesselator's 3 byte arrays.
// Mirrors MeshGraphic.cpp:432-453 (PolylineBuffers multi-VBO layout) — buffer 0
// = a_pos (UBYTE3), buffer 1 = a_prevIndex (UBYTE3), buffer 2 = a_nextIndex +
// a_param (UBYTE3 + UBYTE@offset3). a_pos is the 24-bit LUT key.
struct CornerVao {
    GLuint vao = 0;
    GLuint vbo[3] = {0, 0, 0};
    uint32_t numCorners = 0;
};
CornerVao buildCornerVao(std::vector<Point3d> const& points, float weight) {
    TesselatedPolyline tess = PolylineTesselator::tesselate(
        points.data(), static_cast<uint32_t>(points.size()),
        weight, /*is2d*/ false, /*disjoint*/ false);

    CornerVao r;
    r.numCorners = tess.numCorners();
    if (r.numCorners == 0) return r;

    glGenVertexArrays(1, &r.vao);
    glBindVertexArray(r.vao);

    uint32_t const posBytes = r.numCorners * 3u;
    uint32_t const prevBytes = r.numCorners * 3u;
    uint32_t const nextPropsBytes = r.numCorners * 4u;

    glGenBuffers(3, r.vbo);

    // buffer 0: a_pos (location 0, UBYTE3, stride 3, offset 0)
    glBindBuffer(GL_ARRAY_BUFFER, r.vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, posBytes, tess.indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_UNSIGNED_BYTE, GL_FALSE, 3, nullptr);

    // buffer 1: a_prevIndex (location 1, UBYTE3, stride 3, offset 0)
    glBindBuffer(GL_ARRAY_BUFFER, r.vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, prevBytes, tess.prevIndices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_FALSE, 3, nullptr);

    // buffer 2: a_nextIndex (location 2, UBYTE3) + a_param (location 3, UBYTE @ offset 3)
    // Stride = 4 bytes/corner (3 nextIdx + 1 param).
    glBindBuffer(GL_ARRAY_BUFFER, r.vbo[2]);
    glBufferData(GL_ARRAY_BUFFER, nextPropsBytes, tess.nextIndicesAndParams.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_UNSIGNED_BYTE, GL_FALSE, 4, nullptr);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_UNSIGNED_BYTE, GL_FALSE, 4, reinterpret_cast<void*>(3));

    return r;
}

// (4) Bind the full uniform set the Polyline technique expects (mirrors
// SceneCompositorImpl.cpp:1074-1127 Polyline dispatch + BranchUniforms). The
// shader's modelToWindowCoordinates path skips front-clip under
// u_renderPass == kRenderPass_ViewOverlay (13.0) — same path the ACS triad's
// WorldOverlay decorations take, and avoids near-plane clipping of our z=0
// points. Identity MV/proj/viewportTransformation leaves the polyline in NDC.
void bindPolylineUniforms(GLuint program, float weight, LutBundle const& lut,
                          int viewportW, int viewportH,
                          float renderPass = 13.0f,
                          float const* mv16 = nullptr,
                          float nearPlane = 1.0f,
                          float const* proj16 = nullptr) {
    auto setMat4 = [&](const char* name, float const* m) {
        GLint l = glGetUniformLocation(program, name);
        if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, m);
    };
    auto setVec4 = [&](const char* name, float const* v) {
        GLint l = glGetUniformLocation(program, name);
        if (l >= 0) glUniform4fv(l, 1, v);
    };
    auto setVec3 = [&](const char* name, float const* v) {
        GLint l = glGetUniformLocation(program, name);
        if (l >= 0) glUniform3fv(l, 1, v);
    };
    auto setVec2 = [&](const char* name, float const* v) {
        GLint l = glGetUniformLocation(program, name);
        if (l >= 0) glUniform2fv(l, 1, v);
    };
    auto setF = [&](const char* name, float v) {
        GLint l = glGetUniformLocation(program, name);
        if (l >= 0) glUniform1f(l, v);
    };
    auto setI = [&](const char* name, int v) {
        GLint l = glGetUniformLocation(program, name);
        if (l >= 0) glUniform1i(l, v);
    };
    auto setUiv = [&](const char* name, int count, GLuint const* v) {
        GLint l = glGetUniformLocation(program, name);
        if (l >= 0) glUniform1uiv(l, count, v);
    };

    float const ident4[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    setMat4("u_mvp", ident4);
    // u_mv configurable: defaults to identity (the original ViewOverlay test
    // path). The WorldOverlay clip branch compares eye-space q.z = (u_mv·pos).z
    // against -u_frustum.x, so a translated u_mv is needed to exercise it.
    setMat4("u_mv", mv16 ? mv16 : ident4);
    // u_proj configurable: identity for the identity-camera configs (q.z=0 →
    // ndc_z=0, no GPU clip); an ortho projection for the translated-camera
    // configs so a negative eye-space z maps into NDC[-1,1] (else the GPU
    // NDC-clip discards vertices the shader's front-plane test correctly kept).
    setMat4("u_proj", proj16 ? proj16 : ident4);
    setMat4("u_viewportTransformation", ident4);

    float const viewport[2] = {static_cast<float>(viewportW),
                                static_cast<float>(viewportH)};
    setVec2("u_viewport", viewport);

    // frustum: u_frustum.x = near plane. The clip branch uses s_maxZ = -near.
    // (unused under the ViewOverlay no-clip path, but referenced by addFrustum).
    float const frustum[3] = {nearPlane, 100.0f, 1.0f};
    setVec3("u_frustum", frustum);

    // u_renderPass: 13.0 (ViewOverlay) → modelToWindowCoordinates takes the
    // no-clip overlay branch (ViewportShaders.h:31). 12.0 (WorldOverlay) → the
    // front-plane clip branch (the path ACS world-overlay decorations take).
    setF("u_renderPass", renderPass);

    // --- Polyline-specific uniforms (SceneCompositorImpl.cpp:1074-1127) ---
    setF("u_lineWeight", weight);

    // Bind LUT to texture unit 0; sampler reads unit 0.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, lut.texture);
    setI("u_vertLUT", 0);
    setVec4("u_vertParams", lut.vertParams);

    // Unquantized positions — qOrigin/qScale unused (computeUnquantizedPositionFromLUT
    // does not reference them) but declared by addVertexTable; set safe defaults.
    float const qOrigin[3] = {0, 0, 0};
    float const qScale[3] = {1, 1, 1};
    setVec3("u_qOrigin", qOrigin);
    setVec3("u_qScale", qScale);

    // Red line so the readback can find it against the blue clear.
    float const lineColor[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    setVec4("u_color", lineColor);

    // LineCode pipeline (solid line: u_lineCode=0 → texc=(-1,-1) → no discard).
    setF("u_lineCode", 0.0f);
    setF("u_useCumDist", 0.0f);
    setF("u_pixelsPerWorld", 1.0f);
    setF("u_numLineCodes", 1.0f);
    setI("u_aaSamples", 1);  // non-multisampled → adjustWidth non-AA path
    // u_bgIntensity < 0 → adjustContrast returns baseColor unchanged (Edge.ts
    // adjustContrast: `if (bgi < 0.0) return baseColor;`). With bgi>=0 pure red
    // (rgbi=0.30) maps to s=0.699 → gray output, defeating red-vs-blue detection.
    setF("u_bgIntensity", -1.0f);
    setI("u_reverseWhiteOnWhite", 0);
    GLuint const shaderFlags[5] = {0, 0, 0, 0, 0};
    setUiv("u_shaderFlags[0]", 5, shaderFlags);
}

// (5) Render a polyline at `weight` and return (a) the count of red pixels in
// the rendered band, and (b) the vertical span (rows containing red pixels).
// The band is the center column stripe where the horizontal line lives.
struct Measurement { int redPixels = 0; int verticalSpan = 0; int horizontalSpan = 0; };

Measurement renderAndMeasure(RenderEnv const& env, float weight,
                             float renderPass = 13.0f,
                             float const* mv16 = nullptr,
                             float nearPlane = 1.0f,
                             float const* proj16 = nullptr) {
    // Horizontal line through NDC y=0: spans x ∈ [-0.4, +0.4] → ~205 window px.
    // y=0 NDC maps exactly to window y=128 = pixel boundary (between rows 127
    // and 128). For weight=1 the quad spans window y ∈ [127.5, 128.5] → 1 px
    // (top-left rule). For weight=2 it spans [127.0, 129.0] → 2 px. This is the
    // load-bearing assertion that weight-N → ~N pixel width.
    std::vector<Point3d> points = {
        Point3d::From(-0.4, 0.0, 0.0),
        Point3d::From( 0.4, 0.0, 0.0),
    };

    LutBundle lut = buildAndUploadLut(points, ColorDef::red);
    CornerVao corner = buildCornerVao(points, weight);

    Measurement m;
    if (corner.numCorners == 0) {
        glDeleteTextures(1, &lut.texture);
        return m;
    }

    auto checkErr = [](const char* where) {
        GLenum e = glGetError();
        if (e != GL_NO_ERROR) {
            std::fprintf(stderr, "  GL ERROR at %s: 0x%x\n", where, e);
        }
    };

    glViewport(0, 0, env.viewportW, env.viewportH);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);   // blue clear — red line is the target
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glUseProgram(env.program);
    checkErr("after useProgram");
    bindPolylineUniforms(env.program, weight, lut, env.viewportW, env.viewportH,
                         renderPass, mv16, nearPlane, proj16);
    checkErr("after bindUniforms");

    glBindVertexArray(corner.vao);
    checkErr("after bindVertexArray");
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(corner.numCorners));
    checkErr("after drawArrays");
    glFlush();

    // Read back the center band: rows [120..135] x full width. Count red pixels
    // (R > 0.5) and track vertical/horizontal spans.
    int const y0 = 120;
    int const y1 = 135;   // exclusive
    int const h = y1 - y0;
    std::vector<unsigned char> px(static_cast<size_t>(env.viewportW * h * 4));
    glReadPixels(0, y0, env.viewportW, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());

    int rowMin = +1000000, rowMax = -1000000;
    int colMin = +1000000, colMax = -1000000;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < env.viewportW; ++x) {
            size_t off = static_cast<size_t>((y * env.viewportW + x) * 4);
            int r = px[off + 0], g = px[off + 1];
            // Line pixel = NOT the blue clear (R+G > 100 rules out pure blue).
            // (Clear color is R=0,G=0,B=255; line is anything else.)
            if (r + g > 100) {
                ++m.redPixels;
                int const winY = y0 + y;
                if (winY < rowMin) rowMin = winY;
                if (winY > rowMax) rowMax = winY;
                if (x < colMin) colMin = x;
                if (x > colMax) colMax = x;
            }
        }
    }
    if (rowMin <= rowMax) m.verticalSpan = rowMax - rowMin + 1;
    if (colMin <= colMax) m.horizontalSpan = colMax - colMin + 1;

    std::fprintf(stderr,
        "  [weight=%.1f] redPixels=%d verticalSpan=%d horizontalSpan=%d (rows %d..%d, cols %d..%d)\n",
        static_cast<double>(weight), m.redPixels, m.verticalSpan, m.horizontalSpan,
        rowMin == +1000000 ? -1 : rowMin, rowMax == -1000000 ? -1 : rowMax,
        colMin == +1000000 ? -1 : colMin, colMax == -1000000 ? -1 : colMax);

    glDeleteTextures(1, &lut.texture);
    glDeleteVertexArrays(1, &corner.vao);
    glDeleteBuffers(3, corner.vbo);
    return m;
}

}  // namespace

// Primary regression test: render the same horizontal Polyline at weight 1 and
// weight 2 through the REAL Polyline technique (PolylineVariantCompiler shader
// + VertexTableBuilder LUT + PolylineTesselator corner buffer) and assert
// weight-2 covers strictly MORE pixels than weight-1 (ideally ~2x with vertical
// span going 1→2). Pre-port, every polyline rendered at 1 px (no glLineWidth),
// so weight-1 == weight-2 — this test guards against that regression.
//
// Authored: no reference test exists in itwinjs-core for headless GL render +
// readback of the Polyline thick-line mechanism (itwinjs renders live in a
// browser via WebGL). The 2-point horizontal line mirrors one ACS "X" diagonal
// (AcsTriadDecorator.cpp:109 `b.addLineString(d0, 2)`); the assertion mirrors
// the user's live ACS-triad visual gate at the GL-pixel level.
TEST(PolylineRenderTest, RendersWeightAsPixelWidth)
{
    // Establish the offscreen GL context (side effect: leaves it current).
    test::GlslCompileResult probe = test::compileAndLinkProgram(
        "#version 410 core\nvoid main(){}",
        "#version 410 core\nvoid main(){}");
    if (!probe.contextAvailable) GTEST_SKIP() << "no offscreen GL context available";

    RenderEnv env;
    env.viewportW = 256;
    env.viewportH = 256;

    // FBO + RGBA8 renderbuffer.
    glGenFramebuffers(1, &env.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, env.fbo);
    glGenRenderbuffers(1, &env.rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, env.rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, env.viewportW, env.viewportH);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, env.rbo);
    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    env.program = compilePolylineProgram();
    ASSERT_NE(env.program, 0u) << "Polyline variant shader failed to compile+link";
    env.ok = true;

    Measurement m1 = renderAndMeasure(env, 1.0f);
    Measurement m2 = renderAndMeasure(env, 2.0f);

    glDeleteProgram(env.program);
    glDeleteRenderbuffers(1, &env.rbo);
    glDeleteFramebuffers(1, &env.fbo);

    // ---- Core assertions (the whole point of the faithful thick-line port) ----

    // Sanity: both weights must actually render something. If weight-1 redPixels
    // is 0, the mechanism is fully broken (LUT upload, attribute binding, etc.)
    // and the wider-than assertion below is meaningless.
    ASSERT_GT(m1.redPixels, 0) << "weight-1 rendered NO pixels — Polyline technique is broken";
    ASSERT_GT(m2.redPixels, 0) << "weight-2 rendered NO pixels — Polyline technique is broken";

    // CORE: weight-2 must cover strictly more pixels than weight-1. Pre-port
    // (no glLineWidth), both were 1 px and this would be EQUAL — the regression
    // we're guarding against.
    EXPECT_GT(m2.redPixels, m1.redPixels)
        << "weight-2 did NOT render wider than weight-1 — thick-line mechanism broken";

    // IDEAL: weight-2 vertical span = 2, weight-1 vertical span = 1 (a 2-point
    // horizontal line through NDC y=0 in a 256-tall viewport lands on a pixel
    // boundary, so the 1-px vs 2-px split is unambiguous). Treat as a SOFT
    // expectation — rasterization edge-incidence can shift it by ±1 row, but
    // span(2) > span(1) must hold for the mechanism to be meaningful.
    EXPECT_GE(m2.verticalSpan, 2)
        << "weight-2 vertical span < 2 — expected at least 2 pixels wide";
    EXPECT_LE(m1.verticalSpan, 2)
        << "weight-1 vertical span > 2 — weight-1 should be the thin case";

    // AND the load-bearing inequality in span form:
    EXPECT_GT(m2.verticalSpan, m1.verticalSpan)
        << "weight-2 vertical span must exceed weight-1 — the 2px vs 1px claim";

    // Diagnostic line so the measured widths are in the test log permanently.
    std::fprintf(stderr,
        "  RESULT: weight-1 redPixels=%d span=%d ; weight-2 redPixels=%d span=%d ; "
        "ratio=%.2fx\n",
        m1.redPixels, m1.verticalSpan, m2.redPixels, m2.verticalSpan,
        m1.redPixels > 0 ? static_cast<double>(m2.redPixels) / static_cast<double>(m1.redPixels) : 0.0);
}

// Reproduces the ACS-label-invisibility root cause at the GL-pixel level.
//
// Authored: no reference test exists in itwinjs-core for headless GL render of
// the Polyline clip branch (itwinjs renders live via WebGL). The scenario +
// assertion mirror the user's live report ("X/Y labels don't show, everything
// else normal") and the code path that causes it.
//
// The ACS triad is a WorldOverlay decoration (GraphicType.WorldOverlay →
// RenderPass::WorldOverlay = 12). The Polyline shader's modelToWindowCoordinates
// (Viewport.ts:32-68, ported 1:1) takes its NO-CLIP branch only for
// kRenderPass_ViewOverlay(13) / Background(0); for WorldOverlay(12) it runs the
// front-plane clip branch, which returns w=0 (→ 0 fragments) when the segment's
// eye-space z is in front of -u_frustum.x. The Surface shader (arrow FILLS)
// never calls modelToWindowCoordinates, so fills render while Polyline outlines
// + labels vanish — exactly the reported symptom.
//
// Proves the clip gate and the near-sensitivity:
//   A) ViewOverlay (13), identity cam (q.z=0), near=1   → RENDERS (no-clip)
//   B) WorldOverlay (12), identity cam (q.z=0), near=1  → CLIPS  (q.z=0 > -1)
//   C) WorldOverlay (12), eye back -500 (q.z=-500), near=1   → RENDERS (-500≤-1)
//   D) WorldOverlay (12), eye back -500, near=1000      → CLIPS  (-500 > -1000)
// D reproduces the live bug shape: a too-large near (e.g. world-space z extracted
// from the full mvp instead of an eye-space near) makes visible overlay geometry
// fail the front-plane test and clip away.
TEST(PolylineRenderTest, WorldOverlayClipBranch) {
    test::GlslCompileResult probe = test::compileAndLinkProgram(
        "#version 410 core\nvoid main(){}",
        "#version 410 core\nvoid main(){}");
    if (!probe.contextAvailable) GTEST_SKIP() << "no offscreen GL context available";

    RenderEnv env;
    env.viewportW = 256;
    env.viewportH = 256;
    glGenFramebuffers(1, &env.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, env.fbo);
    glGenRenderbuffers(1, &env.rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, env.rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, env.viewportW, env.viewportH);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, env.rbo);
    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    env.program = compilePolylineProgram();
    ASSERT_NE(env.program, 0u) << "Polyline variant shader failed to compile+link";
    env.ok = true;

    float const ident[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    // Column-major translation(0,0,-500): the z=0 line's eye-space z becomes q.z=-500.
    float const mvBack[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-500,1};
    // Column-major ortho (left=-1,right=1,bottom=-1,top=1,near=1,far=1000) so a
    // translated-camera eye-space z (e.g. -500) maps into NDC[-1,1] instead of
    // being GPU-clipped. x/y pass through; only the z-row maps the frustum.
    // float[10]=-2/(f-n), float[14]=-(f+n)/(f-n); float[11]=0 (ortho sentinel).
    float const ortho[16] = {1,0,0,0, 0,1,0,0, 0,0,-2.0f/999.0f,0, 0,0,-1001.0f/999.0f,1};

    struct Cfg { float pass; float const* mv; float near_; float const* proj; char const* name; bool expect; };
    Cfg const cfgs[] = {
        {13.0f, ident,  1.0f,    ident, "ViewOverlay(13) no-clip, q.z=0, near=1",          true},
        {12.0f, ident,  1.0f,    ident, "WorldOverlay(12), q.z=0, near=1 (no camera MV)", false},
        {12.0f, mvBack, 1.0f,    ortho, "WorldOverlay(12), q.z=-500, near=1 (correct cam)",true},
        {12.0f, mvBack, 1000.0f, ortho, "WorldOverlay(12), q.z=-500, near=1000 (huge near)",false},
    };

    for (Cfg const& c : cfgs) {
        Measurement m = renderAndMeasure(env, 2.0f, c.pass, c.mv, c.near_, c.proj);
        std::fprintf(stderr,
            "  [clip-branch] %-58s redPixels=%d -> %s\n",
            c.name, m.redPixels, m.redPixels > 0 ? "RENDERED" : "clipped(0)");
        EXPECT_EQ(m.redPixels > 0, c.expect)
            << c.name << ": expected " << (c.expect ? "RENDERED" : "clipped");
    }

    glDeleteProgram(env.program);
    glDeleteRenderbuffers(1, &env.rbo);
    glDeleteFramebuffers(1, &env.fbo);
}

// ---------------------------------------------------------------------------
// Live driver-path repro: build the Polyline via the REAL MeshGraphic path
// (VertexLutTexture::create through a real OpenGLDriver + driver buffer/
// primitive APIs + PolylineGeometry::draw) — NOT the test's manual
// glTexImage2D/glGenVertexArrays. The live app emits 0 Polyline fragments
// despite everything verified correct (glReadPixels ground truth, Session 13
// memory); this test isolates whether the driver/geometry path itself
// reproduces 0 fragments headlessly (⇒ bug is in the driver path; bisect
// here) or renders fine (⇒ bug is in the compositor's FBO/pass routing for
// WorldOverlay, not the geometry path).
//
// Authored: no reference test exists in itwinjs-core/imodel-native for a
// headless real-driver Polyline render; this is a live-pipeline isolation test.
// ---------------------------------------------------------------------------
namespace {
// Minimal OpenGLPlatform over the harness's already-current CGL context.
class HeadlessCglPlatform : public rhi::OpenGLPlatform {
public:
    rhi::Driver* createDriver(void* sharedContext, rhi::DriverConfig const& config) override {
        return new rhi::OpenGLDriver(*this, sharedContext, config);
    }
    void terminate() noexcept override {}
    void* createSwapChain(void*, uint64_t) override { return nullptr; }
    void* createSwapChain(uint32_t, uint32_t, uint64_t) override { return nullptr; }
    void destroySwapChain(void*) noexcept override {}
    bool makeCurrent(void*, void*) override { return true; }  // CGL context already current
    void commit(void*) noexcept override {}
};
}  // namespace

TEST(PolylineRenderTest, LiveDriverPathRendersFragments)
{
    // Establish the offscreen GL context (side effect: leaves it current).
    test::GlslCompileResult probe = test::compileAndLinkProgram(
        "#version 410 core\nvoid main(){}",
        "#version 410 core\nvoid main(){}");
    if (!probe.contextAvailable) GTEST_SKIP() << "no offscreen GL context available";

    int const vw = 256, vh = 256;
    GLuint fbo = 0, cbo = 0, dsbo = 0;  // cbo = TEXTURE color attachment (m_renderTarget uses a texture)
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &cbo);
    glBindTexture(GL_TEXTURE_2D, cbo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vw, vh, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cbo, 0);
    // Attach depth+stencil (m_renderTarget uses TargetBufferFlags::ALL).
    glGenRenderbuffers(1, &dsbo);
    glBindRenderbuffer(GL_RENDERBUFFER, dsbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, vw, vh);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, dsbo);
    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    GLuint const program = compilePolylineProgram();
    ASSERT_NE(program, 0u) << "Polyline variant shader failed to compile+link";

    // Real OpenGLDriver over the current CGL context.
    HeadlessCglPlatform platform;
    rhi::OpenGLDriver driver(platform, nullptr, rhi::DriverConfig{});

    // Uniform setters (shared by both frames).
    float const ident[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    auto setMat4 = [&](char const* name, float const* m) {
        GLint l = glGetUniformLocation(program, name); if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, m); };
    auto setVec4 = [&](char const* name, float const* v) {
        GLint l = glGetUniformLocation(program, name); if (l >= 0) glUniform4fv(l, 1, v); };
    auto setVec3 = [&](char const* name, float const* v) {
        GLint l = glGetUniformLocation(program, name); if (l >= 0) glUniform3fv(l, 1, v); };
    auto setVec2 = [&](char const* name, float const* v) {
        GLint l = glGetUniformLocation(program, name); if (l >= 0) glUniform2fv(l, 1, v); };
    auto setF = [&](char const* name, float v) {
        GLint l = glGetUniformLocation(program, name); if (l >= 0) glUniform1f(l, v); };
    auto setI = [&](char const* name, int v) {
        GLint l = glGetUniformLocation(program, name); if (l >= 0) glUniform1i(l, v); };

    std::vector<Point3d> points = {Point3d::From(-0.4, 0.0, 0.0),
                                   Point3d::From( 0.4, 0.0, 0.0)};
    auto makeVbo = [&driver](uint8_t const* bytes, uint32_t n) {
        auto boh = driver.createBufferObject(n, rhi::BufferObjectBinding::VERTEX,
                                             rhi::BufferUsage::STATIC);
        rhi::BufferDescriptor desc(bytes, static_cast<size_t>(n));
        driver.updateBufferObject(boh, std::move(desc), 0);
        return boh;
    };

    // renderFrame: build a FRESH Polyline (the EXACT MeshGraphic path) → draw →
    // count line pixels. The PolylineGeometry + its GL resources (LUT/VBOs/VAO)
    // are destroyed when the lambda returns, mirroring the live triad's PER-FRAME
    // rebuild (Session 9). If frame 2 (fresh resources, recycled GL names, same
    // driver state tracker) drops to 0 while frame 1 renders, the per-frame
    // lifecycle + state-tracker cache is the bug.
    auto renderFrame = [&](int frameIdx) -> int {
        BuiltVertexTable vt = VertexTableBuilder::buildFromPolylines(
            points.data(), static_cast<uint32_t>(points.size()), ColorDef::red);
        VertexLutTexture lut;
        if (!lut.create(driver, vt.data.data(), vt.width, vt.height,
                        vt.numVertices, vt.numRgbaPerVertex)) return -1;
        TesselatedPolyline tess = PolylineTesselator::tesselate(
            points.data(), static_cast<uint32_t>(points.size()), 2.0f, false, false);
        uint32_t const numCorners = tess.numCorners();
        if (numCorners == 0) return -2;
        auto posVbo = makeVbo(tess.indices.data(), numCorners * 3u);
        auto prevVbo = makeVbo(tess.prevIndices.data(), numCorners * 3u);
        auto nextPropsVbo = makeVbo(tess.nextIndicesAndParams.data(), numCorners * 4u);
        rhi::AttributeArray attrs = {};
        attrs[0].buffer = 0; attrs[0].offset = 0; attrs[0].type = rhi::ElementType::UBYTE3;
        attrs[1].buffer = 1; attrs[1].offset = 0; attrs[1].type = rhi::ElementType::UBYTE3;
        attrs[2].buffer = 2; attrs[2].offset = 0; attrs[2].type = rhi::ElementType::UBYTE3;
        attrs[3].buffer = 2; attrs[3].offset = 3; attrs[3].type = rhi::ElementType::UBYTE;
        auto cornerVbih = driver.createVertexBufferInfo(3, 4, attrs);
        auto cornerVbh = driver.createVertexBuffer(numCorners, cornerVbih);
        driver.setVertexBufferObject(cornerVbh, 0, posVbo);
        driver.setVertexBufferObject(cornerVbh, 1, prevVbo);
        driver.setVertexBufferObject(cornerVbh, 2, nextPropsVbo);
        auto primitive = driver.createRenderPrimitive(
            cornerVbh, rhi::IndexBufferHandle{}, rhi::PrimitiveType::TRIANGLES);
        auto const lp = lut.getParams();
        PolylineGeometry geom(driver, std::move(lut), primitive,
                              cornerVbh, cornerVbih, posVbo, prevVbo, nextPropsVbo,
                              numCorners, 2.0f, ColorDef::red);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, vw, vh);
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glUseProgram(program);
        setMat4("u_mvp", ident); setMat4("u_mv", ident); setMat4("u_proj", ident);
        setMat4("u_viewportTransformation", ident);
        float const vp[2] = {static_cast<float>(vw), static_cast<float>(vh)};
        setVec2("u_viewport", vp);
        float const frustum[3] = {1.0f, 100.0f, 1.0f};
        setVec3("u_frustum", frustum);
        setF("u_renderPass", 13.0f);
        setF("u_lineWeight", 2.0f);
        setI("u_vertLUT", 0);
        float const vertParams[4] = {static_cast<float>(lp.texWidth), static_cast<float>(lp.texHeight),
                                     static_cast<float>(lp.numRgbaPerVert), static_cast<float>(lp.numVertices)};
        setVec4("u_vertParams", vertParams);
        float const lineColor[4] = {1.0f, 0.0f, 0.0f, 1.0f};
        setVec4("u_color", lineColor);
        float const qo[3] = {0, 0, 0}, qs[3] = {1, 1, 1};
        setVec3("u_qOrigin", qo);
        setVec3("u_qScale", qs);
        driver.bindTexture(0, geom.getLut().getTexture());
        geom.draw(driver);
        glFlush();

        int const y0 = 120, y1 = 136, bhh = y1 - y0;
        std::vector<unsigned char> px(static_cast<size_t>(vw) * bhh * 4u);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, y0, vw, bhh, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        int linePixels = 0;
        for (int y = 0; y < bhh; ++y)
            for (int x = 0; x < vw; ++x) {
                size_t o = static_cast<size_t>((y * vw + x) * 4);
                if (px[o] + px[o + 1] > 100) ++linePixels;
            }
        std::fprintf(stderr, "  [per-frame frame=%d] linePixels=%d\n", frameIdx, linePixels);
        return linePixels;
    };

    int const f1 = renderFrame(1);
    int const f2 = renderFrame(2);  // fresh GL resources (frame 1's destroyed at the lambda return)
    std::fprintf(stderr, "  [per-frame] f1=%d f2=%d\n", f1, f2);
    EXPECT_GT(f1, 0) << "frame 1 produced 0 (baseline broken)";
    EXPECT_GT(f2, 0) << "frame 2 (after destroy+recreate) produced 0 — per-frame lifecycle bug reproduced headlessly";

    glDeleteProgram(program);
    glDeleteTextures(1, &cbo);
    glDeleteRenderbuffers(1, &dsbo);
    glDeleteFramebuffers(1, &fbo);
}

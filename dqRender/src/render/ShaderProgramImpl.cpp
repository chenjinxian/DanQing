// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Shader program implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ShaderProgram.ts
#include "ShaderProgramImpl.h"
#include "RenderSystemDebugControl.h"  // debug-shaders 收集门/注册表（M-O(2) 3d）
#include "rhi/opengl/OpenGLDriver.h"
#include "rhi/opengl/OpenGLProgram.h"

#include <cassert>
#include <string>

BEGIN_DQ_RENDER_NAMESPACE

// ===========================================================================
// Uniform
// Ported from: itwinjs-core ShaderProgram.ts Uniform.compile() (line 44-52)
//   if (undefined !== prog.glProgram)
//     this._handle = UniformHandle.create(prog, this._name);
//   return this.isValid;
// ===========================================================================
bool Uniform::compile(ShaderProgram& prog)
{
    assert(!m_handle.has_value());
    if (prog.getGlProgram() != 0)
        m_handle = UniformHandle::create(static_cast<GLuint>(prog.getGlProgram()),
                                         m_name.c_str());
    return m_handle.has_value();
}

// ===========================================================================
// ProgramUniform
// Ported from: itwinjs-core ShaderProgram.ts ProgramUniform.bind() (line 76-80)
// ===========================================================================
void ProgramUniform::bind(ShaderProgramParams const& params)
{
    if (m_handle.has_value() && m_bind) {
        m_bind(m_handle.value(), params);
    }
}

// ===========================================================================
// GraphicUniform
// Ported from: itwinjs-core ShaderProgram.ts GraphicUniform.bind() (line 104-108)
// ===========================================================================
void GraphicUniform::bind(DrawParams const& params)
{
    if (m_handle.has_value() && m_bind) {
        m_bind(m_handle.value(), params);
    }
}

// ===========================================================================
// ShaderProgram
// ===========================================================================

// saveShaderCode 的文件名派生（M-O(2) 3d）——desc 剥阶段前缀、分隔符归一、
// 追加阶段后缀；返回注册表下标。
// Ported from: itwinjs-core ShaderProgram.saveShaderCode (:355-374).
static int saveShaderCodeForDebug(bool isVS, std::string const& desc,
                                  std::string const& src)
{
    auto& files = debugShaderFilesRegistry();
    std::string sname;
    auto replaceAll = [](std::string s, std::string const& from,
                         std::string const& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
        return s;
    };
    if (!desc.empty()) {
        sname = replaceAll(desc, isVS ? "//!V! " : "//!F! ", "");
        sname = replaceAll(sname, ": ", "-");
        sname = replaceAll(sname, "; ", "-");
    } else {
        // need to investigate shaders with no comments to derive names, for
        // now come up with unique name（:367-368 逐字语义）
        sname = "noname-" + std::to_string(files.size());
    }
    sname += isVS ? "_VS" : "_FS";
    DebugShaderFile entry;
    entry.filename = sname + ".glsl";
    entry.src = src;
    entry.isVS = isVS;
    entry.isGL = true;
    entry.isUsed = false;
    files.push_back(std::move(entry));
    return static_cast<int>(files.size()) - 1;
}

// ---------------------------------------------------------------------------
// setSource — set shader source code before compilation
// ---------------------------------------------------------------------------
void ShaderProgram::setSource(std::string vertSource, std::string fragSource,
                               std::string description)
{
    m_vertSource = std::move(vertSource);
    m_fragSource = std::move(fragSource);
    m_description = std::move(description);
    m_outputsToPick = m_description.find("Overrides") != std::string::npos
                  || m_description.find("Pick") != std::string::npos;
}

// ---------------------------------------------------------------------------
// setAttributeMap — set attribute name → location mapping
// ---------------------------------------------------------------------------
void ShaderProgram::setAttributeMap(std::unordered_map<std::string, int32_t> attrMap)
{
    m_attrMap = std::move(attrMap);
}

// ---------------------------------------------------------------------------
// addProgramUniform — register a program-level uniform binding
// Ported from: itwinjs-core ShaderProgram.addProgramUniform() (line 318-321)
// ---------------------------------------------------------------------------
void ShaderProgram::addProgramUniform(std::string name, BindProgramUniform binding)
{
    assert(isUncompiled());
    m_programUniforms.push_back(
        std::make_unique<ProgramUniform>(std::move(name), std::move(binding)));
}

// ---------------------------------------------------------------------------
// addGraphicUniform — register a per-draw-call uniform binding
// Ported from: itwinjs-core ShaderProgram.addGraphicUniform() (line 323-326)
// ---------------------------------------------------------------------------
void ShaderProgram::addGraphicUniform(std::string name, BindGraphicUniform binding)
{
    assert(isUncompiled());
    m_graphicUniforms.push_back(
        std::make_unique<GraphicUniform>(std::move(name), std::move(binding)));
}

// ---------------------------------------------------------------------------
// compile — compile and link the shader program
// Ported from: itwinjs-core ShaderProgram.compile() (line 255-285)
//
// compile-on-demand: called lazily on first use().
// Compiles vertex + fragment shaders, links program, resolves uniform locations.
// ---------------------------------------------------------------------------
CompileStatus ShaderProgram::compile(rhi::Driver& driver)
{
    // Already compiled or failed — return cached status.
    if (m_status == CompileStatus::Success)
        return CompileStatus::Success;
    if (m_status == CompileStatus::Failure)
        return CompileStatus::Failure;

    m_status = CompileStatus::Failure;

    // A program with no shader source has nothing to compile or link. Some
    // techniques register default-constructed ShaderPrograms that are never
    // drawn; driving those through glLinkProgram only produces Apple-GL
    // "Compiled vertex/fragment shader was corrupt" log noise (linking empty
    // stages always fails). Fail fast and silently — use() then returns false
    // and the draw is skipped, exactly as it would be after a real link failure.
    if (m_vertSource.empty() && m_fragSource.empty())
        return CompileStatus::Failure;

    // saveShaderCode（M-O(2) 3d）——debug-shaders 收集开启时把两段源码连同
    // 从 description 派生的文件名记入注册表（失败与否都记——compile 错误
    // 的取证正是该面存在的目的）。
    // Ported from: itwinjs-core ShaderProgram.saveShaderCode (:355-374——
    // desc 剥 "//!V! "/"//!F! " 前缀、": "/"； "→"-"、空 desc → noname-N、
    // 追加 _VS/_FS + .glsl；DanQing GL-only 单段）。索引存回成员供 use 标
    // isUsed（setDebugShaderUsage :337-353）。
    if (isDebugShadersEnabled()) {
        m_vertDebugNdx = saveShaderCodeForDebug(true, m_description, m_vertSource);
        m_fragDebugNdx = saveShaderCodeForDebug(false, m_description, m_fragSource);
    }

    // Build ONE rhi::Program carrying BOTH vertex + fragment sources, then create
    // the program once. The driver's createProgram (OpenGLDriver) extracts both
    // stages and compiles+links them together.
    //
    // Root-cause fix for the macOS Apple-GL "Compiled vertex/fragment shader was
    // corrupt" link failure: the previous per-stage compileShader() invoked
    // createProgram only on the VERTEX stage with a vertex-only Program, so the
    // fragment source never reached createProgram and OpenGLProgram::compile linked
    // an empty (no-main) fragment shader. rhi::Program stores sources per-stage
    // (Program.h m_shaderSource[SHADER_STAGE_COUNT]); a single Program must carry
    // every stage — matches filament (one Program per material).
    rhi::Program program;
    program.shader(rhi::ShaderStage::VERTEX, m_vertSource);
    program.shader(rhi::ShaderStage::FRAGMENT, m_fragSource);
    program.shaderLanguage(rhi::ShaderLanguage::ESSL3);
    program.name(m_description);
    for (auto const& [name, loc] : m_attrMap)
        program.attributeLocation(name, static_cast<uint8_t>(loc));

    // Create + link the program (createProgram compiles+links both stages).
    m_programHandle = driver.createProgram(std::move(program));
    if (!m_programHandle)
        return m_status;

    // Resolve attribute locations (glBindAttribLocation).
    if (!linkProgram(driver))
        return m_status;;

    // compile uniforms (resolve locations).
    for (auto& u : m_programUniforms) {
        if (!u->compile(*this))
            return m_status;
    }
    for (auto& u : m_graphicUniforms) {
        if (!u->compile(*this))
            return m_status;
    }

    m_status = CompileStatus::Success;

    // Clear source code after successful compilation (save memory).
    // Ported from: itwinjs-core ShaderProgram.compile() line 281-282
    m_vertSource.clear();
    m_fragSource.clear();

    return m_status;
}

// ---------------------------------------------------------------------------
// ensureCompiled — force compilation if not yet compiled
// ---------------------------------------------------------------------------
CompileStatus ShaderProgram::ensureCompiled(rhi::Driver& driver)
{
    if (m_status == CompileStatus::Uncompiled)
        return compile(driver);
    return m_status;
}

// ---------------------------------------------------------------------------
// use — bind program and invoke program uniform bindings
// Ported from: itwinjs-core ShaderProgram.use() (line 287-303)
// ---------------------------------------------------------------------------
bool ShaderProgram::use(rhi::Driver& driver, ShaderProgramParams const& params)
{
    if (compile(driver) != CompileStatus::Success)
        return false;

    assert(!m_inUse);
    m_inUse = true;

    // setDebugShaderUsage（M-O(2) 3d）——程序激活即标已用（isUsed 供
    // OutputShaders 的 u/n 过滤面）。
    // Ported from: itwinjs-core ShaderProgram.setDebugShaderUsage (:337-353)。
    if (isDebugShadersEnabled()) {
        auto& files = debugShaderFilesRegistry();
        if (m_vertDebugNdx >= 0 && m_vertDebugNdx < static_cast<int>(files.size()))
            files[static_cast<size_t>(m_vertDebugNdx)].isUsed = true;
        if (m_fragDebugNdx >= 0 && m_fragDebugNdx < static_cast<int>(files.size()))
            files[static_cast<size_t>(m_fragDebugNdx)].isUsed = true;
    }

    // Bind the program ONLY. Ported from: itwinjs-core ShaderProgram.use() —
    // gl.useProgram + program uniforms, NO render-state changes. The previous
    // bindPipeline(default PipelineState) applied the filament-default
    // RasterState (culling=BACK, depth on) on every use(), stomping the
    // compositor's itwinjs RenderState — GL_CULL_FACE left enabled culled
    // every solid-primitive surface mesh (reversed winding) while planar
    // shapes (CCW front faces) survived.
    driver.useProgram(m_programHandle);

    // Invoke program-level uniform bindings (once per use).
    for (auto& uniform : m_programUniforms)
        uniform->bind(params);

    return true;
}

// ---------------------------------------------------------------------------
// endUse — unbind program
// Ported from: itwinjs-core ShaderProgram.endUse() (line 305-308)
// ---------------------------------------------------------------------------
void ShaderProgram::endUse(rhi::Driver& /*driver*/)
{
    m_inUse = false;
    // Note: in OpenGL, we don't need to call gl.useProgram(null) explicitly.
    // The next use() call will bind a different program.
}

// releaseGlProgram — destroy GL program + reset compile cache (see .h).
void ShaderProgram::releaseGlProgram(rhi::Driver& driver)
{
    if (m_programHandle) {
        driver.destroyProgram(m_programHandle);
        m_programHandle = rhi::ProgramHandle();
    }
    // Reference dispose(): _glProgram = undefined — the cached GL handle dies
    // with the program.
    m_glProgram = 0;
    // 重置编译缓存——否则 compile() 缓存 Success 早退而句柄已销毁，use() 把死
    // 句柄交给 driver.useProgram（handle_cast 落空静默跳过 glUseProgram）。
    m_status = CompileStatus::Uncompiled;
}

// ---------------------------------------------------------------------------
// draw — invoke graphic uniform bindings, then draw geometry
// Ported from: itwinjs-core ShaderProgram.draw() (line 310-316)
// ---------------------------------------------------------------------------
void ShaderProgram::draw(DrawParams const& params)
{
    assert(m_inUse);

    // Invoke per-draw-call uniform bindings.
    for (auto& uniform : m_graphicUniforms)
        uniform->bind(params);

    // Geometry draw is handled by the caller (SceneCompositor::drawPass).
}

// ---------------------------------------------------------------------------
// linkProgram — link the compiled program
// ---------------------------------------------------------------------------
bool ShaderProgram::linkProgram(rhi::Driver& driver)
{
    // The RHI driver handles linking internally during createProgram.
    // We just need to verify the program is valid.
    if (!m_programHandle)
        return false;

    // Resolve attribute locations.
    // Ported from: itwinjs-core ShaderProgram.linkProgram() (line 206-211)
    // Uses glBindAttribLocation before final linking.
    auto* glDriver = static_cast<rhi::OpenGLDriver*>(&driver);
    if (glDriver) {
        auto* glProg = glDriver->resolveProgram(m_programHandle);
        if (glProg) {
            // Cache the raw GL handle — Uniform::compile resolves uniform
            // locations against it (reference: prog.glProgram property).
            m_glProgram = glProg->getProgram();
            GLuint glHandle = glProg->getProgram();
            for (auto const& [name, location] : m_attrMap) {
                glBindAttribLocation(glHandle, static_cast<GLuint>(location), name.c_str());
            }
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// compileUniforms — compile uniform list (resolve locations)
// Ported from: itwinjs-core ShaderProgram.compileUniforms() (line 328-335)
// ---------------------------------------------------------------------------
bool ShaderProgram::compileUniforms(std::vector<std::unique_ptr<Uniform>>& uniforms)
{
    for (auto& uniform : uniforms) {
        if (!uniform->compile(*this))
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// uploadUniforms — legacy uniform upload via ShaderProgramParams
// ---------------------------------------------------------------------------
void ShaderProgram::uploadUniforms(rhi::Driver& driver,
                                    ShaderProgramParams const& params)
{
    auto* glDriver = static_cast<rhi::OpenGLDriver*>(&driver);
    if (!glDriver) return;

    auto* glProg = glDriver->resolveProgram(m_programHandle);
    if (!glProg) return;

    for (auto const& [name, uv] : params.getUniforms()) {
        auto it = m_uniformCache.find(name);
        if (it == m_uniformCache.end()) {
            CachedUniform cu;
            cu.location = glProg->getUniformLocation(name.c_str());
            if (cu.location < 0) continue;
            m_uniformCache[name] = cu;
        }

        auto& cu = m_uniformCache[name];
        if (cu.location < 0) continue;

        switch (uv.type) {
            case UniformValue::Type::Mat4:
                if (cu.handle.setMatrix4(uv.floatData.data()))
                    glUniformMatrix4fv(cu.location, 1, GL_FALSE, uv.floatData.data());
                break;
            // Mat3 — u_normalMatrix (transpose(inverse(mat3(mv))), Vertex.ts
            // addNormalMatrix). A zero matrix normalize()s to NaN and blacks out
            // every lit surface (glTF texture-on-screen regression).
            case UniformValue::Type::Mat3:
                if (cu.handle.setMatrix3(uv.floatData.data()))
                    glUniformMatrix3fv(cu.location, 1, GL_FALSE, uv.floatData.data());
                break;
            case UniformValue::Type::Vec4:
                if (cu.handle.setUniform4fv(uv.floatData.data()))
                    glUniform4fv(cu.location, 1, uv.floatData.data());
                break;
            case UniformValue::Type::Vec3:
                if (cu.handle.setUniform3fv(uv.floatData.data()))
                    glUniform3fv(cu.location, 1, uv.floatData.data());
                break;
            case UniformValue::Type::Vec2:
                if (cu.handle.setUniform2fv(uv.floatData.data()))
                    glUniform2fv(cu.location, 1, uv.floatData.data());
                break;
            case UniformValue::Type::Float:
                if (cu.handle.setUniform1fv(uv.floatData.data(), uv.arrayCount))
                    glUniform1fv(cu.location, uv.arrayCount, uv.floatData.data());
                break;
            case UniformValue::Type::Int:
                if (cu.handle.setUniform1i(uv.intData))
                    glUniform1i(cu.location, uv.intData);
                break;
            // IntArray — glUniform1iv on the array (e.g. u_surfaceFlags[12],
            // bool[] in GLSL uploads via the int path, matching the reference's
            // uniform.setUniform1iv → gl.uniform1iv — Surface.ts:524).
            case UniformValue::Type::IntArray:
                if (cu.handle.setUniform1iv(uv.intDataArray.data(), static_cast<size_t>(uv.arrayCount)))
                    glUniform1iv(cu.location, uv.arrayCount, uv.intDataArray.data());
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// GL-free test accessors
// Authored: itwinjs-core has no GL-free binding test path; these let GoogleTest
// verify binding registration + lambda logic without compiling (no driver/GL).
// ---------------------------------------------------------------------------
bool ShaderProgram::hasProgramUniform(std::string const& name) const
{
    for (auto const& u : m_programUniforms)
        if (u->getName() == name)
            return true;
    return false;
}

bool ShaderProgram::invokeProgramUniformForTest(std::string const& name, UniformHandle& handle,
                                                ShaderProgramParams const& params) const
{
    for (auto const& u : m_programUniforms) {
        if (u->getName() == name) {
            u->bindForTest(handle, params);
            return true;
        }
    }
    return false;
}

// Authored: GL-free GraphicUniform test accessors (mirrors the ProgramUniform
// pair above); itwinjs-core has no GL-free binding test path.
bool ShaderProgram::hasGraphicUniform(std::string const& name) const
{
    for (auto const& u : m_graphicUniforms)
        if (u->getName() == name)
            return true;
    return false;
}

bool ShaderProgram::invokeGraphicUniformForTest(std::string const& name, UniformHandle& handle,
                                                DrawParams const& params) const
{
    for (auto const& u : m_graphicUniforms) {
        if (u->getName() == name) {
            u->bindForTest(handle, params);
            return true;
        }
    }
    return false;
}

END_DQ_RENDER_NAMESPACE

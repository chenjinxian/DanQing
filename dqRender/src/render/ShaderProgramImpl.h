// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Shader program implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ShaderProgram.ts
//
// Wraps an OpenGL program with compiled vertex + fragment shaders.
// Manages uniform binding via ProgramUniform (per-use) and GraphicUniform
// (per-draw) callback systems, matching itwinjs-core exactly.
#pragma once

#include "UniformHandle.h"
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"
#include "dqRender/rhi/Program.h"

#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class RenderState;
class DrawParams;
class ShaderProgram;
class TargetImpl;

// ---------------------------------------------------------------------------
// UniformValue — cached uniform data for upload (legacy path)
// ---------------------------------------------------------------------------
struct UniformValue {
    enum class Type : uint8_t { Float, Vec2, Vec3, Vec4, Mat4, Mat3, Int, IntArray };
    Type type = Type::Float;
    std::array<float, 16> floatData = {};
    int intData = 0;
    // IntArray storage (e.g. u_surfaceFlags[SurfaceBitIndex.Count] = 12). 16 =
    // UniformHandle::m_dataInt capacity.
    std::array<int, 16> intDataArray = {};
    int arrayCount = 1;
};

// ---------------------------------------------------------------------------
// CachedUniform — per-uniform dirty tracking with cached handle (legacy path)
// ---------------------------------------------------------------------------
struct CachedUniform {
    UniformHandle handle;
    int32_t location = -1;
};

// ---------------------------------------------------------------------------
// ShaderProgramParams — named uniform values (legacy path)
// Used by SceneCompositor for batch uniform upload.
// ---------------------------------------------------------------------------
class ShaderProgramParams {
public:
    void setMatrix4(char const* name, float const* data) {
        auto& uv = m_uniforms[name];
        uv.type = UniformValue::Type::Mat4;
        std::memcpy(uv.floatData.data(), data, 16 * sizeof(float));
    }
    void setMatrix3(char const* name, float const* data) {
        auto& uv = m_uniforms[name];
        uv.type = UniformValue::Type::Mat3;
        std::memcpy(uv.floatData.data(), data, 9 * sizeof(float));
    }
    void setFloat(char const* name, float value) {
        auto& uv = m_uniforms[name];
        uv.type = UniformValue::Type::Float;
        uv.floatData[0] = value;
        uv.arrayCount = 1;
    }
    void setFloatArray(char const* name, float const* data, int count) {
        auto& uv = m_uniforms[name];
        uv.type = UniformValue::Type::Float;
        std::memcpy(uv.floatData.data(), data, static_cast<size_t>(count) * sizeof(float));
        uv.arrayCount = count;
    }
    void setVec2(char const* name, float const* data) {
        auto& uv = m_uniforms[name];
        uv.type = UniformValue::Type::Vec2;
        std::memcpy(uv.floatData.data(), data, 2 * sizeof(float));
    }
    void setVec3(char const* name, float const* data) {
        auto& uv = m_uniforms[name];
        uv.type = UniformValue::Type::Vec3;
        std::memcpy(uv.floatData.data(), data, 3 * sizeof(float));
    }
    void setVec4(char const* name, float const* data) {
        auto& uv = m_uniforms[name];
        uv.type = UniformValue::Type::Vec4;
        std::memcpy(uv.floatData.data(), data, 4 * sizeof(float));
    }
    void setInt(char const* name, int value) {
        auto& uv = m_uniforms[name];
        uv.type = UniformValue::Type::Int;
        uv.intData = value;
    }
    void setIntArray(char const* name, int const* data, int count) {
        auto& uv = m_uniforms[name];
        uv.type = UniformValue::Type::IntArray;
        std::memcpy(uv.intDataArray.data(), data, static_cast<size_t>(count) * sizeof(int));
        uv.arrayCount = count;
    }
    std::unordered_map<std::string, UniformValue> const& getUniforms() const { return m_uniforms; }
    bool isEmpty() const { return m_uniforms.empty(); }

    /// The target this draw is for (faithful: reference params.target; lets
    /// ProgramUniform binding callbacks read target.uniforms.*). Nullptr on the
    /// legacy/test path (callers without a TargetImpl).
    /// Ported from: itwinjs-core ShaderProgramParams.target
    TargetImpl* getTarget() const noexcept { return m_target; }
    void setTarget(TargetImpl* t) noexcept { m_target = t; }

private:
    std::unordered_map<std::string, UniformValue> m_uniforms;
    TargetImpl* m_target = nullptr;
};

// ---------------------------------------------------------------------------
// CompileStatus — result of shader compilation
// Ported from: itwinjs-core ShaderProgram.ts CompileStatus (line 114-118)
// ---------------------------------------------------------------------------
enum class CompileStatus : uint8_t {
    Success,
    Failure,
    Uncompiled,
};

// ---------------------------------------------------------------------------
// BindProgramUniform — callback for program-level uniforms
// Ported from: itwinjs-core ShaderProgram.ts BindProgramUniform (line 61)
//
// Invoked once each time the shader becomes active.
// Responsible for setting the value of the uniform.
// ---------------------------------------------------------------------------
using BindProgramUniform = std::function<void(UniformHandle&, ShaderProgramParams const&)>;

// ---------------------------------------------------------------------------
// BindGraphicUniform — callback for per-draw-call uniforms
// Ported from: itwinjs-core ShaderProgram.ts BindGraphicUniform (line 88)
//
// Invoked once for each graphic primitive rendered with this program.
// Responsible for setting the value of the uniform.
// ---------------------------------------------------------------------------
using BindGraphicUniform = std::function<void(UniformHandle&, DrawParams const&)>;

// ---------------------------------------------------------------------------
// Uniform — base class for uniform variable descriptors
// Ported from: itwinjs-core ShaderProgram.ts Uniform (line 38-54)
// ---------------------------------------------------------------------------
class Uniform {
public:
    explicit Uniform(std::string name) : m_name(std::move(name)) {}
    virtual ~Uniform() = default;

    /// compile the uniform (resolve location from program).
    /// Ported from: itwinjs-core Uniform.compile()
    bool compile(ShaderProgram& prog);

    bool isValid() const noexcept { return m_handle.has_value(); }
    std::string const& getName() const noexcept { return m_name; }
    UniformHandle& getHandle() { return m_handle.value(); }

protected:
    std::string m_name;
    std::optional<UniformHandle> m_handle;
};

// ---------------------------------------------------------------------------
// ProgramUniform — uniform bound once per use() call
// Ported from: itwinjs-core ShaderProgram.ts ProgramUniform (line 68-81)
//
// The binding function is invoked once each time the shader becomes active.
// Used for values that don't change between draw calls (view matrix, lights).
// ---------------------------------------------------------------------------
class ProgramUniform : public Uniform {
public:
    ProgramUniform(std::string name, BindProgramUniform bind)
        : Uniform(std::move(name)), m_bind(std::move(bind)) {}

    /// Invoke the binding callback.
    /// Ported from: itwinjs-core ProgramUniform.bind()
    void bind(ShaderProgramParams const& params);

    /// GL-free test helper: invoke the bind callback against an external handle
    /// (bypasses the compiled m_handle, so no GL/compile needed).
    void bindForTest(UniformHandle& handle, ShaderProgramParams const& params) const
    {
        if (m_bind)
            m_bind(handle, params);
    }

private:
    BindProgramUniform m_bind;
};

// ---------------------------------------------------------------------------
// GraphicUniform — uniform bound per draw call
// Ported from: itwinjs-core ShaderProgram.ts GraphicUniform (line 96-109)
//
// The binding function is invoked once for each graphic primitive rendered.
// Used for per-object values (model matrix, feature ID, color).
// ---------------------------------------------------------------------------
class GraphicUniform : public Uniform {
public:
    GraphicUniform(std::string name, BindGraphicUniform bind)
        : Uniform(std::move(name)), m_bind(std::move(bind)) {}

    /// Invoke the binding callback.
    /// Ported from: itwinjs-core GraphicUniform.bind()
    void bind(DrawParams const& params);

    /// GL-free test helper: invoke the bind callback against an external handle
    /// (bypasses the compiled m_handle, so no GL/compile needed).
    void bindForTest(UniformHandle& handle, DrawParams const& params) const
    {
        if (m_bind)
            m_bind(handle, params);
    }

private:
    BindGraphicUniform m_bind;
};

// ---------------------------------------------------------------------------
// ShaderProgram — compiled shader program with uniform binding
// Ported from: itwinjs-core ShaderProgram.ts ShaderProgram (line 122-335)
//
// Key behaviors matching itwinjs-core:
// - compile-on-demand: compile() called lazily on first use()
// - _inUse tracking: asserts no nested use()
// - ProgramUniform bindings invoked once per use()
// - GraphicUniform bindings invoked per draw()
// - addProgramUniform/addGraphicUniform register callbacks before compile
// ---------------------------------------------------------------------------
class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram() = default;

    ShaderProgram(ShaderProgram const&) = delete;
    ShaderProgram& operator=(ShaderProgram const&) = delete;
    ShaderProgram(ShaderProgram&&) = default;
    ShaderProgram& operator=(ShaderProgram&&) = default;

    // --- Construction (before compile) ---

    /// Set the vertex and fragment source code.
    void setSource(std::string vertSource, std::string fragSource,
                   std::string description);

    /// Set the attribute map (attribute name → location).
    void setAttributeMap(std::unordered_map<std::string, int32_t> attrMap);

    /// Register a program-level uniform binding (invoked once per use()).
    /// Must be called before compile().
    /// Ported from: itwinjs-core ShaderProgram.addProgramUniform()
    void addProgramUniform(std::string name, BindProgramUniform binding);

    /// Register a per-draw-call uniform binding (invoked per draw()).
    /// Must be called before compile().
    /// Ported from: itwinjs-core ShaderProgram.addGraphicUniform()
    void addGraphicUniform(std::string name, BindGraphicUniform binding);

    // --- GL-free test accessors (no compile/driver needed) ---
    // Authored: itwinjs-core has no GL-free binding test path (it tests live);
    // these let GoogleTest verify binding registration + lambda logic without GL.

    /// True if a ProgramUniform with this name is registered.
    bool hasProgramUniform(std::string const& name) const;

    /// Invoke the named ProgramUniform's bind callback against `handle` (GL-free).
    /// Returns false if not registered.
    bool invokeProgramUniformForTest(std::string const& name, UniformHandle& handle,
                                     ShaderProgramParams const& params) const;

    /// True if a GraphicUniform with this name is registered.
    bool hasGraphicUniform(std::string const& name) const;

    /// Invoke the named GraphicUniform's bind callback against `handle` (GL-free).
    /// Returns false if not registered.
    bool invokeGraphicUniformForTest(std::string const& name, UniformHandle& handle,
                                     DrawParams const& params) const;

    // --- Compilation ---

    /// compile and link the program (lazy — called automatically on first use).
    /// Ported from: itwinjs-core ShaderProgram.compile()
    CompileStatus compile(rhi::Driver& driver);

    /// Backward-compatible overload: set source and compile in one call.
    CompileStatus compile(rhi::Driver& driver, std::string const& vertSource,
                          std::string const& fragSource, std::string const& description)
    {
        setSource(vertSource, fragSource, description);
        return compile(driver);
    }

    /// Force compilation if not yet compiled.
    CompileStatus ensureCompiled(rhi::Driver& driver);

    // --- Usage ---

    /// Bind the program and invoke program uniform bindings.
    /// Returns false if compilation failed.
    /// Ported from: itwinjs-core ShaderProgram.use()
    bool use(rhi::Driver& driver, ShaderProgramParams const& params);

    /// Unbind the program.
    /// Ported from: itwinjs-core ShaderProgram.endUse()
    void endUse(rhi::Driver& driver);

    /// Draw with this program (invoke graphic uniform bindings, then draw geometry).
    /// Ported from: itwinjs-core ShaderProgram.draw()
    void draw(DrawParams const& params);

    // --- Queries ---

    bool isValid() const noexcept { return static_cast<bool>(m_programHandle); }
    bool isCompiled() const noexcept { return m_status == CompileStatus::Success; }
    bool isUncompiled() const noexcept { return m_status == CompileStatus::Uncompiled; }
    bool isInUse() const noexcept { return m_inUse; }
    bool outputsToPick() const noexcept { return m_outputsToPick; }

    /// The raw GL program handle (0 before compile and after releaseGlProgram).
    /// Ported from: itwinjs-core ShaderProgram.ts glProgram (line 168) — the
    /// reference exposes it so Uniform.compile → UniformHandle.create can
    /// resolve uniform locations against the linked program.
    uint32_t getGlProgram() const noexcept { return m_glProgram; }

    rhi::ProgramHandle getHandle() const noexcept { return m_programHandle; }
    std::string const& getDescription() const noexcept { return m_description; }

    /// Destroy the GL program (if any) and reset the compile cache so a later
    /// use() recompiles from source. Context-lifetime programs normally never
    /// need this (itwinjs programs live with the WebGL context — Techniques own
    /// them); only owning-container teardown (~SceneCompositor) uses it, to avoid
    /// leaking a GL program per destroyed compositor.
    /// Authored: lifecycle glue — itwinjs never destroys + recreates a program on
    ///           the same ShaderProgram object, so there is no reference for the
    ///           reset semantics (the resize-path use-after-destroy bug of
    ///           2026-09-14 is what this reset guards against).
    void releaseGlProgram(rhi::Driver& driver);

    std::string const& getVertSource() const noexcept { return m_vertSource; }
    std::string const& getFragSource() const noexcept { return m_fragSource; }

    // --- Legacy compatibility ---

    /// Upload uniforms from ShaderProgramParams (legacy path).
    /// Prefer addProgramUniform/addGraphicUniform callbacks instead.
    void uploadUniforms(rhi::Driver& driver, ShaderProgramParams const& params);

private:
    bool linkProgram(rhi::Driver& driver);
    bool compileUniforms(std::vector<std::unique_ptr<Uniform>>& uniforms);

    rhi::ProgramHandle m_programHandle;
    // Raw GL program handle (resolveProgram(getProgram)) cached at link time so
    // Uniform::compile can resolve uniform locations without re-resolving the
    // RHI handle per uniform.  Ported from: ShaderProgram._glProgram.
    uint32_t m_glProgram = 0;
    std::string m_vertSource;
    std::string m_fragSource;
    std::string m_description;
    CompileStatus m_status = CompileStatus::Uncompiled;
    bool m_inUse = false;
    bool m_outputsToPick = false;

    // Attribute map: attribute name → location
    std::unordered_map<std::string, int32_t> m_attrMap;

    // Program-level uniforms (bound once per use()).
    // Ported from: itwinjs-core ShaderProgram._programUniforms
    std::vector<std::unique_ptr<ProgramUniform>> m_programUniforms;

    // Per-draw-call uniforms (bound per draw()).
    // Ported from: itwinjs-core ShaderProgram._graphicUniforms
    std::vector<std::unique_ptr<GraphicUniform>> m_graphicUniforms;

    // Legacy: uniform cache for dirty tracking (used by uploadUniforms).
    std::unordered_map<std::string, CachedUniform> m_uniformCache;
};

END_DQ_RENDER_NAMESPACE

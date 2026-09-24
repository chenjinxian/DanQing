// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GLSL shader source assembly
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ShaderBuilder.ts
//
// Assembles GLSL source code from modular components (vertex shader builder +
// fragment shader builder).  Each builder collects variable declarations,
// functions, macros, and component code snippets, then concatenates them into
// a complete GLSL source string.
//
// Phase 2: adds composable component slots (VertexShaderComponent /
// FragmentShaderComponent) matching itwinjs's modular shader assembly.
#pragma once

#include <array>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// VariableType — GLSL variable types
// Ported from: itwinjs-core ShaderBuilder.ts VariableType enum (line 22-36)
// ---------------------------------------------------------------------------
enum class VariableType : uint8_t {
    Boolean,
    Int,
    Float,
    Vec2,
    Vec3,
    Vec4,
    Mat3,
    Mat4,
    Sampler2D,
    SamplerCube,
    Uint,
    BVec2,
    Count,
};

// ---------------------------------------------------------------------------
// VariableScope — where a variable is declared
// Ported from: itwinjs-core ShaderBuilder.ts VariableScope enum (line 41-47)
// ---------------------------------------------------------------------------
enum class VariableScope : uint8_t {
    Global,     // no qualifier
    Varying,    // varying (out in vert, in in frag)
    Uniform,    // uniform
    Attribute,  // in (vertex input) — GLSL 330 uses "in" instead of "attribute"
    Count,
};

// ---------------------------------------------------------------------------
// VariablePrecision — declared precision of a shader variable
// Ported from: itwinjs-core ShaderBuilder.ts VariablePrecision enum (line 52-58)
// ---------------------------------------------------------------------------
enum class VariablePrecision : uint8_t {
    Default,    // undeclared precision
    Low,        // lowp
    Medium,     // mediump
    High,       // highp
    Count,
};

// ---------------------------------------------------------------------------
// ShaderStage — which shader stage is being built
// ---------------------------------------------------------------------------
enum class ShaderStage : uint8_t {
    Vertex,
    Fragment,
};

// Forward declaration for binding callback
class ShaderProgram;

// Binding callback type — invoked to bind a variable to a compiled program.
// Ported from: itwinjs-core ShaderBuilder.ts addVariableBinding (line 113)
using addVariableBinding = void(*)(ShaderProgram&);

// ---------------------------------------------------------------------------
// ShaderVariable — a declared GLSL variable
// Ported from: itwinjs-core ShaderBuilder.ts ShaderVariable class (line 119-190)
//
// Supports: type, scope, precision, const, array, binding callback, value.
// ---------------------------------------------------------------------------
class ShaderVariable {
public:
    /// Backward-compatible constructor (matches old struct aggregate init).
    /// Parameters: name, type, scope, arraySize (0 = not an array).
    ShaderVariable(std::string name, VariableType type, VariableScope scope,
                   int arraySize = 0)
        : m_name(std::move(name))
        , m_type(type)
        , m_scope(scope)
        , m_arrayLength(arraySize)
    {
    }

    /// Create a standard variable.
    static ShaderVariable create(std::string name, VariableType type,
                                  VariableScope scope,
                                  addVariableBinding binding = nullptr,
                                  VariablePrecision precision = VariablePrecision::Default);

    /// Create an array variable.
    static ShaderVariable createArray(std::string name, VariableType type,
                                       int length, VariableScope scope,
                                       addVariableBinding binding = nullptr,
                                       VariablePrecision precision = VariablePrecision::Default);

    /// Create a global (const or non-const) variable.
    static ShaderVariable createGlobal(std::string name, VariableType type,
                                        std::string value = "", bool isConst = false);

    std::string const& getName() const noexcept { return m_name; }
    VariableType getType() const noexcept { return m_type; }
    VariableScope getScope() const noexcept { return m_scope; }
    VariablePrecision getPrecision() const noexcept { return m_precision; }
    bool isConst() const noexcept { return m_isConst; }
    int getArrayLength() const noexcept { return m_arrayLength; }
    bool hasBinding() const noexcept { return m_binding != nullptr; }

    /// Invoke the binding callback to register the variable with a ShaderProgram.
    void addBinding(ShaderProgram& prog) const;

    /// Get the GLSL type name string.
    std::string_view getTypeName() const;

    /// Get the GLSL scope qualifier string.
    std::string_view getScopeName(bool isVertexShader) const;

    /// Get the GLSL precision qualifier string.
    std::string_view getPrecisionName() const;

    /// Build the single-line GLSL declaration.
    std::string buildDeclaration(bool isVertexShader) const;

private:
    ShaderVariable() = default;  // Used by static factory methods

    std::string m_name;
    std::string m_value;          // for global variables only
    VariableType m_type = VariableType::Float;
    VariableScope m_scope = VariableScope::Global;
    VariablePrecision m_precision = VariablePrecision::Default;
    addVariableBinding m_binding = nullptr;
    int m_arrayLength = 0;        // 0 = not an array
    bool m_isConst = false;       // for global variables only
};

// ---------------------------------------------------------------------------
// ShaderVariables — collection of variables for a shader stage
// Ported from: itwinjs-core ShaderBuilder.ts ShaderVariables class (line 197-400)
//
// Manages variable declarations, dedup, scope-ordered output, and binding.
// ---------------------------------------------------------------------------
class ShaderVariables {
public:
    ShaderVariables() = default;

    /// Find an existing variable by name. Returns nullptr if not found.
    ShaderVariable const* find(std::string const& name) const;

    /// add a variable (dedup by name). Returns true if added.
    bool addVariable(ShaderVariable var);

    /// add a uniform variable with binding callback.
    void addUniform(std::string name, VariableType type,
                    addVariableBinding binding,
                    VariablePrecision precision = VariablePrecision::Default);

    /// add a uniform array with binding callback.
    void addUniformArray(std::string name, VariableType type, int length,
                          addVariableBinding binding);

    /// add a varying variable.
    bool addVarying(std::string name, VariableType type);

    /// add a global variable.
    void addGlobal(std::string name, VariableType type,
                   std::string value = "", bool isConst = false);

    /// add a constant (const global).
    void addConstant(std::string name, VariableType type, std::string value);

    /// add a bit flag constant (uint with value 2^flag).
    void addBitFlagConstant(std::string name, int value);

    /// Build declarations for a specific scope.
    std::string buildScopeDeclarations(bool isVertexShader, VariableScope scope,
                                        bool constantsOnly = false) const;

    /// Build all variable declarations (ordered by scope).
    std::string buildDeclarations(bool isVertexShader) const;

    /// Invoke binding callbacks for all variables with bindings.
    void addBindings(ShaderProgram& prog, ShaderVariables const* predefined = nullptr) const;

    /// Get the number of variables.
    size_t getSize() const noexcept { return m_variables.size(); }

private:
    std::vector<ShaderVariable> m_variables;
};

// ---------------------------------------------------------------------------
// VertexShaderComponent — composable vertex shader slots
// Ported from: itwinjs-core ShaderBuilder.ts VertexShaderComponent enum (line 631-649)
//
// Each slot maps to a GLSL function called by name in main().  The buildSource()
// method wraps each slot body into a function definition and assembles main()
// with the exact call chain from the reference.
// ---------------------------------------------------------------------------
enum class VertexShaderComponent : uint8_t {
    ComputeQuantizedPosition,                    // 0  vec4 computeQuantizedPosition()
    AdjustRawPosition,                           // 1  vec4 adjustRawPosition(vec4 rawPos)
    CheckForEarlyDiscard,                        // 2  bool checkForEarlyDiscard(vec4 rawPos)
    ComputeFeatureOverrides,                     // 3  void computeFeatureOverrides()
    ComputeMaterial,                             // 4  void computeMaterial()
    ComputeBaseColor,                            // 5  vec4 computeBaseColor()
    ApplyMaterialColor,                          // 6  vec4 applyMaterialColor(vec4 baseColor)
    ApplyFeatureColor,                           // 7  vec4 applyFeatureColor(vec4 baseColor)
    AdjustContrast,                              // 8  vec4 adjustContrast(vec4 baseColor)
    CheckForDiscard,                             // 9  bool checkForDiscard()
    ComputePosition,                             // 10 vec4 computePosition(vec4 rawPos) [required]
    ComputeAtmosphericScatteringVaryings,        // 11 void computeAtmosphericScatteringVaryings()
    CheckForLateDiscard,                         // 12 bool checkForLateDiscard()
    FinalizePosition,                            // 13 vec4 finalizePosition(vec4 pos)
    Count
};

// ---------------------------------------------------------------------------
// FragmentShaderComponent — composable fragment shader slots
// Ported from: itwinjs-core ShaderBuilder.ts FragmentShaderComponent enum (line 655-694)
//
// Each slot maps to a GLSL function called by name in main().  The buildSource()
// method wraps each slot body into a function definition and assembles main()
// with the exact call chain from the reference, including the clipping block.
// ---------------------------------------------------------------------------
enum class FragmentShaderComponent : uint8_t {
    CheckForEarlyDiscard,           // 0  bool checkForEarlyDiscard()
    ComputeBaseColor,               // 1  vec4 computeBaseColor() [required]
    ApplyMaterialOverrides,         // 2  vec4 applyMaterialOverrides(vec4 baseColor)
    FinalizeBaseColor,              // 3  vec4 finalizeBaseColor(vec4 baseColor)
    CheckForDiscard,                // 4  bool checkForDiscard(vec4 baseColor)
    DiscardByAlpha,                 // 5  bool discardByAlpha(float alpha)
    ApplyMonochrome,                // 6  vec4 applyMonochrome(vec4 baseColor)
    ApplyThematicDisplay,           // 7  vec4 applyThematicDisplay(vec4 baseColor)
    ApplyLighting,                  // 8  vec4 applyLighting(vec4 baseColor)
    ReverseWhiteOnWhite,            // 9  vec4 reverseWhiteOnWhite(vec4 baseColor)
    ApplyClipping,                  // 10 bvec2 applyClipping()
    ApplyContours,                  // 11 vec4 applyContours(vec4 baseColor)
    ApplyFlash,                     // 12 vec4 applyFlash(vec4 baseColor)
    ApplyPlanarClassifier,          // 13 vec4 applyPlanarClassifications(vec4 baseColor, float depth)
    ApplyDraping,                   // 14 vec4 applyDraping(vec4 baseColor)
    ApplySolarShadowMap,            // 15 vec4 applySolarShadowMap(vec4 baseColor)
    ApplyWiremesh,                  // 16 vec4 applyWiremesh(vec4 baseColor)
    ApplyDebugColor,                // 17 vec4 applyDebugColor(vec4 baseColor)
    AssignFragData,                 // 18 void assignFragData(vec4 baseColor) [required]
    OverrideFeatureId,              // 19 void overrideFeatureId()
    FinalizeDepth,                  // 20 float finalizeDepth()
    OverrideColor,                  // 21 vec4 overrideColor(vec4 baseColor)
    OverrideRenderOrder,            // 22 float overrideRenderOrder()
    ApplyAtmosphericScattering,     // 23 vec4 applyAtmosphericScattering(vec4 baseColor)
    FinalizeNormal,                 // 24 vec3 finalizeNormal()
    Count
};

// ---------------------------------------------------------------------------
// ShaderBuilderFlags — flags for shader builder construction
// Ported from: itwinjs-core ShaderBuilder.ts ShaderBuilderFlags (line 529-532)
// ---------------------------------------------------------------------------
struct ShaderBuilderFlags {
    bool instanced = false;
    // PositionType could be added here when vertex table support is needed
};

// ---------------------------------------------------------------------------
// ShaderBuilder — assembles GLSL source for one shader stage
// Ported from: itwinjs-core ShaderBuilder.ts ShaderBuilder class
//
// Uses ShaderVariables for variable management with dedup and binding callbacks.
// Provides composable component slots for modular shader assembly.
//
// The buildVertexMain() and buildFragmentMain() methods wrap each slot body
// into a named GLSL function and assemble main() with the exact call chain
// from the reference (not raw string concatenation).
// ---------------------------------------------------------------------------
class ShaderBuilder {
public:
    ShaderBuilder() { addDefaultMacros(); }
    explicit ShaderBuilder(ShaderBuilderFlags const& flags) : m_flags(flags) { addDefaultMacros(); }

    // --- Variable management (Ported from: itwinjs-core ShaderBuilder) ---

    /// Get the variables collection.
    ShaderVariables& getVariables() { return m_variables; }
    ShaderVariables const& getVariables() const { return m_variables; }

    /// add a variable declaration (delegates to ShaderVariables).
    bool addVariable(ShaderVariable var);

    /// Look up a declared variable by name (nullptr if not found).
    /// Ported from: itwinjs-core ShaderBuilder.find()
    ShaderVariable const* find(std::string const& name) const { return m_variables.find(name); }

    /// add a uniform with binding callback.
    void addUniform(std::string name, VariableType type,
                    addVariableBinding binding,
                    VariablePrecision precision = VariablePrecision::Default);

    /// add a uniform array with binding callback.
    /// Ported from: itwinjs-core ShaderBuilder.addUniformArray()
    void addUniformArray(std::string name, VariableType type, int length,
                         addVariableBinding binding);

    /// add a varying.
    bool addVarying(std::string name, VariableType type);

    /// add a global variable.
    void addGlobal(std::string name, VariableType type,
                   std::string value = "", bool isConst = false);

    /// add a constant.
    void addConstant(std::string name, VariableType type, std::string value);

    /// add a bit flag constant.
    void addBitFlagConstant(std::string name, int value);

    // --- Source building ---

    /// add a GLSL function (complete definition) to the shader.
    void addFunction(std::string const& functionCode);

    /// add a GLSL function from declaration + implementation.
    /// Ported from: itwinjs-core ShaderBuilder.addFunction(declaration, implementation)
    /// If implementation starts with '\n', treated as inline function.
    /// Otherwise, wraps as: declaration { implementation }
    void addFunction(std::string const& declaration, std::string const& implementation);

    /// add a macro definition.
    void addMacro(std::string const& name, std::string const& value);

    /// add a define with replacement logic.
    /// Ported from: itwinjs-core ShaderBuilder.addDefine()
    void addDefine(std::string const& name, std::string const& value);

    /// add an extension declaration.
    /// Ported from: itwinjs-core ShaderBuilder.addExtension()
    void addExtension(std::string const& extName);

    /// add an initializer (code that runs at the start of main()).
    /// Ported from: itwinjs-core ShaderBuilder.addInitializer()
    void addInitializer(std::string const& code);

    /// Register a computed-varying assignment — emitted AFTER the component
    /// chain in the vertex main (reference ShaderBuilder.ts:833-836).
    /// Ported from: itwinjs-core ShaderBuilder.addComputedVarying()
    void addComputedVarying(std::string const& code) { m_computedVaryings.push_back(code); }

    /// add a raw code snippet (for output declarations etc.).
    void addCode(std::string const& code);

    /// add a fragment output declaration: layout(location=N) out vec4 name.
    /// Ported from: itwinjs-core ShaderBuilder.addFragOutput()
    void addFragOutput(std::string const& name, int location);

    /// Clear all fragment output declarations.
    /// Ported from: itwinjs-core ShaderBuilder.clearFragOutput()
    void clearFragOutputs();

    /// add GL_EXT_draw_buffers extension for MRT.
    /// Ported from: itwinjs-core ShaderBuilder.addDrawBuffersExtension()
    void addDrawBuffersExtension(int count);

    /// Build the final GLSL source string (without component slots).
    std::string buildSource() const;

    /// Set the GLSL version (default: "410 core" for desktop OpenGL).
    void setVersion(std::string const& version) { m_version = version; }
    std::string const& getVersion() const { return m_version; }

    /// Set the shader stage (for correct Varying→in/out mapping).
    void setStage(ShaderStage stage) { m_stage = stage; }
    ShaderStage getStage() const { return m_stage; }

    /// Get the shader builder flags.
    ShaderBuilderFlags const& getFlags() const { return m_flags; }

    /// Whether this (vertex) builder is for instanced geometry.
    /// Ported from: itwinjs-core VertexShaderBuilder.usesInstancedGeometry
    bool usesInstancedGeometry() const noexcept { return m_usesInstancedGeometry; }
    void setUsesInstancedGeometry(bool v) noexcept { m_usesInstancedGeometry = v; }

    /// Opt in to the function-call vertex-main convention (itwinjs
    /// VertexShaderBuilder.buildSource): each set slot is emitted as a named
    /// GLSL function + a call in main(), with rawPosition/baseColor threaded.
    /// Default false keeps the legacy inline-statement convention used by
    /// SurfaceVariantCompiler; the modular Surface path (addXxx) opts in because
    /// its slot bodies are function bodies (params + return), not raw statements.
    /// Transitional: removed when SurfaceVariantCompiler is rewritten to modular.
    void setFunctionCallVertMain(bool v) noexcept { m_functionCallVertMain = v; }
    bool usesFunctionCallVertMain() const noexcept { return m_functionCallVertMain; }

    /// Opt in to the function-call fragment-main convention (itwinjs
    /// FragmentShaderBuilder.buildSource): each set slot is emitted as a named
    /// GLSL function + a call in main(), with baseColor threaded by value.
    /// Default false keeps the legacy inline-statement convention used by
    /// Edge/Polyline/Unlit builders; the Surface path opts in because its slot
    /// bodies are function bodies (params + return), not raw statements.
    /// Transitional: removed when the remaining builders migrate.
    void setFunctionCallFragMain(bool v) noexcept { m_functionCallFragMain = v; }
    bool usesFunctionCallFragMain() const noexcept { return m_functionCallFragMain; }

    // --- Composable component slots ---
    // Ported from: itwinjs-core ShaderBuilder.ts VertexShaderComponent/FragmentShaderComponent

    /// Set the code for a vertex shader component slot.
    /// The code should be the function BODY (GLSL statements), not a complete function.
    /// buildVertexMain() wraps it into a function definition with the correct signature.
    void setVertexComponent(VertexShaderComponent component, std::string const& code);

    /// Set the code for a fragment shader component slot.
    /// The code should be the function BODY (GLSL statements), not a complete function.
    /// buildFragmentMain() wraps it into a function definition with the correct signature.
    void setFragmentComponent(FragmentShaderComponent component, std::string const& code);

    /// Build main() from vertex component slots.
    /// Wraps each slot body as a named function and assembles main() with the
    /// exact call chain from itwinjs-core VertexShaderBuilder.buildSource().
    std::string buildVertexMain() const;

    /// Build main() from fragment component slots.
    /// Wraps each slot body as a named function and assembles main() with the
    /// exact call chain from itwinjs-core FragmentShaderBuilder.buildSource(),
    /// including the clipping block logic.
    std::string buildFragmentMain() const;

    /// Build the full source with component-assembled main().
    /// Ported from: itwinjs-core ShaderBuilder.buildPreludeCommon()
    std::string buildSourceWithComponents() const;

    // --- Binding ---

    /// Invoke binding callbacks for all variables.
    void addBindings(ShaderProgram& prog, ShaderVariables const* predefined = nullptr) const;

    // --- Static helpers ---

    /// Build a GLSL function definition from declaration + implementation.
    /// Ported from: itwinjs-core SourceBuilder.buildFunctionDefinition()
    /// If implementation starts with '\n', returns inline: "decl impl"
    /// Otherwise returns block: "decl {\n  impl\n}\n"
    static std::string buildFunctionDefinition(std::string const& declaration,
                                                std::string const& implementation);

    /// Get the function signature for a vertex shader component slot.
    static std::string_view getVertexComponentSignature(VertexShaderComponent component);

    /// Get the function signature for a fragment shader component slot.
    static std::string_view getFragmentComponentSignature(FragmentShaderComponent component);

private:
    std::string m_version = "410 core";
    ShaderStage m_stage = ShaderStage::Vertex;
    bool m_usesInstancedGeometry = false;  // Ported from VertexShaderBuilder.usesInstancedGeometry
    bool m_functionCallVertMain = false;   // opt-in function-call vertex main (see setter)
    bool m_functionCallFragMain = false;   // opt-in function-call fragment main (see setter)
    ShaderBuilderFlags m_flags;
    ShaderVariables m_variables;
    std::vector<std::string> m_functions;
    std::vector<std::pair<std::string, std::string>> m_macros;
    std::vector<std::string> m_extensions;
    std::vector<std::string> m_initializers;
    // Computed-varying assignments — emitted AFTER the component chain in the
    // vertex main (reference ShaderBuilder.ts:833-836), unlike initializers
    // (which run first). computeSurfaceFlags reads feature_rgb, which
    // computeFeatureOverrides initializes — order is load-bearing.
    std::vector<std::string> m_computedVaryings;
    std::vector<std::string> m_codeSnippets;

    // Fragment output declarations: (name, location)
    std::vector<std::pair<std::string, int>> m_fragOutputs;

    // Component slots
    std::array<std::string, static_cast<size_t>(VertexShaderComponent::Count)>
        m_vertexComponents = {};
    std::array<std::string, static_cast<size_t>(FragmentShaderComponent::Count)>
        m_fragmentComponents = {};

    // Add default macros (TEXTURE, TEXTURE_CUBE, TEXTURE_PROJ).
    // Ported from: itwinjs-core ShaderBuilder.ts constructor (line 509-511)
    void addDefaultMacros() {
        addDefine("TEXTURE", "texture");
        addDefine("TEXTURE_CUBE", "texture");
        addDefine("TEXTURE_PROJ", "textureProj");
    }
};

// ---------------------------------------------------------------------------
// ProgramBuilder — assembles vertex + fragment shaders into a program
// Ported from: itwinjs-core ShaderBuilder.ts ProgramBuilder (line 707-807)
// ---------------------------------------------------------------------------
class ProgramBuilder {
public:
    ProgramBuilder()
    {
        m_vertexBuilder.setStage(ShaderStage::Vertex);
        m_fragmentBuilder.setStage(ShaderStage::Fragment);
    }

    explicit ProgramBuilder(ShaderBuilderFlags const& flags)
        : m_vertexBuilder(flags)
        , m_fragmentBuilder(flags)
    {
        m_vertexBuilder.setStage(ShaderStage::Vertex);
        m_fragmentBuilder.setStage(ShaderStage::Fragment);
    }

    ShaderBuilder& getVertexBuilder() { return m_vertexBuilder; }
    ShaderBuilder& getFragmentBuilder() { return m_fragmentBuilder; }
    ShaderBuilder const& getVertexBuilder() const { return m_vertexBuilder; }
    ShaderBuilder const& getFragmentBuilder() const { return m_fragmentBuilder; }

    /// Set the program name (for debugging).
    void setName(std::string const& name) { m_name = name; }
    std::string const& getName() const { return m_name; }

    /// add a uniform to both vertex and fragment builders.
    /// Ported from: itwinjs-core ProgramBuilder.addUniform()
    void addUniform(std::string name, VariableType type,
                    addVariableBinding binding,
                    VariablePrecision precision = VariablePrecision::Default);

    /// add a uniform array to both vertex and fragment builders.
    /// Ported from: itwinjs-core ProgramBuilder.addUniformArray()
    void addUniformArray(std::string name, VariableType type, int length,
                         addVariableBinding binding);

    /// add a varying to both builders (out in vert, in in frag).
    /// Ported from: itwinjs-core ProgramBuilder.addVarying()
    void addVarying(std::string name, VariableType type);

    /// add a function-computed varying: adds a function to the vertex builder,
    /// a varying to both builders, and a computed varying assignment.
    /// Ported from: itwinjs-core ProgramBuilder.addFunctionComputedVarying()
    void addFunctionComputedVarying(std::string const& name, VariableType type,
                                     std::string const& funcName,
                                     std::string const& funcBody);

    /// add an inline-computed varying: adds a varying to both builders
    /// and a computed varying assignment with an inline expression.
    /// Ported from: itwinjs-core ProgramBuilder.addInlineComputedVarying()
    void addInlineComputedVarying(std::string const& name, VariableType type,
                                   std::string const& inlineExpr);

    /// add a global variable to both vertex and fragment builders.
    /// Ported from: itwinjs-core ProgramBuilder.addGlobal() (ShaderType.Both path)
    void addGlobal(std::string name, VariableType type,
                   std::string value = "", bool isConst = false);

    /// Opt the vertex builder into the function-call vertex-main convention.
    /// @see ShaderBuilder::setFunctionCallVertMain.
    void enableFunctionCallVertexMain() { m_vertexBuilder.setFunctionCallVertMain(true); }

    /// Opt the fragment builder into the function-call fragment-main convention.
    /// @see ShaderBuilder::setFunctionCallFragMain.
    void enableFunctionCallFragmentMain() { m_fragmentBuilder.setFunctionCallFragMain(true); }

private:
    ShaderBuilder m_vertexBuilder;
    ShaderBuilder m_fragmentBuilder;
    std::string m_name;
};

END_DQ_RENDER_NAMESPACE

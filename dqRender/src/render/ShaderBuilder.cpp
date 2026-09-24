// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Shader builder implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ShaderBuilder.ts
#include "ShaderBuilder.h"

#include <cassert>

BEGIN_DQ_RENDER_NAMESPACE

// ===========================================================================
// Helper: type/scope/precision to string
// Ported from: itwinjs-core ShaderBuilder.ts Convert namespace (line 62-105)
// ===========================================================================

static std::string_view typeToString(VariableType type)
{
    switch (type) {
        case VariableType::Boolean: return "bool";
        case VariableType::Int: return "int";
        case VariableType::Float: return "float";
        case VariableType::Vec2: return "vec2";
        case VariableType::Vec3: return "vec3";
        case VariableType::Vec4: return "vec4";
        case VariableType::Mat3: return "mat3";
        case VariableType::Mat4: return "mat4";
        case VariableType::Sampler2D: return "sampler2D";
        case VariableType::SamplerCube: return "samplerCube";
        case VariableType::Uint: return "uint";
        case VariableType::BVec2: return "bvec2";
        default: return "undefined";
    }
}

static std::string_view scopeToString(VariableScope scope, bool isVertexShader)
{
    switch (scope) {
        case VariableScope::Global: return "";
        case VariableScope::Varying: return isVertexShader ? "out" : "in";
        case VariableScope::Uniform: return "uniform";
        case VariableScope::Attribute: return "in";
        default: return "undefined";
    }
}

static std::string_view precisionToString(VariablePrecision precision)
{
    switch (precision) {
        case VariablePrecision::Default: return "";
        case VariablePrecision::Low: return "lowp";
        case VariablePrecision::Medium: return "mediump";
        case VariablePrecision::High: return "highp";
        default: return "undefined";
    }
}

// ===========================================================================
// ShaderVariable
// Ported from: itwinjs-core ShaderBuilder.ts ShaderVariable (line 119-190)
// ===========================================================================

ShaderVariable ShaderVariable::create(std::string name, VariableType type,
                                       VariableScope scope,
                                       addVariableBinding binding,
                                       VariablePrecision precision)
{
    ShaderVariable v;
    v.m_name = std::move(name);
    v.m_type = type;
    v.m_scope = scope;
    v.m_precision = precision;
    v.m_binding = binding;
    return v;
}

ShaderVariable ShaderVariable::createArray(std::string name, VariableType type,
                                            int length, VariableScope scope,
                                            addVariableBinding binding,
                                            VariablePrecision precision)
{
    ShaderVariable v;
    v.m_name = std::move(name);
    v.m_type = type;
    v.m_scope = scope;
    v.m_precision = precision;
    v.m_binding = binding;
    v.m_arrayLength = length;
    return v;
}

ShaderVariable ShaderVariable::createGlobal(std::string name, VariableType type,
                                             std::string value, bool isConst)
{
    ShaderVariable v;
    v.m_name = std::move(name);
    v.m_type = type;
    v.m_scope = VariableScope::Global;
    v.m_value = std::move(value);
    v.m_isConst = isConst;
    return v;
}

void ShaderVariable::addBinding(ShaderProgram& prog) const
{
    if (m_binding)
        m_binding(prog);
}

std::string_view ShaderVariable::getTypeName() const
{
    return typeToString(m_type);
}

std::string_view ShaderVariable::getScopeName(bool isVertexShader) const
{
    return scopeToString(m_scope, isVertexShader);
}

std::string_view ShaderVariable::getPrecisionName() const
{
    return precisionToString(m_precision);
}

// ---------------------------------------------------------------------------
// buildDeclaration — construct single-line GLSL declaration
// Ported from: itwinjs-core ShaderVariable.buildDeclaration() (line 163-189)
// ---------------------------------------------------------------------------
std::string ShaderVariable::buildDeclaration(bool isVertexShader) const
{
    std::string decl;

    if (m_isConst)
        decl += "const ";

    auto scope = getScopeName(isVertexShader);
    if (!scope.empty()) {
        decl += scope;
        decl += ' ';
    }

    auto prec = getPrecisionName();
    if (!prec.empty()) {
        decl += prec;
        decl += ' ';
    }

    decl += getTypeName();
    decl += ' ';
    decl += m_name;

    if (m_arrayLength > 0)
        decl += "[" + std::to_string(m_arrayLength) + "]";

    if (!m_value.empty()) {
        decl += " = ";
        decl += m_value;
    }

    decl += ';';
    return decl;
}

// ===========================================================================
// ShaderVariables
// Ported from: itwinjs-core ShaderBuilder.ts ShaderVariables (line 197-400)
// ===========================================================================

ShaderVariable const* ShaderVariables::find(std::string const& name) const
{
    for (auto const& v : m_variables) {
        if (v.getName() == name)
            return &v;
    }
    return nullptr;
}

bool ShaderVariables::addVariable(ShaderVariable var)
{
    if (find(var.getName()))
        return false;  // Dedup: variable already exists

    m_variables.push_back(std::move(var));
    return true;
}

void ShaderVariables::addUniform(std::string name, VariableType type,
                                   addVariableBinding binding,
                                   VariablePrecision precision)
{
    addVariable(ShaderVariable::create(std::move(name), type,
                                        VariableScope::Uniform, binding, precision));
}

void ShaderVariables::addUniformArray(std::string name, VariableType type,
                                        int length, addVariableBinding binding)
{
    addVariable(ShaderVariable::createArray(std::move(name), type, length,
                                             VariableScope::Uniform, binding));
}

bool ShaderVariables::addVarying(std::string name, VariableType type)
{
    return addVariable(ShaderVariable::create(std::move(name), type,
                                               VariableScope::Varying));
}

void ShaderVariables::addGlobal(std::string name, VariableType type,
                                  std::string value, bool isConst)
{
    addVariable(ShaderVariable::createGlobal(std::move(name), type,
                                              std::move(value), isConst));
}

void ShaderVariables::addConstant(std::string name, VariableType type,
                                    std::string value)
{
    addGlobal(std::move(name), type, std::move(value), true);
}

void ShaderVariables::addBitFlagConstant(std::string name, int value)
{
    addGlobal(std::move(name), VariableType::Uint,
              std::to_string(1 << value) + "u", true);
}

// ---------------------------------------------------------------------------
// buildScopeDeclarations — declarations for a specific scope
// Ported from: itwinjs-core ShaderVariables.buildScopeDeclarations() (line 241-249)
// ---------------------------------------------------------------------------
std::string ShaderVariables::buildScopeDeclarations(bool isVertexShader,
                                                      VariableScope scope,
                                                      bool constantsOnly) const
{
    std::string decls;
    for (auto const& v : m_variables) {
        if (v.getScope() != scope)
            continue;
        if (constantsOnly && !v.isConst())
            continue;
        if (!constantsOnly && v.isConst())
            continue;
        decls += v.buildDeclaration(isVertexShader);
        decls += '\n';
    }
    return decls;
}

// ---------------------------------------------------------------------------
// buildDeclarations — all variable declarations (ordered by scope)
// Ported from: itwinjs-core ShaderVariables.buildDeclarations() (line 252-266)
// ---------------------------------------------------------------------------
std::string ShaderVariables::buildDeclarations(bool isVertexShader) const
{
    std::string decls;
    if (isVertexShader) {
        decls += buildScopeDeclarations(true, VariableScope::Uniform);
        decls += buildScopeDeclarations(true, VariableScope::Attribute);
        decls += buildScopeDeclarations(true, VariableScope::Global, true);
        decls += buildScopeDeclarations(true, VariableScope::Global, false);
        decls += buildScopeDeclarations(true, VariableScope::Varying);
    } else {
        decls += buildScopeDeclarations(false, VariableScope::Varying);
        decls += buildScopeDeclarations(false, VariableScope::Uniform);
        decls += buildScopeDeclarations(false, VariableScope::Global, true);
        decls += buildScopeDeclarations(false, VariableScope::Global, false);
    }
    return decls;
}

// ---------------------------------------------------------------------------
// addBindings — invoke binding callbacks
// Ported from: itwinjs-core ShaderVariables.addBindings() (line 272-279)
// ---------------------------------------------------------------------------
void ShaderVariables::addBindings(ShaderProgram& prog,
                                    ShaderVariables const* predefined) const
{
    for (auto const& v : m_variables) {
        if (!v.hasBinding())
            continue;
        if (predefined && predefined->find(v.getName()))
            continue;  // Skip variables already in predefined set
        v.addBinding(prog);
    }
}

// ===========================================================================
// ShaderBuilder
// ===========================================================================

bool ShaderBuilder::addVariable(ShaderVariable var)
{
    return m_variables.addVariable(std::move(var));
}

void ShaderBuilder::addUniform(std::string name, VariableType type,
                                 addVariableBinding binding,
                                 VariablePrecision precision)
{
    m_variables.addUniform(std::move(name), type, binding, precision);
}

void ShaderBuilder::addUniformArray(std::string name, VariableType type, int length,
                                      addVariableBinding binding)
{
    m_variables.addUniformArray(std::move(name), type, length, binding);
}

bool ShaderBuilder::addVarying(std::string name, VariableType type)
{
    return m_variables.addVarying(std::move(name), type);
}

void ShaderBuilder::addGlobal(std::string name, VariableType type,
                                std::string value, bool isConst)
{
    m_variables.addGlobal(std::move(name), type, std::move(value), isConst);
}

void ShaderBuilder::addConstant(std::string name, VariableType type,
                                  std::string value)
{
    m_variables.addConstant(std::move(name), type, std::move(value));
}

void ShaderBuilder::addBitFlagConstant(std::string name, int value)
{
    m_variables.addBitFlagConstant(std::move(name), value);
}

void ShaderBuilder::addFunction(std::string const& functionCode)
{
    // Dedup by exact string — Ported from: itwinjs-core ShaderBuilder.addFunction
    // (ShaderBuilder.ts:530-537, findFunction exact-match). Prevents the same
    // helper from being emitted twice when two composition steps both add it.
    for (auto const& existing : m_functions) {
        if (existing == functionCode)
            return;
    }
    m_functions.push_back(functionCode);
}

// ---------------------------------------------------------------------------
// addFunction(declaration, implementation) — two-argument overload
// Ported from: itwinjs-core ShaderBuilder.addFunction(declaration, implementation)
// ---------------------------------------------------------------------------
void ShaderBuilder::addFunction(std::string const& declaration,
                                 std::string const& implementation)
{
    m_functions.push_back(buildFunctionDefinition(declaration, implementation));
}

// Ported from: itwinjs-core ShaderBuilder.addMacro() (line 558-561)
// Deduplicates by checking if a macro with the same name already exists.
void ShaderBuilder::addMacro(std::string const& name, std::string const& value)
{
    for (auto& [existingName, existingValue] : m_macros) {
        if (existingName == name) {
            existingValue = value;
            return;
        }
    }
    m_macros.emplace_back(name, value);
}

// ---------------------------------------------------------------------------
// addDefine — add a #define with replacement logic
// Ported from: itwinjs-core ShaderBuilder.addDefine()
// ---------------------------------------------------------------------------
void ShaderBuilder::addDefine(std::string const& name, std::string const& value)
{
    for (auto& [existingName, existingValue] : m_macros) {
        if (existingName == name) {
            existingValue = value;
            return;
        }
    }
    m_macros.emplace_back(name, value);
}

// Ported from: itwinjs-core ShaderBuilder.addExtension() (line 553-556)
// Deduplicates by checking if the extension already exists.
void ShaderBuilder::addExtension(std::string const& extName)
{
    for (auto const& existing : m_extensions) {
        if (existing == extName)
            return;
    }
    m_extensions.push_back(extName);
}

// Ported from: itwinjs-core ShaderBuilder.addInitializer() (line 499-502)
// Deduplicates by checking if the initializer already exists.
void ShaderBuilder::addInitializer(std::string const& code)
{
    for (auto const& existing : m_initializers) {
        if (existing == code)
            return;
    }
    m_initializers.push_back(code);
}

void ShaderBuilder::addCode(std::string const& code)
{
    m_codeSnippets.push_back(code);
}

// ---------------------------------------------------------------------------
// addFragOutput — add layout(location=N) out vec4 name
// Ported from: itwinjs-core ShaderBuilder.addFragOutput()
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ShaderBuilder.addFragOutput() (line 579-585)
// When location == -1, emits "out vec4 name;" without a layout qualifier.
void ShaderBuilder::addFragOutput(std::string const& name, int location)
{
    m_fragOutputs.emplace_back(name, location);
}

// ---------------------------------------------------------------------------
// clearFragOutputs — remove all fragment output declarations
// Ported from: itwinjs-core ShaderBuilder.clearFragOutput()
// ---------------------------------------------------------------------------
void ShaderBuilder::clearFragOutputs()
{
    m_fragOutputs.clear();
}

// ---------------------------------------------------------------------------
// addDrawBuffersExtension — add GL_EXT_draw_buffers for MRT
// Ported from: itwinjs-core ShaderBuilder.addDrawBuffersExtension() (line 965-969)
//
// Clears existing frag outputs and adds N MRT outputs (FragColor0..FragColorN-1)
// with layout(location = i) qualifiers.
// ---------------------------------------------------------------------------
void ShaderBuilder::addDrawBuffersExtension(int count)
{
    clearFragOutputs();
    for (int i = 0; i < count; ++i)
        addFragOutput("FragColor" + std::to_string(i), i);
}

void ShaderBuilder::addBindings(ShaderProgram& prog,
                                  ShaderVariables const* predefined) const
{
    m_variables.addBindings(prog, predefined);
}

// ---------------------------------------------------------------------------
// Macro string generation
// ---------------------------------------------------------------------------
static std::string buildMacros(
    std::vector<std::pair<std::string, std::string>> const& macros)
{
    std::string source;
    for (auto const& [name, value] : macros) {
        source += "#define " + name + " " + value + "\n";
    }
    if (!macros.empty()) source += "\n";
    return source;
}

// ---------------------------------------------------------------------------
// buildFunctionDefinition — build GLSL function from declaration + body
// Ported from: itwinjs-core SourceBuilder.buildFunctionDefinition() (line 458-461)
// ---------------------------------------------------------------------------
std::string ShaderBuilder::buildFunctionDefinition(
    std::string const& declaration, std::string const& implementation)
{
    // If implementation starts with newline, it's an inline function:
    // the body is self-contained (already has braces or is a single expression).
    if (!implementation.empty() && implementation.front() == '\n')
        return declaration + " " + implementation;

    // Block function: wrap body in braces.
    return declaration + " {\n" + implementation + "}\n";
}

// ---------------------------------------------------------------------------
// getVertexComponentSignature — function signature for each vertex slot
// Ported from: itwinjs-core ShaderBuilder.ts VertexShaderComponent signatures
// ---------------------------------------------------------------------------
std::string_view ShaderBuilder::getVertexComponentSignature(
    VertexShaderComponent component)
{
    switch (component) {
        case VertexShaderComponent::ComputeQuantizedPosition:
            return "vec4 computeQuantizedPosition()";
        case VertexShaderComponent::AdjustRawPosition:
            return "vec4 adjustRawPosition(vec4 rawPos)";
        case VertexShaderComponent::CheckForEarlyDiscard:
            return "bool checkForEarlyDiscard(vec4 rawPos)";
        case VertexShaderComponent::ComputeFeatureOverrides:
            return "void computeFeatureOverrides()";
        case VertexShaderComponent::ComputeMaterial:
            return "void computeMaterial()";
        case VertexShaderComponent::ComputeBaseColor:
            return "vec4 computeBaseColor()";
        case VertexShaderComponent::ApplyMaterialColor:
            return "vec4 applyMaterialColor(vec4 baseColor)";
        case VertexShaderComponent::ApplyFeatureColor:
            return "vec4 applyFeatureColor(vec4 baseColor)";
        case VertexShaderComponent::AdjustContrast:
            return "vec4 adjustContrast(vec4 baseColor)";
        case VertexShaderComponent::CheckForDiscard:
            return "bool checkForDiscard()";
        case VertexShaderComponent::ComputePosition:
            return "vec4 computePosition(vec4 rawPos)";
        case VertexShaderComponent::ComputeAtmosphericScatteringVaryings:
            return "void computeAtmosphericScatteringVaryings()";
        case VertexShaderComponent::CheckForLateDiscard:
            return "bool checkForLateDiscard()";
        case VertexShaderComponent::FinalizePosition:
            return "vec4 finalizePosition(vec4 pos)";
        default:
            return "void unknownVertexComponent()";
    }
}

// ---------------------------------------------------------------------------
// getFragmentComponentSignature — function signature for each fragment slot
// Ported from: itwinjs-core ShaderBuilder.ts FragmentShaderComponent signatures
// ---------------------------------------------------------------------------
std::string_view ShaderBuilder::getFragmentComponentSignature(
    FragmentShaderComponent component)
{
    switch (component) {
        case FragmentShaderComponent::CheckForEarlyDiscard:
            return "bool checkForEarlyDiscard()";
        case FragmentShaderComponent::ComputeBaseColor:
            return "vec4 computeBaseColor()";
        case FragmentShaderComponent::ApplyMaterialOverrides:
            return "vec4 applyMaterialOverrides(vec4 baseColor)";
        case FragmentShaderComponent::FinalizeBaseColor:
            return "vec4 finalizeBaseColor(vec4 baseColor)";
        case FragmentShaderComponent::CheckForDiscard:
            return "bool checkForDiscard(vec4 baseColor)";
        case FragmentShaderComponent::DiscardByAlpha:
            return "bool discardByAlpha(float alpha)";
        case FragmentShaderComponent::ApplyMonochrome:
            return "vec4 applyMonochrome(vec4 baseColor)";
        case FragmentShaderComponent::ApplyThematicDisplay:
            return "vec4 applyThematicDisplay(vec4 baseColor)";
        case FragmentShaderComponent::ApplyLighting:
            return "vec4 applyLighting(vec4 baseColor)";
        case FragmentShaderComponent::ReverseWhiteOnWhite:
            return "vec4 reverseWhiteOnWhite(vec4 baseColor)";
        case FragmentShaderComponent::ApplyClipping:
            return "bvec2 applyClipping(vec4 baseColor)";
        case FragmentShaderComponent::ApplyContours:
            return "vec4 applyContours(vec4 baseColor)";
        case FragmentShaderComponent::ApplyFlash:
            return "vec4 applyFlash(vec4 baseColor)";
        case FragmentShaderComponent::ApplyPlanarClassifier:
            return "vec4 applyPlanarClassifications(vec4 baseColor, float depth)";
        case FragmentShaderComponent::ApplyDraping:
            return "vec4 applyDraping(vec4 baseColor)";
        case FragmentShaderComponent::ApplySolarShadowMap:
            return "vec4 applySolarShadowMap(vec4 baseColor)";
        case FragmentShaderComponent::ApplyWiremesh:
            return "vec4 applyWiremesh(vec4 baseColor)";
        case FragmentShaderComponent::ApplyDebugColor:
            return "vec4 applyDebugColor(vec4 baseColor)";
        case FragmentShaderComponent::AssignFragData:
            return "void assignFragData(vec4 baseColor)";
        case FragmentShaderComponent::OverrideFeatureId:
            // Reference signature (ShaderBuilder.ts:930): the slot's body reads the
            // input color and returns the overridden color. Function-call main threads
            // baseColor through by value.
            return "vec4 overrideFeatureId(vec4 baseColor)";
        case FragmentShaderComponent::FinalizeDepth:
            return "float finalizeDepth()";
        case FragmentShaderComponent::OverrideColor:
            return "vec4 overrideColor(vec4 baseColor)";
        case FragmentShaderComponent::OverrideRenderOrder:
            return "float overrideRenderOrder()";
        case FragmentShaderComponent::ApplyAtmosphericScattering:
            return "vec4 applyAtmosphericScattering(vec4 baseColor)";
        case FragmentShaderComponent::FinalizeNormal:
            return "vec3 finalizeNormal()";
        default:
            return "void unknownFragmentComponent()";
    }
}

// ---------------------------------------------------------------------------
// buildSource — build GLSL source without component slots
// ---------------------------------------------------------------------------
std::string ShaderBuilder::buildSource() const
{
    std::string source;
    source += "#version " + m_version + "\n\n";
    source += buildMacros(m_macros);
    source += m_variables.buildDeclarations(m_stage == ShaderStage::Vertex);

    for (auto const& code : m_codeSnippets)
        source += code + "\n";

    for (auto const& func : m_functions)
        source += func + "\n\n";

    source += "void main()\n{\n";
    for (auto const& init : m_initializers)
        source += "    " + init + "\n";
    source += "}\n";

    return source;
}

// ---------------------------------------------------------------------------
// Component slot methods
// Ported from: itwinjs-core ShaderBuilder.ts
// ---------------------------------------------------------------------------

void ShaderBuilder::setVertexComponent(VertexShaderComponent component,
                                         std::string const& code)
{
    m_vertexComponents[static_cast<size_t>(component)] = code;
}

void ShaderBuilder::setFragmentComponent(FragmentShaderComponent component,
                                           std::string const& code)
{
    m_fragmentComponents[static_cast<size_t>(component)] = code;
}

// ---------------------------------------------------------------------------
// buildVertexMain — assemble vertex main() with correct call chain
// Ported from: itwinjs-core VertexShaderBuilder.buildSource() (line 757-832)
//
// Slot bodies are injected as raw statements into main(), preserving backward
// compatibility with existing callers that write inline GLSL.  The call ORDER
// matches the reference exactly (slots 0-13 in sequence with branching).
//
// Note: the reference wraps each slot as a named function.  This is a future
// enhancement that requires updating all callers to provide function bodies
// instead of raw statements.
// ---------------------------------------------------------------------------
std::string ShaderBuilder::buildVertexMain() const
{
    // Function-call convention (itwinjs VertexShaderBuilder.buildSource): each
    // set slot is emitted as a named GLSL function + a call in main(), with
    // rawPosition/baseColor threaded. Opt-in via setFunctionCallVertMain; the
    // default inline-statement path follows below.
    if (m_functionCallVertMain) {
        auto has = [&](VertexShaderComponent c) {
            return !m_vertexComponents[static_cast<size_t>(c)].empty();
        };
        auto funcDef = [&](VertexShaderComponent c) -> std::string {
            return std::string(getVertexComponentSignature(c)) + "\n{\n" +
                   m_vertexComponents[static_cast<size_t>(c)] + "\n}\n";
        };

        std::string out;
        // Function definitions for set slots (ComputePosition required).
        if (has(VertexShaderComponent::ComputeQuantizedPosition))
            out += funcDef(VertexShaderComponent::ComputeQuantizedPosition);
        if (has(VertexShaderComponent::AdjustRawPosition))
            out += funcDef(VertexShaderComponent::AdjustRawPosition);
        if (has(VertexShaderComponent::CheckForEarlyDiscard))
            out += funcDef(VertexShaderComponent::CheckForEarlyDiscard);
        if (has(VertexShaderComponent::ComputeFeatureOverrides))
            out += funcDef(VertexShaderComponent::ComputeFeatureOverrides);
        if (has(VertexShaderComponent::ComputeMaterial))
            out += funcDef(VertexShaderComponent::ComputeMaterial);
        if (has(VertexShaderComponent::ComputeBaseColor))
            out += funcDef(VertexShaderComponent::ComputeBaseColor);
        if (has(VertexShaderComponent::ApplyMaterialColor))
            out += funcDef(VertexShaderComponent::ApplyMaterialColor);
        if (has(VertexShaderComponent::ApplyFeatureColor))
            out += funcDef(VertexShaderComponent::ApplyFeatureColor);
        if (has(VertexShaderComponent::AdjustContrast))
            out += funcDef(VertexShaderComponent::AdjustContrast);
        if (has(VertexShaderComponent::CheckForDiscard))
            out += funcDef(VertexShaderComponent::CheckForDiscard);
        out += funcDef(VertexShaderComponent::ComputePosition);
        if (has(VertexShaderComponent::ComputeAtmosphericScatteringVaryings))
            out += funcDef(VertexShaderComponent::ComputeAtmosphericScatteringVaryings);
        if (has(VertexShaderComponent::CheckForLateDiscard))
            out += funcDef(VertexShaderComponent::CheckForLateDiscard);
        if (has(VertexShaderComponent::FinalizePosition))
            out += funcDef(VertexShaderComponent::FinalizePosition);

        // main() chain — Ported from: itwinjs-core ShaderBuilder.ts:746-840.
        // Non-quantized subset; the quantized/computeVertexPosition path is
        // deferred to the VertexLUT work (Task 2).
        std::string main = "void main()\n{\n";
        for (auto const& init : m_initializers)
            main += "    " + init + "\n";

        if (has(VertexShaderComponent::AdjustRawPosition))
            main += "    vec4 rawPosition = adjustRawPosition(vec4(0.0, 0.0, 0.0, 1.0));\n";
        else
            main += "    vec4 rawPosition = vec4(0.0, 0.0, 0.0, 1.0);\n";

        if (has(VertexShaderComponent::CheckForEarlyDiscard))
            main += "    if (checkForEarlyDiscard(rawPosition)) return;\n";
        if (has(VertexShaderComponent::ComputeFeatureOverrides))
            main += "    computeFeatureOverrides();\n";
        if (has(VertexShaderComponent::ComputeMaterial))
            main += "    computeMaterial();\n";

        if (has(VertexShaderComponent::ComputeBaseColor)) {
            main += "    vec4 baseColor = computeBaseColor();\n";
            if (has(VertexShaderComponent::ApplyMaterialColor))
                main += "    baseColor = applyMaterialColor(baseColor);\n";
            if (has(VertexShaderComponent::ApplyFeatureColor))
                main += "    baseColor = applyFeatureColor(baseColor);\n";
            if (has(VertexShaderComponent::AdjustContrast))
                main += "    baseColor = adjustContrast(baseColor);\n";
            // ShaderBuilder.ts:819 — vertex baseColor chain writes v_color.
            main += "    v_color = baseColor;\n";
        }

        // Ported from: itwinjs-core ShaderBuilder.ts:822-834
        // computePosition MUST be called BEFORE checkForDiscard. This is a
        // workaround for an Intel Ultra 7 driver bug where discarding triangles
        // glitches if gl_Position was not initialized first.
        main += "    gl_Position = computePosition(rawPosition);\n";

        if (has(VertexShaderComponent::CheckForDiscard))
            main += "    if (checkForDiscard()) return;\n";

        if (has(VertexShaderComponent::ComputeAtmosphericScatteringVaryings))
            main += "    computeAtmosphericScatteringVaryings();\n";
        if (has(VertexShaderComponent::CheckForLateDiscard))
            main += "    if (checkForLateDiscard()) return;\n";
        if (has(VertexShaderComponent::FinalizePosition))
            main += "    gl_Position = finalizePosition(gl_Position);\n";

        // Computed varyings AFTER the component chain — the reference emits
        // them here (ShaderBuilder.ts:833-836, after gl_Position/finalize:
        // `for (const comp of this._computedVarying) main.addline(comp)`).
        // Emitting them as initializers (top of main) ran computeSurfaceFlags
        // BEFORE computeFeatureOverrides initialized feature_rgb — the -1
        // "not overridden" sentinel never landed, kSurfaceMask_OverrideRgb
        // fired on every fragment, and textured surfaces rendered flat
        // vertex color (blue-dot/chroma regressions).
        for (auto const& comp : m_computedVaryings)
            main += "    " + comp + "\n";

        main += "}\n";
        return out + main;
    }

    std::string main = "void main()\n{\n";

    // Initializers
    for (auto const& init : m_initializers)
        main += "    " + init + "\n";

    // Slot 0: ComputeQuantizedPosition
    if (!m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ComputeQuantizedPosition)].empty()) {
        main += "    // computeQuantizedPosition\n";
        main += m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ComputeQuantizedPosition)];
        if (main.back() != '\n') main += "\n";
    }

    // Slot 1: AdjustRawPosition
    if (!m_vertexComponents[static_cast<size_t>(VertexShaderComponent::AdjustRawPosition)].empty()) {
        main += "    // adjustRawPosition\n";
        main += m_vertexComponents[static_cast<size_t>(VertexShaderComponent::AdjustRawPosition)];
        if (main.back() != '\n') main += "\n";
    }

    // Slot 2: CheckForEarlyDiscard
    if (!m_vertexComponents[static_cast<size_t>(VertexShaderComponent::CheckForEarlyDiscard)].empty()) {
        main += "    // checkForEarlyDiscard\n";
        main += m_vertexComponents[static_cast<size_t>(VertexShaderComponent::CheckForEarlyDiscard)];
        if (main.back() != '\n') main += "\n";
    }

    // Slot 3: ComputeFeatureOverrides
    if (!m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ComputeFeatureOverrides)].empty()) {
        main += "    // computeFeatureOverrides\n";
        main += m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ComputeFeatureOverrides)];
        if (main.back() != '\n') main += "\n";
    }

    // Slot 4: ComputeMaterial
    if (!m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ComputeMaterial)].empty()) {
        main += "    // computeMaterial\n";
        main += m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ComputeMaterial)];
        if (main.back() != '\n') main += "\n";
    }

    // Slot 5: ComputeBaseColor
    if (!m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ComputeBaseColor)].empty()) {
        main += "    // computeBaseColor\n";
        main += m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ComputeBaseColor)];
        if (main.back() != '\n') main += "\n";
    }

    // Slot 6: ApplyMaterialColor
    if (!m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ApplyMaterialColor)].empty()) {
        main += "    // applyMaterialColor\n";
        main += m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ApplyMaterialColor)];
        if (main.back() != '\n') main += "\n";
    }

    // Slot 7: ApplyFeatureColor
    if (!m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ApplyFeatureColor)].empty()) {
        main += "    // applyFeatureColor\n";
        main += m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ApplyFeatureColor)];
        if (main.back() != '\n') main += "\n";
    }

    // Slot 8: AdjustContrast
    if (!m_vertexComponents[static_cast<size_t>(VertexShaderComponent::AdjustContrast)].empty()) {
        main += "    // adjustContrast\n";
        main += m_vertexComponents[static_cast<size_t>(VertexShaderComponent::AdjustContrast)];
        if (main.back() != '\n') main += "\n";
    }

    // Slot 10: ComputePosition (required — all callers set this)
    // Ported from: itwinjs-core ShaderBuilder.ts:822-834
    // computePosition MUST be called BEFORE checkForDiscard. This is a
    // workaround for an Intel Ultra 7 driver bug where discarding triangles
    // glitches if gl_Position was not initialized first.
    main += "    // computePosition\n";
    main += m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ComputePosition)];
    if (main.back() != '\n') main += "\n";

    // Slot 9: CheckForDiscard (after computePosition per Intel workaround)
    if (!m_vertexComponents[static_cast<size_t>(VertexShaderComponent::CheckForDiscard)].empty()) {
        main += "    // checkForDiscard\n";
        main += m_vertexComponents[static_cast<size_t>(VertexShaderComponent::CheckForDiscard)];
        if (main.back() != '\n') main += "\n";
    }

    // Slot 11: ComputeAtmosphericScatteringVaryings
    if (!m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ComputeAtmosphericScatteringVaryings)].empty()) {
        main += "    // computeAtmosphericScatteringVaryings\n";
        main += m_vertexComponents[static_cast<size_t>(VertexShaderComponent::ComputeAtmosphericScatteringVaryings)];
        if (main.back() != '\n') main += "\n";
    }

    // Slot 12: CheckForLateDiscard
    if (!m_vertexComponents[static_cast<size_t>(VertexShaderComponent::CheckForLateDiscard)].empty()) {
        main += "    // checkForLateDiscard\n";
        main += m_vertexComponents[static_cast<size_t>(VertexShaderComponent::CheckForLateDiscard)];
        if (main.back() != '\n') main += "\n";
    }

    // Slot 13: FinalizePosition
    if (!m_vertexComponents[static_cast<size_t>(VertexShaderComponent::FinalizePosition)].empty()) {
        main += "    // finalizePosition\n";
        main += m_vertexComponents[static_cast<size_t>(VertexShaderComponent::FinalizePosition)];
        if (main.back() != '\n') main += "\n";
    }

    // Computed varyings AFTER the component chain (reference ShaderBuilder.ts:
    // 833-836; see the function-call path's note for the ordering rationale).
    for (auto const& comp : m_computedVaryings)
        main += "    " + comp + "\n";

    main += "}\n";
    return main;
}

// ---------------------------------------------------------------------------
// buildFragmentMain — assemble fragment main() with correct call chain
// Ported from: itwinjs-core FragmentShaderBuilder.buildSource() (line 839-940)
//
// Slot bodies are injected as raw statements into main(), preserving backward
// compatibility with existing callers that write inline GLSL.  The call ORDER
// matches the reference exactly, including the clipping block structure,
// discard return, finalDepth logic, and .y clip color override.
//
// Note: the reference wraps each slot as a named function.  This requires
// updating all callers to provide function bodies instead of raw statements.
// That is deferred to Batch 2 (Surface path rewrite).
// ---------------------------------------------------------------------------
std::string ShaderBuilder::buildFragmentMain() const
{
    // Function-call convention (itwinjs FragmentShaderBuilder.buildSource,
    // ShaderBuilder.ts:971-1147): each set slot is emitted as a named GLSL
    // function + a call in main(), with baseColor threaded by value.
    // Opt-in via setFunctionCallFragMain; the legacy inline path follows below.
    if (m_functionCallFragMain) {
        auto has = [&](FragmentShaderComponent c) {
            return !m_fragmentComponents[static_cast<size_t>(c)].empty();
        };
        auto funcDef = [&](FragmentShaderComponent c) -> std::string {
            return std::string(getFragmentComponentSignature(c)) + "\n{\n" +
                   m_fragmentComponents[static_cast<size_t>(c)] + "\n}\n";
        };

        // Function definitions for set slots (prelude). Order follows itwinjs
        // ShaderBuilder.ts:978-1137 (each prelude.addFunction). ComputeBaseColor
        // and AssignFragData are required.
        std::string out;
        if (has(FragmentShaderComponent::CheckForEarlyDiscard))
            out += funcDef(FragmentShaderComponent::CheckForEarlyDiscard);
        out += funcDef(FragmentShaderComponent::ComputeBaseColor);
        if (has(FragmentShaderComponent::FinalizeNormal))
            out += funcDef(FragmentShaderComponent::FinalizeNormal);
        if (has(FragmentShaderComponent::FinalizeDepth))
            out += funcDef(FragmentShaderComponent::FinalizeDepth);
        if (has(FragmentShaderComponent::ApplyClipping))
            out += funcDef(FragmentShaderComponent::ApplyClipping);
        if (has(FragmentShaderComponent::ApplyMaterialOverrides))
            out += funcDef(FragmentShaderComponent::ApplyMaterialOverrides);
        if (has(FragmentShaderComponent::ApplyThematicDisplay))
            out += funcDef(FragmentShaderComponent::ApplyThematicDisplay);
        if (has(FragmentShaderComponent::ApplyPlanarClassifier))
            out += funcDef(FragmentShaderComponent::ApplyPlanarClassifier);
        if (has(FragmentShaderComponent::ApplySolarShadowMap))
            out += funcDef(FragmentShaderComponent::ApplySolarShadowMap);
        if (has(FragmentShaderComponent::FinalizeBaseColor))
            out += funcDef(FragmentShaderComponent::FinalizeBaseColor);
        if (has(FragmentShaderComponent::CheckForDiscard))
            out += funcDef(FragmentShaderComponent::CheckForDiscard);
        if (has(FragmentShaderComponent::DiscardByAlpha))
            out += funcDef(FragmentShaderComponent::DiscardByAlpha);
        if (has(FragmentShaderComponent::ApplyMonochrome))
            out += funcDef(FragmentShaderComponent::ApplyMonochrome);
        if (has(FragmentShaderComponent::ApplyLighting))
            out += funcDef(FragmentShaderComponent::ApplyLighting);
        if (has(FragmentShaderComponent::ReverseWhiteOnWhite))
            out += funcDef(FragmentShaderComponent::ReverseWhiteOnWhite);
        if (has(FragmentShaderComponent::ApplyContours))
            out += funcDef(FragmentShaderComponent::ApplyContours);
        if (has(FragmentShaderComponent::ApplyFlash))
            out += funcDef(FragmentShaderComponent::ApplyFlash);
        if (has(FragmentShaderComponent::ApplyWiremesh))
            out += funcDef(FragmentShaderComponent::ApplyWiremesh);
        if (has(FragmentShaderComponent::ApplyAtmosphericScattering))
            out += funcDef(FragmentShaderComponent::ApplyAtmosphericScattering);
        if (has(FragmentShaderComponent::ApplyDraping))
            out += funcDef(FragmentShaderComponent::ApplyDraping);
        if (has(FragmentShaderComponent::ApplyDebugColor))
            out += funcDef(FragmentShaderComponent::ApplyDebugColor);
        // OverrideFeatureId (Surface feature-override recolor): the function-call
        // main convention omitted this slot — the legacy inline path has it
        // (see below). Without it the Surface-Overrides fragment shader dropped
        // the override entirely → hilited features rendered unrecolored.
        // Ported from: itwinjs-core ShaderBuilder.ts main chain (overrideFeatureId
        // is invoked just after assignFragData's baseColor write, before the final
        // fragColor assignment — here it edits baseColor in place).
        if (has(FragmentShaderComponent::OverrideFeatureId))
            out += funcDef(FragmentShaderComponent::OverrideFeatureId);
        out += funcDef(FragmentShaderComponent::AssignFragData);

        // main() call chain. Ported from: itwinjs ShaderBuilder.ts:982-1142.
        std::string main = "void main()\n{\n";
        for (auto const& init : m_initializers)
            main += "    " + init + "\n";

        if (has(FragmentShaderComponent::CheckForEarlyDiscard))
            main += "    if (checkForEarlyDiscard()) { discard; return; }\n";
        if (has(FragmentShaderComponent::FinalizeNormal))
            main += "    g_normal = finalizeNormal();\n";
        main += "    vec4 baseColor = computeBaseColor();\n";
        if (has(FragmentShaderComponent::FinalizeDepth)) {
            main += "    float finalDepth = finalizeDepth();\n";
            main += "    gl_FragDepth = finalDepth;\n";
        }

        bool const hasClip = has(FragmentShaderComponent::ApplyClipping);
        if (hasClip) {
            main += "    vec3 g_clipColor;\n";
            main += "    bvec2 g_hasClipColor = applyClipping(baseColor);\n";
            main += "    if (g_hasClipColor.x) {\n";
            main += "        baseColor.rgb = g_clipColor;\n";
            main += "    } else {\n";
        }
        std::string indent = hasClip ? "        " : "    ";

        if (has(FragmentShaderComponent::ApplyMaterialOverrides))
            main += indent + "baseColor = applyMaterialOverrides(baseColor);\n";
        if (has(FragmentShaderComponent::ApplyThematicDisplay)) {
            main += indent + "if (u_renderPass != kRenderPass_PlanarClassification)\n";
            main += indent + "    baseColor = applyThematicDisplay(baseColor);\n";
        }
        if (has(FragmentShaderComponent::ApplyPlanarClassifier)) {
            if (!has(FragmentShaderComponent::FinalizeDepth))
                main += indent + "float finalDepth = 1.0;\n";
            main += indent + "baseColor = applyPlanarClassifications(baseColor, finalDepth);\n";
        }
        if (has(FragmentShaderComponent::ApplySolarShadowMap))
            main += indent + "baseColor = applySolarShadowMap(baseColor);\n";
        if (has(FragmentShaderComponent::FinalizeBaseColor))
            main += indent + "baseColor = finalizeBaseColor(baseColor);\n";
        if (has(FragmentShaderComponent::CheckForDiscard))
            main += indent + "if (checkForDiscard(baseColor)) { discard; return; }\n";
        if (has(FragmentShaderComponent::DiscardByAlpha))
            main += indent + "if (discardByAlpha(baseColor.a)) { discard; return; }\n";

        if (hasClip)
            main += "    }\n";

        if (has(FragmentShaderComponent::ApplyMonochrome))
            main += "    baseColor = applyMonochrome(baseColor);\n";
        if (has(FragmentShaderComponent::ApplyLighting))
            main += "    baseColor = applyLighting(baseColor);\n";
        if (hasClip) {
            main += "    if (g_hasClipColor.y)\n";
            main += "        baseColor = vec4(g_clipColor, 1.0);\n";
        }
        if (has(FragmentShaderComponent::ReverseWhiteOnWhite))
            main += "    baseColor = reverseWhiteOnWhite(baseColor);\n";
        if (has(FragmentShaderComponent::ApplyContours))
            main += "    baseColor = applyContours(baseColor);\n";
        if (has(FragmentShaderComponent::ApplyFlash))
            main += "    baseColor = applyFlash(baseColor);\n";
        if (has(FragmentShaderComponent::ApplyWiremesh))
            main += "    baseColor = applyWiremesh(baseColor);\n";
        if (has(FragmentShaderComponent::ApplyAtmosphericScattering))
            main += "    baseColor = applyAtmosphericScattering(baseColor);\n";
        // clipIndent quirk: applyDraping keeps the clip-block indent (ref ShaderBuilder.ts:1124).
        if (has(FragmentShaderComponent::ApplyDraping))
            main += indent + "baseColor = applyDraping(baseColor);\n";
        if (has(FragmentShaderComponent::ApplyDebugColor))
            main += "    baseColor = applyDebugColor(baseColor);\n";
        // overrideFeatureId mutates baseColor (the override slot's body assigns
        // baseColor.rgb/.a) — call it BEFORE assignFragData so the recolor lands.
        if (has(FragmentShaderComponent::OverrideFeatureId))
            main += "    baseColor = overrideFeatureId(baseColor);\n";
        main += "    assignFragData(baseColor);\n";

        main += "}\n";
        return out + main;
    }

    // --- Legacy inline-statement convention (Edge/Polyline/Unlit paths) ---
    auto has = [&](FragmentShaderComponent c) -> bool {
        return !m_fragmentComponents[static_cast<size_t>(c)].empty();
    };

    // Helper to append a slot body with proper indentation
    auto appendSlot = [&](std::string& out, FragmentShaderComponent c,
                          std::string const& indent) {
        auto const& body = m_fragmentComponents[static_cast<size_t>(c)];
        if (body.empty()) return;
        out += indent;
        out += body;
        if (out.back() != '\n') out += "\n";
    };

    std::string main = "void main()\n{\n";

    // Initializers
    for (auto const& init : m_initializers)
        main += "    " + init + "\n";

    // Slot 0: CheckForEarlyDiscard
    // Ported from: itwinjs-core ShaderBuilder.ts:996
    if (has(FragmentShaderComponent::CheckForEarlyDiscard)) {
        appendSlot(main, FragmentShaderComponent::CheckForEarlyDiscard, "    ");
        main += "    if (checkForEarlyDiscard()) { discard; return; }\n";
    }

    // Slot 24: FinalizeNormal
    if (has(FragmentShaderComponent::FinalizeNormal)) {
        main += "    // finalizeNormal\n";
        appendSlot(main, FragmentShaderComponent::FinalizeNormal, "    ");
    }

    // Slot 1: ComputeBaseColor (required)
    main += "    // computeBaseColor\n";
    appendSlot(main, FragmentShaderComponent::ComputeBaseColor, "    ");

    // Slot 20: FinalizeDepth — store in local var for use by ApplyPlanarClassifier
    // Ported from: itwinjs-core ShaderBuilder.ts:1009-1011
    if (has(FragmentShaderComponent::FinalizeDepth)) {
        main += "    // finalizeDepth\n";
        appendSlot(main, FragmentShaderComponent::FinalizeDepth, "    ");
        main += "    float finalDepth = finalizeDepth();\n";
        main += "    gl_FragDepth = finalDepth;\n";
    }

    // --- Clipping block (slot 10: ApplyClipping) ---
    // Ported from: itwinjs-core ShaderBuilder.ts lines 1016-1089
    // Structure:
    //   vec3 g_clipColor;
    //   bvec2 g_hasClipColor = applyClipping(baseColor);
    //   if (g_hasClipColor.x) {
    //       baseColor.rgb = g_clipColor;
    //   } else {
    //       ... material/thematic/planar/solar/finalize/discard ...
    //   }
    //   // Monochrome + Lighting are OUTSIDE the clip block
    //   // .y clip color override after lighting
    bool const hasClip = has(FragmentShaderComponent::ApplyClipping);

    if (hasClip) {
        main += "    vec3 g_clipColor;\n";
        main += "    // applyClipping\n";
        appendSlot(main, FragmentShaderComponent::ApplyClipping, "    ");
        main += "    bvec2 g_hasClipColor = applyClipping(baseColor);\n";
        main += "    if (g_hasClipColor.x) {\n";
        main += "        baseColor.rgb = g_clipColor;\n";
        main += "    } else {\n";
    }

    std::string indent = hasClip ? "        " : "    ";

    // Slot 2: ApplyMaterialOverrides
    if (has(FragmentShaderComponent::ApplyMaterialOverrides)) {
        main += indent + "// applyMaterialOverrides\n";
        appendSlot(main, FragmentShaderComponent::ApplyMaterialOverrides, indent);
        main += indent + "baseColor = applyMaterialOverrides(baseColor);\n";
    }

    // Slot 7: ApplyThematicDisplay — guarded by render pass
    // Ported from: itwinjs-core ShaderBuilder.ts:1033-1034
    if (has(FragmentShaderComponent::ApplyThematicDisplay)) {
        main += indent + "// applyThematicDisplay\n";
        appendSlot(main, FragmentShaderComponent::ApplyThematicDisplay, indent);
        main += indent + "if (u_renderPass != kRenderPass_PlanarClassification)\n"
              + indent + "    baseColor = applyThematicDisplay(baseColor);\n";
    }

    // Slot 13: ApplyPlanarClassifier — uses finalDepth
    // Ported from: itwinjs-core ShaderBuilder.ts:1038-1046
    if (has(FragmentShaderComponent::ApplyPlanarClassifier)) {
        main += indent + "// applyPlanarClassifications\n";
        appendSlot(main, FragmentShaderComponent::ApplyPlanarClassifier, indent);
        if (has(FragmentShaderComponent::FinalizeDepth))
            main += indent + "baseColor = applyPlanarClassifications(baseColor, finalDepth);\n";
        else
            main += indent + "baseColor = applyPlanarClassifications(baseColor, 1.0);\n";
    }

    // Slot 15: ApplySolarShadowMap
    if (has(FragmentShaderComponent::ApplySolarShadowMap)) {
        main += indent + "// applySolarShadowMap\n";
        appendSlot(main, FragmentShaderComponent::ApplySolarShadowMap, indent);
        main += indent + "baseColor = applySolarShadowMap(baseColor);\n";
    }

    // Slot 3: FinalizeBaseColor
    if (has(FragmentShaderComponent::FinalizeBaseColor)) {
        main += indent + "// finalizeBaseColor\n";
        appendSlot(main, FragmentShaderComponent::FinalizeBaseColor, indent);
        main += indent + "baseColor = finalizeBaseColor(baseColor);\n";
    }

    // Slot 4: CheckForDiscard — with { discard; return; } for driver compat
    // Ported from: itwinjs-core ShaderBuilder.ts:1049
    if (has(FragmentShaderComponent::CheckForDiscard)) {
        main += indent + "// checkForDiscard\n";
        appendSlot(main, FragmentShaderComponent::CheckForDiscard, indent);
        main += indent + "if (checkForDiscard(baseColor)) { discard; return; }\n";
    }

    // Slot 5: DiscardByAlpha
    if (has(FragmentShaderComponent::DiscardByAlpha)) {
        main += indent + "// discardByAlpha\n";
        appendSlot(main, FragmentShaderComponent::DiscardByAlpha, indent);
        main += indent + "if (discardByAlpha(baseColor.a)) { discard; return; }\n";
    }

    // Close clipping else branch
    if (hasClip)
        main += "    }\n";

    // --- Monochrome + Lighting are OUTSIDE the clipping block ---
    // Ported from: itwinjs-core ShaderBuilder.ts:1076-1085

    // Slot 6: ApplyMonochrome
    if (has(FragmentShaderComponent::ApplyMonochrome)) {
        main += "    // applyMonochrome\n";
        appendSlot(main, FragmentShaderComponent::ApplyMonochrome, "    ");
        main += "    baseColor = applyMonochrome(baseColor);\n";
    }

    // Slot 8: ApplyLighting
    if (has(FragmentShaderComponent::ApplyLighting)) {
        main += "    // applyLighting\n";
        appendSlot(main, FragmentShaderComponent::ApplyLighting, "    ");
        main += "    baseColor = applyLighting(baseColor);\n";
    }

    // Slot 9: ReverseWhiteOnWhite
    if (has(FragmentShaderComponent::ReverseWhiteOnWhite)) {
        main += "    // reverseWhiteOnWhite\n";
        appendSlot(main, FragmentShaderComponent::ReverseWhiteOnWhite, "    ");
        main += "    baseColor = reverseWhiteOnWhite(baseColor);\n";
    }

    // .y clip color override after lighting
    // Ported from: itwinjs-core ShaderBuilder.ts:1087-1089
    if (hasClip) {
        main += "    if (g_hasClipColor.y)\n";
        main += "        baseColor = vec4(g_clipColor, 1.0);\n";
    }

    // --- Post-lighting slots (outside clipping block) ---

    // Slot 11: ApplyContours
    if (has(FragmentShaderComponent::ApplyContours)) {
        appendSlot(main, FragmentShaderComponent::ApplyContours, "    ");
        main += "    baseColor = applyContours(baseColor);\n";
    }

    // Slot 12: ApplyFlash
    if (has(FragmentShaderComponent::ApplyFlash)) {
        appendSlot(main, FragmentShaderComponent::ApplyFlash, "    ");
        main += "    baseColor = applyFlash(baseColor);\n";
    }

    // Slot 16: ApplyWiremesh
    if (has(FragmentShaderComponent::ApplyWiremesh)) {
        appendSlot(main, FragmentShaderComponent::ApplyWiremesh, "    ");
        main += "    baseColor = applyWiremesh(baseColor);\n";
    }

    // Slot 23: ApplyAtmosphericScattering
    if (has(FragmentShaderComponent::ApplyAtmosphericScattering)) {
        appendSlot(main, FragmentShaderComponent::ApplyAtmosphericScattering, "    ");
        main += "    baseColor = applyAtmosphericScattering(baseColor);\n";
    }

    // Slot 14: ApplyDraping
    if (has(FragmentShaderComponent::ApplyDraping)) {
        appendSlot(main, FragmentShaderComponent::ApplyDraping, "    ");
        main += "    baseColor = applyDraping(baseColor);\n";
    }

    // Slot 21: OverrideColor
    if (has(FragmentShaderComponent::OverrideColor)) {
        appendSlot(main, FragmentShaderComponent::OverrideColor, "    ");
        main += "    baseColor = overrideColor(baseColor);\n";
    }

    // Slot 17: ApplyDebugColor
    if (has(FragmentShaderComponent::ApplyDebugColor)) {
        appendSlot(main, FragmentShaderComponent::ApplyDebugColor, "    ");
        main += "    baseColor = applyDebugColor(baseColor);\n";
    }

    // Slot 18: AssignFragData (required)
    main += "    // assignFragData\n";
    appendSlot(main, FragmentShaderComponent::AssignFragData, "    ");
    main += "    assignFragData(baseColor);\n";

    // Slot 19: OverrideFeatureId
    if (has(FragmentShaderComponent::OverrideFeatureId)) {
        appendSlot(main, FragmentShaderComponent::OverrideFeatureId, "    ");
        main += "    overrideFeatureId();\n";
    }

    main += "}\n";
    return main;
}

// ---------------------------------------------------------------------------
// buildSourceWithComponents — full source with component-assembled main()
// Ported from: itwinjs-core ShaderBuilder.ts buildPreludeCommon()
// ---------------------------------------------------------------------------
std::string ShaderBuilder::buildSourceWithComponents() const
{
    std::string source;

    // 1. Version
    // Ported from: itwinjs-core ShaderBuilder.buildPreludeCommon() (line 587-636)
    source += "#version " + m_version + "\n\n";

    // 2. Macros (defines)
    source += buildMacros(m_macros);

    // 3. Extensions
    for (auto const& ext : m_extensions)
        source += "#extension " + ext + " : enable\n";
    if (!m_extensions.empty()) source += "\n";

    // 4. Variable declarations
    source += m_variables.buildDeclarations(m_stage == ShaderStage::Vertex);

    // 5. Fragment output declarations
    // Ported from: itwinjs-core ShaderBuilder.addFragOutput() (line 579-585)
    // When location == -1, emit "out vec4 name;" without layout qualifier.
    for (auto const& [name, location] : m_fragOutputs) {
        if (location == -1)
            source += "out vec4 " + name + ";\n";
        else
            source += "layout(location = " + std::to_string(location) + ") out vec4 " + name + ";\n";
    }
    if (!m_fragOutputs.empty()) source += "\n";

    // 6. Code snippets (extra output declarations etc.)
    for (auto const& code : m_codeSnippets)
        source += code + "\n";

    // 7. Functions
    for (auto const& func : m_functions)
        source += func + "\n\n";

    // 8. main() with components
    bool hasVertexComponents = false;
    for (size_t i = 0; i < static_cast<size_t>(VertexShaderComponent::Count); ++i) {
        if (!m_vertexComponents[i].empty()) { hasVertexComponents = true; break; }
    }

    bool hasFragmentComponents = false;
    for (size_t i = 0; i < static_cast<size_t>(FragmentShaderComponent::Count); ++i) {
        if (!m_fragmentComponents[i].empty()) { hasFragmentComponents = true; break; }
    }

    if (hasVertexComponents)
        source += buildVertexMain();
    else if (hasFragmentComponents)
        source += buildFragmentMain();

    return source;
}

// ===========================================================================
// ProgramBuilder
// Ported from: itwinjs-core ShaderBuilder.ts ProgramBuilder (line 707-807)
// ===========================================================================

void ProgramBuilder::addUniform(std::string name, VariableType type,
                                 addVariableBinding binding,
                                 VariablePrecision precision)
{
    m_vertexBuilder.addUniform(name, type, binding, precision);
    m_fragmentBuilder.addUniform(std::move(name), type, binding, precision);
}

// ---------------------------------------------------------------------------
// addUniformArray — add a uniform array to both vertex and fragment builders
// Ported from: itwinjs-core ProgramBuilder.addUniformArray()
// ---------------------------------------------------------------------------
void ProgramBuilder::addUniformArray(std::string name, VariableType type,
                                      int length, addVariableBinding binding)
{
    m_vertexBuilder.addUniformArray(name, type, length, binding);
    m_fragmentBuilder.addUniformArray(std::move(name), type, length, binding);
}

void ProgramBuilder::addVarying(std::string name, VariableType type)
{
    m_vertexBuilder.addVarying(name, type);
    m_fragmentBuilder.addVarying(std::move(name), type);
}

// ---------------------------------------------------------------------------
// addFunctionComputedVarying — add a function to vert + varying to both +
// a computed varying assignment in the vertex main()
// Ported from: itwinjs-core ProgramBuilder.addFunctionComputedVarying()
// ---------------------------------------------------------------------------
void ProgramBuilder::addFunctionComputedVarying(
    std::string const& name, VariableType type,
    std::string const& funcName, std::string const& funcBody)
{
    // add varying to both stages
    addVarying(name, type);

    // add the computation function to the vertex builder as a COMPLETE GLSL
    // definition: `<type> <funcName>() { <funcBody> }`.
    // The previous impl called the 2-arg addFunction(funcName, funcBody), but
    // buildFunctionDefinition treats an implementation that starts with '\n'
    // as an inline decl+impl concatenation — emitting `funcName <body>` with no
    // return type, parameter list, or braces (not compilable GLSL).  The
    // computed-varying funcBody is a statement list, so we must wrap it here.
    std::string funcDef = std::string(typeToString(type)) + " " + funcName +
                          "()\n{\n" + funcBody + "\n}\n";
    m_vertexBuilder.addFunction(funcDef);

    // add a computed-varying assignment — emitted AFTER the component chain in
    // buildVertexMain (reference ShaderBuilder.ts:833-836), NOT as an
    // initializer (which would run before computeFeatureOverrides and read
    // uninitialized feature_rgb — see buildVertexMain's computed-varyings note).
    m_vertexBuilder.addComputedVarying(name + " = " + funcName + "();");
}

// ---------------------------------------------------------------------------
// addInlineComputedVarying — add a varying to both + inline expression
// Ported from: itwinjs-core ProgramBuilder.addInlineComputedVarying()
// ---------------------------------------------------------------------------
void ProgramBuilder::addInlineComputedVarying(
    std::string const& name, VariableType type,
    std::string const& inlineExpr)
{
    // add varying to both stages
    addVarying(name, type);

    // add a computed-varying assignment (after the component chain — see
    // addFunctionComputedVarying's note)
    m_vertexBuilder.addComputedVarying(name + " = " + inlineExpr + ";");
}

// ---------------------------------------------------------------------------
// addGlobal — add a global variable to both vertex and fragment builders.
// Ported from: itwinjs-core ProgramBuilder.addGlobal() (ShaderType.Both path)
// ---------------------------------------------------------------------------
void ProgramBuilder::addGlobal(std::string name, VariableType type,
                               std::string value, bool isConst)
{
    m_vertexBuilder.addGlobal(name, type, value, isConst);
    m_fragmentBuilder.addGlobal(std::move(name), type, std::move(value), isConst);
}

END_DQ_RENDER_NAMESPACE

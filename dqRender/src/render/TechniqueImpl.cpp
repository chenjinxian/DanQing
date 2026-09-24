// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Technique implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Technique.ts
#include "TechniqueImpl.h"
#include "ShaderBuilder.h"
#include "gl/RenderFlags.h"
#include "shader/EdgeShaderBuilder.h"
#include "shader/PolylineShaderBuilder.h"

#include <cassert>

BEGIN_DQ_RENDER_NAMESPACE

// ===========================================================================
// ClippingProgram
// Ported from: itwinjs-core ClippingProgram.ts
// ===========================================================================

ShaderProgram* ClippingProgram::getProgram(uint8_t numClipPlanes)
{
    if (numClipPlanes == 0 || m_variants.empty())
        return nullptr;
    // Return the variant for the requested number of clip planes.
    // Variants are stored 0-indexed, but clip planes are 1-indexed.
    size_t index = static_cast<size_t>(numClipPlanes) - 1;
    if (index >= m_variants.size())
        return nullptr;
    return &m_variants[index];
}

bool ClippingProgram::compile(rhi::Driver& driver)
{
    bool allCompiled = true;
    for (auto& variant : m_variants) {
        if (variant.compile(driver) != CompileStatus::Success)
            allCompiled = false;
    }
    return allCompiled;
}

// ===========================================================================
// VariedTechnique
// Ported from: itwinjs-core Technique.ts VariedTechnique (line 109-278)
// ===========================================================================

VariedTechnique::VariedTechnique(size_t numPrograms)
    : m_basicPrograms(numPrograms)
    , m_clippingPrograms(numPrograms)
{
}

ShaderProgram* VariedTechnique::getShader(TechniqueFlags const& flags)
{
    size_t index = getShaderIndex(flags);

    // If clip planes are active, use the clipping program variant.
    if (flags.hasClip()) {
        auto& clipEntry = m_clippingPrograms[index];
        auto* prog = clipEntry.getProgram(flags.numClipPlanes);
        if (prog)
            return prog;
    }

    // Otherwise use the basic program.
    return &m_basicPrograms[index];
}

bool VariedTechnique::compileShaders(rhi::Driver& driver)
{
    bool allCompiled = true;
    for (auto& prog : m_basicPrograms) {
        if (prog.compile(driver) != CompileStatus::Success)
            allCompiled = false;
    }
    for (auto& clip : m_clippingPrograms) {
        if (!clip.compile(driver))
            allCompiled = false;
    }
    return allCompiled;
}

void VariedTechnique::addShader(ShaderProgram program, TechniqueFlags const& flags)
{
    size_t index = getShaderIndex(flags);
    assert(index < m_basicPrograms.size());
    m_basicPrograms[index] = std::move(program);
}

void VariedTechnique::addHiliteShader(ShaderProgram program, bool instanced,
                                        bool classified, PositionType posType)
{
    TechniqueFlags flags;
    flags.initForHilite(0, instanced, classified, posType);
    addShader(std::move(program), flags);
}

void VariedTechnique::addTranslucentShader(ShaderProgram program,
                                             TechniqueFlags const& flags)
{
    TechniqueFlags translucentFlags = flags;
    translucentFlags.isTranslucent = true;
    addShader(std::move(program), translucentFlags);
}

size_t VariedTechnique::getShaderIndex(TechniqueFlags const& flags) const
{
    // Ported from: itwinjs-core Technique.ts getShaderIndex (line 236-240)：
    //   assert(!flags.isHilite || (!flags.isTranslucent &&
    //          (IsClassified.Yes === flags.isClassified || flags.hasFeatures)),
    //          "invalid technique flags");
    assert(!flags.isHilite() ||
           (!flags.isTranslucent && (flags.isClassified || flags.hasFeatures())));
    size_t index = computeShaderIndex(flags);
    assert(index < m_basicPrograms.size());
    // Safety net (survives Release where assert is stripped): clamp into range
    // to guarantee no OOB access. Returning index 0 (a valid default shader) is
    // preferable to memory corruption; the assert still fires in Debug to flag
    // any future computeShaderIndex drift. Mirrors ref's `assert(index < length)`
    // intent with a Release-safe fallback.
    if (index >= m_basicPrograms.size())
        index = 0u;
    return index;
}

// ===========================================================================
// SurfaceTechnique / PolylineTechnique / EdgeTechnique — DELETED.
// These VariedTechnique subclasses used flat GLSL strings (no ShaderBuilder,
// no addBindings) and were never registered by RenderPipeline. The live
// Surface/Edge path uses MultiVariantTechnique + SurfaceVariantCompiler/
// EdgeVariantCompiler (ShaderBuilder + buildProgram + addBindings).
// ===========================================================================


// Helper: create a point-string shader program (used by PointStringTechnique
// and PointCloudTechnique). Uses createPointStringProgramBuilder (the round-
// point shader: gl_PointSize + gl_PointCoord + premultiply). The real Polyline
// technique (thick-line miter shader) is NOT built here — it is registered in
// RenderPipeline.cpp as a MultiVariantTechnique with PolylineVariantCompiler,
// mirroring itwinjs-core Technique.ts:445-476 (Polyline registration).
static ShaderProgram createPointStringShader(rhi::Driver& /*driver*/,
                                             TechniqueFlags const& flags)
{
    auto builder = createPointStringProgramBuilder(flags.featureMode, flags.positionType);

    ShaderProgram prog;
    prog.setSource(builder.getVertexBuilder().buildSourceWithComponents(),
                   builder.getFragmentBuilder().buildSourceWithComponents(),
                   "PointString: " + flags.buildDescription());

    // Explicit attribute locations (glBindAttribLocation before link), matching
    // the shared MeshGraphic vertex layout (pos=0, normal=1, color=2, featureId=3)
    // that all point geometry uploads. Without this GL auto-assigns a_color to
    // slot 1 → it reads the NORMAL data → the point renders the wrong color and is
    // invisible (the ACS Z-axis tip bug). Mirrors SurfaceVariantCompiler's
    // setAttributeMap (AttributeMap in itwinjs-core).
    prog.setAttributeMap({
        {"a_position", 0},
        {"a_color", 2},
    });

    return prog;
}

// ===========================================================================
// PointStringTechnique
// Ported from: itwinjs-core Technique.ts PointStringTechnique (line 559-614)
// ===========================================================================

PointStringTechnique::PointStringTechnique(rhi::Driver& driver)
    // _kUnquantized * 2 = 28 槽（Technique.ts:566 super(_kUnquantized * 2)）。
    : VariedTechnique(28)
{
    // 参考构造遍历 posType × instanced × featureModes[None,Pick,Overrides] 并为
    // 每个 featureMode 加 translucent 变体（Technique.ts:568-597）；DanQing 简化
    // 子集（无 instanced 变体——TODO 全量 ctor 移植），featureModes×translucent
    // 全量保留：readPixels 的 Pick 模式与半透明 pass 都会在运行期请求这些槽位，
    // 缺槽 = 空程序（画空）。
    TechniqueFlags flags;
    for (int posTypeIdx = 0; posTypeIdx <= 1; ++posTypeIdx) {
        auto posType = static_cast<PositionType>(posTypeIdx);
        addHiliteShader(createPointStringShader(driver, flags), false, false, posType);
        for (auto featMode :
             {FeatureMode::None, FeatureMode::Pick, FeatureMode::Overrides}) {
            flags.reset(featMode, false, false, false, posType);
            addShader(createPointStringShader(driver, flags), flags);
            addTranslucentShader(createPointStringShader(driver, flags), flags);
        }
    }
}

// Ported from: itwinjs-core Technique.ts PointStringTechnique.computeShaderIndex (line 603-613)
size_t PointStringTechnique::computeShaderIndex(TechniqueFlags const& flags) const
{
    // 常量（Technique.ts:560-566）：_kOpaque=0, _kTranslucent=1, _kInstanced=2,
    // _kFeature=4；_kHilite=numFeatureVariants(4)=4*3=12（featureModes 共 3 个）；
    // _kUnquantized=_kHilite+numHiliteVariants=12+2=14。
    constexpr size_t kFeature = 4;
    constexpr size_t kHilite = kFeature * 3;
    constexpr size_t kUnquantized = kHilite + 2;
    size_t const idxOffset =
        flags.positionType == PositionType::Quantized ? 0 : kUnquantized;
    if (flags.isHilite())
        return kHilite + (flags.isInstanced ? 1 : 0) + idxOffset;
    size_t index = flags.isTranslucent ? 1 : 0;
    index += kFeature * static_cast<size_t>(flags.featureMode);
    index += 2 * (flags.isInstanced ? 1 : 0);
    return index + idxOffset;
}

// ===========================================================================
// PointCloudTechnique
// Ported from: itwinjs-core Technique.ts PointCloudTechnique (line 617-680)
// ===========================================================================

PointCloudTechnique::PointCloudTechnique(rhi::Driver& driver)
    // _kHilite + 2 = 10 槽（Technique.ts:620 super(_kHilite + 2)）。
    : VariedTechnique(10)
{
    // 参考构造：classified × thematic × featureModes[None,Overrides]，仅 quantized
    //（Technique.ts:622-655）——Unquantized 点云参考不支持（computeShaderIndex 即
    // assert quantized，见下）。此前 DanQing 自创的 posType 循环（两种 PositionType）
    // 配自创索引数学（4/6 偏移 + 8 槽）在 Hilite+Unquantized 组合下越界
    //（index 12/13 ≥ 8）→ Debug 断言崩溃（Release 被 clamp 掩盖）。
    TechniqueFlags flags;
    for (int iClassified = 0; iClassified <= 1; ++iClassified) {
        addHiliteShader(createPointStringShader(driver, flags), false,
                        iClassified != 0, PositionType::Quantized);
        for (int thematic = 0; thematic <= 1; ++thematic) {
            for (auto featMode : {FeatureMode::None, FeatureMode::Overrides}) {
                flags.reset(featMode, false, false, thematic != 0,
                            PositionType::Quantized);
                flags.isClassified = (iClassified != 0);
                addShader(createPointStringShader(driver, flags), flags);
            }
        }
    }
}

// Ported from: itwinjs-core Technique.ts PointCloudTechnique.computeShaderIndex (line 649-660)
size_t PointCloudTechnique::computeShaderIndex(TechniqueFlags const& flags) const
{
    constexpr size_t kHilite = 8;  // _kHilite（Technique.ts:618）
    assert(flags.positionType == PositionType::Quantized &&
           "Unquantized point cloud positions not currently supported");
    if (flags.isHilite())
        return kHilite + (flags.isClassified ? 1 : 0);
    size_t ndx = 0;
    if (flags.isClassified) ndx++;
    if (flags.featureMode != FeatureMode::None) ndx += 2;
    if (flags.isThematic) ndx += 4;
    return ndx;
}

// ===========================================================================
// Techniques
// ===========================================================================

Technique* Techniques::getTechnique(TechniqueId id)
{
    auto index = static_cast<size_t>(id);
    if (index >= getCount()) return nullptr;
    return m_techniques[index].get();
}

void Techniques::registerTechnique(TechniqueId id, std::unique_ptr<Technique> technique)
{
    auto index = static_cast<size_t>(id);
    if (index >= getCount()) return;
    m_techniques[index] = std::move(technique);
}

// ===========================================================================
// TechniqueFlags::init()
// Ported from: itwinjs-core TechniqueFlags.ts init() (line 81-125)
// ===========================================================================

void TechniqueFlags::init(RenderPass pass, FeatureMode featMode, uint8_t numPlanes,
                           bool instanced, bool animated, bool classified,
                           bool shadowable, bool thematic, bool wiremesh,
                           PositionType posType, bool enableAtmo, bool is3d,
                           int renderMode, bool visibleEdges,
                           bool forceSurfaceDiscard, bool wantAO) noexcept
{
    positionType = posType;

    // Hilite passes get special flags.
    // Ported from: itwinjs-core TechniqueFlags.ts init() (line 86-88)
    if (pass == RenderPass::Hilite || pass == RenderPass::HiliteClassification
        || pass == RenderPass::HilitePlanarClassification) {
#pragma warning(suppress : 4458) // 局部 isClassified 与 itwinjs 参考 1:1（TechniqueFlags.init），遮蔽同名成员仅 MSVC /W4 提示
        bool isClassified = (classified && pass == RenderPass::HilitePlanarClassification);
        initForHilite(numPlanes, instanced, isClassified, posType);
        return;
    }

    m_isHilite = false;
    isTranslucent = (pass == RenderPass::Translucent);
    numClipPlanes = numPlanes;
    isAnimated = animated;
    isInstanced = instanced;
    isClassified = classified;
    isShadowable = shadowable;
    isThematic = thematic;
    isWiremesh = wiremesh;
    enableAtmosphere = enableAtmo;
    featureMode = featMode;

    // Determine edge test: needed if features are present and not classified.
    // Ported from: itwinjs-core TechniqueFlags.ts (line 102-123)
    isEdgeTestNeeded = hasFeatures() && !classified;
    if (!forceSurfaceDiscard && is3d && isEdgeTestNeeded) {
        // Wireframe=0: only edges, no surface discard needed.
        if (renderMode == 0 /* Wireframe */) {
            isEdgeTestNeeded = false;
        }
        // SmoothShade=6: if no visible edges and no AO, no surface discard.
        else if (renderMode == 6 /* SmoothShade */) {
            if (!visibleEdges && !wantAO && pass != RenderPass::PlanarClassification) {
                isEdgeTestNeeded = false;
            }
        }
        // SolidFill=4 and HiddenLine=3 always need edge test.
    }
}

// ===========================================================================
// Techniques — idle compilation
// Ported from: itwinjs-core Techniques.idleCompileNextShader() (line 1021-1061)
// ===========================================================================

bool Techniques::idleCompileNextShader(rhi::Driver& driver)
{
    // Iterate techniques and compile the next uncompiled shader.
    // Ported from: itwinjs-core Techniques.idleCompileNextShader()
    for (size_t i = m_nextCompileIndex; i < kTechniqueCount; ++i) {
        auto& tech = m_techniques[i];
        if (!tech) continue;

        if (tech->compileShaders(driver)) {
            m_nextCompileIndex = i + 1;
            return true;
        }
    }
    return false;
}

END_DQ_RENDER_NAMESPACE

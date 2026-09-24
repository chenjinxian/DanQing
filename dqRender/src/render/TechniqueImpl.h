// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Technique implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Technique.ts
//
// A Technique manages multiple shader program variants for a single rendering
// concept (e.g., Surface, Edge, Polyline).  The correct variant is selected
// by TechniqueFlags.
#pragma once

#include "ShaderProgramImpl.h"
#include "gl/RenderFlags.h"
#include "dqRender/rhi/Driver.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// TechniqueId — identifies a rendering technique
// (Ported from: itwinjs-core TechniqueId.ts)
// ---------------------------------------------------------------------------
enum class TechniqueId : int32_t {
    Invalid = -1,
    Surface,
    Polyline,
    PointCloud,
    PointString,
    Edge,
    SilhouetteEdge,
    IndexedEdge,
    RealityMesh,
    PlanarGrid,

    // Post-process techniques
    CompositeHilite,
    CompositeTranslucent,
    CompositeHiliteAndTranslucent,
    CompositeOcclusion,
    CompositeTranslucentAndOcclusion,
    CompositeHiliteAndOcclusion,
    CompositeAll,
    OITClearTranslucent,
    CopyPickBuffers,
    CopyColor,
    CopyColorNoAlpha,
    VolClassColorUsingStencil,
    ClearPickAndColor,
    EVSMFromDepth,
    SkyBox,
    SkySphereGradient,
    SkySphereTexture,
    AmbientOcclusion,
    Blur,
    BlurTestOrder,
    CombineTextures,
    Combine3Textures,
    VolClassCopyZ,
    VolClassSetBlend,
    VolClassBlend,
    EDLCalcBasic,
    EDLCalcFull,
    EDLFilter,
    EDLMix,

    NumBuiltIn,
    COUNT = NumBuiltIn,
};

// ---------------------------------------------------------------------------
// FeatureMode — feature rendering mode
// (Ported from: itwinjs-core TechniqueFlags.ts FeatureMode)
// ---------------------------------------------------------------------------
enum class FeatureMode : uint8_t {
    None = 0,
    Pick = 1,
    Overrides = 2,
};

// ---------------------------------------------------------------------------
// PositionType — vertex position encoding
// (Ported from: itwinjs-core TechniqueFlags.ts PositionType)
// ---------------------------------------------------------------------------
enum class PositionType : uint8_t {
    Quantized,
    Unquantized,
};

// ---------------------------------------------------------------------------
// TechniqueFlags — selects which shader variant to use
// (Ported from: itwinjs-core TechniqueFlags.ts)
// ---------------------------------------------------------------------------
class TechniqueFlags {
public:
    TechniqueFlags() noexcept = default;
    explicit TechniqueFlags(bool translucent) noexcept : isTranslucent(translucent) {}

    // --- Fields (Ported from: itwinjs-core TechniqueFlags.ts) ---
    uint8_t numClipPlanes = 0;
    FeatureMode featureMode = FeatureMode::None;
    bool isTranslucent = false;
    bool isEdgeTestNeeded = false;
    bool isAnimated = false;
    bool isInstanced = false;
    bool isClassified = false;
    bool isShadowable = false;
    bool isThematic = false;
    bool isWiremesh = false;
    PositionType positionType = PositionType::Quantized;
    bool enableAtmosphere = false;

    // --- Derived getters (Ported from: itwinjs-core TechniqueFlags.ts) ---

    /// Whether clip planes are active.
    bool hasClip() const noexcept { return numClipPlanes > 0; }

    /// Whether features are present (Pick or Overrides mode).
    bool hasFeatures() const noexcept { return featureMode != FeatureMode::None; }

    /// Whether quantized positions are used.
    bool usesQuantizedPositions() const noexcept { return positionType == PositionType::Quantized; }

    /// Whether this is a hilite pass.
    bool isHilite() const noexcept { return m_isHilite; }

    // --- Methods (Ported from: itwinjs-core TechniqueFlags.ts) ---

    /// Compare all fields for equality.
    bool equals(TechniqueFlags const& other) const noexcept
    {
        return numClipPlanes == other.numClipPlanes
            && featureMode == other.featureMode
            && isTranslucent == other.isTranslucent
            && isEdgeTestNeeded == other.isEdgeTestNeeded
            && isAnimated == other.isAnimated
            && isInstanced == other.isInstanced
            && isClassified == other.isClassified
            && isShadowable == other.isShadowable
            && isThematic == other.isThematic
            && isWiremesh == other.isWiremesh
            && positionType == other.positionType
            && enableAtmosphere == other.enableAtmosphere
            && isHilite() == other.isHilite();
    }

    /// Build a dash-separated description string.
    std::string buildDescription() const;

    /// Create flags from a description string.
    static TechniqueFlags fromDescription(std::string const& description);

    /// Reset all fields to known values.
    void reset(FeatureMode mode, bool instanced, bool shadowable, bool thematic,
               PositionType posType) noexcept
    {
        m_isHilite = false;
        featureMode = mode;
        isTranslucent = false;
        isEdgeTestNeeded = false;
        isAnimated = false;
        isClassified = false;
        isInstanced = instanced;
        isShadowable = shadowable;
        isThematic = thematic;
        isWiremesh = false;
        positionType = posType;
        enableAtmosphere = false;
        numClipPlanes = 0;
    }

    /// Set flags for hilite rendering.
    void initForHilite(uint8_t clipPlanes, bool instanced, bool classified,
                       PositionType posType) noexcept
    {
        featureMode = classified ? FeatureMode::None : FeatureMode::Overrides;
        m_isHilite = true;
        isTranslucent = false;
        isEdgeTestNeeded = false;
        isAnimated = false;
        isInstanced = instanced;
        isClassified = classified;
        numClipPlanes = clipPlanes;
        positionType = posType;
    }

    /// Initialize flags from rendering context.
    /// Ported from: itwinjs-core TechniqueFlags.ts init() (line 81-125)
    ///
    /// @param pass Current render pass
    /// @param featureMode Feature mode from batch state
    /// @param numClipPlanes Number of active clip planes from clip stack
    /// @param instanced Whether geometry is instanced
    /// @param animated Whether geometry is animated
    /// @param classified Whether geometry is classified
    /// @param shadowable Whether shadows are enabled
    /// @param thematic Whether thematic rendering is enabled
    /// @param wiremesh Whether wiremesh rendering is enabled
    /// @param posType Position encoding type
    /// @param enableAtmosphere Whether atmosphere is enabled
    /// @param is3d Whether this is a 3D view
    /// @param renderMode Current render mode from view flags (dqCommon::RenderMode)
    /// @param visibleEdges Whether visible edges are enabled
    /// @param forceSurfaceDiscard Whether to force surface discard
    /// @param wantAO Whether ambient occlusion is desired
    void init(RenderPass pass, FeatureMode featureMode, uint8_t numClipPlanes,
              bool instanced, bool animated, bool classified, bool shadowable,
              bool thematic, bool wiremesh, PositionType posType,
              bool enableAtmosphere, bool is3d, int renderMode,
              bool visibleEdges, bool forceSurfaceDiscard, bool wantAO) noexcept;

    // --- Helper setters (boolean-to-enum converters) ---
    void setAnimated(bool animated) noexcept { isAnimated = animated; }
    void setInstanced(bool instanced) noexcept { isInstanced = instanced; }
    void setClassified(bool classified) noexcept { isClassified = classified; }

private:
    bool m_isHilite = false;
};

// ---------------------------------------------------------------------------
// TechniqueFlags inline implementations
// Ported from: itwinjs-core TechniqueFlags.ts
// ---------------------------------------------------------------------------

inline std::string TechniqueFlags::buildDescription() const
{
    std::string desc;
    desc += isTranslucent ? "Translucent" : "Opaque";

    if (isInstanced) desc += "-Instanced";
    if (isEdgeTestNeeded) desc += "-EdgeTestNeeded";
    if (isAnimated) desc += "-Animated";
    if (isHilite()) desc += "-Hilite";
    if (isClassified) desc += "-Classified";
    if (hasClip()) desc += "-Clip";
    if (isShadowable) desc += "-Shadowable";
    if (isThematic) desc += "-Thematic";
    if (hasFeatures()) {
        desc += (featureMode == FeatureMode::Pick) ? "-Pick" : "-Overrides";
    }
    if (isWiremesh) desc += "-Wiremesh";
    if (positionType == PositionType::Unquantized) desc += "-Unquantized";
    if (enableAtmosphere) desc += "-EnableAtmosphere";

    return desc;
}

inline TechniqueFlags TechniqueFlags::fromDescription(std::string const& description)
{
    TechniqueFlags flags(false);

    // Split on '-' and process each part
    size_t start = 0;
    while (start < description.size()) {
        size_t end = description.find('-', start);
        if (end == std::string::npos) end = description.size();
        std::string part = description.substr(start, end - start);

        if (part == "Translucent") flags.isTranslucent = true;
        else if (part == "Instanced") flags.isInstanced = true;
        else if (part == "EdgeTestNeeded") flags.isEdgeTestNeeded = true;
        else if (part == "Animated") flags.isAnimated = true;
        else if (part == "Hilite") flags.m_isHilite = true;
        else if (part == "Classified") flags.isClassified = true;
        else if (part == "Clip") flags.numClipPlanes = 1;
        else if (part == "Shadowable") flags.isShadowable = true;
        else if (part == "Thematic") flags.isThematic = true;
        else if (part == "Wiremesh") flags.isWiremesh = true;
        else if (part == "Unquantized") flags.positionType = PositionType::Unquantized;
        else if (part == "EnableAtmosphere") flags.enableAtmosphere = true;
        else if (part == "Pick") flags.featureMode = FeatureMode::Pick;
        else if (part == "Overrides") flags.featureMode = FeatureMode::Overrides;

        start = end + 1;
    }

    return flags;
}

// ---------------------------------------------------------------------------
// Technique — base class for shader variant management
// (Ported from: itwinjs-core Technique.ts)
// ---------------------------------------------------------------------------
class Technique {
public:
    virtual ~Technique() = default;

    /// Get the shader for the given flags.
    virtual ShaderProgram* getShader(TechniqueFlags const& flags) = 0;

    /// Get the number of shader variants.
    virtual size_t getShaderCount() const = 0;

    /// compile all shaders.  Returns true if all compiled successfully.
    virtual bool compileShaders(rhi::Driver& driver) = 0;
};

// ---------------------------------------------------------------------------
// SingularTechnique — wraps a single shader program
// ---------------------------------------------------------------------------
class SingularTechnique : public Technique {
public:
    explicit SingularTechnique(ShaderProgram program)
        : m_program(std::move(program)) {}

    ShaderProgram* getShader(TechniqueFlags const&) override { return &m_program; }
    size_t getShaderCount() const override { return 1; }
    bool compileShaders(rhi::Driver&) override { return m_program.isValid(); }

private:
    ShaderProgram m_program;
};

// ---------------------------------------------------------------------------
// ClippingProgram — wraps a basic program with clip plane variants
// Ported from: itwinjs-core ClippingProgram.ts
//
// For each number of clip planes (1..N), creates a variant of the shader
// with the appropriate clip plane uniforms and discard logic.
// ---------------------------------------------------------------------------
class ClippingProgram {
public:
    ClippingProgram() = default;
    ~ClippingProgram() = default;

    /// Get or create the program variant for the given number of clip planes.
    ShaderProgram* getProgram(uint8_t numClipPlanes);

    /// compile all variants.
    bool compile(rhi::Driver& driver);

    /// Get the number of variants.
    size_t getVariantCount() const { return m_variants.size(); }

private:
    std::vector<ShaderProgram> m_variants;
};

// ---------------------------------------------------------------------------
// VariedTechnique — abstract base for techniques with multiple shader variants
// Ported from: itwinjs-core Technique.ts VariedTechnique (line 109-278)
//
// Manages an array of ShaderProgram (basic) and ClippingProgram (clip variants).
// Concrete subclasses override computeShaderIndex() to map TechniqueFlags → index.
// The constructor iterates over all flag combinations and calls addShader().
// ---------------------------------------------------------------------------
class VariedTechnique : public Technique {
public:
    ~VariedTechnique() override = default;

    ShaderProgram* getShader(TechniqueFlags const& flags) override;
    size_t getShaderCount() const override { return m_basicPrograms.size(); }
    bool compileShaders(rhi::Driver& driver) override;

protected:
    explicit VariedTechnique(size_t numPrograms);

    /// Map TechniqueFlags to an index in the program array.
    /// Must be implemented by concrete subclasses.
    virtual size_t computeShaderIndex(TechniqueFlags const& flags) const = 0;

    /// Debug description for this technique.
    virtual std::string_view getDebugDescription() const = 0;

    /// add a shader program for the given flags.
    /// The builder is used to create the program, which is stored at the
    /// index returned by computeShaderIndex(flags).
    void addShader(ShaderProgram program, TechniqueFlags const& flags);

    /// add a hilite shader variant.
    void addHiliteShader(ShaderProgram program, bool instanced, bool classified,
                          PositionType posType);

    /// add a translucent shader variant.
    void addTranslucentShader(ShaderProgram program, TechniqueFlags const& flags);

    /// Get the shader index for the given flags (with bounds checking).
    size_t getShaderIndex(TechniqueFlags const& flags) const;

    // Early-Z workaround flags (for buggy Intel drivers).
    std::vector<TechniqueFlags> m_earlyZFlags;

private:
    std::vector<ShaderProgram> m_basicPrograms;
    std::vector<ClippingProgram> m_clippingPrograms;
};

// SurfaceTechnique / PolylineTechnique / EdgeTechnique — DELETED.
// These VariedTechnique subclasses used flat GLSL strings (no ShaderBuilder,
// no addBindings) and were never registered or compiled. The live Surface/Edge
// path uses MultiVariantTechnique + SurfaceVariantCompiler/EdgeVariantCompiler
// (which build via ShaderBuilder + buildProgram + addBindings). See the
// deleted SurfaceTechnique.cpp/EdgeTechnique.cpp for the old stubs.

// ---------------------------------------------------------------------------
// PointStringTechnique — point string rendering
// Ported from: itwinjs-core Technique.ts PointStringTechnique (line 559-614)
// ---------------------------------------------------------------------------
class PointStringTechnique : public VariedTechnique {
public:
    explicit PointStringTechnique(rhi::Driver& driver);
    PointStringTechnique() : VariedTechnique(28) {}  // _kUnquantized*2（Technique.ts:566）

protected:
    size_t computeShaderIndex(TechniqueFlags const& flags) const override;
    std::string_view getDebugDescription() const override { return "PointString"; }
};

// ---------------------------------------------------------------------------
// PointCloudTechnique — point cloud rendering
// Ported from: itwinjs-core Technique.ts PointCloudTechnique (line 617-680)
// ---------------------------------------------------------------------------
class PointCloudTechnique : public VariedTechnique {
public:
    explicit PointCloudTechnique(rhi::Driver& driver);
    PointCloudTechnique() : VariedTechnique(10) {}  // _kHilite+2（Technique.ts:620）

protected:
    size_t computeShaderIndex(TechniqueFlags const& flags) const override;
    std::string_view getDebugDescription() const override { return "PointCloud"; }
};

// ---------------------------------------------------------------------------
// Post-process SingularTechnique aliases
// These are simple SingularTechnique wrappers for post-process effects.
// Each creates a single ShaderProgram for its specific purpose.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// SingularTechnique subclasses — real declarations.
// Each technique's .cpp (PlanarGridTechnique.cpp / SilhouetteEdgeTechnique.cpp /
// PostProcessTechniques.cpp / CompositeTechniques.cpp / SkyTechniques.cpp /
// VolumeClassTechniques.cpp / PointCloudTechnique.cpp) defines the ctor +
// compileShaders (which builds real GLSL via kXxxVert/kXxxFrag in shader/*.h).
// These replace the prior DEFINE_SINGULAR_TECHNIQUE macro stubs, which generated
// empty-program classes whose inherited no-op compileShaders left technique
// programs without GLSL -> macOS Apple-GL "corrupt" link failure -> blank viewport.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Technique.ts
// ---------------------------------------------------------------------------

class PlanarGridTechnique : public SingularTechnique {
public:
    PlanarGridTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
// SilhouetteEdgeTechnique: shader (shader/SilhouetteEdgeShaders.h) not yet ported —
// keep as empty stub (not needed for the blank viewport; edges need geometry).
// TODO: port the SilhouetteEdge shader + wire the real .cpp impl.
class SilhouetteEdgeTechnique : public SingularTechnique {
public:
    SilhouetteEdgeTechnique() : SingularTechnique(ShaderProgram()) {}
};

// --- PostProcessTechniques.cpp ---
class OitClearTechnique : public SingularTechnique {
public:
    OitClearTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class OitCompositeTechnique : public SingularTechnique {
public:
    OitCompositeTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class SsaoTechnique : public SingularTechnique {
public:
    SsaoTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class BlurTechnique : public SingularTechnique {
public:
    BlurTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class EdlTechnique : public SingularTechnique {
public:
    EdlTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class CompositeTechnique : public SingularTechnique {
public:
    CompositeTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};

// --- CompositeTechniques.cpp ---
class CompositeHiliteTechnique : public SingularTechnique {
public:
    CompositeHiliteTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class CompositeHiliteAndTranslucentTechnique : public SingularTechnique {
public:
    CompositeHiliteAndTranslucentTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class CompositeOcclusionTechnique : public SingularTechnique {
public:
    CompositeOcclusionTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class CompositeTranslucentAndOcclusionTechnique : public SingularTechnique {
public:
    CompositeTranslucentAndOcclusionTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class CompositeHiliteAndOcclusionTechnique : public SingularTechnique {
public:
    CompositeHiliteAndOcclusionTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class CopyColorTechnique : public SingularTechnique {
public:
    CopyColorTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class CopyColorNoAlphaTechnique : public SingularTechnique {
public:
    CopyColorNoAlphaTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class CopyPickBuffersTechnique : public SingularTechnique {
public:
    CopyPickBuffersTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class ClearPickAndColorTechnique : public SingularTechnique {
public:
    ClearPickAndColorTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class EvsmFromDepthTechnique : public SingularTechnique {
public:
    EvsmFromDepthTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};

// --- SkyTechniques.cpp ---
class SkyBoxTechnique : public SingularTechnique {
public:
    SkyBoxTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class SkySphereGradientTechnique : public SingularTechnique {
public:
    SkySphereGradientTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class SkySphereTextureTechnique : public SingularTechnique {
public:
    SkySphereTextureTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};

// --- PointCloudTechnique.cpp (PointCloud/PointString are VariedTechnique subclasses,
// declared above with the other VariedTechnique techniques; not re-declared here) ---

// --- VolumeClassTechniques.cpp ---
class VolClassColorUsingStencilTechnique : public SingularTechnique {
public:
    VolClassColorUsingStencilTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class VolClassCopyZTechnique : public SingularTechnique {
public:
    VolClassCopyZTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class VolClassSetBlendTechnique : public SingularTechnique {
public:
    VolClassSetBlendTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class VolClassBlendTechnique : public SingularTechnique {
public:
    VolClassBlendTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class BlurTestOrderTechnique : public SingularTechnique {
public:
    BlurTestOrderTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class CombineTexturesTechnique : public SingularTechnique {
public:
    CombineTexturesTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class Combine3TexturesTechnique : public SingularTechnique {
public:
    Combine3TexturesTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class EdlCalcFullTechnique : public SingularTechnique {
public:
    EdlCalcFullTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class EdlFilterTechnique : public SingularTechnique {
public:
    EdlFilterTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};
class EdlMixTechnique : public SingularTechnique {
public:
    EdlMixTechnique();
    bool compileShaders(rhi::Driver& driver) override;
};

// --- Not yet implemented (no .cpp compileShaders): inline empty stub so the class
// exists; inherits SingularTechnique no-op compileShaders. TODO: real GLSL if used. ---
class CompositeAllTechnique : public SingularTechnique {
public:
    CompositeAllTechnique() : SingularTechnique(ShaderProgram()) {}
};

// PointStringTechnique is a VariedTechnique (defined above).

// ---------------------------------------------------------------------------
// VariantShaderCompiler — base class for dynamic shader variant compilation
// Ported from: itwinjs-core Technique.ts (multi-variant shader generation)
// ---------------------------------------------------------------------------
class VariantShaderCompiler {
public:
    virtual ~VariantShaderCompiler() = default;

    /// Build one variant into `prog`: set the GLSL source AND register uniform
    /// bindings (the glsl-module addUniform callbacks become live
    /// ProgramUniform/GraphicUniform binds via addBindings).  Compilation is
    /// performed lazily by ShaderProgram::use() on first draw.
    /// Ported from: itwinjs-core Technique.ts addShader()/builder.buildProgram()
    ///               (line 176-205) — the live shader-construction + binding path.
    virtual void buildProgram(ShaderProgram& prog, TechniqueFlags const& flags) = 0;
    virtual char const* getDescription() const = 0;
};

// MultiVariantTechnique (the live Surface/Edge multi-variant technique) is
// defined in MultiVariantTechnique.h.  It was previously a SingularTechnique
// placeholder here; it now owns its VariantShaderCompiler and lazily builds
// binding-carrying ShaderPrograms.  Include that header to use it.

// ---------------------------------------------------------------------------
// Techniques — container for all techniques
// (Ported from: itwinjs-core Technique.ts Techniques class)
// ---------------------------------------------------------------------------
class Techniques {
public:
    Techniques() = default;
    ~Techniques() = default;

    /// Get a technique by ID.
    Technique* getTechnique(TechniqueId id);

    /// Register a technique at the given ID (takes ownership).
    void registerTechnique(TechniqueId id, std::unique_ptr<Technique> technique);

    /// compile the next shader that hasn't been compiled yet (idle compilation).
    /// Returns true if a shader was compiled, false if all are done.
    bool idleCompileNextShader(rhi::Driver& driver);

    /// Get the total number of techniques.
    static constexpr size_t kTechniqueCount = static_cast<size_t>(TechniqueId::NumBuiltIn);
    static constexpr size_t getCount() { return kTechniqueCount; }

private:
    std::array<std::unique_ptr<Technique>, kTechniqueCount> m_techniques;
    size_t m_nextCompileIndex = 0;  // Ported from: itwinjs-core Techniques._techniqueByPriorityIndex
};

END_DQ_RENDER_NAMESPACE

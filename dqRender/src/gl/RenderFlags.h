// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderFlags
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/RenderFlags.ts
//
// Defines RenderPass, Pass, TextureUnit, RenderOrder, CompositeFlags,
// SurfaceBitIndex, SurfaceFlags, EmphasisFlags, IsTranslucent, GeometryType.
// All enum values, constants, and helper functions match the reference exactly.
#pragma once

#include <cstdint>

// --- RenderPass ---
// Ported from: itwinjs-core RenderFlags.ts RenderPass (line 15-39)
// Ordered list of render passes which produce a rendered frame.
// NOTE: Defined outside any namespace so both GL:: and dqRender:: can use it.
enum class RenderPass : uint8_t {
    None = 0xff,
    Background = 0,
    OpaqueLayers,           // XY planar models render without depth-testing in order based on priority
    OpaqueLinear,           // Linear geometry that is opaque and needs to be written to the pick data buffers
    OpaquePlanar,           // Planar surface geometry that is opaque and needs to be written to the pick data buffers
    PointClouds,
    OpaqueGeneral,          // All other opaque geometry (including point clouds and reality meshes) which are not written to the pick data buffers
    Classification,         // Stencil volumes for normal processing of reality data classification.
    TranslucentLayers,      // Like Layers but drawn without depth write, blending with opaque
    Translucent,
    HiddenEdge,
    Hilite,
    OverlayLayers,          // Like Layers, but drawn atop all other geometry
    WorldOverlay,           // Decorations
    ViewOverlay,            // Decorations
    SkyBox,
    BackgroundMap,
    HiliteClassification,   // Secondary hilite pass for stencil volumes to process hilited classifiers for reality data
    ClassificationByIndex,  // Stencil volumes for processing classification one classifier at a time
    HilitePlanarClassification,
    PlanarClassification,
    VolumeClassifiedRealityData,
    COUNT,
};

namespace GL {

// --- Pass type ---
// Ported from: itwinjs-core RenderFlags.ts Pass (line 49-63)
// Describes which RenderPass(es) a Primitive wants to be rendered in.
enum class Pass : uint8_t {
    SkyBox,                 // "skybox" → RenderPass.SkyBox
    Opaque,                 // "opaque" → RenderPass.OpaqueGeneral
    OpaqueLinear,           // "opaque-linear" → RenderPass.OpaqueLinear
    OpaquePlanar,           // "opaque-planar" → RenderPass.OpaquePlanar
    Translucent,            // "translucent" → RenderPass.Translucent
    PointClouds,            // "point-clouds" → RenderPass.PointClouds
    ViewOverlay,            // "view-overlay" → RenderPass.ViewOverlay
    Classification,         // "classification" → RenderPass.Classification
    None,                   // "none" → RenderPass.None
    // Double passes: rendered in both opaque and translucent passes
    OpaqueTranslucent,      // "opaque-translucent" → OpaqueGeneral + Translucent
    OpaquePlanarTranslucent, // "opaque-planar-translucent" → OpaquePlanar + Translucent
};

// --- Pass helper functions ---
// Ported from: itwinjs-core RenderFlags.ts Pass namespace (line 84-139)

/// Return the RenderPass corresponding to the specified Pass.
inline RenderPass toRenderPass(Pass pass) {
    switch (pass) {
        case Pass::SkyBox: return RenderPass::SkyBox;
        case Pass::Opaque: return RenderPass::OpaqueGeneral;
        case Pass::OpaqueLinear: return RenderPass::OpaqueLinear;
        case Pass::OpaquePlanar: return RenderPass::OpaquePlanar;
        case Pass::Translucent: return RenderPass::Translucent;
        case Pass::PointClouds: return RenderPass::PointClouds;
        case Pass::ViewOverlay: return RenderPass::ViewOverlay;
        case Pass::Classification: return RenderPass::Classification;
        case Pass::None: return RenderPass::None;
        default: return RenderPass::None;
    }
}

/// Return true if the specified Pass renders during RenderPass.Translucent.
inline bool rendersTranslucent(Pass pass) {
    return pass == Pass::Translucent
        || pass == Pass::OpaqueTranslucent
        || pass == Pass::OpaquePlanarTranslucent;
}

/// Return true if the specified Pass renders during one of the opaque RenderPasses.
inline bool rendersOpaque(Pass pass) {
    return pass == Pass::OpaqueTranslucent
        || pass == Pass::OpaquePlanarTranslucent
        || pass == Pass::Opaque
        || pass == Pass::OpaquePlanar
        || pass == Pass::OpaqueLinear
        || pass == Pass::PointClouds;
}

/// Return true if the specified Pass renders both opaque and translucent.
inline bool rendersOpaqueAndTranslucent(Pass pass) {
    return pass == Pass::OpaqueTranslucent || pass == Pass::OpaquePlanarTranslucent;
}

/// Return the opaque RenderPass for a double pass.
inline RenderPass toOpaquePass(Pass pass) {
    return pass == Pass::OpaqueTranslucent ? RenderPass::OpaqueGeneral : RenderPass::OpaquePlanar;
}

// --- GeometryType ---
// Ported from: itwinjs-core RenderFlags.ts GeometryType (line 77-81)
// Describes the type of geometry rendered by a ShaderProgram.
enum class GeometryType : uint8_t {
    IndexedTriangles,
    IndexedPoints,
    ArrayedPoints,
};

// --- TextureUnit ---
// Ported from: itwinjs-core RenderFlags.ts TextureUnit (line 145-201)
// Reserved texture units for specific sampler variables.
// WebGL 2 guarantees a minimum of 16 vertex texture units.
// NOTE: Values use GL_TEXTURE0-based offsets (0x84C0 + index) to match WebGL constants.
enum class TextureUnit : uint32_t {
    // For shaders which know exactly which textures will be used
    Zero  = 0x84C0,  // WebGLRenderingContext.TEXTURE0
    One   = 0x84C1,  // WebGLRenderingContext.TEXTURE1
    Two   = 0x84C2,  // WebGLRenderingContext.TEXTURE2
    Three = 0x84C3,  // WebGLRenderingContext.TEXTURE3
    Four  = 0x84C4,  // WebGLRenderingContext.TEXTURE4
    Five  = 0x84C5,  // WebGLRenderingContext.TEXTURE5
    Six   = 0x84C6,  // WebGLRenderingContext.TEXTURE6
    Seven = 0x84C7,  // WebGLRenderingContext.TEXTURE7

    ClipVolume = Zero,
    FeatureSymbology = One,
    SurfaceTexture = Two,
    LineCode = Two,

    PickFeatureId = Three,
    PickDepthAndOrder = Four,

    VertexLUT = Five,

    // Texture unit 6 is overloaded. Therefore classification, hilite classification, and aux channel are all mutually exclusive.
    AuxChannelLUT = Six,
    PlanarClassification = Six,
    PlanarClassificationHilite = Six,

    // Texture unit 7 is overloaded. Therefore receiving shadows and thematic display are mutually exclusive.
    ShadowMap = Seven,
    ThematicSensors = Seven,

    // Textures used for up to 6 background or overlay map layers.
    RealityMesh0 = Two,
    RealityMesh1 = VertexLUT,  // Reality meshes do not use VertexLUT.
    RealityMesh2 = ShadowMap,  // Shadow map when picking -- PickDepthAndOrder otherwise...
    RealityMesh3 = 0x84C8,     // WebGLRenderingContext.TEXTURE8
    RealityMesh4 = 0x84C9,     // WebGLRenderingContext.TEXTURE9
    RealityMesh5 = 0x84CA,     // WebGLRenderingContext.TEXTURE10

    RealityMeshThematicGradient = 0x84CB,  // WebGLRenderingContext.TEXTURE11

    // Lookup table for indexed edges.
    EdgeLUT = 0x84CC,  // WebGLRenderingContext.TEXTURE12

    // normal map texture.
    NormalMap = 0x84CD,  // WebGLRenderingContext.TEXTURE13

    // Contours texture.
    Contours = 0x84CE,  // WebGLRenderingContext.TEXTURE14

    // Surface Draping textures.
    SurfaceDraping0 = RealityMesh3,
    SurfaceDraping1 = RealityMesh4,
    SurfaceDraping2 = RealityMesh5,
    SurfaceDraping3 = RealityMeshThematicGradient,
    SurfaceDraping4 = 0x84CF,  // WebGLRenderingContext.TEXTURE15
    SurfaceDraping5 = 0x84D0,  // WebGLRenderingContext.TEXTURE16
};

// --- RenderOrder ---
// Ported from: itwinjs-core RenderFlags.ts RenderOrder (line 214-231)
// Defines the order in which primitives are rendered within a GLESList.
// Used to sort primitives from the same element (e.g., blanking fill behind text).
enum class RenderOrder : uint8_t {
    None = 0,
    Background = 1,     // Background map drawn without depth
    BlankingRegion = 2,
    UnlitSurface = 3,   // Distinction only made for whether or not to apply ambient occlusion.
    LitSurface = 4,
    Linear = 5,
    Edge = 6,
    Silhouette = 7,

    PlanarBit = 8,

    PlanarUnlitSurface = UnlitSurface | PlanarBit,   // = 11
    PlanarLitSurface = LitSurface | PlanarBit,        // = 12
    PlanarLinear = Linear | PlanarBit,                // = 13
    PlanarEdge = Edge | PlanarBit,                    // = 14
    PlanarSilhouette = Silhouette | PlanarBit,        // = 15
};

/// Return true if the specified render order is planar.
// Ported from: itwinjs-core RenderFlags.ts isPlanar (line 234-236)
inline bool isPlanar(RenderOrder order) {
    return static_cast<uint8_t>(order) >= static_cast<uint8_t>(RenderOrder::PlanarBit);
}

// --- CompositeFlags ---
// Ported from: itwinjs-core RenderFlags.ts CompositeFlags (line 241-246)
// Flags indicating operations to be performed by the post-process composite step.
enum class CompositeFlags : uint8_t {
    None = 0,
    Translucent = 1 << 0,
    Hilite = 1 << 1,
    AmbientOcclusion = 1 << 2,
};

inline CompositeFlags operator|(CompositeFlags a, CompositeFlags b) {
    return static_cast<CompositeFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline CompositeFlags operator&(CompositeFlags a, CompositeFlags b) {
    return static_cast<CompositeFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

// --- SurfaceBitIndex ---
// Ported from: itwinjs-core RenderFlags.ts SurfaceBitIndex (line 251-265)
// Location in boolean array of SurfaceFlags.
enum class SurfaceBitIndex : uint8_t {
    hasTexture = 0,
    ApplyLighting = 1,
    HasNormals = 2,
    IgnoreMaterial = 3,
    TransparencyThreshold = 4,
    BackgroundFill = 5,
    HasColorAndNormal = 6,
    OverrideRgb = 7,
    HasNormalMap = 8,
    HasMaterialAtlas = 9,
    UseConstantLodTextureMapping = 10,
    UseConstantLodNormalMapMapping = 11,
    Count = 12,
};

// --- SurfaceFlags ---
// Ported from: itwinjs-core RenderFlags.ts SurfaceFlags (line 270-294)
// Describes attributes of a MeshGeometry object. Used to conditionally execute
// portions of shader programs.
enum class SurfaceFlags : uint16_t {
    None = 0,
    hasTexture = 1 << static_cast<uint8_t>(SurfaceBitIndex::hasTexture),
    ApplyLighting = 1 << static_cast<uint8_t>(SurfaceBitIndex::ApplyLighting),
    HasNormals = 1 << static_cast<uint8_t>(SurfaceBitIndex::HasNormals),
    // In u_surfaceFlags provided to shader, indicates material color/specular/alpha should be ignored.
    // has no effect on texture.
    IgnoreMaterial = 1 << static_cast<uint8_t>(SurfaceBitIndex::IgnoreMaterial),
    // In HiddenLine and SolidFill modes, a transparency threshold is supplied;
    // surfaces that are more transparent than the threshold are not rendered.
    TransparencyThreshold = 1 << static_cast<uint8_t>(SurfaceBitIndex::TransparencyThreshold),
    // For HiddenLine mode
    BackgroundFill = 1 << static_cast<uint8_t>(SurfaceBitIndex::BackgroundFill),
    // For textured meshes, the color index in the vertex LUT is unused - we place the normal there instead.
    // For untextured lit meshes, the normal is placed after the feature ID.
    HasColorAndNormal = 1 << static_cast<uint8_t>(SurfaceBitIndex::HasColorAndNormal),
    // For textured meshes, use rgb from v_color instead of from texture.
    OverrideRgb = 1 << static_cast<uint8_t>(SurfaceBitIndex::OverrideRgb),
    // For geometry with fixed normals (terrain meshes) we must avoid front facing normal reversal
    // or skirts will be incorrectly lit.
    HasNormalMap = 1 << static_cast<uint8_t>(SurfaceBitIndex::HasNormalMap),
    HasMaterialAtlas = 1 << static_cast<uint8_t>(SurfaceBitIndex::HasMaterialAtlas),
};

inline SurfaceFlags operator|(SurfaceFlags a, SurfaceFlags b) {
    return static_cast<SurfaceFlags>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline SurfaceFlags operator&(SurfaceFlags a, SurfaceFlags b) {
    return static_cast<SurfaceFlags>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

inline bool hasFlag(SurfaceFlags value, SurfaceFlags flag) {
    return (static_cast<uint16_t>(value) & static_cast<uint16_t>(flag)) != 0;
}

// --- EmphasisFlags ---
// Ported from: itwinjs-core RenderFlags.ts EmphasisFlags (line 299-305)
// 8-bit flags indicating emphasis effects applied to a feature.
enum class EmphasisFlags : uint8_t {
    None = 0,
    Hilite = 1,
    Emphasized = 2,
    Flashed = 4,
    NonLocatable = 8,
};

inline EmphasisFlags operator|(EmphasisFlags a, EmphasisFlags b) {
    return static_cast<EmphasisFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline EmphasisFlags operator&(EmphasisFlags a, EmphasisFlags b) {
    return static_cast<EmphasisFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

// --- IsTranslucent ---
// Ported from: itwinjs-core RenderFlags.ts IsTranslucent (line 308)
enum class IsTranslucent : uint8_t {
    No,
    Yes,
    Maybe,
};

}  // namespace GL

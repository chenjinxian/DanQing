// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — 默认 technique 全集注册（从 RenderPipeline::initialize 抽出，
// 渲染独立验证测试与管线共用同一注册路径）
#include "render/TechniqueRegistry.h"

#include "render/TechniqueImpl.h"
#include "render/PlanarGridTechnique.h"
#include "render/MultiVariantTechnique.h"
#include "render/shader/EdgeShaderBuilder.h"
#include "render/shader/PolylineShaderBuilder.h"
#include "render/SurfaceVariantCompiler.h"
#include "render/PolylineVariantCompiler.h"
#include "render/PostProcessTechniques.h"
#include "render/SkyTechniques.h"
#include "render/CompositeTechniques.h"
#include "render/VolumeClassTechniques.h"
#include "render/PointCloudTechnique.h"  // PointStringTechnique 同头

namespace dqRender {

std::unique_ptr<Techniques> createDefaultTechniques(rhi::Driver& driver)
{
    auto techniques = std::make_unique<Techniques>();
    auto gridTechnique = std::make_unique<PlanarGridTechnique>();
    gridTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::PlanarGrid, std::move(gridTechnique));

    // Register Surface technique (multi-variant: 24 shader variants)
    auto surfaceCompiler = std::make_unique<SurfaceVariantCompiler>();
    auto surfaceTechnique = std::make_unique<MultiVariantTechnique>(std::move(surfaceCompiler));
    surfaceTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::Surface, std::move(surfaceTechnique));

    // Register Polyline technique (multi-variant thick-line miter shader).
    // Ported from: itwinjs-core Technique.ts:445-476 (Polyline registration).
    // Variant programs build lazily in getShader(); compileShaders is a no-op.
    auto polylineCompiler = std::make_unique<PolylineVariantCompiler>();
    auto polylineTechnique = std::make_unique<MultiVariantTechnique>(std::move(polylineCompiler));
    polylineTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::Polyline, std::move(polylineTechnique));

    // Register Edge technique (multi-variant)
    auto edgeCompiler = std::make_unique<EdgeVariantCompiler>();
    auto edgeTechnique = std::make_unique<MultiVariantTechnique>(std::move(edgeCompiler));
    edgeTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::Edge, std::move(edgeTechnique));

    // Register SilhouetteEdge technique
    auto silhouetteTechnique = std::make_unique<SilhouetteEdgeTechnique>();
    silhouetteTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::SilhouetteEdge, std::move(silhouetteTechnique));

    // Register post-process techniques
    auto oitClearTechnique = std::make_unique<OitClearTechnique>();
    oitClearTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::OITClearTranslucent, std::move(oitClearTechnique));

    auto oitCompositeTechnique = std::make_unique<OitCompositeTechnique>();
    oitCompositeTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::CompositeTranslucent, std::move(oitCompositeTechnique));

    auto ssaoTechnique = std::make_unique<SsaoTechnique>();
    ssaoTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::AmbientOcclusion, std::move(ssaoTechnique));

    auto blurTechnique = std::make_unique<BlurTechnique>();
    blurTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::Blur, std::move(blurTechnique));

    auto edlTechnique = std::make_unique<EdlTechnique>();
    edlTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::EDLCalcBasic, std::move(edlTechnique));

    auto compositeTechnique = std::make_unique<CompositeTechnique>();
    compositeTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::CompositeAll, std::move(compositeTechnique));

    // Register point cloud techniques. Pass the driver so the (driver) ctor builds
    // the 8 shader variants (createPointStringShader); the default ctor leaves
    // m_basicPrograms empty, so getShader() returns an unlinked program that never
    // binds (the ACS Z-axis tip invisibility bug).
    auto pointCloudTechnique = std::make_unique<PointCloudTechnique>(driver);
    pointCloudTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::PointCloud, std::move(pointCloudTechnique));

    auto pointStringTechnique = std::make_unique<PointStringTechnique>(driver);
    pointStringTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::PointString, std::move(pointStringTechnique));

    // Register sky techniques
    auto skyBoxTechnique = std::make_unique<SkyBoxTechnique>();
    skyBoxTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::SkyBox, std::move(skyBoxTechnique));

    auto skySphereGradientTechnique = std::make_unique<SkySphereGradientTechnique>();
    skySphereGradientTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::SkySphereGradient, std::move(skySphereGradientTechnique));

    auto skySphereTextureTechnique = std::make_unique<SkySphereTextureTechnique>();
    skySphereTextureTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::SkySphereTexture, std::move(skySphereTextureTechnique));

    // Register composite techniques
    auto compHiliteTechnique = std::make_unique<CompositeHiliteTechnique>();
    compHiliteTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::CompositeHilite, std::move(compHiliteTechnique));

    auto compHiliteTransTechnique = std::make_unique<CompositeHiliteAndTranslucentTechnique>();
    compHiliteTransTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::CompositeHiliteAndTranslucent, std::move(compHiliteTransTechnique));

    auto compOccTechnique = std::make_unique<CompositeOcclusionTechnique>();
    compOccTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::CompositeOcclusion, std::move(compOccTechnique));

    auto compTransOccTechnique = std::make_unique<CompositeTranslucentAndOcclusionTechnique>();
    compTransOccTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::CompositeTranslucentAndOcclusion, std::move(compTransOccTechnique));

    auto compHiliteOccTechnique = std::make_unique<CompositeHiliteAndOcclusionTechnique>();
    compHiliteOccTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::CompositeHiliteAndOcclusion, std::move(compHiliteOccTechnique));

    // Register utility techniques
    auto copyColorTechnique = std::make_unique<CopyColorTechnique>();
    copyColorTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::CopyColor, std::move(copyColorTechnique));

    auto copyColorNoAlphaTechnique = std::make_unique<CopyColorNoAlphaTechnique>();
    copyColorNoAlphaTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::CopyColorNoAlpha, std::move(copyColorNoAlphaTechnique));

    auto copyPickTechnique = std::make_unique<CopyPickBuffersTechnique>();
    copyPickTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::CopyPickBuffers, std::move(copyPickTechnique));

    auto clearPickColorTechnique = std::make_unique<ClearPickAndColorTechnique>();
    clearPickColorTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::ClearPickAndColor, std::move(clearPickColorTechnique));

    auto evsmTechnique = std::make_unique<EvsmFromDepthTechnique>();
    evsmTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::EVSMFromDepth, std::move(evsmTechnique));

    // Register volume classification techniques
    auto volClassColorTechnique = std::make_unique<VolClassColorUsingStencilTechnique>();
    volClassColorTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::VolClassColorUsingStencil, std::move(volClassColorTechnique));

    auto volClassCopyZTechnique = std::make_unique<VolClassCopyZTechnique>();
    volClassCopyZTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::VolClassCopyZ, std::move(volClassCopyZTechnique));

    auto volClassSetBlendTechnique = std::make_unique<VolClassSetBlendTechnique>();
    volClassSetBlendTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::VolClassSetBlend, std::move(volClassSetBlendTechnique));

    auto volClassBlendTechnique = std::make_unique<VolClassBlendTechnique>();
    volClassBlendTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::VolClassBlend, std::move(volClassBlendTechnique));

    auto blurTestOrderTechnique = std::make_unique<BlurTestOrderTechnique>();
    blurTestOrderTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::BlurTestOrder, std::move(blurTestOrderTechnique));

    auto combineTexturesTechnique = std::make_unique<CombineTexturesTechnique>();
    combineTexturesTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::CombineTextures, std::move(combineTexturesTechnique));

    auto combine3TexturesTechnique = std::make_unique<Combine3TexturesTechnique>();
    combine3TexturesTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::Combine3Textures, std::move(combine3TexturesTechnique));

    auto edlCalcFullTechnique = std::make_unique<EdlCalcFullTechnique>();
    edlCalcFullTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::EDLCalcFull, std::move(edlCalcFullTechnique));

    auto edlFilterTechnique = std::make_unique<EdlFilterTechnique>();
    edlFilterTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::EDLFilter, std::move(edlFilterTechnique));

    auto edlMixTechnique = std::make_unique<EdlMixTechnique>();
    edlMixTechnique->compileShaders(driver);
    techniques->registerTechnique(TechniqueId::EDLMix, std::move(edlMixTechnique));
    return techniques;
}

}  // namespace dqRender

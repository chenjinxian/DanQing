# dqRender —— 渲染引擎（DanQing 五模块之渲染层）
#
# 公开头：PublicAPI/dqRender/（唯一可被其他模块 #include 的目录）
# 实现：  src/
# 测试：  tests/
#
# 架构：忠实移植 itwinjs-core core/frontend（渲染逻辑）+ Filament backend/（RHI）
# 整合点：itwinjs System.* GL 调用 → Filament Driver.* RHI 调用

# --- 版本 ---
set(dqRender_VERSION_MAJOR 0)
set(dqRender_VERSION_MINOR 1)
set(dqRender_VERSION_PATCH 0)

# ---------------------------------------------------------------------------
# 平台依赖：OpenGL
# ---------------------------------------------------------------------------
if(APPLE)
    find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
elseif(WIN32)
    find_package(OpenGL REQUIRED)
elseif(UNIX)
    find_package(OpenGL REQUIRED)
    find_package(X11 REQUIRED)
endif()

# ---------------------------------------------------------------------------
# dqRender 静态库（与 dqBase/dqGeom/dqCommon 统一）
#
# 曾为 SHARED：Windows DLL 默认不导出无 dllexport 标注的符号，而 dqRender 的
# 内部类（EdgeGeometry/Branch/RenderCommands 等，src/ 头）被 dqRenderTest 直接
# 引用——mac 的 visibility("default") 全导出掩盖了这一点。STATIC 下行为与 mac
# 等价且 SDK 一致，DQ_RENDER_EXPORT 走 DQ_RENDER_STATIC 空宏分支。
# ---------------------------------------------------------------------------
add_library(dqRender STATIC)
target_compile_definitions(dqRender PUBLIC DQ_RENDER_STATIC=1)

target_sources(dqRender
    PRIVATE
        # --- Module entry ---
        src/DqRender.cpp

        # --- RHI layer (Filament backend/) ---
        # Phase 0: minimal subset
        src/rhi/opengl/OpenGLDriver.cpp
        src/rhi/opengl/GlLoader.cpp
        src/rhi/opengl/OpenGLAttribute.cpp
        src/rhi/opengl/OpenGLContext.cpp
        src/rhi/opengl/OpenGLState.cpp
        src/rhi/opengl/OpenGLProgram.cpp
        src/rhi/opengl/OpenGLSwapchain.cpp
        src/rhi/UniformBuffer.cpp

        # --- Platform layer (Filament backend/src/opengl/platforms/) ---
        src/platform/PlatformFactory.cpp

        # --- Render layer (itwinjs core/frontend) ---
        # Phase 0: minimal skeleton
        src/render/RenderSystemImpl.cpp
        src/render/TargetImpl.cpp
        src/render/GLTimer.cpp        # GPU 计时查询（GLTimer.ts；Profile GPU 数据源）
        src/render/SceneCompositorImpl.cpp
        src/render/CompositorTextures.cpp
        src/render/CompositorFrameBuffers.cpp
        src/render/TechniqueImpl.cpp
        src/render/ShaderProgramImpl.cpp
        src/render/ShaderProgramExecutor.cpp
        src/render/ShaderBuilder.cpp
        src/render/CommonShaders.cpp
        src/render/TargetGraphics.cpp
        src/render/DisplayParams.cpp
        src/render/GeometryPrimitives.cpp
        src/render/ColorMap.cpp
        src/render/VertexKey.cpp
        src/render/MeshPrimitives.cpp
        src/render/MeshBuilder.cpp
        src/render/MeshBuilderMap.cpp
        src/render/GeometryAccumulator.cpp
        src/render/CachedGeometry.cpp
        src/render/IndexedGeometry.cpp
        src/render/RenderState.cpp
        src/render/UniformHandle.cpp
        src/render/RenderCommands.cpp
        src/render/DrawCommand.cpp
        src/render/Graphic.cpp
        src/render/Primitive.cpp
        src/render/AttributeBuffers.cpp
        src/render/AttributeMap.cpp
        src/render/FrameBuffer.cpp
        src/render/RenderBuffer.cpp
        src/render/TextureHandle.cpp
        src/render/OpenGLRenderSystem.cpp
        src/render/OpenGLRenderTarget.cpp
        src/render/PlanarGridGraphic.cpp
        src/render/PolylineTesselator.cpp
        src/render/VertexLutTexture.cpp
        src/render/VertexTableBuilder.cpp
        src/render/SurfaceVariantCompiler.cpp
        src/render/PolylineVariantCompiler.cpp
        src/render/MultiVariantTechnique.cpp
        src/render/PlanarGridTechnique.cpp
        src/render/PostProcessTechniques.cpp
        src/render/CompositeTechniques.cpp
        src/render/SkyTechniques.cpp
        src/render/VolumeClassTechniques.cpp
        src/render/BatchUniforms.cpp
        src/render/Material.cpp
        src/render/ShaderBindings.cpp
        src/render/PolyfaceGraphic.cpp
        src/render/RenderPipeline.cpp
        src/render/TechniqueRegistry.cpp
        src/render/GLCanvasContext.cpp
        src/render/Batch.cpp
        src/render/ClipVolume.cpp
        src/render/ClipStack.cpp
        src/render/BranchStack.cpp
        src/render/TextureAtlas.cpp
        src/render/CubeTextureHandle.cpp
        src/render/TextureDrape.cpp
        src/render/InstanceBuffers.cpp
        src/render/InstancedGeometry.cpp
        src/render/SurfaceGeometry.cpp
        src/render/ViewportQuadGeometry.cpp
        src/render/ThematicDisplay.cpp
        src/render/ThematicSensors.cpp
        src/render/ContourLines.cpp
        src/render/LineCode.cpp
        src/render/RealityMeshGeometry.cpp
        src/render/MeshGraphic.cpp
        src/render/GraphicBuilder.cpp
        src/render/PrimitiveBuilder.cpp
        src/render/FeatureOverrides.cpp
        src/render/FeatureOverrideLUT.cpp
        src/render/DescriptorSetManager.cpp
        src/render/UniformBlockLayout.cpp
        src/render/FeatureSymbology.cpp
        src/render/VisibleTileFeatures.cpp
        src/render/GraphicTemplateImpl.cpp

        # --- Phase 3 infrastructure ---
        src/render/shader/EdgeShaderBuilder.cpp
        src/render/shader/PolylineShaderBuilder.cpp
        src/render/IndexedEdgeGeometry.cpp
        src/render/EdgeSettings.cpp
        src/render/RenderSkyBoxParams.cpp
        src/render/RenderSystemDebugControl.cpp
        src/render/RenderTargetDebugControl.cpp
        src/render/Diagnostics.cpp
        src/render/PlanarTextureProjection.cpp
        src/render/BackgroundMapDrape.cpp
        src/render/LayerCommands.cpp
        src/render/MapLayerParams.cpp
        src/render/IModelFrameLifecycle.cpp
        src/render/ScratchDrawParams.cpp
        src/render/RenderInstancesParamsImpl.cpp
        src/render/ContourUniforms.cpp
        src/render/RealityModelUniforms.cpp
        src/render/AtmosphereUniforms.cpp
        src/render/ThematicUniforms.cpp
        src/render/ShadowUniforms.cpp
        src/render/ViewRectUniforms.cpp
        src/render/HiliteUniforms.cpp
        src/render/LightingUniforms.cpp
        src/render/FrustumUniforms.cpp
        src/render/StyleUniforms.cpp

        # --- glTF reader (cgltf integration) ---
        src/gltf/GltfReader.cpp

        # --- Tile system (Phase 2b) ---
        src/tile/TileContent.cpp
        src/tile/Tile.cpp
        src/tile/TileTree.cpp
        src/tile/TileRequest.cpp
        src/tile/TileRequestChannel.cpp
        src/tile/TileAdmin.cpp
        src/tile/LRUTileList.cpp
        src/tile/GltfTile.cpp
        src/tile/GltfTileTree.cpp
        src/tile/RealityTile.cpp
        src/tile/RealityTileTree.cpp
        src/tile/BatchTableHierarchy.cpp
        src/tile/ImdlHeader.cpp
        src/tile/ImdlDocument.cpp
        src/tile/ImdlGraphics.cpp
        src/tile/ImdlTileTree.cpp
        src/tile/NullTileFetcher.cpp
)

# 平台后端按目标平台纳入（PlatformCocoaGl.mm 仅 APPLE——MSVC 无法编译
# Objective-C++；各平台文件内部亦有 #ifdef 守卫，双保险）
if(APPLE)
    target_sources(dqRender PRIVATE src/platform/PlatformCocoaGl.mm)
elseif(WIN32)
    target_sources(dqRender PRIVATE src/platform/PlatformWgl.cpp)
elseif(UNIX)
    target_sources(dqRender PRIVATE src/platform/PlatformGlx.cpp src/platform/PlatformEgl.cpp)
endif()

# 公开头以 PUBLIC 暴露
target_include_directories(dqRender
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/PublicAPI>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party
        ${CMAKE_SOURCE_DIR}/third_party/glheaders   # Khronos glcorearb.h（Windows GL 头）
)

# 依赖：dqBase（PUBLIC），dqGeom（PUBLIC），dqCommon（PUBLIC），OpenGL（无 Qt——
# §8.2 引擎 SDK 零 Qt：窗口系统交互只经原生句柄，平台层不链接 Qt）。
target_link_libraries(dqRender
    PUBLIC
        dqBase
        dqGeom
        dqCommon
        danqing_compile_options
)

# OpenGL 链接
if(APPLE)
    target_link_libraries(dqRender
        PRIVATE
            ${OPENGL_FRAMEWORK}
            ${COCOA_FRAMEWORK}
            ${COREVIDEO_FRAMEWORK}
    )
    target_compile_definitions(dqRender PRIVATE DQ_RENDER_PLATFORM_MACOS=1)
elseif(WIN32)
    target_link_libraries(dqRender PRIVATE OpenGL::GL)
    target_compile_definitions(dqRender PRIVATE DQ_RENDER_PLATFORM_WINDOWS=1)
elseif(UNIX)
    target_link_libraries(dqRender PRIVATE OpenGL::GL)
    target_compile_definitions(dqRender PRIVATE DQ_RENDER_PLATFORM_LINUX=1)
endif()

# DLL 导出宏（STATIC 下 Export.h 走 DQ_RENDER_STATIC 空宏分支，此定义保留供
# 未来切回 SHARED 时使用）
target_compile_definitions(dqRender PRIVATE DQ_RENDER_BUILDING=1)

# Suppress OpenGL deprecation warnings on macOS (OpenGL is deprecated since 10.14,
# but we need it for Phase 0-2; Metal backend comes in Phase 3)
if(APPLE)
    set_source_files_properties(
        src/platform/PlatformCocoaGl.mm
        src/rhi/opengl/OpenGLContext.cpp
        src/rhi/opengl/OpenGLState.cpp
        src/rhi/opengl/OpenGLProgram.cpp
        src/rhi/opengl/OpenGLDriver.cpp
        PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations"
    )
endif()

# ---------------------------------------------------------------------------
# 单元测试
# ---------------------------------------------------------------------------
if(DANQING_BUILD_TESTS AND TARGET GTest::gtest_main)
    add_executable(dqRenderTest
        tests/RhiTest.cpp
        tests/RenderSmokeTest.cpp
        tests/RenderSystemTest.cpp
        tests/GeometryAccumulatorTest.cpp
        tests/MeshPrimitivesImplTest.cpp
        tests/BranchLocalToWorldTest.cpp
        tests/MeshGraphicDispatchTest.cpp
        tests/MeshRenderGeometryMockTest.cpp
        tests/MeshGraphicLifecycleTest.cpp
        tests/MeshArgsTest.cpp
        tests/PlanarGridTest.cpp
        tests/PlanarGridPropsTest.cpp
        tests/PlanarGridPolygonTest.cpp
        tests/SkySphereMathTest.cpp
        tests/PolyfaceGraphicTest.cpp
        tests/SurfaceShaderTest.cpp
        tests/BatchClipTest.cpp
        tests/TextureInstancingTest.cpp
        tests/RenderingPipelineTest.cpp
        tests/P2P3FeaturesTest.cpp
        tests/BatchATest.cpp
        tests/BatchBTest.cpp
        tests/RemainingItemsTest.cpp
        tests/FeatureSymbologyTest.cpp
        tests/GltfReaderTest.cpp
        tests/TextureWrapTest.cpp
        tests/LRUTileListTest.cpp
        tests/TileAdminMemoryTest.cpp
        tests/ImdlHeaderTest.cpp
        tests/ImdlTileTreeTest.cpp
        tests/ITileFetcherTest.cpp
        tests/TileAdminDiagnosticsTest.cpp  # Debug Info 面板数据源（2026-09-21）
        tests/GLTimerTest.cpp               # GLTimer 嵌套/异步交付语义（Profile GPU）
        tests/UniformBufferTest.cpp
        tests/BatchVTest.cpp
        tests/ContourUniformsTest.cpp
        tests/RealityModelUniformsTest.cpp
        tests/AtmosphereUniformsTest.cpp
        tests/ThematicUniformsTest.cpp
        tests/ShadowUniformsTest.cpp
        tests/ViewRectUniformsTest.cpp
        tests/HiliteUniformsTest.cpp
        tests/LightingUniformsTest.cpp
        tests/FrustumUniformsTest.cpp
        tests/StyleUniformsTest.cpp
        tests/TargetUniformsTest.cpp
        tests/MonochromeShadersTest.cpp
        tests/CommonShadersTest.cpp
        tests/LightingShadersTest.cpp
        tests/InstancingShadersTest.cpp
        tests/RenderPassShadersTest.cpp
        tests/VertexShadersTest.cpp
        tests/ViewportShadersTest.cpp
        tests/UniformBindingTest.cpp
        tests/UniformBindingDispatchTest.cpp   # 绑定派发探针（真 GL + readPixels，TD-15）
        tests/ShaderProgramCompileTest.cpp
        tests/PrerequisitesTest.cpp
        tests/FeatureOverrideLutWebGlTest.cpp
        tests/BranchUniformsUpdateTest.cpp
        tests/GraphicBuilderTest.cpp
        tests/GraphicPrimitiveTest.cpp
        tests/DisplayParamsTest.cpp
        tests/RenderPrimitivesTest.cpp
        tests/GeometryPrimitivesTest.cpp
        tests/SurfaceFlagsShaderTest.cpp
        tests/SurfaceNormalShaderTest.cpp
        tests/SurfaceTextureShaderTest.cpp
        tests/SurfaceMaterialShaderTest.cpp
        tests/SurfaceCompileTest.cpp
        tests/SurfaceRenderPassConstantsTest.cpp
        tests/OpenGLAttributeTest.cpp
        tests/PolylineTesselatorTest.cpp
        tests/VertexTableBuilderTest.cpp
        tests/VertexLutTextureTest.cpp
        tests/PolylineRenderTest.cpp
        tests/PointStringRenderTest.cpp
        tests/CanvasDecorationRenderTest.cpp
        tests/OrientationGroundTruthTest.cpp
        tests/PlanarGridRenderTest.cpp
    )
    # SurfaceCompileTest + PolylineRenderTest include GlslCompileHarness.h, which
    # uses the deprecated macOS OpenGL/CGL API (-Werror is on project-wide).
    # GL_SILENCE_DEPRECATION (in the harness) + this per-TU flag keep them green.
    # （仅 AppleClang 需要；MSVC 无此弃用告警，GCC 风格 flag 会 D8021）
    if(APPLE)
        set_source_files_properties(tests/SurfaceCompileTest.cpp
            PROPERTIES COMPILE_OPTIONS "-Wno-deprecated-declarations")
        set_source_files_properties(tests/PolylineRenderTest.cpp
            PROPERTIES COMPILE_OPTIONS "-Wno-deprecated-declarations")
    endif()
    target_link_libraries(dqRenderTest
        PRIVATE
            dqRender
            GTest::gtest_main
            danqing_compile_options
    )
    # Allow tests to access internal headers
    target_include_directories(dqRenderTest
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/third_party
            ${CMAKE_SOURCE_DIR}/third_party/glheaders
    )
    # Texture tests read the shared PNG asset — compile-time absolute path
    # (no working-directory dependency). dqRender/.. = repo root.
    target_compile_definitions(dqRenderTest PRIVATE
        DANQING_TEST_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    if(APPLE)
        target_link_libraries(dqRenderTest PRIVATE ${OPENGL_FRAMEWORK} ${COCOA_FRAMEWORK})
    endif()
    include(GoogleTest)
    gtest_discover_tests(dqRenderTest)
endif()

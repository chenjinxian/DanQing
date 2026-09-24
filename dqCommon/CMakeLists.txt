# dqCommon — Common types (corresponds to @itwin/core-common)
#
# PublicAPI: PublicAPI/dqCommon/ (only directory other modules may #include)
# Source:    src/
# Tests:    tests/ (ported from itwinjs-core core/common/src/test/)

set(dqCommon_VERSION_MAJOR 0)
set(dqCommon_VERSION_MINOR 1)
set(dqCommon_VERSION_PATCH 0)

# ---------------------------------------------------------------------------
# Static library
# ---------------------------------------------------------------------------
add_library(dqCommon STATIC)

# 静态库：导出宏为空（Windows 下避免 C4273/C4251/C4275，见 Export.h）
target_compile_definitions(dqCommon PUBLIC DQ_COMMON_STATIC=1)

target_sources(dqCommon
    PRIVATE
        src/DqCommon.cpp
        src/ColorDef.cpp
        src/RgbColor.cpp
        src/ViewFlags.cpp
        src/Frustum.cpp
        src/Cartographic.cpp
        src/FeatureTable.cpp
        src/FeatureSymbology.cpp
        src/FeatureOverrides.cpp
        src/Image.cpp
        src/TextureMapping.cpp
        src/Gradient.cpp
        src/SubCategoryAppearance.cpp
        src/GraphicParams.cpp
        src/LightSettings.cpp
        src/HiddenLine.cpp
        src/PackedFeatureTable.cpp
        src/PerModelCategoryVisibility.cpp
        src/DisplayStyleSettings.cpp
)

target_include_directories(dqCommon
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/PublicAPI>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party   # stb_image/stb_image.h（§8.3，仿 dqRender cgltf 接法）
)

# dqCommon depends on dqBase + dqGeom (both PUBLIC for transitive includes)
target_link_libraries(dqCommon
    PUBLIC
        dqBase
        dqGeom
        danqing_compile_options
)

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
if(DANQING_BUILD_TESTS AND TARGET GTest::gtest_main)
    add_executable(dqCommonTest
        tests/ColorDefTest.cpp
        tests/RgbColorTest.cpp
        tests/ViewFlagsTest.cpp
        tests/FrustumTest.cpp
        tests/FeatureTableTest.cpp
        tests/FeatureSymbologyTest.cpp
        tests/ImageTest.cpp
        tests/ImageDecodeTest.cpp
        tests/HiliteGradientTest.cpp
        tests/CameraTest.cpp
        tests/SubCategoryTest.cpp
        tests/DisplayStyleTest.cpp
        tests/GeometryStreamTest.cpp
        tests/QPointTest.cpp
        tests/PackedFeatureTableTest.cpp
        tests/UntestedHeadersTest.cpp
        tests/DisplayTypesTest.cpp
        tests/DisplayStyleSettingsTest.cpp
        tests/RenderMaterialTextureTest.cpp
        tests/TextureMappingTest.cpp
        tests/FeatureOverridesTest.cpp
        tests/GraphicParamsTest.cpp
        tests/GradientThematicTest.cpp
        tests/GeometryStreamBuilderTest.cpp
        tests/Tier2DisplayTypesTest.cpp
        tests/Tier3DisplayTypesTest.cpp
        tests/Tier4DisplayTypesTest.cpp
        tests/Tier5DisplayTypesTest.cpp
        tests/GeometryStreamIteratorTest.cpp
        tests/CategorySelectorStateTest.cpp
        tests/ModelSelectorStateTest.cpp
        tests/FrustumPlaneIntersectionTest.cpp
    )
    target_link_libraries(dqCommonTest
        PRIVATE
            dqCommon
            GTest::gtest_main
            danqing_compile_options
    )
    target_include_directories(dqCommonTest
        PRIVATE
            ${CMAKE_SOURCE_DIR}/third_party   # stb_image/stb_image.h（§8.3，仿 dqRender cgltf 接法）
    )
    include(GoogleTest)
    gtest_discover_tests(dqCommonTest)

    # ImageDecodeTest 以仓库根相对路径读 tests/assets/*。直接从仓库根跑 exe 可解析；
    # gtest_discover_tests 给每个测试设 WORKING_DIRECTORY=<binary_dir>（= build/dqCommon），
    # 故把资产镜像到 build/dqCommon/dqCommon/tests/assets 使同一相对路径在两种跑法下均可解析。
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests/assets
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/dqCommon/tests)
endif()

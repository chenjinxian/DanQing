# dqApp —— 渲染宿主层（丹青图形引擎的帧管线/工具/视口编排）
#
# 公开头：PublicAPI/dqApp/（唯一可被其他模块 #include 的目录）
# 实现：  src/
# 测试：  tests/
#
# 架构：itwinjs IModelApp 模式（Application singleton + ViewManager + Viewport）
#       渲染走原生 GL 句柄（非 QOpenGLWidget），Qt 仅用于宿主窗口桥接

# --- 版本 ---
set(dqApp_VERSION_MAJOR 0)
set(dqApp_VERSION_MINOR 1)
set(dqApp_VERSION_PATCH 0)

# --- Qt moc/autouic/autorcc ---
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

# ---------------------------------------------------------------------------
# dqApp 静态库（与 4 引擎模块统一；Windows DLL 不导出无标注内部符号，
# mac 的 visibility("default") 全导出掩盖了这一点，详见 dqRender 同款修改）
# ---------------------------------------------------------------------------
add_library(dqApp STATIC)
target_compile_definitions(dqApp PUBLIC DQ_APP_STATIC=1)

target_sources(dqApp
    PRIVATE
        src/Application.cpp
        src/AccuSnap.cpp
        src/Sprites.cpp
        src/ViewManager.cpp
        src/Viewport.cpp
        src/ViewState.cpp
        src/ViewPose.cpp
        src/ViewPicker.cpp
        src/StandardView.cpp
        src/IModelConnection.cpp
        src/BlankConnection.cpp
        src/EcefLocation.cpp
        src/DisplayStyle.cpp
        src/ViewingSpace.cpp
        src/BackgroundMapGeometry.cpp
        src/AcsTriadDecorator.cpp
        src/SelectionSet.cpp
        src/ToolAdmin.cpp
        src/ViewTool.cpp
        src/SelectionTool.cpp
        src/IdleTool.cpp
        src/GltfDecoration.cpp
        src/GltfImport.cpp
        src/DecorateContext.cpp
        src/DecorationsCache.cpp
        src/Animator.cpp
        src/ViewportSync.cpp
        # Application-layer Qt-based ITileFetcher (injected into dqRender TileAdmin;
        # dqRender itself is Qt-free per §8.2). FileTileFetcher wraps it and adds
        # local-filesystem tile content for offline tilesets.
        src/QtTileRequestFetcher.cpp
        src/FileTileFetcher.cpp
        src/tile/Tiles.cpp
        src/tile/TileTreeReference.cpp
        src/tile/DisclosedTileTreeSet.cpp
        src/tile/PrimaryTileTreeSupplier.cpp
        src/tile/SpatialTileTreeReferences.cpp
        # Headers with Q_OBJECT (needed for AUTOMOC)
        PublicAPI/dqApp/Viewport.h
        src/QtTileRequestFetcher.h
        src/FileTileFetcher.h
)

target_include_directories(dqApp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/PublicAPI>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(dqApp
    PUBLIC
        dqBase
        dqRender
        dqGeom
        dqCommon
        danqing_compile_options
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
)

# DLL 导出宏
target_compile_definitions(dqApp PRIVATE DQ_APP_BUILDING=1)

# Bitmap cursor assets (converted from itwinjs-core public/cursors/*.cur —
# PNG + hotspot in third_party/itwinjs-cursors/). Consumed by
# Viewport::setCursor's bitmap tier (ViewManager.ts:585-592 cursor family).
target_compile_definitions(dqApp PRIVATE
    DANQING_CURSOR_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../third_party/itwinjs-cursors"
    DANQING_SPRITE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../third_party/itwinjs-sprites"
)

# Suppress OpenGL deprecation warnings on macOS
if(APPLE)
    set_source_files_properties(
        src/Viewport.cpp
        PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations"
    )
endif()

# ---------------------------------------------------------------------------
# 单元测试
# ---------------------------------------------------------------------------
if(DANQING_BUILD_TESTS AND TARGET GTest::gtest_main)
    # DANQING_TESTING surfaces test-only seams (e.g. Viewport::SetPickResultForTest)
    # so headless GoogleTest runs can stub the GL pick path. PUBLIC so both the
    # dqApp library sources (Viewport.cpp short-circuit) and every downstream
    # test TU that includes Viewport.h see the macro. In non-test builds the
    # symbols vanish entirely — §8.4 (implementation details not in PublicAPI).
    target_compile_definitions(dqApp PUBLIC DANQING_TESTING=1)

    add_executable(dqAppTest
        tests/BlankConnectionTest.cpp
        tests/ViewPoseTest.cpp
        tests/ViewUndoTest.cpp
        tests/EntityStateTest.cpp
        tests/ToolAdminTest.cpp
        tests/FeatureOverrideTest.cpp
        tests/SelectionSetTest.cpp
        tests/DecoratorTest.cpp
        tests/ViewManagerTest.cpp
        tests/StandardViewTest.cpp
        tests/GridSettingsTest.cpp
        tests/LookAtTest.cpp
        tests/WheelZoomCoalesceTest.cpp
        tests/PerfDegradationTest.cpp  # 缩放后 resize 累积劣化插桩（TEMP-DIAG）
        tests/ResizePixelConsistencyTest.cpp  # resize 渲染资源重建的像素级回归
        tests/NestedViewportDiagTest.cpp  # TEMP-DIAG 嵌套 viewport Grid 对照
        tests/ZoomBlackBoxTest.cpp  # TEMP-DIAG 放大黑色方块复现
        tests/IModelAppSubsystemsTest.cpp
        tests/EventDispatchTest.cpp
        tests/FrustumApiTest.cpp
        tests/ViewToolTest.cpp
        tests/ToolSettingsDiagTest.cpp  # ToolSettings/animation 契约（Debug Info 全量 2026-09-21）
        tests/GltfImportTest.cpp
        tests/ViewportShutdownTest.cpp
        tests/CoordinateConversionTest.cpp
        tests/ViewingSpaceTest.cpp
        tests/BackgroundMapGeometryTest.cpp
        tests/ViewListTest.cpp
        tests/WindowAreaTest.cpp
        tests/GridAppDiag.cpp
        tests/SkyRotateDiag.cpp
        tests/StandardViewAnimationTest.cpp
        tests/LookToolTest.cpp
        tests/TileTreeRegistryTest.cpp  # 忠实路径 F3：Tiles 注册表（Ported from Tiles.test.ts）
    )
    target_link_libraries(dqAppTest
        PRIVATE
            dqApp
            GTest::gtest_main
            danqing_compile_options
     dbghelp)
    # Texture tests read the shared PNG asset — compile-time absolute path
    # (no working-directory dependency). dqApp/.. = repo root.
    target_compile_definitions(dqAppTest PRIVATE
        DANQING_TEST_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    include(GoogleTest)
    gtest_discover_tests(dqAppTest)
endif()

# Windows：部署 Qt DLL 到测试输出目录。缺 DLL 时 Loader 在无 UI 会话下弹窗
# 挂起（测试进程 CPU=0 假死），依赖 PATH 的方案对 ctest/CI 不可靠。
if(WIN32 AND TARGET Qt6::Core)
    get_filename_component(QT_BIN_DIR "${Qt6Core_DIR}/../../../bin" ABSOLUTE)
    add_custom_command(TARGET dqAppTest POST_BUILD
        COMMAND "${QT_BIN_DIR}/windeployqt.exe" --no-translations $<TARGET_FILE:dqAppTest>
        COMMENT "Deploying Qt runtime next to dqAppTest"
        VERBATIM)
endif()

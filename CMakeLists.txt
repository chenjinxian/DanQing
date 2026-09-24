# 丹青 DanQing —— 纯客户端图形引擎（itwinjs-core 大体量渲染 × Filament 实时渲染/全平台）
cmake_minimum_required(VERSION 3.20)

project(DanQing
    VERSION 0.1.0
    DESCRIPTION "DanQing — pure client-side graphics engine (itwinjs-core large-scale rendering x Filament realtime rendering)"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# C++ 标准（C++20，对齐 imodel-native，全局强制，禁扩展）
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# 默认 Release（对齐 FreeCAD-LibPack 仅支持 Release 的约定）
# ---------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# ---------------------------------------------------------------------------
# 全局编译选项（INTERFACE 库，供所有 DanQing 目标链接）
# ---------------------------------------------------------------------------
add_library(danqing_compile_options INTERFACE)
target_compile_features(danqing_compile_options INTERFACE cxx_std_20)

if(MSVC)
    # /W4 高警告 /WX 警告即错误 /permissive- 严格标准 /utf-8 源码字符集
    # /EHsc 保持 C++ 异常 ABI 与 Qt 预编译库兼容（我们自己的代码用 Result<T,E>，不抛）
    target_compile_options(danqing_compile_options INTERFACE
        /W4 /WX /permissive- /EHsc /Zc:__cplusplus /Zc:preprocessor /utf-8)
    # Windows 头卫生（业界标准，Windows-first 参考项目同做法）：
    #   WIN32_LEAN_AND_MEAN — windows.h 不拉 winsock.h/inaddr.h（其 s_host 等宏污染标识符）
    #   NOMINMAX            — windows.h 不定义 min/max 宏（破坏 std::min/std::numeric_limits::max）
    #   _CRT_SECURE_NO_WARNINGS — 允许 sscanf 等 CRT 函数（对齐 imodel-native Windows 构建）
    #   _CRT_NONSTDC_NO_WARNINGS — 允许 mkdir/rmdir/unlink 等 POSIX 名（CRT 兼容层）
    #   _USE_MATH_DEFINES       — MSVC <cmath> 默认不定义 POSIX 扩展 M_PI/M_PI_2
    target_compile_definitions(danqing_compile_options INTERFACE
        WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS _USE_MATH_DEFINES)
else()
    target_compile_options(danqing_compile_options INTERFACE
        -Wall -Wextra -Wpedantic -Werror)
    # 自有代码禁异常、禁 RTTI（Qt 预编译头可在此标志下编译）
    target_compile_options(danqing_compile_options INTERFACE -fno-exceptions -fno-rtti)
endif()

# 位置无关代码（便于构建共享库）
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 导出 compile_commands.json（clangd / clang-tidy / VSCode 用）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Qt 6.11.1（FreeCAD-LibPack v3.5.2 基线）
#   配置时 -DCMAKE_PREFIX_PATH 指向 third_party/qt/<platform>/6.11.1
#   引擎 SDK（dqBase/dqGeom/dqCommon/dqRender）零 Qt 依赖（§8.2）——Qt 缺失时
#   仅跳过 dqApp/samples，引擎与测试照常构建。
#   Windows 依赖引导：scripts/bootstrap_third_party.ps1（aqtinstall 下载 Qt）
# ---------------------------------------------------------------------------
find_package(Qt6 6.11.1 COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Network QUIET)
if(Qt6_FOUND)
    message(STATUS "Qt ${Qt6Core_VERSION} found — full build (engine + dqApp + samples)")
else()
    message(STATUS "Qt6 6.11.1 not found — engine SDK only (dqApp/samples skipped)")
    message(STATUS "  to enable: scripts/bootstrap_third_party.ps1 + -DCMAKE_PREFIX_PATH=<qt-dir>")
endif()

# ---------------------------------------------------------------------------
# 单元测试（GoogleTest submodule）
# ---------------------------------------------------------------------------
option(DANQING_BUILD_TESTS "Build unit tests" ON)
enable_testing()

if(DANQING_BUILD_TESTS)
    set(GOOGLETEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest")
    if(EXISTS "${GOOGLETEST_DIR}/CMakeLists.txt")
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        # 排除 gtest 自身的 danqing_compile_options（第三方代码不套我们的 -Werror）
        add_subdirectory("${GOOGLETEST_DIR}" googletest EXCLUDE_FROM_ALL)
    else()
        message(WARNING
            "GoogleTest submodule not found at ${GOOGLETEST_DIR}\n"
            "Run: ./scripts/bootstrap_third_party.sh   (or .ps1 on Windows)")
    endif()
endif()

# ---------------------------------------------------------------------------
# SDK 模块
# ---------------------------------------------------------------------------
add_subdirectory(dqBase)
add_subdirectory(dqGeom)
add_subdirectory(dqCommon)
add_subdirectory(dqRender)
if(Qt6_FOUND)
    add_subdirectory(dqApp)

    # -----------------------------------------------------------------------
    # 示例程序（独立于 SDK，只使用 PublicAPI）
    # -----------------------------------------------------------------------
    add_subdirectory(samples)
endif()

message(STATUS "")
message(STATUS "=== DanQing configuration ===")
message(STATUS "  build type     : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard   : ${CMAKE_CXX_STANDARD}")
if(Qt6_FOUND)
    message(STATUS "  Qt version     : ${Qt6Core_VERSION}")
else()
    message(STATUS "  Qt version     : (not found — dqApp/samples skipped)")
endif()
message(STATUS "  build tests    : ${DANQING_BUILD_TESTS}")
message(STATUS "")

# dqBase —— 共享基础层（DanQing 五模块的地基）
#
# 公开头：PublicAPI/dqBase/（唯一可被其他模块 #include 的目录）
# 实现：  src/
# 测试：  tests/
#
# 零 Qt 依赖 — 引擎 SDK 独立性要求。

set(dqBase_VERSION_MAJOR 0)
set(dqBase_VERSION_MINOR 1)
set(dqBase_VERSION_PATCH 0)

add_library(dqBase STATIC)

# 静态库：导出宏为空（Windows 下避免 C4273/C4251/C4275，见 Export.h）
target_compile_definitions(dqBase PUBLIC DQ_BASE_STATIC=1)

target_sources(dqBase
    PRIVATE
        src/DqBase.cpp
        src/DqClock.cpp
        src/DqGuid.cpp
        src/DqVersion.cpp
        src/Logging.cpp
        src/BeAssert.cpp
        src/BeThread.cpp
        src/Logger.cpp
        src/BeFileName.cpp
        src/BeFile.cpp
        src/DateTime.cpp
        src/BeStringUtilities.cpp
        src/Base64Utilities.cpp
        src/SHA1.cpp
        src/MD5.cpp
        src/JsonUtils.cpp
        src/TypedArrayBuilder.cpp
        src/UnexpectedErrors.cpp
        src/CompressedId64Set.cpp
        src/DqAllocator.cpp
        src/BeTextFile.cpp
        src/BeDebugUtilities.cpp
        src/BeSystemInfo.cpp
        src/BeDirectoryIterator.cpp
        src/BeFileListIterator.cpp
        src/BeGetProcAddress.cpp
        src/LocalState.cpp
        src/ValueFormat.cpp
        src/BeThreadLocalStorage.cpp
        src/GlobalHandleContainer.cpp
        src/BeTest.cpp
        src/BeDebugLog.cpp
        src/BeTimeUtilities.cpp
        src/Desktop/FileSystem.cpp
)

target_include_directories(dqBase
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/PublicAPI>
)

target_link_libraries(dqBase
    PUBLIC
        danqing_compile_options
)

if(APPLE)
    target_link_libraries(dqBase PRIVATE "-framework CoreFoundation")
endif()

if(DANQING_BUILD_TESTS AND TARGET GTest::gtest_main)
    add_executable(dqBaseTest
        tests/RefCountedTest.cpp
        tests/ResultTest.cpp
        tests/DqIdTest.cpp
        tests/DqGuidTest.cpp
        tests/DqEventTest.cpp
        tests/BlockedVectorTest.cpp
        tests/UtilityTest.cpp
        tests/SortedArrayTest.cpp
        tests/OrderedSetTest.cpp
        tests/Phase15Test.cpp
        tests/LoggerTest.cpp
        tests/LoggingTest.cpp
        tests/DqTypesTest.cpp
        tests/HeapZoneTest.cpp
        tests/BeConsoleTest.cpp
        tests/DateTimeTest.cpp
        tests/BeFileTest.cpp
        tests/DqSyncTest.cpp
        tests/MD5Test.cpp
        tests/SHA1Test.cpp
        tests/BeNumericalTest.cpp
        tests/CompressedId64SetTest.cpp
        tests/ClassUtilsTest.cpp
        tests/LocalStateTest.cpp
        tests/ObservableSetTest.cpp
        tests/OrderedId64IterableTest.cpp
        tests/NullableTest.cpp
        tests/Base64UtilitiesTest.cpp
        tests/UnexpectedErrorsTest.cpp
        tests/TupleKeyedMapTest.cpp
        tests/TypedArrayBuilderTest.cpp
        tests/ScopedArrayTest.cpp
        tests/PTypesUTest.cpp
        tests/BeFileNameTest.cpp
        tests/BeStringUtilitiesTest.cpp
        tests/JsonUtilsTest.cpp
        tests/BeThreadTest.cpp
        tests/BeThreadLocalStorageTest.cpp
        tests/BeTextFileTest.cpp
        tests/B1p1RemainingTest.cpp
        tests/DisposableTest.cpp
        tests/BatchHApiSurfaceTest.cpp
        tests/BeAssertTest.cpp
        tests/BeTestTest.cpp
        tests/BeTimeUtilitiesTest.cpp
        tests/SuppressWarningsTest.cpp
    )
    target_link_libraries(dqBaseTest
        PRIVATE
            dqBase
            GTest::gtest_main
            danqing_compile_options
    )
    include(GoogleTest)
    gtest_discover_tests(dqBaseTest)
endif()

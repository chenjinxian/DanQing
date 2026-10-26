# dqGeom —— 几何建模引擎（DanQing 第一层引擎，dqBase 之上；被 dqRender 向下依赖）
#
# 公开头：PublicAPI/dqGeom/（唯一可被其他模块 #include 的目录）
# 实现：  src/
# 测试：  tests/（从 itwinjs core-geometry / GeomLibs 移植）
#
# 零 Qt 依赖 — 引擎 SDK 独立性要求。

set(dqGeom_VERSION_MAJOR 0)
set(dqGeom_VERSION_MINOR 2)
set(dqGeom_VERSION_PATCH 0)

# ---------------------------------------------------------------------------
# dqGeom 共享/静态库
# ---------------------------------------------------------------------------
add_library(dqGeom STATIC)

# 静态库：导出宏为空（Windows 下避免 C4273/C4251/C4275，见 Export.h）
target_compile_definitions(dqGeom PUBLIC DQ_GEOM_STATIC=1)

target_sources(dqGeom
    PRIVATE
        src/DqGeom.cpp
        src/Ellipsoid.cpp
        src/Matrix3d.cpp
        src/ClipPlane.cpp
        src/ClipPrimitive.cpp
        src/ClipVector.cpp
        src/ClipUtilsLoops.cpp
        src/curve/GeometryQuery.cpp
        src/curve/CurvePrimitive.cpp
        src/curve/LineSegment3d.cpp
        src/curve/LineString3d.cpp
        src/curve/Arc3d.cpp
        src/curve/CurveCollection.cpp
        src/curve/Path.cpp
        src/curve/Loop.cpp
        src/solid/SolidPrimitive.cpp
        src/solid/Box.cpp
        src/solid/Cone.cpp
        src/solid/Sphere.cpp
        src/solid/TorusPipe.cpp
        src/solid/LinearSweep.cpp
        src/solid/RotationalSweep.cpp
        src/solid/RuledSweep.cpp
        src/polyface/IndexedPolyface.cpp
        src/polyface/AuxData.cpp
        src/polyface/Polyface.cpp
        src/polyface/PolyfaceVisitor.cpp
        src/polyface/BuildAverageNormalsContext.cpp
        src/polyface/PolyfaceQuery.cpp
        src/polyface/PolyfaceBuilder.cpp
        src/polyface/PolyfaceData.cpp
        src/serialization/IModelJson.cpp
        # A-lane（M-W W-b2）：BSplineCurve3d 子集（AspectRatioSkew 消费面）
        src/bspline/KnotVector.cpp
        src/curve/BSplineCurve.cpp
        src/curve/BezierCurve3d.cpp
)

# 公开头以 PUBLIC 暴露（其他模块 #include <dqGeom/Point3d.h>）
target_include_directories(dqGeom
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/PublicAPI>
)

# 零 Qt 依赖 — 仅用 C++ 标准库 + dqBase
target_link_libraries(dqGeom
    PUBLIC
        dqBase
        danqing_compile_options
)

# ---------------------------------------------------------------------------
# 单元测试
# ---------------------------------------------------------------------------
if(DANQING_BUILD_TESTS AND TARGET GTest::gtest_main)
    add_executable(dqGeomTest
        tests/Point3dTest.cpp
        tests/Point2dTest.cpp
        tests/Vector3dTest.cpp
        tests/PathLoopTest.cpp
        tests/SolidPrimitiveTest.cpp
        tests/RangeTest.cpp
        tests/MatrixTransformTest.cpp
        tests/AngleTest.cpp
        tests/AngleSweepTest.cpp
        tests/CurvePrimitiveTest.cpp
        tests/PolyfaceTest.cpp
        tests/StrokeOptionsTest.cpp
        tests/PolyfaceBuilderTest.cpp
        tests/PlaneTest.cpp
        tests/Ray3dTest.cpp
        tests/ClipPlaneTest.cpp
        tests/PolygonOpsClipTest.cpp
        tests/Matrix3dRigidTest.cpp
        tests/Map4dTest.cpp
        tests/EllipsoidTest.cpp
        tests/Matrix3dRowColumnTest.cpp
        tests/AngleSweepPeriodicFractionTest.cpp
        tests/ClipPlanesTest.cpp
        tests/ClipPrimitiveTest.cpp  # M-P P-A：ClipPrimitive/ClipShape（ClipPrimitives.test.ts 子集）
        tests/ClipVectorTest.cpp     # M-P P-A：ClipVector/StringifiedClipVector（ClipVector.test.ts 子集）
        tests/ClipUtilsLoopsTest.cpp # M-P P-A：loops/rangeOf/does 族（ClipUtilities.test.ts 子集）
        tests/YawPitchRollAnglesTest.cpp  # angles→rotation 链（M-H Task 3：ViewState3d 构造的 angles 应用面）
        # A-lane 锚（M-W 起——A 道代理只在此锚下追加测试注册；勿动他区）
        tests/PolyfaceAuxDataTest.cpp  # M-W W-a1/W-a2：AuxData.ts + IModelJson 子集（PolyfaceAuxData.test.ts 子集）
        tests/BSplineCurveTest.cpp     # M-W W-b2：BSplineCurve3d/KnotVector 子集（BsplineCurve.test.ts 消费面用例）
    )
    target_link_libraries(dqGeomTest
        PRIVATE
            dqGeom
            GTest::gtest_main
            danqing_compile_options
    )
    # A-lane（M-W W-a2）：PolyfaceAuxDataTest 资产路径
    # （rhombicosidodecahedron.imjs 测试数据 + samples 侧 cantilever.imjs 资产）
    target_compile_definitions(dqGeomTest PRIVATE
        DANQING_GEOM_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
        DANQING_DTA_ASSETS_DIR="${CMAKE_SOURCE_DIR}/samples/DisplayTestApp/assets"
    )
    include(GoogleTest)
    gtest_discover_tests(dqGeomTest)
endif()

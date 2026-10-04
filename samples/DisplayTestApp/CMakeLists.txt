# DisplayTestApp — FreeCAD UI Shell
# 1:1 from FreeCAD with minimal stubs

cmake_minimum_required(VERSION 3.16)
project(DisplayTestApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# WIN32 subsystem on Windows (FreeCAD: src/Gui/CMakeLists.txt builds FreeCADGui apps
# as GUI executables). A console-subsystem GUI app inherits the launching terminal's
# console/stdout pipe; while it lives it holds that pipe open, and its abnormal exit
# (or a GPU-driven reset under WGL load) can tear down the host terminal with it.
# GUI subsystem never attaches to a console. Qt6::EntryPoint supplies WinMain→main.
add_executable(DisplayTestApp WIN32
    main.cpp
    resources.qrc
    # === src/Gui/ — 1:1 port of FreeCAD src/Gui/ ===
    src/Gui/MainWindow.cpp
    src/Gui/Application.cpp       # Gui::Application facade (sendMsgToActiveView + newDocument)
    src/Gui/MDIView.cpp
    src/Gui/View.cpp
    src/Gui/Window.cpp
    src/Gui/DockWindowManager.cpp
    src/Gui/StatusBarLabel.cpp
    src/Gui/InputHintWidget.cpp
    src/Gui/View3DInventor.cpp      # DanQing: 3D viewport (replaces Coin3D with dqRender)
    src/Gui/DecorationGeometryExample.cpp   # Surface.ts:155-165 装饰几何示例（2026-09-17）
    src/Gui/DevToolsWidgets.cpp     # frontend-devtools 7 诊断 widget（DiagnosticsPanel）
    src/DumpOpenHelper.cpp          # 已存 iModel 打开链装配（M-H Task 3——Start 入口与同构锁共用）
    src/Gui/DevToolsWidgets.h       # MOC needs this for Q_OBJECT
    src/Gui/DebugWindow.cpp            # Viewer.ts:238-242 Debug info 面板（2026-09-17；2026-09-21 全量 7 widget）
    src/Gui/DtaToolBars.cpp         # DTA 功能分类工具栏区（Views/Selection/View Settings/View Tools/Analysis；Surface 入口在 Start 页）
    src/Gui/DtaTools.cpp            # M-L(3)：工具注册（App.ts SVTTools）+ 状态栏装配（keyin/FPS/瓦装载指示）+ ToolAssistance 接线
    src/Gui/DtaTools.h              # MOC needs this (no Q_OBJECT; include-path parity)
    src/Gui/KeyinField.cpp          # frontend-devtools KeyinField（M-L(3) 接线级 #1）
    src/Gui/KeyinField.h            # MOC needs this for Q_OBJECT
    src/Gui/FpsMonitor.cpp          # FpsMonitor + RecordFpsTool（M-L(3) 诊断三小件）
    src/Gui/FpsMonitor.h            # MOC needs this for Q_OBJECT
    src/Gui/TileLoadIndicator.cpp   # 瓦装载进度指示器（M-L(3) 诊断三小件）
    src/Gui/TileLoadIndicator.h     # MOC needs this for Q_OBJECT
    src/Gui/SaveImageTool.cpp       # SaveImage（M-L(3) 诊断三小件——readImageBuffer 生产包装）
    src/Gui/SaveImageTool.h
    src/Gui/SnapModeTool.cpp        # SetActiveSnapMode（M-M(6)——AccuSnap 活跃模式 keyin 接线）
    src/Gui/GridSettingsTool.cpp    # ChangeGridSettings（M-O(1) I5——grid 设置 keyin 接线）
    src/Gui/GltfDecorationTool.cpp  # AddGltfDecoration（M-O(2) 3f——glTF keyin + 实例化参数面）
    src/Gui/GltfDecorationTool.h
    src/Gui/OutputShadersTool.cpp   # OutputShaders（M-O(2) 3d——shader 落盘 keyin）
    src/Gui/OutputShadersTool.h
    src/Gui/FeatureOverridesPanel.cpp  # Overrides 面板（M-O(2) I10——选择集 appearance 覆盖）
    src/Gui/FeatureOverridesPanel.h    # MOC needs this for Q_OBJECT
    src/Gui/NamedViews.cpp            # NamedVSPS 持久化载体（M-O(2) I9 Saved Views）
    src/Gui/SavedViewsPanel.cpp       # SavedViewPicker 弹出面板（M-O(2) I9）
    src/Gui/SavedViewsPanel.h         # MOC needs this for Q_OBJECT
    src/Gui/SnapModeTool.h
    src/Gui/CategoriesPanel.cpp     # Categories 面板（M-N(1)——category 可见性引擎通道的 UI）
    src/Gui/CategoriesPanel.h
    src/Gui/ZoomToSelectedTool.cpp  # ZoomToSelectedElements（M-N(2)——placement 回放取景）
    src/Gui/ZoomToSelectedTool.h
    src/Gui/SyncViewportsTool.cpp   # SyncViewports/SyncFrusta（M-L(3)——ViewportSync 接线清偿）
    src/Gui/SyncViewportsTool.h
    src/Gui/ViewSettingsPanel.cpp   # DTA View Settings 弹出面板（Task 4）
    src/Gui/OverlayWidgets.h    # MOC needs this for Q_OBJECT
    src/Gui/OverlayManager.h    # MOC needs this for Q_OBJECT
    src/Gui/Command.cpp         # Command + CommandManager
    src/Gui/CommandDoc.cpp      # File domain: New/Import/Quit (M-L(2) 清理后余量)
    src/Gui/CommandView.cpp     # View domain: cameras/standard views/panels/status bar
    src/Gui/CommandStd.cpp      # Std domain: AboutQt（原 Help 域唯一真功能项）
    src/Gui/CommandWindow.cpp   # Window domain: Tile/Cascade/Activate/WindowsMenu (region 4 task 1)
    # M-L(2)：CommandStructure/CommandTools/CommandMacro 三域全删（真功能 0——
    # 分析报告 §3.5 按域量化表）。
    src/Gui/CreateStdCommands.cpp # Master registry: createStdCommands() ties all domains together
    src/Gui/Action.cpp          # Action + ActionGroup + subclasses (Q_OBJECT)
    src/Gui/Action.h            # MOC needs this for Q_OBJECT
    src/Gui/TileTreePanel.cpp   # Models/瓦树停靠面板（M-L(2) 裁决档——原 ComboView 改造）
    src/Gui/TileTreePanel.h     # MOC needs this for Q_OBJECT
    src/Gui/MenuManager.cpp     # MenuItem tree → QMenuBar recursive builder
    src/Gui/MenuManager.h       # MenuItem + MenuManager (no Q_OBJECT)
    src/Gui/ToolBarManager.cpp  # ToolBarItem tree → QToolBar builder
    src/Gui/Workbench.cpp       # Workbench base + StdWorkbench menu trees

    # === src/Mod/Start/Gui/ — 1:1 port of FreeCAD src/Mod/Start/Gui/ ===
    # M-H(4)（2026-09-28 用户指令"去 FreeCAD 无关功能"）：FileCardView/
    # FileCardDelegate/FirstStartWidget/GeneralSettingsWidget/ThemeSelectorWidget
    # + App/RecentFilesModel/App/DisplayedFilesModel 全清（裁决表见 commit
    # message）；保留 = StartView + NewFileButton（卡片控件）+ FlowLayout。
    src/Mod/Start/Gui/StartView.cpp
    src/Mod/Start/Gui/ReadMeView.cpp     # M-L(3)：ReadMe 展示页（Start 第三分组卡片 → 滚动只读页）
    src/Mod/Start/Gui/ReadMeView.h       # MOC needs this for Q_OBJECT
    src/Mod/Start/Gui/FlowLayout.cpp
    src/Mod/Start/Gui/NewFileButton.cpp
)

target_include_directories(DisplayTestApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Mod/Start/Gui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Mod/Start/App
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Mod/Start
    ${CMAKE_SOURCE_DIR}/dqApp/PublicAPI
    ${CMAKE_SOURCE_DIR}/dqRender/PublicAPI
    ${CMAKE_SOURCE_DIR}/dqCommon/PublicAPI
    ${CMAKE_SOURCE_DIR}/dqGeom/PublicAPI
    ${CMAKE_SOURCE_DIR}/dqBase/PublicAPI
)

# Qt6 — try system first, then fall back to bundled path
find_package(Qt6 COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets PrintSupport Svg QUIET)
if(NOT Qt6_FOUND)
    set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/qt/mac/6.11.1/macos")
    find_package(Qt6 COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets PrintSupport Svg REQUIRED)
endif()

# WIN32 subsystem needs the WinMain→main trampoline (Qt 6.5+ ships it as the
# EntryPointPrivate module; Qt6::EntryPoint no longer exists).
if(WIN32)
    find_package(Qt6 COMPONENTS EntryPointPrivate REQUIRED)
endif()

target_link_libraries(DisplayTestApp
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        Qt6::Svg
        $<$<PLATFORM_ID:Windows>:Qt6::EntryPointPrivate>  # WinMain→main for WIN32 subsystem
        $<$<PLATFORM_ID:Windows>:dbghelp>  # main.cpp DANQING_CRASH_STACK vectored handler
        dqApp
        dqRender
        dqCommon
        dqGeom
        dqBase
)

# Note: QT_NO_KEYWORDS is NOT defined here because dqApp headers use Qt keywords
# (signals, slots, emit). FreeCAD uses QT_NO_KEYWORDS but DanQing does not.

if(Qt6PrintSupport_FOUND)
    target_link_libraries(DisplayTestApp PRIVATE Qt6::PrintSupport)
else()
    target_compile_definitions(DisplayTestApp PRIVATE QT_NO_PRINTDIALOG)
endif()

# 示例资产（光标/snap/砖纹 PNG——DecorationGeometryExample 与 Snap sprite 用；
# 与 dqApp 私有宏同名同路径，DisplayTestApp 目标的 Gui 层文件单独需要）。
target_compile_definitions(DisplayTestApp PRIVATE
    DANQING_SPRITE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/itwinjs-sprites"
    DANQING_CURSOR_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/itwinjs-cursors"
    # M-H(4)：Start 页双模型入口的 dump 包根（与 DtaTest 同宏同路径——
    # main.cpp dumpPackageForModel 消费；运行时 DANQING_RPC_DUMP env 覆写）。
    DANQING_TILE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/tile-sample-assets"
)

if(APPLE)
    target_compile_options(DisplayTestApp PRIVATE
        -Wno-deprecated-declarations
        -Wno-unused-parameter
        -Wno-reorder-ctor
        -Wno-implicit-fallthrough
        -Wno-unused-variable
        -Wno-sign-compare
        -Wno-nonportable-include-path
    )
endif()

# --- Tests ---
# GoogleTest is available from root CMakeLists.txt (third_party/googletest)
if(TARGET gtest_main)
    add_executable(DisplayTestAppTest
        tests/StartupTest.cpp
        tests/CommandTest.cpp
        tests/CommandViewTest.cpp
        tests/CreateStdCommandsTest.cpp
        tests/MenuManagerTest.cpp
        tests/ToolBarManagerTest.cpp
        tests/WorkbenchTest.cpp
        tests/ActionSubclassTest.cpp
        tests/IntegrationTest.cpp
        tests/StatusBarTest.cpp
        tests/InputHintWidgetTest.cpp
        tests/DockTest.cpp
        tests/MDIChromeTest.cpp
        tests/StartTabPresentationTest.cpp  # 最大化首 show 呈现回归（2026-09-20 Start tab saga）
        tests/GuiApplicationTest.cpp
        tests/CommandWiringTest.cpp
        tests/IconRenderTest.cpp   # QtSvg path-truncation conformance probe (region-1+2 leftover)
        resources.qrc
        src/Gui/Command.cpp
        src/Gui/CommandDoc.cpp
        src/Gui/CommandView.cpp
        src/Gui/CommandStd.cpp
        src/Gui/CommandWindow.cpp
        src/Gui/CreateStdCommands.cpp
        src/Gui/Action.cpp
        src/Gui/Action.h        # MOC needs this for Q_OBJECT
        src/Gui/MenuManager.cpp
        src/Gui/ToolBarManager.cpp
        src/Gui/Workbench.cpp
        src/Gui/MainWindow.h    # MOC needs this for Q_OBJECT
        src/Gui/MainWindow.cpp
        src/Gui/Application.cpp
        src/Gui/StatusBarLabel.cpp
        src/Gui/InputHintWidget.cpp
        src/Gui/DockWindowManager.cpp
        src/Gui/OverlayWidgets.h    # MOC needs this for Q_OBJECT
        src/Gui/OverlayManager.h    # MOC needs this for Q_OBJECT
        src/Gui/View.cpp
        src/Gui/MDIView.cpp
        src/Gui/Window.cpp
        # M-H(4)：Start 页 FreeCAD-only widget 全清——GeneralSettingsWidget/
        # ThemeSelectorWidget/FirstStartWidget 源文件与 MDIChromeTest 对应
        # 4 测试同步移除（被测对象删除，裁决表见 commit message）。
    )

    target_include_directories(DisplayTestAppTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/stubs
        ${CMAKE_CURRENT_SOURCE_DIR}/stubs_test
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Mod/Start/Gui
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    target_link_libraries(DisplayTestAppTest PRIVATE
        gtest_main
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Svg
    )

    enable_testing()
    add_test(NAME DisplayTestAppTest COMMAND DisplayTestAppTest)
endif()

# DTA 工具栏测试：链接真实 dqApp（不经 stubs_test），与 DisplayTestAppTest 隔离。
if(TARGET gtest_main AND TARGET dqApp)
    add_executable(DisplayTestAppDtaTest
        tests/DtaToolBarsTest.cpp
        tests/ViewSettingsPanelTest.cpp  # Task 4: ViewSettingsPanel 弹出面板
        tests/View3DResizeTest.cpp       # 视口 × 父窗口 resize 风暴（崩溃回归）
        tests/ZoomBlackBoxSeqTest.cpp    # TEMP-DIAG 完整序列截图（最大化+连续放大）
        tests/GltfStandardViewTest.cpp   # 标准视图 Front/Back 下 glTF 箱体居中性取证
        tests/GltfTexturePixelTest.cpp   # glTF 纹理上屏像素回归（Khronos BoxTextured，Task 10）
        tests/GltfNormalMapPixelTest.cpp # glTF 法线贴图上屏像素回归（不对称法线图，2026-09-17）
        tests/DecoGeometryPixelTest.cpp   # Decoration Geometry Example 像素回归（2026-09-17）
        tests/DecoRotateCrashTest.cpp   # Deco 加载 + Rotate 崩溃回归（2026-09-17）
        tests/CursorPositionTest.cpp    # locate 光圈坐标 = 鼠标 client px（2026-09-17）
        tests/CursorStateTest.cpp       # 光标/光圈/snap 状态机回归（2026-09-18）
        tests/DebugWindowDiagTest.cpp   # Debug Info 面板 7 widget 驱动回归（2026-09-21）
        tests/GltfParityTest.cpp        # 同路径 glb × 双实现渲染对拍（官方 Khronos 语料）
        tests/PickHiliteSelectionTest.cpp  # 拾取/高亮/点选三链像素级回归（2026-09-16）
        tests/TileTreeRenderTest.cpp    # 离线 3D Tiles tileset 加载渲染像素回归（2026-09-19，RED 锁）
        tests/RpcDumpRenderTest.cpp     # RPC 真实数据端到端渲染锁（M-D(3)，2026-09-27——注册在 TileTreeRenderTest 之后：同进程直跑时其 fetcher 替换不回溢）
        tests/DumpOpenChainTest.cpp     # 已存 iModel 打开链流程同构锁（M-H Task 3——注册在 RpcDumpRenderTest 之后：同因 fetcher 替换不回溢）
        tests/DumpBrowseTest.cpp        # 双模型浏览零缺失锁（M-H Task 5——注册在 DumpOpenChainTest 之后：同因 fetcher 替换不回溢）
        tests/PickDumpSceneTest.cpp     # dump 场景拾取/点选/高亮贯通像素锁（M-I Task 2——注册在 DumpBrowseTest 之后：同因 fetcher 替换不回溢）
        tests/SavedViewsTest.cpp        # M-O(2) I9 Saved Views 全链锁（Create/Recall/Update/Delete + 持久化 round-trip——注册在 PickDumpSceneTest 之后：同因 fetcher 替换不回溢）
        tests/OutputShadersTest.cpp     # M-O(2) 3d OutputShaders 锁（收集门/注册表/compileAllShaders/落盘对账——注册在 SavedViewsTest 之后）
        tests/JoesHouseColorTest.cpp    # JoesHouse 管线蓝/红回归像素锁（M-I Task 3 色表顶点色——注册在 PickDumpSceneTest 之后：同因 fetcher 替换不回溢）
        tests/JoesHouseEdgeTest.cpp     # JoesHouse 面板黑边线回归像素锁（M-I Task 4 边线两段——注册在 JoesHouseColorTest 之后：同因 fetcher 替换不回溢）
        tests/JoesHouseHoverTest.cpp    # JoesHouse hover 高亮像素锁（M-J Task 2——注册在 JoesHouseEdgeTest 之后：同因 fetcher 替换不回溢）
        tests/DtaToolsWiringTest.cpp    # M-L(3) 接线级回归：Keyin 字段/诊断三小件/视口同步/ToolAssistance 提示表
        src/Gui/SnapModeTool.cpp        # M-M(6)：SetActiveSnapMode（snap 锁被测面）
        src/Gui/ZoomToSelectedTool.cpp  # M-N(2)：ZoomToSelectedElements（zoom 锁被测面）
        src/Gui/GridSettingsTool.cpp  # M-O(1) I5：ChangeGridSettings（grid 设置锁被测面）
        src/Gui/GltfDecorationTool.cpp  # M-O(2) 3f：AddGltfDecoration（gltf keyin 锁被测面）
        src/Gui/GltfDecorationTool.h  # MOC needs this (no Q_OBJECT; include-path parity)
        src/Gui/OutputShadersTool.cpp   # M-O(2) 3d：OutputShaders（落盘锁被测面）
        src/Gui/OutputShadersTool.h
        src/Gui/FeatureOverridesPanel.cpp  # M-O(2) I10：Overrides 面板（像素锁被测面）
        src/Gui/FeatureOverridesPanel.h  # MOC needs this for Q_OBJECT
        src/Gui/NamedViews.cpp            # M-O(2) I9：NamedVSPS 载体（SavedViews 链锁被测面）
        src/Gui/SavedViewsPanel.cpp       # M-O(2) I9：SavedViewPicker（SavedViews 链锁被测面）
        src/Gui/SavedViewsPanel.h       # MOC needs this for Q_OBJECT
        src/Gui/CategoriesPanel.cpp   # M-O(1) R2：面板开关锁（dock 装配被测面）
        tests/ModelsPanelWiringTest.cpp # M-L(3) Models 演示版机制锁：provider 逐模型可见性 → 场景 → 像素（注册在 JoesHouseHoverTest 之后：同因 fetcher 替换不回溢）
        src/Gui/View3DInventor.cpp       # 测试构造真实 View3DInventor（真视口）
        src/DumpOpenHelper.cpp           # 打开链装配（M-H Task 3——DumpOpenChain 锁的被测面）
        src/Gui/DecorationGeometryExample.cpp   # DtaToolBars 引用（Deco 按钮）
        src/Gui/DebugWindow.cpp                # DtaToolBars 引用（Debug 按钮）
        src/Gui/DevToolsWidgets.cpp            # DebugWindow 的 7 诊断 widget（DiagnosticsPanel）
        src/Gui/DevToolsWidgets.h              # MOC needs this for Q_OBJECT
        src/Gui/View3DInventor.h         # MOC needs this for Q_OBJECT
        src/Gui/DtaToolBars.cpp
        src/Gui/DtaTools.cpp             # M-L(3)：工具注册 + 状态栏装配 + ToolAssistance 提示表
        src/Gui/DtaTools.h               # MOC needs this (no Q_OBJECT)
        src/Gui/KeyinField.cpp           # M-L(3)：Keyin 字段（被测面）
        src/Gui/KeyinField.h             # MOC needs this for Q_OBJECT
        src/Gui/FpsMonitor.cpp           # M-L(3)：FPS 监视 + RecordFpsTool（被测面）
        src/Gui/FpsMonitor.h             # MOC needs this for Q_OBJECT
        src/Gui/TileLoadIndicator.cpp    # M-L(3)：瓦装载指示器（被测面）
        src/Gui/TileLoadIndicator.h      # MOC needs this for Q_OBJECT
        src/Gui/SaveImageTool.cpp        # M-L(3)：SaveImage（被测面）
        src/Gui/SaveImageTool.h
        src/Gui/SyncViewportsTool.cpp    # M-L(3)：视口同步工具（被测面）
        src/Gui/SyncViewportsTool.h
        src/Gui/TileTreePanel.cpp        # M-L(3)：Models 面板（provider 消费方——机制锁在 ModelsPanelWiringTest）
        src/Gui/TileTreePanel.h          # MOC needs this for Q_OBJECT
        src/Gui/ViewSettingsPanel.cpp    # Task 4: View Settings 弹出面板
        src/Gui/Application.cpp   # Gui::Application 门面（newDocument 工厂）
        # LNK2019 最小增量：Application.cpp 引用 MainWindow::addWindow/activeWindow +
        # CommandManager::testActive（均非 inline），故补入对应实现文件；其余为
        # MainWindow.cpp/Command.cpp 的链接闭包（按 LNK2019 逐轮补入）。
        src/Gui/MainWindow.cpp
        src/Gui/Command.cpp
        src/Gui/Window.cpp            # WindowParameter（MainWindow DimensionWidget）
        src/Gui/StatusBarLabel.cpp
        src/Gui/InputHintWidget.cpp
        src/Gui/MDIView.cpp
        src/Gui/DockWindowManager.cpp
        src/Gui/ToolBarManager.cpp
        src/Gui/Action.cpp
        src/Gui/Action.h              # MOC needs this for Q_OBJECT
        src/Gui/Workbench.cpp         # Workbench::activeWorkbench
        src/Gui/OverlayManager.h      # MOC needs this for Q_OBJECT
        src/Gui/OverlayWidgets.h      # MOC: OverlayToolButton/OverlayTitleBar（DockWindowManager）
        src/Gui/View.cpp              # BaseView（MDIView 基类）
        src/Gui/MenuManager.cpp       # MenuItem + MenuManager::setup（Workbench::activate）
        resources.qrc                 # DTA 图标资源（字体 + SVG——图标断言测试需要）
    )
    target_include_directories(DisplayTestAppDtaTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src       # DumpOpenHelper.h（M-H Task 3）
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui
        ${CMAKE_CURRENT_SOURCE_DIR}/stubs      # Gui::Application 需要 Base/App 桩头
        ${CMAKE_SOURCE_DIR}/third_party        # TileIOFixtures.h（cylinder 录制夹具——U11(3) silhouette 像素锁的派生资产源）
    )
    # Khronos glTF-Sample-Assets vendor 目录（GltfTexturePixelTest 加载 BoxTextured）。
    # samples/DisplayTestApp → 仓库根 = 两级（../../）；brief 原文 ../../../ 会落到
    # D:/Github/third_party（LoadFromFile "Failed to open file" → loadGltf false）。
    target_compile_definitions(DisplayTestAppDtaTest PRIVATE
        DANQING_GLTF_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gltf-sample-assets"
        # 与 app 目标同路径（Deco 示例的砖纹/光标/snap 资产——DecoRotateCrashTest
        # 与 DecoGeometryPixelTest 经 openDecorationGeometryExample 用到）。
        DANQING_SPRITE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/itwinjs-sprites"
        DANQING_CURSOR_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/itwinjs-cursors"
        # TileTreeRenderTest 的离线 3D Tiles 资产（tileset.json + root.b3dm，DanQing-authored）
        DANQING_TILE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/tile-sample-assets"
    )
    target_link_libraries(DisplayTestAppDtaTest PRIVATE
        gtest_main Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg dqApp dqRender dqCommon dqGeom dqBase danqing_compile_options
        dbghelp  # PickHiliteSelectionTest 的 SEH 崩溃符号化探针（DANQING_CRASH_STACK）
    )
    include(GoogleTest)
    gtest_discover_tests(DisplayTestAppDtaTest)
endif()

# Windows：部署 Qt DLL 到应用/测试输出目录（缺 DLL 在无 UI 会话下弹窗挂起，
# 见 dqApp/CMakeLists.txt 同款注释）
if(WIN32 AND TARGET Qt6::Core)
    get_filename_component(QT_BIN_DIR "${Qt6Core_DIR}/../../../bin" ABSOLUTE)
    add_custom_command(TARGET DisplayTestApp POST_BUILD
        COMMAND "${QT_BIN_DIR}/windeployqt.exe" --no-translations $<TARGET_FILE:DisplayTestApp>
        COMMENT "Deploying Qt runtime next to DisplayTestApp"
        VERBATIM)
    if(TARGET DisplayTestAppTest)
        add_custom_command(TARGET DisplayTestAppTest POST_BUILD
            COMMAND "${QT_BIN_DIR}/windeployqt.exe" --no-translations $<TARGET_FILE:DisplayTestAppTest>
            COMMENT "Deploying Qt runtime next to DisplayTestAppTest"
            VERBATIM)
    endif()
    if(TARGET DisplayTestAppDtaTest)
        add_custom_command(TARGET DisplayTestAppDtaTest POST_BUILD
            COMMAND "${QT_BIN_DIR}/windeployqt.exe" --no-translations $<TARGET_FILE:DisplayTestAppDtaTest>
            COMMENT "Deploying Qt runtime next to DisplayTestAppDtaTest"
            VERBATIM)
    endif()
endif()

# itwinjs-core → DanQing/WASM 可替换性分析

> 2026-09-24 战略分析（Authored：源码取证报告，非参考移植）。姊妹篇：
> `filament-PBR材质接入分析-2026-09-24.md`、`filament优秀实践移植分析-2026-09-24.md`。
> 所有 file:line 证据来自分析时真实读过的源码（itwinjs-core / DanQing 仓内）。

## 1. 结论摘要

1. itwinjs-core 的**渲染内部实现**（webgl 后端 128 文件/33.5k 行、imdl/glTF 解析解码、tile 选择剔除、DrawCommand 构建）是纯计算密集层，DanQing 的 dqBase+dqGeom+dqCommon+dqRender 与之 1:1 对应且公开 API 零 Qt，是 WASM 化的主要替换面。
2. **TS 保留层**清晰：Viewport 24 步管线（`Viewport.ts:2582-2703`）、ToolAdmin/工具（18k 行）、AccuDraw（3.9k 行）、IModelConnection RPC、fetch/Worker——这些绑 DOM/交互/RPC，不该进 WASM。
3. 替换缝就是 itwinjs 的 `RenderTarget` 抽象面（`RenderTarget.ts:41-168` 的 changeScene/changeDecorations/changeRenderPlan/drawFrame/readPixels 等约 15 个抽象成员），DanQing `RenderTarget.h` 已实现其中 80% 以上，缺口（changeDynamics/screenSpaceEffects/analysisFraction）是小型增补。
4. DanQing 引擎 SDK 本身**无 Qt 依赖**（根 `CMakeLists.txt:63-69` 可无 Qt 构建），障碍集中在：新增 Emscripten 平台后端（PlatformFactory 的 `#else` 分支当前返回 nullptr）、GLSL 410→300 es 双目标、tile 字节传输的桥接。
5. Bentley 自己已在渲染栈内嵌 WASM（meshopt 解码器 `MeshoptCompression.ts:23-49`、draco `@loaders.gl/draco`）——先例证明"解码阶段 WASM 化"正是 Bentley 的 WASM 边界画法和收益点。

## 2. 可替换清单（按收益/可行性排序）

### R1. imdl/glTF 解析与几何解码 —— 收益最高、可行性最高
- **itwinjs 位置**：`core/frontend/src/common/imdl/ParseImdlDocument.ts`（1327 行，Worker 中执行，`workers/ImdlParser/Worker.ts:31-48`）；`core/frontend/src/tile/GltfReader.ts`（2980 行）；`core/frontend/src/common/gltf/GltfParser.ts`（draco 路径 :325）；`core/frontend/src/internal/tile/MeshoptCompression.ts`（meshopt 路径）。
- **DanQing 对应**：`dqRender/src/tile/ImdlDocument.cpp` + `ImdlGraphics.cpp:55 decodeImdlGraphics()`（已端到端绿，TD-18/19）；`dqRender/src/gltf/GltfReader.cpp`（cgltf 批准偏差）；`dqCommon/src/Image.cpp:82 stbi_load_from_memory`。
- **替换方式**：WASM 模块导出 `decodeImdl(Uint8Array) -> 顶点/索引/FeatureTable 内存视图`；TS 侧保留 RPC 取字节（`TileAdmin.ts:684-687`），字节直接以 WASM 线性内存视图传入，零拷贝；meshopt/draco 继续由同一 WASM 模块内完成。
- **障碍**：低。① `GltfReader.cpp:251/678-717` 直接 `std::ifstream/_wfopen` 读盘——需把入口改为内存 buffer（itwinjs 侧本来就是 ArrayBuffer 喂入，语义天然对齐）；② FeatureTable 输出格式对齐 `PackedFeatureTable`（`dqCommon/PublicAPI/dqCommon/PackedFeatureTable.h` 已有）。
- **收益**：CPU 密集、buffer 巨大、逐顶点量化解码；Bentley 已为此专开 Worker + 内嵌 WASM，说明热区确认。WASM 版消除 worker 间 transferable 序列化成本，SIMD 路径（Emscripten `-msimd128`，对齐 meshopt 的 wasmSimd 变体 `MeshoptCompression.ts:27,37`）可再提速。渲染线程不再被解码阻塞或等待 postMessage。

### R2. WebGL 渲染后端整体（System/Target/SceneCompositor/Technique/DrawCommand）
- **itwinjs 位置**：`core/frontend/src/internal/render/webgl/`（128 文件、33,565 行；`System.ts:386` `canvas.getContext("webgl2")`、`:469 createTarget`）；`SceneCompositor.ts`（2349 行）；`RenderCommands.ts`/`DrawCommand.ts`/`BranchStack.ts`。
- **DanQing 对应**：`dqRender/src/render/`（RenderSystemImpl/TargetImpl/SceneCompositorImpl/TechniqueImpl/RenderCommands/DrawCommand 等 140+ 编译单元，45.5k 行 cpp）；`dqRender/src/rhi/opengl/`（filament 式 Driver 抽象）。
- **替换方式**：TS `RenderSystem`/`RenderTarget` 抽象由 WASM 实现承接——TS Viewport 继续调用 `target.changeScene/changeDecorations/changeRenderPlan/drawFrame`（`Viewport.ts:2569/2658/2677/2699`），WASM 侧实现同名语义。GL 调用经由 Emscripten 提供的 GLES3 头直编（Windows 下的 `dqgl*` 函数指针机制 `GlLoader.h` 在非 Windows 平台为空，Emscripten 分支直接链接 emscripten GL）。
- **障碍**：中。① **需新增 `PlatformEmscripten`**：`PlatformFactory.cpp` 现仅覆盖 Apple/Win/Linux，`#else` 返回 nullptr——须实现 `emscripten_webgl_create_context` 版平台+swapchain（画布 resize=rebind 语义已存在于 `Swapchain.h: rebind(void*)`）；② **GLSL 版本**：DanQing 默认 `#version 410 core`（`ShaderBuilder.h:468`），但版本串已参数化（`ShaderBuilder.h:384 setVersion`），加 `300 es` 目标即可（遗留 ES 语义差异：vertex shader precision、无隐式 int→float）；③ `GlLoader.cpp:26-29,125-126` 装载的 `glBlendFuncSeparatei/glTexStorage2D` 等——`i` 变体（draw-buffer 索引混合）**不在 WebGL2 内**，但当前 src 内无实际调用点（仅装载），合成 pass 回退非索引混合即可；`glTexStorage2D` WebGL2 原生支持。④ OIT 用 RGBA16F（`CompositorTextures.cpp:40-41`）→ WebGL2 需 `EXT_color_buffer_float`（itwinjs 同样依赖，普遍性无虞）。
- **收益**：高。每帧 TS 侧 BranchStack 遍历、RenderCommands 构建、FeatureOverrides LUT 计算全部下沉为原生代码；大数组（顶点 LUT、feature 表）零拷贝。注意 GPU-bound 的 draw 本身无收益，收益在**每帧 CPU 提交路径**。

### R3. Tile 选择 / LOD / 剔除（selectTiles 族）
- **itwinjs 位置**：`core/frontend/src/tile/TileTree.ts:91 _selectTiles / :136 selectTiles`；`Tile.ts`（677 行）；`TileDrawArgs.ts`；`internal/tile/RealityTileDrawArgs.ts`。
- **DanQing 对应**：`dqRender/src/tile/Tile.cpp/TileTree.cpp/TileDrawArgs.cpp`（已移植）。
- **替换方式**：WASM 侧持有 tile tree 状态，每帧 `selectTiles` 输入视锥/LOD 参数（纯数学），返回选中集。剔除数学零 I/O。
- **障碍**：低-中。Tile 树生命周期与 TS 侧 TileAdmin 调度（`TileAdmin.ts` 1412 行的调度/优先级/取消）要划清归属——建议状态留 TS、剔除计算入 WASM（每帧调用、数据量小）。
- **收益**：中-高。视锥/LOD 判定每帧每树执行，大体量场景树深、节点多；WASM 数学 + 无 GC 压力。

### R4. Feature 表运算 / FeatureSymbology Overrides / 打包
- **itwinjs 位置**：`core/common/src/FeatureTable.ts`、`core/frontend/src/internal/render/webgl/FeatureOverrides.ts`、`VertexLUT.ts`。
- **DanQing 对应**：`dqCommon` FeatureTable/PackedFeatureTable/FeatureSymbology（73 个公开头）；`dqRender/src/render/FeatureOverrides.cpp`、`FeatureOverrideLUT.cpp`、`VertexLutTexture.cpp`。
- **替换方式**：整模块编译；overrides 计算输入为纯数据（ElementId 列表+symbology）。
- **障碍**：低。Id64 表示需对齐（TS 字符串 ↔ C++ uint64）。
- **收益**：中。Overrides 每批每帧重算，字符串 Id 解析在 TS 侧是真实开销。

### R5. core/geometry 纯数学几何 → dqGeom（独立 SDK 维度）
- **itwinjs 位置**：`core/geometry/src`（370 文件、33MB 源码，含 test 目录；**零 DOM/fetch 命中**——纯 TS）。
- **DanQing 对应**：`dqGeom`（54 公开头：curve/polyface/solid/ClipPlane/Ellipsoid/Transform 等；约 16k 行含头）。
- **替换方式**：dqGeom 编 WASM 作为通用几何内核给 TS 调用（Polyface 构建、Bool/clip/stroke、Range 运算）。
- **障碍**：中。**覆盖缺口**：core-geometry 有 bspline/clipping/numerics/topology/geometry4d 子目录，dqGeom 的 BSpline 仅在 `CurvePrimitive.h/GeometryHandler.h` 中被提及、无独立模块——替换前需先按 §0 补齐移植。
- **收益**：中-高（凡走几何运算的下游——装饰器、GraphicBuilder 几何积累 `internal/render/PrimitiveBuilder.ts`、tile 几何收集 `TileGeometryCollector.ts`——全部受益）；但属"能力替换"而非 itwinjs 渲染栈必需。

### R6. core/common 共享类型与算法 → dqCommon
- **itwinjs 位置**：`core/common/src`（233 文件、2.9MB；grep 命中的"document"均为注释伪命中，实际零 DOM 依赖；RpcInterface/Localization/ImageProps 等少量类型仅作数据定义）。
- **DanQing 对应**：`dqCommon`（73 公开头：ColorDef/ViewFlags/Frustum/Gradient/RenderSchedule/ThematicDisplay/GeometryStream… 20k 行）。
- **替换方式**：值类型与算法（Gradient 插值、Frustum 平面提取、ViewFlags 位运算、PackedFeatureTable 打包 `PackedFeatureTable.ts:139-141` 布局）整包 WASM。
- **障碍**：低-中。common 内 Rpc/Authorization 等类型是数据壳，保留 TS 侧即可，dqCommon 只管纯计算子集。
- **收益**：中。单独看每处都小，但它是 R1-R4 的公共依赖，顺手整编。

### R7. 图元几何构建（GraphicBuilder/MeshBuilder/tessellation）
- **itwinjs 位置**：`core/frontend/src/render/GraphicBuilder.ts`、`internal/render/PrimitiveBuilder.ts`、`webgl/MeshBuilder.ts`（在 33.5k 行内）。
- **DanQing 对应**：`dqRender/src/render/GraphicBuilder.cpp/MeshBuilder.cpp/PolylineTesselator.cpp/GeometryAccumulator.cpp`。
- **替换方式**：装饰器/动态图元的 `createGraphic` 调用入 WASM，返回句柄。
- **障碍**：低；需把 TS 侧 GeometryStream 输入按值编组传入。
- **收益**：中。装饰密集场景（ACS/Grid/深度预览等）每帧重建。

## 3. 不可替换 / 不宜替换清单

| 项 | 证据 | 原因 |
|---|---|---|
| Viewport 24 步帧管线、ViewManager、动画 | `Viewport.ts:2582-2703`（3968 行） | 与 DOM 事件循环、requestAnimationFrame、变更事件分发深度耦合；它正是调用 WASM RenderTarget 的保留层 |
| ToolAdmin / 工具集 / AccuDraw | `src/tools/` 18,045 行；`AccuDraw.ts` 3861 行 | 纯交互状态机，无 CPU 热点；dqApp 的对应物依赖 Qt（`dqApp` 中 ~24 文件含 Qt 头），WASM 下无对应物也无需对应物 |
| IModelConnection / TileAdmin RPC 传输 | `TileAdmin.ts:684-687 IModelTileRpcInterface.getClient().generateTileContent`；`IModelTile.ts:90-91` | 取字节走 RPC/fetch（`FetchCloudStorage.ts:14,19`），是 WASM 的**上游数据源**而非被替换者 |
| Worker 编排 | `workers/ImdlParser/Worker.ts:31-48`、`common/WorkerProxy.ts` | 线程边界保留在 TS（Web Worker + Emscripten pthread 二选一均可，编排逻辑不动） |
| WebGL 上下文创建与能力检测 | `webgl/System.ts:386`；`core/webgl-compatibility/src/Capabilities.ts` | canvas/getContext 是浏览器 API；WASM 场景改为 `emscripten_webgl_create_context` 由平台层吸收 |
| 纹理上屏的 ImageBitmap 路径 | `webgl/Texture.ts:256/399/666` | `createImageBitmap`/Image element 是 DOM API；DanQing 的 stb_image 内存解码（`dqCommon/src/Image.cpp:82`）恰好绕开它，属"替换使障碍消失" |
| core/backend 全量 | `backend/package.json:111 @bentley/imodeljs-native 5.14.32`；`backend/src/internal/NativePlatform.ts:9` | Node N-API 原生插件 + SQLite/ECSQL，DanQing 无对应模块，且浏览器 WASM 场景天然不覆盖后端 |
| core/orbitgt | 104 文件（pointcloud/spatial/system） | OPC 点云格式栈，DanQing 未移植；如需替换属独立移植项目 |
| core/ecschema-*、quantity、i18n | — | 与图形引擎正交，无 dq 对应物 |

## 4. itwinjs 自身的 WASM/原生先例证据

1. **meshopt WASM 解码器内嵌渲染栈**：`core/frontend/src/internal/tile/MeshoptCompression.ts:20-49`——注释明言是 meshoptimizer js 解码器的改写版，`:26-27` base64 内嵌 wasm（plain + SIMD 两变体，":24 Built with clang version 16.0.0 / from meshoptimizer 0.20"），`:37 WebAssembly.validate` 探测 SIMD，`:42 WebAssembly.instantiate` 惰性实例化，":22 loads wasm only when a decoder is requested"。**Bentley 的 WASM 边界 = 瓦片解码。**
2. **draco WASM 动态加载**：`core/frontend/src/common/gltf/GltfParser.ts:12`（import type DracoLoader）、`:325 (await import("@loaders.gl/draco")).DracoLoader`——draco 解码器本体是 C++→WASM 的 Google 官方构建，同样用在 glTF 网格解码路径。
3. **Node 侧原生边界**：`core/backend/package.json:111` 依赖 `@bentley/imodeljs-native@5.14.32`；`backend/src/internal/NativePlatform.ts:9 import { IModelJsNative, NativeLibrary } from "@bentley/imodeljs-native"`；`IModelHost.ts:556 loadNativePlatform()`——重内核走 Node 原生插件，**浏览器前端不加载任何原生代码**（`core/bentley/src/ProcessDetector.ts:17` isBrowserProcess 判定即边界）。
4. **线程边界先于 WASM 边界**：`workers/ImdlParser/Worker.ts:31-48` + `internal/tile/ImdlParser.ts:37-41`（acquireImdlParser 主线程内联或 Worker 两实现）——Bentley 已把"解码"放到 Worker，WASM 化是同一思路的下一步。
5. Sprite 资产随原生包分发：`core/frontend/src/Sprites.ts:61` 注释指 assets 目录在 imodeljs-native 包内——前端资源仍部分依赖原生包发布通道。

## 5. 风险与前置条件

**Emscripten 平台后端（最大前置工作）**
- `dqRender/src/platform/PlatformFactory.cpp` 现 `#else → nullptr`；须新增 PlatformEmscripten：`emscripten_webgl_create_context` 建上下文、`Swapchain::rebind(void*)`（`Swapchain.h`）映射 canvas 句柄、resize=表面过期重建（DanQing WGL 侧已有同款语义，机制可直接平移）。`RenderSystem::createTarget(void* nativeWindow, w, h)`（`RenderSystem.h:85`）的 void* 原生句柄约定与 Emscripten canvas selector 天然契合。
- **线程模型**：dqBase 用 std::mutex/condition_variable（`BeThread.h:26-30`），dqRender/dqCommon/dqGeom 的 src **当前零 std::thread**（解码同步执行）——单线程 WASM 无移植成本；若要保留"解码不阻塞渲染"（对齐 itwinjs 的 Worker 先例），需 `-pthread` + SharedArrayBuffer + COOP/COEP 跨源隔离头，且 Emscripten 线程与 Web Worker 的桥接要自己编排。
- **文件 I/O**：`GltfReader.cpp:251/678-717` 的 `ifstream/_wfopen` 直读需收敛为内存入口（上游本来就是 fetch/RPC 字节）；`DANQING_TEST_ASSET_ROOT` 之类编译期资产路径（`dqRender/CMakeLists.txt:366-367`）仅测试用，不影响 SDK。

**WebGL2 vs desktop GL 差异**
- GLSL：`#version 410 core`（`ShaderBuilder.h:468` 默认值）→ 需 `setVersion("300 es")`（接口已存在，`ShaderBuilder.h:384`），并处理 ES 语义差（precision、隐式转换、textureLod 可用性）。itwinjs 侧目标就是 300 es（`ShaderBuilder.ts:508`），可逐 shader 对照。
- 不可用 API：`glBlendFuncSeparatei/glBlendEquationSeparatei`（`GlLoader.cpp:26-29`）不在 WebGL2——当前无调用点，合成 pass 用非索引混合重写该处即可；`glLineWidth>1` 被忽略（`GLCanvasContext.cpp:451` 已用 1.0，无影响）。
- 可用但需扩展：RGBA16F 渲染目标需 `EXT_color_buffer_float`（itwinjs OIT 同款依赖）；MRT 3 附件（color/featureId/depthAndOrder）在 WebGL2 最小保证 4 draw buffers 之内。`GL.h:5-8` 注释已确认"WebGL2 常量与 GL 4.1 Core 1:1 同值"，常量层无差异。

**接口阻抗（TS↔WASM 接缝）**
- 已对齐：changeScene/changeDecorations/changeRenderPlan/drawFrame/setViewRect/updateViewRect/readPixels/setHiliteSet/setFlashed（`RenderTarget.h:89-194` vs `RenderTarget.ts:109-134`）；DanQing 甚至预分了 readPixels/readPickData/readPickDepth（`:137/149/160`），恰好对应 `Pixel.Selector` 三语义。
- 待补：changeDynamics（动态 overlay 图元对）、screenSpaceEffects、analysisFraction、animationBranches（`RenderTarget.ts:79-84/111/158`）——均为 DanQing 公开头中尚未出现的成员，小型增补。
- 数据编组成本：每帧跨缝传 Scene/Decorations 图元描述需扁平化二进制协议（建议参照 `common/imdl/ImdlModel.ts` 的 transferable 模式），否则 marshalling 会吃掉 R2 的收益——**接缝数据最小化是架构级决策**。

**Qt 桥替换面**
- 仅 dqApp 含 Qt（根 `CMakeLists.txt:97-104` 无 Qt 时跳过 dqApp/samples；引擎四模块零 Qt，`CMakeLists.txt:63-69`）。WASM 场景 dqApp 整体不编——TS Viewport/ToolAdmin 就是 dqApp 的等价物。`QtTileRequestFetcher` 的角色由 TS fetch/RPC 直接承担（dqRender 侧已抽象为 `ITileFetcher.h` DI 接口，天然可换）。

**dqGeom 覆盖前置**
- R5/R7 要兑现收益前，dqGeom 需按 §0 补齐 bspline/numerics/topology 的移植（当前 `PublicAPI/dqGeom/` 54 头中无 BSpline 独立模块），否则只能覆盖 core-geometry 的核心子集。

**性能预期边界**
- 收益集中在：瓦片解码（R1，已有 Bentley 自己的 Worker+WASM 佐证）、每帧提交路径 CPU 工作（R2/R3/R4 的 BranchStack/RenderCommands/FeatureOverrides）、装饰几何重建（R7）。
- 无收益：GPU 绘制本身（draw call 提交后）、RPC/网络等待、DOM 事件处理。若渲染本就 GPU-bound，端到端帧率提升有限——WASM 化的第一价值是**主线程解放与掉帧毛刺消除**（解码/构建不再与交互争用 JS 线程），第二才是吞吐。

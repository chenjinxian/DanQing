# itwinjs-core 大体量渲染核心机制深挖 + DanQing 对照报告

> 2026-09-24 战略分析（Authored：源码取证报告，非参考移植）。姊妹篇：
> `filament-跨平台机制分析-2026-09-24.md` 等四份。综合架构结论见
> `双源优势结合-目标架构与重构路线-2026-09-24.md`。
> 所有 file:line 证据来自分析时真实读过的源码（itwinjs-core / DanQing 仓内）。

## 1. 结论摘要

1. itwinjs 的 Tile 链路是"**视口每帧选瓦 → missing 集合 → TileAdmin 优先级队列 → 分 channel 限流并发 → 解码 → onTileLoad → invalidateAllScenes 级联重绘**"，选择递归（`IModelTile.selectTiles`）的 SelectParent 协议与跳级规则（`maxTilesToSkip`/`maxInitialTilesToSkip`）是行为核心。
2. imdl 二进制 = `[ImdlHeader][FeatureTable][Bentley 变体 20B glTF 头][JSON scene][BIN chunk]`；顶点在**线上是 16-bit 量化（QPoint3d）**，feature 表是 3×u32/feature 的 PackedFeatureTable。
3. **Vertex LUT 是 itwinjs 的内存基石**：每顶点 3-4 个 RGBA8 texel（量化位置+colorIndex+24-bit featureIndex[+oct 法线][+qUV]），索引流是 24-bit/角的字节流走 `gl.drawArrays`（attribute 喂索引 → VS 采样 LUT），这是 WebGL1 无 `gl_VertexID` 时代的 vertex pulling。
4. DanQing 现状：**调度骨架/LRU/格式头/feature LUT/polyline LUT 已对齐**；但 **imdl 消费走"CPU 解码→f64 polyface→f32 交错 VBO"**（`ImdlGraphics.cpp` + `PolyfaceGraphic.cpp`），**未走 Vertex LUT 纹理路径**——缺失每顶点 featureIndex/UV/颜色/边缘/实例/meshopt，SSE 判定用合成的 depth 几何误差近似（`ImdlTileTree.cpp:303-306`），选择递归是简化版 BatchedTile 式。
5. 升级空间最大项：LUT 采样改 `texelFetch`+整数纹理（GL 4.1 行为中性）、RGBA16UI 单 texel/顶点（12B→8B）、feature LUT 改 UBO/结构化 SSBO、解码上工作线程；桌面 GL 4.1 下 MDI/bindless 不可用（需 4.3/4.4）。
6. §0 兼容性：wire format 与可见行为可完全保真（像素级回归锁）；GPU 表示层改动按 §11.10 等价登记制走，凡涉数值路径（CPU f64 解量化 vs shader f32 解量化）需专项对照测试。

## 2. Tile 加载全链（机制卡，全部 file:line 已实读）

### 2.1 视口驱动（每帧）
| 环节 | 机制 | 证据 |
|---|---|---|
| 帧入口 | `renderFrame` 24 步管线；scene 失效→`createScene(context)`→`context.requestMissingTiles()`→`_hasMissingTiles = context.hasMissingTiles` | `core/frontend/src/Viewport.ts:2647-2660` |
| missing 收集 | `SceneContext.insertMissingTile` 只对 NotLoaded/Queued/Loading 入集；`requestMissingTiles`→`tileAdmin.requestTiles(viewport, missingTiles)` | `core/frontend/src/ViewContext.ts:421-434` |
| 每树选择入口 | `TileTree.selectTiles`（final）→ `_selectTiles` → `tileAdmin.addTilesForUser(vp, selected, ready, touched)` | `core/frontend/src/tile/TileTree.ts:136-142`；`TileAdmin.ts:514-533` |
| iModel 树 | staticBranch 先选、dynamic 后选；`draw()` 里再 selectTiles 并区分 static/dynamic | `core/frontend/src/internal/tile/IModelTileTree.ts:435-452` |
| 到达重绘 | `onTileLoad/onTileChildrenLoad → invalidateAllScenes()`（构造时挂接）；连续渲染请求**不含** missing tiles，靠事件级联 | `TileAdmin.ts:319-321`；`ViewManager.ts:397-416` |

### 2.2 选择递归（SSE 判定核心）
| 环节 | 机制 | 证据 |
|---|---|---|
| 可见性 | `computeVisibility`：empty→Outside；boundingRange 相交；区域剔除→Outside；不可显示→TooCoarse；叶再看 contentRange；否则 `meetsScreenSpaceError` | `core/frontend/src/tile/Tile.ts:429-453` |
| **SSE 判定式** | `args.getPixelSize(this) * pixelSizeScaleFactor <= this.maximumSize * args.tileSizeModifier` | `Tile.ts:459-463` |
| pixelSize | 包围球：`(0.5·diag).length / metersPerPixelAtClosestPoint`；OBB 角点投影矩形（地图/实景）；metersPerPixel 由 worldToViewMap 在"离眼最近球点"处差分 | `TileDrawArgs.ts:138-148,151-176,190-211` |
| 默认阈值 | `maximumScreenSpaceError ?? 16`（Cesium 默认） | `TileDrawArgs.ts:279` |
| 选择递归 | SelectParent 协议：Visible 但未就绪→`insertMissing`，有图元选它，否则尝试"直接子代全就绪则画子代否则画父"；TooCoarse→`loadChildren()`（异步），`maxInitialTilesToSkip` 允许初始向下钻、`maxTilesToSkip`（默认 1，`TileAdmin.ts:284-287`）限制非细分瓦连续跳级；`parentsAndChildrenExclusive` 控制父+子共存 | `IModelTile.ts:205-334` |
| 剔除 | 视锥平面（box+sphere）→ tree clip → view clip → intersectionClip 四层 | `Tile.ts:391-426` |
| 子代生成 | `computeChildTileProps`：magnification（单孩子×2）或 2/3 轴对剖 4/8 孩子，emptySubRangeMask 跳过已知空子卷，模型 contentRange 拒绝 | `IModelTile.ts:153-169`；DanQing 移植版 `ImdlTileTree.cpp:76-138`（对齐 `TileMetadata.ts:777-853`） |

### 2.3 调度与驱逐
| 环节 | 机制 | 证据 |
|---|---|---|
| 每帧 process | `process() = processQueue + pruneAndPurge + freeMemory` | `TileAdmin.ts:445-453,829-895` |
| 请求生成 | 每 user 的 missing 集→每 tile 一个 `TileRequest`→`channel.append` | `TileAdmin.ts:897-922` |
| 优先级 | 比较器：先 `tree.loadPriority`（Dynamic 5/Terrain 10/Map 15/Primary 20/Context 40/Classifier 50，`Tile.ts:626-639`），再 `request.priority`（默认=tile depth，`Tile.ts:252-254`）；每帧 process 时重算+重排 | `TileRequestChannel.ts:13-20,235-240` |
| 并发 | channel `_concurrency`（HTTP 域 6）；dispatch 至上限；`swapPending` 双缓冲：上轮仍 pending 且 users 空→cancel；active users 空→cancel（HTTP 层不撤但记账） | `TileRequestChannel.ts:146-149,215-219,232-266` |
| LRU/预算 | `LRUTileList` 记录 bytesUsed；`gpuMemoryLimit` default/aggressive/relaxed/none→`maxTotalTileContentBytes`；超预算驱逐未选中分区；`isPreloadingAllowed` 在用量 >percentGPUMemDisablePreload(默认80)% 时停预取；tile 过期默认 20s、tree 300s | `TileAdmin.ts:182,382-440,759-771,844-847`；`TileAdmin.ts:258,303-306` |
| 解码 | imdl 默认 **worker 解码**（`decodeImdlInWorker ?? true`）；meshopt 按需加载 | `TileAdmin.ts:254`；`internal/tile/ImdlDecoder.ts:66`；`ParseImdlDocument.ts:262-266` |

### 2.4 提交
| 环节 | 机制 | 证据 |
|---|---|---|
| RenderCommands | 按 RenderPass 分桶（`_commands[RenderPass.COUNT]`），批/分支/裁剪 Push/Pop 命令保序，PrimitiveCommand=(program,params)；pass 内按 program 排序 | `internal/render/webgl/RenderCommands.ts:36-40`；`DrawCommand.ts:190-247` |
| Batch | 每个 Batch 持 FeatureTable + **独立 FeatureOverrides LUT 纹理**，PushBatchCommand 绑定 | `FeatureOverrides.ts:42-117,443-450` |
| 实例化 | `ImdlInstances`：count + transformCenter + featureIds + transforms（12 float/实例=3×4 行主序）+ symbologyOverrides；`a_featureId`/`a_instanceOverrides` attribute + `u_instanced_rtc` RTC | `ParseImdlDocument.ts:1045-1087`；`InstancedGeometry.ts:345,403-454`；`glsl/Vertex.ts:120-139` |
| 拾取 MRT | `opaqueAll = [color, featureId, depthAndOrder]`（MS 时 RGBA8 resolve + Nearest）；depthAndOrder.x=renderOrder/16、yzw=RGB 编码线性深度；同 feature 早弃优化 | `SceneCompositor.ts:320-347`；`glsl/FeatureSymbology.ts:395-401,403-469`；`Viewport.ts:2753-2778`（`Pixel.Selector.GeometryAndDistance`） |

## 3. imdl 格式规格 + Vertex LUT 布局

### 3.1 imdl 二进制布局（`IModelTileIO.ts:50-101`、`TileIO.ts:97-105`、`ParseImdlDocument.ts:1269-1327`）
```
[0]  u32 magic = 0x6c644d69 ("iMdl")          TileFormat.IModel (TileIO.ts:20)
[4]  u32 version (major<<16 | minor; 当前 Major=37, IModelTileIO.ts:38)
[8]  u32 headerLength
[12] u32 flags (ContainsCurves=1, Incomplete=4, DisallowMagnification=8,
                MultiModelFeatureTable=16   IModelTileIO.ts:17-28)
[16] f64×3 contentRange.low, [40] f64×3 contentRange.high
[64] f64 tolerance（facet 弦容差）
[72] u32 numElementsIncluded, [76] u32 numElementsExcluded
[80] u32 tileLength, [84] u32 emptySubRanges (v2+)
     —— 剩余 headerLength 跳过 (IModelTileIO.ts:95-97)
[headerLength] FeatureTable: u32 length, u32 numSubCategories, u32 count (12B,
     IModelTileIO.ts:107-130)，随后 length-12 字节的 packed u32 数组
[...] Bentley 变体 glTF 头 (20B): format/version/gltfLength/sceneStrLength/value5
     (v1: scene-format 哨兵; v2: 'JSON' chunk type + BIN chunk 头) — DanQing 移植
     ImdlDocument.cpp:35-80
[...] JSON scene（meshes/nodes/materials/bufferViews/namedTextures/...，
     ImdlSchema.ts:436-454）
[...] BIN chunk（bufferView 引用的二进制块）
```
**PackedFeatureTable**（`core/common/src/internal/PackedFeatureTable.ts:60-97,100-155,214`）：每 feature 3×u32 = `[elementId.lo, elementId.hi, subCatIndex | geometryClass<<24]`；表尾 2×u32/subcategory（subCatIndex→Id64）；MultiModel 变体再追加 3×u32/model（`lastFeatureIndex, id.lo, id.hi`，二分/并行迭代取 modelId，`:239-303`）。

**量化顶点（线上格式）**：QPoint3d = u16×3，`world = origin + scale·q`，`origin=decodedMin`、`scale=(decodedMax-min)/65535`（`ParseImdlDocument.ts:1013-1018`；QParams3d）。oct-encoded normal = u16（shader 内 octDecode，见 `glsl/Surface.ts` 引用）。UV = QPoint2d u16×2 + `surface.uvParams`（`ImdlSchema.ts:339-342`）。surface.indices = **24-bit LE**（`VertexIndices.ts:16-29`）。meshopt：`vertices.compressedSize`→`decodeVertexBuffer`（`:969-1003`）、`surface.compressedIndexCount`→`decodeIndexSequence` 32→24bit（`:882-900`）。

### 3.2 Vertex LUT texel 级布局（`VertexTableBuilder.ts`，单位 = RGBA8 texel）

**Quantized（tile 线上格式，每顶点 3-4 texel = 12-16B）：**
```
SimpleBuilder (12B, numRgbaPerVertex=3, :224-279):
  texel0: pos.x(u16) pos.y(u16)
  texel1: pos.z(u16) colorIndex(u16)      // colorIndex 非颜色表时=0
  texel2: featureIndex(u24 LE) | materialIndex(u8)   // .xy=.z=分量，.w=高字节
LitMeshBuilder (16B, :385-399): texel0-1 同上; texel2: featureIndex+materialIndex
                                texel3.xy = octNormal(u16), texel3.zw = 未用
TexturedMeshBuilder (16B, :342-373): colorIndex 位被 qUV(u16×2) 取代
TexturedLitMeshBuilder (16B, :375-383): texel2 = feature+material, texel3.xy =
                                octNormal, texel3.zw = qUV
PolylineBuilder (16B, :281-300): 12B 同上 + texel3 = cumulativeDistance(f32 位拷贝)
```
**Unquantized（装饰图元，每顶点 5-6 texel = 20-24B，`:402-628`）：** 位置 f32×3 与 featureIndex **按字节位面转置**（`[0].xyz-[3].xyz + .w=featID` 每字节 x/y/z/feat 各取一字节，`:455-479`），texel4 = colorIndex+unused，polyline texel5 = cumulativeDistance；textured 覆写 texel4 为 qUV；lit 覆写为 octNormal。

**纹理尺寸**：`computeDimensions`——`nRgba = n·numRgbaPerVertex + nExtra`；≤maxSize 则 `(nRgba,1)`；否则宽=ceil(sqrt) 向上对齐到 numRgbaPerVertex 整数倍（**保证一顶点不跨行**），高=ceil(nRgba/width)（`VertexTable.ts:53-81`）；颜色表追加在顶点之后（`build :178-208`）。

**feature LUT（FeatureOverrides，3 texel/feature = 12B，`FeatureOverrides.ts:174-187`）：**
```
texel0: R=OvrFlags(lo) G=OvrFlags16(hi) B=lineCode A=lineWeight
texel1: R=r G=g B=b A=alpha
texel2: R=lineR G=lineG B=lineB A=lineAlpha
```
增量策略：overrides 引用变更/hilite 变更/flashId 变更/pickExclusions 变更四源 sync 观察器（`:412-441`）；flash/hilite-only 走"只重写 flags 字节"的部分更新（`:290-375`），symbology 全变才 `buildLookupTable` 全量（`Texture2DDataUpdater` + `lut.update()` 局部上传，`:119-130`）。

**Shader 消费**（`glsl/Vertex.ts`）：
- `a_pos` attribute = 3×u8 的 **24-bit 顶点索引**（`SurfaceGeometry.ts:384`），draw 走 `gl.drawArrays(Triangles,0,numIndices)`（`SurfaceGeometry.ts:150-162`）——索引即顶点。
- VS 主流程：`g_vertexLUTIndex = decodeUInt24(qpos)` → `compute_vert_coords`（`glsl/LookupTable.ts:11-21`，含 `0.5/width` epsilon 修正 mod 精度陷阱）→ preread `g_vertLutData0..3(+4,5)`（`Vertex.ts:193-212,253-254`，注释明言"预先读完有小性能收益"）→ `computeVertexPosition` = `decodeUInt16×3 + unquantizePosition(qOrigin,qScale)`（`:35-41`）；`g_featureAndMaterialIndex = g_vertLutData2`。
- feature 索引：`getFeatureIndex = decodeUInt24(g_featureAndMaterialIndex.xyz)` → `compute_feature_coords`（mult=3.0，每 feature 3 texel，`glsl/FeatureSymbology.ts:61-101,255-269`）→ 逐顶点取 overrides（visibility 早弃、颜色/透明度、线宽/线型、flash/hilite/emphasis 位）。
- 多线 LUT：`a_pos`(本点索引)+`a_prevIndex`+`a_nextIndex&param` 三条 24-bit 索引流（`CachedGeometry.ts:1122-1158`），边缘 LUT：`IndexedEdgeParams`（edges 纹理 + 6 索引/segment + numSegments + silhouettePadding，`ImdlSchema.ts:219-232`）。

## 4. DanQing 现状对照表

| 机制 | itwinjs 参考 | DanQing 现状 | 评级 |
|---|---|---|---|
| Tile 状态机 | NotLoaded/Queued/Loading/Ready/NotFound/Abandoned（`Tile.ts:271-295`） | 同形状态机（`Tile.h` + `TileAdmin.cpp:106-119`） | 对齐 |
| 选择递归 | SelectParent + 跳级/独占规则（`IModelTile.ts:205-334`） | 简化 BatchedTile 式（`TileTree.cpp:39-72`）：无 SelectParent、无 maxTilesToSkip/maxInitialTilesToSkip、无 sizeMultiplier 选择语义、无 ready/selected 细分 | 部分 |
| SSE 判定 | `pixelSize ≤ maximumSize·tileSizeModifier`（`Tile.ts:459-463`） | `geometricError=tileScreenSize/2^depth; sse=geometricError/pixelSize`（`ImdlTileTree.cpp:292-308`，代码内自述"registered simplification"）；RealityTileTree 用真 geometricError（`RealityTileTree.cpp:65-123`） | 部分（偏差已登记） |
| pixelSize | 最近点/角点投影差分（`TileDrawArgs.ts:190-211`） | `extents.y/viewHeightPx` + 透视 eye 距离（`dqApp Viewport.cpp:1884-1905`） | 部分 |
| 剔除四层 | 视锥+tree clip+view clip+intersection（`Tile.ts:400-426`） | 视锥+包围球（`ImdlTileTree.cpp:279-286`）；clip 层未接 | 部分 |
| 调度 | 双缓冲 pending、每帧重算优先级、users 空即 cancel、先 tree 优先级（`TileRequestChannel.ts:13-20,232-266`） | 单 "default" channel、按 priority 排序+并发上限（`TileRequestChannel.cpp:26-77`）；**无 per-tree 优先级键、无 users 集合/取消、无 swapPending** | 部分 |
| LRU/预算/驱逐 | markUsed×3 集 + bytesUsed 预算 + 20s 过期 + preload 门 | markUsed/add/drop/freeMemory 骨架齐（`TileAdmin.cpp:136-166,254-268,361-367`）；isPreloadingAllowed 门缺失；bytesUsed 记账依赖各 content 上报（imdl→PolyfaceGraphic 路径需核实填充点） | 部分 |
| 解码执行位 | 默认 worker 线程（`TileAdmin.ts:254`） | fetcher 轮询线程同步 `deliverTileContent→readContent`（`TileAdmin.cpp:281-293`） | 缺失（桌面可接受，大 tile 会卡） |
| imdl 头/FT/glTF 段 | 完整 | 对齐（`ImdlHeader.cpp:51`、`ImdlDocument.cpp:191-244`），leaf 启发式移植 `TileMetadata.ts:880-940` | 对齐 |
| FeatureTable 消费 | PackedFeatureTable→Batch→GPU LUT | 词向量保留在 `ImdlDocument::featureData`（`ImdlDocument.h:49`）但 **`ImdlTile::readContent` 不产出 PackedFeatureTable**（`ImdlTileTree.cpp:177-219`）→ Batch 拿不到表 | 缺失 |
| **imdl 顶点消费路径** | 零 CPU 逐顶点工作：u8 表→纹理（`VertexLUT.ts:93-99`） | **CPU 解码**：qpos→f64 world（`ImdlGraphics.cpp:90-101`）→IndexedPolyface(f64)→fan 三角化→f32 交错 VBO（`PolyfaceGraphic.cpp:47-120`）；oct 法线 CPU 侧解码（`:106-122`） | 缺失（形态性偏差） |
| imdl 颜色/UV/边缘/实例/meshopt | 全路径（`ParseImdlDocument.ts:710-1087`） | 全不消费（`ImdlGraphics.cpp` 只取 qpos+octN+surface 索引） | 缺失 |
| VertexLutTexture | RGBA8 纹理 + width/height 保真上传 | 存在且尺寸语义已修正（`VertexLutTexture.cpp:17-53`，注释记录过 pow2 重建事故） | 对齐（能力在） |
| VertexTableBuilder | 全 builder 族（量化+非量化×mesh/polyline/point） | 仅 `Unquantized.PolylineBuilder` 单路径（`VertexTableBuilder.cpp:131-212`），且只为 ACS 线服务 | 部分 |
| LUT shader 消费 | surface/edge/polyline/pointString 全量 | 仅 polyline（`PolylineShaderBuilder.cpp:188-245`，unquantized 完整、quantized "simplified 1-texel" 降级） | 部分 |
| FeatureOverrides | 3-texel OvrFlags 布局 + 增量更新 + per-Batch 纹理 | **双系统并存**：`FeatureOverrideLUT` 3-texel 版已移植并接 Batch（`FeatureOverrideLUT.cpp:140-190`、`Batch.cpp:25-54`、`Uniforms.h:196`）+ 旧 `FeatureOverrides` 1-texel 颜色 stub（`FeatureOverrides.cpp:38-57`） | 部分（需收敛到 FeatureOverrideLUT） |
| 拾取 MRT | [color, featureId, depthAndOrder] + pingPong | 附件布局 + pingPong 拷贝已移植（`SceneCompositorImpl.cpp:999-1023`；`SurfaceCommon.h:173-189` MRT 双写注释） | 对齐（能力在） |
| 实例化 | ImdlInstances + a_featureId + RTC | `InstanceBuffers/InstancedGeometry` 骨架存在；imdl instances 不消费；glTF 路径未走实例 | 部分 |

**两种 imdl 消费形态的性能/内存特征（实测推理，基于源码布局）**——以 10 万顶点/30 万角的典型 tile 计：
- itwinjs LUT 形：顶点表 16B/顶点 = 1.6MB + 索引流 3B/角 = 0.9MB ≈ **2.5MB**；CPU 侧零逐顶点运算（`glTexImage2D` 直传）；GPU 侧每顶点 3-4 次 texture fetch（preread 后实际 1 次 LUT 采样/顶点 + 索引 attribute）。
- DanQing VBO 形：CPU 侧每顶点 f64 解量化 + octDecode + polyface 构造 + 每角 fan 展开（顶点不共享，PolyfaceGraphic 按角发顶点）→ f32 顶点 ~(12B pos+12B normal)×30 万角 ≈ **7-14MB**（无索引复用时更高），上传带宽 3-5×，且 IndexedPolyface f64 中间态内存峰值再翻倍。CPU 解码时延直接阻塞 fetcher 轮询线程。
- 反方向权衡：VBO attribute 路径无 LUT 采样间接层、无 24-bit 解码开销、与 GL 4.1 全兼容无精度陷阱——"更现代"不等于"更省"，itwinjs 的 LUT 形对 BIM 高顶点复用场景是真优化，**建议保 LUT 形为主消费路径**。

## 5. 升级空间清单（妥协证据 / 升级机制 / 收益 / §0 评级）

| # | itwinjs 妥协（证据） | 升级机制（DanQing 桌面 GL 4.1+） | 收益 | §0 兼容性评级 |
|---|---|---|---|---|
| U1 | LUT 经 `TEXTURE()` 归一化采样 + `*255+0.5` 反编码 + `computeLUTCoords` epsilon 修正（`glsl/Vertex.ts:70-87`；`glsl/LookupTable.ts:15-18`） | `texelFetch` 整数寻址（RGBA8UI/普通 RGBA8 均可），删 epsilon 修正与 255 缩放 | 删一层 f32 归一化往返与半像素偏移陷阱；精度更稳 | **行为中性**（同值采样；需 1 个像素级回归锁） |
| U2 | 12-16B/顶点 RGBA8 LUT、24-bit featureIndex（`VertexTableBuilder.ts:224-279`） | **RGBA16UI 单 texel/顶点**：qpos xyz 三通道 48bit + w=featureIndex u32；法线/UV 走第二 texel（RG16UI+RG16UI oct/qUV） | 12B→8B/顶点，featureIndex 24→32bit 无材料位挤占；整数纹理 4.1 core | **行为中性偏优**（数值相同、位宽变大；等价登记 §11.10 + 回归） |
| U3 | feature 索引经 LUT 二次采样（`glsl/FeatureSymbology.ts:100-117`） | feature overrides 表改 **UBO 结构化数组**（GL 4.1 VS 动态索引 UBO 合法）：`struct FeatureOverride { u32 flags; u8 lineCode; u8 lineWeight; u8 rgb[4]; u8 lineRgb[4]; }`（20B 对齐 16→24B/feature） | 删 3-texel 纹理与 mult=3 坐标计算；CPU 侧 m_data 直写 UBO 持久映射，增量更新零上传开销 | **行为中性**（表语义 1:1 迁移；OvrFlags 位序保持） |
| U4 | 索引流 24-bit 字节 attribute + `drawArrays`（`VertexIndices.ts`；`SurfaceGeometry.ts:384,162`） | 桌面有 `gl_VertexID`：索引用 u32 真索引缓冲 + `drawElements`（顶点表在 LUT/SSBO 时 VS 内以 gl_VertexID 查），或保持 attribute 流但升 u32 | 每角 3B→4B 但免 24-bit 移位解码；可用真 indexed draw | **行为中性**（纯表示层；§3 命名对齐保持 `VertexIndices` 名） |
| U5 | 每 PrimitiveCommand 一次 draw + 全量 uniform 重绑（`RenderCommands.ts:36-40`；`DrawCommand.ts:190-247`） | GL 4.1 无 MDI（需 4.3）——可做：同 program 图元按 branch uniform 分组实例化渲染；UBO 装 branch 数组；驱动批 `drawArrays` 循环化 | draw call 数降一个量级 | **半行为中性**（渲染顺序/RenderOrder 语义必须保持——排序键即行为；仅"同键合并"安全） |
| U6 | imdl 解码默认 worker（`TileAdmin.ts:254`；meshopt decoder） | DanQing 同步解码在轮询线程（`TileAdmin.cpp:281-293`）→ 移植线程池解码（dqBase 已有线程设施）；meshopt 引入 `third_party`（参考 `getMeshoptDecoder`） | 大 tile 不卡帧；压缩传输省带宽 | **行为中性**（执行位迁移） |
| U7 | CPU 侧零解码（纹理直传）vs DanQing f64 解量化+octDecode+polyface+fan（`ImdlGraphics.cpp:90-145`） | 恢复参考形态：imdl 顶点表**不解码**，直接上传 LUT 纹理；保留 polyface 路径仅作 glTF/装饰图元通道 | 4-5× GPU 内存、解码时延近零、带宽大降 | **§0 正收益回归**（这才是参考机制本体——TD-18/19 的自注已承认是"minimal pass"）；数值上 shader f32 解量化 vs 现 f64→f32 有 ~1ulp 差，需等价登记+像素回归 |
| U8 | PackedFeatureTable→Batch→feature LUT 全链（`PackedFeatureTable.ts`；`FeatureOverrides.ts`） | 补齐 `ImdlTile::readContent` 的 featureTable 产出 + dqCommon::PackedFeatureTable 接线（dqCommon 已有类型） | 拾取/高亮/按元素 symbology 的前提 | **行为补齐**（纯移植缺口） |
| U9 | IModelTile SSE = maximumSize/pixelSize（`Tile.ts:459-463`）+ 选择递归跳级协议 | 用真实 `TileProps.maximumSize`（tree props 已有）替换合成 `tileScreenSize/2^depth`；移植完整 `selectTiles` 递归 | LOD 切换点与参考逐像素一致 | **行为修复**（当前是已登记近似） |
| U10 | WebGL 无 SSBO/4.1 无 bindless、无 persistent map | GL 4.1 有 `ARB_shader_storage_buffer_object`？——**无**（4.3）。4.1 可用：persistent map 需 `ARB_buffer_storage`（4.4）也不可用 → 仅 UBO + 映射上传；bindless（4.4+）与 MDI（4.3+）列为 WebGL2/WASM 远期目标 | 明确能力边界，避免写 4.1 不支持的代码 | 边界声明 |
| U11 | edge 体系：segments/silhouettes/indexed/compact 四级（`ImdlSchema.ts:195-286`） | DanQing 未消费任何 imdl 边缘；EdgeShaderBuilder 已有非索引边着色器 | 隐藏线/可见边渲染的前提 | 移植缺口（非升级） |
| U12 | 边缘早弃靠 pick 纹理回读（`glsl/FeatureSymbology.ts:403-469`） | 桌面可改 `GL_EXT_texture_buffer`/early-z 或多 pass 深度预载——但属行为等价替换，收益小风险中 | — | 不建议先做 |

**落地优先级建议**：U8→U7→U9（把 imdl 消费链补成参考形态，行为正确性是前提）→ U1/U2/U3（GL 4.1 表示层升级，全部可逆且各有像素回归锁）→ U6/U5（吞吐）→ U11（边缘）。每项落地须满足 §5 测试保真（参考测试优先 `core/frontend/src/test/imdl/ImdlParser.test.ts:284-443` 的 meshopt 夹具）与 §11.10 等价登记制。

---

**关键文件索引（基于 D:\Github 检出）**
- 参考：`itwinjs-core\core\frontend\src\tile\{Tile,TileTree,TileAdmin,TileDrawArgs,TileRequestChannel}.ts`、`core\frontend\src\internal\tile\IModelTile.ts`、`core\frontend\src\common\imdl\{ParseImdlDocument,ImdlSchema}.ts`、`core\common\src\tile\IModelTileIO.ts`、`core\common\src\internal\PackedFeatureTable.ts`、`core\frontend\src\internal\render\webgl\{VertexLUT,FeatureOverrides,MeshData,CachedGeometry,SurfaceGeometry,SceneCompositor}.ts`、`core\frontend\src\common\internal\render\{VertexTable,VertexTableBuilder,VertexIndices}.ts`、`core\frontend\src\internal\render\webgl\glsl\{Vertex,LookupTable,FeatureSymbology}.ts`
- DanQing：`dqRender\src\tile\{ImdlGraphics,ImdlDocument,ImdlTileTree,TileAdmin,TileRequestChannel,TileTree}.cpp`、`dqRender\src\render\{VertexLutTexture,VertexTableBuilder,FeatureOverrideLUT,FeatureOverrides,PolyfaceGraphic,MeshBuilder,Batch,SceneCompositorImpl}.cpp`、`dqApp\src\Viewport.cpp:1880-1948`

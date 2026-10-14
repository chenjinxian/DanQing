# 阶段 4 M-W Analysis Style 与装饰演示 实现计划（2026-10-14）

> 里程碑：M-W（Analysis Style 大件 7/16 + PathDecoration/AspectRatioSkew 两装饰演示）。
> 基线：M-V 收口（2026-10-14，全量 ctest 2793 项 100% 通过）。权威对照面：
> `docs/DTA功能对照统计-2026-10-07.md`（§2 大件 #10 Analysis Style / #13 Path·AspectRatio 装饰演示）+
> CLAUDE.md §15 余量序。
> 用户指令：继续阶段 4 清偿 DTA 差异功能余量（2026-10-14 会话）；操作模式=主会话编排 +
> 每次 2 子代理并行（worktree 隔离，契约见 §3）；四裁决已落：Background Map 结案 /
> Atmosphere 排 M-Z B / 点云 M-Z spike / .bim 架构外结案。
> 参考检出：D:/Github/itwinjs-core（只读）。

## 0. 范围勘察结论（参考全读记录）

### 0.1 A 道 Analysis Style——参考侧全读

**数据层（core/geometry）**：
- `core/geometry/src/polyface/AuxData.ts`（305 行，全读）：
  - `AuxChannelDataType`（:21-38）：Scalar=0/Distance=1/Vector=2/Normal=3——变换语义逐型
    （Scalar 不变/Distance 乘 scale/Vector 旋转缩放/Normal 逆转置+重规范化）。
  - `AuxChannelData`（:43-77）：input+values；copyValues 块拷贝/clone 深拷贝/
    isAlmostEqual（默认 tol 1.0e-8）。
  - `AuxChannel`（:82-177）：data/dataType/name?/inputName?；isScalar 静态+实例双形态；
    entriesPerValue（scalar=1 他=3）；valueCount；scalarRange（非 scalar→undefined）；
    computeDisplacementRange(scale, result?)（:164-176，Vector 型逐点 extendXYZ）。
  - `PolyfaceAuxData`（:191-293）：channels+indices；clone/isAlmostEqual（indices 精确+channels
    容差）/静态 isAlmostEqual（双 undefined 捕获 :225）；createForVisitor（:233-245——
    值数组留空骨架）；tryTransformInPlace（:252-292——scale=|det|^(1/3)×sign(det)；
    Normal 型 inverseRot 懒建+multiplyTransposeVectorInPlace+重规范化 tol 1.0e-15 怪癖
    :276；不可逆→false）。
  - 自由函数 transformPoints（:295-304）。
- `core/geometry/src/test/polyface/PolyfaceAuxData.test.ts`——A 道测试移植源（全读归代理）。
- **Polyface 变换联动**：`createCantilever` 对 polyface 调 tryTransformInPlace(scale 30)
  （AnalysisStyleExample.ts:73-74）——代理须核 IndexedPolyface/PolyfaceData 变换时
  auxData 是否联动（参考 Polyface.ts 变换链），联动则 DanQing 同步移植。
- **IModelJson 读取面子集**（Cantilever 资产消费所需）：
  `core/geometry/src/serialization/IModelJsonSchema.ts`（2008 行）——只需
  Reader.parsePolyfaceAuxData（:960-980 区——channels/data{input,values}/name/inputName/
  dataType + SerializationHelpers.announceZeroBasedIndicesFromSignedOneBasedIndices :973）
  + indexedMesh 读入路径（:1041-1042 挂接点 + 其下游 point/pointIndex 等读取）。
  Cantilever JSON（291KB 单行串）键面实证：`indexedMesh`+`auxData{channels[],data[{input,values}]}`。
  其余几何种类**不移植**，TODO+参考锚登记（E3）。

**打包与消费层（core/frontend）**：
- `common/internal/render/AuxChannelTable.ts`（372 行，全读）：
  - AuxChannel/AuxDisplacementChannel/AuxParamChannel（:30-88——props 构造/toJSON）。
  - AuxChannelTable（:118-189）：data/width/height/numVertices/numBytesPerVertex/三族数组；
    fromJSON(:147-171——三族全空→undefined)；fromChannels(:186-188)→builder。
  - **AuxChannelTableBuilder**（:198-359，核心缺口）：buildAuxChannelTable——
    numBytesPerVertex=ΣcomputeNumBytesPerVertex（Vector 6B×nEntries、Normal/Distance/Scalar
    2B×nEntries，:361-371）；nRgbaPerVertex 对齐与**2 余字节隔顶点跨 texel 中缝**怪癖
    （:214-223——computeDimensions((numVertices+1)/2, numBytesPerVertex/2, 0, maxDimension)）；
    addNormals（OctEncodedNormal.encode 逐顶点 :253-275）/addParams（Range1d 全域+
    Quantization.computeScale/quantize，qOrigin=low、qScale=invert(scale) :285-316）/
    addDisplacements（QParams3d.fromRange+QPoint3d.init，qScale 逐分量 invert :318-358）。
  - computeDimensions 复用 VertexTable.ts（同目录）。
- `common/internal/render/GeometryAccumulator.ts`：_analysisDisplacement 成员（:22，ctor 从
  options.analysisStyleDisplacement :43）；addPolyface 前段（:101-115——按 channelName 找
  channel→computeDisplacementRange(scale)→逐顶点扩 range 六面）。
- `common/internal/render/MeshBuilder.ts`（:181-190）：visitor.auxData 在场→**无去重**分支
  （addVertex 直调+addAuxChannels(channels, sourceIndex)），否则 addVertex 去重路。
- `common/internal/render/MeshPrimitives.ts`（:184-220）：auxChannels getter +
  addAuxChannels 全量——**通道失配 bail 怪癖**（已存在则逐 channel 比 dataType/name/
  inputName，不合即 return 静默丢弃；首建=空值骨架拷贝）；copyValues 逐 data 追加。
- `common/render/GraphicAssembler.ts`（:90）：analysisStyleDisplacement 源头=
  `this.analysisStyle?.displacement`（GraphicBuilder 构造链——代理追 GraphicAssembler
  analysisStyle getter 来源）。
- `internal/render/webgl/VertexLUT.ts`（:21-110）：**AuxChannelLUT**——texture+numVertices+
  numBytesPerVertex+**Map\<string, channel\> 三族**（initChannels 泛型 :36-50）+bytesUsed+
  hasScalarAnimation+dispose+create（TextureHandle.createForData）；VertexLUT.auxChannels
  字段（:80）+hasAnimation/hasScalarAnimation getter+bytesUsed 叠加；
  `MeshData.ts:98`——VertexLUT.createFromVertexTable(vertices, **auxChannels** 第二参）。
- `internal/render/webgl/TechniqueFlags.ts`：isAnimated 位（:59 字段/:93 ctor 拷贝/:132
  reset/:145 setAnimated/:169 equals/:186 哈希组合/:225 createFromMesh 置位链——代理全读
  落实置位条件）。
- `internal/render/webgl/glsl/Animation.ts`（305 行，全读）：
  - GLSL 七段（initialize/computeAnimLUTCoords/sampleAnimVec2/computeAnimationFrame
    Displacement/computeAnimationDisplacement/adjustRawPosition/computeAnimationFrame
    Normal/computeAnimationNormal/computeAnimationFrameParam/computeAnimationParam）。
  - addAnimation(vert, isSurface)（:187-305）：u_animLUT（TextureUnit.AuxChannelLUT 采样）/
    u_animLUTParams（width/height/numBytesPerVertex/2）必装；位移三 uniform
    （u_animDispParams/u_qAnimDispScale/u_qAnimDispOrigin——getDisplacementChannel 缺席则
    全 0）；**isSurface 半**：octDecodeNormal+法线两 uniform+u_animScalarParams+
    u_animScalarQParams（:287-303——rangeScale 0→1 防除零+ThematicGradientSettings.margin/
    contentRange 归一映射，复用 M-S 渐变机制）。
  - computeAnimParams（:171-184——inputValue=fraction×末 input 线性插值索引对+fraction；
    **注意 :178 `params[2] = inputValue - inputs[i] / (inputs[i+1] - inputs[i])` 运算符
    优先级怪癖原样**——参考疑似 bug 照抄锁定）。
  - getDisplacementChannel/getNormalChannel/getScalarChannel（:145-169——target.
    analysisStyle?.displacement/.normalChannelName/.thematic 三路查 Map）。
- `glsl/Surface.ts`：:272 `if (animated) addAnimation(vert, true)` 门（createCommon 内）；
  **getComputeAnimatedTexCoord**（:466-473——u_animScalarQParams.x>=0 时
  computeAnimationParam 覆写 texcoord→喂 M-S thematic 渐变）；createCommon/addNormal/
  addTexture 的 isAnimated 形参链（:726/:750/:760）。
- `glsl/Edge.ts`（:274-277）：`if (isAnimated) addAnimation(vert, false)` 门——**TD-28④
  清偿点**（DanQing 现恒调）。

**DTA 演示（test-apps/display-test-app）**：
- `AnalysisStyleExample.ts`（299 行，全读）：
  - populateAnalysisStyles（:26-64）：逐 scalar channel 产 style（scalar{channelName,range,
    thematicSettings}——"Height" 结尾者 SteppedWithDelimiter+Custom seaMountain 五色键
    :24；配套 Vector channel 在场→displacement{channelName,scale} 并入名）；"None"→
    undefined 首位。
  - createCantilever（:66-77）：IModelJson.Reader.parse(cantileverJsonString)→
    tryTransformInPlace(scale 30)。
  - createFlatMeshWithWaves（:85-177）：PolyfaceBuilder 100×100 quad 网格+6 通道
    （Static×3[单帧]/Animated Radial×3[三帧 0,1,2]/Linear×3[十帧]）；auxData indices=
    polyface.data.pointIndex。
  - AnalysisDecorator（:187-240）：transientIds.getNext 拾取 id+onDisposed+
    **addOnAnalysisStyleChangedListener**（样式变→disposeGraphic 重建）+decorate
    （GraphicType.Scene+darkSlateBlue+addPolyface）+[Symbol.dispose] 自清理。
  - openAnalysisStyleExample（:242-299）：双 mesh 装载+mesh/style 双 select（DOM→
    Qt combo EQUIVALENCE E2）+Iso 旋转+zoomToVolume(projectExtents)+SolidFill+
    双色天空盒。
  - 入口：Surface.ts:143-154 appToolBar "Analysis Style Example" 钮（openBlankConnection
    extents(0,0,-30,100,100,20)）——**主会话集成挂点**（DtaToolBars 禁动，归主会话）。
- `Cantilever.ts`（291KB 单行 JSON 串资产）。

### 0.2 B 道两装饰演示——参考侧全读

- `PathDecorationTest.ts`（109 行，全读）：
  - PathCanvasDecoration（:10-18——CanvasDecoration 文本 "Path Decoration Test Tool
    Active" @ (256,256) 14px sans-serif 白字居中）。
  - PathDecorationTest（:22-72）：_path+_pickId（transientIds.getNext 惰性）+
    **useCachedDecorations=true**+decorate（WorldDecoration builder+addPath+
    addCanvasDecoration）+testDecorationHit/getDecorationGeometry(→undefined)+
    start/stop/toggle 静态三件套。
  - PathDecorationTestTool（:74-82）：toolId="TogglePathDecoration"，run 取
    selectedView.projectExtents toggle。
  - _getPath（:84-109）：Range1d(0,0.2)×1000 迭代——每代一线段串 [a,b,c,d]+一 Arc3d
    （create(d, d→e, d→f, AngleSweep 0→90°)）——dqGeom 原件全在位。
- `AspectRatioSkewDecorator.ts`（93 行，全读）：
  - 装饰器（:12-66）：projectExtents 三极点 bcurve Path（IModelJson JSON 构造 :23-40——
    **E1 直构 BSplineCurve 等价**）+**ViewDetails.maxSkew=1000 置位**（:43）+decorate
    （isSpatialView 门+createGraphic{type:WorldDecoration,**applyAspectRatioSkew**}+白 3px）。
  - ToggleAspectRatioSkewDecoratorTool（:73-93）：toolId="ToggleAspectRatioSkewDecorator"；
    parseArgs `a` bool（缺省 true）；run 取 selectedView toggle。
  - 注册面：App.ts:452 工具注册表（B 道同名注册到 DanQing 工具面）。

### 0.3 DanQing 侧现状实证（已读码核实）

**已在位**：
- dqCommon：AnalysisStyle 数据层（M-U U-b——scalar/displacement/normalChannelName/thematic
  全字段+equals+toJSON）；DisplayStyle.OnAnalysisStyleChanged 事件（DisplayStyle.h:60，
  :183 raise 先于赋值）。
- dqApp：TargetImpl analysisFraction/analysisStyle 存管（M-U U-e）；DisplayStyle 监听族
  InvalidateRenderPlan 十一补齐（M-U U-g）；canvas decoration 面（DecorateContext/
  Sprites——MeasureLabel 先例）；ParseArgs（M-L(3)）。
- dqRender：AuxChannelTable.{h,cpp} 骨架（三族 vector 存+createLutTexture——**缺
  fromChannels 打包器与 Map 查找**）；AnimationShaders.h 位移半 addAnimation（**缺
  isSurface 半与门控**）；ShaderBindings wireAnim* 桩；RenderFlags.h TextureUnit 族。
- dqGeom：Arc3d/AngleSweep/Path/Range1d/BSplineCurve 全在位（B 道依赖零缺口）。

**缺口**：
- dqGeom：PolyfaceAuxData 三件套+PolyfaceData.auxData 字段（IndexedPolyface.h 无）；
  IModelJson 读取面零存在。
- dqRender：GeometryAccumulator.h:13/43 DEFERRED 注记+MeshBuilder.cpp:128 DEFERRED 分支
  （auxData 两断点）；MeshPrimitives.h:14 auxChannels 注记（载体缺）；TechniqueFlags.h
  无 isAnimated 位；Edge addAnimation 恒调（TD-28④）。
- dqCommon/dqApp skew 缝：ViewDetails.maxSkew 缺（ViewDetails.h 无）、ViewState.cpp:1103
  `aspectRatioSkew=1.0` 硬编码（=ViewState.ts:871 移植缺口）、GraphicBuilderOptions
  无 applyAspectRatioSkew、GraphicBuilder pixelSize skew 消费缺（GraphicBuilder.ts:177-182）。
- 宿主：AnalysisStyleExample/PathDecorationTest/AspectRatioSkewDecorator 三件全缺；
  appToolBar "Analysis Style Example" 钮缺（主会话集成件）。

## 1. 任务分解

### 主会话预备件（dispatch 前落主干，worktree 可见）

**W-p0 CMake 锚点**：dqGeom/dqCommon/dqRender/dqApp 四 CMakeLists 测试列表区尾打
`# A-lane` 锚注释；samples/DisplayTestApp/CMakeLists.txt 的 src 列表区与 tests 列表区
各打 `# A-lane`/`# B-lane` 双锚。

**W-p1 AspectRatioSkew 引擎缝**（小件，RED→GREEN，主会话自落）：
- ViewDetails.maxSkew=25 static+clamp 方法（ViewDetails.ts:95-101 区 1:1）。
- ViewState::getAspectRatioSkew（ViewState.ts:971）+ ViewState.cpp:1103 硬编码归位
  （+:909 对应消费点核查归位）。
- GraphicBuilderOptions.applyAspectRatioSkew 字段（GraphicBuilder.ts:81/114 双形态）+
  pixelSize skew 消费（GraphicBuilder.ts:170-183——skew>1→pixelSize/=skew）。
- 锁（dqCommonTest/dqAppTest）：clamp 上下界/getAspectRatioSkew 值/skew>1 容差归约生效。

### A 道（Analysis Style 大件，六件逐件 RED→GREEN+提交）

**W-a1 dqGeom AuxData.ts 全量 1:1**：AuxChannelDataType/AuxChannelData/AuxChannel/
PolyfaceAuxData+transformPoints+PolyfaceData.auxData 字段挂接+**Polyface 变换联动核移**
（参考变换链核实后照机制）。锁=PolyfaceAuxData.test.ts 移植（dqGeomTest）。

**W-a2 IModelJson Reader 子集**（dqGeom）：indexedMesh 读入+parsePolyfaceAuxData+
SerializationHelpers 零基转换；仅 Cantilever 消费面，余 TODO+锚（E3）。锁=Cantilever
JSON 截段读取断言（顶点/索引/三族通道计数与输入序列）。

**W-a3 AuxChannelTable.fromChannels 打包器**（dqRender）：AuxChannelTableBuilder 全量
（:198-371——三族量化/对齐怪癖/computeDimensions 复用）。锁=打包字节级钉值+
MeshBuilder.test.ts 的 auxData 例移植适配。

**W-a4 消费链接线**（dqRender）：GeometryAccumulator _analysisDisplacement 段+
MeshBuilder 无去重分支+MeshPrimitives.addAuxChannels（失配 bail 怪癖原样）+
AuxChannelLUT（Map 化+纹理+hasScalarAnimation——VertexLutTexture/MeshData 落点代理定）
+MeshData createFromVertexTable auxChannels 二参。锁=装配链锁（polyface+auxData→
MeshBuilder→auxChannels 载体计数/字节对账）。

**W-a5 glsl/Animation.ts 全量归位**（dqRender）：isSurface 半补齐+Surface :272 门+
getComputeAnimatedTexCoord+Edge :274-277 门（**TD-28④ 清偿**）+TechniqueFlags.
isAnimated 位全链（含 equals/哈希/置位源）+addAnimation 门控从恒调归位。锁=变体键锁
（isAnimated 维入键）+shader 源码锁（位移/法线/参数三段出场）+TD-28④ 行勾销。

**W-a6 宿主演示**：AnalysisStyleExample 宿主件（Qt combo 双件 E2+AnalysisDecorator 全链
+onAnalysisStyleChanged 监听消费）+Cantilever 资产入库（E5）+openAnalysisStyleExample
等价入口函数（主会话挂 appToolBar）。**E2E 像素锁**（Authored §5(g)，主会话收口跑）：
flat-waves mesh+位移通道 fraction 泵→顶点位移像素变化双向断言+scalar 通道 thematic
渐变着色 WHERE 断言。

### B 道（装饰演示两小件，逐件 RED→GREEN+提交）

**W-b1 PathDecorationTest**（109 行 1:1）：装饰器三件套+canvas 文本（E7）+
TogglePathDecoration 工具注册+_getPath 几何。锁=路径几何钉值（1000 迭代线弧交替计数
+首末弧参数）+toggle 状态机+canvas 文本存活（W 像素锁主会话跑）。

**W-b2 AspectRatioSkewDecorator**（93 行 1:1）：装饰器（W-p1 缝消费）+Toggle 工具
（parseArgs `a`）+maxSkew=1000 置位+**E1 直构 BSplineCurve**。锁=skew>1 细分加密断言
（弦差顶点数对比）+toggle 状态机+E1 几何对账（三极点/阶/结向量逐位）。

## 2. EQUIVALENCE 预登记（§11.10 制度）

| # | 边界 | 处置 |
|---|---|---|
| E1 | AspectRatioSkew bcurve Path 构造：参考经 IModelJson JSON（AspectRatioSkewDecorator.ts:23-40） | 直构 BSplineCurve（同极点/阶/结向量）；发散=零（同参数构造路径）；验证=W-b2 几何对账锁 |
| E2 | AnalysisStyleExample UI：DOM select×2 → Qt combo；入口 appToolBar 钮→主会话挂点 | 交互语义 1:1（切换 dispose 重建+样式置位）；发散=控件形态；验证=宿主 wiring 锁 |
| E3 | IModelJson Reader 仅移植 indexedMesh+auxData 子集 | 余几何种类 TODO+IModelJsonSchema.ts 锚；发散=其它 JSON 种类不可解析（Cantilever 唯一消费面）；验证=W-a2 截段锁 |
| E4 | AnalysisDecorator 拾取 id=transientIds.getNext | DanQing blank connection transientIds 面代理核实；缺则拾取 id 面登记（装饰不拾取=功能等价的偏差源——testDecorationHit 语义保） |
| E5 | Cantilever 291KB JSON 资产入库位 | samples/DisplayTestApp 资产目录（非 third_party——DTA 演示资产非引擎第三方）；loadEmbedded 等价 |
| E6 | AnalysisStyle scalar→thematic 渐变衔接 | getComputeAnimatedTexCoord 复用 M-S u_thematicIndex 通道；发散=无（同一机制）；验证=E2E scalar 着色锁 |
| E7 | PathDecorationTest canvas 文本 | DanQing canvas decoration 面（Sprites/MeasureTool 先例）；发散=文本栅格化引擎差异；验证=文本存活锁（位置宽松带） |

## 3. 双代理编排契约落实（用户 2026-10-14 契约原文逐条）

1. **文件集不相交**：A 道=dqGeom/dqRender/dqCommon/dqApp+引擎测试+宿主示例新文件
   （AnalysisStyleExample/Cantilever）；B 道=samples/DisplayTestApp/src/Gui 新件
   （PathDecorationTest/AspectRatioSkewDecorator）+其测试新文件。
2. **CMake 锚点**：W-p0 已落；A 道只写 `# A-lane` 锚区，B 道只写 `# B-lane` 锚区。
3. **禁动清单**（两道同禁）：DtaToolBars.cpp、CLAUDE.md、docs/ 台账与计划文档、
   对方道文件、.gitignore、third_party。
4. **worktree 初始化**（代理自做自验）：third_party\\qt Junction+cmake configure。
5. **代理测试面**：只跑无真窗无 dump 锁（dqGeomTest/dqCommonTest/dqAppTest/面板状态机）；
   真窗像素锁/dump E2E/全量 ctest 归主会话合并后串行。
6. **代理纪律**：§11.8 全读参考（锚清单随 prompt）、§5 RED→GREEN 双向证据、
   EQUIVALENCE §11.10 登记、逐件提交、报告=文件清单+测试摘要+EQUIVALENCE 表+遗留。
7. **主会话集成**：merge→接线（appToolBar Analysis Style 钮+keyin 注册）→全量门禁
   （TD-26/29 规程）→CLAUDE.md §1/§15+统计台账+本文实录回填→收口提交。
8. **流水线**：一道完成即派同道下一件（M-X 备料：Contour 勘察）。

## 4. 门禁

- A/B worktree：各自目标面全绿（dqGeomTest/dqRenderTest/dqCommonTest/dqAppTest/
  面板族），RED→GREEN 证据随报告。
- 主会话收口：全量 ctest（Debug）串行；TD-26/29 族失败按 §14 规程隔离复跑 ×2 归因。
- E2E 像素锁（W-a6/W-b1 真窗件）主会话跑绿为收口门。

## 5. 完成实录（回填区）

（收口时回填：提交笔数/钉值/门禁数/偏离登记。）

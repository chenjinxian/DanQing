# 阶段 4 M-S：Thematic Display 主题显示（display-test-app ThematicDisplayEditor + 引擎着色链）

**立项**：用户 2026-10-08 选定（DTA 对照矩阵大件 14 余量中按 M-O 总纲列表序的下一件
——`docs/DTA功能对照与专业化分析-2026-09-30.md` §2.4 行 "Thematic Display｜
❌[引擎]（ThematicUniforms 底层在、着色/解析未通）｜大件"；M-P=Sectioning、
M-Q=Rendering Style 已清偿）。

**工作流**：勘察（本文 §1-§3，三路并行代理 + 承重锚点主会话逐一亲验）→ 逐件
RED→GREEN + 回归 + 逐件提交 → 收口（全量门禁 + CLAUDE.md §1 增 M-S 段 + §15 基线 +
完成实录）。纪律：§0 来源铁律 + §11.8 开工前实读 + §5 RED 先行 + §6 五维对照。

---

## 1. 参考实证账（锚）

> 参考检出 `D:/Github/itwinjs-core`（现 88da8fb9，7e57d018 后代；thematic 路径
> diff 为空在案——M-M(1) 登记同域）。

### 1.1 数据层（core/common）

- **`ThematicDisplay.ts`**（549 行全读）：
  - 枚举：`ThematicGradientMode`（:20-29，Smooth=0/Stepped=1/SteppedWithDelimiter=2
    [仅 Height]/IsoLines=3[仅 Height]）；`ThematicGradientTransparencyMode`
    （:39-47，SurfaceOnly=0/MultiplySurfaceAndGradient=1）；
    `ThematicGradientColorScheme`（:55-70，BlueRed/RedBlue/Monochrome/Topographic/
    SeaMountain/Custom）；`ThematicDisplayMode`（:411-426，Height=0/
    InverseDistanceWeightedSensors=1/Slope=2/HillShade=3）。
  - `ThematicGradientSettings`（:96-279）：默认 mode=Smooth/stepCount=10（<2 钳 2，
    :203-205）/marginColor=黑/colorScheme=BlueRed（越界回退 :209-211）/customKeys
    空（Custom 且 <2 键回退内置白→黑 :216-221）/colorMix=0（仅 terrain/pointcloud
    生效）/transparencyMode=SurfaceOnly；静态常量 margin=0.001/contentRange/
    contentMax（:119-121）；**toJSON 省略默认值**（:232-257）。
  - `ThematicDisplaySensorSettings`（:349-405）：sensors（position+value，value
    clamp [0,1] :308-311）+ distanceCutoff（默认 0=全局）；toJSON 恒写两字段。
  - `ThematicDisplay`（:459-548）：字段 displayMode/gradientSettings/**range:
    Range1d（可空——默认 null range）**/axis/sunDirection（**默认均 {0,0,0}**）/
    sensorSettings；**构造校验**（:514-527）：displayMode 越界→Height；非 Height
    模式 IsoLines/SteppedWithDelimiter **强制降级 Smooth**；Slope range 钳 [0,90]；
    `equals`（:479-494）逐字段（range/axis/sunDirection 用 isAlmostEqual）；
    toJSON（:534-547）恒写 displayMode/gradientSettings/axis/sunDirection/range，
    sensorSettings 仅非空写。
- **`Gradient.ts`**：`Gradient.Symb.createThematic`（:140-158——固定 scheme 展键 /
  Custom 用 customKeys[<2 回退红→绿+assert]）；`getThematicImageForRenderer`
  （:307-359——**宽 1 × 高 N 的 RGBA8 列向纹理**：Smooth→N=maxDimension（
  ThematicUniforms._getGradientDimension=min(8192,maxTextureSize)，:209-213），
  Stepped 族→N=stepCount[分隔线/isoline 全在 shader 画，纹理只是纯色 step 带]；
  采样 f=1-j/dimension[Smooth]/1-j/(dimension-1)[Stepped 两端精确命中]；
  mapColor :254-289[clamp/Invert/相邻键线性插值]）；`produceImage` 的 Thematic
  分支（:482-499，含 margin 行——供 AnalysisStyle，非本链）。
- **`DisplayStyleSettings.ts`**：`thematic?: ThematicDisplayProps`（:162-163）；
  `DisplayStyle3dSettings._thematic`（:1085，ctor fromJSON :1102）；setter
  （:1219-1228——equals 短路→**先 Raise onThematicChanged**（基类事件 :514-515）
  →替换→同步 `_json3d.thematic`）；applyOverrides（:1210-1211）；toOverrides
  （:1152-1184——非 iModelSpecific 剥 sensorSettings 与 Height 的 range）。
- **`SolarCalculate.ts`**：`calculateSolarDirectionFromAngles({azimuth,elevation})`
  （:191——DTA 面板 HillShade 默认太阳向所需；DanQing 未移植）。

### 1.2 引擎链（core/frontend webgl）

- **RenderPlan 填充**：`createRenderPlanFromViewport`（internal/render/RenderPlan.
  ts:127）——`thematic = (style.is3d() && view.displayStyle.viewFlags.
  thematicDisplay) ? style.settings.thematic : undefined`；打包 :170；
  createEmptyRenderPlan（:77-97）不含。
- **Target**：`wantThematicDisplay` 是**计算 getter**（Target.ts:398-400——
  `currentViewFlags.thematicDisplay && is3d && undefined !== uniforms.thematic.
  thematicDisplay`——非存储字段）；`wantThematicSensors`（:406-409——
  wantThematicDisplay && mode==IDW && sensors 非空）；`changeRenderPlan(plan)`
  在 changeFrustum 后、updateRenderPlan 前调 **`uniforms.thematic.update(this)`**
  （:537）；`TargetUniforms.thematic` 成员（TargetUniforms.ts:133）。
- **`ThematicUniforms.update(target)`**（ThematicUniforms.ts:90-152，全文亲读）：
  - 快路径（:93-106）：thematicDisplay 在 && plan.thematic 在 && equals && _texture
    在 → 只刷新传感器 viewMatrix + **Slope 轴随 viewMatrix 逐帧重算**（:97-99）/
    **HillShade 太阳向逐帧重算**（:100-102，negate+normalize）→ desync → return。
  - 主路径：desync→`_thematicDisplay = plan.thematic`（**可置 undefined——关闭
    即清态** :110-113）→ dispose 纹理 → range（Slope 度→弧度 :115-121）→ colorMix
    → axis（`_updateAxis` :73-79——**恒 normalize；仅 Slope 传 viewMatrix** :125）
    → sunDirection（仅 HillShade，`_updateSunDirection` :81-88——viewMatrix 变换
    + **negate** + normalize）→ marginColor → displayMode → fragSettings
    （gradientMode/distanceCutoff/min(stepCount,gradientDimension)/transparency
    标志 :134-140）→ 无 cutoff 且 wantThematicSensors → 全局共享
    ThematicSensors 纹理（:142-147）→ **`Gradient.Symb.createThematic` +
    `getThematicImageForRenderer` + `TextureHandle.createForImageBuffer(…,
    RenderTexture.Type.ThematicGradient)`**（:149-151）。
  - bind 面全部 `sync(this, uniform)` 去重（:154-192）；bindTexture/bindSensors
    assert 纹理在。
- **渐变纹理 GL 形态**：`RenderTexture.Type.ThematicGradient` → wrap=ClampToEdge +
  **无 mipmap + MIN/MAG 皆 NEAREST**（Texture.ts:293-309/389-390 + 
  loadTexture2DImageData :80-81）+ 豁免 2 幂断言（:528）；传感器纹理 RGBA32F。
- **纹理单元**：普通 surface 渐变纹理 = **TextureUnit.SurfaceTexture（单元 2，
  glsl/Surface.ts:600——s_texture 被 thematic 接管**；createSurfaceBuilder
  :760 `addTexture(…, isThematic, …)` + :821 thematic 时 ComputeBaseColor=
  `return getSurfaceColor()`[跳过贴图采样，基色只留 alpha 供相乘]）；
  RealityMesh=单元 11；传感器=**TextureUnit.ThematicSensors=单元 7（与
  ShadowMap 复用，RenderFlags.ts:171-173——thematic 与 shadows 互斥的根因）**。
- **逐 draw 决策**：DrawCommand.ts:206-223——`thematic = geometry.
  supportsThematicDisplay && target.wantThematicDisplay`；thematic 时
  shadowable 强制 No（:207）；点云+Slope/HillShade 强制关（:215-216）。
- **能力位**：`CachedGeometry.supportsThematicDisplay` 默认 false
  （CachedGeometry.ts:120）；**SurfaceGeometry = `!isGlyph`（SurfaceGeometry.
  ts:135-137）**；RealityMesh=true；PointCloud=true；InstancedGeometry 透传
  （:376）；边线/多段线不支持。
- **变体**：SurfaceTechnique `_kThematic=32`（Technique.ts:289）+ 变体循环
  （:318-349，**Thematic+Shadowable 组合跳过** :329-330）+ computeShaderIndex
  （:382-426）。
- **shader 绑定**：`addThematicDisplay`（glsl/Thematic.ts:214-336，全文亲读）——
  uniforms 全为 **GraphicUniform**（u_modelToWorld→branch.
  bindModelToWorldTransform / u_thematicRange·Axis·SunDirection·DisplayMode·
  Settings·marginColor→uniforms.thematic.bind* / u_numSensors·s_sensorSampler→
  全局或逐 batch 分流 :289-313 / u_thematicColorMix 非 terrain 恒 0）；**唯
  `u_discardBetweenIsolines` 是 ProgramUniform**（:320-327——readPixels 中置 1，
  isoline 间片元 discard 影响拾取）；`addEyeSpace(builder)`（v_eyeSpace 供 IDW
  循环）；**顶点位经 `getComputeThematicIndex`（:172-191）——Height 用
  `u_modelToWorld * rawPosition`（解码后模型空间位！）**，HillShade 顶点态两形
  （computeSurfaceNormal().z / g_hillshadeIndex[terrain]）；Slope 与 HillShade
  的片元分支在 `slopeAndHillShadeShader`（acos(|dot(normal,axis)|) 度外钳 -1=
  margin 色 / max(0,dot(g_normal,sunDir))）；IDW 片元循环（prelude——8192 硬
  上限、1/dist² 加权、v_eyeSpace 距、cutoff 逐传感器再判）。
- **pass 路由**：SurfaceGeometry.getPass（:195-277）——**IsoLines 强制
  translucent**（:208-211）；transparencyMode=MultiplySurfaceAndGradient 时按
  gradientSettings.textureTransparency 分 Opaque/Translucent/Mixed（:249-262）。
- **readPixels**：Target.ts:888-898——仅 IsoLines 保留 thematic（拾取态）。
- **DisplayStyleState**：`registerSettingsEventListeners`（:1004-1017——
  onOverridesApplied 监听：**thematic 且 Height 且无 range → 以 iModel.
  projectExtents.zLow/zHigh 补齐后重赋值**——DTA "Thematic: Height" 预设不写
  range 赖此）；`overrideTerrainDisplay`（:1020-1030——IsoLines→wantSkirts=
  false；Slope/HillShade→wantNormals=true，地形面）。
- **Viewport**：`settings.onThematicChanged.addListener(displayStyleChanged)`
  （Viewport.ts:1403）→ invalidateRenderPlan+ChangeFlags.displayStyle（:1282-1285）。

### 1.3 DTA UI（ThematicDisplayEditor）

- `ViewAttributes.ts`：ctor :327 `addThematicDisplay()`（序：Environment→
  BackgroundMapOrTerrain→EdgeDisplay→AO→**Thematic**，:320-328）；
  ThematicDisplayEditor 实例化 :484-487 + `_updates` 注册（onViewChanged 刷新）。
- `ThematicDisplay.ts`（:37-777）：`defaultSettings`（:21-35——Height/Smooth/
  marginColor=blanchedAlmond/BlueRed/axis(0,0,1)/range[0,1]/sunDirection=
  calculateSolarDirectionFromAngles(315°,45°)/sensors=[]）；可见性门=**is3d**
  （:167，非 3d 整区隐藏）+ checkbox 反映 `viewFlags.thematicDisplay`（:168）
  勾选才展开子控件；**写入闭环 = toJSON→改字段→fromJSON→
  `getDisplayStyle3d().settings.thematic = …`（immutable-replace，:760-764）→
  `vp.synchWithView()`（:774-776）**；首次勾选侧效（:179-207——range=extents.z、
  distanceCutoff=xLength/25、铺 4 默认传感器）；Display Mode 切换侧效（Slope 自动
  range=[0,90]，离开回 project extents :235-241）；Gradient Mode 条目随
  displayMode 变（Height 4 项 :126-131，其余 2 项 :132-135）；Color Scheme
  5+3 伪项（Custom opaque/transparent/mixed→customKeys 表 :278-285）；控件
  13 组（checkbox/mode/gradientMode/stepCount[2..65536]/scheme/ Multiply alpha/
  range High·Low/axis XYZ[-1,1]/colorMix 滑条/sunDirection XYZ/distanceCutoff/
  sensor 编辑器[选择+X/Y/Z/Value+增删+32×32 网格]/Reset :669-674）；
  回读 updateThematicDisplayUI（:712-758，range null 时 project extents 兜底）。
- **Rendering Style 预设**（M-Q 已落数据面）：#11 "Thematic: Height"
  （ViewAttributes.ts:190-197——vf.thematicDisplay=true + axis(0,0,1) +
  SteppedWithDelimiter，**不写 range**）；#12 "Thematic: Slope"（:198-215——
  Slope+range[0,90]+axis(0,0,1)+Custom 双色键 0x404040→0xffffff）。
- 参考测试锚：`core/common/src/test/ThematicDisplay.test.ts`（JSON 派生+默认值+
  错误值单例）、`ThematicGradientSettings.test.ts`（compares/textureTransparency）、
  `Gradient.test.ts` thematic 段（:268-330——width=1 约束/margin 色/尺寸不约束）。

## 2. DanQing 现状断点清单（勘察 + 主会话亲验）

**已在（完成度高的面）**：dqCommon ThematicDisplay 类型族/GradientSymb.
createThematic+getThematicImageForRenderer（Gradient.cpp:211-293 1:1 列向纹理）/
DisplayStyle3dSettings.m_thematic+applyOverrides3d（DisplayStyleSettings.cpp:153）；
dqRender ThematicShaders.h GLSL 片段齐全/ThematicDisplayShaders.h 注册器/
TechniqueFlags.isThematic 变体维/MultiVariantTechnique 互斥/DrawCommand.
cpp:53-55 消费判定/SurfaceVariantCompiler.cpp:374-378 编译接线/InstancedGeometry
透传；dqApp Viewport::overrideDisplayStyle thematic 分支失效（Viewport.cpp:2761-
2766）+ OnThematicChanged 监听（:678-682）；宿主 RenderingStyles 两预设数据面
（RenderingStyles.cpp:416-454）。

**断点（亲验）**：

| # | 断点 | 锚 | 后果 |
|---|---|---|---|
| G1 | `ThematicUniforms::update()` 零调用 | SceneCompositorImpl.cpp:1570-1575 只 bind 不 update | 渐变纹理永不建、bind 恒 no-op |
| G2 | `wantThematicDisplay` 是存储字段且 setter 零调用（参考=计算 getter） | TargetImpl.h:333-337,533 vs Target.ts:398-400 | isThematic 永假 → 全链死 |
| G3 | `RenderPlan` 无 thematic 字段 | RenderPlan.h:35-139 vs RenderPlan.ts:127 | 数据无法到 Target |
| G4 | `changeRenderPlan` 不接 thematic、不调 uniforms.thematic.update | OpenGLRenderTarget.cpp:112-160 vs Target.ts:537 | 断链 |
| G5 | `SurfaceGeometry` 无 supportsThematicDisplay 覆写 | CachedGeometry.h:214-215 默认 false；仅 RealityMeshGeometry.h:58=true | 普通表面永不 thematic |
| G6 | 传感器 GPU 上传缺（CPU float vector 止步）；BatchUniforms::bindThematicSensors TODO | ThematicSensors.h:55-121；Uniforms.h:180-184 | IDW 模式无源 |
| G7 | `DisplayStyle` 无 setThematic 门面（OnThematicChanged 无人 Raise） | DisplayStyle.h:56 事件在、setter 缺 | 面板写入无事件链 |
| G8 | `dqRender::ThematicDisplay` 孤儿类（枚举值序与 dqCommon 不同！仅 P2P3FeaturesTest 用） | dqRender/src/render/ThematicDisplay.{h,cpp} | 歧义源，须清除 |
| G9 | `ThematicDisplay::equals` 漏 axis/sunDirection/sensorSettings | ThematicDisplay.h:299-303 | 快路径漏检轴/太阳向变更 |
| G10 | UI 置灰占位 | ViewSettingsPanel.cpp:724-732 | 无交互入口 |
| G11 | ValidateRenderPlan 不填 thematic | Viewport.cpp:2206-2266 | 断链首环 |
| G12 | addThematicDisplay 顶点用 `a_position`（LUT 几何=24-bit 量化索引，非模型位） | ThematicDisplayShaders.h:56-68 vs glsl/Thematic.ts:173-183 用 rawPosition | Height 着色必错（LUT 主路径） |
| G13 | uniforms 全 nullptr 绑定（无喂数方）+ `u_discardBetweenIsolines` 无 CPU 源 | ThematicDisplayShaders.h:36-49 vs glsl/Thematic.ts:233-327 全 GraphicUniform/ProgramUniform 绑定 | 编译过但值恒默认 |
| G14 | **JSON 形状发散**：props 用 rangeMin/rangeMax 双 double（0/1 默认，无可空语义）vs 参考 `range?: Range1dProps{low,high}`（可空——null 触发 projectExtents 补齐链） | ThematicDisplay.h:79-80,260-261 vs ThematicDisplay.ts:442 | dump round-trip 不兼容 + Height 无 range 语义不可表达 |
| G15 | **构造校验缺**：非 Height 降级 IsoLines/SteppedWithDelimiter→Smooth、Slope 钳 [0,90]、displayMode 越界回 Height 全缺 | ThematicDisplay.h:272-284 vs ThematicDisplay.ts:514-527 | 非法组合直达 shader |
| G16 | **默认值发散**：axis/sunDirection 默认 (0,0,1) vs 参考 (0,0,0) | ThematicDisplay.h:262-263 vs :471-473 | 未设值场景行为不同 |
| G17 | **update 语义发散**：签名 (thematic,driver,viewMatrix) 非 (target)；无 clear-when-undefined；快路径不刷新 Slope 轴/HillShade 太阳向（相机转动脉冻结）；axis 不 normalize 且凡给 viewMatrix 恒变换（参考仅 Slope）；sunDirection 不 negate 不 normalize（参考 negate+normalize 且仅 HillShade） | ThematicUniforms.h:92-159 vs ThematicUniforms.ts:90-152 | 方向性着色错误 |
| G18 | **渐变纹理走错路**：produceImage(width=8192,height=1) 行向 vs 参考 getThematicImageForRenderer **1×N 列向**（shader 采样 vec2(0.0,ndx)——行向纹理 u=0 恒取首 texel=恒色）；过滤参数未按 ThematicGradient 型（NEAREST/ClampToEdge/无 mipmap） | ThematicUniforms.h:231-264 vs Gradient.cpp:258-293（正确面已在！）+ Texture.ts:293-309 | 上色必错（恒首色） |
| G19 | `calculateSolarDirectionFromAngles` 未移植（面板 HillShade 默认太阳向所需） | SolarCalculate.ts:191 | UI 默认缺 |
| G20 | SceneCompositorImpl.cpp:1572 渐变纹理绑单元 **1**（参考=SurfaceTexture 单元 2 的 s_texture 接管语义）+ `u_thematicEnabled` 是 DanQing 自创 uniform（参考无此位——shader 经变体维选择） | SceneCompositorImpl.cpp:1568-1575 vs glsl/Surface.ts:600,760,821 | 单元冲突/自创面 |

## 3. 范围裁决

- **in**：dqCommon 数据层归位（G9/G14/G15/G16+gradient/sensor 校验核查+
  SolarCalculate）；RenderPlan 载体（G3/G11）；Target 中枢（G1/G2/G4/G17/G18/G20
  ——TargetUniforms.thematic 归位+update(target) 重写+changeRenderPlan 接线+
  getter 化+纹理归位）；shader 真绑定+rawPos 修正+能力位+pass 路由（G5/G12/G13）
  + **Height/Slope/HillShade E2E 像素锁首绿**；传感器 GPU 链（G6+IDW 像素锁）；
  DisplayStyle 门面+projectExtents 补齐（G7+DisplayStyleState.ts:1008-1015）；
  UI 编辑器 13 控件（G10/G19）；孤儿类清理（G8）。
- **out/登记**：
  - 点云 thematic（Slope/HillShade 关、isoline 固定容差）——DanQing 无点云瓦格式
    （采集面未立）；点云分支代码随移植落、验证面登记。
  - RealityMesh/地形 thematic（colorMix 生效面、g_hillshadeIndex 顶点态、
    overrideTerrainDisplay 的 wantNormals/wantSkirts）——地形瓦未移植，登记。
  - `toOverrides` 剥 sensorSettings/Height range（:1152-1184）——toOverrides 面
    未移植（M-Q 已登记），本件不扩。
  - TextureUnit 7 与 ShadowMap 复用——DanQing 阴影 pass 未移植，无冲突实害，
    登记（阴影落地时须维持互斥）。
  - 逐 batch 传感器纹理（distanceCutoff>0，Graphic.ts:111-120 PerTargetBatchData
    缓存）——随 S-e 一并，Batch 侧结构适配按参考落。
  - `dqRender::ThematicDisplay` 孤儿类的 P2P3FeaturesTest 用例——随删除退役
    （其断言面=渐变 LUT 构建，正确面已在 GradientSymb 并有 GradientThematicTest
    锁定，不丢覆盖）。

## 4. 分件

### S-a dqCommon 数据层归位

- **落点**：`dqCommon/PublicAPI/dqCommon/ThematicDisplay.h`（+对应 .cpp 若有）——
  ①props 形状：`rangeMin/rangeMax` → `range`（`std::optional<Range1dProps>`——
  dqGeom Range1d 已在 Range3d.h:690；Range1dProps 载体按既有 props 惯例增
  {low,high} 结构或直接 Range1d）；②类字段 rangeMin/rangeMax →
  `dqGeom::Range1d range`（可空语义=Range1d null）；③默认 axis/sunDirection
  (0,0,1)→(0,0,0)；④构造校验三件套（fromJSON 尾：displayMode 越界→Height；
  非 Height 的 IsoLines/SteppedWithDelimiter→Smooth[经 props round-trip 重建，
  参考 :519-523 同款]；Slope range 钳 [0,90]）；⑤equals 补 range（isAlmostEqual）
  /axis/sunDirection/sensorSettings（:479-494）；⑥toJSON 形状（恒写五段、
  sensorSettings 条件写）；⑦ThematicGradientSettings 校验核查（stepCount<2 钳/
  越界回退/Custom<2 键回退——勘察称已在，逐条对账 :199-221）；⑧sensor value
  clamp [0,1] 核查（:308-311）；⑨`calculateSolarDirectionFromAngles` 移植
  （dqCommon，SolarCalculate.ts:191 全文——含 azimuth/elevation→方向向量数学）。
- **影响面排查**：rangeMin/rangeMax 的既有消费方（ThematicUniforms.h:104-109/
  RenderingStyles.cpp 两预设/DisplayStyleSettingsTest:152-173/GradientThematicTest
  ——全部随改）。
- **RED 锁**：移植 `core/common/src/test/ThematicDisplay.test.ts`（JSON 派生+默认
  +错误值）+ `ThematicGradientSettings.test.ts` 两例（compares/textureTransparency）
  + `Gradient.test.ts` thematic 段四例（:268-330——width=1/margin 色两向/尺寸
  不约束）；自查锁：equals 全字段差异检出（axis/sun/sensors 各翻一→不等）。
- **开工前实读**：ThematicDisplay.ts:76-548 全文 + Range1d（Range3d.h:690-）面 +
  参考三测试文件全文。

### S-b RenderPlan 载体

- **落点**：`dqRender/PublicAPI/dqRender/RenderPlan.h`（增
  `std::optional<dqCommon::ThematicDisplay> thematic` + equals 段——optional 语义
  对齐参考 `thematic?`）+ `dqApp/src/Viewport.cpp` ValidateRenderPlan（:2243-2247
  hline 段后插——RenderPlan.ts:127 条件 `is3d && vf.thematicDisplay`）。
- **RED 锁**：plan 变化检测（thematic 缺席→在场 / 在场→缺席 / equals 内字段翻 →
  `newPlan != m_currentRenderPlan` 触发 changeRenderPlan——既有锁模式随行）。
- **开工前实读**：RenderPlan.ts:103-170 + RenderPlan.h equals 现构。

### S-c Target 中枢

- **落点**：
  ①`Uniforms.h` TargetUniforms 增 `thematic` 成员（TargetUniforms.ts:133；
  头注 :505-507 的 deferred 登记勾销 thematic 项）；
  ②`ThematicUniforms.h` **update 重写为 update(TargetImpl&)**（
  ThematicUniforms.ts:90-152 逐行：快路径[Slope 轴/HillShade 太阳向随
  frustum.viewMatrix 逐帧刷新——viewMatrix 源=target.getUniforms().frustum]+
  clear 路径[plan.thematic 缺席→_thematicDisplay 清+纹理 dispose]+axis 恒
  normalize+仅 Slope 变换+sunDirection 仅 HillShade 且 negate+normalize+
  desync/sync 键[Sync.h 已在]）；签名从 (thematic,driver,viewMatrix) 改
  (TargetImpl&)——driver 经 target 取；
  ③`createGradientTexture` 归位 getThematicImageForRenderer（1×N 列向——
  Gradient.cpp:258-293 已在）+ NEAREST/ClampToEdge/无 mipmap（TextureHandle
  面按 M-P clip 纹理先例[setTextureFilters 虚接口]；2 幂豁免核实）；
  ④`TargetImpl`：m_wantThematicDisplay 存储字段删→**wantThematicDisplay()
  计算 getter**（Target.ts:398-400——currentViewFlags.thematicDisplay && is3d &&
  uniforms.thematic.getThematicDisplay()!=null；currentViewFlags 源=branch 栈底
  或 plan——查证 DanQing 等价面）+ wantThematicSensors（:406-409）；
  ⑤`OpenGLRenderTarget::changeRenderPlan` 在 changeFrustum 后、updateRenderPlan
  前插 `uniforms.thematic.update(*m_impl)`（Target.ts:537 序）；
  ⑥SceneCompositorImpl.cpp:1567-1575 死绑段删（u_thematicEnabled 自创面+
  单元 1——绑定归 shader 注册面）+ m_thematicUniforms 成员移除（转调
  target uniforms）。
- **RED 锁**：update 值断言（range 度→弧度[Slope]/axis normalize/太阳向 negate/
  fragSettings 四槽/stepCount 钳）+ 快路径（设置不变+纹理在→不重建；viewMatrix
  变→Slope 轴刷新）+ clear 路径（plan.thematic 缺席→getThematicDisplay()=null）+
  渐变纹理字节对账（getThematicImageForRenderer 输出=纹理上传字节+1×N 形态）。
- **开工前实读**：ThematicUniforms.ts:27-213 全文 + Target.ts:390-410/495-544 +
  TextureHandle.h create2D/过滤面 + Sync.h。

### S-d shader 绑定 + 能力位 + pass + E2E 首绿

- **落点**：
  ①`ThematicDisplayShaders.h` addThematicDisplay 真绑定（glsl/Thematic.ts:214-336
  ——全 GraphicUniform lambda 经 `params.getTarget()->getUniforms().thematic.
  bind*`；u_modelToWorld→branch 的 modelToWorld 面[查证 DanQing BranchUniforms
  等价物，无则按 BranchUniforms.ts 补]；u_discardBetweenIsolines=ProgramUniform
  [isReadPixelsInProgress]；u_numSensors/s_sensorSampler 分流暂落全局臂、逐
  batch 臂随 S-e）；**顶点索引用 rawPos**（SurfaceCommon.h:133-140 的
  AdjustRawPosition 槽产出——G12 修正：a_position 对 LUT 几何是 24-bit 索引）；
  ②`SurfaceGeometry::supportsThematicDisplay` 覆写 = !isGlyph（SurfaceGeometry.
  ts:135-137；isGlyph 在 DanQing 恒 false[注释在案]——覆写仍按参考形落）；
  ③getPass 归位（SurfaceGeometry.ts:208-211 IsoLines→translucent；:249-262
  MultiplySurfaceAndGradient 按 textureTransparency 分 pass——查证 DanQing
  getPass 现状后 1:1）；
  ④readPixels IsoLines 保留（Target.ts:888-898——查证 DanQing pick 变体选择面）。
- **RED 锁**：invokeGraphicUniformForTest（ShaderProgramImpl.cpp:457 测试面已在）
  逐 uniform 值断言 + SurfaceCompileTest 变体扩 + **E2E 像素锁首绿**：
  - `ThematicHeightGradientColors`——真实 dump（joeshouse：墙面高度变化）或
    专用资产；应用 thematic Height（range=内容 z 域）→ WHERE 断言：低域片元
    偏 BlueRed scheme 低端色、高域偏高端色（判据=高度两分带的 hue 域分离）+
    toggle off（vf.thematicDisplay=false）恢复原色；
  - `ThematicSlopeDistinguishesFlatFromVertical`——joeshouse 屋面/地面 vs 墙面
    的 slope 色分离（0° vs 90° 两端色键）。
- **开工前实读**：glsl/Thematic.ts:44-336 全文 + Surface.ts:600/725-825 +
  DanQing SurfaceVariantCompiler.cpp:374-378 调用上下文 + getPass 现状 +
  pick 变体面。

### S-e 传感器 GPU 链（IDW 模式）

- **落点**：`ThematicSensors.{h,cpp}` GPU 上传（createForData——1×N RGBA32F
  浮点纹理，M-P clip 纹理的 RGBA32F+NEAREST 面复用）+ ThematicUniforms 全局
  共享传感器纹理（update 内 wantGlobalSensorTexture 臂——参考 :142-147）+
  bindSensors（单元 7）+ 逐 batch 面（distanceCutoff>0：Batch.
  getThematicSensors 的 PerTargetBatchData 缓存 Graphic.ts:111-120 +
  BatchUniforms.setSensors 消费面接通 Uniforms.h:157-184 TODO）+
  addThematicDisplay 的 u_numSensors/s_sensorSampler 逐 batch 臂。
- **RED 锁**：传感器纹理字节对账（eye-space 变换+打包）+ **IDW 像素锁**（合成
  场景：双传感器异 value → 片元色随近传感器值渐变；cutoff 外=margin 色）。
- **开工前实读**：ThematicSensors.ts 全文 + Graphic.ts:100-130/300-330 +
  BatchUniforms.ts:55-110 + DanQing Batch/Graphic 对应结构。

### S-f DisplayStyle 门面 + projectExtents 补齐

- **落点**：`dqApp/PublicAPI/dqApp/DisplayStyle.h`——`setThematic(
  ThematicDisplay const&)` 门面（equals 短路→Raise OnThematicChanged→经
  settings.setThematic 替换；参考 setter 序 DisplayStyleSettings.ts:1219-1228——
  **先 Raise 后替换**的序按参考落）+ **applyOverrides thematic 的 Height 无
  range → projectExtents.z 补齐**（DisplayStyleState.ts:1008-1015——DanQing 落点：
  dqApp DisplayStyle 的 applyOverrides 消费侧，iModel projectExtents 经既有
  连接面取——查证 DumpIModelConnection projectExtents 在案性）。
- **RED 锁**：门面事件恰一次 + 无效赋值短路 + **Height 无 range 补齐锁**
  （applyOverrides 携 thematic{Height,无 range} → 落 settings 的 range=
  projectExtents.zLow/zHigh——M-Q "Thematic: Height" 预设的真实语义面）。
- **开工前实读**：DisplayStyleState.ts:1004-1030 + DisplayStyleSettings.ts:1210-
  1228 + DanQing DisplayStyle applyOverrides 链。

### S-g UI 编辑器（ViewSettingsPanel Thematic 分区）

- **落点**：`ViewSettingsPanel.cpp`（:724-732 置灰占位删→真分区；分区序按
  ViewAttributes.ts:320-328——Environment 后、面板尾部）——**ThematicDisplayEditor
  :37-777 全量 1:1**：defaultSettings（:21-35）+ is3d 门（DanQing 面板即 3d
  场景，门按参考落注记）+ checkbox（vf.thematicDisplay 读写 + 首次勾选三侧效
  [:179-207——range=extents.z/cutoff=xLength/25/4 默认传感器]）+ 子控件显隐门
  + Display Mode 4 项（切换侧效 :235-241）+ Gradient Mode（条目随模式重建
  :726-730）+ stepCount 数字框 + Color Scheme 5+3（customColorSchemes 表
  :278-285）+ Multiply alpha + range High/Low + axis XYZ + colorMix 滑条
  （terrain/pointcloud 专属——DanQing 无该两面，控件按参考落、登记消费面无）
  + sunDirection XYZ + distanceCutoff + sensor 编辑器（选择/XYZ/Value/增删/
  32×32 网格 :615-664）+ Reset；写入闭环=toJSON→改→fromJSON→
  **DisplayStyle::setThematic**（S-f 门面）→synchWithView；回读
  updateThematicDisplayUI（:712-758——onViewChanged 刷新链已在面板更新面）。
- **RED 锁**：wiring（控件→settings 变化：checkbox 翻转 vf 位/mode 切换落
  displayMode+range 侧效/Reset 回默认）+ 回读锁（外部改 settings→控件反映）。
- **开工前实读**：ThematicDisplay.ts:37-777 全文 + ViewSettingsPanel 现构 +
  M-O(1) Monochrome/M-O(4) 编辑器两件的宿主模式先例。

### S-h 孤儿类清理 + 收口

- **落点**：`dqRender/src/render/ThematicDisplay.{h,cpp}` 整删（dqRender::
  ThematicDisplay 孤儿类——枚举值序与 dqCommon 不一致的歧义源）+
  P2P3FeaturesTest.cpp 对应用例退役（渐变 LUT 断言面由 GradientThematicTest
  承接）+ CMake 行清理 + 全仓 grep 残留零命中。
- **收口**：全量 ctest（TD-26/TD-29 规程）+ CLAUDE.md §1 增 M-S 段 + §15 基线
  （大件 14→13）+ 本文完成实录回填 + 台账/M-Q E2 登记勾销（AO/Thematic 视觉面
  之 Thematic 半——M-Q 计划 E2 行与 RenderingStyles.cpp:12 注释同步更新）。

## 5. EQUIVALENCE / 裁决预登记

| # | 面 | 预期发散 | 验证法 |
|---|---|---|---|
| E1 | 点云 thematic 分支 | DanQing 无点云瓦格式 | 代码面登记；分支随参考落、测试面缺标 Authored |
| E2 | RealityMesh/地形（colorMix 生效/g_hillshadeIndex/overrideTerrainDisplay） | 地形瓦未移植 | 登记；colorMix uniform 绑定按参考形落（非 terrain 恒 0） |
| E3 | TextureUnit 7 复用（ShadowMap/ThematicSensors） | DanQing 无阴影 pass，无实害 | 登记；阴影落地时维持互斥门 |
| E4 | 传感器逐 batch 缓存的 DanQing 结构适配 | PerTargetBatchData 的 C++ 承载 | S-e 取证明白登记 |
| E5 | u_thematicColorMix 非 terrain 恒 0 绑定 | 无发散（参考同形 :278-284） | uniform 值断言 |
| E6 | readPixels 仅 IsoLines 保留 thematic | DanQing pick 变体选择面待查证 | S-d 取证后登记 |
| E7 | Range1dProps 的 C++ props 承载形状 | {low,high} 结构 vs TS 数组——dump JSON 消费面以 {low,high} 对象形为准（Range1d.toJSON 输出形） | round-trip 锁 |

## 6. 门禁与收口

- 每件：目标面回归（新增锁 + 相邻：dqCommonTest[ThematicDisplay/Gradient/
  DisplayStyleSettings]/dqRenderTest[ThematicUniforms/SurfaceCompile/
  FeatureOverrideLutWebGl/BranchUniformsUpdate]/dqAppTest/DisplayTestAppTest/
  DtaTest 像素族）+ 增量提交。
- S-c/S-d 触渲染核心：既有像素回归族抽跑（TileTreeRender/RpcDumpRender/
  DumpOpenChain/JoesHouse×3/PickDumpScene）。
- 收口：全量 ctest（TD-26/TD-29 规程——失败即隔离复跑 ×2 记录）+ CLAUDE.md
  §1 增 M-S 段 + §15 基线 + 本文档完成实录。

---

## 完成实录（收口时回填）

### S-e 完成实录（2026-10-09，传感器 GPU 链 + 三修复 + 一纠错）

**主线**：传感器 GPU 链全通——ThematicSensors 视空间打包（1×N RGBA32F
列向浮点纹理，NEAREST/ClampToEdge，单元 7[ThematicSensors 与 ShadowMap 复用
——阴影未移植无冲突]）+ 逐帧 update 惰性门（viewMatrix isAlmostEqual）+
全局臂（ThematicUniforms 持）/逐 batch 臂（Batch::getThematicSensors——
accumulateSensorsInRange 按 batch.range×localToWorld 过滤 + settings 指针
同一性缓存失效，Graphic.ts:111-120/BatchUniforms._setCurrentBatch :74-82 语义）
双臂分流绑定 + u_numSensors/s_sensorSampler shader 绑定分流（glsl/
Thematic.ts:293-318）。IDW E2E 像素锁 SensorIdwColorSeparatesNearEachSensor
（joeshouse 双传感器异值——红族质心 (702,572)[A 端值 1]/蓝族 (1407,919)
[B 端值 0]、蓝族 36804px、质心 2D 分离 786px——IDW 逐片元 1/dist² 加权的
直接证据）。

**纠错（S-d 的"E1 世界帧"EQUIVALENCE 登记撤销）**：S-e 取证连环定音——
DANQING_THM_FRAGDBG=10 直读 v_eyeSpace=视空间坐标场（z 负深度形态）、
mode=1 法线场 roofTop 读回 (0.09,0.83,0.61)≈R_iso·ẑ——DanQing 着色器为
**视空间结构，与参考同帧**（S-d 的"世界帧"判定系误诊，Slope 混帧的当时
根因已不可考[疑为 S-c 前 update 先于 changeFrustum 的时序]，E1 按错误前提
舍去了参考的视变换臂）。归位：axis 仅 Slope 经视矩阵变换（_updateAxis
:73-79 条件臂）、sunDirection HillShade 变换+negate+normalize（:81-88）、
传感器视空间逐帧刷新（ThematicSensors.update 惰性门——S-e 曾短暂拆除此臂
的 fast-path 调用，一并归位）。单测两锁翻钉（AxisTransformedByViewMatrix-
ForSlopeOnly/HillShadeSunDirectionViewTransformedNegatedAndNormalized——
rotX90 手算对拍）+ FastPath 锁订正（Slope 臂 desync 恢复→syncKey 推进）+
SensorsGpu 锁翻钉（视空间打包值对拍+惰性门）。

**修复 1——BranchStack 双栈分裂统一（本里程碑最大的引擎级清偿）**：
参考为 BranchUniforms._stack **单栈**（BranchUniforms.ts:50——命令构建/
绘制派发/changeRenderPlan/拾取全共享）；DanQing 适配面裂为 TargetImpl 与
SceneCompositorImpl 两栈，changeRenderPlan 只喂 target 栈根 → 绘制栈根永持
默认 vf → drawPass 的 isThematic 门恒假（[THM-DRAW] 探针实锤 vf.thematic=0，
且 draws 周无 PushBranch/PushState 命令——栈顶即 drawFrame 根 push 的
defaultFlags）。**S-d 的 E2E 四锁"首绿"实系 MSVC 增量构建陈旧 obj 掩盖下的
假绿**（清净重建后 S-d 树裸跑同红——归档教训再应验：行为未随源码变时先
删 obj 强制重编）。修复：SceneCompositorImpl::m_branchStack 改**引用成员**
（与 m_batchState 同形态，TargetImpl 成员序 branchStack<compositor 安全）
+ drawFrame 根 push 改 pushTransform（vf 继承栈根=plan vf——原为
defaultFlags 覆写）。全部五锁转绿。
**修复 2——WorldDecorations 豁免语义锁定**：参考 Target.ts:236-243/
Graphic.ts:484-491——世界装饰分支强制自带 vf（thematicDisplay=默认 false），
**对 thematic 豁免是参考刻意行为**。TallBox 测试改挂 GraphicType::Scene
（normal 列表并入场景随 plan vf）后渐变扫描上屏。
**修复 3——FeatureOverrides LUT 纹理单元 7→1 归位**（RenderFlags.ts:157
FeatureSymbology=One + FeatureOverrides.ts:449——初提交起误用单元 7，
与 ThematicSensors 碰撞：IDW+激活 override 集同帧时 override 采样将读
传感器纹理；单元 1 在 opaque surface pass 空闲）。

**TD-31 登记+清偿**（全二进制模式跨测试污染——CLAUDE.md §14）：TileAdmin
全局 fetcher 被 dump 测试换为 DumpTileFetcher 不恢复（app 会话刻意语义），
文件 tileset 测试后置命中 NotFound 全黑——MinimalSolidBox 重挂
FileTileFetcher（消费侧隔离；TileTreeRender 的注册序侥幸同族风险登记）。

**探针**：DANQING_THM_FRAGDBG 扩 7-10（getSensor(0/1) 位置/双值通道/
v_eyeSpace 帧直读——§13.1 登记）；[THM] 传感器纹理 GPU 回读对拍探针
（CLIPDUMP 先例，§13.1 登记）；TD-30 S-e 复读实锤（压平仍在）。

**门禁（S-e 收口轮）**：dqRenderTest 749/749 + dqAppTest 445/445 +
DisplayTestAppTest 139/139 + ThematicDisplayE2E 5/5（含污染邻接对拍：
JoesHouseHover→E2E 全绿）+ DumpOpenChain.OpensBaytownOrthographicSavedView
隔离复跑 ×2 绿（TD-29 族规程）；全量 ctest 见收口提交注。

### S-f 完成实录（2026-10-09——DisplayStyle 门面 + projectExtents 补齐）

①**DisplayStyle::setThematic 门面**（dqApp DisplayStyle.h——
DisplayStyleSettings.ts:1221-1227 setter 1:1：equals 短路 → 先 raise 后赋值
[监听器事件内读旧值，锁内实钉] → 赋值）：**OnThematicChanged ported-but-
uncalled 清偿**（事件与 Viewport 监听面[Viewport.cpp:678=SetDisplayStyle+
RequestRedraw]早已在、本件前无 raise 方）；EQUIVALENCE=事件无载荷（参考携
新值形参，监听面不消费）。②**projectExtents 补齐**（DisplayStyleState.ts
:1004-1016 的 onOverridesApplied 监听语义——DanQing 无该事件面，
applyOverrides3d 唯一调用点在 Viewport::overrideDisplayStyle，同域落地）：
overrides 携 thematic 且应用后模式==Height 且 props 无 range →
iModel.projectExtents.z 填充 + 门面 setter 重写；DisplayStyle::getIModel
访问面新增。锁 2（Authored——参考无对应测试面）：ThematicFacadeSetter-
Semantics（短路/恰一次/读旧值/新值就位/同值短路）+ ThematicHeightWithout-
RangeFillsFromProjectExtents（填 [-100,100]+事件 / 显式保持 / Slope 不填）。
门禁：dqAppTest 447/447（+2）+ E2E 5/5 回归绿。提交 43736642c3。

### S-g 完成实录（2026-10-09——ViewSettingsPanel Thematic 编辑器全量）

新建 Gui/ThematicDisplayEditor.{h,cpp}（ThematicDisplay.ts:37-777 的
ThematicDisplayEditor 1:1，参考独立文件划分）：**13 控件组全量**（首启
副作用复选 / Display Mode 四条目带 range 副作用 / Gradient Mode 随模式重建
/ Step Count / Color Scheme 5+3 伪项[键值表原样] / Multiply alpha / Range
High-Low / Axis XYZ / colorMix 滑条 / SunDir XYZ / DistanceCutoff / 传感器
编辑[选择+XYZ/Value+Add/Delete/Grid 32×32] / Reset）+ 写回环单环（toJSON→
修改→fromJSON→S-f 门面→synchWithView）+ UI 回读（range null→extents 默认
域显示）。宿主：ViewSettingsPanel 末节（置灰清单摘 Thematic 项——留
Background Map/AO）。锁 5（Authored——DTA 无编辑器测试面）：
EnableFirstTimeSideEffects / DisplayModeSlopeRangeAndGradientEntries /
ColorSchemeCustomPseudoEntries / SensorGridAddDeleteAndEdit /
ResetAndFacadeEventAndUiReadback。EQUIVALENCE：activeViewport 逐次取（面板
共享件）/传感器选择态属主=QComboBox/sensorSettings 缺段防御补壳（参考非空
断言恒真域外零分叉）。门禁：编辑器 5/5 + 面板族子集 19/19 +
DisplayTestAppTest 139/139。提交 579db322fe。

### S-h 完成实录（2026-10-09——孤儿类清理 + M-S 收口）

**孤儿 dqRender::ThematicDisplay 删除**（dqRender/src/render/
ThematicDisplay.{h,cpp} 整删 + dqRender/CMakeLists 注册行 + P2P3FeaturesTest
的三测试块[DefaultState/Configure/BuildGradientLut]与 include 清除）：
该类系早期自创面——枚举序相悖（Slope=1/HillShade=2/IDW=3 vs 参考
Height=0/IDW=1/Slope=2/HillShade=3）、出处注释虚假（"Ported from
FeatureSymbology.test.ts"——该文件无 thematic 面）、buildGradientLutData
系自创 API；真测试面已由 S-a~S-e 五层覆盖（dqCommon ThematicDisplayTest +
ThematicUniformsTest + ThematicDisplayShaderTest + ThematicSensorsGpuTest +
E2E 五锁）。另：M-Q E2 登记勾销 Thematic 半（M-Q 计划文档 E2 行注记——
AO/Atmosphere 两半维持）。



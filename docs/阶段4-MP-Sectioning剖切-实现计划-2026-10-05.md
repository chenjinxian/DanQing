# 阶段 4 M-P：Sectioning 剖切——实现计划（2026-10-05）

**Architecture:** M-P = 大件首立项（用户 2026-10-01 已选；M-O(1)~(4) 已清 DTA 对照矩阵接线级 12 + 移植级 9，➖9 架构外结案，删除侧无余量）。本计划先落**勘察实证账**（参考侧剖切全链 vs 仓库 2026-10-05 现状），再分件 P-A~P-G 逐件清偿（RED→GREEN + 回归 + 逐件提交），收口 = 全量门禁零失败 + CLAUDE.md §1/§15 更新。

**Spec:** CLAUDE.md §15（M-O 指令延续）+ `docs/DTA功能对照与专业化分析-2026-09-30.md` §1.5/§2（Sectioning 行）+ M-O(1) 核销账 M-P 立项行。

---

## 0. 勘察修正（先于一切）

参考检出现状（88da8fb9，7e57d018 后代）与立项登记的两处**锚名偏差**：

1. **core 侧无独立 `SectionTools.ts`/`ViewClipDecoration.ts`**——已合并进 **`core/frontend/src/tools/ClipViewTool.ts`（2073 行）**：全部 ViewClip 工具类 + `ViewClipDecoration` + `ViewClipDecorationProvider` 都在此文件。
2. 对照矩阵锚 `SectionTools.ts:33-239` 实为 **test-app 的 UI 面板文件** `test-apps/display-test-app/src/frontend/SectionTools.ts`（239 行，类 SectionsPanel）。
3. 几何侧文件名偏差：`ClipShape` 定义在 **ClipPrimitive.ts** 内（无 ClipShape.ts）；`ClipUtilities` 现名 **ClipUtils.ts**。

矩阵行 20（Sectioning ❌ 置灰占位）与 §1.5 描述本身不受影响——五种 clip = **Plane / Range / Element / Shape / Geometry**（面板下拉项），范围照旧。

## 1. 参考侧全链（实证锚——逐跳 file:line）

### 1.1 几何层（core/geometry/src/clipping/）

| 文件 | 行数 | 关键内容 |
|---|---|---|
| ClipVector.ts | 518 | `class ClipVector :42`（`clips` :52 = ClipPrimitive[]）；工厂 createEmpty :63 / createCapture :71 / create :79 / fromJSON :103 / clone :86；appendClone :124 / appendReference :128 / **appendShape :132**；classifyPointContainment :342 / classifyRangeContainment :362；transformInPlace :221；extractBoundaryLoops :242 |
| ClipPrimitive.ts | 903 | `enum ClipMaskXYZRangePlanes :37`；`class ClipPrimitive :116`（fetchClipPlanesRef :126 → UnionOfConvexClipPlaneSets；createCapture :143；fromJSON :306）；**`class ClipShape :358`**（transformFromClip/transformToClip :385/:389；zLow/zHigh 谓词 :403-440；setPolygon :467；initSecondaryProps :490；createFrom :535；createShape :549；**createBlock :580**） |
| ClipPlane.ts | 669 | DanQing 已全量移植（dqGeom/ClipPlane.h 248 行） |
| ConvexClipPlaneSet.ts | 827 | DanQing 已移植（dqGeom/ConvexClipPlaneSet.h 139 行 + 弧区间 cpp）；createRange3dPlanes :125 / createConvexPolyface :796 等 |
| ClipUtils.ts | 1189 | DanQing 仅子集（ClipPlaneContainment + selectIntervals01）；**缺 `loopsOfConvexClipPlaneIntersectionWithRange :448`**（装饰轮廓核心）及 ClipStepAction :60 / ClipStatus :73 / Clipper :86 / PolygonClipper :151 / doesClipperIntersectRange :552 等 |
| UnionOfConvexClipPlaneSets.ts | 405 | **DanQing 零移植**；addConvexSet :111 / classifyPointContainment :199 / polygonClip :223 / takeConvexSets :338 |

### 1.2 存储层（clip 归属 ViewState.details，非 DisplayStyle）

- 工具写入：ClipViewTool.ts:146-150 `viewport.view.setViewClip(clip)` + `viewport.setupFromView()`。
- ViewState.ts:999-1008 `setViewClip`→`details.clipVector`；getViewClip 读回。
- **core/common/src/ViewDetails.ts:137-167**：getter 惰性从 JSON `viewDetails.clip` 反序列化（ClipVector.fromJSON :140，空/无效→undefined :143）；setter 先 `onClipVectorChanged.raiseEvent`（:160）再写内存+JSON（:162-166）；事件定义 :76。
- clipStyle（配色/cutGeometry 外观）在 DisplayStyle.settings.clipStyle（core/common ClipStyle.ts——**DanQing dqCommon/ClipStyle.h 已全量移植**）；Viewport.clipStyle getter/setter Viewport.ts:662-679（写后 invalidateRenderPlan）。

### 1.3 引擎链（逐跳）

1. Viewport.ts:1237 `view.details.onClipVectorChanged → invalidateRenderPlan()`（:433）。
2. 帧内 Viewport.ts:2568-2569 `validateRenderPlan() → target.changeRenderPlan(createRenderPlanFromViewport)`。
3. **RenderPlan.ts**：字段 `clip?: ClipVector` :56 + `clipStyle: ClipStyle` :57；createRenderPlanFromViewport 取 :122-123、随返 :165-166。
4. Target.ts:496 changeRenderPlan；**:519 `uniforms.branch.updateViewClip(plan.clip, plan.clipStyle)`**。
5. BranchUniforms.ts:153-155 `updateViewClip → clipStack.setViewClip(clip, style)`；ClipStack 构造 :78-81（transform=视矩阵，`wantViewClip = _viewClipEnabled && top.viewFlags.clipVolume`）。
6. **ClipStack.ts:109-149 setViewClip**：比较/去重 → `renderSystem.createClipVolume(clip)`（:140）→ push；同时更新 inside/outside 色 + intersection 风格。
7. webgl System.ts:809-811 → **ClipVolume.create（ClipVolume.ts:179-212）**：每 ClipPrimitive 的 UnionOfConvexClipPlaneSet（fetchClipPlanesRef :187）编码为纹理行；ClipPlanesBuffer.updateData（:125-158）把平面变到**视坐标**写 (normal.xyz, distance)；集合边界哨兵行 (0,0,0,0) :88-90、union 边界哨兵行 (2,2,2,0) :92-94。
8. 每帧 SceneCompositor.ts:1442（主场景）/ :1536（二次）`target.pushViewClip()` → Target.ts:352 → BranchUniforms :135-141 置 `_viewClipEnabled`（ClipStack.startIndex 从 0 起 :174-177）；嵌套 branch clip 由 pushBranch/pushState `clipStack.push`（:109-110/:119-120）与 DrawCommand.ts:166-187 PushClip/PopClipCommand 压弹。
9. 纹理与 CPU 预剔除：ClipStack.recomputeTexture/uploadTexture :235-262（RGBA float 纹理）；isRangeClipped :192-207（classifyPointContainment，Target.ts:362 消费）。
10. 变体选择：TechniqueFlags.ts:81-92 `init` 取 `clipStack.hasClip ? textureHeight : 0` 为 numClipPlanes；**ClippingProgram.ts:24-53**（createClippingBuilder → addClipping :19；getProgram(numPlanes) :36-38，0 面返回 undefined 用无裁剪版程序）。
11. **片元 discard（glsl/Clipping.ts，addClipping :136-208）**：uniforms u_clipParams(int[3]=[startIndex,endIndex,textureHeight]) :159-177 / s_clipSampler :194-201（TextureUnit.ClipVolume）/ u_insideRgba/u_outsideRgba :142-152 / u_colorizeIntersection :179-183 / u_clipIntersection :185-189；GLSL getClipPlane=texelFetch :17-21、calcClipPlaneDist=dot(vec4(eyePos,1),plane) :37-41、主循环 :43-63（哨兵行 2.0/0.0 识别 set/union 边界）；**postlude :98-114：被全部 plane set 裁掉 → 有 outside 色则染色否则 discard（:106）**；交线颜色化 :65-83（composite 阶段 glsl/Composite.ts:175 消费 intersectionStyle）。
12. 副路：剖切面几何（produceCutGeometry）——PrimaryTileTree.ts:799-813 `getSectionCutFromView` 把 clip 串成 StringifiedClipVector 发**后端**生成切面（treeId.sectionCut）→ **➖9 GenerateTileContent 同族，架构外结案**（见 §3）。

### 1.4 工具层（ClipViewTool.ts 类清单，实测行号）

| 类 | toolId(keyin) | 行 |
|---|---|---|
| ViewClipEventHandler（接口 7 回调） | — | :37 |
| DrawClipOptions | — | :55 |
| ViewClipTool extends PrimitiveTool（基类：enableClipVolume :138 / setViewClip :146 / doClipToPlane :159 / doClipToShape :181 / doClipToRange :187 / doClipClear :196 / getPlaneInwardNormal :131 / getClipRayTransformed :203 / getOffsetValueTransformed :217 / getClipShapePoints :312 / getClipShapeExtents :320 / drawClipShape :294-309 / drawClipPlanesLoops :356-382 / isSingleClipShape :345 / isSingleConvexClipPlaneSet :385 / isSingleClipPlane :396） | 无 | :77 |
| ViewClipClearTool | `ViewClip.Clear` | :445 |
| ViewClipByPlaneTool（单点接受；orientation ToolSettings 枚举 Top/Front/Left/Bottom/Back/Right/View/Face :91-99 默认 Face :487；`_clearExistingPlanes` ctor 参数 :489 叠加多面） | `ViewClip.ByPlane` | :483 |
| ViewClipByShapeTool（逐点 _points；AccuDrawHint :649-672；dynamics 橡皮筋 decorate :717-745 用 getClipPoints :674-709[投影首点平面；2 点无 Ctrl 自动矩形 :692-703]；≥2 点无 Ctrl 落点即闭合 :764-768；Ctrl 加点 :759；Ctrl+Z onUndoPreviousStep :792） | `ViewClip.ByShape` | :564 |
| ViewClipByRangeTool（两角点；getClipRange :840-852 经 AccuDrawHintBuilder.getContextRotation(Top)+逆变换 ACS 对齐；dynamics addRangeBox :855-880；→ doClipToRange :899-900） | `ViewClip.ByRange` | :805 |
| ViewClipByElementTool（onPostInstall 纯持久选集直取 :953-968 否则 initLocateElements；落点 doLocate :1053 → doClipToElements :978[iModel.elements.getPlacements :980；单→元素对齐盒 placement.transform+ClipShape 走 doClipToRange :1036；多→合并 range :993-994；XY 退化→XZ/YZ 四点 shape 回退 :1004-1027]） | `ViewClip.ByElement` | :926 |
| ViewClipModifyTool（abstract extends EditManipulator.HandleTool；getOffsetValue :1101-1120 鼠标点投影手柄射线；动态 updateViewClip(ev,false)+invalidateDecorations :1151-1155；accept →(ev,true) :1157-1162；取消恢复原 clip+clipStyle :1164-1169；进修改临时关 produceCutGeometry :1081-1088） | 内部 | :1061 |
| ViewClipShapeModifyTool（Shift=全部偏移；zLow/zHigh 命名手柄联动 :1190-1191） | 内部 | :1173 |
| ViewClipPlanesModifyTool（选中多面联动 :1257） | 内部 | :1248 |
| ViewClipControlArrow（手柄数据） | — | :1285 |
| ViewClipDecoration extends EditManipulator.HandleProvider（单例 _decorator :1311；get/create/clear/toggle :1966-1996；getClipData :1368-1425[解析 _clipShape 或 _clipPlanes+_clipPlanesLoops 经 loopsOfConvexClipPlaneIntersectionWithRange :1417；>5 点压缩写回 :1375-1385；>12 面或非单凸集只读预览 :1389-1416]；_clipId=transientIds.getNext :1339；decorate :1854-1964[白线+半透明青填充+非贡献面红虚线 :1870-1871；可见/隐藏双 builder]；createControls :1535-1564[clipId 选中门]；createClipShapeControls :1438-1466[每边中点法向箭头+zLow/zHigh]；createClipPlanesControls :1477-1533[loop 质心定位+非贡献面红]；右键 doClipPlaneNegate :1587[cloneNegated]/doClipPlaneClear :1607/doClipPlaneOrientView :1727[setupFromFrustum+synchWithView+animateFrustumChange]/doClipShapeSetZExtents :1800[ClipShape.createFrom+initSecondaryProps]；单面箭头不可见自动迁移 floatingOrigin :1891-1919） | — | :1310 |
| ClipEventType{New,NewPlane,Modify,Clear} | — | :2003 |
| ViewClipDecorationProvider implements ViewClipEventHandler（onActiveClipChanged BeEvent :2014；onActiveClipRightClick :2020[无监听默认 negate :2048-2049]；show/hide/toggleDecoration :2054-2057；单例 create/clear :2059-2072） | — | :2008 |

外围依赖：**EditManipulator.ts（329 行）**——HandleTool extends InputCollector :47（accept/cancel/onComplete :87-111）；HandleProvider :127（订阅 manipulatorToolEvent :137 + selectionSet.onChanged :163；updateDecorationListener :188；onDecorationButtonEvent :246 拖拽分发）；HandleUtils :279（adjustForBackgroundColor :285 / getArrowTransform :300 / getArrowShape :316）。

注册：IModelApp.ts:438-446 `tools.registerModule(clipViewTool, "CoreTools")`（keyin=toolId）。

### 1.5 宿主 UI（test-app SectionTools.ts，239 行）

- 下拉 "Clip type" :33-46：Plane=`ViewClip.ByPlane` / Range=`ViewClip.ByRange` / Element=`ViewClip.ByElement` / Shape=`ViewClip.ByShape` / Geometry=`DtaClipByElementGeometry`。
- 按钮 Define :50-59（`tools.run(toolName, ViewClipDecorationProvider.create())`）/ Edit :60-66（`provider.toggleDecoration(vp)`）/ Clear :67-73（`tools.run("ViewClip.Clear", provider)`）。
- Add Panel :76-93 + Negate Plane 开关 :94-100 → ModelClipTool.applyModelClipping（:111-144：ClipPlane.createNormalAndPoint+ConvexClipPlaneSet.createPlanes+ClipPrimitive/ClipVector.createCapture+AccuDrawHintBuilder.getBoresite+view.details.modelClipGroups=…+invalidateScene）→ **modelClipGroups 独立项，本里程碑外（§3）**。
- TwoPanelDivider :157-239（纯 UI）。挂载：Viewer.ts:391。

## 2. DanQing 现状（2026-10-05 实证）——断点逐跳清单

| # | 跳 | 现状 | 锚 |
|---|---|---|---|
| ① | dqGeom ClipVector/ClipShape/ClipPrimitive/UnionOfConvexClipPlaneSets | **零移植**（全仓 grep 零命中）；ClipPlane/ConvexClipPlaneSet 已全量、ClipUtils 子集 | dqGeom/ClipPlane.h :3、ConvexClipPlaneSet.h :3、ClipUtils.h :4-9（TODO 明示） |
| ② | ViewState clipVector 存储 + 事件 | 事件已声明（ViewState.h:137）**从未 Raise**；无 clipVector 字段；ViewDetails 未移植（ViewState.h:247-248） | ViewState.h:137 |
| ③ | Viewport 失效监听 | **已通**：OnClipVectorChanged→InvalidateRenderPlan（Viewport.cpp:742-745）+ OnClipStyleChanged→SetDisplayStyle+InvalidateRenderPlan（:649-654）——两事件均等 Raise | Viewport.cpp:649,742 |
| ④ | DisplayStyle 包装层 clipStyle | dqCommon 设置层 JSON 完整（DisplayStyleSettings.h:50,115-116,158）；**dqApp 包装层无访问器、OnClipStyleChanged 从未 Raise** | DisplayStyle.h:55 |
| ⑤ | RenderPlan clip 字段 | **无**（RenderPlan.h:33-119 字段清单无 clip/clipStyle；equals 同） | RenderPlan.h:33 |
| ⑥ | changeRenderPlan→ClipVolume | TargetImpl.cpp:671-688 只传播 viewFlags/is3d/hline；setClipVolume 零调用（TargetImpl.h:358-359）；RenderSystem.createClipVolume 恒 nullptr（RenderSystem.h:246-251） | TargetImpl.cpp:671 |
| ⑦ | RenderCommands PushClip | 克隆丢 volume（RenderCommands.cpp:804-806,836-838 make_unique 无参）；场景遍历从不产出 PushClip（:731-733 注释明示需动画系统）；BranchState.m_clipVolume 只做父子继承（BranchStack.cpp:181,193-197；GraphicBranch 无 clipVolume 字段） | RenderCommands.cpp:804 |
| ⑧ | ClipStack/ClipVolume | **骨架级简化**（ClipVolume 本地 ClipPlane struct+bind 空桩"Phase 2+"；ClipStack 平铺拼接无参考语义/inside-outside 色/isRangeClipped/纹理）；挂载 SceneCompositorImpl.h:96,207 但运行期恒空（写点仅 PushClip/PopClip 分支 :2418-2430，零读点） | dqRender/src/render/ClipVolume.h:30-39、ClipStack.h |
| ⑨ | RenderClipVolume | 抽象类无子类（RenderClipVolume.h:24-27 TODO "ClipVector not yet ported"；clipVector() 返裸指针 :40）；唯一引用=BatchVTest is_abstract 编译锁 | RenderClipVolume.h:24 |
| ⑩ | shader 片元裁剪 | **ClippingShaders.h 全量 GLSL 已移植但是孤儿**（186 行，零 include）；ShaderBuilder ApplyClipping 槽忠实预移植（ShaderBuilder.h:255、.cpp:548-549,919-1206）但无 builder set 它；TechniqueFlags.numClipPlanes/ClippingProgram 半接线（TechniqueImpl.h:106-347，variants 空→回退 basic 程序）；TextureUnit::ClipVolume 预留无人绑（gl/RenderFlags.h:133） | ClippingShaders.h:3、ShaderBuilder.h:255 |
| ⑪ | 自创面（无参考对应物） | SurfaceVariantCompiler.cpp:379-392 无条件注入 u_numClipPlanes+u_clipPlanes[6]+gl_ClipDistance（**无喂数恒 false 行为中性**）；RemainingShaderModules.h:55-74 同型 isClipped()；合成侧 u_clipIntersection 在（CompositeShaders.h:136-248）但 rg.g 位恒 0（OitShaders.h:71-72 自认） | SurfaceVariantCompiler.cpp:379 |
| ⑫ | 工具/装饰/UI | 零；Analysis 工具栏 "Sectioning" 置灰占位（DtaToolBars.cpp:503 addDisabled 0xe916；锁 DtaToolBarsTest.cpp:251-259 索引 3 disabled） | DtaToolBars.cpp:503 |
| ⑬ | 已通的邻接面 | viewFlags.clipVolume 位全链在（ViewSettingsPanel.cpp:141,778 复选 + DumpIModelConnectionTest round-trip）；placements 数据面在（M-N(2)：DumpIModelConnection findPlacement + instances60-placements-v1）；dqCommon ClipStyle 全量；dqGeom 平面数学活着（背景地图/视锥用途，与本链无关） | — |

## 3. 范围裁决

**M-P 内（本里程碑）**：
- 五种 clip 定义中的 **Plane / Range / Shape / Element 四种全链**（几何→存储→引擎→shader→工具→装饰→UI）。
- SectionsPanel 核心面（Clip type 下拉 + Define/Edit/Clear）+ Analysis 工具栏 Sectioning 按钮激活。
- clipStyle 引擎消费面（inside/outside 配色 + intersection——GLSL 已在，接 uniform 即可）。

**M-P 外（显式登记）**：
1. **Geometry 第五型（DtaClipByElementGeometry）**：依赖 `iModel.generateElementMeshes`（后端 RPC）+ vhacd-js 凸分解（test-app 私有依赖，非 core）——数据面与第三方库双缺席。面板下拉处置开工裁决（倾向：四项 + 登记说明）。**不引入 vhacd**（§8.3 之外的新第三方库需单独裁决）。
2. **ModelClipGroups / ApplyModelClip / Add Panel+Negate**：矩阵独立行（ModelClipTools.ts:9-41）；需 ViewDetails3d.modelClipGroups + SpatialViewState._modelClips + per-model tile tree volume 链——**转后续里程碑续排**（本计划只保 view clip 单链）。
3. **produceCutGeometry 切面几何**：后端生成瓦内容（PrimaryTileTree.getSectionCutFromView → StringifiedClipVector → 后端）——**➖9 GenerateTileContent 同族，架构外结案沿用**；clipStyle.cutStyle 类型面已移植不动，cutStyle 语义随 produceCutGeometry 一并缺席。
4. **修改手柄的 produceCutGeometry 临时关闭**（:1081-1088）：随 3 缺席——修改工具该门为 no-op 登记。

## 4. 分件清单（P-A → P-G，每件：参考锚 → DanQing 落点 → RED 锁 → 开工前实读）

> 执行纪律：每件开工先**完整读**该件参考源（§11.8）；RED 锁先行（§5，优先参考测试移植；参考无对应测试才 Authored+标注）；WHERE 断言（§11.11）；每件过目标面回归后提交。

### P-A dqGeom 剖切几何三件 + ClipUtils 扩展

- **参考**：ClipPrimitive.ts（903）[ClipMaskXYZRangePlanes/ClipPrimitive/ClipShape 全量] + ClipVector.ts（518）[工厂/append 族/classify 族/transformInPlace/extractBoundaryLoops] + UnionOfConvexClipPlaneSets.ts（405）+ ClipUtils.ts 扩展[loopsOfConvexClipPlaneIntersectionWithRange :448 + ClipStepAction/ClipStatus/Clipper/PolygonClipper/doesClipperIntersectRange]。
- **落点**：`dqGeom/PublicAPI/dqGeom/{ClipPrimitive,ClipVector,UnionOfConvexClipPlaneSets,ClipUtils}.h`（ClipUtils.h 扩展）+ `dqGeom/src/*.cpp` + dqGeom/tests。
- **RED 锁**：core/geometry 剖切测试移植（开工盘点 `core/geometry/src/test/clipping/` 全目录——ClipPlanes.test.ts 已有子集先例；ClipVector/ClipShape 对应 .test.ts 存在性开工核实，无则按 §5(f) Authored 并标注）。
- **开工前实读**：四参考文件全文 + 既有 ClipPlane.h/ConvexClipPlaneSet.h/ClipUtils.h（对齐dqGeom 风格与既有类型：Transform/Range3d/Loop 等）。

### P-B ViewState clipVector 存储 + DisplayStyle clipStyle 半边

- **参考**：ViewDetails.ts:76,137-167（JSON viewDetails.clip 惰性 getter/事件先 Raise 后写）+ ViewState.ts:999-1008（setViewClip/getViewClip）+ Viewport.ts:662-679（clipStyle getter/setter→invalidateRenderPlan）。
- **落点**：dqApp ViewState（**承载面开工实读定夺**：DanQing 无 ViewDetails 类——候选 = ViewState 内 details 段/props JSON 直挂；EQUIVALENCE 登记）；setViewClip/getViewClip + OnClipVectorChanged.Raise；DisplayStyle 包装层 clipStyle 访问器 + OnClipStyleChanged.Raise（Viewport.cpp:649,742 两监听自动接通——③号断点即闭合）。
- **RED 锁**：clipVector JSON round-trip（真 props 载荷）+ 缺席守恒（fromJSON 无 clip → getViewClip 空，不建空 clip）+ setViewClip 事件恰一次 + clipStyle 访问器 round-trip + OnClipStyleChanged 恰一次。
- **开工前实读**：DanQing ViewState.h/.cpp 全量（props/Clone/round-trip 链）+ DisplayStyle 包装层 + 上述参考段。

### P-C 引擎链：RenderPlan → ClipStack/ClipVolume 参考语义

- **参考**：RenderPlan.ts:48-57,103-166 + Target.ts:496,519 + BranchUniforms.ts:78-81,135-155,174-177 + **ClipStack.ts 全量（285）** + **ClipVolume.ts 全量（219，视坐标编码+哨兵行）** + webgl System.ts:809-811 + SceneCompositor.ts:1442,1536（pushViewClip/popViewClip）+ Target.ts:352,362 + DrawCommand.ts:166-187 + TechniqueFlags.ts:81-92 + ClippingProgram.ts:24-53。
- **落点**：dqRender RenderPlan.h 增 clip/clipStyle + equals；Viewport.cpp ValidateRenderPlan 填充（:2163-2233 现点）；TargetImpl changeRenderPlan → BranchStack 传播（DanQing BranchStack=BranchUniforms 对应面，开工实读）；**ClipVolume/ClipStack 重写为参考语义**（现有简化结构拆除；BatchClipTest Authored 测试同步改/删）；RenderClipVolume 具体化（TODO 解除，clipVector() 返 dqGeom::ClipVector——dqRender→dqGeom 合法边 §8.1）；RenderSystem.createClipVolume 实装；RenderCommands 克隆丢 volume 修正（⑦）；compositor pushViewClip/popViewClip（对齐 §10.3 管线既有注释位）。
- **RED 锁**：参考 `core/frontend/src/test/render/webgl/ClipStack.test.ts` + `ClipVolume.test.ts` 移植（哨兵行编码/视坐标变换/栈语义/setViewClip 去重/isRangeClipped——存在性开工核实）+ RenderPlan clip 字段 equals 变更检测锁。
- **开工前实读**：上述参考文件全文 + DanQing TargetImpl/BranchStack/SceneCompositorImpl/DrawCommand/RenderCommands 现状。

### P-D shader 片元裁剪接线

- **参考**：glsl/Clipping.ts addClipping :136-208（uniform 清单/texelFetch/哨兵协议/postlude discard）+ ClippingProgram.ts:24-53（createClippingBuilder/getProgram(numPlanes)，0 面用无裁剪版）+ TechniqueFlags.ts:81-92 + Composite.ts:175（intersection 消费）。
- **落点**：ClippingShaders.h 孤儿→接线（经 ShaderBuilder ApplyClipping 槽——槽体已忠实预移植）；TechniqueImpl VariedTechnique m_clippingPrograms 填充；uniform 喂数点（u_clipParams/s_clipSampler/u_insideRgba/u_outsideRgba/u_colorizeIntersection/u_clipIntersection）+ TextureUnit::ClipVolume 绑定；合成 rg.g 位接线（OitShaders.h:71-72 恒 0 处）；**gl_ClipDistance 顶点注入处置**（⑪：自创面无参考对应——倾向拆除归参考形，开工实读裁决；BatchClipTest.SurfaceVariantCompilerTest.ClipPlanes 同步）。
- **RED 锁**：变体源码锁（numClipPlanes>0 含 s_clipSampler/texelFetch/discard；=0 不含裁剪块）+ uniform 喂数锁 + 片元级裁剪像素锁（离屏 GL：clip plane 后半空间丢弃——WHERE 断言）。
- **开工前实读**：glsl/Clipping.ts 全文 + ClippingProgram.ts + DanQing ShaderBuilder.{h,cpp}/TechniqueImpl.{h,cpp}/SurfaceVariantCompiler.cpp/RemainingShaderModules.h/ClippingShaders.h/CompositeShaders.h/OitShaders.h。

### P-E ViewClip 工具族（引擎侧 dqApp）

- **参考**：ClipViewTool.ts:37-443（基类+两接口：enableClipVolume/setViewClip/doClip 四法/判定三法/draw 两法/getPlaneInwardNormal 等）+ :445-1058（Clear/ByPlane/ByShape/ByRange/ByElement 五工具——交互流见 §1.4）。
- **落点**：`dqApp/PublicAPI/dqApp/ClipViewTool.h` + `dqApp/src/ClipViewTool.cpp`（MeasureTool 先例：引擎侧工具+ToolAdmin.cpp:740-742 注册式，keyin=`ViewClip.ByPlane` 等 1:1）；ViewClipEventHandler 接口随基类。
- **EQUIVALENCE 面（预登记，开工核实）**：①AccuDrawHintBuilder.getContextRotation/getBoresite（DanQing AccuDraw 消费面未移植——M-O(1) 先立登记；orientation 解析缺上下文旋转时的回退形）；②ByElement placements（findPlacement[placements.json，M-N(2) 既有] vs getPlacements RPC——数据面已立）；③transientIds.getNext（DanQing 瞬态 id 面现状）；④ViewClipDecorationProvider 事件面（BeEvent→DqEvent）。
- **RED 锁**：四工具构造面锁（测试缝=静态方法直调，MeasureTool 先例）：doClipToPlane→单 ClipPrimitive 包 ConvexClipPlaneSet[追加面=_clearExistingPlanes 双态]/doClipToShape→ClipShape zLow·zHigh/doClipToRange→createBlock All|XAndY 薄 Z 分支/ByElement 单元素对齐盒+多元素合并+XY 退化四点回退；Clear→clip 清空+事件。
- **开工前实读**：ClipViewTool.ts:1-1058 全文 + DanQing ToolAdmin.h/PrimitiveTool/AccuDraw 现状/DumpIModelConnection.findPlacement + MeasureTool.cpp（工具风格样板）。

### P-F ViewClipDecoration + EditManipulator + 修改工具

- **参考**：EditManipulator.ts 全量（329）+ ClipViewTool.ts:1061-2073（ViewClipModifyTool/ShapeModify/PlanesModify/ControlArrow/ViewClipDecoration 全交互[§1.4 表]/ClipEventType/Provider）。
- **落点**：`dqApp` EditManipulator.{h,cpp} + ClipViewDecoration（并入 ClipViewTool.cpp 或独立文件，开工按文件规模裁决）；ViewManager AddDecorator（CesiumDecorator 先例）；SelectionSet 联动（onChanged 订阅——DanQing 选集面现状开工实读）。
- **EQUIVALENCE 面（预登记）**：右键上下文菜单（onActiveClipRightClick 无监听默认 negate 的宿主菜单面）；animateFrustumChange（DanQing 动画面现状——StandardViewAnimation 在，逐工具实装面核实）；>5 点 compressByChapterError（PolylineOps 面 DanQing 现状）。
- **RED 锁**：装饰轮廓像素锁（白线+半透明青填充，WHERE：剖切面位于帧内具体象限）+ 手柄图元存在性/选中门（createControls 要求 clipId 选中）+ negate 语义（cloneNegated 后内外翻转——像素锁双向）+ zExtents（setZExtents 后 zLow/zHigh 变化断言）+ Provider 事件序（New/NewPlane/Modify/Clear 四态恰一次）。
- **开工前实读**：EditManipulator.ts 全文 + ClipViewTool.ts:1061-2073 全文 + DanQing DecorateContext/IDecorator/SelectionSet/ViewManager 现状。

### P-G 宿主 UI + E2E 像素锁

- **参考**：test-app SectionTools.ts:33-100（下拉+Define/Edit/Clear）+ Viewer.ts:391 挂载 + DTA Analysis 工具栏 Sectioning 项。
- **落点**：Analysis 工具栏 Sectioning 按钮 `addDisabled`→激活（DtaToolBars.cpp:503；DtaToolBarsTest.cpp:251-259 断言同步 disabled→enabled）+ 弹出面板（SectionsPanel——ViewSettingsPanel/Models 面板先例；Geometry 项按 §3 裁决处置并登记）。
- **E2E 像素锁（§11.11 WHERE 断言）**：instances60 dump 打开 → setViewClip（居中平面剖切）→ **半空间内容消失**（左半塌缩/右半保持——不对称位置断言）+ 装饰轮廓上屏（白线像素）+ viewFlags.clipVolume=false 时不裁（开关语义）+ Clear 恢复。工具入口真跑一遍（keyin ViewClip.ByPlane 单点接受路径）。
- **RED 锁**：面板 wiring 锁（Define→tools.run 参数面/Edit→toggleDecoration/Clear）+ 工具栏激活断言更新 + E2E 像素锁。
- **开工前实读**：test-app SectionTools.ts 全文 + DanQing DtaToolBars.cpp/ViewSettingsPanel 面板族 + 既有 dump 打开链测试样板（DumpOpenChain）。

## 5. EQUIVALENCE 预登记总表（开工逐条核实/补验法）

| # | 面 | 预期发散 | 验证法 |
|---|---|---|---|
| E1 | ViewState clipVector 承载 | DanQing 无 ViewDetails 类（承载面实读后定） | JSON round-trip 锁 |
| E2 | AccuDrawHint 上下文旋转 | getContextRotation 未移植→orientation 回退形 | 工具构造面锁（Plane 法向断言） |
| E3 | ByElement placements | placements.json vs getPlacements RPC | 六轴世界域断言（M-N(2) 先例） |
| E4 | transientIds | DanQing 瞬态 id 面现状 | 装饰选中门锁 |
| E5 | 右键菜单宿主 | 无监听默认 negate 的菜单面 | Provider 事件序锁 |
| E6 | animateFrustumChange | orientView 动画面 | 视图参数终态断言 |
| E7 | compressByChapterError | >5 点多边形压缩 | PolylineOps 现状核实 |
| E8 | clipStyle cutStyle | produceCutGeometry ➖（§3.3） | 类型面既有锁 |

## 6. 门禁与收口

- 每件：目标面回归（新增锁 + 相邻既有锁：ClipPlane/ClipPlanes/PolygonOpsClip/DisplayTypes/DisplayStyleSettings/BatchClip 改造族/DtaToolBars 更新族）+ 增量提交。
- P-C/P-D 触渲染核心：既有像素回归族全跑（TileTreeRender/RpcDumpRender/DumpOpenChain/DumpBrowse/JoesHouse×3/PickDumpScene——剖切默认关，零行为变化为基线；BatchClipTest 简化语义改造如实同步）。
- 收口：全量 ctest 零失败（TD-26/TD-29 环境态族按规程：失败即隔离复跑 ×2 归因）+ CLAUDE.md §1 增 M-P 段 + §15 基线更新 + 本文档完成实录。
- 参考检出漂移复勘：开工时 `git -C D:/Github/itwinjs-core log -1` 记录 commit；ClipViewTool/ClipStack/ClipVolume/glsl 路径 `git diff 7e57d018..HEAD` 为空则锚稳定（M-M(1) 先例）。

---

## 完成实录（收口时回填）

（待逐件完成后回填：每件提交哈希 + 锁清单 + 门禁数字 + 裁决落实情况。）

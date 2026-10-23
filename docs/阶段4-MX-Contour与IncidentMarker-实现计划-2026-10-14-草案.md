# 阶段 4 M-X Contour 显示与 IncidentMarker 实现计划（2026-10-21 正式）

> 里程碑：M-X（Contour display 大件 8/16 + IncidentMarker/MarkerSet）。
> 用户指令：按 M-X（Contour+IncidentMarker）开工（2026-10-21 会话）。
> 基线：P-1d/TD-35 收口后主干 HEAD ca4247486f（X-p0 锚点）；基线门禁（2026-10-21 frontier 轮，Debug）2891 项 = 2861 通过 + 28 跳过 + 2 环境态失败（TD-26/29 族归因在案）。权威对照面：
> `docs/DTA功能对照统计-2026-10-07.md`（§2 大件 #5 Contour / #12 IncidentMarker）。
> 本文前身=2026-10-14 草案备料（M-W 双代理运行期间勘察）；2026-10-21 经五代理参考仓渲染层需求勘察精化后正式化。
> 参考检出：D:/Github/itwinjs-core @88da8fb9（只读）。

## 0. 范围勘察结论（参考全读记录——2026-10-14 M-W 期间备料）

### 0.1 A 道 Contour——链图与缺口

**参考链（全读）**：
- `core/common/src/ContourDisplay.ts`（471 行）：ContourGroupProps(:256)/ContourGroup(:278)/
  ContourDisplay(:384——maxContourGroups=5 :397、groups 截断 :410)。
- `core/common/src/DisplayStyleSettings.ts`：contours props 字段(:164)+_contours 成员
  (:1086)+fromJSON(:1103)+toJSON(:1181)+OnContoursChanged 事件(:516)。
- `core/frontend/src/internal/render/webgl/Target.ts`：changeRenderPlan 携 plan.contours
  → `uniforms.branch.changeRenderPlan(vf, is3d, hline, contours)`(:533)+
  `uniforms.contours.update(this)`(:539)+currentContours=currentBranch.contourLine(:269)。
- `Graphic.ts`（webgl）：GraphicBranch.contourLine 字段(:353)+opts.contours 消费(:374)。
- `BatchUniforms.ts`：bindContourLUT(:115)/bindContourLUTWidth(:121)——per-batch LUT
  纹理（featureIndex→contour 组 nibble，4-bit 打包）。
- `glsl/Contours.ts`（151 行，全读）：computeContourNdx（LUT width==0→15 早退、
  4-bit nibble 解包）+applyContours（v_height 世界高度线距/主次线/线型图案 10 族
  :83/showGeometry 丢弃语义 :91-95/透明度提升 :97）+addApplyContours（:102-151——
  u_contourLUT/u_contourLUTWidth/u_modelToWorldC/u_contourDefs[8] 绑定）。
- **门控形态**：Surface.ts:776 `addApplyContours(builder)` **无条件**编译进 surface
  着色器（无 TechniqueFlags 位——运行时 u_contourLUTWidth==0u 早退门 :17-18）。
- DTA：`test-apps/display-test-app/src/frontend/Contours.ts`（454 行）——
  ContoursSettings 面板(:27-433，per-model 组编辑器：线距/主副色宽型/组管理)+
  ContoursPanel ToolBarDropDown(:435-454)=工具栏 item 24。

**DanQing 现状（已读码实证）**：
- **已在位**：dqCommon ContourDisplay.h 数据层（216 行——ContourStyle/Contour/
  ContourGroup/ContourDisplay+Props 全族）；dqRender ContourUniforms.{h,cpp}
  （update(TargetImpl) 在）；**等高线 GLSL 已移植**（FeatureEffectShaderBuilders.h
  :164-220——computeContourNdx/applyContours/u_contourDefs[8]/v_contourNdx）；
  ShaderBuilder ApplyContours 槽位（:256）。
- **缺口**：DisplayStyleSettings contours 载体（DisplayStyleSettings.h:71 TODO 注记
  "contours, solarShadows, planProjections not yet ported"）；RenderPlan.contours；
  GraphicBranch.contourLine；BatchUniforms bindContourLUT/Width（Uniforms.h:182 TODO
  注记）；shader 槽激活链（GLSL 在但谁 set 槽/uniform 绑定链待核）；
  **ContourLines.h 早期自创脚手架**（setInterval/setOrigin/setColor 形态与参考
  ContourGroup/ContourDisplay 不符——M-S S-h 孤儿类同族，评估归位或清退）；
  DTA ContoursPanel；工具栏 item 24 激活（主会话集成件）。
- 验证资产：DTA 演示面=面板手调；E2E 锁=合成场景+等高线像素锁（主会话收口跑）。

### 0.2 B 道 IncidentMarker——DOM→Qt 承载裁决材料

- `test-apps/display-test-app/src/frontend/IncidentMarkerDemo.ts`（262 行，全读）：
  IncidentMarker(:16-77——makeColor 绿→琥珀→红插值 :28-31、onMouseButton 消息框
  :34-41、setImage/imageOffset/imageSize/title/setScaleFactor/label 六面、
  addMarker WorldDecoration 200mm 圆 :64-76)/IncidentClusterMarker(:80-137——
  drawFunc canvas 圆+top10 严重度 title)/IncidentMarkerSet(:140-144)/
  IncidentMarkerDemo(:150-251——500 随机 incidents/六 SVG 图标加载/
  useCachedDecorations/loading 门 invalidateCachedDecorations)+
  ToggleIncidentMarkers 工具(:253-262)。
- `core/frontend/src/Marker.ts`（494 行——**Marker implements CanvasDecoration :71**
  关键形态：Marker 即画布装饰；DOM div 仅是 itwinjs 的画布宿主）+Cluster(:325)+
  MarkerSet(:357，聚类算法纯几何零 DOM)。
- 资产：六 SVG（Warning_sign/Hazard_*——DTA public 树，入库 sample 资产面）。

**裁决选项（M-X 计划呈批时用户裁决）**：
- **A（画布承载，推荐）**：Marker=CanvasDecoration 同构（QPainter 绘入 DanQing
  canvas decoration 层——DecorateContext::AddCanvasDecoration 已在）；title→
  QToolTip（M-O(1) hover tooltip 链已在）；onMouseButton→画布命中路由。
  发散=DOM 事件模型差异；验证=画布像素锁+命中锁。
- B（widget 承载）：每 marker 一 QWidget——偏离 implements CanvasDecoration 语义。
- C 结案。

## 1. 任务分解（2026-10-21 正式化——五代理勘察锚点已并入）

### 主会话预备件（dispatch 前落主干）

**X-p0 CMake 锚点**（已落 ca4247486f）：dqRender src A 锚 / dqApp src B 锚 / dqAppTest B 锚；M-W 存量锚复用（dqCommon tests A / dqRender tests A / DisplayTestApp src+tests A/B / dqAppTest A）。

### A 道（Contour，五件逐件 RED→GREEN+提交）

- **X-a1 DisplayStyleSettings.contours 载体 + dqApp 门面**（dqCommon+dqApp）：
  `core/common/src/DisplayStyleSettings.ts` 的 props.contours 字段(:164)+_contours 成员(:1086)+fromJSON(:1103)+toJSON(:1181)+OnContoursChanged 事件(:516)+setter 触发 onContoursChanged(:1231-1238) 1:1（dqCommon `ContourDisplay.h` 全族已在位——`ContourDisplay.ts:256-471`：maxContourGroups=5 :397/groups 截断 :410/空 subCategories=default group 语义）。dqApp DisplayStyle 门面 + OnContoursChanged→InvalidateRenderPlan 接线（M-U U-g 监听族先例）。锁=dqCommonTest（props 往返/五组截断/默认组语义）+dqAppTest（门面事件恰一次+plan 失效）。
- **X-a2 RenderPlan.contours + Target/Branch 接线**（dqRender）：
  RenderPlan.contours 字段（RenderPlan.ts:62+:128 提取+:171 拷贝，is3d 门对齐 M-S thematic 段先例）；GraphicBranch.contourLine 字段（Graphic.ts:353——GraphicBranch.h:39 现仅注释）+BranchOptions.contours(:165)；BranchStack/BranchState contourLine 透传（BranchState.ts:54/:93）+BranchUniforms.changeRenderPlan 第 4 参（BranchUniforms.ts:149-150）；TargetImpl::changeRenderPlan 调 uniforms.contours.update（Target.ts:533-539——ContourUniforms::update(TargetImpl) 已在位）+currentContours 从 branch 读（Target.ts:269——TargetImpl.h:310-316 归位）。锁=dqRenderTest（plan 提取/branch 透传/changeRenderPlan 调用序）。
- **X-a3 Contours LUT 纹理类 + BatchUniforms 绑定 + shader 槽激活**（dqRender）：
  新 `src/render/Contours.{h,cpp}`=Contours.ts:27-152 全量 1:1（LUT 纹理 TextureUnit.Contours=TEXTURE14 已定义 RenderFlags.h:168-169；buildLookupTable 4-bit/feature 打包 :92-108——每 RGBA byte 装 2 feature 的 group index；bindContourLUT :115/bindContourLUTWidth :121/wantContourLines getter）；BatchUniforms _contours 惰性创建（BatchUniforms.ts:36/:91）+绑定调用点（Uniforms.h:182-189 TODO 勾销）；Graphic.getContours per-target LUT 缓存（Graphic.ts:135-145）。shader 槽激活=Surface 变体构建处**恒调** addApplyContours（Surface.ts:776——无 TechniqueFlags 位，运行时 u_contourLUTWidth==0u 早退门 Contours.ts:17-18；GLSL 已在 FeatureEffectShaderBuilders.h:164-220，槽位 ShaderBuilder.h:256 已注册、组装已在 ShaderBuilder.cpp:550/941/1026/1215——核通 caller 链）。锁=dqRenderTest LUT 打包字节序钉值+变体源码锁（applyContours 调用出场）+Contours.test 参考测试面核查移植。
- **X-a4 ContourLines.h 自创脚手架归位/清退**（S-h 孤儿类先例）：setInterval/setOrigin/setColor 形态与参考 ContourGroup/ContourDisplay 语义错位——评估消费者后清退或归位。
- **X-a5 E2E 像素锁（主会话收口跑；A 道备好合成测试资产与判据建议）**：合成场景高程等高线——线带像素 WHERE 断言/showGeometry=false discard 臂/开关可逆（§5(g)）。

### B 道（IncidentMarker，两件序贯——Marker 移植先于演示；画布承载裁决 A 已落 M-Z）

- **X-b1 Marker/Cluster/MarkerSet 移植**（Marker.ts 494 行——dqApp 引擎面）：
  `core/frontend/src/Marker.ts`：Marker implements CanvasDecoration 同构（:71-318——worldLocation :82/size :84/visible :76/drawFunc :127/drawDecoration 主入口 :195-223[先 drawFunc→drawImage→fillText 序]/setImage :229-241/imageOffset·imageSize :92/:94/label·labelFont·labelColor·labelMaxWidth :96-108/setScaleFactor{low,high} :155-158/setPosition 投影+视锥剔除 :250-283/pick :148/onMouseButton 族 :130-145/addMarker→addCanvasDecoration :300-307）；Cluster(:325-350)+MarkerSet(:357-494——minimumClusterSize :369/clusterRadius :373/markDirty :405/addDecoration 屏幕 XY 距离聚类 :427-493，纯几何零 DOM)。画布承载=DanQing 既有 CanvasDecoration/GLCanvasContext 通路（XE1——DecorateContext::AddCanvasDecoration 已被 WindowAreaTest/MeasureLabel 验证）；title→QToolTip（XE3）；htmlElement DOM 覆盖面不移植登记（XE6）。锁=dqAppTest（聚类距离判定/Cluster 合成/setPosition 剔除/drawDecoration 序——core/frontend 对应 Marker 测试面先核查移植）。
- **X-b2 IncidentMarkerDemo + 工具 + 资产**（IncidentMarkerDemo.ts 262 行 1:1）：
  samples/DisplayTestApp/src 新件：IncidentMarker(:16-77——makeColor 绿→琥珀→红插值 :28-31/onMouseButton 消息框 :34-41/setImage 六面/addMarker WorldDecoration 200mm 圆 :64-76)+IncidentClusterMarker(:80-137——drawFunc canvas 圆+top10 严重度 title)+IncidentMarkerSet(:140-144)+IncidentMarkerDemo(:150-251——500 随机 incidents/useCachedDecorations/loading 门 invalidateCachedDecorations)+ToggleIncidentMarkers 工具(:253-262，toolId "ToggleIncidentMarkers")。六 SVG 资产（Warning_sign/Hazard_*——DTA public 树）入库 samples/DisplayTestApp/assets/（XE5）；imageElementFromUrl→Qt QImage/QSvgRenderer（XE2）；WorldDecoration 圆饼复用 GraphicBuilder 通路（GltfDecoration addDecoration 先例）。锁=DisplayTestAppTest（toggle 状态机/makeColor 插值钉值/聚类合成计数/loading 门/资产加载）。

## 2. EQUIVALENCE 预登记（§11.10 制度）
- XE1：Marker 画布承载（DOM div→canvas decoration 层）。
- XE2：imageElementFromUrl→Qt SVG 装载（QSvgRenderer/QImage 面）。
- XE3：title HTML div→QToolTip 纯文本（`<br>`→换行——M-O(1) tooltip 链）。
- XE4：Contour 面板 DOM→Qt 控件（ContoursSettings 面板族同 E2 先例）。
- XE5：六 SVG 资产入库位=samples/DisplayTestApp/assets/（M-W E5 先例：DTA 演示资产非引擎第三方目录）；发散=无；验证=资产加载锁。
- XE6：Marker 的 htmlElement DOM 覆盖（Marker.ts:121）与 title HTMLElement（:110）不移植——title 走 XE3 QToolTip 纯文本、htmlElement 登记不做；发散=富文本形态不可表达；验证=tooltip 文本锁。

## 3. 双代理编排契约（M-X 版——M-W 契约 8 条沿用）
1. **文件集不相交**：A 道=dqCommon/dqRender/dqApp（DisplayStyle 门面）+引擎测试；B 道=dqApp Marker 族新文件+samples/DisplayTestApp src 新件+其测试+资产。共享面=dqApp/CMakeLists.txt 与 dqAppTest 列表——各道只写本道锚区。
2. **CMake 锚点**：X-p0 已落+M-W 存量锚复用；各道只写本道锚区。
3. **禁动清单**（两道同禁）：DtaToolBars.{h,cpp}（主会话集成面）、CLAUDE.md、docs/ 台账与计划文档、对方道文件、.gitignore、third_party。
4. **worktree 初始化**（代理自做自验）：third_party\qt Junction（指向主检出 D:\Github\DanQing\third_party\qt）+cmake configure（-DCMAKE_PREFIX_PATH=<worktree>/third_party/qt/windows/6.11.1/msvc2022_64）；锚区若缺席（基底旧于 ca4247486f）先 `git merge main --no-edit`。
5. **代理测试面**：无真窗无 dump（dqCommonTest/dqRenderTest/dqAppTest/DisplayTestAppTest）；真窗像素锁/dump E2E/全量 ctest 归主会话合并后串行。
6. **代理纪律**：§11.8 全读参考（锚清单在 §1 任务文）、§5 RED→GREEN 双向证据、EQUIVALENCE §11.10 登记、逐件提交、报告=文件清单+测试摘要+EQUIVALENCE 表+遗留。
7. **主会话集成**：merge→ContoursPanel+工具栏 item 24 激活+IncidentMarker 入口核实→X-a5 E2E+真窗验证→全量门禁（TD-26/29 规程）→CLAUDE.md §1/§15+统计台账+本文实录回填→收口提交。
8. **流水线**：同道内逐件序贯；两道并行；完成即报。

## 4. 门禁（同 M-W 契约）
- A/B worktree：各自目标面全绿（dqCommonTest/dqRenderTest/dqAppTest/DisplayTestAppTest），RED→GREEN 证据随报告。
- 主会话收口：全量 ctest（Debug）串行；TD-26/29 族失败按 §14 规程隔离复跑 ×2 归因。
- E2E 像素锁（X-a5 合成场景等高线 + IncidentMarker 真窗）主会话跑绿为收口门。

## 5. 完成实录（回填区）

---

## 附录：M-Y 空间分类初勘（2026-10-14 备料，M-Y 计划时深化）

**体量**：SpatialClassification.ts（453 行 common 数据层）+ SpatialClassifiersState.ts（83）+
SceneVolumeClassifier.ts（17 薄）+ ClassifierTileTree.ts（205）+ SceneCompositor
renderVolumeClassification（:1895-1922）+ renderForVolumeClassification（:1146）。

**关键范围问题（M-Y 计划呈批时须裁决）**：DTA ClassificationsPanel（420 行）深耦合
@itwin/reality-data-client（RealityDataQueryCriteria/ITwinRealityData——网络 HUB 面，
**与 ➖ Reality data 结案冲突**）。引擎侧分类目标分两族：PlanarClassifier（现实模型
瓦分类=网络数据面）vs **SceneVolumeClassifier/模型-模型体积分类**（疑似无网络依赖）。
M-Y 可行范围疑似 = 模型分类器子集（SpatialClassifiers 数据层 + 体积分类消费链 +
面板模型段），现实模型段登记 ➖。需在 M-Y 勘察时读 SceneVolumeClassifier/
ClassifierTileTree 全文定案。

**DynamicClassifierTool.ts**（364 行）另行全读（M-Y 计划时）。

---

## 附录 2：M-Z 裁决落账（2026-10-15 用户裁决）

- **点云 = 引擎链 only**：段 A-C（dqCommon RealityModelDisplaySettings 五默认值归位
  [shape/edlStrength/edlRadius/maxPixelsPerVoxel/edlFilter]+PointCloudGeometry 生产构造
  +TargetUniforms realityModel 接线+renderPointClouds per-branch settings 消费+EDL 四程序
  全链[glsl/EDL.ts 291 行+CachedGeometry 四包装+合成器支路]）排 A 道队列（Analysis Style
  →BSplineCurve→Marker 族→点云链）；合成点云资产验证（测试内程序生成点阵——
  TileIOFixtures/MinimalSolidBox 先例），零新 dump 采集。**段 D 面板结案**（无真实数据源
  的面板无演示价值）；**段 E 维持结案**（➖9 网络域）。工具栏 item 23 保持置灰（面板结案故）。
  spike 报告全文在 B 道代理纪要（体量：~1,450-1,650 行实现+~500 行锁）。
- **Format Set = 不排期**：等首个真消费面（MeasureTool formatProvider 化/QuantityFormatter
  真移植）捆绑清偿，避免无消费者 ported-but-uncalled；工具栏 item 25 保持置灰。
- **Atmosphere 编辑器 = 提前启动**：载体已落主干（8692f30480）；渲染深链登记 render-dead
  （gating 需 isGeoLocated+背景地图——RenderPlan.ts:130/Target.ts:402/:541 TODO 锚）；
  面板=ViewSettingsPanel Environment 分区内 Atmosphere 子区（B 道在制）。
- **IncidentMarker 序贯件**：A 道移植 Marker/Cluster/MarkerSet（Marker.ts 494 行——
  dqApp 引擎面）→ B 道演示（画布承载裁决：Marker implements CanvasDecoration 同构）。
- **W-b2 解堵件**：A 道 BSplineCurve3d+KnotVector 子集移植后 B 道回做。



## 6. 完成实录（2026-10-23 收口回填）

**提交账**（主干序）：ca4247486f（X-p0 锚点）→ 10e670e384（计划正式化）→ A 道七笔（7382427196 a1 / 5ad3f0a9e1 a2 / 48863a046a a3 / 6d077feae0 a4 / b1c150c12d a5 / 8c32ce52f4 fix1 / 771ab367c4 fix2）→ B 道（merge 7da0c0c880 含 e075dbfae9 b1+73ebf2cd3b b2+修复 67b780e9ea..8a16b83317 两轮）→ 主会话集成件（208eb864e9 真窗锁+ab79d37324 钉值）→ C 道（8aa0fb97af ContoursPanel）→ AV 修复双轮（15e5c44ceb batch-fix / 833b72f5f1 batch-fix2）。

**门禁**（2026-10-23 收口，Debug，正确二进制[fix2 重建]）：**2950 项 = 2948 通过 + 1 跳过族区 + 2 失败均隔离绿**（MinimizeRestore=TD-29 签名；ReopenReloads=M-Y 在册移交项）。四真窗 AV 测试全过（MdiAcsDisc 机器态内核冻结 ×2=非代码[40/40+12/12 干净证据在案]，剔除跑）。dump 族全绿（fix2 实证）。dqRenderTest 793/dqCommonTest 520/dqAppTest 480/DisplayTestAppTest 139。

**AV saga 全账**：收口门禁四真窗 SEH 0xc0000005 翻转态 → A/B 净基线对照（基线 12/12 vs 合并后翻转）→ 主会话增量 bisect 作废（跨公共头 stale-obj 混合——§11.11 教训第四次实录）→ 专案定音 H4（TD-35 每帧产批 × TD-36 注册表双缺口=悬垂 Batch* UAF；M-X 为布局/时序放大器非引入者）→ batch-fix（装饰路径 ×40 160/160）→ 门禁抓出瓦批路径回归（fix1 在 dump teardown 序下反指针悬垂——验证面盲区实证）→ batch-fix2（析构双向闭合）→ 正确二进制门禁 99.9%+2 隔离绿。**TD-36 登记+清偿**（CLAUDE.md §14）；TD-35 增注（暴露器角色）。

**移交项（M-X 域外）**：①~~bridge-edit 数据面 `-b-1-0-0-0-1` 键全根 error=1~~ **已销账（2026-10-23 调查定案）**：该键在 bridge-edit-sweep-v1 存在且 size==byteLength（files/10.imdl 33.9MB），主仓两桥锁实跑全过（859/0/0+L4/L8 钉值吻合）——**lane worktree 数据面伪象**（.gitignore 本地持有件不随 worktree 传播→lane 的 stash 净基线对照被污染=幻影基线失败；教训：worktree 基线对照须先盘点数据面 parity——B 道 4 败/A 道 fix2 移交/终审移交三处同源）；②BridgeEditLodCoverageDiagnostic 同 signature（同源，同销）；③MdiAcsDisc 机器态内核冻结 ×2（修复后干净证据 52 轮在案——留痕观察）；④P-2 newBridge 采集进行中（独立任务，sweep 20GB budget-capped 已收口/d9 续采波待跑/仓内集成排 M-X 后）。

**裁决全录**：见 .superpowers/sdd/…M-X…/progress.md（A/B 并行编排、Marker 分 B 道、Important-2 锁重推导、AV 根因 H4、验证规模×40→×10→一轮门禁[用户流程纠偏：低概率缺陷 A/B 定案后立即升级专案，不做无仪器概率轮]）。

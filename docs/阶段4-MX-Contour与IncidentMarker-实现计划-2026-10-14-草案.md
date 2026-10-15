# 阶段 4 M-X Contour 显示与 IncidentMarker 实现计划（草案——M-W 收口后呈批）

> 里程碑：M-X（Contour display 大件 8/16 + IncidentMarker/MarkerSet）。
> 基线：M-W 收口（日期待回填，全量门禁数待回填）。权威对照面：
> `docs/DTA功能对照统计-2026-10-07.md`（§2 大件 #5 Contour / #12 IncidentMarker）。
> 本草案=M-W 双代理运行期间的勘察备料（M-W 收口后回填基线并呈批）。

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

## 1. 任务分解（草案——呈批时细化）

### A 道（Contour，五件）
- X-a1 DisplayStyleSettings.contours 载体（DisplayStyleSettings.ts:164/:1086/:1103/
  :1181+事件 :516）+ dqApp DisplayStyle 门面。
- X-a2 RenderPlan.contours + Target changeRenderPlan 携带 + ContourUniforms.update
  接线（Target.ts:533/:539）+ GraphicBranch.contourLine。
- X-a3 BatchUniforms.bindContourLUT/Width（BatchUniforms.ts:115-121——LUT 纹理构建
  链全读移植）+ shader 槽激活链核通。
- X-a4 ContourLines.h 自创脚手架归位/清退（S-h 孤儿类先例）。
- X-a5 E2E 像素锁（合成场景高程等高线 WHERE 断言——主会话收口跑）。

### B 道（IncidentMarker，裁决后两件）
- X-b1 Marker/Cluster/MarkerSet 宿主件（裁决 A 形态——Marker.ts:71-494 子集
  移植：位置/尺寸/图/标签/缩放因子/title/onMouseButton + 聚类）。
- X-b2 IncidentMarkerDemo 演示 + ToggleIncidentMarkers 工具注册 + 六 SVG 资产。

## 2. EQUIVALENCE 预登记（草案）
- XE1：Marker 画布承载（DOM div→canvas decoration 层）。
- XE2：imageElementFromUrl→Qt SVG 装载（QSvgRenderer/QImage 面）。
- XE3：title HTML div→QToolTip 纯文本（`<br>`→换行——M-O(1) tooltip 链）。
- XE4：Contour 面板 DOM→Qt 控件（ContoursSettings 面板族同 E2 先例）。

## 3. 门禁（同 M-W 契约）
## 4. 完成实录（回填区）

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



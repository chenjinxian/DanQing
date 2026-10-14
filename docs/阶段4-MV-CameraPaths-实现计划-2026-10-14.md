# 阶段 4 M-V CameraPaths 实现计划（2026-10-14）

> 里程碑：M-V（相机路径——大件 6/16）。
> 基线：M-U 收口（2026-10-10，2750 项全量门禁）。权威对照面：
> `docs/DTA功能对照统计-2026-10-07.md`（§2 大件 #1 相机路径）+
> CLAUDE.md §15 余量序（相机路径列首）。
> 用户指令：按照计划，继续完成与 DTA 有差异的功能（2026-10-14 会话选定 M-V=相机路径）。

## 0. 范围勘察结论（参考全读记录）

DTA 侧（权威工作面）：
- `test-apps/display-test-app/src/frontend/CameraPaths.ts`（685 行，全读）——
  工具栏 item 8 Camera Paths 下拉（Viewer.ts:306-315，`new CameraPathsMenu(this.viewport, container)`，
  tooltip "Saved camera paths"、图标 0xe932）。
  - **Keyframe**（:21-115）：六字段 time(ms)/location/target/up/lensAngle(undefined=正交)/extents
    （undefined=透视）；interpolate(:67-90) 双 undefined 怪癖原样（this.lensAngle 缺席取 other、
    否则 other 缺席取 this、双双在场 Angle.createInterpolate；extents 同形）；clone/createZero/
    fromJSON(缺 json→createZero)/toJSON。
  - **CameraPath**（:123-247）：name/duration/keyframes；sortKeyframes（time 升序）；
    simplifies(:160-184，while 循环 i=1..n-2：before/current/after 三元组 fraction 重建插值，
    sameCameraType 门 + locationDistance/targetDistance < distanceThreshold +
    lensAngleDistance < threshold.degrees + extentDistance < threshold 四条件满足则 splice 删除，
    否则 i++——stop() 调用点实参 = (0.005, Angle.createDegrees(1.0), 0.01))；
    getKeyframeAtTime(:191-203，after 缺席直接用 before；timeDifference==0→fraction=0)；
    getKeyframeRangeAtTime(:210-227，四分支：空 throw / 单帧或 time<首帧→{before:首帧} /
    中间逐对 / 晚于末帧→{before:末帧})；fromJSON(缺→createEmpty)/toJSON。
  - **CameraPathsMenu extends ToolBarDropDown**（:249-685）：
    - UI（populateFromPathList :321-453）：path 名 textbox（size=36、keypresshandler Enter 追加+存盘、
      onkeyup 校验：非空且不重复，非法→红字+Create 禁用）+ hr + select 列表
      （size = 1===n?2:min(15,n)，空隐藏；空路径灰字+tooltip"This path is empty..."；
      onchange→selectedPath；keyup Delete→deletePath）+ Create/Delete 钮 + hr +
      Record/Play/Stop 钮；_onStateChanged(:434-448) 互斥矩阵：无选中→play/record/stop/delete 全禁+
      列表解禁；有选中→canRecord=(duration==0‖keyframes 空)，play 禁=isPlaying‖canRecord，
      record 禁=isPlaying‖!canRecord，stop 禁=!isPlaying，列表禁=isPlaying，delete 恒解禁。
    - 持久化（:490-508）：`DtaRpcInterface.readExternalCameraPaths/writeExternalCameraPaths(
      filename=viewport.view.iModel.key)`，JSON.parse 失败→空表。
    - onViewChanged(:303-310)：iModel 变更→重 populate。
    - prepareView(:516-529)：setAnimator(undefined) + 非录制 turnCameraOn()
      （注释：防正交视图无相机 bug——非透视首帧会关相机）+ synchWithView()。
    - play(record)(:531-561)：播放中先 stop；复位计时；sortKeyframes；rAF 泵
      （elapsed=timestamp 差，首帧 0；computeAnimationFrame；存活则续约）；
      prepareView 后启泵、置 _isPlaying/_isRecording、_onStateChanged。
    - stop()(:563-589)：cancelAnimationFrame；录制收尾=duration 落账+simplifies+存盘+重 populate；
      复位全部状态。
    - recordAnimationFrame(:617-642)：is3d+supportsCamera 门；location=npcToWorld(0.5,0.5,1)、
      target=npcToWorld(0.5,0.5,0)（距离<=EPSILON 跳过）、up=location.unitVectorTo(
      npcToWorld(0.5,1,0))（undefined 跳过）；isCameraOn→lensAngle=camera.getLensAngle()，
      否则 extents=view.getExtents()；push Keyframe(currentAnimationTime,...)。
    - setCameraPosition(:648-684)：is3d+supportsCamera 门；lensAngle 在场→透视
      lookAt({eyePoint:location,targetPoint:target,upVector:up,lensAngle:validateLensAngle 后})；
      否则 extents 必须（缺→throw）→正交 lookAt({eyePoint:location,
      viewDirection:target.minus(location),upVector:up,newExtents:extents})；
      synchWithView({noSaveInUndo:true})。
    - playAnimationFrame(:608-615)：setAnimator(undefined)+getKeyframeAtTime+setCameraPosition；
      currentAnimationTime>duration→endAnimation。

DanQing 侧（现状实证）：
- **引擎依赖全部在位**：Viewport::SetAnimator/TurnCameraOn(M-O(3) P1)/SynchWithView
  （SyncOptions.noSaveInUndo 字段在案 Viewport.h:101-103）/NpcToWorld；ViewState3d::lookAt(
  LookAtArgs)；Camera::validateLensAngle(Camera.h:60)；ViewState3d::GetLensAngle(:352)/
  GetExtents(:165)；IModelConnection::GetKey(:139)。
- **工具栏 item 8**：DtaToolBars.cpp:486-487 `addEmptyDropDown(tb,"Camera Paths",0xe932,...)` 置灰占位。
- **先例**：TimelinePanel（M-U U-f——QToolButton InstantPopup+openDropDown 接法、widget destroyed→
  delete panel 生命周期、QTimer 16ms=rAF 泵 E5）；SavedViewsPanel（I9——本地文件持久化
  `DANQING_SAVED_VIEWS_DIR` env + sanitizeKey + `<dir>/<key>.json`）。
- **缺口**：Keyframe/CameraPath 数据类；CameraPathsPanel；录制/播放泵；持久化缝；
  item 8 激活；dqGeom 小件 Point3d::UnitVectorTo（geometry-core Point3d.unitVectorTo）、
  Angle 插值（geometry-core Angle.createInterpolate——先核 DanQing Angle.h 是否已有等价面，
  缺则 1:1 补齐）。

## 1. 任务分解（五件，逐件 RED→GREEN + 提交）

### V-a Keyframe/CameraPath 数据层
宿主侧新文件 `samples/DisplayTestApp/src/Gui/CameraPaths.{h,cpp}`（SavedViewsPanel 同族位置）。
- Keyframe 全量（六字段/interpolate 双 undefined 怪癖/clone/createZero/JSON 对偶——
  AngleProps=double 弧度 JSON 形态核 CameraPaths.ts:101-112）。
- CameraPath 全量（sortKeyframes/simplifies/getKeyframeAtTime/getKeyframeRangeAtTime/JSON 对偶）。
- dqGeom 补齐：Point3d::UnitVectorTo（geometry-core Point3d.unitVectorTo——先读参考再补）；
  Angle 插值面（createInterpolate/Interpolate——核 Angle.h 现状，缺则补）。
- 测试 `CameraPathsTest.cpp`（Authored——DTA 无 CameraPaths 测试面，§5(f) 标注；
  行为锚=CameraPaths.ts 逐行语义）：interpolate 双 undefined 三形态/单帧直通/fraction=0 除零门/
  simplifies 四条件逐阈删减（删/留双向）/getKeyframeRangeAtTime 四分支/JSON round-trip 缺省守恒。

### V-b 持久化缝
readExternalCameraPaths/writeExternalCameraPaths → **EQUIVALENCE 本地文件**（I9 同型）：
`DANQING_CAMERA_PATHS_DIR` env（缺省回落=SavedViewsPanel 同目录策略）、
`<dir>/<sanitize(iModel.key)>.cameraPaths.json`；读失败/JSON 坏→空表（参考 catch 语义 :494-498）。
锁：round-trip（写后读逐字段）+ 坏 JSON→空表 + 缺席文件→空表。

### V-c CameraPathsPanel 面板
`CameraPathsPanel` QWidget（CameraPathsMenu :249-514 1:1）：
- QLineEdit path 名（placeholder=tooltip 文案；Enter=追加+存盘；textChanged=校验红字+Create 禁用）。
- QListWidget 路径表（min(15,n) 行高策略 EQUIVALENCE；空隐藏；空路径项灰字+tooltip；
  currentRowChanged→selectedPath；Delete 键删）。
- Create/Delete/Record/Play/Stop 五 QPushButton；互斥矩阵 _onStateChanged 1:1。
- onViewChanged 换 iModel 重 populate（TimelinePanel 同契约——Viewport OnViewChanged 事件）。
- 锁：互斥矩阵全态（无选中/空路径选中/实路径选中/播放中）、校验红字三态、
  Enter 追加链、Delete 键删、populate 换 iModel。

### V-d 录制/播放泵
- prepareView/play/stop/computeAnimationFrame/playAnimationFrame/recordAnimationFrame/
  setCameraPosition/endAnimation 全量（:516-684 1:1）。
- **QTimer 16ms 单发链=rAF**（M-U E5 先例——暂停/停止早退不补链=泵自然死亡）；
  QElapsedTimer 供 timestamp delta（首帧 elapsed=0 怪癖原样）。
- 锁：录制两帧位姿钉值（NpcToWorld 已知视口→location/target/up 断言）、播放帧
  lookAt 参数对账（synthetic 路径两帧插值→视口 frustum 变化锁）、stop 录制
  duration 落账+simplifies 调用、播放越界 endAnimation。

### V-e 激活 + E2E + 收口
- DtaToolBars item 8：`addEmptyDropDown` → Timeline item 19 同形（QToolButton InstantPopup+
  openDropDown+widget destroyed→delete panel）。
- **E2E 像素锁**（Authored §5(g)，WHERE 位置断言）：joeshouse saved 视图→合成两帧路径
  （首帧=saved 位姿、末帧=平移后位姿）→播放泵驱动→内容位移方向断言（平移方向 vs 内容
  移动方向反向）+播完恢复路径外的视口不动。
- 收口：CLAUDE.md §1 增 M-V 段 + §15 基线（大件 6/16、余量 10）+ 统计台账
  Camera Paths 行 ❌→✅ + 全量门禁。

## 2. 数据面边界（EQUIVALENCE 预登记，§11.10 制度）

| # | 边界 | 处置 |
|---|---|---|
| E1 | DtaRpcInterface 外存 RPC → 本地 JSON 文件 | I9 Saved Views 同型，`DANQING_CAMERA_PATHS_DIR` env |
| E2 | rAF 帧泵 → QTimer 16ms 单发链 + QElapsedTimer | M-U E5 先例 |
| E3 | DOM select size 属性/option 灰字 tooltip | Qt QListWidget 行数策略+item 前景色+setToolTip 等价 |
| E4 | "TODO: Handle global coordinates"（参考未处理） | 原样不处理（参考同界） |

## 3. 门禁标准
- 每任务：定向测试 RED→GREEN 双向 + 受影响族回归绿。
- 里程碑收口：全量 ctest（Debug）零失败（环境态族按 TD-26/29 规程隔离归因）。

---

## 4. 完成实录（收口时回填）

| 任务 | 提交 | 实绩 |
|---|---|---|
| V-a | 待定 |  |
| V-b | 待定 |  |
| V-c | 待定 |  |
| V-d | 待定 |  |
| V-e | 待定 |  |

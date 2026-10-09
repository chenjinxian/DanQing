# 阶段 4 M-T：Ambient Occlusion（环境光遮蔽）实现计划

> 立项：2026-10-09 用户裁决（M-S 收口后大件 13 余量中选定 AO）。
> 执行纪律：§0 来源铁律（先全读参考再动手——§11.8）+ §5 RED 锁先行 +
> §6 五维对照 + §11.10 ported-but-uncalled/EQUIVALENCE 登记 + §11.11 WHERE
> 位置断言。本文档 = 计划 + 完成实录（逐件回填）。
> 参考检出：D:/Github/itwinjs-core（7e57d018 后代 88da8fb9——行号以此为准）。

## 0. 目标与范围裁决

**目标**：DTA 的 Ambient Occlusion 功能全链对齐——面板开关与八参编辑、
引擎 HBAO 式屏空间遮蔽上屏、像素级回归锁。

**范围**（参考实证账见 §1）：
- 收：RenderPlan.ao 载体 → Target AO 门 → compositor AO 渲染路径
 （renderOpaqueAO/renderAmbientOcclusion）→ AO/Blur 两 shader 移植 →
 composite AO 乘腿 → 噪声纹理 → 面板 AO 编辑区（checkbox+8 滑条+Reset）→
 像素锁。
- 旧件清退：SsaoTechnique 自创 SSAO 孤儿（CompositingShaderBuilders.h
 getSsaoFragmentShader——非参考 HBAO）+ BlurTechnique/BlurTestOrderTechnique
 桩件随本件替换为参考移植。
- 不收（登记）：hidden-edges 子路径（renderOpaqueAO :1009-1043——DanQing
 HiddenEdge pass 形态待核对，首版走 haveHiddenEdges=false 臂 + 登记）；
 DB 变体（readDepth 真深度纹理——见 E1 裁决：PB 变体先行）；点云 EDL
 （EdlTechnique 桩不在本件范围）。

## 1. 参考实证账（全读完毕——2026-10-09）

### 1.1 数据面（已在仓）

- `core/common/AmbientOcclusion.ts`（86 行全读）：`AmbientOcclusion.Settings`
 八字段——bias 0.25 / zLengthCap 0.0025 / maxDistance 10000 /
 **intensity 1.0**（注释文档写 2.0——代码为准）/ **texelStepSize 1**
 （注释写 1.95——代码为准）/ blurDelta 1.0 / blurSigma 2.0 /
 blurTexelStepSize 1.0；toJSON 省略默认值（逐字段 !=default 才写）。
 DanQing `dqCommon/AmbientOcclusion.h` 已在（M-Q E2 数据面）——默认值须逐
 字段对拍（本计划 T-a）。
- ViewFlags.ambientOcclusion 位已在（dqCommon）。
- DTA 编辑器 `AmbientOcclusion.ts`（232 行全读）：checkbox + 8 滑条
 （Bias 0..1 step .025 / Length Cap 0..0.25 step .000025 / Max Distance
  1..50000 step 10 / Intensity 0.1..16 step 0.1 / Step 1..50 step .005 /
  Blur Delta 0.5..1.5 step .0001 / Blur Sigma 0.5..5 step .0001 /
  Blur Step 1..5 step .005）+ Reset（Settings.defaults）+
 updateAmbientOcclusion 写环（toJSON→改→fromJSON→settings setter→sync）+
 _update 闭包（is3d 门 + 粗体 label + 显隐 + 回读）。
- `DisplayStyleSettings.ts`：`ambientOcclusionSettings` setter +
 onAmbientOcclusionChanged（DanQing 门面事件 OnAmbientOcclusionChanged 已在
 dqApp DisplayStyle.h:58 + Viewport 监听在——setter/raise 面待落[T-b]）。

### 1.2 引擎链（参考逐行锚）

| 环节 | 参考锚 | DanQing 现状 |
|---|---|---|
| plan.ao 载体 | RenderPlan.ts:60（`readonly ao?: AmbientOcclusion.Settings`）+ :125（`style.is3d() ? style.settings.ambientOcclusionSettings : undefined`） | **缺**（RenderPlan.h 无 ao 字段；ValidateRenderPlan 未填） |
| Target AO 门 | Target.ts:524-530（SmoothShade && is3d && plan.ao && vf.ambientOcclusion → _wantAO=true + settings 存管；否则 vf 位关） | 半在——TargetImpl::m_wantAO + wantAmbientOcclusion() 在（TargetImpl.h:292-294/545），**无 setter 调用方=ported-but-uncalled**；无 ambientOcclusionSettings 载体 |
| compositeFlags AO 位 | RenderCommands.ts:86-89（wantAO → flags|=AO） | **已在**（RenderCommands.cpp:1150-1152） |
| AO 分流 | SceneCompositor.ts:943-947（renderOpaque 头部：AO 位且非读像素 → renderOpaqueAO） | 缺（renderOpaque 恒直出主目标——**DanQing 结构差异：参考 opaque 恒入 FBO 再合成，DanQing 非合成帧直出屏**——AO 开启时须切 FBO 路径，见 §3 裁决） |
| renderOpaqueAO | SceneCompositor.ts:981-1044（opaqueAndCompositeAll MRT[color+featureId+depthAndOrder+depth] 三 pass + hidden-edge 子路径[:1009-1043]） | 缺 |
| renderAmbientOcclusion | SceneCompositor.ts:1220-1252（AO 计算→blurX→blurY 三绘制，fbStack.execute + RenderState.defaults） | 缺 |
| FBO/纹理 | SceneCompositor.ts Textures :185-200（occlusion/occlusionBlur = RGBA8 附件）+ FrameBuffers :254/:354 | **已在**（CompositorTextures enableOcclusion/getOcclusion/getOcclusionBlur + CompositorFrameBuffers 同名面——ported-but-uncalled 待接线） |
| AO 几何/程序 | CachedGeometry.ts:740-758（AmbientOcclusionGeometry——ViewportQuad 承载 + [depth, depthAndOrder] 纹理组 + System.noiseTexture）+ glsl/AmbientOcclusion.ts（301 行——HBAO 式：4 方向×6 步，噪声旋转、深度重建法线、bias/zLengthCap/intensity/texelStepSize 四参 + maxDistance 淡出 + PB/DB 双变体） | 缺（**SsaoTechnique 自创孤儿占位在 TechniqueId::AmbientOcclusion 注册位**——清退） |
| Blur 几何/程序 | CachedGeometry.ts:767-790（BlurGeometry——BlurType NoTest/TestOrder）+ glsl/Blur.ts（123 行——高斯 7 步、u_blurSettings=blurDelta/blurSigma/blurTexelStepSize、TestOrder 臂读 depthAndOrder 跳过线边） | 缺（BlurTechnique/BlurTestOrderTechnique 桩件——清退替换） |
| composite AO 乘腿 | glsl/Composite.ts:73（`opaque.rgb *= computeAmbientOcclusion()`）+:78-79（默认/真两变体）+:127-132（wantOcclusion 门）+:186-189（u_occlusion 绑单元 4=CompositeGeometry.occlusion[textures[4]]） | 缺（DanQing 合成 shader 无 AO 腿） |
| 噪声纹理 | System.ts:457-460（4×4 Luminance 16 字节定值 [152,235,94,173,...] Repeat——onInitialized 建） | 缺（RenderSystemImpl 无 noiseTexture） |
| Technique 注册 | Technique.ts:910-912 + :1082-1083（SingularTechnique×3） | 占位在（SsaoTechnique 错体——换参考体） |
| DTA 面板区 | ViewAttributes.ts:326（addAmbientOcclusion）+ AmbientOcclusion.ts 全件 | 面板置灰清单有 "Ambient Occlusion" 项——摘除+挂真件 |

### 1.3 DanQing 结构适配面（裁决登记）

- **E1（PB vs DB 变体）**：参考 `_shouldUseDB()=supportsLogZBuffer`——DB 臂
 读真浮点深度纹理（u_depthBuffer）+ logZ 反算；PB 臂读 depthAndOrder pick
 纹理（mix(far,near,linearDepth)）。DanQing 的 opaque FBO 深度附件为不可读
 renderbuffer 形态（待 T-c 实锤）→ **PB 变体先行**（参考合法路径——
 WebGL1 平台同款）；若 T-c 实锤 DanQing FBO 深度可采样则改 DB 并双态锁。
- **E2（DanQing 非合成帧直出屏的结构差）**：参考 opaque 恒入
 opaqueAndCompositeAll FBO；DanQing 无合成需求时直出主目标。AO 开 →
 必须走 FBO+合成路径（compositeFlags AO 位已驱动 needComposite）。
 renderOpaqueAO 的 DanQing 形：renderOpaque 头部 AO 分流（:943-947 语义）→
 FBO 三 pass → AO 三绘制 → 合成出屏。
- **E3（hidden-edge 子路径）**：DanQing 的 HiddenEdge pass 命令桶面待核对；
 首版 haveHiddenEdges=false 臂（pingPong 复用/copyPickBuffers 子链不建）+
 登记。若 HiddenEdge 桶在 AO 开启时有内容则按参考补全（T-d 判定）。
- **E4（noise 纹理承载）**：参考 System.onInitialized 建 4×4 Luminance；
 DanQing RenderSystemImpl 初始化同位建（RenderSystem 单例语义）。
- **E5（u_occlusion 单元 4）**：与 PickDepthAndOrder 同单元——合成 pass 与
 pick 读取帧序隔离（参考同形）；DanQing 合成器绑定面落单元 4。
- **E6（displayStyleChanged 族失效）**：AO 设置改动 → 参考 setter→
 onAmbientOcclusionChanged→displayStyleChanged[SetDisplayStyle+
 maybeInvalidateScene+setFeatureOverrideProviderChanged]；DanQing 门面
 setAmbientOcclusionSettings + OnAmbientOcclusionChanged raise + Viewport
 既有监听面（Viewport.cpp:684 族——核对其失效粒度）。

## 2. 分件

- **T-a 数据面对拍**（dqCommon AmbientOcclusion.Settings 八字段默认值/toJSON
 省略策略 vs 参考逐字段——含 intensity/texelStepSize 的注释-代码分歧钉
 [代码 1.0/1 为准]；锁：dqCommon 侧默认值锁+round-trip）。
- **T-b RenderPlan/Target 接线**：RenderPlan.ao 载体（RenderPlan.ts:60/125）
 + ValidateRenderPlan 填充 + TargetImpl::changeRenderPlan AO 门
 （Target.ts:524-530——四条件 AND + 否臂 vf 位关）+ ambientOcclusionSettings
 载体 + DisplayStyle::setAmbientOcclusionSettings 门面（S-f 同形——equals
 短路+先 raise 后赋值；OnAmbientOcclusionChanged ported-but-uncalled 清偿）。
 锁：dqRenderTest 门条件矩阵 + dqAppTest 门面锁。
- **T-c FBO/纹理接线 + 深度可读性实锤**：CompositorTextures/FrameBuffers 的
 occlusion 使能面接入 compositor preDraw（wantAmbientOcclusion 驱动）+
 深度附件采样性取证（DB/PB 裁决实锤——E1）+ opaqueAndCompositeAll FBO 的
 AO 路径适配。锁：FBO 建立/尺寸跟随/释放。
- **T-d AO+Blur shader 移植**：glsl/AmbientOcclusion.ts（301 行——PB 变体臂）
 + glsl/Blur.ts（123 行——NoTest 臂先行，TestOrder 臂随 E3 判定）→
 DanQing shader 基建形态（ProgramBuilder/GraphicUniform 绑纹理）+
 AmbientOcclusionGeometry/BlurGeometry 包装（CachedGeometry.ts:740-790 形）+
 Technique 注册三件套换参考体（SsaoTechnique 孤儿清退）+ 噪声纹理建
 （RenderSystemImpl::onInitialized 等价面 + 16 字节定值表原样）。锁：
 shader 编译锁（SurfaceCompileTest 族同形——headless-GL 跳过族注记）+
 变体源断言。
- **T-e compositor AO 渲染路径**：renderOpaque AO 分流 + renderOpaqueAO
 （FBO 三 pass——E3 首版无 hidden-edge 子链）+ renderAmbientOcclusion 三
 绘制（fbStack.execute 等价=DanQing FBO 驱动面）+ composite AO 乘腿
 （u_occlusion 单元 4 + wantOcclusion 变体门）。锁：**AO 像素锁先行**
 （§5(g)——可控资产两盒交角或 joeshouse 墙角：AO on/off 帧差 + WHERE
 [交角带变暗量 > 开阔面变暗量] + 关闭恢复精确）。
- **T-f 面板 AO 编辑区**（AmbientOcclusion.ts 232 行 1:1——checkbox +
 8 滑条[参考 min/max/step 原值] + Reset + 写环 + 回读 + is3d 门）；
 ViewSettingsPanel 置灰清单摘 "Ambient Occlusion"。锁：编辑器槽锁
 （滑条写值→settings 逐项 + Reset 默认 + vf 位开关 + 回读）。
- **T-g 收口**：全量 ctest（TD-26/29 族规程）+ CLAUDE.md §1 M-T 段 + §15
 基线（大件 13→12）+ 本文完成实录回填 + M-Q E2 的 AO 半勾销。

## 3. 风险与既有教训前置

- **FBO 渲染路径切换风险**（E2——DanQing 直出屏架构上 AO 开启切 FBO+合成）：
 合成路径已有 OIT 合成（CompositeFlags≠None 族）——AO 复用该通道而非新建；
 像素锁必须先锁"AO 开且内容零变化不黑屏"（合成路径回归），再锁遮蔽语义。
- **深度重建法线的数值敏感**：computeNormalFromDepth 邻域差分——PB 变体的
 pick 深度精度（depthAndOrder 的打包精度）决定法线质量；像素锁判据须容忍
 边界噪声（断言暗区**趋势量**非精确值——同 M-Q Q-d 的 cel 量化判据先例）。
- **TD-26/29 族**：AO 新增 FBO 绘制改变帧时序——真窗口像素锁遇抖动按规程
 隔离复跑 ×2 记录。
- **孤儿清退面**：SsaoTechnique/BlurTechnique 旧件的引用面（TechniqueRegistry
 注册点+编译点）随替换全清——含编译锁的断言迁移。

---

## 完成实录（逐件回填）

（待填）

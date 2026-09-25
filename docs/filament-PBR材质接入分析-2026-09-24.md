# DanQing 接入 filament 全量 PBR 材质可行性分析

> 2026-09-24 战略分析（Authored：源码取证报告，非参考移植）。姊妹篇：
> `itwinjs-core-WASM可替换性分析-2026-09-24.md`、`filament优秀实践移植分析-2026-09-24.md`。
> filament @ 6a63790（2026-09-08）；所有 file:line 证据来自分析时真实读过的源码。

## 1. 结论摘要

1. **可行且唯一合理的路径是方案 C**（混合）：管线编排保留 itwinjs 多 Pass（MRT 拾取/WBOIT/edge/7 变体合成全部不动），材质与光照数学整体移植 filament 的 GLSL chunk。
2. filament PBR 全集 = 5 个着色模型（UNLIT/LIT/SUBSURFACE/CLOTH/SPECULAR_GLOSSINESS）+ LIT 上的 8 类编译期特性层（clearcoat/sheen/anisotropy/iridescence/refraction/second-lobe/specular-factor/emissive），全部以 `MATERIAL_HAS_*` define 组合，无运行时分支。
3. DanQing 侧 gap 集中在 5 处：Cook-Torrance BRDF 缺失（现为 Blinn-Phong）、IBL 三件套（DFG LUT/预滤波 cubemap/SH）为零、glTF MR/自发光/AO 纹理未读、RHI cubemap 面上传与半浮点上传有缺口、无曝光/tonemap 链。
4. filament 前端（FEngine/FrameGraph）**不可整体引入**——违反 §0/§7/§8（双引擎、资源模型冲突、拾取/OIT 语义不兼容）。
5. MRT 拾取与 WBOIT 对 PBR 天然透明：featureId 走 AssignFragData 与光照模型无关；OIT 权重只消费 PBR 输出的最终颜色。
6. 分四期落地：P0 metallic-roughness+方向光 → P1 IBL → P2 特性层 → P3 点光源与 EVSM 阴影整合；每期均可独立像素回归锁定。

## 2. filament PBR 全集清单

### 2.1 着色模型（5 个）

| 模型 | 证据 | 说明 |
|---|---|---|
| UNLIT | `shaders/src/surface_shading_unlit.fs:38`（evaluateMaterial） | 无光照，仅 baseColor+emissive+postLighting |
| **LIT（standard）** | `shaders/src/surface_shading_model_standard.fs:112-174`（surfaceShading）+ `shaders/src/surface_shading_lit.fs:368`（evaluateMaterial） | Cook-Torrance 微表面，可选各特性层 |
| SUBSURFACE | `shaders/src/surface_shading_model_subsurface.fs`（全文件 65 行，wrap diffuse + thickness 透射近似） | 5 参数：subsurfacePower/Color/thickness |
| CLOTH | `shaders/src/surface_shading_model_cloth.fs:1-61` | Charlie D + Neubelt V  sheen 项 + 可选 subsurfaceColor |
| SPECULAR_GLOSSINESS | `libs/filabridge/include/filament/MaterialEnums.h:52` + `surface_shading_lit.fs:63-69` | 遗留 KHR_materials_pbrSpecularGlossiness |

枚举定义：`libs/filabridge/include/filament/MaterialEnums.h:47-53`（`enum class Shading: UNLIT/LIT/SUBSURFACE/CLOTH/SPECULAR_GLOSSINESS`）。

### 2.2 LIT 模型的特性层（编译期开关，来自 .mat 参数自动生成 `MATERIAL_HAS_*` define）

`MaterialInputs` 全字段：`shaders/src/surface_material_inputs.fs:20-136`。各特性证据：

| 特性 | 直接光路径 | IBL 路径 | glTF 扩展对应 |
|---|---|---|---|
| clearcoat（IOR 固定 1.5） | `surface_shading_model_standard.fs:10-28`（clearCoatLobe，D_GGX+V_Kelemen+F_Schlick(0.04)） | `surface_light_indirect.fs:405-435`（evaluateClearCoatIBL，F_Schlick(0.04,NoV) 衰减） | KHR_materials_clearcoat |
| sheen | `standard.fs:1-8`（sheenLobe：D_Charlie+V_Neubelt） | `surface_light_indirect.fs:388-403`（sheenDFG×prefilteredRadiance + sheenScaling 反照率衰减） | KHR_materials_sheen |
| anisotropy | `standard.fs:30-59`（Kulla 2017 由单一 roughness 派生 at/ab） | `surface_light_indirect.fs:153-160`（bent reflected vector） | KHR_materials_anisotropy |
| iridescence | `surface_brdf.fs:163-289`（Belcour & Barla 2017 Fourier 拟合，`iridescentF0` 重拟合 Schlick） | 经 f0 进入 specularDFG | KHR_materials_iridescence |
| transmission/refraction | `surface_shading_model_standard.fs:132-134`（Fd×(1−transmission)） | `surface_light_indirect.fs:449-714`：REFRACTION_TYPE SOLID/THIN × REFRACTION_MODE CUBEMAP/SSR 两轴；色散 4 波长 K0-K3 矩阵 `:569-667`（tools/specgen 生成） | KHR_materials_transmission / ior / volume |
| specularFactor/specularColorFactor | `standard.fs:66-92`（dielectric f0/f90 重算） | `surface_light_indirect.fs:139-144` | KHR_materials_specular |
| second specular lobe | `surface_shading_lit.fs:210-250` + `standard.fs:125-129`（CLIENT_MATERIAL_API_LEVEL≥2） | `surface_light_indirect.fs:147-151,761-769` | filament 自有 |
| emissive | `surface_shading_lit.fs:349-360`（exposure 衰减） | — | core glTF emissive |
| ambientOcclusion | `PixelParams`（`surface_lighting.fs:30`）→ SSAO 组合（`surface_light_indirect.fs:779-785`） | | core glTF occlusion |

### 2.3 公共 BRDF chunk

`shaders/src/surface_brdf.fs`（396 行）：BRDF 配置宏 `:29-48`（DIFFUSE_LAMBERT、SPECULAR GGX/SmithGGX(Fast)/Schlick、clearcoat Kelemen、anisotropic GGX、cloth Charlie/Neubelt）；`D_GGX:54`、`V_SmithGGXCorrelated:102`、`V_SmithGGXCorrelated_Fast:115`、`V_Kelemen:133`、`V_Neubelt:139`、`F_Schlick:145-157`、`Fd_Lambert:369`、`Fd_Burley:373`、dispatch 层 `:295-396`。

### 2.4 IBL 与光照基础设施

- **DFG LUT**：`filament/src/DFG.cpp:41-80`——128×128 RGB16F（`DFG.h:33-66`，`FILAMENT_DFG_LUT_SIZE 128`），离线生成（`generated/resources/dfg.h`）zstd 压缩内嵌引擎；shader 侧 `PrefilteredDFG_LUT` `surface_light_indirect.fs:25-28`。
- **specular IBL**：预滤波 cubemap mip 链，`prefilteredRadiance` `:122-129`；roughness→LOD 二次映射 `perceptualRoughnessToLod:115-120`。
- **irradiance**：SH9 `Irradiance_SphericalHarmonics:43-63`（`frameUniforms.iblSH`），无 SH 时 roughness-one mip 4-tap 回退 `:65-109`。
- **多散射能量补偿**：`energyCompensation = 1 + f0*(1/dfg.y − 1)`，`surface_shading_lit.fs:266-268`。
- **直接光**：方向光 `surface_light_directional.fs:23-97`（含 sun-disc 面积光近似 `:9-21`）；额外方向光 `:106-155`；点光/聚光走 froxel 网格 `surface_light_punctual.fs:6-63`、光源 UBO 解码 `getLight:126-173`、循环 `evaluatePunctualLights:180-275`。
- **LightManager 类型**：`filament/include/filament/LightManager.h:199-205`——SUN/DIRECTIONAL/POINT/FOCUSED_SPOT/SPOT。
- **IBL 资产管线（离线或运行时均可）**：`libs/ibl/include/ibl/CubemapIBL.h:56-63`（`roughnessFilter` 预滤波）、`CubemapSH.h:47`（`computeSH`）；离线工具 `tools/cmgen`；**运行时**路径 `libs/iblprefilter`（`IBLPrefilterContext`，equirect→prefiltered cubemap+SH）、`libs/generatePrefilterMipmap`。
- **SSR**：`MATERIAL_HAS_REFLECTIONS` + `sampler0_ssr`，`surface_light_indirect.fs:727-748`。
- **主入口**：`evaluateIBL` `surface_light_indirect.fs:717-839`；灯光总编排 `evaluateLights` `surface_shading_lit.fs:316-347`（IBL→directional→extra→punctual）；`main` 骨架 `surface_main.fs:40-61`。

### 2.5 材质描述与编译管线

- **.mat 格式**：`filament/src/materials/defaultMaterial.mat:1-13`——`material{name, shadingModel, variantFilter, featureLevel}` 头 + `fragment{void material(inout MaterialInputs)}` 用户代码块。
- **编译**：`matc` → `libs/filamat/src/MaterialBuilder.cpp:893-913`（`generateShaders` 用 `ShaderGenerator` 拼 chunk + glslang 前端 + `GLSLPostProcessor.cpp` 后端消歧/压缩）→ `.filamat` 二进制（filaflat 容器）；变体枚举 `libs/filamat/src/MaterialVariants.cpp:30-57`。
- **变体键（7 bit/128 个，61 个在用）**：`libs/filabridge/include/private/filament/Variant.h:30-107`——DIR（方向光）/STE（stereo）/SRE（收阴影）/SKN（蒙皮 morphing）/DEP（depth-only）/FOG/PCK/S2D/MNT；位布局注释 `:57-89`。**关键结论：材质特性（clearcoat 等）不是变体维度，而是编译期 `MATERIAL_HAS_*` define**；变体只处理"管线状态"（光/阴影/雾/深度）。
- **运行时**：`filament/src/Material.cpp` + `MaterialInstance.cpp`；渲染时按 variant key 从 Material 缓存取 program。

## 3. DanQing 现状与 gap

### 3.1 现状取证

| 面 | 现状 | 证据 |
|---|---|---|
| 光照模型 | itwinjs Blinn-Phong：`computeDirectionalLight`（sun+portrait 双方向光、pow(specularDot, exponent)）+ ambient + hemisphere ground/sky + fresnel + toon | `dqRender/src/render/LightingShaders.h:27-35,38-122`；uniform 装配 `SceneCompositorImpl.cpp:846-856` |
| 材质参数 | MicroStation 风格：`mat_weights`(diffuse/specular) + `mat_specular.rgb` + exponent + textureWeight；material atlas LUT 4 texel | `dqRender/src/shader/SurfaceMaterialShaders.h:74-93,160-187` |
| 纹理 | `s_texture`=unit 0，`s_normalMap`=unit 13；法线 oct 编码 + **导数 TBN**（无 tangent 属性） | `SceneCompositorImpl.cpp:1660-1706`；`SurfaceNormalShaders.h:20-30,68-94`；`dqRender/src/gl/RenderFlags.h:122-177`（TextureUnit 0–16） |
| MRT 拾取 | opaqueAll = [color, featureId, depthAndOrder]+depth；fragment 主链为插槽函数链（computeBaseColor→finalizeNormal→applyLighting→assignFragData），MultiVariantTechnique 出 pick/nopick 变体 | `CompositorFrameBuffers.h:92-93`；`ShaderBuilder.cpp:871-940`；`TechniqueImpl.h:30-51`（TechniqueId 表，7 个 Composite* 变体 `:43-49`） |
| OIT | WBOIT：accumulation+revealage 双 MRT，权重与光照模型无关 | `CompositorFrameBuffers.h:94`；`OitShaders.h:53-77,148-153`；`SceneCompositorImpl.cpp:886-918` |
| glTF 消费 | GltfMesh 有 `metallicFactor/roughnessFactor` 字段但**无人消费**；只传 baseColorFactor+baseColorTexture+normalMapTexture | `dqRender/PublicAPI/dqRender/GltfReader.h:56-58,65,77`；`dqApp/src/GltfDecoration.cpp:165-228` |
| 阴影 | EVSM 太阳阴影已存在（可复用为 PBR directional visibility） | `dqRender/src/shader/SolarShadowShaders.h:1-5` |
| RHI | filament 风格 Driver/Handle；`SAMPLER_CUBEMAP` 枚举、`createTexture(SamplerType,levels)`、GL 后端 `glTexStorage2D` 支持 cubemap | `PublicAPI/dqRender/rhi/DriverEnums.h:128`；`Driver.h:150`；`OpenGLDriver.cpp:1457,1594-1599` |
| uniform 机制 | 无 UBO：UniformHandle set* 直发 glUniform（TD-15） | `LightingShaders.h:128-149`；CLAUDE.md TD-15 |
| 天空 | itwinjs 风格 SkyBox（image/solid/gradient），无 HDR/IBL 概念 | `dqCommon/PublicAPI/dqCommon/Environment.h:24-29` |

### 3.2 RHI 缺口（实测）

1. **cubemap 面上传不通**：`OpenGLDriver.cpp:1647-1650` 对 `GL_TEXTURE_CUBE_MAP` 走 `glTexSubImage2D(tex->glTarget,...)`——cube target 必须按 `GL_TEXTURE_CUBE_MAP_POSITIVE_X + z` 分发面，当前 z 被忽略（上传会失败/落在非法 target）。
2. **半浮点上传不通**：`:1646,1649` 像素类型硬编码 `GL_UNSIGNED_BYTE`——`RGB16F` 格式枚举存在（`DriverEnums.h:163`、`OpenGLDriver.cpp:1482/1512`）但无 HALF 数据通路；DFG LUT 与 HDR 环境图需要扩 API（PixelBufferDescriptor 增加 type 字段）。

### 3.3 Gap 清单（对照 filament）

| # | Gap | 规模评估 |
|---|---|---|
| G1 | Cook-Torrance BRDF（D/V/F + dispatch）缺失 | ~400 行 GLSL chunk，纯移植 |
| G2 | 材质参数域：metallic/roughness/F0/reflectance 无载体（现为 weights+exponent） | 新 uniform 集 + glTF 侧字段消费 |
| G3 | IBL 三件套为零：DFG LUT、预滤波 cubemap、SH irradiance | chunk + 资产管线 + RHI 缺口修复 |
| G4 | 点光/聚光/光源管理缺失（itwinjs 只有 sun+portrait） | 可裁剪：CAD 场景用 uniform 数组循环替代 froxel |
| G5 | glTF MR/emissive/occlusion 纹理未读（reader 只有 factor；单 UV 集） | GltfReader 扩展 + 纹理单元重排 |
| G6 | tangent 无属性（anisotropy 需要 `shading_tangentToWorld`，`surface_lighting.fs:116-123`） | 可用导数 TBN 适配（与现有 normal map 同法） |
| G7 | cubemap 面上传/半浮点上传 RHI 缺口 | 小修（见 3.2） |
| G8 | 无曝光/tonemap（filament 全程 pre-exposed HDR + ACES，`PostProcessManager`） | P1 起需决策：IBL 结果 clamp 或引入轻量 tonemap（记 EQUIVALENCE） |
| G9 | frameUniforms/lightsUniforms UBO → DanQing 散 uniform 改写 | 机械改写，桌面 GL 无性能问题 |

## 4. 方案对比

| 维度 | A：chunk 移植进 Surface 路径 | B：filament 前端整体并存 | C：混合（A 的落地形态，管线锁死 itwinjs） |
|---|---|---|---|
| §0/§7/§8 合规 | 高（filament chunk 照移植，管线路线不变） | 低（引入整套 FEngine/FrameGraph/Entity/job-system，与 dqBase 容器/所有权模型双轨） | 高 |
| MRT featureId 拾取 | AssignFragData 照常写，天然共存 | filament 无 featureId 概念，PCK 变体语义不同（`Variant.h:105`），需自研 | 同 A，不动 |
| WBOIT OIT | PBR 颜色直接喂 accumulation（权重公式与光照无关 `OitShaders.h:148`） | filament 为简单 forward blend，无 WBOIT——半透明输出与既有管线不可比 | 同 A |
| edge/silhouette pass | 独立 technique，零影响 | 需在 filament 侧重建 | 零影响 |
| 大 tile 场景性能 | 每片元 ALU ×3-5 + IBL 2 采样；**按材质分流**（普通 Mesh 走旧 Blinn-Phong，glTF PBR 走新路径），TechniqueId::SurfacePbr 独立注册，无存量场景税 | 双引擎争抢同 GL 上下文，状态跟踪器互相失效（§12.9 类事故） | 同 A |
| IBL 资产 | 离线 cmgen 或运行时 `libs/ibl` 移植二选一 | 自带 iblprefilter | 同 A；建议运行时（CubemapIBL::roughnessFilter 纯 CPU/图像库，无 GL 依赖） |
| 变体管理 | DanQing 已有 MultiVariantTechnique/SurfaceVariantCompiler；特性位 = `MATERIAL_HAS_*` 风格 define，program cache 按 feature-key | filament 61 变体体系整体搬入，与现有 pick/nopick 正交冲突 | 同 A，feature-key cache |
| 直接光/阴影 | 一期复用 u_sunDir + EVSM（visibility 输入替换 `surface_light_directional.fs:50-90` 的 shadow hook） | filament CSM/VSM 体系 | 同 A |
| 工程量 | 中（chunk 移植 + 接线 + RHI 小修） | 极大且 architecturally 否决 | 中 |
| 主要风险 | 特性组合 program 数膨胀；HDR 无 tonemap | 架构违规、双上下文状态污染 | 同 A（已在路线图缓解） |

**推荐：方案 C**（以方案 A 为实现形态，但明确"管线编排层一行不改"的边界——SceneCompositor 24 步、MRT、WBOIT、合成 7 变体、edge pass 全部冻结，filament 仅作为"材质/光照数学库"进入 dqRender/src/shader/ 与新的 `PbrShaders.h` 族）。这一形态同时满足：§0 来源铁律（chunk 逐行移植 + `// Ported from:`）、§5 测试保真（PBR 场景在 itwinjs 无对应测试 → 按 §5(f) Authored 标注 + §11.11 位置断言资产）、§8 依赖方向（零新依赖）。

## 5. 分阶段路线图

### P0 — 最小 PBR（metallic-roughness + 方向光 + 常量 ambient）
- **着色器**：移植 `surface_brdf.fs` + `surface_shading_model_standard.fs` + `surface_light_directional.fs`（去 shadow hook）为 `PbrShaders.h` 常量；新增 `FragmentShaderComponent::ApplyPbrLighting` 替换 ApplyLighting slot（`ShaderBuilder.cpp:915` 链位）。
- **uniform**：`u_metallicFactor/u_roughnessFactor/u_baseColorFactor` + `u_pbrSunDir/u_pbrSunColor`；无 IBL 时 `energyCompensation=1`、`dfg=解析近似`（记 EQUIVALENCE）。
- **glTF 侧**：GltfReader 读 `metallicRoughnessTexture`（MR 合图 G=B 通道约定）；GltfDecoration/PolyfaceGraphic 增加 MR 纹理句柄与 sampler（TextureUnit 14，需扩 `RenderFlags.h` 表并重排 draping 占用）。
- **technique**：`TechniqueId::SurfacePbr`，RenderPipeline 按 geometry 材质类型选 Surface/SurfacePbr。
- **风险**：① 新光照路径无 itwinjs 参考测试 → §5(f) Authored + BoxTexturedDots 式不对称资产锁像素；② Blinn-Phong→GGX 视觉差异大，旧 glTF 用例基线需重录。

### P1 — IBL（DFG LUT + 预滤波 cubemap + SH irradiance）
- **着色器**：移植 `surface_light_indirect.fs` 的 `IBL_INTEGRATION_PREFILTERED_CUBEMAP` 路径（`evaluateIBL:717-839` + SH irradiance `:43-63`）。
- **RHI 修复**：cubemap 面 target 分发（`OpenGLDriver.cpp:1647`）；PixelBufferDescriptor 增加 type 字段支持 HALF 上传。
- **资产管线**：移植 `libs/ibl`（CubemapIBL::roughnessFilter + CubemapSH::computeSH，纯图像代码无 GL 依赖）；DFG LUT 128×128 RGB16F 离线生成 C 数组（照 filament `generated/resources/dfg.h` 模式）。
- **SkyBox 对接**：dqCommon `Environment.sky` 纹理 → equirect → cubemap + SH。
- **风险**：① HDR 无 tonemap 溢出 → 输出 clamp 或引入轻量 ACES（与 filament `PostProcessManager` 对齐取值，EQUIVALENCE 登记）；② 半浮点 RHI 改动影响 OIT RGBA16F 既有路径，需回归 OIT 用例。

### P2 — 特性层（clearcoat → sheen → transmission → iridescence → anisotropy）
- 每特性一个编译期 define 位（filament `MATERIAL_HAS_*` 同构），program cache 按 feature-key；**组合数 = 实际材质种数**而非笛卡尔积（gltfio 同款策略）。
- transmission/refraction 先走 `REFRACTION_MODE_CUBEMAP`（`surface_light_indirect.fs:537-540`，无需 SSR）；anisotropy 用导数 TBN 适配（替代 `shading_tangentToWorld` 属性矩阵，G6）。
- **纹理单元预算重排**：baseColor(0)/normal(13)/MR(14)/emissive(15)/occlusion(16)/clearcoat(+1)/sheen(+1) + DFG LUT + IBL specular + shadowmap——16 单元上限内需统一重排 `RenderFlags.h` TextureUnit 表（桌面 GL 4.1 保证 16，实际硬件 32，留 headroom）。
- **风险**：program 缓存膨胀（需上限 + LRU，参照 `MaterialCache.cpp`）；组合回归矩阵爆炸（按特性单测 + 组合冒烟）。

### P3 — 直接光扩展与阴影整合
- 点/聚光：uniform 数组（≤8）循环替代 froxel（`surface_light_punctual.fs` 的 Light 结构保留、getFroxel* 裁剪）——CAD 场景光源密度低，froxel 不划算。
- 阴影：现有 EVSM solar shadow 作为 directional visibility 填入（替换 `surface_light_directional.fs:50-90` 的 shadow 采样段，来源 `SolarShadowShaders.h` 的 shadowMapEVSM）。
- SPECULAR_GLOSSINESS 遗留模型最低优先（glTF 已 deprecated）。
- **风险**：多光源 uniform 上限与 itwinjs `u_lightSettings[16]`  packing 的并存策略需定契约；阴影可见性接入点的像素回归（位置断言：阴影落侧）。

---

**关键文件索引**（均为绝对路径）：
- filament chunk 源：`D:\Github\filament\shaders\src\`（surface_brdf.fs / surface_shading_model_standard.fs / surface_shading_lit.fs / surface_light_indirect.fs / surface_light_directional.fs / surface_light_punctual.fs / surface_material_inputs.fs / surface_lighting.fs / surface_main.fs）
- filament IBL：`D:\Github\filament\filament\src\DFG.cpp`、`D:\Github\filament\libs\ibl\include\ibl\`、工具 `D:\Github\filament\tools\cmgen`、`D:\Github\filament\libs\iblprefilter`
- filament 编译/变体：`D:\Github\filament\libs\filamat\src\MaterialBuilder.cpp`、`D:\Github\filament\libs\filabridge\include\private\filament\Variant.h`、`D:\Github\filament\libs\filabridge\include\filament\MaterialEnums.h`
- DanQing 光照/材质现状：`D:\Github\DanQing\dqRender\src\render\LightingShaders.h`、`D:\Github\DanQing\dqRender\src\shader\SurfaceMaterialShaders.h`、`D:\Github\DanQing\dqRender\src\render\SceneCompositorImpl.cpp`、`D:\Github\DanQing\dqRender\src\render\CompositorFrameBuffers.h`、`D:\Github\DanQing\dqRender\src\shader\OitShaders.h`、`D:\Github\DanQing\dqRender\src\render\TechniqueImpl.h`
- DanQing glTF 消费面：`D:\Github\DanQing\dqRender\PublicAPI\dqRender\GltfReader.h`（:57-58 factor 字段闲置）、`D:\Github\DanQing\dqApp\src\GltfDecoration.cpp`
- DanQing RHI 缺口：`D:\Github\DanQing\dqRender\src\rhi\opengl\OpenGLDriver.cpp`（:1457 cubemap target、:1594-1599 storage、:1647-1650 面分发缺口、:1646/1649 类型硬编码）

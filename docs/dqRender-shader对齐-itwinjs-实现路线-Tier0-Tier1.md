// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Surface.ts
//              core/frontend/src/internal/render/webgl/glsl/Composite.ts
//              core/frontend/src/internal/render/webgl/glsl/Translucent.ts
//              core/frontend/src/internal/render/webgl/glsl/Edge.ts
//
// 本文档是 dqRender shader 对齐 itwinjs-core 的实现路线规划（Tier 0 + Tier 1）。
// Authored: 2026-07-12 调研沉淀，作为后续多会话工程指南。

# dqRender Shader 对齐 itwinjs-core 实现路线（Tier 0 + Tier 1）

## 0. 背景与现状

### 0.1 已完成（commit da26098）
- **A 阶段**：全量 `#version 330 core` → `#version 410 core`（ShaderBuilder 默认值 + 15 setVersion + 75 R"glsl + 16 src+= + 4 Edge 内联 + 6 Compositing + 5 测试），对齐 macOS GL 4.1 context，不引入 4.2+ 特性。
- **死代码清理**：删除 11 个零引用文件（glsl/ 7 + SurfaceShaders/SurfaceShadersFull/EdgeShadersFull/SilhouetteEdgeShaders）。
- 验证：1889 测试全绿。

### 0.2 渲染层 shader 保真度现状（对照 itwinjs glsl/*.ts 全量审计）
- **高保真（13）**：Copy*/Clear*/Combine*/Decode/RenderPass/MaplayerDraping/Wiremesh/Atmosphere/Translucency/4 个未连接的 Surface 片段/SkySphere 主体。
- **中漂移（12）**：Polyline/CopyStencil/SolarShadow/RealityMesh/Color(缺 Monochrome)/Clipping(缺 g_clipColor)/EVSMFromDepth/LogDepth/LookupTable/ViewportQuad/OitShaders/SkySphere 连接侧。
- **低漂移（14，含核心路径）**：SurfaceVariantCompiler/EdgeVariantCompiler/Composite/OitShaders(合成)/VolumeClass/Sky/AmbientOcclusion/PostProcess/PointCloud/SurfaceShadersFull(已删)/EdgeShadersFull(已删)/SilhouetteEdge(已删)/PlanarGrid/GeometryShaderBuilders。

> **保真度 ~31%。Tier 0/1 解决核心渲染路径（Surface + Composite/OIT/Edge），完成后预计升至 ~55%。**

### 0.3 关键调研结论（避免下次重新调研）

1. **两条 Surface 构建路径都是简化版**：
   - `dqRender/src/render/SurfaceVariantCompiler.cpp`（主路径，被 `MultiVariantTechnique` 注册）：Lighting 只有 Lambert，无 `u_surfaceFlags[]`、无 octDecodeNormal、无材质/纹理系统。已 include `FeatureSymbologyShaders.h`/`FeatureEffectShaderBuilders.h`/`CommonShaders.h`。
   - `dqRender/src/render/shader/SurfaceShaderBuilder.cpp`（第二条路径）：同样简化，且**自创**了 `computeLighting`/`applyFog`/`getFeatureColor`/`encodeFeatureId` 等非 itwinjs 函数（违反 §0 来源铁律，需清理）。

2. **4 个高保真 Surface 片段零引用、无 C++ 组装函数**：
   - `dqRender/src/shader/SurfaceFlagsShaders.h`（159 行）/ `SurfaceNormalShaders.h`（112）/ `SurfaceTextureShaders.h`（116）/ `SurfaceMaterialShaders.h`（193）。
   - 只提供 `inline constexpr std::string_view` GLSL 字符串片段，**没有** itwinjs 对应的 C++ 组装函数（`addSurfaceFlags`/`addNormal`/`addTexture`/`addMaterial`）。
   - 全部 `#include` 引用为零。

3. **"连接 4 片段" = 重建 Surface 子系统**：片段依赖一整套在 DanQing 完全缺失的基础设施（见下表）。`agent 初评"几乎不用写新 GLSL"严重低估`。

| 层 | 缺失内容 |
|---|---|
| C++ 组装层 | itwinjs `addSurfaceFlags/addNormal/addMaterial/addTexture` 的 C++ 等价物 |
| GLSL 契约层 | `u_surfaceFlags[12]` 数组、`MAT_MV`/`MAT_NORM` 宏、`g_vertLutData*` LUT、`mat_rgb/alpha`/`use_material`/`g_materialParams`/`g_surfaceTexel` globals、`chooseVec3WithBitFlag`/`unquantize2d`/`unpackAndNormalize2Bytes`/`computeLUTCoords`/`decodeUInt16` 辅助 |
| uniform 绑定 C++ | 每个 uniform 的 bind 回调（从 geometry/target 读数据上传） |
| ⭐ **geometry 数据供给** | `SurfaceGeometry.computeSurfaceFlags()`/`materialInfo`/`texture`/`normalMap`/`lut.uvQParams` —— 依赖 **GeometryAccumulator 在累积 mesh 时存储这些数据** |

4. **最后一点是关键阻塞**：shader 声明的 uniform 若 GeometryAccumulator 没存对应数据，uniform 无值可绑 → 渲染错误。Tier 0 是跨 dqRender（shader）+ GeometryAccumulator（数据供给）的工程。

---

## 1. itwinjs 权威源索引（Surface.ts 行号，已核对）

文件：`../itwinjs-core/core/frontend/src/internal/render/webgl/glsl/Surface.ts`

| 函数/常量 | 行号 | 用途 |
|---|---|---|
| `createSurfaceBuilder` | 725-835 | **总入口**，组件组装顺序权威 |
| `createSurfaceHiliter` | 298-312 | 高亮 shader |
| `createCommon` | 265-295 | 投影/MV 矩阵 + frustum + v_eyeSpace + ComputePosition |
| `addSurfaceFlags` | 507-527 | **Step 1** u_surfaceFlags[] 系统 |
| `addSurfaceFlagsLookup` | 319-351 | 常量 + isSurfaceBitSet + surfaceFlags global |
| `isSurfaceBitSet` | 314-316 | GLSL helper |
| `initSurfaceFlags` | 353-358 | 顶点端 flags 初始化 |
| `computeBaseSurfaceFlags` | 360-369 | feature_ignore_material 分支 |
| `computeColorSurfaceFlags` | 372-375 | OverrideRgb 分支 |
| `returnSurfaceFlags` | 377 | varying 输出 |
| `addNormal` | 529-566 | **Step 2** 法线系统 |
| `octDecodeNormal` | 383-394 | 八进制解码 |
| `getComputeNormal` | 396-406 | 量化/非量化法线读取 |
| `finalizeNormalPrelude/NormalMap/Postlude` | 408-446 | 含 normal map TBN（dFdx/dFdy） |
| `addTexture` | 571-687 | **Step 3** 纹理系统（含 6 层 maplayer draping） |
| `constantLodTextureLookup` | 49-75 | constant LOD 纹理查找 |
| `sampleSurfaceTexture` | 78-89 | 纹理采样入口 |
| `getComputeTexCoord` | 460-467 | 量化 texCoord 解码 |
| `getSurfaceColor` | 478-480 | `return v_color` |
| `computeBaseColor` | 486-502 | glyph + whiteOnWhite 反转 |
| `addMaterial` | 183-238 | **Step 4** 材质系统 |
| `applyMaterialColor` | 91-96 | 材质颜色应用 |
| `applyTextureWeight` | 103-112 | 纹理权重混合 |
| `decodeFragMaterialParams` | 114-124 | 材质参数解码 |
| `decodeMaterialColor` | 126-131 | 材质颜色解码 |
| `computeMaterialParams` | 134-137 | 默认值 (0x6699,0xffff,0xffff,13.5) |
| `readMaterialAtlas` | 140-167 | 材质 atlas LUT 读取 |
| `computeMaterial/Instanced` | 169-181 | atlas 或 uniform 分支 |

**createSurfaceBuilder 组件组装顺序（L725-835，Tier 0 实现顺序权威）：**
```
createCommon → addShaderFlags → addFeatureSymbology → addSurfaceFlags →
addSurfaceDiscard → addNormal → FinalizeBaseColor=applyBackgroundColor →
addTexture → addColor → getSurfaceColor → addLighting → addWhiteOnWhiteReversal →
addApplyContours → addApplySurfaceDraping → (addTranslucency | pick outputs) →
addSurfaceMonochrome → addMaterial → addWiremesh
```

---

## 2. Tier 0: Surface 子系统重建

### 实现策略
- **不**在 `SurfaceVariantCompiler.cpp`（主路径）原地扩展，而是**新建 `SurfaceBuilder.cpp`** 实现 itwinjs `createSurfaceBuilder` 的 C++ 等价物（对齐组件组装顺序），复用 4 个高保真片段的 GLSL。
- 完成后让 `SurfaceVariantCompiler` 委托给新 builder（或直接替换主路径注册）。
- `SurfaceShaderBuilder.cpp` 的自创函数（`computeLighting`/`applyFog`/`getFeatureColor`/`encodeFeatureId`）**删除**——它们违反 §0 来源铁律。

### 跨步骤前置依赖
所有 4 步都依赖 **Step 1（addSurfaceFlags）**，因为后续组件用 `u_surfaceFlags[kSurfaceBitIndex_*]` 驱动分支。geometry 数据供给（GeometryAccumulator 扩展）是所有步骤的并行前置。

---

### Step 1: addSurfaceFlags 基础设施

**itwinjs 源**：`Surface.ts:507-527`（addSurfaceFlags）+ `319-351`（addSurfaceFlagsLookup）+ `314-377`（isSurfaceBitSet/initSurfaceFlags/computeBaseSurfaceFlags/computeColorSurfaceFlags/returnSurfaceFlags）。

**DanQing GLSL 已移植**：`SurfaceFlagsShaders.h` 提供 `kSurfaceFlagsIndexConstants`/`kSurfaceFlagsBitConstants`/`kSurfaceFlagsHelperFunctions`/`kSurfaceFlagsGlobal`/`kInitSurfaceFlags`/`kComputeBaseSurfaceFlags`/`kComputeColorSurfaceFlags`/`kReturnSurfaceFlags`/`kUnpackSurfaceFlags`（9 个 string_view 片段）。

**C++ 组装层要做**（对齐 addSurfaceFlags L507-527）：
1. vert + frag 都调 `addSurfaceFlagsLookup`：注入 `kSurfaceFlagsIndexConstants`（12 个 kSurfaceBitIndex_*）+ `kSurfaceFlagsBitConstants`（kSurfaceBit_*/kSurfaceMask_*）+ `kSurfaceFlagsHelperFunctions`（isSurfaceBitSet + nthBitSet）+ `kSurfaceFlagsGlobal`（`uint surfaceFlags`）。
2. 声明 `u_surfaceFlags` uniform **bool 数组**（长度 = SurfaceBitIndex.Count = 12）。
3. `addFunctionComputedVarying("v_surfaceFlags", Float, "computeSurfaceFlags", compute)`，其中 `compute = initSurfaceFlags + computeBaseSurfaceFlags(withFeatureOverride) + [computeColorSurfaceFlags(withFeatureColor)] + returnSurfaceFlags`。
4. `frag.addInitializer("surfaceFlags = uint(floor(v_surfaceFlags + 0.5));")`。

**uniform 绑定**：`u_surfaceFlags[12]` ← `geometry.asSurface.computeSurfaceFlags(params)` 返回 `Int32Array(12)`。

**⭐ geometry 数据供给（GeometryAccumulator 扩展）**：
- `SurfaceBitIndex` 12 个标志（参考 itwinjs `RenderFlags.ts SurfaceBitIndex`）：HasTexture(0)/ApplyLighting(1)/HasNormals(2)/IgnoreMaterial(3)/TransparencyThreshold(4)/BackgroundFill(5)/HasColorAndNormal(6)/OverrideRgb(7)/HasNormalMap(8)/HasMaterialAtlas(9)/UseConstantLodTextureMapping(10)/UseConstantLodNormalMapMapping(11)。
- GeometryAccumulator 在累积 mesh 时，根据 mesh 属性设置这 12 个 bool（如 mesh 有纹理 → HasTexture=true；有法线 → HasNormals=true；ViewFlags.lighting → ApplyLighting）。
- 需新增 `SurfaceGeometry::computeSurfaceFlags()` C++ 方法。

**可验证点**：
- 编译 dqRender 通过。
- 生成的 shader 字符串含 `uniform bool u_surfaceFlags[12]` + `float v_surfaceFlags` + `uint surfaceFlags` + `isSurfaceBitSet`。
- 新增测试：`SurfaceFlagsShaderTest`（对齐 `SurfaceFlagsShaders.h` 的 GLSL 输出 + uniform 绑定）。

---

### Step 2: addNormal

**itwinjs 源**：`Surface.ts:529-566`（addNormal）+ `383-394`（octDecodeNormal）+ `396-406`（getComputeNormal）+ `408-446`（finalizeNormal*）。

**DanQing GLSL 已移植**：`SurfaceNormalShaders.h` 提供 octDecodeNormal + getComputeNormal(quantized/non-quantized) + finalizeNormalPrelude/NormalMap/Postlude。**缺失**：动画法线路径（`getComputeAnimatedNormal`）。

**C++ 组装层**（对齐 addNormal L529-566）：
1. `addNormalMatrix(vert)` → 声明 `u_normalMatrix` (Mat3) uniform，定义 `MAT_NORM` 宏 = `u_normalMatrix`。
2. `vert.addFunction(octDecodeNormal)`。
3. `vert.addFunction("vec3 computeSurfaceNormal()", getComputeNormal(quantized))` —— quantized 分支用 `g_vertLutData3.xy/g_vertLutData1.zw`，非 quantized 用 `g_vertLutData4.zw/g_vertLutData5.xy`（依赖 LUT 系统，见 Step 3 前置）。
4. `addFunctionComputedVarying("v_n", Vec3, "computeLightingNormal", "return computeSurfaceNormal();")`。
5. `frag.addGlobal("g_normal", Vec3)`。
6. `frag.set(FinalizeNormal, finalizeNormalPrelude + finalizeNormalNormalMap + finalizeNormalPostlude)`。
7. `frag.addFunction(constantLodTextureLookup)` + `u_normalMapScale` uniform。

**uniform 绑定**：
- `u_normalMatrix` ← target/geometry normal matrix（`mat3(mv)` 的 inverse-transpose，或 itwinjs 的 `addNormalMatrix` 实现见 `Vertex.ts`）。
- `u_normalMapScale` ← `materialInfo.textureMapping.normalMapParams.{scale,greenUp}`。

**⭐ geometry 数据供给**：
- 非 quantized：`a_normal` attribute（GeometryAccumulator 已有 normal 通道）。
- quantized：法线存在顶点 LUT（`g_vertLutData*`），需 GeometryAccumulator 的 LUT 打包路径（见 Step 3 的 LUT 系统）。
- normalMap：`materialInfo.textureMapping.normalMapParams`。

**依赖**：Step 1（`u_surfaceFlags[kSurfaceBitIndex_HasNormals/HasColorAndNormal/HasNormalMap]`）。

**可验证点**：shader 含 `vec3 octDecodeNormal(vec2)` + `v_n` varying + `u_normalMatrix`；编译 + 现有 lighting 测试不退化。

---

### Step 3: addTexture

**itwinjs 源**：`Surface.ts:571-687`（addTexture）+ `49-75`（constantLodTextureLookup）+ `78-89`（sampleSurfaceTexture）+ `460-467`（getComputeTexCoord）+ `478-480`（getSurfaceColor）+ `486-502`（computeBaseColor）。

**DanQing GLSL 已移植**：`SurfaceTextureShaders.h` 提供 unquantize2d + constantLodTextureLookup + sampleSurfaceTexture + getSurfaceColor + computeBaseColor（glyph + whiteOnWhite）+ getComputeTexCoord。**缺失**：动画 texCoord 路径、maplayer 6 层 draping uniforms（`s_texture0..5`/`u_texParams*/u_texMatrix*`）。

**C++ 组装层**（对齐 addTexture L571-687）：
1. `vert.addFunction(unquantize2d)` + `addChooseVec2WithBitFlagsFunction(vert)`。
2. `addFunctionComputedVarying("v_texCoord", Vec2, "computeTexCoord", getComputeTexCoord(quantized))`。
3. `u_qTexCoordParams` (Vec4) uniform ← `surfGeom.lut.uvQParams`。
4. `s_texture` sampler 绑定 ← `surfGeom.texture`（或 thematic texture）。
5. `s_normalMap` sampler 绑定 ← `surfGeom.normalMap`。
6. `frag.addFunction(constantLodTextureLookup)` + `frag.addFunction(sampleSurfaceTexture)`。
7. `frag.addGlobal("g_surfaceTexel", Vec4)`。
8. `frag.set(ComputeBaseColor, computeBaseColor)`。
9. `u_applyGlyphTex` / `u_reverseWhiteOnWhite` uniforms。

**⭐ geometry 数据供给 + LUT 系统（本步骤最重）**：
- `lut.uvQParams`：texture coord 量化参数（Vec4），GeometryAccumulator 的 LUT 打包路径需提供。
- `surfGeom.texture`：`Material.textureMapping.textureMap.texture`。
- `surfGeom.useTexture()`：判断是否有有效纹理。
- `g_vertLutData*`：quantized 模式下顶点数据（位置/法线/texCoord/color/material）打包进 LUT 纹理。这是 itwinjs 的 `VertexLUT` 系统（见 `Vertex.ts`/`VertexTable.ts`），DanQing 需对应实现。**注意**：当前 DanQing SurfaceVariantCompiler 的 quantized 路径用的是简化 LUT（只位置），完整 LUT 系统是 Step 3 的隐性大头。

**依赖**：Step 1（`u_surfaceFlags[kSurfaceBitIndex_HasTexture/UseConstantLodTextureMapping]`）。

**可验证点**：shader 含 `sampleSurfaceTexture` + `v_texCoord` + `s_texture`；纹理 mesh 渲染正确（需视觉验证或参考测试）。

---

### Step 4: addMaterial

**itwinjs 源**：`Surface.ts:183-238`（addMaterial）+ `91-181`（GLSL 字符串）。

**DanQing GLSL 已移植**：`SurfaceMaterialShaders.h` 提供 decodeMaterialColor + decodeFragMaterialParams + computeMaterial(Instanced) + applyMaterialColor + applyTextureWeight + readMaterialAtlas + unpackFloat/unpack2Bytes。全部高保真（agent 评估确认）。

**C++ 组装层**（对齐 addMaterial L183-238）：
1. `frag.addGlobal(mat_texture_weight/mat_weights/mat_specular)`。
2. `addUnpackAndNormalize2Bytes(frag)` + `frag.addFunction(decodeFragMaterialParams)` + `frag.addInitializer("decodeMaterialParams(v_materialParams);")`。
3. `addChooseVec3WithBitFlagFunction(frag)` + `frag.set(ApplyMaterialOverrides, applyTextureWeight)`。
4. `vert.addGlobal(mat_rgb/mat_alpha/use_material)` + `addInitializer("use_material = !u_surfaceFlags[kSurfaceBitIndex_IgnoreMaterial];")`。
5. `vert.addFunction(decodeMaterialColor)` + `u_materialColor`(Vec4) + `u_materialParams`(Vec4) uniforms。
6. 非 instanced：`vert.addFunction(unpackFloat)` + `readMaterialAtlas` + `u_numColors` uniform。
7. `vert.addGlobal(g_materialParams)` + `vert.set(ComputeMaterial, computeMaterial|computeMaterialInstanced)` + `vert.set(ApplyMaterialColor, applyMaterialColor)`。
8. `addFunctionComputedVarying("v_materialParams", Vec4, "computeMaterialParams", computeMaterialParams)`。

**uniform 绑定**：
- `u_materialColor` ← `materialInfo.rgba`（或 `Material.default`）。
- `u_materialParams` ← `materialInfo.fragUniforms`。
- `u_numColors` ← `materialInfo.vertexTableOffset`（atlas 模式）。

**⭐ geometry 数据供给**：
- `materialInfo`：`Material` 对象（含 `rgba`/`fragUniforms`/`isAtlas`/`vertexTableOffset`/`textureMapping`）。GeometryAccumulator 需在累积 mesh 时存储 materialInfo（当前可能部分支持，需核对 `GeometryAccumulator` 的 material 路径）。
- `wantMaterials(viewFlags)`：根据 ViewFlags.materialStyles 决定。

**依赖**：Step 1（`u_surfaceFlags[kSurfaceBitIndex_IgnoreMaterial/HasMaterialAtlas]`）+ Step 3（`g_surfaceTexel` from texture）。

**可验证点**：shader 含 `decodeMaterialColor` + `u_materialColor` + `v_materialParams`；材质 mesh 渲染正确。

---

### Tier 0 收尾
- 替换主路径：`MultiVariantTechnique` 的 Surface 注册从 `SurfaceVariantCompiler` 切到新 `SurfaceBuilder`（或 SurfaceVariantCompiler 委托）。
- 删除 `SurfaceShaderBuilder.cpp` 的自创函数（`computeLighting`/`applyFog`/`getFeatureColor`/`encodeFeatureId`）。
- **addLighting**：Step 1-4 完成后，`ApplyLighting` 组件仍需对齐 itwinjs `Lighting.ts`（完整光照模型：阳光/环境/半球/镜像/材质 specular）。当前只有 Lambert。这是 Tier 0 的第 5 个子步骤（可选，本次规划范围）。

---

## 3. Tier 1: Composite / OIT / Edge 核心合成修正

### Step 1: CompositeShaders.h — WBOIT 合成修正 + 高亮轮廓

**itwinjs 源**：`Composite.ts:1-201`。

**现状问题**（agent 评估确认）：
1. **WBOIT 合成分母错误**：当前除以 `accum.a`（权重和），应除以 `revealage.r`（透射率）。
2. **revealage 输出错**：当前输出 `alpha`，应为 `1-alpha`。
3. **缺失 `u_clipIntersection`**。
4. **高亮轮廓完全缺失**：3×3 + ring-2 邻域采样、`u_hilite_settings`(Mat3)、`u_hilite_width`(Vec2)、`computeNearbyHilites`、emphasis-vs-hilite 顺序。
5. **AO 嵌套错误**：当前 AO 嵌在合并 pass，itwinjs 是独立 pass。
6. **捆绑错误**：CopyColor/Pick/EVSM shader 不该在 CompositeShaders.h（应各自独立，对齐 itwinjs 的 CopyColor.ts/CopyPickBuffers.ts/EVSMFromDepth.ts）。

**实现**：重写 CompositeShaders.h 对照 Composite.ts。高亮轮廓是最大单项工作（邻域采样逻辑复杂）。

**资源依赖**：hilite buffer texture（来自 Hilite pass）、emphasis texture。

**可验证点**：透明物体合成正确（WBOIT 分母）、高亮物体有轮廓。

---

### Step 2: OitShaders.h — 分母 + revealage + clipIntersection

**itwinjs 源**：`Composite.ts`（消费者）+ `Translucent.ts:1-59`（生产者）。

**现状问题**：
1. 合成分母错（同 Tier 1 Step 1）。
2. revealage 输出错（`1-alpha` vs `alpha`）。
3. 缺 `u_clipIntersection`。
4. **生产者待对照 `Translucent.ts` 审计**（注意：`TranslucencyShaders.h` 是高保真生产者，`OitShaders.h` 是消费者，两者要配套）。

**实现**：OitShaders.h 消费者对齐 Composite.ts 的 WBOIT 合成；确认 TranslucencyShaders.h（高保真）产出正确的 accum/revealage。

**可验证点**：透明合成数值正确（accum/revealage 通道分离）。

---

### Step 3: EdgeVariantCompiler — 索引边缘 LUT + 屏幕空间 quad 展开

**itwinjs 源**：`Edge.ts`（全）+ `Polyline.ts`（复用）。

**现状问题**（agent 评估确认）：当前 EdgeVariantCompiler 是裸 GL_LINES passthrough（`fragColor = v_color`），缺：
1. **索引边缘 LUT**：`u_edgeLUT`/`u_edgeParams`/`computeIndexedQuantizedPosition`/`decodeUInt24`。
2. **屏幕空间 quad 展开**：`modelToWindowCoordinates`/`g_quadIndex`/`g_windowPos`/`g_windowDir`（SegmentEdge/Silhouette/IndexedEdge 三种几何）。
3. 线宽/线型、轮廓剔除（正交/透视测试）、法线矩阵、八进制解码、对比度、动画、feature symbology、pick 输出。

**关键复用**：itwinjs 的 `Edge.ts` 建立在 `Polyline.ts` 之上。`PolylineShaders.h`（已保留，246 行，函数级高保真）已忠实移植 `modelToWindowCoordinates`/`adjustWidth`/`computeLineCodeTextureCoords`/`visknt_*` —— **复用可使 Edge 工作量减半**。

**实现策略**：
1. 先连接 PolylineShaders.h 的忠实函数作为 Edge 的屏幕空间展开基础。
2. 在此之上加 Edge.ts 的索引 LUT + quad 索引 + 轮廓剔除。
3. 删除 `EdgeVariantCompiler` 当前的裸 passthrough。

**geometry 数据供给**：Edge 几何（SegmentEdge/SilhouetteEdge/IndexedEdge）需要 GeometryAccumulator 的 edge 提取路径提供（顶点对 + quad 参数 + 线宽/线型/颜色）。

**可验证点**：线宽可控、轮廓边正确显示、线型（虚线等）正确。

---

## 4. 跨步骤依赖图

```
Tier 0:
  GeometryAccumulator 数据供给扩展（surfaceFlags/materialInfo/texture/normalMap/LUT）
    ├─ Step 1: addSurfaceFlags（前置，所有后续依赖 u_surfaceFlags[]）
    ├─ Step 2: addNormal（依赖 Step 1 + LUT 系统）
    ├─ Step 3: addTexture（依赖 Step 1 + LUT 系统，最重：含 VertexLUT）
    └─ Step 4: addMaterial（依赖 Step 1 + Step 3 的 g_surfaceTexel）
  Step 5（可选）: addLighting 完整光照模型（Lighting.ts）

Tier 1（独立于 Tier 0，可并行）:
  Step 1: CompositeShaders（WBOIT + 高亮轮廓，需 hilite buffer 资源）
  Step 2: OitShaders（配套 TranslucencyShaders 高保真生产者）
  Step 3: Edge（复用 PolylineShaders + Edge.ts LUT + geometry edge 数据）
```

---

## 5. 每步统一验证标准

1. **编译**：`cmake --build build -j` 全量通过（含 dqRender + 所有测试 target）。
2. **测试**：`ctest --test-dir build -j 4` 1889+ 测试全绿，新增 shader 测试通过。
3. **shader 字符串检查**：新增测试用 `EXPECT_NE(source.find("期望的 uniform/varying/函数"), std::string::npos)` 验证生成内容（对齐 BatchClipTest 模式）。
4. **来源溯源**：每个新 C++ 组装函数 + GLSL 片段必须有 `// Ported from: itwinjs-core .../Surface.ts:行号`（§4 代码溯源）。
5. **不引入 4.2+ 特性**：保持 GLSL 410 core + itwinjs 保守子集（§0）。

---

## 6. 后续 Tier 2/3/4 概览（本次规划范围外，来自全量审计）

- **Tier 2（数据类型+环境）**：PointCloud（uniform 契约重写）、Sky（`u_rot`+swizzle 策略）、SolarShadow（重集成为内联组件）、RealityMesh（连接 applyDraping + thematic/hilite 变体）。
- **Tier 3（特效）**：AmbientOcclusion（HBAO 重写：噪声纹理 + 方向内核 + PB/DB 双路径 + logZ）、PostProcess（4 子 shader 重写，EDL 多尺度最大）、Atmosphere（加每顶点路径 + compositor）、VolumeClass（对照 PlanarClassification.ts 从头重写）。
- **Tier 4（局部修补+清理）**：CopyStencil（加 SetBlend）、Color（加 Monochrome.ts）、Clipping（加 g_clipColor）、Polyline（连接忠实片段为默认）、ViewportQuad（删伪 gl_FragDepth）、去重 VolClassCopyZ、LookupTable g_ 前缀核对、EVSMFromDepth 提取共享 warpDepth、LogDepth 签名协调。

---

## 7. 建议执行顺序（多会话）

1. **会话 N+1**：Tier 0 前置——GeometryAccumulator 数据供给扩展（surfaceFlags + LUT 系统）+ Step 1（addSurfaceFlags）。
2. **会话 N+2**：Tier 0 Step 2（addNormal）+ Step 3（addTexture，含 VertexLUT）。
3. **会话 N+3**：Tier 0 Step 4（addMaterial）+ 主路径切换 + SurfaceShaderBuilder 自创函数清理。
4. **会话 N+4**：Tier 1 Step 1（CompositeShaders WBOIT + 高亮轮廓）。
5. **会话 N+5**：Tier 1 Step 2（OitShaders）+ Step 3（Edge，复用 PolylineShaders）。

每个会话以"编译 + 全测试绿 + 该步 shader 字符串验证"为完成标准，独立可提交。

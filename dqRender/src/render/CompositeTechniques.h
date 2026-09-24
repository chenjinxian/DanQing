// Composite technique helpers
// Ported from: itwinjs-core TechniqueId.ts
#pragma once
#include "TechniqueImpl.h"
// 不得 include ../gl/Technique.h——gl/ 与 render/ 的 TechniqueFlags/Technique/
// TechniqueId/Techniques 是同名不同布局的 ODR 双定义（gl 版：int numClipPlanes +
// enum 包装 + bool usesQuantizedPositions；render 版：uint8_t + PositionType），
// 曾使同一 dqRenderTest.exe 内两套内联定义竞争 COMDAT，链接器任选其一后跨 TU
// 布局错配（Debug 崩溃家族）。render/ 宇宙（TechniqueImpl.h）为唯一权威。

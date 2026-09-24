// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — 默认 technique 全集注册工厂
// Authored: 从 RenderPipeline::initialize 抽出（渲染管线与渲染独立验证测试共用
//           同一注册路径，避免两处漂移）
#pragma once

#include <memory>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class Techniques;
namespace rhi {
class Driver;
}

/// 创建并注册全部渲染 technique（PlanarGrid/Surface/Polyline/Edge/后处理/天空/
/// 合成/点云/体分类…），全部 compileShaders。失败容忍：单项编译失败不阻断注册。
std::unique_ptr<Techniques> createDefaultTechniques(rhi::Driver& driver);

END_DQ_RENDER_NAMESPACE

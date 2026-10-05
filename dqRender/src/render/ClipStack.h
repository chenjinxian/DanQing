// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ClipStack (view clip + nested branch clip 栈)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ClipStack.ts
//
// M-P P-C：按参考语义整体重写（原简化面[平铺拼接 + buildCombinedTextureData]
// 移除——BatchClipTest 的 Authored 锁随换为参考测试移植）。
// §3.4 适配：
//  - Uint8Array/Float32Array 双视图 → 单一 vector<uint8_t>（上传时按 float 视图
//    解释——两视图本就共享内存，参考测试的视图同一性断言以 cpuBuffer 承载）；
//  - 纹理创建经注入的 rhi::Driver*（参考 Texture2DHandle 静态 GL 门面；测试子类
//    覆写 uploadTexture 虚槽免 GL——同参考测试的覆写点）；
//  - emptyClip 单例哨兵 → 空 RefPtr 判空（同一性比较语义等价）；
//  - allocateGpuBuffer 记账（参考测试的实例字段覆写）无 C++ 对应——不入虚槽。
#pragma once

#include "ClipVolume.h"

#include <dqCommon/ClipStyle.h>
#include <dqCommon/RgbColor.h>
#include <dqGeom/ClipVector.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>
#include <dqRender/rhi/Driver.h>
#include <dqRender/rhi/Handle.h>

#include <cstdint>
#include <functional>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#endif
#ifndef END_DQ_RENDER_NAMESPACE
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

/// Float color components（[r,g,b,a] 0..1；tbgr 追踪 + setRgbColor 的 alpha=1
/// 语义——交线风格将 alpha 复用为线宽，参考怪癖 1:1）。
/// Ported from: itwinjs-core FloatRgba (FloatRGBA.ts subset——ClipStack 颜色面)
struct ClipStackFloatRgba {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;

    /// Ported from: FloatRgba.setTbgr (FloatRGBA.ts:46-53) —— 分量 = c/255、
    /// alpha = 1 - t/255（tbgr 的 t 段为透明度）。
    void setTbgr(uint32_t tbgr);
    /// Ported from: FloatRgba.setRgbColor (FloatRGBA.ts:42-44)
    void setRgbColor(dqCommon::RgbColor const& rgb);
};

/// Maintains a stack of ClipVolumes. The volumes nest such that the stack
/// represents the intersection of all the volumes. The bottom of the stack
/// represents the view's clip volume and is always present even if the view
/// has no clip. Also maintains the inside/outside clip colors (alpha=1 →
/// color applied).
/// Ported from: itwinjs-core ClipStack (ClipStack.ts:44-283)
class ClipStack {
public:
    /// Ported from: ClipStack ctor (:73-80) —— getTransform = 视矩阵来源、
    /// wantViewClip = 栈底视图 clip 是否生效（_viewClipEnabled && viewFlags）。
    ClipStack(std::function<dqGeom::Transform const&()> getTransform,
              std::function<bool()> wantViewClip);

    /// 生产上传通道（参考经 Texture2DHandle 全局 GL；DanQing 注入 driver）。
    void setDriver(rhi::Driver* driver) noexcept { m_driver = driver; }

    /// Ported from: ClipStack.insideColor (:82-84)
    ClipStackFloatRgba& insideColor() noexcept { return m_insideColor; }
    /// Ported from: ClipStack.outsideColor (:86-88)
    ClipStackFloatRgba& outsideColor() noexcept { return m_outsideColor; }
    /// Ported from: ClipStack.hasOutsideColor (:90-92)
    bool hasOutsideColor() const noexcept { return m_outsideColor.a != 0.0f; }
    /// Ported from: ClipStack.colorizeIntersection (:94-100)
    bool colorizeIntersection() const noexcept { return m_colorizeIntersection; }
    void setColorizeIntersection(bool b) noexcept { m_colorizeIntersection = b; }
    /// Ported from: ClipStack.intersectionStyle (:101-103) —— alpha 段复用为线宽。
    ClipStackFloatRgba& intersectionStyle() noexcept { return m_intersectionStyle; }
    /// Ported from: ClipStack.bytesUsed (:105-107)
    size_t bytesUsed() const noexcept;

    /// Ported from: ClipStack.setViewClip (:109-149) —— 恒等短路（几何同一性）/
    /// 颜色与交线风格更新 / 视矩阵变化置脏 / createClipVolume 失败回退空。
    void setViewClip(dqGeom::ClipVector const* clip, dqCommon::ClipStyle const& style);

    /// Ported from: ClipStack.push (:151-158)
    void push(ClipVolume::Ptr clip);
    /// Ported from: ClipStack.pop (:160-164)
    void pop();

    /// Ported from: ClipStack.hasClip (:166-168)
    bool hasClip() const noexcept { return startIndex() < endIndex(); }
    /// Ported from: ClipStack.hasViewClip (:170-172)
    bool hasViewClip() const noexcept { return m_stack[0].IsValid() && m_wantViewClip(); }
    /// Ported from: ClipStack.startIndex (:174-177)
    uint32_t startIndex() const noexcept { return m_wantViewClip() ? 0 : stack0NumRows(); }
    /// Ported from: ClipStack.endIndex (:179-181)
    uint32_t endIndex() const noexcept { return m_numRowsInUse; }
    /// Ported from: ClipStack.textureHeight (:183-185)
    uint32_t textureHeight() const noexcept { return m_numTotalRows; }
    /// Ported from: ClipStack.texture (:187-190)
    rhi::TextureHandle texture();

    /// Ported from: ClipStack.isRangeClipped (:192-207)
    bool isRangeClipped(dqGeom::Range3d range, dqGeom::Transform const& transform);

    /// Exposed strictly for tests.（:209-212）
    std::vector<ClipVolume::Ptr> const& clips() const noexcept { return m_stack; }
    /// 空栈底判别（参考 emptyViewClip 单例同一性 → 空 Ptr 判空，§3.4）。
    static ClipVolume::Ptr emptyViewClip() noexcept { return ClipVolume::Ptr{}; }

    // --- test observability（参考测试的 protected 暴露面对应物）---
    std::vector<uint8_t> const& cpuBuffer() const noexcept { return m_cpuBuffer; }
    uint32_t numTotalRows() const noexcept { return m_numTotalRows; }
    uint32_t numRowsInUse() const noexcept { return m_numRowsInUse; }
    bool isStackDirty() const noexcept { return m_isStackDirty; }

protected:
    /// Ported from: ClipStack.updateTexture (:219-233)
    virtual void updateTexture();
    /// Ported from: ClipStack.recomputeTexture (:235-253)
    virtual void recomputeTexture();
    /// Ported from: ClipStack.uploadTexture (:255-262)
    virtual void uploadTexture();

    // 测试可观测面（headless 无 GL——参考测试以真纹理句柄同一性断言；DanQing 以
    // 生成计数承载"重建 vs 替换"语义，登记于测试头注）。
    rhi::TextureHandle m_texture;
    bool m_textureAllocated = false;    // 逻辑分配位（headless 无句柄——resize 判据）
    uint32_t m_textureGeneration = 0;   // 每次 create 递增（句柄同一性的计数对偶）
    bool m_lastUploadCreated = false;   // 上次 upload 是 create 还是 replace

private:
    uint32_t stack0NumRows() const noexcept { return m_stack[0] ? m_stack[0]->numRows() : 0; }

    /// Ported from: ClipStack.updateColor (:268-272)
    static void updateColor(dqCommon::RgbColor const* rgb, ClipStackFloatRgba& rgba);
    /// Ported from: ClipStack.updateIntersectionStyle (:274-283)
    void updateIntersectionStyle(bool const* colorizeIntersection,
                                 dqCommon::ClipIntersectionStyle const* style);

    std::vector<uint8_t> m_cpuBuffer;  // numRows*16 字节（cpu+gpu 双视图的单一承载）
    rhi::Driver* m_driver = nullptr;   // 生产上传通道（非拥有）
    uint32_t m_numTotalRows = 0;       // 纹理高度（只增不减）
    uint32_t m_numRowsInUse = 0;
    std::vector<ClipVolume::Ptr> m_stack;  // 栈底 = 视图 clip（空 Ptr = emptyClip）
    bool m_isStackDirty = false;
    std::function<dqGeom::Transform const&()> m_getTransform;
    std::function<bool()> m_wantViewClip;
    ClipStackFloatRgba m_insideColor;
    ClipStackFloatRgba m_outsideColor;
    dqGeom::Transform m_prevTransform;
    bool m_colorizeIntersection = false;
    ClipStackFloatRgba m_intersectionStyle;
    uint32_t m_textureHeight = 0;  // 现纹理高度（resize 判据——参考读 texture.height）
};

END_DQ_RENDER_NAMESPACE

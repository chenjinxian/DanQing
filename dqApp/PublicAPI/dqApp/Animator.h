// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Animator interface for view animations
// Ported from: itwinjs-core core/frontend/src/ViewAnimation.ts (Animator /
//              ViewAnimationOptions) + core/frontend/src/FrustumAnimator.ts
//              (FrustumAnimator / interpolateSwingingEye)。
//
// Provides smooth transitions between view states (frustum changes,
// camera movements, etc.) by interpolating over time.
#pragma once

#include "Export.h"
#include "ViewPose.h"  // ViewPose3d（m_begin/m_end 值成员）

#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace dqApp {

class Viewport;

// Easing functions for animation interpolation.
// Ported from: itwinjs-core BeEase.ts
enum class EasingFunction : uint8_t {
    Linear = 0,
    CubicOut = 1,    // Decelerating curve (smooth stop)
    CubicInOut = 2,  // Smooth start and stop
};

// Animation options.
// Ported from: itwinjs-core ViewAnimation.ts ViewAnimationOptions (:36-45)。
// 注意参考的两处默认：
//   animationTime — 毫秒；undefined → ScreenViewport.animation.time.normal（1.0s）。
//   cancelOnAbort — undefined（falsy）→ interrupt 时补到终点（FrustumAnimator.ts:152
//                   三分支的 Infinity 路径）；true → 当前进度 +6% 后停住。
struct AnimationOptions {
    std::optional<double> animationTime;             // 毫秒（:38）
    bool cancelOnAbort = false;                      // 参考 undefined 默认（见上）
    EasingFunction easingFunction = EasingFunction::CubicOut;  // 默认 Easing.Cubic.Out
    // didComplete is true only if the animation finished without being interrupted (:44)。
    std::function<void(bool)> animationFinishedCallback;

    // skipAspectFix：滚轮缩放正交分支（Viewport.ts:2211-2225 vp.zoom）不调用
    // FixAspectRatio——直接 setExtents(extents*factor)，y 随 x 同比例缩放。DanQing
    // 的 doZoom 走 synchWithView → SetupFromView → FixAspectRatio，把 y 强行改回
    // x/aspect，对 y 引入 1.185 的额外压缩（8 事件累积 1.185⁸≈3.9 倍），即连滚净比
    // 比 DTA 慢 ~14% 的根因。滚轮路径置 true 跳过 FixAspectRatio，与参考 1:1。
    bool skipAspectFix = false;
};

// Abstract animator interface.
// Ported from: itwinjs-core ViewAnimation.ts Animator
class DQ_APP_EXPORT Animator {
public:
    virtual ~Animator() = default;

    // Called each frame to advance the animation.
    // Returns true if the animation is complete.
    // Ported from: itwinjs-core Animator.animate()
    virtual bool animate() = 0;

    // Called when the animation is interrupted.
    // Ported from: itwinjs-core Animator.interrupt()
    virtual void interrupt() = 0;
};

// Frustum animator — interpolates between start and end frustum poses.
// Ported from: itwinjs-core FrustumAnimator (FrustumAnimator.ts:53-156)。
//
// 移植范围说明（deferral，同既有登记位）：
//   - zoom-out（:79-101 的 extentBias/eyeBias 建立）：需要 calculateFocusCorners +
//     lookAtViewAlignedVolume 内部件（DanQing 均未落地，见 ViewState.cpp TODO）——
//     本移植按 zoomSettings.enable=false 的参考配置等价（标准视图切换的 begin/end
//     zVec 必然不等，:81 门控本就不可达，行为无差）。
//   - 相机分支（:114-126 lookAt）：ViewState3d::lookAt 尚未全量移植（Step 3 简化，
//     ViewState.h:318 登记）——相机开启的 begin/end 按参考 "duration <= 0 跳过动画"
//     的既有提前返回处理（:63），落到终点姿态；interpolateSwingingEye 已按
//     FrustumAnimator.ts:20-47 移植备用。
class DQ_APP_EXPORT FrustumAnimator : public Animator {
public:
    // Ported from: itwinjs-core FrustumAnimator constructor (FrustumAnimator.ts:58)。
    FrustumAnimator(AnimationOptions const& options, Viewport* viewport,
                    ViewPose3d const& begin, ViewPose3d const& end);
    ~FrustumAnimator() override = default;

    // Animator interface
    bool animate() override;    // ← FrustumAnimator.ts:141-146
    void interrupt() override;  // ← FrustumAnimator.ts:149-155

private:
    // 每帧 onUpdate（FrustumAnimator.ts:111-136 的 onUpdate 体，无相机分支）。
    void onUpdate(double fraction);
    // onComplete（:110）：viewport.setupFromView(end)。
    void onComplete();

    Viewport* m_viewport;
    AnimationOptions m_options;
    ViewPose3d m_begin;  // 值持有（姿态快照，动画期间原视图继续被修改）
    ViewPose3d m_end;

    // 解析状态（构造时求值）
    double m_durationMs = 0.0;              // 0 ⇒ 无动画（首帧即完成，:63-64）
    dqGeom::Vector3d m_axis{0.0, 0.0, 1.0}; // :76 旋转轴（begin→end）
    double m_angleRadians = 0.0;            // :76 旋转角（弧度）

    // 时序（参考 Tweens 的壁钟驱动，Tween.ts update(time) 用 Date.now()）
    std::chrono::steady_clock::time_point m_startTime;
    bool m_startTimeValid = false;
    double m_interruptBoostMs = 0.0;        // interrupt() 的 +6% 跳时（:152）
    bool m_cancelAtCurrent = false;         // interrupt() 的 cancelOnAbort 分支
    bool m_interrupted = false;
};

// Apply easing function to a normalized time value (0-1).
// Ported from: itwinjs-core BeEase.ts
float DQ_APP_EXPORT ApplyEasing(float t, EasingFunction function);

}  // namespace dqApp

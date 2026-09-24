// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Animator implementation
// Ported from: itwinjs-core core/frontend/src/ViewAnimation.ts (Animator /
//              ViewAnimationOptions) + core/frontend/src/FrustumAnimator.ts
//              (FrustumAnimator / interpolateSwingingEye)。
#include "dqApp/Animator.h"
#include "dqApp/Viewport.h"
#include "dqApp/ViewPose.h"
#include "dqApp/ViewState.h"

#include <algorithm>
#include <cmath>

namespace dqApp {

// ---------------------------------------------------------------------------
// Easing functions
// Ported from: itwinjs-core BeEase.ts
// ---------------------------------------------------------------------------
float ApplyEasing(float t, EasingFunction function)
{
    t = std::clamp(t, 0.0f, 1.0f);

    switch (function) {
    case EasingFunction::Linear:
        return t;

    case EasingFunction::CubicOut: {
        float f = t - 1.0f;
        return f * f * f + 1.0f;
    }

    case EasingFunction::CubicInOut: {
        if (t < 0.5f) {
            return 4.0f * t * t * t;
        } else {
            float f = 2.0f * t - 2.0f;
            return 0.5f * f * f * f + 1.0f;
        }
    }

    default:
        return t;
    }
}

namespace {

// Ported from: itwinjs-core FrustumAnimator.ts interpolateSwingingEye (:20-47)。
// 相机分支备用（当前随 lookAt 全量移植落地后启用，见 Animator.h 登记）。
struct SwingingEyeResult { dqGeom::Point3d target; dqGeom::Point3d eye; double distance; };
[[maybe_unused]] SwingingEyeResult interpolateSwingingEye(
    dqGeom::Matrix3d const& axes0, dqGeom::Point3d const& eye0, double distance0,
    dqGeom::Matrix3d const& axes1, dqGeom::Point3d const& eye1, double distance1,
    double fraction, dqGeom::Matrix3d const& axesAtFraction)
{
    auto const z0 = axes0.RowZ();                                        // :30
    auto const z1 = axes1.RowZ();                                        // :31
    auto const zA = axesAtFraction.RowZ();                               // :32
    // back up from the eye points to the targets (:34-35)
    dqGeom::Point3d const target0{
        eye0.x - z0.x * distance0, eye0.y - z0.y * distance0, eye0.z - z0.z * distance0};
    dqGeom::Point3d const target1{
        eye1.x - z1.x * distance1, eye1.y - z1.y * distance1, eye1.z - z1.z * distance1};
    // RULE: Target point moves by simple interpolation (:37)
    auto const targetA = dqGeom::Point3d::FromInterpolate(target0, fraction, target1);
    // RULE: Distance from target to eye is simple interpolation (:39)
    double const distanceA = distance0 + fraction * (distance1 - distance0);  // Geometry.interpolate (:39)
    // The interpolated target, interpolated distance, and specified axes give
    // the intermediate eyepoint (:41)
    dqGeom::Point3d const eyeA{
        targetA.x + zA.x * distanceA, targetA.y + zA.y * distanceA, targetA.z + zA.z * distanceA};
    return {targetA, eyeA, distanceA};
}

// Ported from: itwinjs-core Matrix3d.getAxisAndAngleOfRotation
//              (core/geometry Matrix3d.ts:1171-1233)。
// 返回 {axis, angleRadians, ok}。通用 180°（非标准轴）特征向量路径
// （fastSymmetricEigenvalues, Matrix3d.ts:1211-1223）未移植——与参考失败兜底
// 同形返回 ok:false（axis=(0,0,1), angle=0），标准视图切换（含 Top↔Bottom 等
// 180° 绕标准轴）全部由标准基路径覆盖。
struct AxisAngle { dqGeom::Vector3d axis; double angleRadians; bool ok; };
AxisAngle getAxisAndAngleOfRotation(dqGeom::Matrix3d const& m)
{
    constexpr double kSmallAngleRadians = 1.0e-12;  // Geometry.smallAngleRadians (Geometry.ts:260)
    auto const& c_ = m.coffs;
    double const trace = c_[0] + c_[4] + c_[8];           // :1172
    double const skewXY = c_[3] - c_[1];                  // :1173 — 2*z*sin
    double const skewYZ = c_[7] - c_[5];                  // :1174 — 2*y*sin
    double const skewZX = c_[2] - c_[6];                  // :1175 — 2*x*sin
    double const c = (trace - 1.0) / 2.0;                 // :1177 — cosine
    double const s = std::sqrt(skewXY * skewXY + skewYZ * skewYZ + skewZX * skewZX) / 2.0;  // :1178 — sine
    double const e = c * c + s * s - 1.0;                 // :1179
    if (std::abs(e) > kSmallAngleRadians)                 // :1181-1183 — bad matrix
        return {dqGeom::Vector3d{0.0, 0.0, 1.0}, 0.0, false};

    if (std::abs(s) < kSmallAngleRadians) {               // :1185 — angle 0 or 180
        if (c > 0)                                        // :1186-1187 — no rotation
            return {dqGeom::Vector3d{0.0, 0.0, 1.0}, 0.0, true};
        // 180°：标准基轴检查（:1200-1210）。
        double const axx = c_[0];
        double const ayy = c_[4];
        double const azz = c_[8];
        auto const almostEq = [](double a, double b) {
            return std::abs(a - b) <= 1.0e-8;             // Geometry.isAlmostEqualNumber 默认容差
        };
        constexpr double kPi = 3.14159265358979323846;
        if (almostEq(-1.0, ayy) && almostEq(-1.0, azz))   // :1204-1205
            return {dqGeom::Vector3d{1.0, 0.0, 0.0}, kPi, true};
        if (almostEq(-1.0, axx) && almostEq(-1.0, azz))   // :1206-1207
            return {dqGeom::Vector3d{0.0, 1.0, 0.0}, kPi, true};
        if (almostEq(-1.0, axx) && almostEq(-1.0, ayy))   // :1208-1209
            return {dqGeom::Vector3d{0.0, 0.0, 1.0}, kPi, true};
        // :1211-1223 — 通用特征向量路径（fastSymmetricEigenvalues）未移植，
        // 与参考失败兜底同形返回（见函数头注释）。
        return {dqGeom::Vector3d{0.0, 0.0, 1.0}, 0.0, false};
    }

    // :1225-1231 — good matrix and non-zero sine
    double const a = 1.0 / (2.0 * s);
    return {dqGeom::Vector3d{skewYZ * a, skewZX * a, skewXY * a},
            std::atan2(s, c),  // Angle.createAtan2 (:1230)
            true};
}

}  // namespace

// ---------------------------------------------------------------------------
// FrustumAnimator
// Ported from: itwinjs-core FrustumAnimator (FrustumAnimator.ts:53-156)
// ---------------------------------------------------------------------------
FrustumAnimator::FrustumAnimator(AnimationOptions const& options, Viewport* viewport,
                                 ViewPose3d const& begin, ViewPose3d const& end)
    : m_viewport(viewport)
    , m_options(options)
    , m_begin(begin)
    , m_end(end)
{
    // :59-66 — duration 解析：options.animationTime ?? settings.time.normal。
    double const duration = options.animationTime.value_or(Viewport::animation().time.normal);
    if (duration <= 0.0 || begin.IsCameraOn() != end.IsCameraOn()) {  // :63-64
        m_durationMs = 0.0;   // no duration means skip animation. We can't animate if the camera toggles.
        return;
    }
    // 相机分支（:114-126）依赖完整 lookAt（未移植，登记见 Animator.h）——相机开启
    // 的动画按同一提前返回处理（落终点姿态）。
    if (begin.IsCameraOn()) {
        m_durationMs = 0.0;
        return;
    }
    m_durationMs = duration;

    // :76 — axis to rotate begin to get to end
    dqGeom::Matrix3d beginRotInverse;
    if (begin.rotation.Inverse(beginRotInverse)) {
        auto const delta = end.rotation.MultiplyMatrix(beginRotInverse);
        auto const aa = getAxisAndAngleOfRotation(delta);
        m_axis = aa.axis;
        m_angleRadians = aa.angleRadians;
    }

    // :79-101 — zoom-out（extentBias/eyeBias）建立未移植：按 zoomSettings.enable=false
    // 的参考配置等价（标准视图切换 begin/end zVec 不等，:81 门控不可达；其余动画
    // 走 :112 的 fraction 线性路径——与本实现一致）。落地条件：calculateFocusCorners
    // + lookAtViewAlignedVolume（见 Animator.h 登记）。
}

// Ported from: itwinjs-core FrustumAnimator tween onUpdate (FrustumAnimator.ts:111-136)。
void FrustumAnimator::onUpdate(double fraction)
{
    auto* view = m_viewport ? m_viewport->GetView() : nullptr;
    auto* view3 = view ? view->AsViewState3d() : nullptr;
    if (!view3)
        return;

    // :113 — rot = rotationAroundVector(axis, fraction·angle) · begin.rotation
    auto const rot = dqGeom::Matrix3d::CreateRotationAroundAxis(m_axis, fraction * m_angleRadians)
                         .MultiplyMatrix(m_begin.rotation);

    // :127-134 — no-camera branch：extents 插值 + rotation + target 插值（setCenter 最后，
    // depends on extents and rotation）。
    auto const extents = dqGeom::Vector3d::FromInterpolate(m_begin.extents, fraction, m_end.extents);  // :128
    view3->SetExtents(extents);                                                                        // :131
    view3->SetRotation(rot);                                                                           // :132
    view3->setCenter(dqGeom::Point3d::FromInterpolate(m_begin.GetTarget(), fraction, m_end.GetTarget()));  // :133

    m_viewport->SetupFromView(m_options.skipAspectFix);                                                // :135 + skipAspectFix 门控
}

// Ported from: itwinjs-core FrustumAnimator tween onComplete (FrustumAnimator.ts:110)。
void FrustumAnimator::onComplete()
{
    // viewport.setupFromView(end) — applyPose(end) + doSetupFromView。
    if (m_viewport)
        m_viewport->SetupFromView(&m_end);
}

// Ported from: itwinjs-core FrustumAnimator.animate (FrustumAnimator.ts:141-146)。
bool FrustumAnimator::animate()
{
    if (m_interrupted)
        return true;

    if (m_durationMs <= 0.0) {
        // 无动画路径（:63-64 提前返回）——首帧即完成。
        onComplete();
        if (m_options.animationFinishedCallback)
            m_options.animationFinishedCallback(true);   // :143-144
        return true;
    }

    // tween start — 参考 Tweens.create(start: true) 以创建时刻为起点（Tween.ts）。
    if (!m_startTimeValid) {
        m_startTime = std::chrono::steady_clock::now();
        m_startTimeValid = true;
    }
    auto const elapsedMs =
        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
            std::chrono::steady_clock::now() - m_startTime).count()
        + m_interruptBoostMs;
    double const linearFraction = std::clamp(elapsedMs / m_durationMs, 0.0, 1.0);
    // 参考 tween：easing 作用于归一化进度（Easing.Cubic.Out 默认）。
    double const fraction = ApplyEasing(static_cast<float>(linearFraction),
                                        m_options.easingFunction);
    onUpdate(fraction);

    if (linearFraction >= 1.0) {
        onComplete();
        if (m_options.animationFinishedCallback)
            m_options.animationFinishedCallback(true);   // :143-144
        return true;
    }
    return false;
}

// Ported from: itwinjs-core FrustumAnimator.interrupt (FrustumAnimator.ts:149-155)。
void FrustumAnimator::interrupt()
{
    // :152 — cancelOnAbort: 当前进度 +6% 后停住（leave at current point）；
    //         否则（falsy）补到终点（Infinity）。
    if (m_options.cancelOnAbort && m_durationMs > 0.0) {
        m_interruptBoostMs = m_durationMs * 0.06;
        // 以当前壁钟进度 +6% 应用一次（tween 被 update(futureTime) 推进一次后，
        // 动画器即被丢弃——视图停在该插值点）。越过终点则按终点处理。
        if (!m_startTimeValid) {
            m_startTime = std::chrono::steady_clock::now();
            m_startTimeValid = true;
        }
        auto const elapsedMs =
            std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
                std::chrono::steady_clock::now() - m_startTime).count()
            + m_interruptBoostMs;
        double const linearFraction = std::clamp(elapsedMs / m_durationMs, 0.0, 1.0);
        if (linearFraction >= 1.0)
            onComplete();
        else
            onUpdate(ApplyEasing(static_cast<float>(linearFraction), m_options.easingFunction));
    } else {
        onComplete();
    }
    if (m_options.animationFinishedCallback)
        m_options.animationFinishedCallback(false);  // :153-154
    m_interrupted = true;
}

}  // namespace dqApp

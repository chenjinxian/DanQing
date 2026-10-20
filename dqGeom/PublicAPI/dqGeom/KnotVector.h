// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — KnotVector（B 样条结向量）
// Ported from: itwinjs-core core/geometry/src/bspline/KnotVector.ts
//
// 子集（M-W W-b2，AspectRatioSkew/BSplineCurve3d 消费面）：结存储与固定值、
// create/createUniformClamped/createUniformWrapped、span↔knot↔fraction 互换、
// evaluateBasisFunctions(1)（Mansfield-Cox-de Boor）、knotToLeftKnotIndex、
// isIndexOfRealSpan、reflectKnots、copyKnots、isAlmostEqual。
// TODO（随消费面扩展）：getKnotMultiplicity(AtIndex)/normalize/testClosable/
// addKnot 相关面（clonePartialCurve 链）、wrappable 序列化反转。
#pragma once

#include "DqGeom.h"

#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// BSplineWrapMode — 1:1 KnotVector.ts:18-33
// ---------------------------------------------------------------------------
enum class BSplineWrapMode : int {
    None = 0,
    OpenByAddingControlPoints = 1,
    OpenByRemovingKnots = 2,
};

// ---------------------------------------------------------------------------
// KnotVector — 非降结数组（active interval = 去首末 degree-1 结）
// Ported from: KnotVector.ts:50-555
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT KnotVector {
public:
    // 1:1 KnotVector.knotTolerance（:61）。
    static constexpr double knotTolerance = 1.0e-9;

    // 结值数组（1:1 knots 公开字段——参考 public knots: Float64Array）。
    std::vector<double> knots;
    // 多项式次数（1:1 degree 公开字段）。
    int degree = 0;

    KnotVector() = default;

    // 1:1 clone（:116-118——wrappable 随拷）。
    KnotVector clone() const
    {
        KnotVector out;
        out.knots = knots;
        out.degree = degree;
        out.m_wrapMode = m_wrapMode;
        out.setupFixedValues();
        return out;
    }

    // 1:1 leftKnot/rightKnot（:63-68）——active interval 端值。
    double leftKnot() const noexcept { return m_knot0; }
    double rightKnot() const noexcept { return m_knot1; }
    // 1:1 leftKnotIndex/rightKnotIndex（:70-77）。
    int leftKnotIndex() const noexcept { return degree - 1; }
    int rightKnotIndex() const noexcept { return static_cast<int>(knots.size()) - degree; }
    // 1:1 numSpans（:88-91——含重结的零长 span）。
    int numSpans() const noexcept { return rightKnotIndex() - leftKnotIndex(); }
    // 1:1 knotLength01（:129-131）——active 区间长。
    double knotLength01() const noexcept { return m_knot1 - m_knot0; }

    // 1:1 wrappable 访问器（:82-87——缺席即 None）。
    BSplineWrapMode wrappable() const noexcept
    {
        return m_wrapMode;
    }
    void setWrappable(BSplineWrapMode value) noexcept { m_wrapMode = value; }

    // 1:1 setKnots（:230-242——skipFirstAndLast 剥首末结）+ setKnotsCapture
    // （:244-247——值语义移动即 C++ 的"捕获"）。
    void setKnots(std::vector<double> const& newKnots, bool skipFirstAndLast = false);
    void setKnotsCapture(std::vector<double>&& newKnots)
    {
        knots = std::move(newKnots);
        setupFixedValues();
    }

    // 1:1 createUniformClamped（:255-267——{degree-1} 重复端结 + 内部均匀）。
    static KnotVector createUniformClamped(int numPoles, int degree, double a0, double a1);
    // 1:1 createUniformWrapped（:278-285——缠绕结 + 内部均匀；numInterval=span 数）。
    static KnotVector createUniformWrapped(int numInterval, int degree, double a0, double a1);
    // 1:1 create（:292-297）。
    static KnotVector create(std::vector<double> const& knotArray, int degree,
                             bool skipFirstAndLast = false);

    // 1:1 grevilleKnot（:303-313——负下标回落 leftKnot、越右回落 rightKnot）。
    double grevilleKnot(int knotIndex) const;
    // 1:1 createBasisArray（:315-317——order 长零数组）。
    std::vector<double> createBasisArray() const
    {
        return std::vector<double>(static_cast<size_t>(degree) + 1, 0.0);
    }

    // 1:1 baseKnotFractionToKnot（:319-323）。
    double baseKnotFractionToKnot(int knotIndex0, double localFraction) const;
    // 1:1 spanFractionToKnot（:325-329）。
    double spanFractionToKnot(int spanIndex, double localFraction) const;
    // 1:1 spanFractionToFraction（:331-334——span 内分点占 active 区间的分位）。
    double spanFractionToFraction(int spanIndex, double localFraction) const;
    // 1:1 fractionToKnot（:336-339——B 样条不可外延的 clamp 原样）。
    double fractionToKnot(double fraction) const;

    // 1:1 evaluateBasisFunctions（:353-387——Mansfield-Cox-de Boor 递推）。
    bool evaluateBasisFunctions(int knotIndex0, double u, std::vector<double>& f) const;
    // 1:1 evaluateBasisFunctions1（:398-469——f/df/(ddf) 三数组）。
    bool evaluateBasisFunctions1(int knotIndex0, double u, std::vector<double>& f,
                                 std::vector<double>& df, std::vector<double>* ddf = nullptr) const;

    // 1:1 knotToLeftKnotIndex（:475-486——u 越右界回落最后非平凡 span）。
    int knotToLeftKnotIndex(double u) const;
    // 1:1 spanIndexToLeftKnotIndex（:491-496）。
    int spanIndexToLeftKnotIndex(int spanIndex) const noexcept
    {
        int const d = degree;
        if (spanIndex <= 0)
            return d - 1;
        return std::min(spanIndex + d - 1, static_cast<int>(knots.size()) - d - 1);
    }
    // 1:1 spanIndexToSpanLength（:498-501）。
    double spanIndexToSpanLength(int spanIndex) const;
    // 1:1 isIndexOfRealSpan（:508-512——重结零长 span 非"实"span）。
    bool isIndexOfRealSpan(int spanIndex) const;

    // 1:1 reflectKnots（:514-521——左右端值保持的镜像 + 数组反转）。
    void reflectKnots();

    // 1:1 isAlmostEqual（:170-174——次数 + 逐结 knotTolerance）。
    bool isAlmostEqual(KnotVector const& other) const;

    // 1:1 copyKnots（:523-554——静态 + 成员两形态；includeExtraEndKnot 的经典
    // 过钳位外结复制）。
    static std::vector<double> copyKnots(std::vector<double> const& knotArray, int degree,
                                         bool includeExtraEndKnot = false,
                                         BSplineWrapMode wrapMode = BSplineWrapMode::None);
    std::vector<double> copyKnots(bool includeExtraEndKnot) const
    {
        // 参考成员形态 :552——`testClosable() ? wrappable : undefined`；testClosable
        // 未移植（子集）——EQUIVALENCE：wrappable 恒 None（无周期性生产面）时参考同值
        // （wrapMode 仅在 isExtraEndKnotPeriodic 判中消费；None ⇒ 非周期臂）。
        return copyKnots(knots, degree, includeExtraEndKnot, BSplineWrapMode::None);
    }

private:
    double m_knot0 = 0.0;  // 1:1 _knot0（active interval 左端）
    double m_knot1 = 1.0;  // 1:1 _knot1（active interval 右端）
    BSplineWrapMode m_wrapMode = BSplineWrapMode::None;  // 1:1 _wrapMode

    // 1:1 setupFixedValues（:119-124）。
    void setupFixedValues()
    {
        if (degree > 0 && knots.size() > static_cast<size_t>(degree)) {
            m_knot0 = knots[static_cast<size_t>(degree) - 1];
            m_knot1 = knots[knots.size() - static_cast<size_t>(degree)];
        }
    }
};

END_DQ_GEOM_NAMESPACE

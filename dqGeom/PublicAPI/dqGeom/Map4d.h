// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Map4d (a pair of inverse Matrix4d)
// Ported from: itwinjs-core core/geometry/src/geometry4d/Map4d.ts
//
// Map4 carries two Matrix4d which are inverses of each other. Used by ViewingSpace
// for the worldToNpc / npcToWorld maps (transform0 = forward/worldToNpc,
// transform1 = reverse/npcToWorld), including the perspective case where the map
// is non-affine (the w-row carries the perspective foreshortening).
#pragma once

#include "Matrix4d.h"
#include "Transform.h"

#include <algorithm>
#include <optional>
#include <utility>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// Map4d — a forward/reverse Matrix4d pair (inverses of each other)
// Ported from: itwinjs-core Map4d (Map4d.ts:9-188)
// ---------------------------------------------------------------------------
class Map4d {
public:
    // Ported from: itwinjs-core Map4d.transform0/transform1 (Map4d.ts:26-33)
    Matrix4d& Transform0() { return m_matrix0; }
    Matrix4d const& Transform0() const { return m_matrix0; }
    Matrix4d& Transform1() { return m_matrix1; }
    Matrix4d const& Transform1() const { return m_matrix1; }

    // Ported from: itwinjs-core Map4d.createRefs (Map4d.ts:34-37)
    static Map4d CreateRefs(Matrix4d matrix0, Matrix4d matrix1) {
        return Map4d(std::move(matrix0), std::move(matrix1));
    }

    // Ported from: itwinjs-core Map4d.createIdentity (Map4d.ts:38-41)
    static Map4d CreateIdentity() {
        return Map4d(Matrix4d::CreateIdentity(), Matrix4d::CreateIdentity());
    }

    // Ported from: itwinjs-core Map4d.createTransform (Map4d.ts:42-57).
    // If transform1 is null, the inverse is computed; otherwise the pair is validated
    // as inverses. Returns nullopt on a singular/non-inverse pair.
    static std::optional<Map4d> CreateTransform(Transform const& transform0,
                                                Transform const* transform1 = nullptr)
    {
        if (transform1 == nullptr) {
            Transform inv;
            if (!transform0.Inverse(inv))
                return std::nullopt;
            return Map4d(Matrix4d::CreateTransform(transform0), Matrix4d::CreateTransform(inv));
        }
        if (!transform0.MultiplyTransform(*transform1).IsIdentity())
            return std::nullopt;
        return Map4d(Matrix4d::CreateTransform(transform0), Matrix4d::CreateTransform(*transform1));
    }

    // Ported from: itwinjs-core Map4d.createBoxMap (Map4d.ts:58-76).
    // Axis-aligned box-to-box scale+translate pair. Returns nullopt if any axis of
    // either box is zero-sized.
    static std::optional<Map4d> CreateBoxMap(Point3d const& lowA, Point3d const& highA,
                                             Point3d const& lowB, Point3d const& highB)
    {
        auto t0 = Matrix4d::CreateBoxToBox(lowA, highA, lowB, highB);
        auto t1 = Matrix4d::CreateBoxToBox(lowB, highB, lowA, highA);
        if (!t0 || !t1)
            return std::nullopt;
        return Map4d(*t0, *t1);
    }

    // Ported from: itwinjs-core Map4d.createVectorFrustum (Map4d.ts:113-150).
    // origin = lower-left-rear of the frustum; u/v/w = the three edge vectors from
    // origin (u=right, v=up, w=toward-eye); fraction = frontSize/rearSize (1.0 = ortho).
    // Returns nullopt if the (u,v,w) frame is not invertible.
    static std::optional<Map4d> CreateVectorFrustum(Point3d const& origin,
                                                    Vector3d const& uVector,
                                                    Vector3d const& vVector,
                                                    Vector3d const& wVector,
                                                    double fraction)
    {
        fraction = std::max(fraction, 1.0e-8);
        Transform slabToWorld = Transform::CreateOriginAndMatrixColumns(origin, uVector, vVector, wVector);
        Transform worldToSlab;
        if (!slabToWorld.Inverse(worldToSlab))
            return std::nullopt;
        Map4d worldToSlabMap(Matrix4d::CreateTransform(worldToSlab),
                             Matrix4d::CreateTransform(slabToWorld));
        // Ported from: itwinjs-core Map4d.ts:142-147 — slabToNPC forward/inverse pair.
        Map4d slabToNPCMap(
            Matrix4d::CreateRowValues(1, 0, 0, 0,  0, 1, 0, 0,  0, 0, fraction, 0,  0, 0, fraction - 1.0, 1),
            Matrix4d::CreateRowValues(1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1.0 / fraction, 0,  0, 0, (1.0 - fraction) / fraction, 1));
        return slabToNPCMap.MultiplyMapMap(worldToSlabMap);
    }

    // Ported from: itwinjs-core Map4d.setFrom (Map4d.ts:77-80)
    void SetFrom(Map4d const& other) {
        m_matrix0.SetFrom(other.m_matrix0);
        m_matrix1.SetFrom(other.m_matrix1);
    }

    // Ported from: itwinjs-core Map4d.clone (Map4d.ts:82-84)
    Map4d Clone() const { return Map4d(m_matrix0.clone(), m_matrix1.clone()); }

    // Ported from: itwinjs-core Map4d.setIdentity (Map4d.ts:86-89)
    void SetIdentity() {
        m_matrix0.SetIdentity();
        m_matrix1.SetIdentity();
    }

    // Ported from: itwinjs-core Map4d.isAlmostEqual (Map4d.ts:109-112)
    bool IsAlmostEqual(Map4d const& other) const {
        return m_matrix0.IsAlmostEqual(other.m_matrix0) && m_matrix1.IsAlmostEqual(other.m_matrix1);
    }

    // Ported from: itwinjs-core Map4d.multiplyMapMap (Map4d.ts:151-161).
    // output.matrix0 = this.matrix0 * other.matrix0;
    // output.matrix1 = other.matrix1 * this.matrix1;  (inverse-side order reversed)
    Map4d MultiplyMapMap(Map4d const& other) const {
        return Map4d(m_matrix0.MultiplyMatrixMatrix(other.m_matrix0),
                     other.m_matrix1.MultiplyMatrixMatrix(m_matrix1));
    }

    // Ported from: itwinjs-core Map4d.reverseInPlace (Map4d.ts:162-167)
    void ReverseInPlace() { std::swap(m_matrix0, m_matrix1); }

    // Ported from: itwinjs-core Map4d.sandwich0This1 (Map4d.ts:168-177)
    Map4d Sandwich0This1(Map4d const& other) const {
        return Map4d(
            other.m_matrix0.MultiplyMatrixMatrix(m_matrix0.MultiplyMatrixMatrix(other.m_matrix1)),
            other.m_matrix0.MultiplyMatrixMatrix(m_matrix1.MultiplyMatrixMatrix(other.m_matrix1)));
    }

    // Ported from: itwinjs-core Map4d.sandwich1This0 (Map4d.ts:178-187)
    Map4d Sandwich1This0(Map4d const& other) const {
        return Map4d(
            other.m_matrix1.MultiplyMatrixMatrix(m_matrix0.MultiplyMatrixMatrix(other.m_matrix0)),
            other.m_matrix1.MultiplyMatrixMatrix(m_matrix1.MultiplyMatrixMatrix(other.m_matrix0)));
    }

private:
    // Ported from: itwinjs-core Map4d private ctor (Map4d.ts:16-21)
    explicit Map4d(Matrix4d matrix0, Matrix4d matrix1)
        : m_matrix0(std::move(matrix0)), m_matrix1(std::move(matrix1)) {}

    Matrix4d m_matrix0;  // forward (worldToNpc)
    Matrix4d m_matrix1;  // reverse (npcToWorld)
};

END_DQ_GEOM_NAMESPACE

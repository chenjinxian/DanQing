// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Frustum uniforms
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/FrustumUniforms.ts
//
// Represents a Target's frustum for use in glsl as a pair of uniforms:
//   u_frustumPlanes (vec4 { top, bottom, left, right })
//   u_frustum       (vec3 { near, far, type })
// plus the projection matrix, view matrix, log-depth constants, and view-up vector.
#pragma once

#include "IModelFrameLifecycle.h"
#include "Matrix.h"
#include "UniformHandle.h"

#include <dqCommon/Frustum.h>
#include <dqCommon/Npc.h>

#include <dqGeom/Matrix3d.h>
#include <dqGeom/Matrix4d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <cmath>
#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

using dqGeom::Matrix3d;
using dqGeom::Matrix4d;
using dqGeom::Point3d;
using dqGeom::Transform;
using dqGeom::Vector3d;

// ---------------------------------------------------------------------------
// FrustumUniformType — matches itwinjs-core FrustumUniformType const enum
// ---------------------------------------------------------------------------
enum class FrustumUniformType : int { TwoDee = 0, Orthographic = 1, Perspective = 2 };

// ---------------------------------------------------------------------------
// Free helper functions — ported verbatim from FrustumUniforms.ts bottom block
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core FrustumUniforms.normalizedDifference()
inline Vector3d normalizedDifference(Point3d const& p0, Point3d const& p1)
{
    Vector3d r = Vector3d::From(p0.x - p1.x, p0.y - p1.y, p0.z - p1.z);
    r.Normalize();
    return r;
}

// Ported from: itwinjs-core FrustumUniforms.fromSumOf()
inline Point3d fromSumOf(Point3d const& p, Vector3d const& v, double scale)
{
    return Point3d::From(p.x + v.x * scale, p.y + v.y * scale, p.z + v.z * scale);
}

// Ported from: itwinjs-core FrustumUniforms.dotDifference()
inline double dotDifference(Point3d const& pt, Point3d const& origin, Vector3d const& vec)
{
    return (pt.x - origin.x) * vec.x + (pt.y - origin.y) * vec.y + (pt.z - origin.z) * vec.z;
}

// Ported from: itwinjs-core FrustumUniforms.lookIn()
inline void lookIn(Point3d const& eye, Vector3d const& viewX, Vector3d const& viewY,
                   Vector3d const& viewZ, Transform& result)
{
    auto& rot = result.matrix.coffs;
    rot[0] = viewX.x; rot[1] = viewX.y; rot[2] = viewX.z;
    rot[3] = viewY.x; rot[4] = viewY.y; rot[5] = viewY.z;
    rot[6] = viewZ.x; rot[7] = viewZ.y; rot[8] = viewZ.z;

    result.origin.x = -viewX.DotProduct(eye);
    result.origin.y = -viewY.DotProduct(eye);
    result.origin.z = -viewZ.DotProduct(eye);
}

// Ported from: itwinjs-core FrustumUniforms.ortho()
inline void orthoProjection(double left, double right, double bottom, double top,
                            double nearVal, double farVal, Matrix4d& result)
{
    result = Matrix4d::CreateRowValues(
        2.0 / (right - left), 0.0, 0.0, -(right + left) / (right - left),
        0.0, 2.0 / (top - bottom), 0.0, -(top + bottom) / (top - bottom),
        0.0, 0.0, -2.0 / (farVal - nearVal), -(farVal + nearVal) / (farVal - nearVal),
        0.0, 0.0, 0.0, 1.0);
}

// Ported from: itwinjs-core FrustumUniforms.frustum()
inline void frustumProjection(double left, double right, double bottom, double top,
                              double nearVal, double farVal, Matrix4d& result)
{
    result = Matrix4d::CreateRowValues(
        (2.0 * nearVal) / (right - left), 0.0, (right + left) / (right - left), 0.0,
        0.0, (2.0 * nearVal) / (top - bottom), (top + bottom) / (top - bottom), 0.0,
        0.0, 0.0, -(farVal + nearVal) / (farVal - nearVal), -(2.0 * farVal * nearVal) / (farVal - nearVal),
        0.0, 0.0, -1.0, 0.0);
}

// ---------------------------------------------------------------------------
// FrustumUniforms — Target frustum uniform state
// Ported from: itwinjs-core FrustumUniforms
// ---------------------------------------------------------------------------
class FrustumUniforms {
public:
    // Ported from: itwinjs-core FrustumUniformType
    using Type = FrustumUniformType;

    // Ported from: itwinjs-core Plane const enum (index into _planeData)
    enum class Plane : int { Top = 0, Bottom = 1, Left = 2, Right = 3 };

    FrustumUniforms() noexcept
    {
        // Reference: projectionMatrix = Matrix4d.createIdentity(); viewMatrix = Transform.createIdentity();
        m_projection = Matrix4d::CreateIdentity();
        m_view = Transform::CreateIdentity();
        m_projection32.initFromMatrix4d(m_projection);
    }

    // --- GPU-state accessors ---

    // uniform vec4 u_frustumPlanes; // { top, bottom, left, right }
    float const* getPlanes() const noexcept { return m_planeData; }

    // uniform vec3 u_frustum; // { near, far, type }
    float const* getFrustumData() const noexcept { return m_frustumData; }

    float getNearPlane() const noexcept { return m_frustumData[0]; }
    float getFarPlane() const noexcept { return m_frustumData[1]; }
    FrustumUniformType getType() const noexcept
    {
        return static_cast<FrustumUniformType>(static_cast<int>(m_frustumData[2]));
    }
    bool getIs2d() const noexcept { return FrustumUniformType::TwoDee == getType(); }
    float getPlanFraction() const noexcept { return static_cast<float>(m_planFraction); }

    // uniform vec2 u_logZ; // { 1/near, log(far/near) }
    float const* getLogZ() const noexcept { return m_logZData; }

    Transform const& getViewMatrix() const noexcept { return m_view; }
    Matrix4d const& getProjectionMatrix() const noexcept { return m_projection; }
    Matrix4 const& getProjectionMatrix32() const noexcept { return m_projection32; }
    float const* getViewUpVector() const noexcept { return m_viewUpVector32; }

    // --- Bind ---

    // Ported from: itwinjs-core FrustumUniforms.bindProjectionMatrix()
    void bindProjectionMatrix(UniformHandle& uniform) const
    {
        uniform.setMatrix4(m_projection32.data);
    }

    // Ported from: itwinjs-core FrustumUniforms.bindUpVector()
    void bindUpVector(UniformHandle& uniform) const
    {
        uniform.setUniform3fv(m_viewUpVector32);
    }

    // --- Mutators ---

    /// Recompute view/projection from a new frustum.
    /// Ported from: itwinjs-core FrustumUniforms.changeFrustum()
    void changeFrustum(dqCommon::Frustum const& newFrustum, double newFraction, bool is3d)
    {
        // Reference early-out: same fraction, same is2d-ness, same frustum.
        if (newFraction == m_planFraction && is3d != getIs2d() && newFrustum.equals(m_planFrustum))
            return;

        m_planFrustum = newFrustum;

        Point3d const& farLowerLeft = newFrustum.getCorner(dqCommon::Npc::LeftBottomRear);
        Point3d const& farLowerRight = newFrustum.getCorner(dqCommon::Npc::RightBottomRear);
        Point3d const& farUpperLeft = newFrustum.getCorner(dqCommon::Npc::LeftTopRear);
        Point3d const& farUpperRight = newFrustum.getCorner(dqCommon::Npc::RightTopRear);
        Point3d const& nearLowerLeft = newFrustum.getCorner(dqCommon::Npc::LeftBottomFront);
        Point3d const& nearLowerRight = newFrustum.getCorner(dqCommon::Npc::RightBottomFront);
        Point3d const& nearUpperLeft = newFrustum.getCorner(dqCommon::Npc::LeftTopFront);
        Point3d const& nearUpperRight = newFrustum.getCorner(dqCommon::Npc::RightTopFront);

        Point3d nearCenter = Point3d::FromInterpolate(nearLowerLeft, 0.5, nearUpperRight);

        Vector3d viewX = normalizedDifference(nearLowerRight, nearLowerLeft);
        Vector3d viewY = normalizedDifference(nearUpperLeft, nearLowerLeft);
        Vector3d viewZ = Vector3d::FromCrossProduct(viewX, viewY);
        viewZ.Normalize();

        m_planFraction = newFraction;

        if (!is3d || newFraction > 0.999) {  // ortho or 2d
            double halfWidth = Vector3d::From(farLowerLeft.x - farLowerRight.x,
                                              farLowerLeft.y - farLowerRight.y,
                                              farLowerLeft.z - farLowerRight.z).Magnitude() * 0.5;
            double halfHeight = Vector3d::From(farUpperRight.x - farLowerRight.x,
                                               farUpperRight.y - farLowerRight.y,
                                               farUpperRight.z - farLowerRight.z).Magnitude() * 0.5;
            double depth = Vector3d::From(nearLowerLeft.x - farLowerLeft.x,
                                          nearLowerLeft.y - farLowerLeft.y,
                                          nearLowerLeft.z - farLowerLeft.z).Magnitude();

            lookIn(nearCenter, viewX, viewY, viewZ, m_view);
            orthoProjection(-halfWidth, halfWidth, -halfHeight, halfHeight, 0.0, depth, m_projection);

            m_nearPlaneCenter = Point3d::FromInterpolate(nearLowerLeft, 0.5, nearUpperRight);

            setPlanes(static_cast<float>(halfHeight), static_cast<float>(-halfHeight),
                      static_cast<float>(-halfWidth), static_cast<float>(halfWidth));
            setFrustum(0.0, static_cast<float>(depth),
                       is3d ? FrustumUniformType::Orthographic : FrustumUniformType::TwoDee);
        } else {  // perspective
            double scale = 1.0 / (1.0 - newFraction);
            Vector3d zVec = Vector3d::From(nearLowerLeft.x - farLowerLeft.x,
                                           nearLowerLeft.y - farLowerLeft.y,
                                           nearLowerLeft.z - farLowerLeft.z);
            Point3d cameraPosition = fromSumOf(farLowerLeft, zVec, scale);

            double frustumLeft = dotDifference(farLowerLeft, cameraPosition, viewX) * newFraction;
            double frustumRight = dotDifference(farLowerRight, cameraPosition, viewX) * newFraction;
            double frustumBottom = dotDifference(farLowerLeft, cameraPosition, viewY) * newFraction;
            double frustumTop = dotDifference(farUpperLeft, cameraPosition, viewY) * newFraction;
            double frustumFront = -dotDifference(nearLowerLeft, cameraPosition, viewZ);
            double frustumBack = -dotDifference(farLowerLeft, cameraPosition, viewZ);

            lookIn(cameraPosition, viewX, viewY, viewZ, m_view);
            frustumProjection(frustumLeft, frustumRight, frustumBottom, frustumTop,
                              frustumFront, frustumBack, m_projection);

            // Ported from: itwinjs-core IModelFrameLifecycle.onChangeCameraView/onChangeCameraFrustum
            float camPos[3] = {
                static_cast<float>(cameraPosition.x),
                static_cast<float>(cameraPosition.y),
                static_cast<float>(cameraPosition.z)};
            float vx[3] = {static_cast<float>(viewX.x), static_cast<float>(viewX.y), static_cast<float>(viewX.z)};
            float vy[3] = {static_cast<float>(viewY.x), static_cast<float>(viewY.y), static_cast<float>(viewY.z)};
            float vz[3] = {static_cast<float>(viewZ.x), static_cast<float>(viewZ.y), static_cast<float>(viewZ.z)};
            FrameLifecycle::fireOnChangeCameraView(camPos, vx, vy, vz);
            FrameLifecycle::fireOnChangeCameraFrustum(
                static_cast<float>(frustumLeft), static_cast<float>(frustumRight),
                static_cast<float>(frustumBottom), static_cast<float>(frustumTop),
                static_cast<float>(frustumFront), static_cast<float>(frustumBack));

            m_nearPlaneCenter = Point3d::FromInterpolate(nearLowerLeft, 0.5, nearUpperRight);

            setPlanes(static_cast<float>(frustumTop), static_cast<float>(frustumBottom),
                      static_cast<float>(frustumLeft), static_cast<float>(frustumRight));
            setFrustum(static_cast<float>(frustumFront), static_cast<float>(frustumBack),
                       FrustumUniformType::Perspective);
        }

        // Transform view-up vector into view space.
        m_viewUpVector = m_view.matrix.MultiplyVector(m_worldUpVector);
        m_viewUpVector.Normalize();
        m_viewUpVector32[0] = static_cast<float>(m_viewUpVector.x);
        m_viewUpVector32[1] = static_cast<float>(m_viewUpVector.y);
        m_viewUpVector32[2] = static_cast<float>(m_viewUpVector.z);

        m_projection32.initFromMatrix4d(m_projection);
    }

    /// Replace the projection matrix directly.
    /// Ported from: itwinjs-core FrustumUniforms.changeProjectionMatrix()
    void changeProjectionMatrix(Matrix4d const& newMatrix)
    {
        m_projection.SetFrom(newMatrix);
        m_projection32.initFromMatrix4d(m_projection);
    }

    /// Replace the view matrix directly — the view half of the changeFrustum()
    /// adaptation. setViewportTransform receives (mv, worldToNdc) matrices instead of a
    /// Frustum, so the view comes from the supplied mv. Without this, m_view stays
    /// identity (changeFrustum has no caller on that path) and every view-relative
    /// consumer (BranchUniforms mv = view·model, branch-stack seeding, viewUpVector)
    /// sees an identity camera.
    /// Ported from: itwinjs-core FrustumUniforms.changeFrustum() (view-matrix half;
    /// reference computes m_view via lookIn from the frustum planes, we take the
    /// caller-supplied transform — same uniform, same consumers).
    void changeViewMatrix(Transform const& view)
    {
        m_view = view;
        // changeFrustum tail (reference recomputes the view-up vector from the new view).
        m_viewUpVector = m_view.matrix.MultiplyVector(m_worldUpVector);
        m_viewUpVector.Normalize();
        m_viewUpVector32[0] = static_cast<float>(m_viewUpVector.x);
        m_viewUpVector32[1] = static_cast<float>(m_viewUpVector.y);
        m_viewUpVector32[2] = static_cast<float>(m_viewUpVector.z);
    }

    /// Set near/far/type directly (used by the projection-matrix extraction path).
    /// Ported from: itwinjs-core FrustumUniforms.setFrustum()
    void setFrustum(float nearPlane, float farPlane, FrustumUniformType type) noexcept
    {
        m_frustumData[0] = nearPlane;
        m_frustumData[1] = farPlane;
        m_frustumData[2] = static_cast<float>(static_cast<int>(type));

        // If nearPlane is zero, shader computes linear depth.
        m_logZData[0] = (0.0f != nearPlane) ? 1.0f / nearPlane : 0.0f;
        m_logZData[1] = (0.0f != nearPlane) ? std::log(farPlane / nearPlane) : farPlane;
    }

protected:
    // Ported from: itwinjs-core FrustumUniforms.setPlanes()
    void setPlanes(float top, float bottom, float left, float right) noexcept
    {
        m_planeData[static_cast<int>(Plane::Top)] = top;
        m_planeData[static_cast<int>(Plane::Bottom)] = bottom;
        m_planeData[static_cast<int>(Plane::Left)] = left;
        m_planeData[static_cast<int>(Plane::Right)] = right;
    }

private:
    // CPU state
    dqCommon::Frustum m_planFrustum;
    double m_planFraction = 0;
    Point3d m_nearPlaneCenter;
    Transform m_view;
    Matrix4d m_projection;
    Vector3d m_worldUpVector = Vector3d::From(0.0, 0.0, 1.0);
    Vector3d m_viewUpVector = Vector3d::From(0.0, 0.0, 1.0);

    // GPU state
    float m_planeData[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_frustumData[3] = {0.0f, 0.0f, 0.0f};
    Matrix4 m_projection32;
    float m_logZData[2] = {0.0f, 0.0f};
    float m_viewUpVector32[3] = {0.0f, 0.0f, 1.0f};
};

END_DQ_RENDER_NAMESPACE

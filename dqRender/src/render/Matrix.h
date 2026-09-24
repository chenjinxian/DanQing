// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Matrix math utilities
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Matrix.ts
//
// Matrix math utilities (3x3 and 4x4) used throughout the renderer.
#pragma once

#include <dqGeom/Matrix3d.h>
#include <dqGeom/Matrix4d.h>
#include <dqGeom/Transform.h>

#include <array>
#include <cmath>
#include <cstring>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Matrix3 — 3x3 matrix (column-major, GL convention: at(row,col)=data[col*3+row])
// ---------------------------------------------------------------------------
struct Matrix3 {
    float data[9] = {1,0,0, 0,1,0, 0,0,1};

    static Matrix3 identity()
    {
        return {{1,0,0, 0,1,0, 0,0,1}};
    }

    static Matrix3 fromMatrix4(float const* m4x4)
    {
        // Extract upper-left 3x3 from 4x4
        return {{m4x4[0], m4x4[1], m4x4[2],
                 m4x4[4], m4x4[5], m4x4[6],
                 m4x4[8], m4x4[9], m4x4[10]}};
    }

    /// Initialize from a row-major Matrix3d (transpose into column-major).
    /// Ported from: itwinjs-core Matrix3.initFromMatrix3d()
    void initFromMatrix3d(dqGeom::Matrix3d const& rot) noexcept
    {
        data[0] = static_cast<float>(rot.at(0, 0));
        data[1] = static_cast<float>(rot.at(1, 0));
        data[2] = static_cast<float>(rot.at(2, 0));
        data[3] = static_cast<float>(rot.at(0, 1));
        data[4] = static_cast<float>(rot.at(1, 1));
        data[5] = static_cast<float>(rot.at(2, 1));
        data[6] = static_cast<float>(rot.at(0, 2));
        data[7] = static_cast<float>(rot.at(1, 2));
        data[8] = static_cast<float>(rot.at(2, 2));
    }

    static Matrix3 fromMatrix3d(dqGeom::Matrix3d const& rot) noexcept
    {
        Matrix3 m;
        m.initFromMatrix3d(rot);
        return m;
    }

    float* ptr() noexcept { return data; }
    float const* ptr() const noexcept { return data; }
};

// ---------------------------------------------------------------------------
// Matrix4 — 4x4 matrix (column-major)
// ---------------------------------------------------------------------------
struct Matrix4 {
    float data[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    static Matrix4 identity()
    {
        return {{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}};
    }

    static Matrix4 translation(float x, float y, float z)
    {
        return {{1,0,0,0, 0,1,0,0, 0,0,1,0, x,y,z,1}};
    }

    static Matrix4 scale(float sx, float sy, float sz)
    {
        return {{sx,0,0,0, 0,sy,0,0, 0,0,sz,0, 0,0,0,1}};
    }

    static Matrix4 rotationZ(float radians)
    {
        float c = std::cos(radians);
        float s = std::sin(radians);
        return {{c,s,0,0, -s,c,0,0, 0,0,1,0, 0,0,0,1}};
    }

    /// multiply two 4x4 matrices (result = a * b).
    static Matrix4 multiply(Matrix4 const& a, Matrix4 const& b)
    {
        Matrix4 r;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += a.data[k * 4 + row] * b.data[col * 4 + k];
                }
                r.data[col * 4 + row] = sum;
            }
        }
        return r;
    }

    /// Transpose the matrix.
    Matrix4 transpose() const
    {
        Matrix4 r;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                r.data[j * 4 + i] = data[i * 4 + j];
            }
        }
        return r;
    }

    /// Initialize from a row-major Matrix4d (transpose into column-major).
    /// Ported from: itwinjs-core Matrix4.initFromMatrix4d()
    /// NB: Matrix4 is column-major (GL); Matrix4d is row-major — this transposes.
    void initFromMatrix4d(dqGeom::Matrix4d const& mat) noexcept
    {
        data[0]  = static_cast<float>(mat.at(0, 0));
        data[1]  = static_cast<float>(mat.at(1, 0));
        data[2]  = static_cast<float>(mat.at(2, 0));
        data[3]  = static_cast<float>(mat.at(3, 0));
        data[4]  = static_cast<float>(mat.at(0, 1));
        data[5]  = static_cast<float>(mat.at(1, 1));
        data[6]  = static_cast<float>(mat.at(2, 1));
        data[7]  = static_cast<float>(mat.at(3, 1));
        data[8]  = static_cast<float>(mat.at(0, 2));
        data[9]  = static_cast<float>(mat.at(1, 2));
        data[10] = static_cast<float>(mat.at(2, 2));
        data[11] = static_cast<float>(mat.at(3, 2));
        data[12] = static_cast<float>(mat.at(0, 3));
        data[13] = static_cast<float>(mat.at(1, 3));
        data[14] = static_cast<float>(mat.at(2, 3));
        data[15] = static_cast<float>(mat.at(3, 3));
    }

    static Matrix4 fromMatrix4d(dqGeom::Matrix4d const& mat) noexcept
    {
        Matrix4 m;
        m.initFromMatrix4d(mat);
        return m;
    }

    /// Convert to a row-major Matrix4d.
    /// Ported from: itwinjs-core Matrix4.toMatrix4d()
    dqGeom::Matrix4d toMatrix4d() const noexcept
    {
        return dqGeom::Matrix4d::CreateRowValues(
            data[0], data[4], data[8],  data[12],
            data[1], data[5], data[9],  data[13],
            data[2], data[6], data[10], data[14],
            data[3], data[7], data[11], data[15]);
    }

    /// Initialize from a Transform (4x3 -> 4x4, last row = [0,0,0,1]).
    /// Ported from: itwinjs-core Matrix4.initFromTransform()
    void initFromTransform(dqGeom::Transform const& xf) noexcept
    {
        auto const& mat = xf.GetMatrix();
        auto const& org = xf.GetOrigin();
        data[0]  = static_cast<float>(mat.coffs[0]);
        data[1]  = static_cast<float>(mat.coffs[3]);
        data[2]  = static_cast<float>(mat.coffs[6]);
        data[3]  = 0.0f;
        data[4]  = static_cast<float>(mat.coffs[1]);
        data[5]  = static_cast<float>(mat.coffs[4]);
        data[6]  = static_cast<float>(mat.coffs[7]);
        data[7]  = 0.0f;
        data[8]  = static_cast<float>(mat.coffs[2]);
        data[9]  = static_cast<float>(mat.coffs[5]);
        data[10] = static_cast<float>(mat.coffs[8]);
        data[11] = 0.0f;
        data[12] = static_cast<float>(org.x);
        data[13] = static_cast<float>(org.y);
        data[14] = static_cast<float>(org.z);
        data[15] = 1.0f;
    }

    static Matrix4 fromTransform(dqGeom::Transform const& xf) noexcept
    {
        Matrix4 m;
        m.initFromTransform(xf);
        return m;
    }

    /// Build from row-major named entries (stored column-major).
    /// Ported from: itwinjs-core Matrix4.fromValues()
    static Matrix4 fromValues(
        float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33) noexcept
    {
        Matrix4 m;
        m.data[0]  = m00; m.data[1]  = m10; m.data[2]  = m20; m.data[3]  = m30;
        m.data[4]  = m01; m.data[5]  = m11; m.data[6]  = m21; m.data[7]  = m31;
        m.data[8]  = m02; m.data[9]  = m12; m.data[10] = m22; m.data[11] = m32;
        m.data[12] = m03; m.data[13] = m13; m.data[14] = m23; m.data[15] = m33;
        return m;
    }

    /// Orthographic projection matrix (left, right, bottom, top, near, far).
    /// Ported from: itwinjs-core Matrix4.fromOrtho()
    static Matrix4 fromOrtho(float l, float r, float b, float t, float n, float f) noexcept
    {
        return fromValues(
            2.0f / (r - l), 0.0f,           0.0f,            -(r + l) / (r - l),
            0.0f,           2.0f / (t - b), 0.0f,            -(t + b) / (t - b),
            0.0f,           0.0f,           -2.0f / (f - n), -(f + n) / (f - n),
            0.0f,           0.0f,           0.0f,            1.0f);
    }

    /// Get translation component.
    void getTranslation(float& x, float& y, float& z) const
    {
        x = data[12]; y = data[13]; z = data[14];
    }

    float* ptr() noexcept { return data; }
    float const* ptr() const noexcept { return data; }

    float& operator[](int i) { return data[i]; }
    float operator[](int i) const { return data[i]; }
};

// ---------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------

/// fromSumOf(p, v, scale) = p + v * scale
inline void fromSumOf(float const* p, float const* v, float scale, float* result)
{
    result[0] = p[0] + v[0] * scale;
    result[1] = p[1] + v[1] * scale;
    result[2] = p[2] + v[2] * scale;
}

/// normalizedDifference(p0, p1) = normalize(p0 - p1)
inline void normalizedDifference(float const* p0, float const* p1, float* result)
{
    float dx = p0[0] - p1[0];
    float dy = p0[1] - p1[1];
    float dz = p0[2] - p1[2];
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len > 1e-10f) {
        result[0] = dx / len;
        result[1] = dy / len;
        result[2] = dz / len;
    } else {
        result[0] = 0; result[1] = 0; result[2] = 0;
    }
}

/// Build a view matrix from eye position and 3 axes.
inline void lookIn(float const* eye, float const* viewX, float const* viewY,
                   float const* viewZ, float* result)
{
    // Column-major view matrix
    result[0] = viewX[0]; result[1] = viewY[0]; result[2] = viewZ[0]; result[3] = 0;
    result[4] = viewX[1]; result[5] = viewY[1]; result[6] = viewZ[1]; result[7] = 0;
    result[8] = viewX[2]; result[9] = viewY[2]; result[10] = viewZ[2]; result[11] = 0;
    result[12] = -(viewX[0]*eye[0] + viewX[1]*eye[1] + viewX[2]*eye[2]);
    result[13] = -(viewY[0]*eye[0] + viewY[1]*eye[1] + viewY[2]*eye[2]);
    result[14] = -(viewZ[0]*eye[0] + viewZ[1]*eye[1] + viewZ[2]*eye[2]);
    result[15] = 1;
}

/// Build an orthographic projection matrix.
inline void ortho(float left, float right, float bottom, float top,
                  float near, float far, float* result)
{
    float rl = right - left;
    float tb = top - bottom;
    float fn = far - near;

    std::memset(result, 0, 16 * sizeof(float));
    result[0] = 2.0f / rl;
    result[5] = 2.0f / tb;
    result[10] = -2.0f / fn;
    result[12] = -(right + left) / rl;
    result[13] = -(top + bottom) / tb;
    result[14] = -(far + near) / fn;
    result[15] = 1.0f;
}

/// Build a perspective projection matrix.
inline void frustum(float left, float right, float bottom, float top,
                    float near, float far, float* result)
{
    float rl = right - left;
    float tb = top - bottom;
    float fn = far - near;

    std::memset(result, 0, 16 * sizeof(float));
    result[0] = (2.0f * near) / rl;
    result[5] = (2.0f * near) / tb;
    result[8] = (right + left) / rl;
    result[9] = (top + bottom) / tb;
    result[10] = -(far + near) / fn;
    result[11] = -1.0f;
    result[14] = -(2.0f * far * near) / fn;
}

END_DQ_RENDER_NAMESPACE

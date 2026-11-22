#pragma once
// Small header-only math helpers shared by the renderer and the editing core (so tests cover the same math).
#include <algorithm>
#include <cmath>

#include "mesh.h"

namespace m3 {

inline Vec3f add(Vec3f a, Vec3f b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3f sub(Vec3f a, Vec3f b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3f mulf(Vec3f a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(Vec3f a, Vec3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3f cross(Vec3f a, Vec3f b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(Vec3f a) { return std::sqrt(dot(a, a)); }
inline Vec3f normalize(Vec3f a) {
    const float l = length(a);
    return l > 0.0f ? mulf(a, 1.0f / l) : a;
}

// Column-major 4x4 matrices (OpenGL convention): element (row, col) = m[col * 4 + row].
inline void perspective(float* m, float fovyRad, float aspect, float zn, float zf) {
    std::fill(m, m + 16, 0.0f);
    const float f = 1.0f / std::tan(fovyRad * 0.5f);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zf + zn) / (zn - zf);
    m[11] = -1.0f;
    m[14] = 2.0f * zf * zn / (zn - zf);
}

inline void lookAt(float* m, Vec3f eye, Vec3f center, Vec3f up) {
    const Vec3f f = normalize(sub(center, eye));
    const Vec3f s = normalize(cross(f, up));
    const Vec3f u = cross(s, f);
    m[0] = s.x;  m[4] = s.y;  m[8] = s.z;   m[12] = -dot(s, eye);
    m[1] = u.x;  m[5] = u.y;  m[9] = u.z;   m[13] = -dot(u, eye);
    m[2] = -f.x; m[6] = -f.y; m[10] = -f.z; m[14] = dot(f, eye);
    m[3] = 0.0f; m[7] = 0.0f; m[11] = 0.0f; m[15] = 1.0f;
}

inline void multiply(float* r, const float* a, const float* b) {
    float t[16];
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a[k * 4 + row] * b[c * 4 + k];
            t[c * 4 + row] = sum;
        }
    std::copy(t, t + 16, r);
}

/** Inverse of a column-major 4x4 matrix (Gauss-Jordan, partial pivoting). Returns false if singular. */
inline bool invert4(const double* in, double* out) {
    double a[4][8];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            a[r][c] = in[c * 4 + r];
            a[r][4 + c] = (r == c) ? 1.0 : 0.0;
        }
    for (int col = 0; col < 4; ++col) {
        int piv = col;
        for (int r = col + 1; r < 4; ++r)
            if (std::fabs(a[r][col]) > std::fabs(a[piv][col])) piv = r;
        if (std::fabs(a[piv][col]) < 1e-300) return false;
        if (piv != col)
            for (int c = 0; c < 8; ++c) std::swap(a[piv][c], a[col][c]);
        const double inv = 1.0 / a[col][col];
        for (int c = 0; c < 8; ++c) a[col][c] *= inv;
        for (int r = 0; r < 4; ++r) {
            if (r == col) continue;
            const double f = a[r][col];
            if (f == 0.0) continue;
            for (int c = 0; c < 8; ++c) a[r][c] -= f * a[col][c];
        }
    }
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) out[c * 4 + r] = a[r][4 + c];
    return true;
}

}  // namespace m3

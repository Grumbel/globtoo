#pragma once
#include <cmath>
#include <cstring>

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };

struct Quat {
    float x = 0.f, y = 0.f, z = 0.f, w = 1.f; // identity

    static Quat identity() { return {}; }

    static Quat from_axis_angle(Vec3 axis, float angle) {
        float half = angle * 0.5f;
        float s = sinf(half);
        float len = sqrtf(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
        if (len < 1e-8f) return identity();
        float inv = 1.f / len;
        return { axis.x * inv * s, axis.y * inv * s, axis.z * inv * s, cosf(half) };
    }

    static Quat multiply(Quat a, Quat b) {
        return {
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
        };
    }

    static Quat normalize(Quat q) {
        float n = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
        if (n < 1e-8f) return identity();
        float inv = 1.f / n;
        return { q.x * inv, q.y * inv, q.z * inv, q.w * inv };
    }

    // Column-major Mat4 from unit quaternion
    Mat4 to_mat4() const {
        float xx = x * x, yy = y * y, zz = z * z;
        float xy = x * y, xz = x * z, yz = y * z;
        float wx = w * x, wy = w * y, wz = w * z;
        Mat4 r = Mat4::identity();
        r.m[0]  = 1.f - 2.f * (yy + zz);
        r.m[1]  = 2.f * (xy + wz);
        r.m[2]  = 2.f * (xz - wy);
        r.m[4]  = 2.f * (xy - wz);
        r.m[5]  = 1.f - 2.f * (xx + zz);
        r.m[6]  = 2.f * (yz + wx);
        r.m[8]  = 2.f * (xz + wy);
        r.m[9]  = 2.f * (yz - wx);
        r.m[10] = 1.f - 2.f * (xx + yy);
        return r;
    }
};

struct Mat4 {
    float m[16];

    static Mat4 identity() {
        Mat4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f;
        return r;
    }

    static Mat4 perspective(float fovy_rad, float aspect, float near, float far) {
        float f = 1.f / tanf(fovy_rad * 0.5f);
        Mat4 r{};
        r.m[0]  = f / aspect;
        r.m[5]  = f;
        r.m[10] = (far + near) / (near - far);
        r.m[11] = -1.f;
        r.m[14] = (2.f * far * near) / (near - far);
        return r;
    }

    static Mat4 look_at(Vec3 eye, Vec3 center, Vec3 up) {
        Vec3 f = normalize(sub(center, eye));
        Vec3 s = normalize(cross(f, up));
        Vec3 u = cross(s, f);

        Mat4 r = identity();
        r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
        r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -dot(s, eye);
        r.m[13] = -dot(u, eye);
        r.m[14] =  dot(f, eye);
        return r;
    }

    static Mat4 multiply(const Mat4& a, const Mat4& b) {
        Mat4 r{};
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                r.m[col * 4 + row] =
                    a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                    a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                    a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                    a.m[3 * 4 + row] * b.m[col * 4 + 3];
            }
        }
        return r;
    }

    static Mat4 translate(Vec3 t) {
        Mat4 r = identity();
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }

    static Mat4 scale(float s) {
        Mat4 r = identity();
        r.m[0] = r.m[5] = r.m[10] = s;
        return r;
    }

    static Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    static float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    static Vec3 cross(Vec3 a, Vec3 b) {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    static Vec3 normalize(Vec3 v) {
        float len = sqrtf(dot(v, v));
        if (len < 1e-8f) return {0, 0, 0};
        return {v.x / len, v.y / len, v.z / len};
    }
};

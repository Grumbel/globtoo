#include "camera.hpp"
#include <algorithm>
#include <cmath>

Mat4 Camera::view_matrix() const {
    // Orbit around origin
    float cp = cosf(pitch), sp = sinf(pitch);
    float cy = cosf(yaw),   sy = sinf(yaw);

    Vec3 eye{
        distance * cp * sy,
        distance * sp,
        distance * cp * cy
    };
    return Mat4::look_at(eye, {0, 0, 0}, {0, 1, 0});
}

Mat4 Camera::proj_matrix(float aspect) const {
    return Mat4::perspective(0.7f, aspect, 0.05f, 100.f);
}

void camera_update(Camera& cam, float dt) {
    // Inertia decay – feels physical
    const float damping = 4.5f;
    cam.yaw   += cam.yaw_vel   * dt;
    cam.pitch += cam.pitch_vel * dt;

    cam.yaw_vel   *= expf(-damping * dt);
    cam.pitch_vel *= expf(-damping * dt);

    // Clamp pitch so we don't flip
    const float limit = 1.4f;
    if (cam.pitch >  limit) { cam.pitch =  limit; cam.pitch_vel = 0; }
    if (cam.pitch < -limit) { cam.pitch = -limit; cam.pitch_vel = 0; }
}

void camera_apply_drag(Camera& cam, float dx, float dy, float sensitivity) {
    cam.yaw_vel   += dx * sensitivity;
    cam.pitch_vel += dy * sensitivity;
}

void camera_apply_zoom(Camera& cam, float delta) {
    // Logarithmic-ish zoom feels more natural
    float factor = 1.f - delta * 0.12f;
    cam.distance *= factor;
    cam.distance = std::clamp(cam.distance, cam.min_dist, cam.max_dist);
}

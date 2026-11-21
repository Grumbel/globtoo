#include "camera.hpp"
#include <algorithm>
#include <cmath>

Mat4 Camera::model_matrix() const {
    return orientation.to_mat4();
}

Mat4 Camera::view_matrix() const {
    // Camera on +Z looking at origin, Y up. Globe rotated by orientation.
    Vec3 eye{0.f, 0.f, distance};
    return Mat4::look_at(eye, {0, 0, 0}, {0, 1, 0});
}

Mat4 Camera::proj_matrix(float aspect) const {
    return Mat4::perspective(0.7f, aspect, 0.05f, 100.f);
}

void camera_update(Camera& cam, float dt) {
    float speed = sqrtf(cam.ang_vel.x * cam.ang_vel.x +
                        cam.ang_vel.y * cam.ang_vel.y +
                        cam.ang_vel.z * cam.ang_vel.z);
    if (speed > 1e-5f) {
        float angle = speed * dt;
        Vec3 axis = { cam.ang_vel.x / speed, cam.ang_vel.y / speed, cam.ang_vel.z / speed };
        Quat delta = Quat::from_axis_angle(axis, angle);
        cam.orientation = Quat::normalize(Quat::multiply(delta, cam.orientation));
    }

    // Physical damping
    const float damping = 2.8f;
    float decay = expf(-damping * dt);
    cam.ang_vel.x *= decay;
    cam.ang_vel.y *= decay;
    cam.ang_vel.z *= decay;
}

void camera_apply_drag(Camera& cam, float dx, float dy, float sensitivity) {
    // View-space arcball: drag direction defines rotation in the plane facing the camera.
    // Horizontal → around Y, vertical → around X. No world-up lock, no poles.
    float ax =  dy * sensitivity;
    float ay =  dx * sensitivity;
    float angle = sqrtf(ax * ax + ay * ay);
    if (angle < 1e-8f)
        return;

    Vec3 axis = { ax / angle, ay / angle, 0.f };
    Quat delta = Quat::from_axis_angle(axis, angle);
    cam.orientation = Quat::normalize(Quat::multiply(delta, cam.orientation));

    // Inertia: last motion becomes angular velocity (rad/s). Scale so a
    // normal flick keeps the globe spinning for a second or two.
    const float vel_scale = 40.f;
    cam.ang_vel.x = ax * vel_scale;
    cam.ang_vel.y = ay * vel_scale;
    cam.ang_vel.z = 0.f;
}

void camera_apply_zoom(Camera& cam, float delta) {
    float factor = 1.f - delta * 0.12f;
    cam.distance *= factor;
    cam.distance = std::clamp(cam.distance, cam.min_dist, cam.max_dist);
}

void camera_grab(Camera& cam) {
    cam.ang_vel = {0, 0, 0};
}

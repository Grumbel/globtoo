// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "camera.hpp"
#include <algorithm>
#include <cmath>

static Vec3 quat_rotate(Quat q, Vec3 v) {
    // q * (0,v) * q^{-1}
    Quat p = { v.x, v.y, v.z, 0.f };
    Quat qi = { -q.x, -q.y, -q.z, q.w }; // unit inverse
    Quat r = Quat::multiply(Quat::multiply(q, p), qi);
    return { r.x, r.y, r.z };
}

Mat4 Camera::view_matrix() const {
    // Camera orbits a fixed Earth: eye = R * (0,0,distance), up = R * (0,1,0)
    Vec3 eye = quat_rotate(orientation, {0.f, 0.f, distance});
    Vec3 up  = quat_rotate(orientation, {0.f, 1.f, 0.f});
    return Mat4::look_at(eye, {0, 0, 0}, up);
}

Mat4 Camera::proj_matrix(float aspect) const {
    // Depth precision must be concentrated around the unit-radius Earth.
    // A fixed 0.05..200 frustum wastes most of the 24-bit depth buffer and
    // causes the near-surface political overlay to flicker at the limb.
    // Stars use their own projection and are drawn with depth disabled.
    const float near_plane = std::max(0.05f, (distance - content_radius - 0.01f) * 0.9f);
    const float far_plane  = distance + content_radius + 0.1f;
    return Mat4::perspective(vertical_fov, aspect, near_plane, far_plane);
}

Vec3 camera_view_to_world(const Camera& cam, Vec3 v) {
    return quat_rotate(cam.orientation, v);
}

void camera_update(Camera& cam, float dt) {
    if (cam.grab_active) return;

    float speed = sqrtf(cam.ang_vel.x * cam.ang_vel.x +
                        cam.ang_vel.y * cam.ang_vel.y +
                        cam.ang_vel.z * cam.ang_vel.z);
    if (speed > 1e-5f) {
        float angle = speed * dt;
        Vec3 axis = { cam.ang_vel.x / speed, cam.ang_vel.y / speed, cam.ang_vel.z / speed };
        Quat delta = Quat::from_axis_angle(axis, angle);
        cam.orientation = Quat::normalize(Quat::multiply(delta, cam.orientation));
    }

    const float damping = 2.8f;
    float decay = expf(-damping * dt);
    cam.ang_vel.x *= decay;
    cam.ang_vel.y *= decay;
    cam.ang_vel.z *= decay;
}

void camera_apply_drag_arcball(Camera& cam, float dx, float dy, float sensitivity) {
    // View-space drag → rotate camera (opposite sign to old model-spin so earth still follows the finger)
    float ax = -dy * sensitivity;
    float ay = -dx * sensitivity;
    float angle = sqrtf(ax * ax + ay * ay);
    if (angle < 1e-8f)
        return;

    // Axis in view space, then to world
    Vec3 axis_view = { ax / angle, ay / angle, 0.f };
    Vec3 axis_world = quat_rotate(cam.orientation, axis_view);
    Quat delta = Quat::from_axis_angle(axis_world, angle);
    cam.orientation = Quat::normalize(Quat::multiply(delta, cam.orientation));

    const float vel_scale = 40.f;
    cam.ang_vel.x = axis_world.x * angle * vel_scale;
    cam.ang_vel.y = axis_world.y * angle * vel_scale;
    cam.ang_vel.z = axis_world.z * angle * vel_scale;
}

static bool ray_sphere(const Camera& cam, float ndc_x, float ndc_y, float aspect, Vec3& out_hit) {
    const float tan_half = tanf(Camera::vertical_fov * 0.5f);
    Vec3 dir_view = Mat4::normalize({
        ndc_x * tan_half * aspect,
        ndc_y * tan_half,
        -1.f
    });
    Vec3 origin = quat_rotate(cam.orientation, {0.f, 0.f, cam.distance});
    Vec3 dir = quat_rotate(cam.orientation, dir_view);

    float b = Mat4::dot(origin, dir);
    float c = Mat4::dot(origin, origin) - 1.f;
    float disc = b * b - c;
    if (disc < 0.f)
        return false;
    float t = -b - sqrtf(disc);
    if (t < 0.f)
        t = -b + sqrtf(disc);
    if (t < 0.f)
        return false;
    out_hit = Mat4::normalize({
        origin.x + t * dir.x,
        origin.y + t * dir.y,
        origin.z + t * dir.z
    });
    return true;
}

bool camera_grab_begin(Camera& cam, float ndc_x, float ndc_y, float aspect) {
    Vec3 hit{};
    if (!ray_sphere(cam, ndc_x, ndc_y, aspect, hit)) {
        cam.grab_active = false;
        return false;
    }
    cam.grab_anchor = hit;
    cam.grab_active = true;
    cam.ang_vel = {0, 0, 0};
    return true;
}

bool camera_grab_move(Camera& cam, float ndc_x, float ndc_y, float aspect, float dt) {
    if (!cam.grab_active)
        return false;

    Vec3 hit{};
    if (!ray_sphere(cam, ndc_x, ndc_y, aspect, hit))
        return false;

    // Rotate the camera so the grabbed world point follows the pointer.
    const Vec3 cross = Mat4::cross(hit, cam.grab_anchor);
    const float sine = sqrtf(Mat4::dot(cross, cross));
    if (sine < 1e-8f)
        return true;

    const float angle = atan2f(sine, Mat4::dot(hit, cam.grab_anchor));
    const Quat delta = Quat::from_axis_angle(cross, angle);
    cam.orientation = Quat::normalize(Quat::multiply(delta, cam.orientation));

    // Bound the rate for batched events and filter short-term sampling jitter.
    const float sample_time = std::max(dt, .004f);
    const float speed = std::min(angle / sample_time, 20.f);
    const float blend = Mat4::dot(cam.ang_vel, cam.ang_vel) > 0.f
        ? -std::expm1(-sample_time / .025f) : 1.f;
    const float scale = speed / sine;
    cam.ang_vel = {
        std::lerp(cam.ang_vel.x, cross.x * scale, blend),
        std::lerp(cam.ang_vel.y, cross.y * scale, blend),
        std::lerp(cam.ang_vel.z, cross.z * scale, blend)
    };
    return true;
}

void camera_grab_reanchor(Camera& cam, float ndc_x, float ndc_y, float aspect) {
    ray_sphere(cam, ndc_x, ndc_y, aspect, cam.grab_anchor);
}

void camera_grab_end(Camera& cam) {
    cam.grab_active = false;
}

void camera_apply_zoom(Camera& cam, float delta) {
    // Half-size, reciprocal wheel steps: zooming in then out returns to the
    // same distance instead of accumulating the old asymmetric 12% steps.
    cam.distance *= std::pow(1.065f, -delta);
    cam.distance = std::clamp(cam.distance, cam.min_dist, cam.max_dist);
}

void camera_grab_kill_inertia(Camera& cam) {
    cam.ang_vel = {0, 0, 0};
}

void camera_apply_roll(Camera& cam, float dx, float sensitivity) {
    // View axis: from eye toward origin = -eye direction in world
    // eye = R * (0,0,distance) → axis = normalize(-eye) = R * (0,0,-1)
    static auto quat_rotate = [](Quat q, Vec3 v) {
        Quat p = { v.x, v.y, v.z, 0.f };
        Quat qi = { -q.x, -q.y, -q.z, q.w };
        Quat r = Quat::multiply(Quat::multiply(q, p), qi);
        return Vec3{ r.x, r.y, r.z };
    };
    float angle = dx * sensitivity;
    if (fabsf(angle) < 1e-8f)
        return;
    Vec3 axis = quat_rotate(cam.orientation, {0.f, 0.f, -1.f});
    Quat delta = Quat::from_axis_angle(axis, angle);
    cam.orientation = Quat::normalize(Quat::multiply(delta, cam.orientation));

    const float vel_scale = 40.f;
    cam.ang_vel.x = axis.x * angle * vel_scale;
    cam.ang_vel.y = axis.y * angle * vel_scale;
    cam.ang_vel.z = axis.z * angle * vel_scale;
}

Vec3 camera_headlight_dir(const Camera& cam) {
    // Lambert shading expects the direction FROM the surface TO the light.
    // The camera-mounted light is at the eye, not behind the Earth.
    return quat_rotate(cam.orientation, {0.f, 0.f, 1.f});
}

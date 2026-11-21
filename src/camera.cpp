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
    return Mat4::perspective(0.7f, aspect, 0.05f, 200.f);
}

Vec3 camera_view_to_world(const Camera& cam, Vec3 v) {
    return quat_rotate(cam.orientation, v);
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
    const float fovy = 0.7f;
    float tan_half = tanf(fovy * 0.5f);
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
    cam.grab_prev = hit;
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

    Vec3 a = cam.grab_prev;
    Vec3 b = hit;
    float d = std::clamp(Mat4::dot(a, b), -1.f, 1.f);
    float angle = acosf(d);
    if (angle < 1e-6f) {
        cam.grab_prev = hit;
        return true;
    }

    // Camera rotates so the previous hit stays under the cursor:
    // rotate camera by the rotation that takes B back toward A.
    Vec3 axis = Mat4::normalize(Mat4::cross(b, a));
    Quat delta = Quat::from_axis_angle(axis, angle);
    cam.orientation = Quat::normalize(Quat::multiply(delta, cam.orientation));

    if (dt > 1e-6f) {
        float w = angle / dt;
        cam.ang_vel.x = axis.x * w;
        cam.ang_vel.y = axis.y * w;
        cam.ang_vel.z = axis.z * w;
    }

    // Re-hit after camera move so next frame is stable
    Vec3 new_hit{};
    if (ray_sphere(cam, ndc_x, ndc_y, aspect, new_hit))
        cam.grab_prev = new_hit;
    else
        cam.grab_prev = a;
    return true;
}

void camera_grab_end(Camera& cam) {
    cam.grab_active = false;
}

void camera_apply_zoom(Camera& cam, float delta) {
    float factor = 1.f - delta * 0.12f;
    cam.distance *= factor;
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

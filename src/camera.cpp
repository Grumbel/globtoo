#include "camera.hpp"
#include <algorithm>
#include <cmath>

Mat4 Camera::model_matrix() const {
    return orientation.to_mat4();
}

Mat4 Camera::view_matrix() const {
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

    const float damping = 2.8f;
    float decay = expf(-damping * dt);
    cam.ang_vel.x *= decay;
    cam.ang_vel.y *= decay;
    cam.ang_vel.z *= decay;
}

void camera_apply_drag_arcball(Camera& cam, float dx, float dy, float sensitivity) {
    float ax =  dy * sensitivity;
    float ay =  dx * sensitivity;
    float angle = sqrtf(ax * ax + ay * ay);
    if (angle < 1e-8f)
        return;

    Vec3 axis = { ax / angle, ay / angle, 0.f };
    Quat delta = Quat::from_axis_angle(axis, angle);
    cam.orientation = Quat::normalize(Quat::multiply(delta, cam.orientation));

    const float vel_scale = 40.f;
    cam.ang_vel.x = ax * vel_scale;
    cam.ang_vel.y = ay * vel_scale;
    cam.ang_vel.z = 0.f;
}

// Ray from camera through NDC against unit sphere at origin. Returns hit in world space.
static bool ray_sphere(float ndc_x, float ndc_y, float aspect, float distance, Vec3& out_hit) {
    // Match Camera::proj_matrix fovy = 0.7
    const float fovy = 0.7f;
    float tan_half = tanf(fovy * 0.5f);
    float vx = ndc_x * tan_half * aspect;
    float vy = ndc_y * tan_half;
    float vz = -1.f;

    Vec3 origin{0.f, 0.f, distance};
    Vec3 dir = Mat4::normalize({vx, vy, vz});

    // |o + t d|^2 = 1
    float b = Mat4::dot(origin, dir);
    float c = Mat4::dot(origin, origin) - 1.f;
    float disc = b * b - c;
    if (disc < 0.f)
        return false;
    float t = -b - sqrtf(disc); // nearer intersection
    if (t < 0.f)
        t = -b + sqrtf(disc);
    if (t < 0.f)
        return false;
    out_hit = { origin.x + t * dir.x, origin.y + t * dir.y, origin.z + t * dir.z };
    // Snap to unit sphere for numerical safety
    out_hit = Mat4::normalize(out_hit);
    return true;
}

bool camera_grab_begin(Camera& cam, float ndc_x, float ndc_y, float aspect) {
    Vec3 hit{};
    if (!ray_sphere(ndc_x, ndc_y, aspect, cam.distance, hit)) {
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
    if (!ray_sphere(ndc_x, ndc_y, aspect, cam.distance, hit))
        return false;

    Vec3 a = cam.grab_prev;
    Vec3 b = hit;
    float d = Mat4::dot(a, b);
    d = std::clamp(d, -1.f, 1.f);
    float angle = acosf(d);
    if (angle < 1e-6f) {
        cam.grab_prev = hit;
        return true;
    }

    Vec3 axis = Mat4::normalize(Mat4::cross(a, b));
    // Rotate the globe so previous point moves toward the new hit
    Quat delta = Quat::from_axis_angle(axis, angle);
    cam.orientation = Quat::normalize(Quat::multiply(delta, cam.orientation));

    // Inertia from this step (world-space axis)
    if (dt > 1e-6f) {
        float w = angle / dt;
        cam.ang_vel.x = axis.x * w;
        cam.ang_vel.y = axis.y * w;
        cam.ang_vel.z = axis.z * w;
    }

    // Important: after rotating the model, the same world hit is still under the cursor
    // only if we re-intersect — keep grab_prev as the *new* ray hit for next frame.
    cam.grab_prev = hit;
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

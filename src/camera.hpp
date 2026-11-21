#pragma once
#include "math.hpp"

struct Camera {
    // Globe orientation (model rotation). Identity shows equator upright.
    Quat orientation = Quat::identity();

    // Angular velocity in view space (radians / second) for inertia
    Vec3 ang_vel = {0, 0, 0};

    float distance = 3.2f;
    float min_dist = 1.15f;
    float max_dist = 8.0f;

    Mat4 view_matrix() const;
    Mat4 model_matrix() const;
    Mat4 proj_matrix(float aspect) const;
};

void camera_update(Camera& cam, float dt);
// Screen-space drag in pixels; sensitivity in radians per pixel
void camera_apply_drag(Camera& cam, float dx, float dy, float sensitivity);
void camera_apply_zoom(Camera& cam, float delta);
void camera_grab(Camera& cam); // kill inertia on press

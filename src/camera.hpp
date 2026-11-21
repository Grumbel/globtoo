#pragma once
#include "math.hpp"

struct Camera {
    float yaw   = 0.f;      // radians, around Y
    float pitch = 0.2f;     // slight tilt
    float distance = 3.2f;  // from center

    float yaw_vel   = 0.f;
    float pitch_vel = 0.f;

    float min_dist = 1.15f; // close enough to see small countries
    float max_dist = 8.0f;

    Mat4 view_matrix() const;
    Mat4 proj_matrix(float aspect) const;
};

void camera_update(Camera& cam, float dt);
void camera_apply_drag(Camera& cam, float dx, float dy, float sensitivity);
void camera_apply_zoom(Camera& cam, float delta);

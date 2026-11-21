#pragma once
#include "math.hpp"

enum class ControlMode {
    Arcball, // free view-space drag (no pole lock)
    Grab     // surface stays under cursor
};

struct Camera {
    Quat orientation = Quat::identity();
    Vec3 ang_vel = {0, 0, 0};

    float distance = 3.2f;
    float min_dist = 1.15f;
    float max_dist = 8.0f;

    ControlMode mode = ControlMode::Grab;

    // Grab tracking (unit-sphere points in world space, valid while dragging)
    bool  grab_active = false;
    Vec3  grab_prev{}; // previous hit on unit sphere

    Mat4 view_matrix() const;
    Mat4 model_matrix() const;
    Mat4 proj_matrix(float aspect) const;
};

void camera_update(Camera& cam, float dt);
void camera_apply_drag_arcball(Camera& cam, float dx, float dy, float sensitivity);
// Grab: rotate so previous surface point maps toward the new ray hit.
// Returns false if ray misses the sphere.
bool camera_grab_begin(Camera& cam, float ndc_x, float ndc_y, float aspect);
bool camera_grab_move(Camera& cam, float ndc_x, float ndc_y, float aspect, float dt);
void camera_grab_end(Camera& cam);
void camera_apply_zoom(Camera& cam, float delta);
void camera_grab_kill_inertia(Camera& cam);

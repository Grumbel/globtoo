// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "math.hpp"

enum class ControlMode {
    Arcball,
    Grab
};

struct Camera {
    // Camera orbit orientation (world). Earth is fixed at origin.
    Quat orientation = Quat::identity();
    Vec3 ang_vel = {0, 0, 0};

    float distance = 3.2f;
    // Keep the camera outside the 1.0015-radius political layer and the near plane.
    // Distance 1.08 gives almost twice the linear magnification of 1.15.
    float min_dist = 1.08f;
    float max_dist = 8.0f;

    ControlMode mode = ControlMode::Grab;

    bool  grab_active = false;
    Vec3  grab_prev{};

    Mat4 view_matrix() const;
    Mat4 proj_matrix(float aspect) const;
};

void camera_update(Camera& cam, float dt);
void camera_apply_drag_arcball(Camera& cam, float dx, float dy, float sensitivity);
bool camera_grab_begin(Camera& cam, float ndc_x, float ndc_y, float aspect);
bool camera_grab_move(Camera& cam, float ndc_x, float ndc_y, float aspect, float dt);
void camera_grab_end(Camera& cam);
void camera_apply_zoom(Camera& cam, float delta);
void camera_grab_kill_inertia(Camera& cam);
// Roll around view axis (camera → origin). dx pixels, sensitivity rad/px.
void camera_apply_roll(Camera& cam, float dx, float sensitivity);

// Rotate a view-space vector into world using camera orientation
Vec3 camera_view_to_world(const Camera& cam, Vec3 v);
// World-space direction from the globe toward the camera-mounted light.
Vec3 camera_headlight_dir(const Camera& cam);

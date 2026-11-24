// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

attribute vec3 a_pos; // equatorial plane, globe radii
uniform mat4 u_view;
uniform mat4 u_projection;
varying mediump vec3 v_world;
void main() {
    v_world = a_pos;
    gl_Position = u_projection * (u_view * vec4(a_pos, 1.0));
}

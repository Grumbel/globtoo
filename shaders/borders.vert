// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

attribute vec3 a_pos;
attribute vec3 a_anchor;
#include "projection.glsl"
void main() {
    gl_Position = scene_position(a_pos, a_anchor);
}

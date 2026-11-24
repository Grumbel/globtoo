// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

precision mediump float;
uniform vec4 u_color;
varying mediump float v_map_lon;
varying mediump vec2 v_edge;
uniform mediump float u_morph;
uniform mediump float u_bounded_projection;
void main() {
    if (u_morph > 0.999 && u_bounded_projection > 0.5 &&
        abs(v_map_lon) > 3.141593) discard;
    // Opaque core, so overlapping caps at segment joins are invisible;
    // only the one-pixel outer fringe blends.
    float coverage = clamp(v_edge.y + 0.5 - abs(v_edge.x), 0.0, 1.0);
    if (coverage <= 0.0) discard;
    gl_FragColor = vec4(u_color.rgb, u_color.a * coverage);
}

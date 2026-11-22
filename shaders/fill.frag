// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

precision mediump float;
varying mediump vec3 v_color;
varying mediump vec3 v_normal;
uniform vec3 u_light_dir;
uniform float u_shading;
uniform mediump float u_fill_opacity;

varying mediump float v_map_lon;
uniform mediump float u_morph;
uniform mediump float u_bounded_projection;
void main() {
    if (u_morph > 0.999 && u_bounded_projection > 0.5 &&
        abs(v_map_lon) > 3.141593) discard;
    float ndotl = max(dot(normalize(v_normal), normalize(u_light_dir)), 0.0);
    float ambient = 0.48;
    float diffuse = 0.52 * ndotl;
    float lit = ambient + diffuse;
    vec3 c = mix(v_color, v_color * lit, (u_shading * (1.0 - u_morph)));
    gl_FragColor = vec4(c, u_fill_opacity);
}

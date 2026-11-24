// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

precision mediump float;
uniform sampler2D u_tex;      // radial ring profile, alpha = 1 - exp(-tau)
uniform vec3 u_light_dir;     // headlight, world space
uniform vec3 u_eye;           // camera position, world space
uniform vec2 u_ring_range;    // inner and outer profile radius
uniform float u_fade;
varying mediump vec3 v_world;
void main() {
    float t = (length(v_world.xz) - u_ring_range.x) /
              (u_ring_range.y - u_ring_range.x);
    if (t < 0.0 || t > 1.0) discard;
    vec4 ring = texture2D(u_tex, vec2(t, 0.5));
    // Slant optical depth: a grazing view crosses more ring material.
    float mu = max(abs(normalize(u_eye - v_world).y), 0.02);
    float alpha = 1.0 - pow(max(1.0 - ring.a, 0.0005), 1.0 / mu);
    float light = 0.45 + 0.55 * abs(u_light_dir.y);
    gl_FragColor = vec4(ring.rgb * light, alpha * u_fade);
}

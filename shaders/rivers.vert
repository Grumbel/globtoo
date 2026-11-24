// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Rivers as screen-space ribbons: each segment is a quad extruded from its
// projected centerline. The width is physical (globe radii), so a large
// river widens as the camera approaches, with a minimum pixel width so that
// distant rivers stay visible. Square caps of half the width close the gaps
// between consecutive segments.
attribute vec3 a_pos;    // this endpoint
attribute vec3 a_other;  // the segment's other endpoint
// x: side (-1/+1); y: +/-(physical half width in globe radii), positive at
// the segment start and negative at its end; z: minimum half width in pixels.
attribute vec3 a_corner;
uniform vec2 u_viewport;
varying mediump vec2 v_edge; // (signed distance from center, half width), px
#include "projection.glsl"
void main() {
    vec3 anchor = normalize(a_pos + a_other);
    vec4 other = scene_position(a_other, anchor);
    vec4 here = scene_position(a_pos, anchor); // last: owns v_map_lon
    vec2 half_viewport = 0.5 * u_viewport;
    vec2 here_px = here.xy / here.w * half_viewport;
    vec2 other_px = other.xy / other.w * half_viewport;
    float end = sign(a_corner.y);
    vec2 delta = (other_px - here_px) * end; // segment direction, start->end
    float len = length(delta);
    vec2 dir = len > 1e-4 ? delta / len : vec2(1.0, 0.0);
    vec2 normal = vec2(-dir.y, dir.x);
    // Globe radii to pixels; a flat map scales the unit radius by its zoom.
    float scale = mix(1.0, u_map_zoom, u_morph) * u_projection[1][1] *
                  half_viewport.y / here.w;
    float half_width = max(a_corner.z, abs(a_corner.y) * scale);
    float extent = half_width + 0.75; // room for the antialiased edge
    vec2 offset = normal * a_corner.x * extent - dir * end * half_width;
    v_edge = vec2(a_corner.x * extent, half_width);
    gl_Position = vec4((here_px + offset) / half_viewport * here.w, here.zw);
}

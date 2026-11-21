attribute vec3 a_pos;
attribute vec2 a_uv;

uniform mat4 u_mvp;
uniform mat4 u_model;

varying vec2 v_uv;
varying vec3 v_normal; // world-space normal (unit sphere: model * pos)

void main() {
    v_uv = a_uv;
    // Unit sphere: object normal == position; rotate into world
    vec4 n = u_model * vec4(a_pos, 0.0);
    v_normal = normalize(n.xyz);
    gl_Position = u_mvp * vec4(a_pos, 1.0);
}

attribute vec3 a_pos;
attribute vec2 a_uv;
uniform mat4 u_model;
varying mediump vec2 v_uv;
varying mediump vec3 v_normal;
attribute vec3 a_anchor;
#include "projection.glsl"
void main() {
    v_uv = a_uv;
    v_normal = normalize((u_model * vec4(a_pos, 0.0)).xyz);
    gl_Position = scene_position(a_pos, a_anchor);
}

attribute vec3 a_pos;
attribute vec2 a_uv;
uniform mat4 u_model;
varying vec2 v_uv;
varying vec3 v_normal;
#include "projection.glsl"
void main() {
    v_uv = a_uv;
    v_normal = normalize((u_model * vec4(a_pos, 0.0)).xyz);
    gl_Position = scene_position(a_pos, vec2((a_uv.x - 0.5)*360.0, (0.5-a_uv.y)*180.0));
}

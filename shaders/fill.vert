attribute vec3 a_pos;
attribute vec3 a_color;
varying mediump vec3 v_color;
varying mediump vec3 v_normal;
attribute vec3 a_anchor;
#include "projection.glsl"
void main() {
    v_color = a_color;
    v_normal = normalize(a_pos);
    gl_Position = scene_position(a_pos, a_anchor);
}

attribute vec3 a_pos;
attribute vec3 a_anchor;
#include "projection.glsl"
void main() {
    gl_Position = scene_position(a_pos, a_anchor);
}

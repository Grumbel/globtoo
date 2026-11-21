attribute vec3 a_pos;
attribute vec2 a_lonlat;
#include "projection.glsl"
void main() {
    gl_Position = scene_position(a_pos, a_lonlat);
}

attribute vec3 a_pos;
attribute vec3 a_color;
uniform mat4 u_mvp;
varying vec3 v_color;
varying vec3 v_normal;
void main() {
    v_color = a_color;
    v_normal = normalize(a_pos); // unit sphere
    gl_Position = u_mvp * vec4(a_pos, 1.0);
}

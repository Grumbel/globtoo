attribute vec2 a_pos;
attribute vec4 a_color;
uniform vec2 u_viewport;
varying vec4 v_color;
void main() {
    vec2 p = vec2(a_pos.x * 2.0 / u_viewport.x - 1.0,
                  1.0 - a_pos.y * 2.0 / u_viewport.y);
    gl_Position = vec4(p, 0.0, 1.0);
    v_color = a_color;
}

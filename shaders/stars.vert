attribute vec3 a_pos;
attribute float a_bright;

uniform mat4 u_mvp;

varying float v_bright;

void main() {
    v_bright = a_bright;
    gl_Position = u_mvp * vec4(a_pos, 1.0);
    // Point size from brightness (GLES2: gl_PointSize in VS)
    gl_PointSize = 1.5 + a_bright * 3.5;
}

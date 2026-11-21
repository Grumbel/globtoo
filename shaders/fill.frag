precision mediump float;
varying vec3 v_color;
uniform float u_shade; // 0..1 mix with simple lighting via normal~pos
void main() {
    // a_pos is unit-ish; approximate normal from position
    gl_FragColor = vec4(v_color, 1.0);
}

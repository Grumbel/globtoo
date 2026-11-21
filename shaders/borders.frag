precision mediump float;
uniform vec4 u_color;
varying float v_map_lon;
uniform float u_morph;
void main() {
    if (u_morph > 0.999 && abs(v_map_lon) > 3.141593) discard;
    gl_FragColor = u_color;
}

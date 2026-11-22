precision mediump float;
uniform vec4 u_color;
varying mediump float v_map_lon;
uniform mediump float u_morph;
uniform mediump float u_bounded_projection;
void main() {
    if (u_morph > 0.999 && u_bounded_projection > 0.5 &&
        abs(v_map_lon) > 3.141593) discard;
    gl_FragColor = u_color;
}

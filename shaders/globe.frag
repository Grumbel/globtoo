precision mediump float;

varying vec2 v_uv;
uniform sampler2D u_tex;

void main() {
    vec4 color = texture2D(u_tex, v_uv);
    // mild atmosphere rim could be added later
    gl_FragColor = color;
}

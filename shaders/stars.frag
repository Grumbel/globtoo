precision mediump float;

varying mediump float v_bright;

void main() {
    // Soft circular point
    vec2 c = gl_PointCoord - vec2(0.5);
    float r = length(c);
    if (r > 0.5)
        discard;
    float a = smoothstep(0.5, 0.1, r) * v_bright;
    vec3 col = mix(vec3(0.7, 0.8, 1.0), vec3(1.0, 0.98, 0.9), v_bright);
    gl_FragColor = vec4(col, a);
}

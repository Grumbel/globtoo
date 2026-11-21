precision mediump float;
varying vec3 v_color;
varying vec3 v_normal;
uniform vec3 u_light_dir;
uniform float u_shading;

void main() {
    float ndotl = max(dot(normalize(v_normal), normalize(u_light_dir)), 0.0);
    float ambient = 0.35;
    float diffuse = 0.65 * ndotl;
    float lit = ambient + diffuse;
    vec3 c = mix(v_color, v_color * lit, u_shading);
    gl_FragColor = vec4(c, 1.0);
}

precision mediump float;
varying vec3 v_color;
varying vec3 v_normal;
uniform vec3 u_light_dir;
uniform float u_shading;

varying float v_map_lon;
uniform float u_morph;
void main() {
    if (u_morph > 0.999 && abs(v_map_lon) > 3.141593) discard;
    float ndotl = max(dot(normalize(v_normal), normalize(u_light_dir)), 0.0);
    float ambient = 0.48;
    float diffuse = 0.52 * ndotl;
    float lit = ambient + diffuse;
    vec3 c = mix(v_color, v_color * lit, (u_shading * (1.0 - u_morph)));
    gl_FragColor = vec4(c, 1.0);
}

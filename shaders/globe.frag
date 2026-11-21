precision mediump float;

varying vec2 v_uv;
varying vec3 v_normal;

uniform sampler2D u_tex;
uniform float u_shading; // 0 = flat, 1 = lit
uniform vec3 u_light_dir; // world-space, toward the light

void main() {
    vec4 albedo = texture2D(u_tex, v_uv);
    float ndotl = max(dot(normalize(v_normal), normalize(u_light_dir)), 0.0);
    float ambient = 0.28;
    float diffuse = 0.72 * ndotl;
    float lit = ambient + diffuse;
    // Mix flat albedo with lit result
    vec3 color = mix(albedo.rgb, albedo.rgb * lit, u_shading);
    gl_FragColor = vec4(color, albedo.a);
}

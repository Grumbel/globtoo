precision mediump float;

varying vec2 v_uv;
varying vec3 v_normal;

uniform sampler2D u_tex;
uniform float u_shading; // 0 = flat, 1 = lit
uniform vec3 u_light_dir;
uniform float u_textured; // 1 = sample texture, 0 = solid ocean

void main() {
    vec3 albedo;
    if (u_textured > 0.5) {
        albedo = texture2D(u_tex, v_uv).rgb;
    } else {
        // Classic desk-globe ocean
        albedo = vec3(0.25, 0.42, 0.62);
    }
    float ndotl = max(dot(normalize(v_normal), normalize(u_light_dir)), 0.0);
    float ambient = 0.45;
    float diffuse = 0.55 * ndotl;
    float lit = ambient + diffuse;
    vec3 color = mix(albedo, albedo * lit, u_shading);
    gl_FragColor = vec4(color, 1.0);
}

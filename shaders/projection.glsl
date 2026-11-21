// Shared GLES2 vertex projection.  Values are longitude/latitude in DEGREES.
// Imported into the three geometry vertex shaders by shader.cpp.
uniform mat4 u_view;
uniform mat4 u_projection;
// Shared with fragment shaders: GLES2 requires matching uniform precision.
uniform mediump float u_morph;
uniform float u_projection_from;
uniform float u_projection_to;
uniform float u_projection_blend;
uniform float u_center_lon;
uniform float u_center_lat;
uniform float u_map_zoom;
uniform float u_map_distance;
uniform float u_wrap;
// Cross-stage varyings must also have identical precision qualifiers.
varying mediump float v_map_lon;

const float geo_pi = 3.141592653589793;
vec2 project_map(float kind, float lon, float lat) {
    lat = clamp(lat, -1.57079632679, 1.57079632679);
    if (kind < 1.5) return vec2(lon, lat); // equirectangular
    if (kind < 2.5) return vec2(lon, 0.65 * log(tan(geo_pi * 0.25 +
                   clamp(lat, -1.48352986, 1.48352986) * 0.5)));
    if (kind < 3.5) { // Mollweide equal-area
        float theta = lat;
        if (abs(lat) > 1.57079) theta = sign(lat) * 1.57079632679;
        else {
            for (int i = 0; i < 8; ++i) {
                float f = 2.0*theta + sin(2.0*theta) - geo_pi*sin(lat);
                theta -= f / max(0.0001, 2.0 + 2.0*cos(2.0*theta));
            }
        }
        return vec2(0.900316316 * lon * cos(theta), 1.414213562 * sin(theta));
    }
    // Hammer equal-area. The interpolation into this projection behaves
    // like unfolding the sphere into a continuous oval.
    float d = sqrt(max(0.00001, 1.0 + cos(lat)*cos(lon*0.5)));
    return vec2(2.828427124 * cos(lat)*sin(lon*0.5)/d,
                1.414213562 * sin(lat)/d);
}
vec4 scene_position(vec3 sphere_xyz, vec2 lonlat) {
    float lon = radians(lonlat.x) - u_center_lon + u_wrap;
    float lat = radians(lonlat.y);
    v_map_lon = lon;
    vec2 to_xy = project_map(u_projection_to, lon, lat) -
                 project_map(u_projection_to, 0.0, u_center_lat);
    vec2 xy = to_xy;
    if (u_projection_blend < 0.999) {
        vec2 from_xy = project_map(u_projection_from, lon, lat) -
                       project_map(u_projection_from, 0.0, u_center_lat);
        xy = mix(from_xy, to_xy, u_projection_blend);
    }
    xy *= u_map_zoom;
    vec3 globe_camera = (u_view * vec4(sphere_xyz, 1.0)).xyz;
    vec3 flat_camera = vec3(xy, -u_map_distance);
    return u_projection * vec4(mix(globe_camera, flat_camera, u_morph), 1.0);
}

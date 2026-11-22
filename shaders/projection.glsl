// Shared geographic projection for sphere, country fills, borders and labels.
// The Earth is physically rotated into a local tangent frame before mapping.
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mediump float u_morph;
uniform float u_projection_from;
uniform float u_projection_to;
uniform float u_projection_blend;
uniform vec3 u_map_east;
uniform vec3 u_map_north;
uniform vec3 u_map_center;
uniform float u_map_zoom;
uniform float u_map_distance;
uniform float u_wrap; // longitude-copy index -1, 0, +1
uniform float u_map_roll;
varying mediump float v_map_lon;

const float geo_pi = 3.141592653589793;
vec2 project_map(float kind, float lon, float lat) {
    lat = clamp(lat, -1.57079632679, 1.57079632679);
    if (kind < 1.5) return vec2(lon, lat);
    if (kind < 2.5) {
        float maxlat = 1.48352986;
        float clipped = clamp(lat, -maxlat, maxlat);
        float y = 0.65 * (log(tan(geo_pi*0.25 + clipped*0.5)) +
                           (lat-clipped) / cos(maxlat));
        return vec2(lon, y);
    }
    if (kind < 3.5) {
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
    float d = sqrt(max(0.00001, 1.0 + cos(lat)*cos(lon*0.5)));
    return vec2(2.828427124 * cos(lat)*sin(lon*0.5)/d,
                1.414213562 * sin(lat)/d);
}
vec2 rotated_lonlat(vec3 xyz) {
    vec3 p = normalize(xyz);
    return vec2(atan(dot(p,u_map_east),dot(p,u_map_center)),
                asin(clamp(dot(p,u_map_north),-1.0,1.0)));
}
vec4 scene_position(vec3 sphere_xyz, vec3 primitive_anchor) {
    vec2 xy_ll = rotated_lonlat(sphere_xyz);
    // Keep the triangle / border segment continuous when its vertices are
    // on opposite sides of the *moving* projection seam. Fragment clipping
    // then cuts it cleanly at +/-pi instead of stretching it across Earth.
    float reference = rotated_lonlat(primitive_anchor).x;
    float lon = reference + mod(xy_ll.x-reference+geo_pi,2.0*geo_pi)-geo_pi;
    lon += u_wrap * 2.0*geo_pi;
    v_map_lon = lon;
    vec2 xy = project_map(u_projection_to,lon,xy_ll.y);
    if (u_projection_blend < 0.999) {
        vec2 from_xy = project_map(u_projection_from,lon,xy_ll.y);
        xy = mix(from_xy,xy,u_projection_blend);
    }
    float cr=cos(u_map_roll), sr=sin(u_map_roll);
    xy = vec2(cr*xy.x-sr*xy.y, sr*xy.x+cr*xy.y)*u_map_zoom;
    vec3 globe_camera = (u_view * vec4(sphere_xyz,1.0)).xyz;
    vec3 flat_camera = vec3(xy,-u_map_distance);
    return u_projection * vec4(mix(globe_camera,flat_camera,u_morph),1.0);
}

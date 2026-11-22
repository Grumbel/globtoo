#include "map_view.hpp"
#include "geocoordinates.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {
constexpr double pi = geo::pi;
constexpr double max_lat = 85.0 * geo::to_rad;
double clamp_lat(double lat) { return std::clamp(lat, -pi * .5, pi * .5); }
float approach(float current, float goal, float dt, float duration) {
    if (std::abs(goal - current) < 0.00001f) return goal;
    // Exponential damping is frame-rate-independent and interruptible.
    const float alpha = 1.f - std::exp(-dt * 5.0f / duration);
    current += (goal - current) * alpha;
    return std::abs(goal - current) < .0001f ? goal : current;
}

// zone.tab coordinates: +DDMM[SS]+DDDMM[SS] (or negative signs).
bool parse_coordinate(std::string_view token, unsigned degree_digits, double& value) {
    if (token.size() != 1 + degree_digits + 2 &&
        token.size() != 1 + degree_digits + 4) return false;
    if (token[0] != '+' && token[0] != '-') return false;
    unsigned degrees = 0, minutes = 0, seconds = 0;
    for (std::size_t i = 1; i < token.size(); ++i)
        if (token[i] < '0' || token[i] > '9') return false;
    for (unsigned i = 1; i <= degree_digits; ++i)
        degrees = degrees * 10 + (token[i] - '0');
    minutes = (token[1+degree_digits] - '0') * 10 + (token[2+degree_digits] - '0');
    if (token.size() == 1 + degree_digits + 4)
        seconds = (token[3+degree_digits] - '0') * 10 + (token[4+degree_digits] - '0');
    if (minutes >= 60 || seconds >= 60 || degrees > (degree_digits == 2 ? 90u : 180u))
        return false;
    value = (degrees + minutes / 60.0 + seconds / 3600.0) *
            (token[0] == '-' ? -1.0 : 1.0);
    return true;
}

bool parse_zone_coordinates(std::string_view text, double& lon, double& lat) {
    const std::size_t second = text.find_first_of("+-", 1);
    return second != std::string_view::npos &&
           parse_coordinate(text.substr(0, second), 2, lat) &&
           parse_coordinate(text.substr(second), 3, lon);
}

std::string timezone_name() {
    if (const char* tz = std::getenv("TZ"); tz && *tz) {
        std::string name(tz[0] == ':' ? tz + 1 : tz);
        constexpr std::string_view prefix = "/usr/share/zoneinfo/";
        if (name.starts_with(prefix)) name.erase(0, prefix.size());
        return name;
    }
    char link[1024];
    const ssize_t n = readlink("/etc/localtime", link, sizeof(link) - 1);
    if (n > 0) {
        link[n] = 0;
        std::string path(link);
        if (const auto where = path.rfind("/zoneinfo/"); where != std::string::npos)
            return path.substr(where + 10);
    }
    std::ifstream file("/etc/timezone");
    std::string name;
    if (file >> name) return name;
    return {};
}

bool lookup_timezone(std::string_view name, double& lon, double& lat) {
    if (name.empty()) return false;
    for (const auto* path : {"/usr/share/zoneinfo/zone1970.tab", "/usr/share/zoneinfo/zone.tab"}) {
        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            const auto first = line.find('\t');
            const auto second = first == std::string::npos ? first : line.find('\t', first+1);
            const auto third = second == std::string::npos ? second : line.find('\t', second+1);
            if (second == std::string::npos) continue;
            const std::string_view sv(line);
            if (sv.substr(second+1, third == std::string::npos ? std::string::npos : third-second-1) != name)
                continue;
            if (parse_zone_coordinates(sv.substr(first+1, second-first-1), lon, lat)) return true;
        }
    }
    return false;
}
} // namespace

std::array<double, 2> map_project(MapProjection projection, double relative_lon, double lat) {
    lat = clamp_lat(lat);
    switch (projection) {
    case MapProjection::Mercator: {
        // True Mercator diverges at the poles. Continue linearly above 85
        // degrees with its tangent so the *rotated* pole remains finite.
        const double clipped = std::clamp(lat, -max_lat, max_lat);
        const double extra = lat - clipped;
        return {relative_lon, 0.65 * (std::asinh(std::tan(clipped)) +
                                      extra / std::cos(max_lat))};
    }
    case MapProjection::Hammer: {
        const double d = std::sqrt(std::max(.00001, 1.0 + std::cos(lat) * std::cos(relative_lon * .5)));
        return {2.0 * std::sqrt(2.0) * std::cos(lat) * std::sin(relative_lon * .5) / d,
                std::sqrt(2.0) * std::sin(lat) / d};
    }
    case MapProjection::Mollweide: {
        double theta = lat;
        if (std::abs(lat) > pi*.5 - .000001) theta = std::copysign(pi*.5, lat);
        else for (int i=0;i<8;i++) {
            const double f = 2*theta + std::sin(2*theta) - pi*std::sin(lat);
            theta -= f / std::max(.0001, 2.0 + 2.0*std::cos(2*theta));
        }
        return {2.0 * std::sqrt(2.0) / pi * relative_lon * std::cos(theta),
                std::sqrt(2.0) * std::sin(theta)};
    }
    case MapProjection::Globe:
    case MapProjection::Equirectangular: return {relative_lon, lat};
    }
    return {relative_lon, lat};
}

MapFrame map_frame(const MapView& m) {
    if (m.navigation_active &&
        std::abs(std::remainder(m.center_lon-m.navigation_lon,2*pi)) < 1e-9 &&
        std::abs(m.center_lat-m.navigation_lat) < 1e-9)
        return m.navigation_frame;
    const double sl = std::sin(m.center_lon), cl = std::cos(m.center_lon);
    const double sp = std::sin(m.center_lat), cp = std::cos(m.center_lat);
    // Globe coordinate convention: increasing east longitude points toward -Z.
    return {{-sl, 0., -cl},
            {-sp*cl, cp, sp*sl},
            {cp*cl, sp, -cp*sl}};
}

void map_reset_navigation(MapView& m) {
    m.navigation_active=false;
}

std::array<double, 2> map_relative_lonlat(const MapView& m,
                                          double lon, double lat) {
    const auto f = map_frame(m);
    const std::array<double,3> p{std::cos(lat)*std::cos(lon), std::sin(lat),
                                  -std::cos(lat)*std::sin(lon)};
    const auto dot = [&](const std::array<double,3>& v) {
        return p[0]*v[0]+p[1]*v[1]+p[2]*v[2];
    };
    return {std::atan2(dot(f.east), dot(f.center)),
            std::asin(std::clamp(dot(f.north),-1.,1.))};
}

double map_unwrap_lon(double lon, double reference) {
    return reference + std::remainder(lon-reference,2*pi);
}

double map_world_width(MapProjection projection) {
    switch (projection) {
    case MapProjection::Mollweide:
    case MapProjection::Hammer: return 4*std::sqrt(2.);
    default: return 2*pi;
    }
}

std::array<double, 2> map_geographic_center(const MapView& m) {
    const auto f=map_frame(m);
    return {std::atan2(-f.center[2], f.center[0]),
            std::asin(std::clamp(f.center[1],-1.,1.))};
}

void map_apply_roll(MapView& m, float dx_pixels, float sensitivity) {
    if (m.flat_goal < .5f) return;
    m.roll = std::remainder(m.roll + dx_pixels*sensitivity, 2*pi);
}

double map_north_heading(const MapView& m, const Camera& cam) {
    if (m.flat_goal > .5f) {
        const auto f=map_frame(m);
        const auto center=map_geographic_center(m);
        const double lon=center[0],lat=center[1];
        // Geographic north in world space, projected into the *rotated*
        // local frame. It remains correct after arbitrary great-circle drags.
        const std::array<double,3> north{-std::sin(lat)*std::cos(lon),
                                           std::cos(lat),
                                           std::sin(lat)*std::sin(lon)};
        const auto dot=[&](const std::array<double,3>& v) {
            return north[0]*v[0]+north[1]*v[1]+north[2]*v[2];
        };
        return std::remainder(std::atan2(dot(f.east),dot(f.north))-m.roll,2*pi);
    }
    const Vec3 facing = camera_view_to_world(cam, {0,0,1});
    const auto center = geo::xyz_to_lonlat(facing.x, facing.y, facing.z);
    const double lon = center[0]*geo::to_rad;
    const double lat = center[1]*geo::to_rad;
    const Vec3 north{static_cast<float>(-std::sin(lat)*std::cos(lon)),
                     static_cast<float>( std::cos(lat)),
                     static_cast<float>( std::sin(lat)*std::sin(lon))};
    const Vec3 right = camera_view_to_world(cam, {1,0,0});
    const Vec3 up = camera_view_to_world(cam, {0,1,0});
    return std::atan2(static_cast<double>(Mat4::dot(north,right)),
                      static_cast<double>(Mat4::dot(north,up)));
}

void map_align_north(MapView& m, Camera& cam) {
    if (m.flat_goal > .5f) {
        m.roll=std::remainder(m.roll+map_north_heading(m,cam),2*pi);
    } else {
        camera_grab_end(cam);
        camera_grab_kill_inertia(cam);
        camera_apply_roll(cam,static_cast<float>(map_north_heading(m,cam)),1.f);
        camera_grab_kill_inertia(cam);
    }
}

void camera_center_on(Camera& cam, double lon, double lat) {
    const Quat yaw = Quat::from_axis_angle({0.f,1.f,0.f},
                                           static_cast<float>((90.0+lon)*geo::to_rad));
    const Quat pitch = Quat::from_axis_angle({1.f,0.f,0.f},
                                             static_cast<float>(-lat*geo::to_rad));
    cam.orientation = Quat::normalize(Quat::multiply(yaw, pitch));
    cam.ang_vel = {0,0,0};
}

void map_select(MapView& m, MapProjection selected, Camera& cam) {
    if (selected == m.selected) return;
    camera_grab_kill_inertia(cam);
    camera_grab_end(cam);
    if (selected == MapProjection::Globe) {
        // Fold using the complete navigated frame, not the Euler angles that
        // become singular when the user drags sideways at a geographic pole.
        const auto center=map_geographic_center(m);
        const double heading=map_north_heading(m,cam);
        camera_center_on(cam,center[0]*geo::to_deg,center[1]*geo::to_deg);
        camera_apply_roll(cam,static_cast<float>(-heading),1.f);
        camera_grab_kill_inertia(cam);
        m.flat_goal=0.f;
    } else {
        if (m.selected == MapProjection::Globe) {
            const Vec3 facing = camera_view_to_world(cam, {0,0,1});
            const auto ll = geo::xyz_to_lonlat(facing.x, facing.y, facing.z);
            m.center_lon = ll[0] * geo::to_rad;
            m.center_lat = ll[1] * geo::to_rad;
            map_reset_navigation(m);
            // Preserve camera roll when unfolding (do not straighten a
            // globe that the user has rotated with Shift+drag).
            Camera upright{};
            camera_center_on(upright, ll[0], ll[1]);
            const Vec3 a = camera_view_to_world(upright, {0,1,0});
            const Vec3 b = camera_view_to_world(cam, {0,1,0});
            const Vec3 forward = camera_view_to_world(upright, {0,0,-1});
            const Vec3 cross = Mat4::cross(a, b);
            m.roll = std::atan2(Mat4::dot(forward, cross), Mat4::dot(a, b));
            // Preserve regional zoom when flattening a close-up globe.
            m.zoom = std::clamp(1.f / std::max(.08f, cam.distance - 1.f),
                                1.f, 14.f);
            m.from = m.to = selected;
            m.projection_blend = 1.f;
        } else {
            m.from = m.to;
            m.to = selected;
            m.projection_blend = 0.f;
        }
        m.flat_goal = 1.f;
    }
    m.selected = selected;
}

void map_update(MapView& m, float dt) {
    m.globe_to_flat = approach(m.globe_to_flat, m.flat_goal, dt, .85f);
    m.projection_blend = approach(m.projection_blend, 1.f, dt, .85f);
    if (m.projection_blend == 1.f) m.from = m.to;
}

void map_pan(MapView& m, float dx_pixels, float dy_pixels, int height) {
    if (height <= 0 || m.flat_goal < .5f) return;
    const double world_per_pixel = 2.0 * std::tan(.35) * m.map_distance /
                                   (static_cast<double>(height) * m.zoom);
    const double sx = dx_pixels * world_per_pixel;
    const double sy = -dy_pixels * world_per_pixel;
    const double dx = std::cos(m.roll)*sx + std::sin(m.roll)*sy;
    const double dy = -std::sin(m.roll)*sx + std::cos(m.roll)*sy;
    // Every map is now projected about the center of a rotated sphere. The
    // local Jacobian at (0,0) is constant, even at the physical poles; pan by
    // rotating this frame, never by translating/mirroring existing map sheets.
    const double t = m.projection_blend;
    const auto derivative = [](MapProjection p, bool east) {
        const double h=1e-5;
        const auto minus = map_project(p,east ? -h : 0.,east ? 0. : -h);
        const auto plus  = map_project(p,east ?  h : 0.,east ? 0. :  h);
        return ((east ? plus[0]-minus[0] : plus[1]-minus[1])/(2*h));
    };
    const double scale_x = derivative(m.from,true)*(1.-t)+derivative(m.to,true)*t;
    const double scale_y = derivative(m.from,false)*(1.-t)+derivative(m.to,false)*t;
    const double de=-dx/scale_x, dn=-dy/scale_y;
    const double theta=std::hypot(de,dn);
    if (theta<1e-12) return;
    const auto frame=map_frame(m);
    // Great-circle rotation of the complete local frame. This is an SO(3)
    // operation and has no gimbal singularity at either pole.
    std::array<double,3> tangent{};
    for (int k=0;k<3;k++)
        tangent[k]=(de*frame.east[k]+dn*frame.north[k])/theta;
    const std::array<double,3> axis{
        frame.center[1]*tangent[2]-frame.center[2]*tangent[1],
        frame.center[2]*tangent[0]-frame.center[0]*tangent[2],
        frame.center[0]*tangent[1]-frame.center[1]*tangent[0]};
    const auto rotate=[&](const std::array<double,3>& v) {
        const double c=std::cos(theta), sn=std::sin(theta);
        const double proj=axis[0]*v[0]+axis[1]*v[1]+axis[2]*v[2];
        return std::array<double,3>{
            v[0]*c+(axis[1]*v[2]-axis[2]*v[1])*sn+axis[0]*proj*(1-c),
            v[1]*c+(axis[2]*v[0]-axis[0]*v[2])*sn+axis[1]*proj*(1-c),
            v[2]*c+(axis[0]*v[1]-axis[1]*v[0])*sn+axis[2]*proj*(1-c)};
    };
    m.navigation_frame={rotate(frame.east),rotate(frame.north),rotate(frame.center)};
    m.navigation_active=true;
    m.center_lon=std::atan2(-m.navigation_frame.center[2],m.navigation_frame.center[0]);
    m.center_lat=std::asin(std::clamp(m.navigation_frame.center[1],-1.,1.));
    m.navigation_lon=m.center_lon;
    m.navigation_lat=m.center_lat;
}

void map_zoom(MapView& m, float wheel) {
    m.zoom = std::clamp(m.zoom * std::pow(1.15f, wheel), .6f, MapView::max_zoom);
}

std::array<double, 2> timezone_center() {
    const auto name = timezone_name();
    double lon = 0, lat = 0;
    if (lookup_timezone(name, lon, lat)) return {lon, lat};
    // POSIX offset fallback for timezone definitions without geographic data.
    std::time_t now = std::time(nullptr);
    std::tm local{}, utc{};
    localtime_r(&now, &local);
    gmtime_r(&now, &utc);
    // mktime interprets both tm as local; their difference is the UTC offset.
    const double offset_hours = std::difftime(std::mktime(&local), std::mktime(&utc)) / 3600.0;
    return {std::clamp(offset_hours * 15.0, -180.0, 180.0), 0.0};
}

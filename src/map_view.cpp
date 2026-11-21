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
        // degrees with the same slope, making a finite continuous pole sheet.
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

int map_lat_band(double unwrapped_lat) {
    return static_cast<int>(std::floor((unwrapped_lat + pi*.5) / pi));
}

double map_sheet_y(MapProjection kind, double unwrapped_lat) {
    const int band = map_lat_band(unwrapped_lat);
    const double sign = band % 2 == 0 ? 1.0 : -1.0;
    const double physical_lat = sign * (unwrapped_lat - band*pi);
    const double height = map_project(kind, 0., pi*.5)[1];
    return 2.0 * band * height + sign * map_project(kind, 0., physical_lat)[1];
}

std::array<double, 2> map_geographic_center(const MapView& m) {
    const int band = map_lat_band(m.center_lat);
    const double sign = band % 2 == 0 ? 1.0 : -1.0;
    return {std::remainder(m.center_lon + band*pi, 2*pi),
            sign * (m.center_lat - band*pi)};
}

std::vector<int> map_visible_bands(const MapView& m, float aspect) {
    // A rotated viewport has a larger vertical extent in map coordinates.
    const double half_h = std::tan(.35) * m.map_distance / std::max(.6f, m.zoom);
    const double half_w = half_h * std::max(.01f, aspect);
    const double extent = std::abs(std::sin(m.roll))*half_w +
                          std::abs(std::cos(m.roll))*half_h + .005;
    const double t = m.projection_blend;
    const double h = map_project(m.from, 0., pi*.5)[1]*(1.-t) +
                     map_project(m.to, 0., pi*.5)[1]*t;
    const double y = map_sheet_y(m.from, m.center_lat)*(1.-t) +
                     map_sheet_y(m.to, m.center_lat)*t;
    if (h < .01) return {0};
    const int first = static_cast<int>(std::ceil((y-extent-h) / (2*h)));
    const int last = static_cast<int>(std::floor((y+extent+h) / (2*h)));
    std::vector<int> bands;
    for (int band=first;band<=last;band++) bands.push_back(band);
    return bands;
}

void map_apply_roll(MapView& m, float dx_pixels, float sensitivity) {
    if (m.flat_goal < .5f) return;
    m.roll = std::remainder(m.roll + dx_pixels*sensitivity, 2*pi);
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
        // While flat, orientation is hidden. Align the outgoing sphere with
        // the center of the map BEFORE beginning the unfold transition.
        const auto center = map_geographic_center(m);
        camera_center_on(cam, center[0] * geo::to_deg, center[1] * geo::to_deg);
        // Preserve Shift+drag map roll when folding back into the globe.
        camera_apply_roll(cam, static_cast<float>(m.roll), 1.f);
        camera_grab_kill_inertia(cam);
        m.flat_goal = 0.f;
    } else {
        if (m.selected == MapProjection::Globe) {
            const Vec3 facing = camera_view_to_world(cam, {0,0,1});
            const auto ll = geo::xyz_to_lonlat(facing.x, facing.y, facing.z);
            m.center_lon = ll[0] * geo::to_rad;
            m.center_lat = ll[1] * geo::to_rad;
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
    // Undo flat-map roll before interpreting the drag as east/north motion.
    const double sx = dx_pixels * world_per_pixel;
    const double sy = -dy_pixels * world_per_pixel; // screen Y points down
    const double dx = std::cos(m.roll)*sx + std::sin(m.roll)*sy;
    const double dy = -std::sin(m.roll)*sx + std::cos(m.roll)*sy;
    m.center_lon = std::remainder(m.center_lon - dx, 2*pi);
    // Invert the continuous pole-reflected sheet Y. Bisection avoids a
    // singular derivative at Mercator's clamped poles and ellipse tips.
    const double t = m.projection_blend;
    const auto y_of = [&](double lat) {
        return map_sheet_y(m.from,lat)*(1.-t)+map_sheet_y(m.to,lat)*t;
    };
    const double desired_y = y_of(m.center_lat) - dy;
    if (std::abs(dy) < 1e-12) return;
    double lo = -8*pi, hi = 8*pi;
    for (int i=0;i<50;i++) {
        const double mid = (lo+hi)*.5;
        if (y_of(mid)<desired_y) lo=mid;
        else hi=mid;
    }
    // One full north/south circuit is 2pi and restores the same geographic
    // orientation; bounding this number prevents indefinite drift in GLSL.
    m.center_lat = std::remainder((lo+hi)*.5, 2*pi);
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

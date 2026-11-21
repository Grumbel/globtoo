#include "geocoordinates.hpp"
#include "camera.hpp"
#include <cassert>
#include <cmath>
#include <array>

namespace {
void near(double x, double y, double tolerance = 1e-6) {
    assert(std::abs(x - y) < tolerance);
}
void xyz_near(std::array<double, 3> a, std::array<double, 3> b) {
    for (int i = 0; i < 3; ++i) near(a[i], b[i]);
}
std::array<double, 3> sub(std::array<double, 3> a, std::array<double, 3> b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
std::array<double, 3> cross(std::array<double, 3> a, std::array<double, 3> b) {
    return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
}
double dot(std::array<double, 3> a, std::array<double, 3> b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
}

int main() {
    // With +Y north and a right-handed world frame, east is -Z.
    xyz_near(geo::lonlat_to_xyz(0, 0), {1, 0, 0});
    xyz_near(geo::lonlat_to_xyz(90, 0), {0, 0, -1});
    xyz_near(geo::lonlat_to_xyz(-90, 0), {0, 0, 1});
    xyz_near(geo::lonlat_to_xyz(0, 90), {0, 1, 0});
    xyz_near(geo::lonlat_to_xyz(0, -90), {0, -1, 0});

    // The antimeridian is a UV seam, not a discontinuity in world space.
    xyz_near(geo::uv_to_xyz(0, 0.5), {-1, 0, 0});
    xyz_near(geo::uv_to_xyz(1, 0.5), {-1, 0, 0});
    xyz_near(geo::uv_to_xyz(0.5, 0.5), {1, 0, 0});
    xyz_near(geo::uv_to_xyz(0.5, 0), {0, 1, 0});
    xyz_near(geo::uv_to_xyz(0.5, 1), {0, -1, 0});
    xyz_near(geo::uv_to_xyz(0.625, 0.5), geo::lonlat_to_xyz(45, 0));

    for (double lat : {-80., -25., 0., 43., 80.}) {
        for (double lon : {-179., -90., 0., 90., 179.}) {
            const auto xyz = geo::lonlat_to_xyz(lon, lat);
            const auto ll = geo::xyz_to_lonlat(xyz[0], xyz[1], xyz[2]);
            near(ll[0], lon); near(ll[1], lat);
        }
    }

    // Mesh indexing (a, b, a+1) must point outward after east is -Z.
    // Check at arbitrary location as well as in the four quadrants.
    for (double u : {0.1, 0.3, 0.6, 0.8}) {
        for (double v : {0.1, 0.4, 0.7}) {
            auto a = geo::uv_to_xyz(u, v);
            auto b = geo::uv_to_xyz(u, v + 0.001);
            auto east = geo::uv_to_xyz(u + 0.001, v);
            assert(dot(cross(sub(b,a), sub(east,a)), a) > 0);
        }
    }

    // From a camera looking at Greenwich with north upward, east (+10)
    // must project to the RIGHT and west (-10) to the LEFT.
    const auto east = geo::lonlat_to_xyz(10, 0);
    const auto west = geo::lonlat_to_xyz(-10, 0);
    const auto screen_right = std::array<double,3>{0, 0, -1};
    assert(dot(east, screen_right) > 0);
    assert(dot(west, screen_right) < 0);

    Camera cam{};
    // Test the REAL view/projection matrix, not just a hand-derived
    // screen-right vector. Point the camera from +X at longitude zero.
    cam.orientation = Quat::from_axis_angle({0, 1, 0},
                                            static_cast<float>(geo::pi / 2.0));
    const Mat4 vp = Mat4::multiply(cam.proj_matrix(1.6f), cam.view_matrix());
    const auto project = [&](std::array<double, 3> p) {
        const double x = vp.m[0]*p[0] + vp.m[4]*p[1] + vp.m[8]*p[2] + vp.m[12];
        const double y = vp.m[1]*p[0] + vp.m[5]*p[1] + vp.m[9]*p[2] + vp.m[13];
        const double w = vp.m[3]*p[0] + vp.m[7]*p[1] + vp.m[11]*p[2] + vp.m[15];
        assert(w > 0.0);
        return std::array<double, 2>{x/w, y/w};
    };
    assert(project(geo::lonlat_to_xyz(10, 0))[0] > 0.0);
    assert(project(geo::lonlat_to_xyz(-10, 0))[0] < 0.0);
    assert(project(geo::lonlat_to_xyz(0, 10))[1] > 0.0);
    assert(project(geo::lonlat_to_xyz(0, -10))[1] < 0.0);

    // A high-resolution country can now fill the screen without moving the
    // camera inside the globe or clipping the near-surface country mesh.
    assert(cam.min_dist < 1.1f && cam.min_dist > 1.0015f);
    for (int i = 0; i < 80; ++i) camera_apply_zoom(cam, 1.f);
    near(cam.distance, cam.min_dist, 1e-5);
    for (int i = 0; i < 200; ++i) camera_apply_zoom(cam, -1.f);
    near(cam.distance, cam.max_dist, 1e-5);

    for (float distance : {cam.min_dist, 3.2f, cam.max_dist}) {
        cam.distance = distance;
        const auto projection = cam.proj_matrix(1.6f);
        // In perspective matrix, near/far are recovered from m22 and m32.
        const float near_z = projection.m[14] / (projection.m[10] - 1.f);
        const float far_z = projection.m[14] / (projection.m[10] + 1.f);
        assert(near_z > 0.f && near_z < distance - 1.0015f);
        assert(far_z > distance + 1.0015f);
        assert(far_z / near_z < 50.f); // no enormous 0.05..200 range
    }
}

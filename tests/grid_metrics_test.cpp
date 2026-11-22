#include "grid_metrics.hpp"
#include <cassert>
#include <cmath>
#include <limits>

int main() {
    assert(grid::angular_step(1.) == 90.);
    assert(grid::angular_step(4.) == 30.);
    assert(grid::angular_step(12.) == 10.);
    assert(grid::angular_step(120.) == 1.);
    assert(grid::angular_step(1200.) == .1);
    assert(grid::angular_step(2400.) == .05);
    assert(grid::angular_step(4800.) == .025);
    assert(grid::angular_step(0.) == 90.);
    assert(grid::angular_step(std::numeric_limits<double>::quiet_NaN()) == 90.);
    double previous = 90.;
    for (int pixel=1;pixel<=10000;pixel++) {
        const double step=grid::angular_step(pixel);
        assert(step<=previous);
        previous=step;
    }
    assert(grid::round_distance_km(0.)==0.);
    assert(grid::round_distance_km(0.75)==.5);
    assert(grid::round_distance_km(1.)==1.);
    assert(grid::round_distance_km(7.5)==5.);
    assert(grid::round_distance_km(11.)==10.);
    assert(grid::round_distance_km(250.)==200.);
    assert(grid::round_distance_km(600.)==500.);
    assert(grid::round_distance_km(2000.)==2000.);
    assert(std::abs(grid::great_circle_km(3.141592653589793/2.)-10007.56)<1.);
}

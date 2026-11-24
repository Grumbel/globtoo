// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ring_profile.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

struct Rgb { double r, g, b; };

// One main ring: opacity and color are linearly interpolated from its inner
// to its outer edge.
struct Band {
    double inner, outer;          // km from Saturn's center
    double opacity_in, opacity_out; // 1 - exp(-tau) at normal incidence
    Rgb color_in, color_out;
};

// An empty (or nearly empty) gap carved out of the bands.
struct Gap { double center, width, remaining; };

// Radii from the Cassini-era ring nomenclature (IAU / NASA Planetary Fact
// Sheet); opacities are rounded from Voyager/Cassini optical depth profiles.
constexpr Band saturn_bands[] = {
    // D ring: faint, innermost.
    {66900, 74510, .02, .04, {.50,.47,.43}, {.52,.48,.44}},
    // C ring: translucent, greyish.
    {74658, 92000, .10, .22, {.50,.46,.42}, {.58,.53,.47}},
    // B ring: the bright, almost opaque core of the system.
    {92000, 99000, .72, .90, {.80,.72,.59}, {.86,.78,.64}},
    {99000, 104500, .90, .97, {.88,.80,.66}, {.90,.82,.68}},
    {104500, 117580, .97, .85, {.90,.82,.68}, {.84,.76,.62}},
    // Cassini Division: sparse material between B and A.
    {117580, 122170, .08, .12, {.55,.51,.46}, {.58,.53,.47}},
    // A ring.
    {122170, 136775, .62, .52, {.78,.71,.60}, {.74,.68,.58}},
};

constexpr Gap saturn_gaps[] = {
    {77871, 150, .1},   // Colombo gap (C ring)
    {87491, 270, .1},   // Maxwell gap (C ring)
    {117680, 285, 0},   // Huygens gap, inner edge of the Cassini Division
    {133589, 325, 0},   // Encke gap (A ring, cleared by Pan)
    {136505, 42, 0},    // Keeler gap (A ring, cleared by Daphnis)
};

// The narrow, braided F ring outside the A ring.
constexpr double f_ring_center = 140180, f_ring_core_width = 50;

double smooth_box(double x, double inner, double outer, double edge) {
    const auto step=[edge](double t){
        const double s=std::clamp(t/edge+.5,0.,1.);
        return s*s*(3-2*s);
    };
    return step(x-inner)*step(outer-x);
}

// Deterministic value noise: ringlets and density waves give the real
// rings a fine radial texture. Purely cosmetic, a few percent of opacity.
double hash(std::uint32_t n) {
    n=(n<<13U)^n;
    n=n*(n*n*15731U+789221U)+1376312589U;
    return static_cast<double>(n&0x7fffffffU)/2147483647.;
}

double ringlets(double radius_km, double scale_km) {
    const double x=radius_km/scale_km;
    const double i=std::floor(x), f=x-i;
    const double t=f*f*(3-2*f);
    const auto k=static_cast<std::uint32_t>(static_cast<std::int64_t>(i));
    return hash(k)*(1-t)+hash(k+1)*t;
}

void sample_saturn(double radius, double texel_km, double& opacity, Rgb& color) {
    opacity=0;
    color={.80,.73,.62};
    const double edge=std::max(texel_km,20.);
    for (const Band& band : saturn_bands) {
        const double weight=smooth_box(radius,band.inner,band.outer,edge);
        if (weight<=0) continue;
        const double t=std::clamp((radius-band.inner)/(band.outer-band.inner),0.,1.);
        const double value=band.opacity_in+(band.opacity_out-band.opacity_in)*t;
        // Neighbouring bands share edges: keep the stronger contribution.
        if (value*weight>opacity) {
            opacity=value*weight;
            color={band.color_in.r+(band.color_out.r-band.color_in.r)*t,
                   band.color_in.g+(band.color_out.g-band.color_in.g)*t,
                   band.color_in.b+(band.color_out.b-band.color_in.b)*t};
        }
    }
    if (opacity>0) {
        // Structure is strongest in the B and A rings, subtle in the C ring.
        const double texture=.75*ringlets(radius,180.)+.25*ringlets(radius,37.);
        opacity*=.88+.24*texture;
        const double shade=.94+.12*ringlets(radius+5000.,620.);
        color={color.r*shade,color.g*shade,color.b*shade};
    }
    for (const Gap& gap : saturn_gaps) {
        // Gaps narrower than a texel thin the ring proportionally.
        const double width=std::max(gap.width,texel_km);
        const double cut=smooth_box(radius,gap.center-width*.5,gap.center+width*.5,
                                    std::min(width*.5,edge))*
                         std::min(1.,gap.width/texel_km);
        opacity*=1-cut*(1-gap.remaining);
    }
    const double f_width=std::max(f_ring_core_width,texel_km);
    const double f=.35*std::min(1.,f_ring_core_width/texel_km)*
                   std::exp(-std::pow((radius-f_ring_center)/(f_width*.5),2.))+
                   .04*std::exp(-std::pow((radius-f_ring_center)/400.,2.));
    if (f>opacity) {
        color={.82,.76,.66};
        opacity=std::max(opacity,f);
    }
    opacity=std::clamp(opacity,0.,1.);
}

} // namespace

RingProfile ring_profile(BodyId body, int width) {
    RingProfile profile;
    if (body!=BodyId::Saturn || width<2) return profile;
    profile.inner_km=saturn_bands[0].inner;
    profile.outer_km=f_ring_center+1200.;
    profile.width=width;
    profile.rgba.resize(static_cast<std::size_t>(width)*4);
    const double texel=(profile.outer_km-profile.inner_km)/width;
    constexpr int supersamples=8;
    for (int i=0;i<width;i++) {
        double opacity=0, r=0, g=0, b=0;
        for (int s=0;s<supersamples;s++) {
            const double radius=profile.inner_km+(i+(s+.5)/supersamples)*texel;
            double a; Rgb c;
            sample_saturn(radius,texel/supersamples,a,c);
            opacity+=a; r+=c.r*a; g+=c.g*a; b+=c.b*a;
        }
        const auto byte=[](double v){
            return static_cast<unsigned char>(std::lround(std::clamp(v,0.,1.)*255.));
        };
        unsigned char* texel_rgba=&profile.rgba[static_cast<std::size_t>(i)*4];
        // Color is opacity-weighted, so gaps do not bleed dark fringes.
        const double inv=opacity>0 ? 1./opacity : 0.;
        texel_rgba[0]=byte(opacity>0 ? r*inv : .8);
        texel_rgba[1]=byte(opacity>0 ? g*inv : .73);
        texel_rgba[2]=byte(opacity>0 ? b*inv : .62);
        texel_rgba[3]=byte(opacity/supersamples);
    }
    return profile;
}

double ring_profile_opacity(const RingProfile& profile, double radius_km) {
    if (!profile.valid()) return 0;
    const double t=(radius_km-profile.inner_km)/(profile.outer_km-profile.inner_km);
    if (t<0 || t>=1) return 0;
    const int i=std::min(profile.width-1,static_cast<int>(t*profile.width));
    return profile.rgba[static_cast<std::size_t>(i)*4+3]/255.;
}

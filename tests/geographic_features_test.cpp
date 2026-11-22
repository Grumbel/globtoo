// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "geographic_features.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <cstring>
#include <string>

namespace {
void u16(std::ostream& o, unsigned v) {o.put(v&255);o.put((v>>8)&255);}
void u32(std::ostream& o, std::uint32_t v) {u16(o,v);u16(o,v>>16);}
void f32(std::ostream& o,float v) {
    static_assert(sizeof(v)==4);std::uint32_t bits;
    std::memcpy(&bits,&v,4);u32(o,bits);
}
void write_fixture(const std::string& filename) {
    std::ofstream o(filename,std::ios::binary);
    o.write("GLBFEAT1",8);u32(o,1);u32(o,2);u32(o,1);
    f32(o,13.405f);f32(o,52.52f);u32(o,3770000);o.put(0);o.put(1);
    u16(o,6);u16(o,0);o.write("Berlin",6);
    f32(o,8.68f);f32(o,50.11f);u32(o,780000);o.put(4);o.put(0);
    u16(o,9);u16(o,0);o.write("Frankfurt",9);
    for (float x:{1.f,0.f,0.f,.9999f,.01f,0.f}) f32(o,x);
    o.put(3);
}
}
int main(int argc,char** argv) {
    const std::string filename="geography_fixture.gfeat";
    write_fixture(filename);
    GeographicFeatures f;
    std::string error;
    assert(read_geographic_features(filename,f,&error));
    assert(error.empty());
    assert(f.places.size()==2 && f.rivers.size()==1);
    assert(f.places[0].name=="Berlin" && f.places[0].capital);
    assert(std::abs(f.places[0].longitude-13.405f)<.001f);
    assert(f.places[1].name=="Frankfurt" && !f.places[1].capital);
    assert(f.rivers[0].scale_rank==3);
    assert(geographic_city_visible(f.places[0],0,true,false));
    assert(!geographic_city_visible(f.places[0],10,false,true));
    assert(!geographic_city_visible(f.places[1],3,true,true));
    assert(geographic_city_visible(f.places[1],4,true,true));
    assert(geographic_detail_rank(3.2f,1.f,false)<geographic_detail_rank(1.08f,1.f,false));
    assert(geographic_detail_rank(3.f,1.f,true)<geographic_detail_rank(3.f,10.f,true));
    auto saved=f;
    {std::ofstream o(filename,std::ios::binary|std::ios::trunc);o<<"not a feature file";}
    assert(!read_geographic_features(filename,f,&error));
    assert(!error.empty() && f.places.size()==saved.places.size());
    std::remove(filename.c_str());
    if (argc>1) {
        GeographicFeatures real;
        if (!read_geographic_features(argv[1],real,&error)) {
            std::fprintf(stderr,"cannot read compiled geography %s: %s\n",
                         argv[1],error.c_str());
            return 1;
        }
        if (real.places.empty() || real.rivers.empty()) {
            std::fprintf(stderr,"compiled geography is empty: %zu places, %zu rivers\n",
                         real.places.size(),real.rivers.size());
            return 1;
        }
    }
    return 0;
}

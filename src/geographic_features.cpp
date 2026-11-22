// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "geographic_features.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace {
struct Reader {
    std::ifstream in;
    bool read(void* dst, std::size_t size) {
        return static_cast<bool>(in.read(static_cast<char*>(dst), static_cast<std::streamsize>(size)));
    }
    std::uint8_t u8() { std::uint8_t v=0; if (!read(&v,1)) ok=false; return v; }
    std::uint16_t u16() { return u8() | (static_cast<std::uint16_t>(u8()) << 8); }
    std::uint32_t u32() { return u16() | (static_cast<std::uint32_t>(u16()) << 16); }
    float f32() {
        static_assert(sizeof(float)==4);
        std::uint32_t bits=u32();
        float value;
        std::memcpy(&value,&bits,sizeof(value));
        return value;
    }
    std::string str(std::size_t len) {
        std::string s(len,'\0');
        if (len && !read(s.data(),len)) ok=false;
        return s;
    }
    bool ok=true;
};
}

bool read_geographic_features(const std::string& filename, GeographicFeatures& output,
                              std::string* error) {
    auto fail=[&](const char* why) { if (error) *error=why; return false; };
    Reader r{std::ifstream(filename,std::ios::binary)};
    if (!r.in) return fail("cannot open geographic feature file");
    char magic[8];
    if (!r.read(magic,8) || std::memcmp(magic,"GLBFEAT1",8)!=0)
        return fail("invalid geographic feature file magic");
    const auto version=r.u32(), city_count=r.u32(), segment_count=r.u32();
    if (!r.ok || version!=1 || city_count>200000 || segment_count>4000000)
        return fail("unsupported or oversized geographic feature file");
    GeographicFeatures loaded;
    loaded.places.reserve(city_count);
    loaded.rivers.reserve(segment_count);
    for (std::uint32_t i=0;i<city_count;i++) {
        GeographicPlace p;
        p.longitude=r.f32(); p.latitude=r.f32(); p.population=r.u32();
        p.scale_rank=r.u8(); p.capital=r.u8()!=0;
        const auto name_len=r.u16(), alt_len=r.u16();
        if (!r.ok || name_len>1024 || alt_len>1024) return fail("invalid place record");
        p.name=r.str(name_len); p.alternate_name=r.str(alt_len);
        if (!r.ok || !std::isfinite(p.longitude) || !std::isfinite(p.latitude) ||
            std::abs(p.longitude)>180 || std::abs(p.latitude)>90 || p.name.empty())
            return fail("invalid place coordinates or name");
        loaded.places.push_back(std::move(p));
    }
    for (std::uint32_t i=0;i<segment_count;i++) {
        GeographicRiverSegment seg;
        for (float& v:seg.a) v=r.f32();
        for (float& v:seg.b) v=r.f32();
        seg.scale_rank=r.u8();
        if (!r.ok) return fail("truncated river record");
        auto valid=[](const float (&p)[3]) {
            const float length=p[0]*p[0]+p[1]*p[1]+p[2]*p[2];
            return std::isfinite(length) && length>.9f && length<1.1f;
        };
        if (!valid(seg.a) || !valid(seg.b)) return fail("invalid river coordinates");
        loaded.rivers.push_back(seg);
    }
    if (r.in.peek()!=std::char_traits<char>::eof()) return fail("unexpected trailing bytes");
    if (error) error->clear();
    output=std::move(loaded); // keep previous valid data on error
    return true;
}

unsigned geographic_detail_rank(float camera_distance, float map_zoom, bool flat) {
    const float detail=flat ? map_zoom : std::max(0.f,4.f-camera_distance);
    if (detail>8.f) return 10;
    if (detail>3.f) return 8;
    if (detail>1.5f) return 6;
    return 3;
}
bool geographic_city_visible(const GeographicPlace& p, unsigned rank,
                             bool capitals, bool cities) {
    if (p.capital) return capitals;
    return cities && p.scale_rank<=rank;
}

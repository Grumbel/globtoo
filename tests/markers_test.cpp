// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "markers.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
void check(std::string_view input, double lat_expected, double lon_expected,
           CoordinateOrder order=CoordinateOrder::LatitudeLongitude) {
    double lat=999, lon=999;
    std::string error;
    const bool valid=parse_coordinates(input,order,lat,lon,error);
    if (!valid || std::abs(lat-lat_expected)>1e-5 ||
                  std::abs(lon-lon_expected)>1e-5) {
        std::cerr<<"Coordinate failure: '"<<input<<"': "<<error
                 <<" -> "<<lat<<", "<<lon<<"\n";
        std::abort();
    }
}
void reject(std::string_view input) {
    double lat=0,lon=0;
    std::string error;
    if (parse_coordinates(input,CoordinateOrder::LatitudeLongitude,lat,lon,error)) {
        std::cerr<<"Accepted invalid coordinates: '"<<input<<"'\n";
        std::abort();
    }
    assert(!error.empty());
}
} // namespace

int main() {
    check("52.52,13.405",52.52,13.405);
    check("-33.8688,151.2093",-33.8688,151.2093);
    check("51.5074 -0.1278",51.5074,-0.1278);
    check("51.5074 N, 0.1278 W",51.5074,-.1278);
    check("W0.1278 N51.5074",51.5074,-.1278);
    check("S33.92, E18.42",-33.92,18.42);
    check("51:30:26N,0:07:39W",51.+30./60+26./3600,-(7./60+39./3600));
    check("N40°41′21″, W74°02′40″",40+41./60+21./3600,-(74+2./60+40./3600));
    check("51°30.5'N, 0°7.25'W",51+30.5/60.,-7.25/60.);
    check("51 30 26 N 0 7 39 W",51.+30./60+26./3600,-(7./60+39./3600));
    check("geo:52.52,13.405",52.52,13.405);
    check("geo:52.52,13.405,250;u=10",52.52,13.405);
    check("+52.52+013.405/",52.52,13.405);
    check("+5230+01324/",52.5,13.4);
    check("+5230.5+01324.5/",52+30.5/60.,13+24.5/60.);
    check("+523026.5+0132430.5/",52+30./60+26.5/3600.,13+24./60+30.5/3600.);
    check("-334500+1511230/",-33.75,151+12./60+30./3600);
    check("13.405,52.52",52.52,13.405,CoordinateOrder::LongitudeLatitude);
    check("180,90",90,180,CoordinateOrder::LongitudeLatitude);
    for (auto invalid : {"", "51", "91,0", "0,181", "NaN,0",
                         "51:60:00N,0E", "51:30:60,0", "0x20,0",
                         "1E,2E", "N-20,20", "1,2,3", "N51 W181",
                         "51N 30 26E 0", "1,2junk", "geo:1,2,nope", "+5260+01324/"})
        reject(invalid);

    {
        CommandLine planetary;
        std::string err;
        const char* args[]={"globtoo","--body","MaRs","--body-texture",
                            "mars=mars.jpg","--body-texture","moon=moon.jpg",
                            "--texture","override.jpg"};
        assert(parse_command_line(static_cast<int>(std::size(args)),args,planetary,err));
        assert(planetary.body==BodyId::Mars);
        assert(planetary.body_textures[static_cast<std::size_t>(BodyId::Moon)]=="moon.jpg");
        assert(planetary.body_textures[static_cast<std::size_t>(BodyId::Mars)]=="mars.jpg");
        assert(planetary.texture_path=="override.jpg");
        const char* tiled[]={"globtoo","--body-imagery-dir","mars=/maps/mars",
                             "--body-imagery-dir=io=/maps/io","--body-imagery-dir",
                             "earth=/maps/earth"};
        assert(parse_command_line(static_cast<int>(std::size(tiled)),tiled,planetary,err));
        assert(planetary.body_imagery_dirs[static_cast<std::size_t>(BodyId::Mars)]=="/maps/mars");
        assert(planetary.body_imagery_dirs[static_cast<std::size_t>(BodyId::Io)]=="/maps/io");
        assert(planetary.body_imagery_dirs[static_cast<std::size_t>(BodyId::Earth)]=="/maps/earth");
        const char* bad_tiles[]={"globtoo","--body-imagery-dir","jupiter=/maps/jupiter"};
        assert(!parse_command_line(3,bad_tiles,planetary,err));
        const char* empty_tiles[]={"globtoo","--body-imagery-dir=io="};
        assert(!parse_command_line(2,empty_tiles,planetary,err));
        const char* invalid[]={"globtoo","--body","venus"};
        assert(!parse_command_line(static_cast<int>(std::size(invalid)),invalid,planetary,err));
    }

    {
        CommandLine feature;
        std::string err;
        const char* args[]={"globtoo","--focus","LoKi PaTeRa"};
        assert(parse_command_line(3,args,feature,err));
        assert(feature.body==BodyId::Io);
        assert(feature.center.has_value());
        assert(std::abs(feature.center->first-51.2)<1.e-8);
        assert(std::abs(feature.center->second-12.97)<1.e-8);
        const char* ordered[]={"globtoo","--focus","Olympus Mons","--body","mars"};
        assert(parse_command_line(5,ordered,feature,err));
        assert(feature.body==BodyId::Mars);
        const char* reverse[]={"globtoo","--body","mars","--focus","Olympus Mons"};
        assert(parse_command_line(5,reverse,feature,err));
        const char* wrong[]={"globtoo","--focus","Loki Patera","--body","mars"};
        assert(!parse_command_line(5,wrong,feature,err));
        const char* clash[]={"globtoo","--center","0,0","--focus","Tycho"};
        assert(!parse_command_line(5,clash,feature,err));
        const char* unknown[]={"globtoo","--focus","not-a-crater"};
        assert(!parse_command_line(3,unknown,feature,err));
        const char* list[]={"globtoo","--list-features"};
        assert(parse_command_line(2,list,feature,err));
        assert(feature.action==CommandAction::ListFeatures);
        const char* extra[]={"globtoo","--list-features","--body","mars"};
        assert(!parse_command_line(4,extra,feature,err));
    }

    {
        CommandLine download;
        std::string err;
        const char* args[]={"globtoo","--download-asset","earth"};
        assert(parse_command_line(3,args,download,err));
        assert(download.action==CommandAction::DownloadAsset);
        assert(download.download_asset=="earth");
        const char* equal[]={"globtoo","--download-asset=earth"};
        assert(parse_command_line(2,equal,download,err));
        const char* unknown[]={"globtoo","--download-asset","mars"};
        assert(!parse_command_line(3,unknown,download,err));
        const char* extra[]={"globtoo","--download-asset","earth","--layer","relief"};
        assert(!parse_command_line(5,extra,download,err));
    }

    CommandLine parsed;
    std::string error;
    const char* argv[]={"globtoo", "--marker-color", "#1a2B3c", "--marker-size", "12",
                        "--marker", "52.52,13.405|Berlin", "--label", "Berlin Center",
                        "-m", "geo:48.8583,2.2945", "--marker-lonlat", "-0.12,51.5",
                        "--center", "40,-74", "--texture", "earth.jpg"};
    assert(parse_command_line(static_cast<int>(std::size(argv)),argv,parsed,error));
    assert(parsed.markers.size()==3);
    assert(parsed.markers[0].label=="Berlin Center");
    assert(parsed.markers[0].color==0x1a2b3c);
    assert(parsed.markers[0].radius==12.f);
    assert(std::abs(parsed.markers[2].latitude-51.5)<1e-8);
    assert(parsed.center && parsed.center->first==-74 && parsed.center->second==40);
    assert(parsed.texture_path=="earth.jpg");
    assert(parsed.action==CommandAction::Run);

    const char* equals[]={"globtoo","--marker=geo:52.52,13.405",
                           "--label=Berlin", "--center=52.52,13.405",
                           "--marker-color=#abcdef", "--marker-size=6"};
    assert(parse_command_line(static_cast<int>(std::size(equals)),equals,parsed,error));
    assert(parsed.markers.size()==1 && parsed.markers[0].label=="Berlin");
    assert(parsed.center && parsed.center->first==13.405);
    const char* after_double_dash[]={"globtoo","--","-earth.jpg"};
    assert(parse_command_line(3,after_double_dash,parsed,error));
    assert(parsed.texture_path=="-earth.jpg");
    const char* bad[]={"globtoo","--label","orphan"};
    assert(!parse_command_line(3,bad,parsed,error));
    const char* missing[]={"globtoo","--marker"};
    assert(!parse_command_line(2,missing,parsed,error));
    const char* invalid_color[]={"globtoo","--marker-color","green"};
    assert(!parse_command_line(3,invalid_color,parsed,error));
    const char* invalid_size[]={"globtoo","--marker-size","0"};
    assert(!parse_command_line(3,invalid_size,parsed,error));
    const char* imagery[]={"globtoo","--imagery-dir=/tmp/imagery"};
    assert(parse_command_line(2,imagery,parsed,error));
    assert(parsed.imagery_dir=="/tmp/imagery");
    const char* missing_imagery[]={"globtoo","--imagery-dir"};
    assert(!parse_command_line(2,missing_imagery,parsed,error));
    const char* relief[]={"globtoo","--layer=relief","--relief-dir","/tmp/relief"};
    assert(parse_command_line(4,relief,parsed,error));
    assert(parsed.relief_dir=="/tmp/relief");
    assert(parsed.surface_layer=="relief");
    const char* elevation[]={"globtoo","--layer=elevation", "--elevation-dir", "/tmp/topography"};
    assert(parse_command_line(4,elevation,parsed,error));
    assert(parsed.surface_layer=="elevation" && parsed.elevation_dir=="/tmp/topography");
    const char* missing_elevation_dir[]={"globtoo","--elevation-dir"};
    assert(!parse_command_line(2,missing_elevation_dir,parsed,error));
    const char* population[]={"globtoo","--layer=population","--population-dir","/tmp/population"};
    assert(parse_command_line(4,population,parsed,error));
    assert(parsed.surface_layer=="population" && parsed.population_dir=="/tmp/population");
    const char* missing_population[]={"globtoo","--population-dir"};
    assert(!parse_command_line(2,missing_population,parsed,error));
    const char* bad_layer[]={"globtoo","--layer","precipitation"};
    assert(!parse_command_line(3,bad_layer,parsed,error));
    const char* help[]={"globtoo","--help"};
    assert(parse_command_line(2,help,parsed,error) && parsed.action==CommandAction::Help);
    const char* version[]={"globtoo","--version"};
    assert(parse_command_line(2,version,parsed,error) && parsed.action==CommandAction::Version);
    assert(command_line_help("globtoo").find("ISO 6709")!=std::string::npos);

    const auto path=std::filesystem::temp_directory_path()/"globtoo-markers-regression.csv";
    {
        std::ofstream csv(path);
        csv<<"# Locations from a geographic export\n";
        csv<<"latitude,longitude,label\n";
        csv<<"52.52,13.405,Berlin\n";
        csv<<"40.6892,-74.0445,\"Liberty, Island\"\n";
        csv<<"# comment\n";
        csv<<"51:30:26N,0:07:39W|London\n";
    }
    const std::string p=path.string();
    const char* fileargs[]={"globtoo","--marker-file",p.c_str()};
    assert(parse_command_line(3,fileargs,parsed,error));
    assert(parsed.markers.size()==3);
    assert(parsed.markers[0].label=="Berlin");
    assert(parsed.markers[1].label=="Liberty, Island");
    assert(parsed.markers[2].label=="London");
    std::filesystem::remove(path);
    std::cout<<"marker coordinate and CLI tests passed\n";
}

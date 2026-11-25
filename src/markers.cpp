// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "markers.hpp"
#include "planetary_features.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <locale>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <utility>

namespace {
std::string trim(std::string_view in) {
    const auto space=[](unsigned char c) { return std::isspace(c)!=0; };
    while (!in.empty() && space(static_cast<unsigned char>(in.front()))) in.remove_prefix(1);
    while (!in.empty() && space(static_cast<unsigned char>(in.back()))) in.remove_suffix(1);
    return std::string(in);
}

void replace_all(std::string& s, std::string_view needle, std::string_view value) {
    for (std::size_t p=0; (p=s.find(needle,p))!=std::string::npos; p+=value.size())
        s.replace(p,needle.size(),value);
}

std::string normalized(std::string s) {
    // UTF-8 degree/prime symbols are common when GPS coordinates are copied
    // from Wikipedia or a phone. Keep this independent of the system locale.
    replace_all(s,"\xc2\xb0",":");    // °
    replace_all(s,"\xe2\x80\xb2",":"); // ′
    replace_all(s,"\xe2\x80\xb3",":"); // ″
    replace_all(s,"\xe2\x80\x99",":"); // ’
    replace_all(s,"\xe2\x80\x9d",":"); // ”
    replace_all(s,"\xe2\x88\x92","-"); // unicode minus
    return s;
}

template<typename Float>
bool number(std::string_view s, Float& result) {
    if (s.empty()) return false;
    if (s.front()=='+') s.remove_prefix(1);
    if (s.empty()) return false;
    const auto* begin=s.data();
    const auto* end=begin+s.size();
    if constexpr (requires { std::from_chars(begin,end,result); }) {
        const auto r=std::from_chars(begin,end,result);
        return r.ec==std::errc{} && r.ptr==end && std::isfinite(result);
    } else {
        // ArkOS ships the GCC 9 library, whose from_chars is integer-only.
        std::istringstream stream{std::string(s)};
        stream.imbue(std::locale::classic());
        return bool(stream >> std::noskipws >> result) && stream.eof() &&
               std::isfinite(result);
    }
}

bool is_direction(char c) {
    c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return c=='N' || c=='S' || c=='E' || c=='W';
}
char direction(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    if (s.empty()) return 0;
    if (is_direction(s.front())) return static_cast<char>(std::toupper(s.front()));
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    if (!s.empty() && is_direction(s.back()))
        return static_cast<char>(std::toupper(s.back()));
    return 0;
}

struct Angle { double value=0.; char hemisphere=0; };
bool angle(std::string_view raw, bool latitude, Angle& result) {
    std::string s=trim(normalized(std::string(raw)));
    if (s.empty()) return false;
    char hemisphere=0;
    if (is_direction(s.front())) {
        hemisphere=static_cast<char>(std::toupper(s.front()));
        s=trim(std::string_view(s).substr(1));
    }
    if (!s.empty() && is_direction(s.back())) {
        const char suffix=static_cast<char>(std::toupper(s.back()));
        if (hemisphere && hemisphere!=suffix) return false;
        hemisphere=suffix;
        s=trim(std::string_view(s).substr(0,s.size()-1));
    }
    if (hemisphere && (latitude ? hemisphere!='N' && hemisphere!='S' :
                                   hemisphere!='E' && hemisphere!='W')) return false;
    const bool negative=!s.empty() && s.front()=='-';
    if (hemisphere && negative && (hemisphere=='N' || hemisphere=='E')) return false;
    if (hemisphere && !negative && !s.empty() && s.front()=='+' &&
        (hemisphere=='S' || hemisphere=='W')) return false;
    std::vector<double> pieces;
    for (std::size_t start=0;start<s.size();) {
        const auto separator=[](char c) {
            return std::isspace(static_cast<unsigned char>(c)) ||
                c==':' || c=='\'' || c=='"' || c=='d' || c=='D' || c=='m' || c=='M';
        };
        if (separator(s[start])) { ++start; continue; }
        std::size_t end=start;
        while (end<s.size() && !separator(s[end])) ++end;
        double n=0;
        if (!number(std::string_view(s).substr(start,end-start),n)) return false;
        pieces.push_back(n);
        start=end;
    }
    if (pieces.empty() || pieces.size()>3) return false;
    const double degrees=std::abs(pieces[0]);
    if (pieces.size()>1) {
        if (std::floor(degrees)!=degrees || pieces[1]<0 || pieces[1]>=60) return false;
        if (pieces.size()==3 && (std::floor(pieces[1])!=pieces[1] ||
                                 pieces[2]<0 || pieces[2]>=60)) return false;
    }
    double value=degrees;
    if (pieces.size()>1) value+=pieces[1]/60.;
    if (pieces.size()>2) value+=pieces[2]/3600.;
    if (value>(latitude ? 90. : 180.)) return false;
    if (hemisphere=='S' || hemisphere=='W' || negative) value=-value;
    result={value,hemisphere};
    return true;
}

bool iso_angle(std::string_view s, bool latitude, double& value) {
    // ISO 6709 uses signed decimal degrees or compact DDMM[SS] and
    // DDDMM[SS], optionally with a fractional final component.
    const bool neg=!s.empty() && s.front()=='-';
    if (!s.empty() && (s.front()=='+' || neg)) s.remove_prefix(1);
    const std::size_t width=latitude ? 2u : 3u;
    const auto dot=s.find('.');
    const auto digits=dot==std::string_view::npos ? s.size() : dot;
    double raw=0;
    if (digits==width) {
        if (!number(s,raw)) return false;
        value=raw;
    } else if (digits==width+2 || digits==width+4) {
        double degrees=0,minutes=0,seconds=0;
        if (!number(s.substr(0,width),degrees)) return false;
        if (digits==width+2) {
            if (!number(s.substr(width),minutes) || minutes>=60) return false;
        } else {
            if (!number(s.substr(width,2),minutes) ||
                !number(s.substr(width+2),seconds) || minutes>=60 || seconds>=60)
                return false;
        }
        value=degrees+minutes/60.+seconds/3600.;
    } else return false;
    if (value>(latitude ? 90. : 180.)) return false;
    if (neg) value=-value;
    return true;
}

bool iso_pair(std::string_view input, double& lat, double& lon) {
    if (input.empty() || (input.front()!='+' && input.front()!='-')) return false;
    if (input.back()=='/') input.remove_suffix(1);
    const auto boundary=input.find_first_of("+-",1);
    if (boundary==std::string_view::npos) return false;
    const auto extra=input.find_first_of("+-",boundary+1);
    if (extra!=std::string_view::npos) return false;
    return iso_angle(input.substr(0,boundary),true,lat) &&
           iso_angle(input.substr(boundary),false,lon);
}

bool pair_parts(std::string_view first, std::string_view second,
                CoordinateOrder order, double& lat, double& lon) {
    const char a=direction(first), b=direction(second);
    if (a && b && ((a=='N'||a=='S')==(b=='N'||b=='S'))) return false;
    // Explicit directional suffixes take precedence over decimal positional
    // defaults (e.g. "W0.12 N51.5" is longitude first).
    if (a) order=(a=='E'||a=='W') ? CoordinateOrder::LongitudeLatitude :
                                   CoordinateOrder::LatitudeLongitude;
    else if (b) order=(b=='E'||b=='W') ? CoordinateOrder::LatitudeLongitude :
                                       CoordinateOrder::LongitudeLatitude;
    Angle one,two;
    if (order==CoordinateOrder::LatitudeLongitude) {
        if (!angle(first,true,one) || !angle(second,false,two)) return false;
        lat=one.value; lon=two.value;
    } else {
        if (!angle(first,false,one) || !angle(second,true,two)) return false;
        lat=two.value; lon=one.value;
    }
    return true;
}

bool parse_marker(std::string_view raw, CoordinateOrder order,
                  Marker& marker, std::string& error) {
    std::string input=trim(raw);
    const auto divider=input.find('|');
    if (divider!=std::string::npos) {
        marker.label=trim(std::string_view(input).substr(divider+1));
        input=trim(std::string_view(input).substr(0,divider));
    }
    if (!parse_coordinates(input,order,marker.latitude,marker.longitude,error)) return false;
    return true;
}

bool rgb(std::string_view input, std::uint32_t& out) {
    if (!input.empty() && input.front()=='#') input.remove_prefix(1);
    if (input.size()!=6) return false;
    std::uint32_t value=0;
    for (char c : input) {
        value<<=4;
        if (c>='0' && c<='9') value+=c-'0';
        else if (c>='a' && c<='f') value+=c-'a'+10;
        else if (c>='A' && c<='F') value+=c-'A'+10;
        else return false;
    }
    out=value;
    return true;
}

// Parse a simple RFC-4180 CSV row, used by --marker-file only. In other
// contexts pipe-delimited names are less ambiguous than CSV labels.
bool csv_fields(std::string_view text, std::vector<std::string>& fields) {
    std::string field;
    bool quoted=false;
    bool after_quote=false;
    for (std::size_t i=0;i<text.size();i++) {
        const char c=text[i];
        if (quoted) {
            if (c=='"') {
                if (i+1<text.size() && text[i+1]=='"') { field+='"'; ++i; }
                else { quoted=false; after_quote=true; }
            } else field+=c;
        } else if (c==',' || c=='\t') {
            fields.push_back(trim(field)); field.clear(); after_quote=false;
        } else if (c=='"' && field.empty() && !after_quote) quoted=true;
        else if (after_quote && !std::isspace(static_cast<unsigned char>(c))) return false;
        else if (c=='"') return false;
        else field+=c;
    }
    if (quoted) return false;
    fields.push_back(trim(field));
    return true;
}

bool unquoted_pipe(std::string_view s) {
    bool quoted=false;
    for (std::size_t i=0;i<s.size();++i) {
        if (s[i]=='"') {
            if (quoted && i+1<s.size() && s[i+1]=='"') ++i;
            else quoted=!quoted;
        } else if (s[i]=='|' && !quoted) return true;
    }
    return false;
}

bool read_marker_file(const std::string& path, std::uint32_t color, float radius,
                      std::vector<Marker>& output, std::string& error) {
    std::ifstream file(path);
    if (!file) { error="cannot open marker file: " + path; return false; }
    std::string line;
    std::size_t line_number=0;
    bool header_allowed=true;
    while (std::getline(file,line)) {
        ++line_number;
        const auto value=trim(line);
        if (value.empty() || value.front()=='#') continue;
        Marker m;
        m.color=color; m.radius=radius;
        std::string issue;
        if (unquoted_pipe(value)) {
            if (!parse_marker(value,CoordinateOrder::LatitudeLongitude,m,issue)) {
                error=path+":"+std::to_string(line_number)+": "+issue;
                return false;
            }
        } else {
            std::vector<std::string> fields;
            if (!csv_fields(value,fields) || fields.size()>3) {
                error=path+":"+std::to_string(line_number)+": expected lat,lon[,label]";
                return false;
            }
            if (header_allowed && fields.size()>=2 &&
                (fields[0]=="latitude" || fields[0]=="lat") &&
                (fields[1]=="longitude" || fields[1]=="lon")) {
                header_allowed=false;
                continue;
            }
            if (fields.size()>=2) {
                if (!parse_coordinates(fields[0]+","+fields[1],
                                       CoordinateOrder::LatitudeLongitude,
                                       m.latitude,m.longitude,issue)) {
                    error=path+":"+std::to_string(line_number)+": "+issue;
                    return false;
                }
                if (fields.size()==3) m.label=fields[2];
            } else if (!parse_marker(value,CoordinateOrder::LatitudeLongitude,m,issue)) {
                error=path+":"+std::to_string(line_number)+": "+issue;
                return false;
            }
        }
        header_allowed=false;
        output.push_back(std::move(m));
    }
    if (file.bad()) { error="error reading marker file: "+path; return false; }
    return true;
}
} // namespace

bool parse_coordinates(std::string_view raw, CoordinateOrder order,
                       double& latitude, double& longitude, std::string& error) {
    std::string s=trim(raw);
    const bool geo_uri=s.size()>=4 && s.substr(0,4)=="geo:";
    if (geo_uri) {
        s=s.substr(4);
        order=CoordinateOrder::LatitudeLongitude;
        const auto options=s.find(';');
        if (options!=std::string::npos) s.resize(options);
        // RFC 5870's altitude is optional; markers are pinned to the surface.
        const auto first=s.find(',');
        if (first!=std::string::npos) {
            const auto second=s.find(',',first+1);
            if (second!=std::string::npos) {
                double altitude=0;
                if (!number(trim(std::string_view(s).substr(second+1)),altitude)) {
                    error="invalid geo: altitude"; return false;
                }
                s.resize(second);
            }
        }
    }
    if (iso_pair(s,latitude,longitude)) return true;
    const auto comma=s.find(',');
    if (comma!=std::string::npos) {
        if (s.find(',',comma+1)==std::string::npos &&
            pair_parts(std::string_view(s).substr(0,comma),
                       std::string_view(s).substr(comma+1),order,latitude,longitude)) return true;
    } else {
        // Supports "N51.5 W0.1", "51 30 26 N 0 7 39 W" and plain
        // "51.5 -0.1". Try all whitespace boundaries; only accept an
        // unambiguous split to avoid silently interpreting DMS as decimals.
        int matches=0;
        double candidate_lat=0,candidate_lon=0;
        for (std::size_t i=1;i<s.size();i++) {
            if (!std::isspace(static_cast<unsigned char>(s[i])) ||
                std::isspace(static_cast<unsigned char>(s[i-1]))) continue;
            double lat=0,lon=0;
            if (pair_parts(std::string_view(s).substr(0,i),
                           std::string_view(s).substr(i),order,lat,lon)) {
                ++matches; candidate_lat=lat; candidate_lon=lon;
            }
        }
        if (matches==1) { latitude=candidate_lat; longitude=candidate_lon; return true; }
        if (matches>1) { error="ambiguous coordinates; separate latitude and longitude with a comma"; return false; }
    }
    error="invalid coordinates (expected latitude,longitude; see --help)";
    return false;
}

bool parse_command_line(int argc, const char* const* argv,
                        CommandLine& output, std::string& error) {
    output={};
    std::uint32_t color=0xffc54b;
    float radius=7.f;
    bool positional_done=false;
    bool explicit_body=false;
    bool explicit_center=false;
    for (int i=1;i<argc;i++) {
        const std::string_view arg=argv[i];
        if (!positional_done && (arg=="--help" || arg=="-h")) {
            output.action=CommandAction::Help;
            return true;
        }
        if (!positional_done && (arg=="--version" || arg=="-V")) {
            output.action=CommandAction::Version;
            return true;
        }
        if (!positional_done && arg=="--") { positional_done=true; continue; }
        // Both --option=value and --option value are supported. The latter
        // intentionally consumes signed coordinate strings beginning with '-'.
        const auto equals=arg.find('=');
        const auto key=(!positional_done && arg.substr(0, 2) == "--" &&
                        equals!=std::string_view::npos) ? arg.substr(0,equals) : arg;
        const auto next=[&]() -> std::optional<std::string_view> {
            if (key.size()!=arg.size()) return arg.substr(equals+1);
            if (i+1<argc) return std::string_view(argv[++i]);
            error="missing argument for "+std::string(key);
            return std::nullopt;
        };
        if (!positional_done &&
            (key=="--marker" || key=="-m" || key=="--marker-lonlat")) {
            const auto value=next();
            if (!value) return false;
            Marker marker;
            marker.color=color;
            marker.radius=radius;
            if (!parse_marker(*value,key=="--marker-lonlat" ?
                CoordinateOrder::LongitudeLatitude : CoordinateOrder::LatitudeLongitude,
                marker,error)) return false;
            output.markers.push_back(std::move(marker));
        } else if (!positional_done && (key=="--label" || key=="-l")) {
            const auto value=next();
            if (!value) return false;
            if (output.markers.empty()) { error="--label needs a preceding marker"; return false; }
            output.markers.back().label=*value;
        } else if (!positional_done && key=="--marker-file") {
            const auto value=next();
            if (!value) return false;
            if (!read_marker_file(std::string(*value),color,radius,output.markers,error))
                return false;
        } else if (!positional_done && key=="--marker-color") {
            const auto value=next();
            if (!value) return false;
            if (!rgb(*value,color)) { error="--marker-color needs #RRGGBB"; return false; }
        } else if (!positional_done && key=="--marker-size") {
            const auto value=next();
            if (!value) return false;
            double parsed=0;
            if (!number(*value,parsed) || parsed<2 || parsed>32) {
                error="--marker-size must be between 2 and 32 pixels";
                return false;
            }
            radius=static_cast<float>(parsed);
        } else if (!positional_done && key=="--center") {
            const auto value=next();
            if (!value) return false;
            double lat=0,lon=0;
            if (!parse_coordinates(*value,CoordinateOrder::LatitudeLongitude,lat,lon,error))
                return false;
            output.center=std::make_pair(lon,lat);
            explicit_center=true;
        } else if (!positional_done && key=="--focus") {
            const auto value=next();
            if (!value || value->empty()) { error="--focus requires a feature name"; return false; }
            output.focus_name=*value;
        } else if (!positional_done && key=="--list-bodies") {
            output.action=CommandAction::ListBodies;
        } else if (!positional_done && key=="--list-features") {
            output.action=CommandAction::ListFeatures;
        } else if (!positional_done && key=="--download-asset") {
            const auto value=next();
            if (!value) return false;
            if (*value!="earth") {
                error="--download-asset currently supports only earth";
                return false;
            }
            output.action=CommandAction::DownloadAsset;
            output.download_asset=*value;
        } else if (!positional_done && key=="--body") {
            const auto value=next();
            if (!value) return false;
            if (!parse_body(*value,output.body)) {
                error="unknown --body "+std::string(*value)+" (see --list-bodies)"; return false;
            }
            explicit_body=true;
        } else if (!positional_done && key=="--body-texture") {
            const auto value=next();
            if (!value) return false;
            const auto sep=value->find('=');
            BodyId id=BodyId::Earth;
            if (sep==std::string_view::npos || sep==0 || sep+1==value->size() ||
                !parse_body(value->substr(0,sep),id)) {
                error="--body-texture expects BODY=FILE (see --list-bodies)"; return false;
            }
            output.body_textures[body_index(id)] =
                std::string(value->substr(sep+1));
        } else if (!positional_done && key=="--body-imagery-dir") {
            const auto value=next();
            if (!value) return false;
            const auto sep=value->find('=');
            BodyId id=BodyId::Earth;
            if (sep==std::string_view::npos || sep==0 || sep+1==value->size() ||
                !parse_body(value->substr(0,sep),id)) {
                error="--body-imagery-dir expects BODY=DIR (see --list-bodies)"; return false;
            }
            output.body_imagery_dirs[body_index(id)]=
                std::string(value->substr(sep+1));
        } else if (!positional_done && key=="--layer") {
            const auto value=next();
            if (!value) return false;
            if (*value!="imagery" && *value!="relief" && *value!="elevation" &&
                *value!="population") {
                error="--layer must be imagery, relief, elevation or population"; return false;
            }
            output.surface_layer=*value;
        } else if (!positional_done && key=="--elevation-dir") {
            const auto value=next();
            if (!value || value->empty()) { error="--elevation-dir requires a directory"; return false; }
            output.elevation_dir=*value;
        } else if (!positional_done && key=="--population-dir") {
            const auto value=next();
            if (!value || value->empty()) { error="--population-dir requires a directory"; return false; }
            output.population_dir=*value;
        } else if (!positional_done && key=="--imagery-dir") {
            const auto value=next();
            if (!value || value->empty()) { error="--imagery-dir requires a directory"; return false; }
            output.imagery_dir=*value;
        } else if (!positional_done && key=="--relief-dir") {
            const auto value=next();
            if (!value || value->empty()) { error="--relief-dir requires a directory"; return false; }
            output.relief_dir=*value;
        } else if (!positional_done && key=="--texture") {
            const auto value=next();
            if (!value) return false;
            output.texture_path=*value;
        } else if (positional_done || arg.empty() || arg.front()!='-') {
            if (!output.texture_path.empty()) { error="multiple texture files specified"; return false; }
            output.texture_path=arg;
        } else {
            error="unknown option: "+std::string(arg);
            return false;
        }
    }
    if (!output.focus_name.empty()) {
        const auto* feature=find_planetary_feature(output.focus_name);
        if (!feature) { error="unknown planetary feature: "+output.focus_name+
            " (see --list-features)"; return false; }
        if (explicit_body && output.body!=feature->body) {
            error="--focus feature belongs to a different --body"; return false;
        }
        if (explicit_center) {
            error="--center and --focus are mutually exclusive"; return false;
        }
        output.body=feature->body;
        output.center=std::make_pair(feature_east_longitude(*feature),feature->source_latitude);
    }
    if (output.action==CommandAction::ListFeatures && argc!=2) {
        error="--list-features must be used by itself";
        return false;
    }
    if (output.action==CommandAction::ListBodies && argc!=2) {
        error="--list-bodies must be used by itself";
        return false;
    }
    if (output.action==CommandAction::DownloadAsset &&
        !(argc==3 || (argc==2 && std::string_view(argv[1]).substr(0, sizeof("--download-asset=")-1) == "--download-asset="))) {
        error="--download-asset must be used by itself";
        return false;
    }
    return true;
}

std::string body_list_text() {
    std::string text;
    char line[160];
    for (const auto& body : body_catalog) {
        const auto kind=body_kind_name(body.kind);
        const auto parent=body.parent==body.id ? std::string_view("Sun") :
            body_info(body.parent).name;
        std::snprintf(line,sizeof(line),"%-10.*s %-10.*s %-12.*s %-8.*s %9.1f km%s\n",
                      static_cast<int>(body.slug.size()),body.slug.data(),
                      static_cast<int>(body.name.size()),body.name.data(),
                      static_cast<int>(kind.size()),kind.data(),
                      static_cast<int>(parent.size()),parent.data(),
                      body.mean_radius_km,body.solid_surface ? "" : "  (cloud tops)");
        text+=line;
    }
    return text;
}

std::string command_line_help(std::string_view program) {
    std::string help;
    help += "Usage: "+std::string(program)+" [options] [earth-texture.jpg]\n\n";
    help += "Markers (repeat any number of times):\n";
    help += "  -m, --marker COORDS       Latitude,longitude with optional |Label\n";
    help += "      --marker-lonlat COORDS Longitude,latitude (for GIS-style data)\n";
    help += "  -l, --label TEXT          Set label of the preceding marker\n";
    help += "      --marker-file FILE    Load markers from CSV: lat,lon[,label]\n";
    help += "                            Also accepts COORDS | Label per line\n";
    help += "      --marker-color HEX    #RRGGBB for subsequent markers (default #FFC54B)\n";
    help += "      --marker-size PX      Radius 2..32 for subsequent markers (default 7)\n";
    help += "      --center COORDS       Center initial view here (otherwise local timezone)\n";
    help += "      --body NAME           Initial body, e.g. moon, mars, titan (default earth)\n";
    help += "      --list-bodies         List the selectable planets and moons (no SDL)\n";
    help += "      --focus NAME          Start on an IAU-named landmark; selects its body\n";
    help += "      --list-features       List built-in planetary landmarks (no SDL)\n";
    help += "      --body-texture B=FILE Set a body's equirectangular image (repeatable)\n";
    help += "      --texture FILE        Texture of the initial body (or positional path)\n";
    help += "      --download-asset earth Fetch verified Earth image to XDG cache and exit\n";
    help += "                            Only available in curl-enabled (lite) builds\n";
    help += "      --layer NAME          imagery, relief, elevation, population (Earth only)\n";
    help += "      --imagery-dir DIR     High-resolution Earth imagery tiles\n";
    help += "      --body-imagery-dir B=DIR High-resolution tiles for body B (repeatable)\n";
    help += "      --relief-dir DIR      Directory with visual relief tile data\n";
    help += "      --elevation-dir DIR   Directory with measured elevation tiles/legend\n";
    help += "      --population-dir DIR  Directory with measured density tiles/legend\n";
    help += "  -h, --help                Display this help without starting SDL\n";
    help += "  -V, --version             Print full build version and exit\n\n";
    help += "Coordinate forms (latitude first unless explicitly marked E/W):\n";
    help += "  52.5200,13.4050           Signed decimal degrees, north/east positive\n";
    help += "  51.5074 N, 0.1278 W       Compass suffixes or prefixes (N/S/E/W)\n";
    help += "  51:30:26N, 0:07:39W      Degrees/minutes/seconds (also ° ' \" symbols)\n";
    help += "  51°30.5'N, 0°7.25'W      Degrees and decimal minutes\n";
    help += "  geo:52.52,13.405         RFC 5870 Geo URI (altitude ignored)\n";
    help += "  +52.52+013.405/          ISO 6709 signed coordinate pair\n\n";
    help += "Planetary maps must use east-positive longitude, north at image top.\n";
    help += "Earth-only layers are hidden on other bodies; M / Shift+M cycle bodies.\n\n";
    help += "Examples:\n";
    help += "  globtoo --marker '52.5200,13.4050|Berlin'\n";
    help += "  globtoo --marker 'N40°41′21″, W74°02′40″' --label 'Liberty Island'\n";
    help += "  globtoo --marker-color '#ff5555' --marker-size 10 --marker 'geo:48.8583,2.2945'\n";
    help += "  globtoo --marker-file places.csv --center '48.8583,2.2945'\n";
    help += "  globtoo --body mars --texture /maps/mars-east-positive.jpg\n";
    help += "  globtoo --body io --body-imagery-dir io=/maps/io-tiles\n";
    return help;
}

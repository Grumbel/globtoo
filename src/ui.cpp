// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui.hpp"
#include "scene.hpp"
#include "grid_metrics.hpp"
#include "geocoordinates.hpp"
#include "planetary_features.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <string>
#include <tuple>

namespace {
struct Color { float r,g,b,a; };
constexpr Color panel{.035f,.055f,.085f,.90f};
constexpr Color panel_edge{.32f,.45f,.58f,.38f};
constexpr Color text{.92f,.96f,1.f,1.f};
constexpr Color muted{.62f,.70f,.79f,1.f};
constexpr Color dimmed{.62f,.70f,.79f,.42f};
constexpr Color active{.18f,.36f,.53f,1.f};
constexpr Color accent{.42f,.74f,1.f,1.f};
constexpr Color inactive{.10f,.165f,.24f,1.f};
constexpr Color needle_north{.98f,.38f,.28f,1.f};
constexpr Color needle_south{.43f,.67f,.86f,1.f};
constexpr float pi = 3.14159265358979323846f;

Color lighten(Color c, float t) {
    return {c.r+(1.f-c.r)*t, c.g+(1.f-c.g)*t, c.b+(1.f-c.b)*t, c.a};
}

struct Bounds { float x,y,w,h; };
bool contains(Bounds b, float x,float y) {
    return x>=b.x && x<b.x+b.w && y>=b.y && y<b.y+b.h;
}
bool overlaps(Bounds a, Bounds b) {
    return a.x<b.x+b.w && a.x+a.w>b.x &&
           a.y<b.y+b.h && a.y+a.h>b.y;
}

// ---- Layout --------------------------------------------------------------
// All controls are anchored to the bottom edge: the layer toolbar on the
// left, the compass on the right and the scale bar between them (or above
// the compass when the window is too narrow). The raster legend, the only
// optional panel, takes the otherwise empty top-right corner.
constexpr float margin=8.f, pad=8.f, gap=5.f;
constexpr float button_h=30.f, header_h=24.f;
constexpr float compass_button_h=28.f, compass_gap=4.f;
constexpr float rose_area=100.f, compass_column=136.f;
constexpr float scale_w=184.f, scale_h=66.f;
constexpr float legend_w=264.f, legend_h=257.f;
constexpr float toolbar_h=pad+header_h+gap+3*button_h+2*gap+pad;
constexpr std::size_t body_name_chars=9; // "Enceladus", the longest name

// Width of a run of bitmap glyphs without the trailing inter-glyph space.
float text_width(std::string_view s, int scale) {
    return s.empty() ? 0.f : static_cast<float>(s.size()*6*scale-scale);
}
float button_width(std::string_view label, int scale=2) {
    return std::max(30.f, text_width(label,scale)+18.f);
}

struct Button { std::string_view label; Bounds b; UiCommand command; int font=2; };
struct RowItem { std::string_view label; UiCommand command; };
using C = UiCommand;
constexpr RowItem projection_row[]{
    {"3D GLOBE",C::Globe},{"PLATE",C::Plate},{"MERCATOR",C::Mercator},
    {"MOLLWEIDE",C::Mollweide},{"HAMMER",C::Hammer}};
constexpr RowItem earth_layer_row[]{
    {"SHADING",C::Shading},{"TEXTURE",C::Texture},{"COUNTRIES",C::Countries},
    {"BORDERS",C::Borders},{"NAMES",C::Names},{"GRID",C::Grid}};
constexpr RowItem earth_feature_row[]{
    {"CAPITALS",C::Capitals},{"CITIES",C::Cities},{"RIVERS",C::Rivers},
    {"RELIEF",C::Relief},{"ELEV",C::Elevation},{"POP",C::Population}};
constexpr RowItem body_layer_row[]{
    {"SHADING",C::Shading},{"TEXTURE",C::Texture},{"GRID",C::Grid}};
constexpr RowItem landmark_row[]{{"LANDMARKS",C::Landmarks}};

template <std::size_t N>
float row_width(const RowItem (&row)[N]) {
    float w=0;
    for (const auto& item : row) w+=button_width(item.label)+gap;
    return w-gap;
}
// The panel keeps the Earth width for every body, so switching bodies never
// moves the compass or scale bar.
float toolbar_width() {
    const float zoom=2*30.f+gap+2*gap; // separated - / + pair
    return pad+std::max({row_width(projection_row)+zoom,
                         row_width(earth_layer_row),
                         row_width(earth_feature_row)})+pad;
}

struct Panels { Bounds toolbar, compass, scale, legend; };
Panels panels(const Ui& ui) {
    const float vw=static_cast<float>(ui.viewport_w);
    const float vh=static_cast<float>(ui.viewport_h);
    Panels p{};
    p.toolbar={margin,std::max(margin,vh-margin-toolbar_h),toolbar_width(),toolbar_h};
    const float compass_w=pad+rose_area+pad+compass_column+pad;
    p.compass={std::max(margin,vw-margin-compass_w),p.toolbar.y,compass_w,toolbar_h};
    const bool narrow=p.toolbar.x+p.toolbar.w+margin>p.compass.x;
    if (narrow) p.compass.y=std::max(margin,p.toolbar.y-margin-toolbar_h);
    // Prefer the free bottom strip between the toolbar and compass.
    const float beside=p.compass.x-margin-scale_w;
    if (!narrow && beside>=p.toolbar.x+p.toolbar.w+margin)
        p.scale={beside,p.compass.y+p.compass.h-scale_h,scale_w,scale_h};
    else
        p.scale={p.compass.x+p.compass.w-scale_w,
                 std::max(margin,p.compass.y-margin-scale_h),scale_w,scale_h};
    p.legend={std::max(margin,vw-margin-legend_w),margin,legend_w,legend_h};
    return p;
}

struct Layout {
    Panels panel;
    Bounds rose;            // clickable compass rose (also North Up)
    float rose_x=0, rose_y=0;
    Bounds body_name;       // between the < and > body buttons
    std::array<Button,32> buttons{};
    std::size_t count=0;
    void add(std::string_view label, Bounds b, UiCommand command, int font=2) {
        buttons[count++]={label,b,command,font};
    }
    auto begin() const { return buttons.begin(); }
    auto end() const { return buttons.begin()+count; }
};

Layout layout(const Ui& ui, BodyId body) {
    Layout l{};
    l.panel=panels(ui);
    const Bounds t=l.panel.toolbar;
    // Header: application name and the body selector.
    float x=t.x+pad+text_width("GLOBTOO",2)+18.f;
    const float hy=t.y+pad;
    l.add("<",{x,hy,26.f,header_h},C::PreviousBody);
    x+=26.f+4.f;
    l.body_name={x,hy,static_cast<float>(body_name_chars*12-2)+16.f,header_h};
    x+=l.body_name.w+4.f;
    l.add(">",{x,hy,26.f,header_h},C::NextBody);

    const auto row=[&](const auto& items, int index) {
        float rx=t.x+pad;
        const float ry=t.y+pad+header_h+gap+index*(button_h+gap);
        for (const auto& item : items) {
            const float w=button_width(item.label);
            l.add(item.label,{rx,ry,w,button_h},item.command);
            rx+=w+gap;
        }
        return rx;
    };
    const float after=row(projection_row,0)+2*gap;
    const float ry=t.y+pad+header_h+gap;
    l.add("-",{after,ry,30.f,button_h},C::ZoomOut);
    l.add("+",{after+30.f+gap,ry,30.f,button_h},C::ZoomIn);
    if (body==BodyId::Earth) {
        row(earth_layer_row,1);
        row(earth_feature_row,2);
    } else {
        // Country and thematic layers have no meaning on an alien surface.
        row(body_layer_row,1);
        if (body_has_planetary_features(body)) row(landmark_row,2);
    }

    const Bounds c=l.panel.compass;
    l.rose={c.x+pad,c.y+pad,rose_area,c.h-2*pad};
    l.rose_x=c.x+pad+rose_area*.5f;
    l.rose_y=c.y+pad+50.f;
    const float bx=c.x+pad+rose_area+pad;
    const float column_h=4*compass_button_h+3*compass_gap;
    float by=c.y+(c.h-column_h)*.5f;
    for (const auto& [label,command] : std::array<RowItem,4>{{
             {"NORTH UP",C::NorthUp},{"LOCK NORTH",C::NorthLock},
             {"RESET",C::Reset},{"",C::ControlMode}}}) {
        l.add(label,{bx,by,compass_column,compass_button_h},command);
        by+=compass_button_h+compass_gap;
    }
    return l;
}

// Panels that hide map labels and markers beneath them.
struct Occluders {
    std::array<Bounds,4> b{};
    std::size_t n=0;
    explicit Occluders(const Ui& ui) {
        if (!ui.visible) return;
        const auto p=panels(ui);
        b[n++]=p.toolbar; b[n++]=p.compass; b[n++]=p.scale;
        if (ui.raster_legend_visible) b[n++]=p.legend;
    }
    bool hits(Bounds r) const {
        for (std::size_t i=0;i<n;i++) if (overlaps(r,b[i])) return true;
        return false;
    }
    bool hits(float x,float y) const {
        for (std::size_t i=0;i<n;i++) if (contains(b[i],x,y)) return true;
        return false;
    }
};

void rect(Ui& ui, float x, float y, float w, float h, Color c) {
    if (w<=0 || h<=0) return;
    auto emit = [&](float xx,float yy) {
        ui.triangles.push_back({xx,yy,c.r,c.g,c.b,c.a});
    };
    emit(x,y); emit(x+w,y); emit(x,y+h);
    emit(x+w,y); emit(x+w,y+h); emit(x,y+h);
}
void outline(Ui& ui, Bounds b, Color c) {
    rect(ui,b.x,b.y,b.w,1,c);
    rect(ui,b.x,b.y+b.h-1,b.w,1,c);
    rect(ui,b.x,b.y+1,1,b.h-2,c);
    rect(ui,b.x+b.w-1,b.y+1,1,b.h-2,c);
}
void draw_panel(Ui& ui, Bounds b) {
    rect(ui,b.x,b.y,b.w,b.h,panel);
    outline(ui,b,panel_edge);
}

void triangle(Ui& ui, float ax,float ay,float bx,float by,float cx,float cy,Color col) {
    ui.triangles.push_back({ax,ay,col.r,col.g,col.b,col.a});
    ui.triangles.push_back({bx,by,col.r,col.g,col.b,col.a});
    ui.triangles.push_back({cx,cy,col.r,col.g,col.b,col.a});
}
void stroke(Ui& ui,float ax,float ay,float bx,float by,float width,Color col) {
    const float dx=bx-ax,dy=by-ay;
    const float len=std::sqrt(dx*dx+dy*dy);
    if (len<.0001f) return;
    const float px=-dy*width/(2*len),py=dx*width/(2*len);
    triangle(ui,ax+px,ay+py,bx+px,by+py,ax-px,ay-py,col);
    triangle(ui,ax-px,ay-py,bx+px,by+py,bx-px,by-py,col);
}
void ring(Ui& ui,float x,float y,float radius,Color col) {
    for (int i=0;i<48;i++) {
        const float a=2*pi*i/48, b=2*pi*(i+1)/48;
        stroke(ui,x+radius*std::cos(a),y+radius*std::sin(a),
              x+radius*std::cos(b),y+radius*std::sin(b),1.25f,col);
    }
}
// Public-domain-style compact 5x7 bitmap alphabet. UI and English map labels
// are rendered with the same self-contained GLES2 code, without OS fonts.
// 7 rows, high bit is leftmost pixel of the 5-bit glyph.
struct Glyph { char ch; std::array<unsigned char,7> pixels; };
constexpr Glyph font[] = {
{'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
{'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
{'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
{'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
{'I',{14,4,4,4,4,4,14}}, {'J',{7,2,2,2,18,18,12}},
{'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
{'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
{'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
{'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
{'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
{'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
{'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
{'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
{'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
{'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
{'4',{2,6,10,18,31,2,2}}, {'5',{31,16,30,1,1,17,14}},
{'6',{7,8,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
{'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,2,28}},
{'-',{0,0,0,31,0,0,0}}, {'.',{0,0,0,0,0,12,12}},
{' ',{0,0,0,0,0,0,0}}, {'/',{1,1,2,4,8,16,16}},
{'&',{12,18,20,8,21,18,13}}, {'\'',{4,4,8,0,0,0,0}},
{'(',{2,4,8,8,8,4,2}}, {')',{8,4,2,2,2,4,8}},
{'?',{14,17,1,2,4,0,4}}, {':',{0,4,4,0,4,4,0}},
{'+',{0,4,4,31,4,4,0}}, {',',{0,0,0,0,4,4,8}},
{'_',{0,0,0,0,0,0,31}}, {'!',{4,4,4,4,4,0,4}},
{'<',{2,4,8,16,8,4,2}}, {'>',{8,4,2,1,2,4,8}},
{'\x7f',{12,18,18,12,0,0,0}} // degree sign, see normalize_ascii
};

unsigned char normalize_ascii(std::string_view input, std::size_t& i) {
    const auto c = static_cast<unsigned char>(input[i]);
    if (c < 128) return static_cast<unsigned char>(std::toupper(c));
    // UTF-8 diacritics common in English Natural Earth names. Never split a
    // multibyte codepoint into multiple '?' characters.
    if (c == 0xc2 && i+1 < input.size() &&
        static_cast<unsigned char>(input[i+1]) == 0xb0) {
        ++i;
        return 0x7f; // U+00B0 DEGREE SIGN uses the otherwise unused DEL slot
    }
    if ((c == 0xc3 || c == 0xc4 || c == 0xc5) && i+1 < input.size()) {
        const auto next = static_cast<unsigned char>(input[++i]);
        if (c == 0xc3) {
            if ((next>=0x80 && next<=0x85) || (next>=0xa0 && next<=0xa5)) return 'A';
            if (next==0x87 || next==0xa7) return 'C';
            if ((next>=0x88 && next<=0x8b) || (next>=0xa8 && next<=0xab)) return 'E';
            if ((next>=0x8c && next<=0x8f) || (next>=0xac && next<=0xaf)) return 'I';
            if (next==0x91 || next==0xb1) return 'N';
            if ((next>=0x92 && next<=0x96) || (next>=0xb2 && next<=0xb6) || next==0x98 || next==0xb8) return 'O';
            if ((next>=0x99 && next<=0x9c) || (next>=0xb9 && next<=0xbc)) return 'U';
            if (next==0xbd || next==0xbf) return 'Y';
        }
    } else if ((c & 0xe0) == 0xc0 && i+1<input.size()) ++i;
    else if ((c & 0xf0) == 0xe0 && i+2<input.size()) i+=2;
    else if ((c & 0xf8) == 0xf0 && i+3<input.size()) i+=3;
    return '?';
}

void draw_text(Ui& ui, float x,float y, std::string_view str, int scale, Color color) {
    // Country/city labels contain hundreds of characters per frame. Replace
    // the per-character linear scan over the bitmap font with a 128-slot table.
    static const auto glyphs = [] {
        std::array<const Glyph*,128> out{};
        for (const auto& glyph : font)
            out[static_cast<unsigned char>(glyph.ch)] = &glyph;
        return out;
    }();
    for (std::size_t i=0;i<str.size(); ++i) {
        const unsigned char ch = normalize_ascii(str,i);
        if (ch < glyphs.size() && glyphs[ch]) {
            const auto& glyph = *glyphs[ch];
            for (int row=0;row<7;row++) for (int col=0;col<5;col++) {
                if (glyph.pixels[row] & (1u << (4-col)))
                    rect(ui,x+col*scale,y+row*scale,scale,scale,color);
            }
        }
        x += 6*scale;
    }
}

// Centered single-line text inside a rectangle.
void draw_centered(Ui& ui, Bounds b, std::string_view label, int scale, Color color) {
    draw_text(ui,std::round(b.x+(b.w-text_width(label,scale))*.5f),
              std::round(b.y+(b.h-7.f*scale)*.5f),label,scale,color);
}

std::string_view button_label(const Button& b, const Camera& cam) {
    if (b.command==C::ControlMode)
        return cam.mode==ControlMode::Grab ? "GRAB" : "ARCBALL";
    return b.label;
}

bool command_on(UiCommand id, const Ui& ui, const MapView& m,
                const Globe& g, const Countries& c) {
    switch (id) {
    case C::Globe: case C::Plate: case C::Mercator: case C::Mollweide: case C::Hammer:
        return static_cast<int>(m.selected)==static_cast<int>(id);
    case C::Shading: return g.shading;
    case C::Countries: return c.fill_visible;
    case C::Borders: return c.borders_visible;
    case C::Names: return c.names_visible;
    case C::Texture: return g.texture_visible;
    case C::Grid: return ui.grid_visible;
    case C::NorthLock: return m.north_locked;
    case C::Capitals: return ui.capitals_visible;
    case C::Cities: return ui.cities_visible;
    case C::Rivers: return ui.rivers_visible;
    case C::Landmarks: return ui.planetary_features_visible;
    case C::Relief: return g.surface==SurfaceLayer::Relief && g.relief.valid();
    case C::Elevation: return g.surface==SurfaceLayer::Elevation && g.elevation.valid();
    case C::Population: return g.surface==SurfaceLayer::Population && g.population.valid();
    default: return false; // one-shot actions
    }
}

// Optional raster datasets may not be installed; their buttons stay
// clickable (and explain why on stderr) but are drawn dimmed.
bool command_available(UiCommand id, const Globe& g) {
    switch (id) {
    case C::Relief: return g.relief.valid();
    case C::Elevation: return g.elevation.valid();
    case C::Population: return g.population.valid();
    default: return true;
    }
}

void draw_button(Ui& ui, const Button& b, std::string_view label,
                 bool on, bool available) {
    Color fill=on ? active : inactive;
    if (ui.hover==static_cast<int>(b.command)) fill=lighten(fill,.14f);
    rect(ui,b.b.x,b.b.y,b.b.w,b.b.h,fill);
    if (on) rect(ui,b.b.x,b.b.y+b.b.h-2,b.b.w,2,accent);
    draw_centered(ui,b.b,label,b.font,available ? text : dimmed);
}

void draw_compass(Ui& ui, const Layout& l, const MapView& map, const Camera& cam) {
    const float x=l.rose_x, y=l.rose_y;
    const float bearing=static_cast<float>(map_north_heading(map,cam));
    if (ui.hover==static_cast<int>(C::NorthUp))
        ring(ui,x,y,36.f,accent);
    ring(ui,x,y,30.f,muted);
    // Tick marks stay fixed on screen; cardinal letters and needle turn with
    // true geographic north, not with the window or an arbitrary world axis.
    for (int i=0;i<12;i++) {
        const float a=2*pi*i/12;
        const float outer=i%3==0 ? 35.f : 33.f;
        stroke(ui,x+30*std::sin(a),y-30*std::cos(a),
              x+outer*std::sin(a),y-outer*std::cos(a),1.f,muted);
    }
    const float nx=std::sin(bearing), ny=-std::cos(bearing);
    const float px=-ny, py=nx;
    triangle(ui,x+nx*26,y+ny*26,
             x+px*5.f,y+py*5.f,x-px*5.f,y-py*5.f,needle_north);
    triangle(ui,x-nx*24,y-ny*24,
             x+px*5.f,y+py*5.f,x-px*5.f,y-py*5.f,needle_south);
    rect(ui,x-2,y-2,4,4,text);
    constexpr const char* letters="NESW";
    for (int i=0;i<4;i++) {
        const float a=bearing+i*pi*.5f;
        const char label[2]={letters[i],0};
        draw_text(ui,std::round(x+std::sin(a)*42-2.5f),
                  std::round(y-std::cos(a)*42-3.5f),label,1,i==0 ? needle_north : text);
    }
    int degrees=static_cast<int>(std::lround(bearing*180.f/pi));
    degrees=(degrees%360+360)%360;
    char heading[24];
    std::snprintf(heading,sizeof(heading),"HDG %03d\x7f",degrees);
    draw_centered(ui,{l.rose.x,l.rose.y+l.rose.h-10.f,l.rose.w,8.f},heading,1,muted);
}

void execute(UiCommand id, Ui& ui, Camera& cam, MapView& m, Globe& g, Countries& c,
             InputState& in) {
    switch (id) {
    case C::Globe: case C::Plate: case C::Mercator: case C::Mollweide: case C::Hammer:
        input_set_trackball(in,false);
        map_select(m,static_cast<MapProjection>(static_cast<int>(id)),cam);
        break;
    case C::ZoomOut: if (m.flat_goal>.5f) map_zoom(m,-1.f); else camera_apply_zoom(cam,-1.f); break;
    case C::ZoomIn: if (m.flat_goal>.5f) map_zoom(m,1.f); else camera_apply_zoom(cam,1.f); break;
    case C::Shading: g.shading = !g.shading; break;
    case C::Countries: c.fill_visible = !c.fill_visible; break;
    case C::Borders: c.borders_visible = !c.borders_visible; break;
    case C::Names: c.names_visible = !c.names_visible; break;
    case C::Texture: g.texture_visible = !g.texture_visible; break;
    case C::Grid: ui.grid_visible = !ui.grid_visible; break;
    case C::Capitals: ui.capitals_visible = !ui.capitals_visible; break;
    case C::Cities: ui.cities_visible = !ui.cities_visible; break;
    case C::Rivers: ui.rivers_visible = !ui.rivers_visible; break;
    case C::Landmarks: ui.planetary_features_visible = !ui.planetary_features_visible; break;
    case C::Relief: case C::Elevation: case C::Population: {
        const auto layer = id==C::Relief ? SurfaceLayer::Relief :
            id==C::Elevation ? SurfaceLayer::Elevation : SurfaceLayer::Population;
        if (command_available(id,g)) {
            g.surface = g.surface==layer ? SurfaceLayer::Imagery : layer;
            g.texture_visible=true;
        } else std::fprintf(stderr,"%s layer is not installed.\n",
            id==C::Relief ? "Relief" : id==C::Elevation ? "Elevation" : "Population");
        break;
    }
    case C::ControlMode:
        cam.mode = cam.mode == ControlMode::Grab ? ControlMode::Arcball : ControlMode::Grab;
        camera_grab_end(cam); camera_grab_kill_inertia(cam); break;
    case C::Reset: {
        input_set_trackball(in,false);
        camera_grab_end(cam);
        camera_grab_kill_inertia(cam);
        const auto home = g.body==BodyId::Earth ? timezone_center() :
            std::array<double,2>{0.,0.};
        camera_center_on(cam, home[0],home[1]);
        cam.distance=3.2f;
        m.center_lon = home[0]*pi/180.;
        m.center_lat = home[1]*pi/180.;
        m.roll=0.;
        map_reset_navigation(m);
        m.zoom=1.f;
        break;
    }
    case C::NorthUp:
        camera_grab_end(cam);
        map_align_north(m,cam);
        break;
    case C::NextBody: globe_select_body(g,next_body(g.body)); break;
    case C::PreviousBody: globe_select_body(g,previous_body(g.body)); break;
    case C::NorthLock:
        m.north_locked = !m.north_locked;
        camera_grab_kill_inertia(cam);
        map_enforce_north(m,cam);
        break;
    }
}

std::string upper(std::string_view s) {
    std::string out(s);
    for (auto& ch : out) ch=static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return out;
}
} // namespace

bool ui_command_center(const Ui& ui, BodyId body, UiCommand command,
                       float& x, float& y) {
    for (const auto& b : layout(ui,body)) {
        if (b.command!=command) continue;
        x=b.b.x+b.b.w*.5f;
        y=b.b.y+b.b.h*.5f;
        return true;
    }
    return false;
}

UiPanels ui_panels(const Ui& ui) {
    const auto p=panels(ui);
    const auto copy=[](Bounds b, float (&out)[4]) {
        out[0]=b.x; out[1]=b.y; out[2]=b.w; out[3]=b.h;
    };
    UiPanels out{};
    copy(p.toolbar,out.toolbar); copy(p.compass,out.compass);
    copy(p.scale,out.scale); copy(p.legend,out.legend);
    return out;
}

bool ui_init(Ui& ui) {
    ui.shader = load_shader("shaders/ui.vert","shaders/ui.frag");
    if (!ui.shader.program) return false;
    glGenBuffers(1,&ui.vbo);
    ui.triangles.reserve(4096); // reuse storage for the dynamic overlay mesh
    return ui.vbo!=0;
}
void ui_shutdown(Ui& ui) {
    if (ui.vbo) glDeleteBuffers(1,&ui.vbo);
    destroy_shader(ui.shader);
    ui = {};
}

bool ui_event(Ui& ui,const SDL_Event& e,Camera& cam,MapView& map,
              Globe& globe,Countries& countries,InputState& input) {
    if (e.type==SDL_EVENT_KEY_DOWN && e.key.key==SDLK_TAB) {
        if (!e.key.repeat) {
            ui.visible = !ui.visible;
            ui.hover = -1;
        }
        return true; // Tab no longer switches Grab / Arcball
    }
    if (e.type==SDL_EVENT_KEY_DOWN && e.key.key==SDLK_F &&
        globe.body!=BodyId::Earth) {
        if (!e.key.repeat) ui.planetary_features_visible=!ui.planetary_features_visible;
        return true; // Feature toggle also works while screen controls are hidden
    }
    if (!ui.visible) return false; // Hidden controls must not intercept map drags
    if (e.type!=SDL_EVENT_MOUSE_MOTION && e.type!=SDL_EVENT_MOUSE_BUTTON_DOWN &&
        e.type!=SDL_EVENT_MOUSE_BUTTON_UP && e.type!=SDL_EVENT_MOUSE_WHEEL) return false;
    // Captured drags continue even if the cursor passes over controls;
    // otherwise releasing above the toolbar leaves a stuck drag.
    if (input.dragging && (e.type==SDL_EVENT_MOUSE_MOTION ||
        (e.type==SDL_EVENT_MOUSE_BUTTON_UP && e.button.button==input.drag_button)))
        return false;
    float x=0,y=0;
    if (e.type==SDL_EVENT_MOUSE_MOTION) { x=e.motion.x; y=e.motion.y; }
    else if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN || e.type==SDL_EVENT_MOUSE_BUTTON_UP) {
        x=e.button.x; y=e.button.y;
    } else SDL_GetMouseState(&x,&y);

    const auto l=layout(ui,globe.body);
    const bool on_controls=contains(l.panel.toolbar,x,y) || contains(l.panel.compass,x,y);
    const bool on_passive=contains(l.panel.scale,x,y) ||
        (ui.raster_legend_visible && contains(l.panel.legend,x,y));
    ui.hover=-1;
    if (!on_controls && !on_passive) return false;
    if (e.type==SDL_EVENT_MOUSE_BUTTON_UP && e.button.button==SDL_BUTTON_LEFT) {
        input.dragging=false;
        input.drag_button=0;
        camera_grab_end(cam);
    }
    // All visible control surfaces consume input, including the compass
    // needle and the spaces between buttons, so no globe drag can begin.
    // Wheel zoom should still work with the pointer over the passive ruler.
    if (!on_controls) return e.type!=SDL_EVENT_MOUSE_WHEEL;
    int command=-1;
    for (const auto& b : l)
        if (contains(b.b,x,y)) { command=static_cast<int>(b.command); break; }
    // Clicking the compass rose itself is also a North-up shortcut.
    if (command<0 && contains(l.rose,x,y)) command=static_cast<int>(UiCommand::NorthUp);
    ui.hover=command;
    if (command>=0 && e.type==SDL_EVENT_MOUSE_BUTTON_DOWN &&
        e.button.button==SDL_BUTTON_LEFT) {
        input.dragging=false;
        input.drag_button=0;
        camera_grab_end(cam);
        execute(static_cast<UiCommand>(command),ui,cam,map,globe,countries,input);
    }
    return true;
}


// Derive the local scale from actual projected geographic displacements, not
// a nominal projection zoom. This works on the globe, maps, arbitrary map
// roll, and through projection morphs. The Earth radius converts radians to km.
namespace {
struct LocalScale {
    double pixels_east = 1., pixels_north = 1.; // pixels per radian
    double lon = 0., lat = 0.;                 // physical center, degrees
};
LocalScale local_scale(const Ui& ui, const Camera& cam, const MapView& map,
                       float aspect) {
    const bool flat=map.globe_to_flat>=.5f;
    const auto frame=map_frame(map);
    const Vec3 eye=camera_view_to_world(cam,{0,0,1});
    const auto ll_center=flat ? map_sheet_center(map) : std::nullopt;
    const std::array<double,3> c=ll_center ?
        geo::lonlat_to_xyz((*ll_center)[0]*geo::to_deg,(*ll_center)[1]*geo::to_deg,1.) :
        (flat ? frame.center : std::array<double,3>{eye.x,eye.y,eye.z});
    const Vec3 view_right=camera_view_to_world(cam,{1,0,0});
    const Vec3 view_up=camera_view_to_world(cam,{0,1,0});
    const auto realgeo=geo::xyz_to_lonlat(c[0],c[1],c[2]);
    const double lon=realgeo[0]*geo::to_rad, lat=realgeo[1]*geo::to_rad;
    const std::array<double,3> east=flat ?
        std::array<double,3>{-std::sin(lon),0.,-std::cos(lon)} :
        std::array<double,3>{view_right.x,view_right.y,view_right.z};
    const std::array<double,3> north=flat ?
        std::array<double,3>{-std::sin(lat)*std::cos(lon),std::cos(lat),std::sin(lat)*std::sin(lon)} :
        std::array<double,3>{view_up.x,view_up.y,view_up.z};
    const auto ll=geo::xyz_to_lonlat(c[0],c[1],c[2]);
    const auto screen=[&](const std::array<double,3>& v, float& x, float& y) {
        const auto geo=geo::xyz_to_lonlat(v[0],v[1],v[2]);
        return scene_project(cam,aspect,map,geo[0],geo[1],x,y);
    };
    constexpr double delta=.002;
    float cx=0,cy=0;
    const auto derivative=[&](const std::array<double,3>& tangent) {
        float x=0,y=0;
        const std::array<double,3> p{
            c[0]*std::cos(delta)+tangent[0]*std::sin(delta),
            c[1]*std::cos(delta)+tangent[1]*std::sin(delta),
            c[2]*std::cos(delta)+tangent[2]*std::sin(delta)};
        if (!screen(p,x,y)) return 0.;
        return std::hypot((x-cx)*ui.viewport_w*.5,
                          (y-cy)*ui.viewport_h*.5)/delta;
    };
    const double fallback=static_cast<double>(ui.viewport_h)*.5/
        std::tan(.35)/(flat ? map.map_distance/std::max(.1f,map.zoom) :
                                   std::max(.08f,cam.distance-1.f));
    if (!screen(c,cx,cy)) return {fallback,fallback,ll[0],ll[1]};
    const double px_e=derivative(east), px_n=derivative(north);
    return {px_e>1. ? px_e : fallback,
            px_n>1. ? px_n : fallback,ll[0],ll[1]};
}

void draw_scale_bar(Ui& ui) {
    const Bounds b=panels(ui).scale;
    draw_panel(ui,b);
    draw_text(ui,b.x+10,b.y+8,"SCALE",1,muted);
    if (ui.grid_visible) {
        // The adaptive graticule spacing belongs with the distance scale.
        char step[48];
        std::snprintf(step,sizeof(step),"GRID %.3g\x7f/%.3g\x7f",
                      ui.grid_lat_step,ui.grid_lon_step);
        draw_text(ui,b.x+b.w-10-text_width(step,1),b.y+8,step,1,muted);
    }
    if (ui.scale_bar_km <= 0. || ui.scale_bar_pixels <= 1.f) {
        draw_text(ui,b.x+10,b.y+22,"--",2,muted);
        return;
    }
    char units[48];
    if (ui.scale_bar_km < 1.)
        std::snprintf(units,sizeof(units),"%.0f M",ui.scale_bar_km*1000.);
    else if (ui.scale_bar_km < 10. && std::floor(ui.scale_bar_km)!=ui.scale_bar_km)
        std::snprintf(units,sizeof(units),"%.1f KM",ui.scale_bar_km);
    else
        std::snprintf(units,sizeof(units),"%.0f KM",ui.scale_bar_km);
    draw_text(ui,b.x+10,b.y+22,units,2,text);
    const float x=std::round(b.x+12), bar_y=b.y+b.h-12;
    const float width=std::round(std::clamp(ui.scale_bar_pixels,1.f,b.w-24.f));
    rect(ui,x-1,bar_y-7,width+2,8,{.01f,.02f,.04f,1.f});
    for (int i=0;i<4;i++)
        rect(ui,x+i*width/4,bar_y-6,width/4,6,(i%2==0) ? text : active);
    stroke(ui,x,bar_y-10,x,bar_y+2,1.5f,text);
    stroke(ui,x+width,bar_y-10,x+width,bar_y+2,1.5f,text);
}
} // namespace

void ui_draw_graticule(Ui& ui, const Camera& cam, const MapView& map, float aspect) {
    const auto scale=local_scale(ui,cam,map,aspect);
    const double km_for_150px=150.*ui.body_radius_km/scale.pixels_east;
    ui.scale_bar_km=grid::round_distance_km(km_for_150px);
    ui.scale_bar_pixels=static_cast<float>(ui.scale_bar_km /
        ui.body_radius_km*scale.pixels_east);
    // Use the nominal projection scale at the center of the globe or map.
    // Blend the tangent magnification and depth as the scene morphs.
    const double morph=map.globe_to_flat;
    const double depth=std::lerp(static_cast<double>(cam.distance)-1.,
                                 static_cast<double>(map.map_distance),morph);
    const double magnification=std::lerp(1.,static_cast<double>(map.zoom),morph);
    const double focal_pixels=ui.viewport_h*.5/std::tan(Camera::vertical_fov*.5);
    const double step=grid::angular_step(focal_pixels*magnification/depth*geo::to_rad);
    ui.grid_lon_step=step;
    ui.grid_lat_step=step;
    if (!ui.grid_visible) return;

    // Only sample the cap around the visible center. At close zoom this
    // avoids constructing hundreds of thousands of unseen world grid lines.
    const double span=std::min(geo::pi,
        1.5*std::hypot(ui.viewport_w,ui.viewport_h)/
        std::max(1.,std::min(scale.pixels_east,scale.pixels_north)));
    const double lat_span=span*geo::to_deg;
    const double coslat=std::abs(std::cos(scale.lat*geo::to_rad));
    const double lon_span=(std::abs(scale.lat)+lat_span>=90.) ? 180. :
        std::min(180.,lat_span/std::max(.1,coslat));
    const double min_lat=std::max(-90.,scale.lat-lat_span);
    const double max_lat=std::min(90.,scale.lat+lat_span);
    const double min_lon=lon_span>=180. ? -180. : scale.lon-lon_span;
    const double max_lon=lon_span>=180. ?  180. : scale.lon+lon_span;

    const Color color{.84f,.91f,1.f,.52f};
    // Re-project each sampled geographic segment through the same code as
    // the labels, including the rotating map frame, globe occlusion and seams.
    // Reject large pixel jumps at projection cuts instead of joining across
    // the opposite sides of the map.
    const auto line=[&](bool meridian, double fixed, double lo, double hi) {
        const double px_per_degree=std::max(scale.pixels_east,scale.pixels_north)*geo::to_rad;
        const double sample_deg=std::clamp(7./std::max(1.,px_per_degree),.02,2.);
        const int segments=std::clamp(static_cast<int>(std::ceil((hi-lo)/sample_deg)),1,4096);
        bool had=false;
        float lastx=0,lasty=0;
        for (int i=0;i<=segments;i++) {
            const double value=lo+(hi-lo)*i/segments;
            const double lon=meridian ? fixed : value;
            const double lat=meridian ? value : fixed;
            float nx=0,ny=0;
            const bool valid=scene_project(cam,aspect,map,lon,lat,nx,ny);
            const float x=(nx+1.f)*ui.viewport_w*.5f;
            const float y=(1.f-ny)*ui.viewport_h*.5f;
            const bool inside=valid && x>=-3 && x<=ui.viewport_w+3 &&
                y>=-3 && y<=ui.viewport_h+3;
            if (inside && had && std::hypot(x-lastx,y-lasty)<35.f)
                stroke(ui,lastx,lasty,x,y,1.15f,color);
            had=inside;
            lastx=x; lasty=y;
        }
    };
    const auto each_line=[&](double lo, double hi, double step, auto draw) {
        const long k0=static_cast<long>(std::ceil((lo-1e-8)/step));
        const long k1=static_cast<long>(std::floor((hi+1e-8)/step));
        for (long k=k0;k<=k1;k++) draw(k*step);
    };
    each_line(min_lon,max_lon,ui.grid_lon_step,[&](double lon) {
        // Normalize dateline-spanning caps back onto the physical globe.
        const double physical_lon=std::remainder(lon,360.);
        line(true,physical_lon,min_lat,max_lat);
    });
    each_line(min_lat,max_lat,ui.grid_lat_step,[&](double lat) {
        line(false,lat,min_lon,max_lon);
    });
}

namespace {
Color legend_color(const RasterLegend& legend, float value) {
    const auto& stops=legend.stops;
    if (stops.empty()) return muted;
    const RasterLegendStop* lo=&stops.front();
    const RasterLegendStop* hi=&stops.back();
    for (size_t i=1; i<stops.size();++i) {
        if (value<=stops[i].value) {hi=&stops[i]; lo=&stops[i-1]; break;}
    }
    const auto transform=[&](float v) {
        return legend.logarithmic ? std::log1p(std::max(0.f,v)) : v;
    };
    const float fraction = lo==hi ? 0.f : std::clamp(
        (transform(value)-transform(lo->value)) /
        (transform(hi->value)-transform(lo->value)),0.f,1.f);
    return {(lo->red+(hi->red-lo->red)*fraction)/255.f,
            (lo->green+(hi->green-lo->green)*fraction)/255.f,
            (lo->blue+(hi->blue-lo->blue)*fraction)/255.f,1.f};
}
void draw_raster_legend(Ui& ui, const RasterLegend& legend) {
    if (!legend.valid()) return;
    const auto box=panels(ui).legend;
    draw_panel(ui,box);
    draw_text(ui,box.x+8,box.y+8,legend.title,1,text);
    draw_text(ui,box.x+8,box.y+21,legend.units,1,muted);
    const int range_low=legend.stops.front().value;
    const int range_high=legend.stops.back().value;
    constexpr int bar_height=170;
    const auto transform=[&](float value) {
        return legend.logarithmic ? std::log1p(std::max(0.f,value)) : value;
    };
    const float low=transform(range_low), high=transform(range_high);
    const float bar_x=box.x+10.f,bar_y=box.y+38.f;
    for (int i=0;i<bar_height;i++) {
        const float t=high-(i+.5f)*(high-low)/bar_height;
        const float value=legend.logarithmic ? std::expm1(t) : t;
        rect(ui,bar_x,bar_y+i,18,1,legend_color(legend,value));
    }
    const auto tick=[&](int value) {
        if (value<range_low || value>range_high) return;
        const float y=bar_y+(high-transform(value))*bar_height/(high-low);
        stroke(ui,bar_x+20,y,bar_x+29,y,1.f,muted);
        draw_text(ui,bar_x+35,y-3,std::to_string(value),1,text);
    };
    if (legend.logarithmic) {
        for (int value : {0,1,10,100,1000,10000,20000}) tick(value);
    } else {
        for (int value : {8000,4000,0,-2000,-6000,-10000}) tick(value);
    }
    // Metadata travels with the asset and is supplied by the converter.
    // A single 90-character source name exceeds the compact 264px panel.
    // Wrap into two text rows without chopping off the dataset year.
    constexpr std::size_t max_chars = 40;
    if (legend.attribution.size() <= max_chars) {
        draw_text(ui,box.x+8,box.y+219,legend.attribution,1,muted);
    } else {
        std::size_t split = legend.attribution.rfind(' ',max_chars);
        if (split==std::string::npos || split<16) split=max_chars;
        draw_text(ui,box.x+8,box.y+214,
                  std::string_view(legend.attribution).substr(0,split),1,muted);
        const std::size_t next=split<legend.attribution.size() &&
            legend.attribution[split]==' ' ? split+1 : split;
        draw_text(ui,box.x+8,box.y+226,
                  std::string_view(legend.attribution).substr(next,max_chars),1,muted);
    }
    draw_text(ui,box.x+8,box.y+243,legend.logarithmic ?
              "NAVY: NO DATA" : "MAGENTA: NO DATA",1,muted);
}
} // namespace

void ui_draw(Ui& ui, const Camera& cam, const MapView& map, const Globe& globe,
             const Countries& countries) {
    if (!ui.visible) return;
    const auto l=layout(ui,globe.body);
    const Bounds toolbar=l.panel.toolbar;
    draw_panel(ui,toolbar);
    draw_panel(ui,l.panel.compass);
    // Header: title, body selector, body kind and the current view summary.
    const float hy=l.body_name.y, hh=l.body_name.h;
    draw_text(ui,toolbar.x+pad,std::round(hy+(hh-14.f)*.5f),"GLOBTOO",2,text);
    const auto& info=body_info(globe.body);
    rect(ui,l.body_name.x,l.body_name.y,l.body_name.w,l.body_name.h,{.01f,.02f,.04f,.85f});
    draw_centered(ui,l.body_name,upper(info.name),2,text);
    for (const auto& b : l)
        draw_button(ui,b,button_label(b,cam),
                    command_on(b.command,ui,map,globe,countries),
                    command_available(b.command,globe));
    std::string status=map.selected==MapProjection::Globe ? "SPHERE" : "FLAT MAP";
    if (globe.surface==SurfaceLayer::Relief && globe.relief.valid()) status+=" / RELIEF";
    if (globe.surface==SurfaceLayer::Elevation && globe.elevation.valid()) status+=" / ELEVATION";
    if (globe.surface==SurfaceLayer::Population && globe.population.valid()) status+=" / POPULATION";
    const float status_x=toolbar.x+toolbar.w-pad-text_width(status,1);
    const float text_y=std::round(hy+(hh-7.f)*.5f);
    draw_text(ui,status_x,text_y,status,1,muted);
    std::string kind=upper(body_kind_name(info.kind));
    if (info.parent!=info.id) kind+=" OF "+upper(body_info(info.parent).name);
    const float kind_x=l.body_name.x+l.body_name.w+4.f+26.f+12.f;
    if (kind_x+text_width(kind,1)+12.f<status_x)
        draw_text(ui,kind_x,text_y,kind,1,muted);

    draw_compass(ui,l,map,cam);
    draw_scale_bar(ui);
    if (ui.raster_legend_visible)
        draw_raster_legend(ui,globe.surface==SurfaceLayer::Population ?
                           globe.population.legend : globe.elevation.legend);
}

void ui_draw_country_names(Ui& ui, const Countries& countries, const Camera& cam,
                           const MapView& map, float aspect) {
    if (!countries.names_visible) return;
    const auto& gpu = countries.using_high ? countries.high : countries.low;
    if (gpu.labels.empty()) return;
    const Occluders blocked(ui);
    const bool dense = cam.distance<2.1f || (map.globe_to_flat>.5f && map.zoom>2.f);
    const int scale = dense ? 2 : 1;
    struct Candidate { float x,y,dist; const CountryGeometry::Label* label; };
    std::vector<Candidate> candidates;
    candidates.reserve(gpu.labels.size());
    for (const auto& l : gpu.labels) {
        float nx,ny;
        if (!scene_project(cam,aspect,map,l.longitude,l.latitude,nx,ny)) continue;
        const float x=(nx+1.f)*ui.viewport_w*.5f;
        const float y=(1.f-ny)*ui.viewport_h*.5f;
        if (x<10 || y<10 || x>ui.viewport_w-10 || y>ui.viewport_h-10 ||
            blocked.hits(x,y)) continue;
        candidates.push_back({x,y,nx*nx+ny*ny,&l});
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& a,const auto& b) {
        return a.dist<b.dist;
    });
    struct Bounds { float l,t,r,b; };
    std::vector<Bounds> occupied;
    for (const auto& candidate : candidates) {
        if (occupied.size()>=90) break;
        std::string_view name=candidate.label->name;
        if (name.size()>28) name=name.substr(0,28);
        const float tw=static_cast<float>(name.size()*6*scale);
        const float th=7.f*scale;
        const Bounds b{candidate.x-tw*.5f-3,candidate.y-th*.5f-2,
                       candidate.x+tw*.5f+3,candidate.y+th*.5f+2};
        if (b.l<1 || b.r>ui.viewport_w-1 || b.t<1 || b.b>ui.viewport_h-1 ||
            blocked.hits({b.l,b.t,b.r-b.l,b.b-b.t})) continue;
        bool clash=false;
        for (const auto& other: occupied) {
            if (b.l<other.r+3 && b.r>other.l-3 && b.t<other.b+3 && b.b>other.t-3) {
                clash=true; break;
            }
        }
        if (clash) continue;
        rect(ui,b.l,b.t,b.r-b.l,b.b-b.t,{.95f,.97f,.99f,.75f});
        draw_text(ui,b.l+3,b.t+2,name,scale,{.045f,.08f,.12f,1.f});
        occupied.push_back(b);
        ui.label_boxes.push_back({b.l,b.t,b.r-b.l,b.b-b.t});
    }
}

void ui_draw_places(Ui& ui, const GeographicFeatures& geography, const Camera& cam,
                    const MapView& map, float aspect) {
    if ((!ui.capitals_visible && !ui.cities_visible) || geography.places.empty()) return;
    const Occluders blocked(ui);
    const bool flat=map.flat_goal>.5f;
    const unsigned rank=geographic_detail_rank(cam.distance,map.zoom,flat);
    struct Candidate { float x,y; const GeographicPlace* place; };
    std::vector<Candidate> visible;
    visible.reserve(256);
    // Source order is capitals, then rank and population. Cull before doing
    // any UI geometry or allocating label rectangles.
    for (const auto& p:geography.places) {
        if (!geographic_city_visible(p,rank,ui.capitals_visible,ui.cities_visible)) continue;
        float nx,ny;
        if (!scene_project(cam,aspect,map,p.longitude,p.latitude,nx,ny)) continue;
        const float x=(nx+1.f)*ui.viewport_w*.5f;
        const float y=(1.f-ny)*ui.viewport_h*.5f;
        if (x<6 || y<6 || x>ui.viewport_w-6 || y>ui.viewport_h-6) continue;
        if (blocked.hits(x,y)) continue;
        visible.push_back({x,y,&p});
    }
    // Avoid engulfing the globe in pins at global scale. Prefer the
    // highest-ranked places; at close zoom the projection culls naturally.
    std::size_t pin_count=0, name_count=0;
    for (const auto& c:visible) {
        if (pin_count>=180 || name_count>=75) break;
        const bool capital=c.place->capital;
        const float radius=capital ? 4.5f : 2.7f;
        const Color ink=capital ? Color{1.f,.78f,.27f,1.f} : Color{.98f,.95f,.86f,1.f};
        // The outline makes a city visible over any raster or country color.
        if (capital) {
            triangle(ui,c.x,c.y-radius-2,c.x+radius+2,c.y,c.x,c.y+radius+2,
                     {.015f,.07f,.12f,.96f});
            triangle(ui,c.x,c.y-radius-2,c.x,c.y+radius+2,c.x-radius-2,c.y,
                     {.015f,.07f,.12f,.96f});
            triangle(ui,c.x,c.y-radius,c.x+radius,c.y,c.x,c.y+radius,ink);
            triangle(ui,c.x,c.y-radius,c.x,c.y+radius,c.x-radius,c.y,ink);
        } else {
            for (int j=0;j<12;j++) {
                const float a=2*pi*j/12,b=2*pi*(j+1)/12;
                triangle(ui,c.x,c.y,c.x+(radius+1)*std::cos(a),c.y+(radius+1)*std::sin(a),
                         c.x+(radius+1)*std::cos(b),c.y+(radius+1)*std::sin(b),
                         {.03f,.07f,.11f,.9f});
                triangle(ui,c.x,c.y,c.x+radius*std::cos(a),c.y+radius*std::sin(a),
                         c.x+radius*std::cos(b),c.y+radius*std::sin(b),ink);
            }
        }
        ++pin_count;
        const int font_scale = 1;
        std::string_view name(c.place->name);
        name=name.substr(0,30);
        const float w=static_cast<float>(name.size()*6*font_scale+8), h=15.f;
        float x=c.x+radius+6,y=c.y-h*.5f;
        if (x+w>ui.viewport_w-3) x=c.x-radius-6-w;
        if (x<3 || y<3 || x+w>=ui.viewport_w-3 || y+h>=ui.viewport_h-3) continue;
        const Bounds r{x,y,w,h};
        if (blocked.hits(r)) continue;
        bool collision=false;
        for (const auto& other:ui.label_boxes) {
            if (overlaps({r.x-3,r.y-3,r.w+6,r.h+6},
                         {other.x,other.y,other.w,other.h})) {
                collision=true; break;
            }
        }
        if (collision) continue;
        rect(ui,x,y,w,h,{.02f,.05f,.08f,.81f});
        rect(ui,x,y,capital ? 3.f : 2.f,h,ink);
        draw_text(ui,x+6,y+4,name,font_scale,text);
        ui.label_boxes.push_back({x,y,w,h});
        ++name_count;
    }
}

void ui_draw_markers(Ui& ui, const std::vector<Marker>& markers, const Camera& cam,
                     const MapView& map, float aspect) {
    // scene_project applies the same 3D occlusion, rotating geography,
    // projection morph and seam handling as country names and the grid.
    // Markers are fixed-size screen-space annotations, not Earth geometry.
    struct LabelBounds { float x,y,w,h; };
    const Occluders blocked(ui);
    std::vector<LabelBounds> occupied;
    for (const auto& marker : markers) {
        float ndc_x=0,ndc_y=0;
        if (!scene_project(cam,aspect,map,marker.longitude,marker.latitude,
                           ndc_x,ndc_y)) continue;
        const float x=(ndc_x+1.f)*ui.viewport_w*.5f;
        const float y=(1.f-ndc_y)*ui.viewport_h*.5f;
        const float r=marker.radius;
        if (x < -r || x > ui.viewport_w+r || y < -r || y > ui.viewport_h+r)
            continue;
        // A ring and contrasting center stay legible over both the terrain
        // texture and the solid country color layer.
        const Color marker_color{
            ((marker.color>>16)&255)/255.f,
            ((marker.color>>8)&255)/255.f,
            (marker.color&255)/255.f,1.f};
        ring(ui,x,y,r+2.f,{.015f,.035f,.06f,.85f});
        for (int i=0;i<24;i++) {
            const float a=2*pi*i/24.f, b=2*pi*(i+1)/24.f;
            triangle(ui,x,y,x+r*std::cos(a),y+r*std::sin(a),
                     x+r*std::cos(b),y+r*std::sin(b),marker_color);
        }
        ring(ui,x,y,r, text);
        rect(ui,x-1.25f,y-1.25f,2.5f,2.5f,{.035f,.055f,.085f,1.f});
        if (marker.label.empty()) continue;
        const int scale=2;
        // The compact built-in font supports ASCII plus basic transliteration.
        // Truncate long labels rather than covering the entire viewport.
        const auto label=std::string_view(marker.label).substr(0,48);
        const float w=label.size()*6.f*scale+12.f, h=7.f*scale+10.f;
        float lx=x+r+9.f;
        if (lx+w>ui.viewport_w-4.f) lx=x-r-9.f-w;
        const float ly=std::clamp(y-h*.5f,4.f,std::max(4.f,ui.viewport_h-h-4.f));
        const LabelBounds box{lx,ly,w,h};
        if (lx<4.f || lx+w>ui.viewport_w-4.f ||
            blocked.hits(Bounds{lx,ly,w,h})) continue;
        bool overlap=false;
        for (const auto& other : occupied)
            if (overlaps({lx-4,ly-4,w+8,h+8}, {other.x,other.y,other.w,other.h})) {
                overlap=true; break;
            }
        if (overlap) continue;
        rect(ui,lx,ly,w,h,panel);
        rect(ui,lx,ly,3.f,h,marker_color);
        draw_text(ui,lx+8.f,ly+5.f,label,scale,text);
        occupied.push_back(box);
    }
}

void ui_flush(Ui& ui) {
    if (!ui.shader.program || !ui.vbo || ui.triangles.empty()) return;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    use_shader(ui.shader);
    const GLint loc=ui.shader.u_viewport;
    if (loc>=0) glUniform2f(loc,static_cast<float>(ui.viewport_w),static_cast<float>(ui.viewport_h));
    glBindBuffer(GL_ARRAY_BUFFER,ui.vbo);
    glBufferData(GL_ARRAY_BUFFER,ui.triangles.size()*sizeof(UiVertex),ui.triangles.data(),GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(UiVertex),(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(UiVertex),(void*)(2*sizeof(float)));
    glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(ui.triangles.size()));
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    ui.triangles.clear();
}

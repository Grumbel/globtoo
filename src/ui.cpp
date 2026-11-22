#include "ui.hpp"
#include "scene.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <string>
#include <tuple>

namespace {
struct Color { float r,g,b,a; };
constexpr Color panel{.035f,.055f,.085f,.93f};
constexpr Color text{.92f,.96f,1.f,1.f};
constexpr Color muted{.65f,.72f,.80f,1.f};
constexpr Color active{.20f,.38f,.55f,1.f};
constexpr Color inactive{.11f,.18f,.26f,1.f};
constexpr Color hover{.22f,.29f,.38f,1.f};
constexpr Color needle_north{.98f,.38f,.28f,1.f};
constexpr Color needle_south{.43f,.67f,.86f,1.f};
constexpr float pi = 3.14159265358979323846f;

struct Bounds { float x,y,w,h; };
constexpr Bounds toolbar{8,8,600,100};
Bounds compass_panel(const Ui& ui) {
    const float x = std::max(8.f, static_cast<float>(ui.viewport_w) - 128.f);
    // At narrow window widths, do not cover the main toolbar.
    return {x, x < toolbar.x+toolbar.w+8 ? 116.f : 8.f, 120.f, 165.f};
}
bool contains(Bounds b, float x,float y) {
    return x>=b.x && x<b.x+b.w && y>=b.y && y<b.y+b.h;
}
bool overlaps(Bounds a, Bounds b) {
    return a.x<b.x+b.w && a.x+a.w>b.x &&
           a.y<b.y+b.h && a.y+a.h>b.y;
}


struct Button { const char* label; int x,y,w,h; int command; };
// All hit rectangles and draw positions derive from the same layout.
std::vector<Button> buttons() {
    std::vector<Button> out;
    const char* top[] = {"3D GLOBE", "PLATE", "MERCATOR", "MOLLWEIDE", "HAMMER", "-", "+"};
    const int topw[] = {103,72,108,121,88,30,30};
    const char* bottom[] = {"SHADING", "COUNTRIES", "BORDERS", "NAMES", "TEXTURE", "GRAB"};
    const int botw[] = {93,116,94,73,94,68};
    int x = 14;
    for (int i=0;i<7;i++) { out.push_back({top[i],x,34,topw[i],30,i}); x += topw[i]+5; }
    x = 14;
    for (int i=0;i<6;i++) { out.push_back({bottom[i],x,70,botw[i],30,i+7}); x += botw[i]+5; }
    return out;
}

void rect(Ui& ui, float x, float y, float w, float h, Color c) {
    if (w<=0 || h<=0) return;
    auto emit = [&](float xx,float yy) {
        ui.triangles.push_back({xx,yy,c.r,c.g,c.b,c.a});
    };
    emit(x,y); emit(x+w,y); emit(x,y+h);
    emit(x+w,y); emit(x+w,y+h); emit(x,y+h);
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
{'_',{0,0,0,0,0,0,31}}, {'!',{4,4,4,4,4,0,4}}
};

unsigned char normalize_ascii(std::string_view input, std::size_t& i) {
    const auto c = static_cast<unsigned char>(input[i]);
    if (c < 128) return static_cast<unsigned char>(std::toupper(c));
    // UTF-8 diacritics common in English Natural Earth names. Never split a
    // multibyte codepoint into multiple '?' characters.
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
    for (std::size_t i=0;i<str.size(); ++i) {
        unsigned char ch = normalize_ascii(str,i);
        for (const auto& glyph : font) {
            if (static_cast<unsigned char>(glyph.ch) != ch) continue;
            for (int row=0;row<7;row++) for (int col=0;col<5;col++) {
                if (glyph.pixels[row] & (1u << (4-col)))
                    rect(ui,x+col*scale,y+row*scale,scale,scale,color);
            }
            break;
        }
        x += 6*scale;
    }
}

void draw_compass(Ui& ui, const Camera& cam, const MapView& map) {
    const auto box=compass_panel(ui);
    rect(ui,box.x,box.y,box.w,box.h,panel);
    const float x=box.x+box.w*.5f, y=box.y+48.f;
    const float bearing=static_cast<float>(map_north_heading(map,cam));
    ring(ui,x,y,29.f,muted);
    // Tick marks stay fixed on screen; cardinal letters and needle turn with
    // true geographic north, not with the window or an arbitrary world axis.
    for (int i=0;i<12;i++) {
        const float a=2*pi*i/12;
        stroke(ui,x+29*std::sin(a),y-29*std::cos(a),
              x+(i%3==0 ? 34.f : 32.f)*std::sin(a),
              y-(i%3==0 ? 34.f : 32.f)*std::cos(a),1.f,muted);
    }
    const float nx=std::sin(bearing), ny=-std::cos(bearing);
    const float px=-ny, py=nx;
    triangle(ui,x+nx*25,y+ny*25,
             x+px*4.5f,y+py*4.5f,x-px*4.5f,y-py*4.5f,needle_north);
    triangle(ui,x-nx*23,y-ny*23,
             x+px*4.5f,y+py*4.5f,x-px*4.5f,y-py*4.5f,needle_south);
    rect(ui,x-2,y-2,4,4,text);
    constexpr const char* letters="NESW";
    for (int i=0;i<4;i++) {
        const float a=bearing+i*pi*.5f;
        const char label[2]={letters[i],0};
        draw_text(ui,x+std::sin(a)*40-2.5f,
                  y-std::cos(a)*40-3.5f,label,1,i==0 ? needle_north : text);
    }
    int degrees=static_cast<int>(std::lround(bearing*180.f/pi));
    degrees=(degrees%360+360)%360;
    char heading[24];
    std::snprintf(heading,sizeof(heading),"N %03d DEG",degrees);
    draw_text(ui,x-27.f,box.y+97.f,heading,1,muted);
    const Button north{"NORTH UP",static_cast<int>(box.x+7),
                       static_cast<int>(box.y+107),106,24,14};
    const Button reset{"RESET",static_cast<int>(box.x+7),
                       static_cast<int>(box.y+135),106,24,13};
    for (auto b : {north,reset}) {
        rect(ui,b.x,b.y,b.w,b.h,ui.hover==b.command ? hover : inactive);
        draw_text(ui,b.x+(b.w-12.f*std::char_traits<char>::length(b.label))*.5f,
                  b.y+6,b.label,2,text);
    }
}

bool command_active(int id, const Camera& cam, const MapView& m,
                    const Globe& g, const Countries& c) {
    if (id<=4) return static_cast<int>(m.selected)==id;
    switch (id) {
    case 7: return g.shading;
    case 8: return c.fill_visible;
    case 9: return c.borders_visible;
    case 10: return c.names_visible;
    case 11: return !c.fill_visible;
    case 12: return cam.mode == ControlMode::Grab;
    }
    return false;
}

void execute(int id, Camera& cam, MapView& m, Globe& g, Countries& c, InputState& in) {
    if (id<=4) {
        input_set_trackball(in,false);
        map_select(m,static_cast<MapProjection>(id),cam);
        return;
    }
    switch (id) {
    case 5: if (m.flat_goal>.5f) map_zoom(m,-1.f); else camera_apply_zoom(cam,-1.f); break;
    case 6: if (m.flat_goal>.5f) map_zoom(m,1.f); else camera_apply_zoom(cam,1.f); break;
    case 7: g.shading = !g.shading; break;
    case 8: c.fill_visible = !c.fill_visible; break;
    case 9: c.borders_visible = !c.borders_visible; break;
    case 10: c.names_visible = !c.names_visible; break;
    case 11: c.fill_visible = false; break;
    case 12:
        cam.mode = cam.mode == ControlMode::Grab ? ControlMode::Arcball : ControlMode::Grab;
        camera_grab_end(cam); camera_grab_kill_inertia(cam); break;
    case 13: {
        input_set_trackball(in,false);
        camera_grab_end(cam);
        camera_grab_kill_inertia(cam);
        const auto home = timezone_center();
        camera_center_on(cam, home[0],home[1]);
        cam.distance=3.2f;
        m.center_lon = home[0]*pi/180.;
        m.center_lat = home[1]*pi/180.;
        m.roll=0.;
        map_reset_navigation(m);
        m.zoom=1.f;
        break;
    }
    case 14:
        camera_grab_end(cam);
        map_align_north(m,cam);
        break;
    }
}

bool inside(const Button& button, float x,float y) {
    return x>=button.x && x<button.x+button.w && y>=button.y && y<button.y+button.h;
}
} // namespace

bool ui_init(Ui& ui) {
    ui.shader = load_shader("shaders/ui.vert","shaders/ui.frag");
    if (!ui.shader.program) return false;
    glGenBuffers(1,&ui.vbo);
    return ui.vbo!=0;
}
void ui_shutdown(Ui& ui) {
    if (ui.vbo) glDeleteBuffers(1,&ui.vbo);
    destroy_shader(ui.shader);
    ui = {};
}

bool ui_event(Ui& ui,const SDL_Event& e,Camera& cam,MapView& map,
              Globe& globe,Countries& countries,InputState& input) {
    if (e.type!=SDL_EVENT_MOUSE_MOTION && e.type!=SDL_EVENT_MOUSE_BUTTON_DOWN &&
        e.type!=SDL_EVENT_MOUSE_BUTTON_UP && e.type!=SDL_EVENT_MOUSE_WHEEL) return false;
    float x=0,y=0;
    if (e.type==SDL_EVENT_MOUSE_MOTION) { x=e.motion.x; y=e.motion.y; }
    else if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN || e.type==SDL_EVENT_MOUSE_BUTTON_UP) {
        x=e.button.x; y=e.button.y;
    } else SDL_GetMouseState(&x,&y);

    const auto cp=compass_panel(ui);
    const bool on_toolbar=contains(toolbar,x,y);
    const bool on_compass=contains(cp,x,y);
    ui.hover=-1;
    if (!on_toolbar && !on_compass) return false;
    if (e.type==SDL_EVENT_MOUSE_BUTTON_UP && e.button.button==SDL_BUTTON_LEFT) {
        input.dragging=false;
        camera_grab_end(cam);
    }
    // All visible control surfaces consume input, including the compass
    // needle and the spaces between buttons, so no globe drag can begin.
    if (on_compass) {
        const Bounds north{cp.x+7,cp.y+107,106,24};
        const Bounds reset{cp.x+7,cp.y+135,106,24};
        // Clicking the compass rose itself is also a North-up shortcut.
        const bool rose=contains({cp.x+13,cp.y+4,94,94},x,y);
        const int command=contains(reset,x,y) ? 13 :
                          (contains(north,x,y) || rose ? 14 : -1);
        ui.hover=command;
        if (command>=0 && e.type==SDL_EVENT_MOUSE_BUTTON_DOWN &&
            e.button.button==SDL_BUTTON_LEFT) {
            input.dragging=false;
            execute(command,cam,map,globe,countries,input);
        }
        return true;
    }
    for (const auto& b:buttons()) {
        if (!inside(b,x,y)) continue;
        ui.hover=b.command;
        if (e.type==SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button==SDL_BUTTON_LEFT) {
            input.dragging=false;
            camera_grab_end(cam);
            execute(b.command,cam,map,globe,countries,input);
        }
        break;
    }
    return true;
}

void ui_draw(Ui& ui, const Camera& cam, const MapView& map, const Globe& globe,
             const Countries& countries) {
    rect(ui,toolbar.x,toolbar.y,toolbar.w,toolbar.h,panel);
    draw_text(ui,14,15,"GLOBTOO   VIEW",2,text);
    for (const auto& b : buttons()) {
        Color color = command_active(b.command,cam,map,globe,countries) ? active : inactive;
        if (ui.hover==b.command) color=hover;
        rect(ui,b.x,b.y,b.w,b.h,color);
        const float namewidth = static_cast<float>(std::char_traits<char>::length(b.label)*6);
        const int scale=2;
        draw_text(ui,b.x+(b.w-namewidth*scale)*.5f,b.y+8,b.label,scale,text);
    }
    // Avoid overlapping the compass at narrow window widths.
    const bool compact = compass_panel(ui).x < 750.f;
    const float info_x = compact ? 14.f : 622.f;
    const float info_y = compact ? 116.f : 15.f;
    const char* mode = map.selected==MapProjection::Globe ? "SPHERE" : "FLAT MAP";
    draw_text(ui,info_x,info_y,mode,2,muted);
    if (map.flat_goal > .5f) {
        draw_text(ui,info_x,info_y+26.f,"SHIFT DRAG: ROTATE",1,muted);
        draw_text(ui,info_x,info_y+40.f,"DRAG THROUGH POLES",1,muted);
    }
    draw_compass(ui,cam,map);
}

void ui_draw_country_names(Ui& ui, const Countries& countries, const Camera& cam,
                           const MapView& map, float aspect) {
    if (!countries.names_visible) return;
    const auto& gpu = countries.using_high ? countries.high : countries.low;
    if (gpu.labels.empty()) return;
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
            contains(toolbar,x,y) || contains(compass_panel(ui),x,y)) continue;
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
            overlaps({b.l,b.t,b.r-b.l,b.b-b.t},toolbar) ||
            overlaps({b.l,b.t,b.r-b.l,b.b-b.t},compass_panel(ui))) continue;
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
    GLint loc=glGetUniformLocation(ui.shader.program,"u_viewport");
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

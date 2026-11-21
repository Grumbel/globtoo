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

struct Button { const char* label; int x,y,w,h; int command; };
// All hit rectangles and draw positions derive from the same layout.
std::vector<Button> buttons() {
    std::vector<Button> out;
    const char* top[] = {"3D GLOBE", "PLATE", "MERCATOR", "MOLLWEIDE", "HAMMER", "-", "+"};
    const int topw[] = {103,72,108,121,88,30,30};
    const char* bottom[] = {"SHADING", "COUNTRIES", "BORDERS", "NAMES", "TEXTURE", "GRAB", "RESET"};
    const int botw[] = {93,116,94,73,94,68,72};
    int x = 14;
    for (int i=0;i<7;i++) { out.push_back({top[i],x,34,topw[i],30,i}); x += topw[i]+5; }
    x = 14;
    for (int i=0;i<7;i++) { out.push_back({bottom[i],x,70,botw[i],30,i+7}); x += botw[i]+5; }
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
        camera_grab_kill_inertia(cam);
        const auto home = timezone_center();
        camera_center_on(cam, home[0],home[1]);
        cam.distance=3.2f;
        m.center_lon = home[0]*3.141592653589793/180.;
        m.center_lat = home[1]*3.141592653589793/180.;
        m.zoom=1.f;
        break;
    }
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
    } else {
        float mx=0,my=0;
        SDL_GetMouseState(&mx,&my);
        x=mx; y=my;
    }
    // The toolbar occupies two compact rows in the upper-left corner.
    if (x<10 || x>690 || y<9 || y>106) { ui.hover=-1; return false; }
    ui.hover=-1;
    if (e.type==SDL_EVENT_MOUSE_BUTTON_UP && e.button.button==SDL_BUTTON_LEFT) {
        input.dragging=false;
        camera_grab_end(cam);
    }
    for (const auto& b:buttons()) {
        if (inside(b,x,y)) {
            ui.hover=b.command;
            if (e.type==SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button==SDL_BUTTON_LEFT) {
                input.dragging=false;
                camera_grab_end(cam);
                execute(b.command,cam,map,globe,countries,input);
            }
            break;
        }
    }
    return true;
}

void ui_draw(Ui& ui, const Camera& cam, const MapView& map, const Globe& globe,
             const Countries& countries) {
    rect(ui,8,8,682,100,panel);
    draw_text(ui,14,15,"GLOBTOO   VIEW",2,text);
    for (const auto& b : buttons()) {
        Color color = command_active(b.command,cam,map,globe,countries) ? active : inactive;
        if (ui.hover==b.command) color=hover;
        rect(ui,b.x,b.y,b.w,b.h,color);
        const float namewidth = static_cast<float>(std::char_traits<char>::length(b.label)*6);
        const int scale=2;
        draw_text(ui,b.x+(b.w-namewidth*scale)*.5f,b.y+8,b.label,scale,text);
    }
    // Explicitly state current mode and the interpretation of timezone start.
    const char* mode = map.selected==MapProjection::Globe ? "SPHERE" : "FLAT MAP";
    draw_text(ui,704,15,mode,2,muted);
    if (map.flat_goal > .5f) {
        draw_text(ui,704,41,"SHIFT DRAG: ROTATE",1,muted);
        draw_text(ui,704,55,"DRAG THROUGH POLES",1,muted);
    }
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
            (x<700 && y<110)) continue;
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
            (b.l<700 && b.t<110)) continue;
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

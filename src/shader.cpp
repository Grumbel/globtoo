// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shader.hpp"
#include "resources.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>

static std::string read_file(const char* path) {
    const std::string location = resource_path(path);
    FILE* f = fopen(location.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", location.c_str());
        return {};
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string data(size, '\0');
    if (fread(data.data(), 1, size, f) != (size_t)size) {
        fclose(f);
        return {};
    }
    fclose(f);
    return data;
}

static bool expand_includes(std::string& source, const std::string& shader_path) {
    const std::string marker = "#include \"projection.glsl\"";
    auto at = source.find(marker);
    if (at == std::string::npos) return true;
    auto slash = shader_path.find_last_of("/\\");
    std::string inc = (slash == std::string::npos ? "" : shader_path.substr(0,slash+1)) + "projection.glsl";
    const std::string contents = read_file(inc.c_str());
    if (contents.empty()) return false;
    source.replace(at, marker.size(), contents);
    return true;
}

static GLuint compile(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        fprintf(stderr, "Shader compile error:\n%s\n", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

Shader load_shader(const char* vert_path, const char* frag_path) {
    Shader s{};
    std::string vsrc = read_file(vert_path);
    std::string fsrc = read_file(frag_path);
    if (vsrc.empty() || fsrc.empty() || !expand_includes(vsrc, vert_path)) return s;

    GLuint vs = compile(GL_VERTEX_SHADER, vsrc.c_str());
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc.c_str());
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return s;
    }

    s.program = glCreateProgram();
    glAttachShader(s.program, vs);
    glAttachShader(s.program, fs);
    glBindAttribLocation(s.program, 0, "a_pos");
    glBindAttribLocation(s.program, 1, "a_uv");
    glBindAttribLocation(s.program, 1, "a_bright"); // stars; same slot
    glBindAttribLocation(s.program, 1, "a_color");  // country fill
    glBindAttribLocation(s.program, 3, "a_anchor");
    glBindAttribLocation(s.program, 2, "a_other");  // river ribbons
    glBindAttribLocation(s.program, 4, "a_corner");
    glLinkProgram(s.program);

    GLint ok = 0;
    glGetProgramiv(s.program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(s.program, sizeof(log), nullptr, log);
        fprintf(stderr, "Shader link error (%s + %s):\n%s\n",
                vert_path, frag_path, log);
        glDeleteProgram(s.program);
        s.program = 0;
    } else {
        s.u_mvp = glGetUniformLocation(s.program, "u_mvp");
        s.u_model = glGetUniformLocation(s.program, "u_model");
        s.u_tex = glGetUniformLocation(s.program, "u_tex");
        s.u_shading = glGetUniformLocation(s.program, "u_shading");
        s.u_light_dir = glGetUniformLocation(s.program, "u_light_dir");
        s.u_textured = glGetUniformLocation(s.program, "u_textured");
        s.u_fill_opacity = glGetUniformLocation(s.program, "u_fill_opacity");
        s.u_color = glGetUniformLocation(s.program, "u_color");
        s.u_viewport = glGetUniformLocation(s.program, "u_viewport");
        auto& u=s.scene;
        u.view=glGetUniformLocation(s.program,"u_view");
        u.projection=glGetUniformLocation(s.program,"u_projection");
        u.morph=glGetUniformLocation(s.program,"u_morph");
        u.projection_from=glGetUniformLocation(s.program,"u_projection_from");
        u.projection_to=glGetUniformLocation(s.program,"u_projection_to");
        u.projection_blend=glGetUniformLocation(s.program,"u_projection_blend");
        u.map_east=glGetUniformLocation(s.program,"u_map_east");
        u.map_north=glGetUniformLocation(s.program,"u_map_north");
        u.map_center=glGetUniformLocation(s.program,"u_map_center");
        u.map_zoom=glGetUniformLocation(s.program,"u_map_zoom");
        u.map_distance=glGetUniformLocation(s.program,"u_map_distance");
        u.wrap=glGetUniformLocation(s.program,"u_wrap");
        u.bounded_projection=glGetUniformLocation(s.program,"u_bounded_projection");
        u.map_roll=glGetUniformLocation(s.program,"u_map_roll");
        u.map_pan=glGetUniformLocation(s.program,"u_map_pan");
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return s;
}

void destroy_shader(Shader& s) {
    if (s.program) glDeleteProgram(s.program);
    s = {};
}

void use_shader(const Shader& s) {
    glUseProgram(s.program);
}

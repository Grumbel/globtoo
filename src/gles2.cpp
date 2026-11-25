// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gles2.hpp"

#include <SDL3/SDL.h>

#define GLOBTOO_GL(type, name) type name = nullptr;
#include "gles2_functions.inc"
#undef GLOBTOO_GL

bool load_gles2() {
#define GLOBTOO_GL(type, name) \
    name = reinterpret_cast<type>(SDL_GL_GetProcAddress(#name)); \
    if (!name) { SDL_Log("Missing GLES2 function %s", #name); return false; }
#include "gles2_functions.inc"
#undef GLOBTOO_GL
    return true;
}

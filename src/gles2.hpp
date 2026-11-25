// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifdef GLOBTOO_LOAD_GLES2
#define SDL_USE_BUILTIN_OPENGL_DEFINITIONS
#define GL_GLES_PROTOTYPES 0
#endif
#include <SDL3/SDL_opengles2.h>

#ifdef GLOBTOO_LOAD_GLES2
#define GLOBTOO_GL(type, name) extern type name;
#include "gles2_functions.inc"
#undef GLOBTOO_GL

bool load_gles2();
#endif

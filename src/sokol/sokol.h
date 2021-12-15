// #ifdef __APPLE__
// #include <OpenGL/gl3.h>
// #else
// #include <GL/glew.h>
// #endif

/* Code is used as an importable module, so apps must provide their own main */
#define SOKOL_NO_ENTRY

/* Select graphics API implementation */
#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE33
#endif

#ifdef NDEBUG
#ifndef SOKOL_DEBUG
#define SOKOL_DEBUG
#endif
#endif

#ifndef __EMSCRIPTEN__
#define SOKOL_SHADER_HEADER SOKOL_SHADER_VERSION SOKOL_SHADER_PRECISION
#define SOKOL_SHADER_VERSION "#version 330\n"
#define SOKOL_SHADER_PRECISION "precision mediump float;\n"
#else
#define SOKOL_SHADER_HEADER SOKOL_SHADER_VERSION SOKOL_SHADER_PRECISION
#define SOKOL_SHADER_VERSION  "#version 300 es\n"
#define SOKOL_SHADER_PRECISION "precision mediump float;\n"
#endif

#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"

// Single implementation unit for sokol and stb_image.
#define SOKOL_NO_ENTRY
#define SOKOL_APP_IMPL
#define SOKOL_GFX_IMPL
#define SOKOL_COLOR_IMPL
#define SOKOL_GLUE_IMPL
#define SOKOL_LOG_IMPL
#define SOKOL_GLCORE

#include <sokol/sokol_app.h>
#include <sokol/sokol_gfx.h>
#include <sokol/sokol_color.h>
#include <sokol/sokol_glue.h>
#include <sokol/sokol_log.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

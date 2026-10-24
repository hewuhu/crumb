module;
#define SOKOL_NO_ENTRY
#define SOKOL_GLCORE
#include <sokol/sokol_app.h>
#include <sokol/sokol_log.h>
#include <stb/stb_image.h>
export module crumb.window;
import crumb.type;
import crumb.logger;
import crumb.assets;
import crumb.input;

export namespace crumb::win {

// Window title and initial client size.
struct WindowConfig {
  string title = "hello crumb";
  i32 width = 640;
  i32 height = 360;

  void SetTitle(string_view value) {
    title.assign(value.data(), value.size());
  }

  void SetSize(i32 w, i32 h) {
    width = w;
    height = h;
  }
};

// Callbacks run by sokol-app; userData is passed through unchanged.
struct WindowHooks {
  void* userData = nullptr;
  void (*init)(void* userData) = nullptr;
  void (*frame)(void* userData, double dt) = nullptr;
  void (*cleanup)(void* userData) = nullptr;
};

// Create the window and block until it closes.
void Run(const WindowConfig& cfg, const WindowHooks& hooks);
// Current framebuffer size in pixels.
i32  Width();
i32  Height();
void SetTitle(string_view title);
void Quit();

} // namespace crumb::win

struct Context {
  crumb::win::WindowHooks hooks;
};

void OnInit(void* user_data) {
  auto* ctx = static_cast<Context*>(user_data);
  crumb::input::Reset();
  if (ctx->hooks.init) ctx->hooks.init(ctx->hooks.userData);
}

void OnFrame(void* user_data) {
  auto* ctx = static_cast<Context*>(user_data);
  if (ctx->hooks.frame) ctx->hooks.frame(ctx->hooks.userData, sapp_frame_duration());
  crumb::input::EndFrame();
}

void OnCleanup(void* user_data) {
  auto* ctx = static_cast<Context*>(user_data);
  if (ctx->hooks.cleanup) ctx->hooks.cleanup(ctx->hooks.userData);
}

void OnEvent(const sapp_event* e, void*) {
  crumb::input::HandleEvent(e);
}

namespace crumb::win {

void Run(const WindowConfig& cfg, const WindowHooks& hooks) {
  Context context { hooks };

  sapp_desc native {};
  native.init_userdata_cb    = &OnInit;
  native.frame_userdata_cb   = &OnFrame;
  native.cleanup_userdata_cb = &OnCleanup;
  native.event_userdata_cb   = &OnEvent;
  native.user_data           = &context;
  native.width               = cfg.width;
  native.height              = cfg.height;
  native.window_title        = cfg.title.c_str();
  native.high_dpi            = true;
  native.depth_format        = SAPP_PIXELFORMAT_NONE;
  native.logger.func         = slog_func;

  // Decode the embedded icon once and hand the RGBA pixels to sokol-app.
  // The pixels must stay alive until sapp_run() has created the window icons.
  const assets::Asset iconPng = assets::IconPng();
  i32 iconWidth    = 0;
  i32 iconHeight   = 0;
  i32 iconChannels = 0;
  uchar* iconPixels = nullptr;

  if (!iconPng.Empty()) {
    iconPixels = stbi_load_from_memory(
      iconPng.data, to_i32(iconPng.size),
      &iconWidth, &iconHeight, &iconChannels, 4);
  }

  if (iconPixels && iconWidth > 0 && iconHeight > 0) {
    native.icon.images[0].width  = iconWidth;
    native.icon.images[0].height = iconHeight;
    native.icon.images[0].pixels.ptr  = iconPixels;
    native.icon.images[0].pixels.size =
      to_size_t(iconWidth) * to_size_t(iconHeight) * 4;
  } else {
    Logger::Warn("window: failed to decode embedded crumb-ico.png");
  }

  sapp_run(&native);

  if (iconPixels)
    stbi_image_free(iconPixels);
}

i32  Width()  { return static_cast<i32>(sapp_width());  }
i32  Height() { return static_cast<i32>(sapp_height()); }

void SetTitle(string_view title) {
  string copy(title);
  sapp_set_window_title(copy.c_str());
}

void Quit() { sapp_quit(); }

} // namespace crumb::win

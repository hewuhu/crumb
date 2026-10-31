module;
#include <string>
#include <string_view>
#include <vector>

#include <sokol/sokol_app.h>
#include <sokol/sokol_log.h>
#include <stb/stb_image.h>
export module crumb.platform.window;
import crumb.core.types;
import crumb.core.log;
import crumb.core.assets;
import crumb.platform.input;

export namespace crumb::win {

// Window title and initial client size, in pixels.
struct WindowConfig {
  string title = "";
  i32 width = 0;
  i32 height = 0;

  void SetTitle(string_view value) {
    title.assign(value.data(), value.size());
  }

  void SetSize(i32 new_width, i32 new_height) {
    width = new_width;
    height = new_height;
  }
};

// Game callbacks run by sokol-app; userData is the Runtime instance.
struct WindowHooks {
  void* userData = nullptr;
  void (*init)(void* user_data) = nullptr;
  void (*frame)(void* user_data, double raw_dt) = nullptr;
  void (*cleanup)(void* user_data) = nullptr;
};

// Everything the window owns. Runtime keeps exactly one of these.
struct WindowContext {
  input::InputState input;
  WindowHooks hooks;
};

// Editor path: build the description, then run sokol-app blocking.
void Run(WindowContext& context, const WindowConfig& cfg);

// Template path: sokol-app reads the description from sokol_main(). The context
// and config must stay alive until the window has been created.
sapp_desc MakeDesc(WindowContext& context, const WindowConfig& cfg);

i32  Width();
i32  Height();
void SetTitle(string_view title);
void Quit();

}

namespace {

// Editor debug builds keep a console; templates and release builds do not.
#if defined(CRUMB_TEMPLATE)
constexpr bool SHOW_CONSOLE = false;
#elif defined(NDEBUG)
constexpr bool SHOW_CONSOLE = false;
#else
constexpr bool SHOW_CONSOLE = true;
#endif

// sokol-app reads the icon while creating the window, after MakeDesc returns.
// Keeping the decoded pixels here avoids pointing at a dead stack buffer.
std::vector<crumb::ui8> g_icon_pixels;

// Reset input before user code can observe it.
void OnInit(void* user_data) {
  auto* context = static_cast<crumb::win::WindowContext*>(user_data);
  crumb::input::Reset(context->input);
  if (context->hooks.init)
    context->hooks.init(context->hooks.userData);
}

// sokol's userdata frame callback has no dt parameter; query it instead.
void OnFrame(void* user_data) {
  auto* context = static_cast<crumb::win::WindowContext*>(user_data);
  if (context->hooks.frame)
    context->hooks.frame(context->hooks.userData, sapp_frame_duration());
  crumb::input::EndFrame(context->input);
}

void OnCleanup(void* user_data) {
  auto* context = static_cast<crumb::win::WindowContext*>(user_data);
  if (context->hooks.cleanup)
    context->hooks.cleanup(context->hooks.userData);
}

void OnEvent(const sapp_event* event, void* user_data) {
  auto* context = static_cast<crumb::win::WindowContext*>(user_data);
  crumb::input::HandleEvent(context->input, event);
}

}

namespace crumb::win {

// Build the sokol description and keep the icon pixels alive for window creation.
sapp_desc MakeDesc(WindowContext& context, const WindowConfig& cfg) {
  sapp_desc native {};
  native.init_userdata_cb     = &OnInit;
  native.frame_userdata_cb    = &OnFrame;
  native.cleanup_userdata_cb  = &OnCleanup;
  native.event_userdata_cb    = &OnEvent;
  native.user_data            = &context;
  native.width                = cfg.width;
  native.height               = cfg.height;
  native.window_title         = cfg.title.c_str();
  native.high_dpi             = true;
  native.depth_format         = SAPP_PIXELFORMAT_NONE;
  native.logger.func          = slog_func;
  native.win32.console_create = SHOW_CONSOLE;

  const Asset icon_png = IconPng();
  i32 icon_width    = 0;
  i32 icon_height   = 0;
  i32 icon_channels = 0;

  g_icon_pixels.clear();
  if (!icon_png.Empty()) {
    ui8* pixels = stbi_load_from_memory(
      icon_png.data, to_i32(icon_png.size),
      &icon_width, &icon_height, &icon_channels, 4);
    if (pixels != nullptr) {
      const usize size = to_usize(icon_width) * to_usize(icon_height) * 4;
      g_icon_pixels.assign(pixels, pixels + size);
      stbi_image_free(pixels);
    }
  }

  if (!g_icon_pixels.empty() && icon_width > 0 && icon_height > 0) {
    native.icon.images[0].width  = icon_width;
    native.icon.images[0].height = icon_height;
    native.icon.images[0].pixels.ptr  = g_icon_pixels.data();
    native.icon.images[0].pixels.size = g_icon_pixels.size();
  } else {
    Logger::Warn("window: failed to decode embedded crumb-ico.png");
  }

  return native;
}

// Blocking editor loop; returns when the window closes.
void Run(WindowContext& context, const WindowConfig& cfg) {
  const sapp_desc native = MakeDesc(context, cfg);
  sapp_run(&native);
}

// Current framebuffer size in pixels.
i32 Width()  { return to_i32(sapp_width()); }
i32 Height() { return to_i32(sapp_height()); }

void SetTitle(string_view title) {
  const string copy(title);
  sapp_set_window_title(copy.c_str());
}

void Quit() { sapp_quit(); }

}

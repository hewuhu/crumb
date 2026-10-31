module;
#include <charconv>
#include <cctype>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module crumb.app.runtime;

import crumb.core.types;
import crumb.core.error;
import crumb.core.log;
import crumb.core.time;
import crumb.gfx.renderer;
import crumb.platform.dialog;
import crumb.platform.input;
import crumb.platform.vfs;
import crumb.platform.window;
import crumb.script.lua;
import crumb.script.bindings;

namespace crumb {

using std::vector;

namespace {

// Trim ASCII whitespace from both ends.
string_view Trim(string_view text) {
  while (!text.empty() &&
         std::isspace(to_ui8(text.front()))) {
    text.remove_prefix(1);
  }
  while (!text.empty() &&
         std::isspace(to_ui8(text.back()))) {
    text.remove_suffix(1);
  }
  return text;
}

// Windows editors may prepend a UTF-8 BOM, which Lua rejects.
void StripBom(string& text) {
  if (text.size() >= 3 &&
      to_ui8(text[0]) == 0xEF &&
      to_ui8(text[1]) == 0xBB &&
      to_ui8(text[2]) == 0xBF) {
    text.erase(0, 3);
  }
}

// Parse a decimal integer in [1, max_value]; return fallback otherwise.
i32 ParsePositive(string_view text, i32 fallback, i32 max_value) {
  i32 value = 0;
  c_string begin = text.data();
  c_string end = text.data() + text.size();
  const auto result = std::from_chars(begin, end, value);
  if (result.ec != std::errc {} || result.ptr != end ||
      value <= 0 || value > max_value) {
    return fallback;
  }
  return value;
}

// Read simple key = value lines. '#' starts a comment; unknown keys are ignored.
VoidResult ParseConfig(string_view text, ProjectConfig& config) {
  usize cursor = 0;
  while (cursor < text.size()) {
    usize end = text.find('\n', cursor);
    if (end == string_view::npos)
      end = text.size();

    string_view line = text.substr(cursor, end - cursor);
    if (!line.empty() && line.back() == '\r')
      line.remove_suffix(1);
    cursor = end + 1;

    const auto comment = line.find('#');
    if (comment != string_view::npos)
      line = line.substr(0, comment);

    const auto equal = line.find('=');
    if (equal == string_view::npos)
      continue;

    const string_view key = Trim(line.substr(0, equal));
    const string_view value = Trim(line.substr(equal + 1));
    if (key.empty() || value.empty())
      continue;

    if (key == "name") {
      config.title = string(value);
    } else if (key == "width") {
      config.width = ParsePositive(value, config.width, 4096);
    } else if (key == "height") {
      config.height = ParsePositive(value, config.height, 4096);
    } else if (key == "sprite_size") {
      config.spriteSize = ParsePositive(value, config.spriteSize, 256);
    } else if (key == "window_scale") {
      config.windowScale = ParsePositive(value, config.windowScale, 8);
    } else if (key == "fixed_fps") {
      config.fixedFps = ParsePositive(value, config.fixedFps, 1000);
    } else if (key == "max_fixed_steps") {
      config.maxFixedSteps = ParsePositive(value, config.maxFixedSteps, 100);
    }
  }

  return {};
}

}

// Probe the path, copy the mount source into the config, and verify main.lua.
Result<ProjectConfig> ResolveProject(string_view path) {
  namespace fs = std::filesystem;

  ProjectConfig config;
  std::error_code ec;
  fs::path target = path.empty()
    ? fs::current_path(ec)
    : fs::path(string(path));
  if (ec) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_IO, "cannot resolve project path: " + ec.message()) };
  }

  const fs::path absolute_target = fs::absolute(target, ec);
  if (!ec)
    target = absolute_target;

  vfs::Vfs probe;
  string mount_error;
  string main_name = "main.lua";
  string title;

  if (fs::is_directory(target, ec)) {
    if (!probe.MountDisk(target.string(), &mount_error)) {
      return std::unexpected<Error> {
        MakeError(ErrorCode::ERROR_NOT_FOUND, mount_error) };
    }
    title = target.filename().string();
  } else if (fs::is_regular_file(target, ec)) {
    const string extension = target.extension().string();
    if (extension == ".lua") {
      fs::path parent = target.parent_path();
      if (parent.empty())
        parent = fs::current_path(ec);
      if (!probe.MountDisk(parent.string(), &mount_error)) {
        return std::unexpected<Error> {
          MakeError(ErrorCode::ERROR_NOT_FOUND, mount_error) };
      }
      main_name = target.filename().string();
      title = parent.filename().string();
    } else if (extension == ".pak" || extension == ".exe") {
      if (!probe.MountAuto(target.string(), &mount_error)) {
        return std::unexpected<Error> {
          MakeError(ErrorCode::ERROR_NOT_FOUND, mount_error) };
      }
      title = target.stem().string();
    } else {
      return std::unexpected<Error> {
        MakeError(ErrorCode::ERROR_INVALID_ARGUMENT,
                  "not a project directory, .lua, .pak, or .exe: " + target.string()) };
    }
  } else {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_NOT_FOUND, "project not found: " + target.string()) };
  }

  config.sourceKind = probe.Kind();
  config.sourcePath = probe.SourcePath();
  config.mainScript = main_name;
  config.spritesPath = "sprites.png";

  if (title.empty() || title == "." || title == "..")
    title = "crumb";
  config.title = title;

  if (!probe.Exists(main_name)) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_NOT_FOUND,
                "main.lua not found in " + config.sourcePath) };
  }

  const Result<string> cfg = probe.ReadText("crumb.cfg");
  if (cfg) {
    const VoidResult parsed = ParseConfig(*cfg, config);
    if (!parsed)
      return std::unexpected<Error> { parsed.error() };
  } else if (cfg.error().code != ErrorCode::ERROR_NOT_FOUND) {
    return std::unexpected<Error> { cfg.error() };
  }

  return config;
}

// Invalid fixed-step values fall back to 60 Hz with a 5-step catch-up cap.
Runtime::Runtime(ProjectConfig config)
  : m_config(std::move(config)),
    m_time(1.0 / to_double(
             m_config.fixedFps > 0 ? m_config.fixedFps : 60),
           0.25,
           m_config.maxFixedSteps > 0 ? m_config.maxFixedSteps : 5) {
}

// Blocking editor path; returns a process exit code.
int Runtime::RunBlocking() {
  if (m_config.sourcePath.empty()) {
    Logger::Error("runtime: no project source");
    return 1;
  }

  Prepare();
  const win::WindowConfig window = MakeWindowConfig();
  win::Run(m_window, window);
  return m_failed ? 1 : 0;
}

// The window uses the logical resolution multiplied by windowScale.
win::WindowConfig Runtime::MakeWindowConfig() const {
  win::WindowConfig window;
  window.SetTitle(m_config.title);

  const i32 scale = m_config.windowScale > 0 ? m_config.windowScale : 1;
  window.SetSize(m_config.width * scale, m_config.height * scale);
  return window;
}

// Route sokol-app callbacks back to this instance through user_data.
void Runtime::Prepare() {
  m_window.hooks.userData = this;
  m_window.hooks.init = &Runtime::OnInit;
  m_window.hooks.frame = &Runtime::OnFrame;
  m_window.hooks.cleanup = &Runtime::OnCleanup;
}

// Exposed so the template entry can pass the context to sokol-app.
win::WindowContext& Runtime::Window() {
  return m_window;
}

void Runtime::OnInit(void* user_data) {
  static_cast<Runtime*>(user_data)->Start();
}

void Runtime::OnFrame(void* user_data, double raw_dt) {
  static_cast<Runtime*>(user_data)->Frame(raw_dt);
}

void Runtime::OnCleanup(void* user_data) {
  static_cast<Runtime*>(user_data)->Cleanup();
}

// Mount the project, initialize Lua/renderer, bind the API, then run main.lua.
void Runtime::Start() {
  string mount_error;
  bool mounted = false;
  switch (m_config.sourceKind) {
    case vfs::SourceKind::DISK:
      mounted = m_vfs.MountDisk(m_config.sourcePath, &mount_error);
      break;
    case vfs::SourceKind::PAK:
      mounted = m_vfs.MountPakFile(m_config.sourcePath, &mount_error);
      break;
    case vfs::SourceKind::SELF:
      mounted = m_vfs.MountSelf(m_config.sourcePath, &mount_error);
      break;
  }
  if (!mounted) {
    Fail(mount_error);
    return;
  }

  if (!m_lua.Init()) {
    Fail("failed to initialize Lua");
    return;
  }

  if (!m_renderer.Init(m_config.width, m_config.height, m_config.spriteSize)) {
    Fail("renderer initialization failed");
    return;
  }

  // sprites.png is optional; failure keeps the checkerboard placeholder.
  const Result<vector<ui8>> sprites = m_vfs.ReadBinary(m_config.spritesPath);
  if (sprites)
    m_renderer.LoadSpritesFromMemory(sprites->data(), sprites->size());

  // The closures keep raw pointers; both objects outlive the Lua VM.
  const ScriptContext context { &m_renderer, &m_window.input };
  if (!BindLuaApi(m_lua, context)) {
    Fail("Lua API binding failed");
    return;
  }

  const Result<string> main_source = m_vfs.ReadText(m_config.mainScript);
  if (!main_source) {
    Fail(main_source.error().message);
    return;
  }

  string source = *main_source;
  StripBom(source);

  string error;
  if (!m_lua.DoString(source, "@" + m_config.mainScript, &error)) {
    Fail(error);
    return;
  }

  if (m_lua.HasFunction("_init") && !m_lua.Call("_init", {}, &error)) {
    Fail(error);
    return;
  }

  m_hasFixedUpdate = m_lua.HasFunction("_fixed_update");
  m_hasUpdate      = m_lua.HasFunction("_update");
  m_hasDraw        = m_lua.HasFunction("_draw");
  m_hasCleanup     = m_lua.HasFunction("_cleanup");
  m_started = true;
}

// Run fixed updates first, then one variable update, then draw.
void Runtime::Frame(double raw_dt) {
  if (!m_started || m_failed)
    return;

  const double dt = m_time.Tick(raw_dt);
  string error;

  if (m_hasFixedUpdate) {
    bool fixed_failed = false;
    m_time.ConsumeFixed([&](double fixed_dt) {
      if (fixed_failed)
        return;
      if (!m_lua.Call("_fixed_update", { fixed_dt }, &error))
        fixed_failed = true;
    });

    // A script error skips further fixed updates and closes the window.
    if (fixed_failed) {
      Fail(error);
      return;
    }
  }

  if (m_hasUpdate && !m_lua.Call("_update", { dt }, &error)) {
    Fail(error);
    return;
  }

  m_renderer.BeginFrame(win::Width(), win::Height());

  if (m_hasDraw && !m_lua.Call("_draw", { dt }, &error)) {
    m_renderer.EndFrame();
    Fail(error);
    return;
  }

  m_renderer.EndFrame();
}

// _cleanup runs at most once; renderer and VFS are always released.
void Runtime::Cleanup() {
  if (m_started && m_hasCleanup) {
    string error;
    if (!m_lua.Call("_cleanup", {}, &error))
      Logger::Error("runtime: {}", error);
  }

  m_renderer.Shutdown();
  m_vfs.Unmount();
  m_started = false;
}

// Log and request shutdown; GUI builds also show a dialog when a project path
// exists, because stderr is invisible there.
void Runtime::Fail(string_view message) {
  Logger::Error("runtime: {}", message);
#ifdef CRUMB_TEMPLATE

  if (!m_config.sourcePath.empty())
    platform::ShowError("Crumb", message);
#endif
  m_failed = true;
  win::Quit();
}

// Resolve and run a project on the calling thread.
int RunProject(string_view path) {
  const Result<ProjectConfig> result = ResolveProject(path);
  if (!result) {
    Logger::Error("{}", result.error().message);
    return 1;
  }

  Runtime runtime(std::move(*result));
  return runtime.RunBlocking();
}

}

module;
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

module crumb.engine;

import crumb.type;
import crumb.logger;
import crumb.time;
import crumb.renderer;
import crumb.lua;
import crumb.bindings;
import crumb.window;

namespace crumb {

namespace {

// Trim leading/trailing whitespace.
string_view Trim(string_view text) {
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.front()))) {
    text.remove_prefix(1);
  }
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.back()))) {
    text.remove_suffix(1);
  }
  return text;
}

// Parse a positive integer, or return the fallback value.
i32 ParsePositive(string_view text, i32 fallback, i32 maxValue) {
  i32 value = 0;
  const char* begin = text.data();
  const char* end = text.data() + text.size();
  const auto result = std::from_chars(begin, end, value);
  if (result.ec != std::errc {} || result.ptr != end ||
      value <= 0 || value > maxValue) {
    return fallback;
  }
  return value;
}

// Reads simple key = value pairs from crumb.cfg.
bool LoadConfigFile(const std::filesystem::path& path,
                    EngineConfig& config,
                    string* error) {
  std::ifstream file(path);
  if (!file) {
    if (error) *error = "cannot open " + path.string();
    return false;
  }

  string line;
  while (std::getline(file, line)) {
    const auto comment = line.find('#');
    if (comment != string::npos)
      line.resize(comment);

    const auto equal = line.find('=');
    if (equal == string::npos)
      continue;

    const string_view key = Trim(string_view(line).substr(0, equal));
    const string_view value = Trim(string_view(line).substr(equal + 1));
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

  return true;
}

// Resolves a project directory or main.lua path and fills EngineConfig.
bool ResolveProject(string_view path, EngineConfig& config, string* error) {
  namespace fs = std::filesystem;

  std::error_code ec;
  const fs::path target = path.empty()
    ? fs::current_path(ec)
    : fs::path(string(path));
  if (ec) {
    if (error) *error = "cannot resolve project path: " + ec.message();
    return false;
  }

  fs::path projectDir;
  fs::path mainScript;

  if (fs::is_directory(target, ec)) {
    projectDir = target;
    mainScript = projectDir / "main.lua";
  } else if (fs::is_regular_file(target, ec)) {
    if (target.extension() != fs::path(".lua")) {
      if (error) *error = "not a Lua script: " + target.string();
      return false;
    }
    mainScript = target;
    projectDir = target.parent_path();
    if (projectDir.empty())
      projectDir = fs::current_path(ec);
  } else {
    if (error) *error = "project not found: " + target.string();
    return false;
  }

  std::error_code absEc;
  const fs::path absoluteDir = fs::absolute(projectDir, absEc);
  if (!absEc)
    projectDir = absoluteDir;

  const fs::path absoluteMain = fs::absolute(mainScript, absEc);
  if (!absEc)
    mainScript = absoluteMain;

  if (!fs::is_regular_file(mainScript, ec)) {
    if (error) *error = "main.lua not found: " + mainScript.string();
    return false;
  }

  config.mainScript = mainScript.string();
  config.spritesPath = (projectDir / "sprites.png").string();
  config.title = projectDir.filename().string();
  if (config.title.empty() || config.title == "." || config.title == "..")
    config.title = "crumb";

  const fs::path configPath = projectDir / "crumb.cfg";
  if (fs::is_regular_file(configPath, ec)) {
    if (!LoadConfigFile(configPath, config, error))
      return false;
  }

  if (config.title.empty())
    config.title = "crumb";

  return true;
}

} // namespace

Engine::Engine(EngineConfig config)
  : m_config(std::move(config)),
    m_time(1.0 / static_cast<double>(
             m_config.fixedFps > 0 ? m_config.fixedFps : 60),
           0.25,
           m_config.maxFixedSteps > 0 ? m_config.maxFixedSteps : 5) {
}

// Initializes Lua, opens the window, and blocks until the game quits.
int Engine::Run() {
  if (m_config.mainScript.empty()) {
    Logger::Error("engine: no main script");
    return 1;
  }

  if (!m_lua.Init()) {
    Logger::Error("engine: failed to initialize Lua");
    return 1;
  }

  win::WindowConfig window;
  window.SetTitle(m_config.title);

  const i32 scale = m_config.windowScale > 0 ? m_config.windowScale : 1;
  window.SetSize(m_config.width * scale, m_config.height * scale);

  win::WindowHooks hooks;
  hooks.userData = this;
  hooks.init = &Engine::OnInit;
  hooks.frame = &Engine::OnFrame;
  hooks.cleanup = &Engine::OnCleanup;

  win::Run(window, hooks);
  return m_failed ? 1 : 0;
}

void Engine::OnInit(void* userData) {
  static_cast<Engine*>(userData)->Start();
}

void Engine::OnFrame(void* userData, double rawDt) {
  static_cast<Engine*>(userData)->Frame(rawDt);
}

void Engine::OnCleanup(void* userData) {
  static_cast<Engine*>(userData)->Cleanup();
}

// Called once after the window is created.
void Engine::Start() {
  if (!m_renderer.Init(m_config.width, m_config.height, m_config.spriteSize)) {
    Fail("renderer initialization failed");
    return;
  }

  // Optional project sprite sheet; failure keeps the checker placeholder.
  if (!m_config.spritesPath.empty())
    m_renderer.LoadSprites(m_config.spritesPath);

  const ScriptContext context { &m_renderer };
  if (!BindLuaApi(m_lua, context)) {
    Fail("Lua API binding failed");
    return;
  }

  string error;
  if (!m_lua.DoFile(m_config.mainScript, &error)) {
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

// Fixed updates first, then one variable update, then draw.
void Engine::Frame(double rawDt) {
  if (!m_started || m_failed)
    return;

  const double dt = m_time.Tick(rawDt);
  string error;

  if (m_hasFixedUpdate) {
    // Catch up on fixed steps first; _update still runs once per frame.
    bool fixedFailed = false;
    m_time.ConsumeFixed([&](double fixedDt) {
      if (fixedFailed)
        return;
      if (!m_lua.Call("_fixed_update", { fixedDt }, &error))
        fixedFailed = true;
    });

    if (fixedFailed) {
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

// Runs the optional _cleanup callback and releases renderer resources.
void Engine::Cleanup() {
  if (m_started && m_hasCleanup) {
    string error;
    if (!m_lua.Call("_cleanup", {}, &error))
      Logger::Error("engine: {}", error);
  }

  m_renderer.Shutdown();
  m_started = false;
}

// Logs the error once and requests a clean window shutdown.
void Engine::Fail(string_view message) {
  Logger::Error("engine: {}", message);
  m_failed = true;
  win::Quit();
}

int RunProject(string_view path) {
  EngineConfig config;
  string error;
  if (!ResolveProject(path, config, &error)) {
    Logger::Error("{}", error);
    return 1;
  }

  Engine engine(std::move(config));
  return engine.Run();
}

} // namespace crumb

export module crumb.engine;

import crumb.type;
import crumb.renderer;
import crumb.lua;
import crumb.time;

export namespace crumb {

struct EngineConfig {
  // Window title, normally the project name.
  string title = "crumb";
  // Absolute path to main.lua.
  string mainScript;
  // Absolute path to sprites.png; the file is optional.
  string spritesPath;
  // Logical game resolution and sprite grid.
  i32 width = 320;
  i32 height = 180;
  i32 spriteSize = 16;
  i32 windowScale = 2;
  // Fixed-step settings for _fixed_update.
  i32 fixedFps = 60;
  i32 maxFixedSteps = 5;
};

class Engine {
public:
  explicit Engine(EngineConfig config);
  ~Engine() = default;

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;
  Engine(Engine&&) = delete;
  Engine& operator=(Engine&&) = delete;

  // Creates the window, loads main.lua, then runs the update/draw loop until quit.
  int Run();

private:
  // Sokol-app callbacks; userData is the Engine instance.
  static void OnInit(void* userData);
  static void OnFrame(void* userData, double rawDt);
  static void OnCleanup(void* userData);

  // Called once after the window exists.
  void Start();
  // One frame: fixed updates, _update, then _draw.
  void Frame(double rawDt);
  // Optional _cleanup plus renderer shutdown.
  void Cleanup();
  // Print an error and ask the window to close.
  void Fail(string_view message);

  // Owned runtime objects. Lua closes before the renderer on destruction.
  EngineConfig m_config;
  Renderer m_renderer;
  LuaVM m_lua;
  Time m_time;

  bool m_started = false;
  bool m_failed = false;
  bool m_hasFixedUpdate = false;
  bool m_hasUpdate = false;
  bool m_hasDraw = false;
  bool m_hasCleanup = false;
};

// Resolves a project directory or a main.lua path, reads crumb.cfg, and runs it.
int RunProject(string_view path);

} // namespace crumb

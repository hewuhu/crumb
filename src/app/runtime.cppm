export module crumb.app.runtime;

import crumb.core.types;
import crumb.core.error;
import crumb.core.time;
import crumb.gfx.renderer;
import crumb.platform.input;
import crumb.platform.vfs;
import crumb.platform.window;
import crumb.script.lua;

export namespace crumb {

// Project settings resolved from the path passed to the CLI and from crumb.cfg.
struct ProjectConfig {
  string title = "crumb";

  // Logical names inside the mounted VFS.
  string mainScript = "main.lua";
  string spritesPath = "sprites.png";

  // Mount source: a directory, an archive, or a self-contained executable.
  vfs::SourceKind sourceKind = vfs::SourceKind::DISK;
  string sourcePath;

  // Logical game resolution, sprite cell size, and integer window scale.
  i32 width = 320;
  i32 height = 180;
  i32 spriteSize = 16;
  i32 windowScale = 2;

  // Fixed-step settings passed to Time.
  i32 fixedFps = 60;
  i32 maxFixedSteps = 5;
};

// Owns the window, renderer, Lua VM, clock, and VFS for one project.
class Runtime {
public:
  explicit Runtime(ProjectConfig config);
  ~Runtime() = default;

  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;
  Runtime(Runtime&&) = delete;
  Runtime& operator=(Runtime&&) = delete;

  // Install the sokol-app hooks; call before RunBlocking() or MakeDesc().
  void Prepare();

  // Editor path: create the window and block until the game quits.
  int RunBlocking();

  // Template path: sokol-app creates the window from these.
  win::WindowContext& Window();
  win::WindowConfig MakeWindowConfig() const;

private:
  static void OnInit(void* user_data);
  static void OnFrame(void* user_data, double raw_dt);
  static void OnCleanup(void* user_data);

  // Called once after the window exists.
  void Start();

  // One frame: fixed updates, _update, then _draw.
  void Frame(double raw_dt);

  // Optional _cleanup plus renderer/VFS shutdown.
  void Cleanup();

  // Log an error, show a dialog in GUI builds, and request window shutdown.
  void Fail(string_view message);

  // Declaration order matters: members are destroyed in reverse, so Lua closes
  // before the renderer.
  ProjectConfig m_config;
  Renderer m_renderer;
  LuaVM m_lua;
  Time m_time;
  vfs::Vfs m_vfs;
  win::WindowContext m_window;

  bool m_started = false;
  bool m_failed = false;
  bool m_hasFixedUpdate = false;
  bool m_hasUpdate = false;
  bool m_hasDraw = false;
  bool m_hasCleanup = false;
};

// Resolve a directory, .lua, .pak, or executable and load its crumb.cfg.
Result<ProjectConfig> ResolveProject(string_view path);

// Editor helper: resolve `path` and run it blocking until the window closes.
int RunProject(string_view path);

}

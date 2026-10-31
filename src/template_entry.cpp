#include <sokol/sokol_app.h>

// Exported-template entry point. sokol-app supplies main/WinMain and owns the
// event loop; this file only describes the window and runtime.

import crumb.app.runtime;
import crumb.core.types;
import crumb.core.error;
import crumb.core.log;
import crumb.platform.dialog;
import crumb.platform.launch;
import crumb.platform.window;

namespace {
using namespace crumb;

// Prefer --project; otherwise let ResolveProject inspect the executable itself
// (appended footer, sibling .pak, or the executable directory in dev builds).
ProjectConfig MakeConfig() {
  const platform::LaunchOptions options =
    platform::MakeLaunchOptions();

  const string path = !options.projectPath.empty()
    ? options.projectPath
    : (!options.executablePath.empty()
         ? options.executablePath
         : options.executableDir);

  const Result<ProjectConfig> resolved =
    ResolveProject(path);
  if (!resolved) {
    Logger::Error("template: {}", resolved.error().message);
    platform::ShowError("Crumb", resolved.error().message);
    return {};
  }
  return *resolved;
}

}

// The returned description and every pointer it holds must outlive the call,
// hence the function-static config, runtime, and window.
sapp_desc sokol_main(int, char**) {
  static ProjectConfig config = MakeConfig();
  static Runtime runtime(config);
  runtime.Prepare();
  static win::WindowConfig window = runtime.MakeWindowConfig();
  return win::MakeDesc(runtime.Window(), window);
}

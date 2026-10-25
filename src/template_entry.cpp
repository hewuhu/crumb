#include <sokol/sokol_app.h>

import crumb.app.runtime;
import crumb.core.types;
import crumb.core.error;
import crumb.core.log;
import crumb.platform.dialog;
import crumb.platform.launch;
import crumb.platform.window;

namespace {
using namespace crumb;
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

} // namespace

// sokol-app's generated WinMain/main calls this. Returning the description
// hands the event loop to sokol-app, so the runtime lives in function statics.
sapp_desc sokol_main(int, char**) {
  static ProjectConfig config = MakeConfig();
  static Runtime runtime(config);
  runtime.Prepare();
  static win::WindowConfig window = runtime.MakeWindowConfig();
  return win::MakeDesc(runtime.Window(), window);
}

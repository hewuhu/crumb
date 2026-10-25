module;
#include <expected>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

export module crumb.editor.cli;
import crumb.editor.print;

import crumb.core.version;
import crumb.core.types;
import crumb.core.error;
import crumb.core.log;
import crumb.core.assets;
import crumb.app.runtime;
import crumb.editor.exporter;

export namespace crumb {

struct Cmd {
  string_view name;
  std::vector<string_view> args;

  static Cmd Parse(int argc, char** argv) {
    Cmd command;
    command.name = argv[1];
    for (int i = 2; i < argc; ++i)
      command.args.emplace_back(argv[i]);

    return command;
  }
};

VoidResult WriteFile(const std::filesystem::path& path, string_view content) {
  std::ofstream file(path, std::ios::out | std::ios::binary);
  if (!file) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_IO, "cannot open " + path.string()) };
  }

  file.write(content.data(), static_cast<std::streamsize>(content.size()));
  if (!file.good()) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_IO, "cannot write " + path.string()) };
  }

  return {};
}

constexpr string_view LUARC_JSON = R"JSON({
  "$schema": "https://raw.githubusercontent.com/LuaLS/vscode-lua/master/setting/schema.json",
  "runtime.version": "Lua 5.5",
  "workspace.library": ["meta"],
  "workspace.checkThirdParty": false,
  "diagnostics.globals": [
    "gfx",
    "input",
    "crumb",
    "_init",
    "_update",
    "_draw",
    "_cleanup"
  ],
  "diagnostics.disable": ["lowercase-global"]
})JSON";

// Write .luarc.json and meta/crumb.lua for LuaLS.
VoidResult WriteLspFiles(const std::filesystem::path& project_dir) {
  namespace fs = std::filesystem;

  std::error_code err;
  const fs::path meta_dir = project_dir / "meta";
  fs::create_directories(meta_dir, err);
  if (err) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_IO,
                "cannot create '" + meta_dir.string() + "': " + err.message()) };
  }

  const Asset stub = LspStub();
  if (stub.Empty()) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_NOT_FOUND, "LSP stub asset is missing") };
  }

  const string_view stub_text(
    reinterpret_cast<c_string>(stub.data), stub.size);

  VoidResult written = WriteFile(meta_dir / "crumb.lua", stub_text);
  if (!written)
    return written;

  return WriteFile(project_dir / ".luarc.json", LUARC_JSON);
}

// Resolve a project directory or main.lua and refresh its LSP files.
VoidResult InstallLsp(string_view path) {
  namespace fs = std::filesystem;

  std::error_code err;
  const fs::path target = path.empty()
    ? fs::current_path(err)
    : fs::path(string(path));
  if (err) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_IO, "cannot resolve path: " + err.message()) };
  }

  fs::path project_dir;
  if (fs::is_directory(target, err)) {
    project_dir = target;
  } else if (fs::is_regular_file(target, err)) {
    if (target.extension() != fs::path(".lua")) {
      return std::unexpected<Error> {
        MakeError(ErrorCode::ERROR_INVALID_ARGUMENT,
                  "not a Lua script: " + target.string()) };
    }
    project_dir = target.parent_path();
    if (project_dir.empty())
      project_dir = fs::current_path(err);
  } else {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_NOT_FOUND, "project not found: " + target.string()) };
  }

  const fs::path absolute_dir = fs::absolute(project_dir, err);
  if (!err)
    project_dir = absolute_dir;

  return WriteLspFiles(project_dir);
}

// Scaffold a new game project in the current directory.
VoidResult CreateProject(string_view name) {
  namespace fs = std::filesystem;

  std::error_code err;
  const fs::path cwd = fs::current_path(err);
  if (err) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_IO,
                "cannot get current directory: " + err.message()) };
  }

  const fs::path project_dir = cwd / fs::path(string(name));

  if (fs::exists(project_dir, err)) {
    if (!fs::is_directory(project_dir, err)) {
      return std::unexpected<Error> {
        MakeError(ErrorCode::ERROR_INVALID_ARGUMENT,
                  "'" + project_dir.string() + "' already exists and is not a directory") };
    }
    if (fs::exists(project_dir / "main.lua", err)) {
      return std::unexpected<Error> {
        MakeError(ErrorCode::ERROR_INVALID_ARGUMENT,
                  "'" + project_dir.string() + "' already has a main.lua") };
    }
  }

  fs::create_directories(project_dir, err);
  if (err) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_IO,
                "cannot create '" + project_dir.string() + "': " + err.message()) };
  }

  const string name_text(name);
  const string main_lua = R"LUA(function _init()
  print("hello crumb")
end

function _update(dt)
end

function _draw(dt)
  gfx.cls(gfx.BLACK)
  gfx.text("hello crumb", 8, 8, gfx.WHITE, 0.5)
end
)LUA";

  const string cfg =
      "# crumb project config\n"
      "name = " + name_text + "\n"
      "width = 320\n"
      "height = 180\n"
      "sprite_size = 16\n"
      "window_scale = 2\n"
      "fixed_fps = 60\n"
      "max_fixed_steps = 5\n";

  VoidResult written = WriteFile(project_dir / "main.lua", main_lua);
  if (!written)
    return written;

  written = WriteFile(project_dir / "crumb.cfg", cfg);
  if (!written)
    return written;

  return WriteLspFiles(project_dir);
}

// CLI entry point. Returns 0 on success, 1 on a command error.
int CliRun(int argc, char* argv[]) {
  if (argc < 2) {
    Println(ANSI_BLUE, "Usage: ");
    Println("  crumb <command> [args]\n");

    Println(ANSI_BLUE, "Commands:");
    Println(
      "  version            Show the crumb version\n"
      "  create [name]      Create a new project (default: NewGame)\n"
      "  lsp [path]         Write .luarc.json and meta/crumb.lua for LuaLS\n"
      "  run [path]         Run a project (default: current directory)\n"
      "  dev [path]         Run a project in dev mode (currently same as run)\n"
      "  export [path]      Export a game (default: single exe)\n"
    );
    return 0;
  }

  const auto command = Cmd::Parse(argc, argv);

  if (command.name == "version") {
    if (!command.args.empty()) {
      Logger::Error("version takes no arguments");
      return 1;
    }

    Println("Version: {}", ENGINE_VERSION.ToStr());
    return 0;
  }

  if (command.name == "create") {
    if (command.args.size() > 1) {
      Logger::Error("create takes at most one argument");
      return 1;
    }

    const string_view name =
      command.args.empty() ? string_view { "NewGame" } : command.args[0];
    const VoidResult created = CreateProject(name);
    if (!created) {
      Logger::Error("{}", created.error().message);
      return 1;
    }

    Println(ANSI_GREEN, "Create finish! {}", name);
    return 0;
  }

  if (command.name == "lsp") {
    if (command.args.size() > 1) {
      Logger::Error("lsp takes at most one argument");
      return 1;
    }

    const string_view path =
      command.args.empty() ? string_view { "." } : command.args[0];
    const VoidResult installed = InstallLsp(path);
    if (!installed) {
      Logger::Error("{}", installed.error().message);
      return 1;
    }

    Println(ANSI_GREEN, "LSP files ready.");
    return 0;
  }

  if (command.name == "export") {
    ExportOptions options;
    bool has_project = false;
    for (usize i = 0; i < command.args.size(); ++i) {
      const string_view arg = command.args[i];
      if (arg == "-o" || arg == "--out") {
        if (i + 1 >= command.args.size()) {
          Logger::Error("{} needs a path", arg);
          return 1;
        }
        options.outputPath = string(command.args[++i]);
      } else if (arg == "--template") {
        if (i + 1 >= command.args.size()) {
          Logger::Error("--template needs a path");
          return 1;
        }
        options.templatePath = string(command.args[++i]);
      } else if (arg == "--format") {
        if (i + 1 >= command.args.size()) {
          Logger::Error("--format needs 'single' or 'pak'");
          return 1;
        }
        const string_view format = command.args[++i];
        if (format == "pak")
          options.format = ExportFormat::EXE_PAK;
        else if (format == "single")
          options.format = ExportFormat::SINGLE_FILE;
        else {
          Logger::Error("unknown export format '{}'", format);
          return 1;
        }
      } else if (!has_project) {
        options.projectPath = string(arg);
        has_project = true;
      } else {
        Logger::Error("unexpected export argument '{}'", arg);
        return 1;
      }
    }

    if (!has_project)
      options.projectPath = ".";

    const Result<string> exported = ExportProject(std::move(options));
    if (!exported) {
      Logger::Error("{}", exported.error().message);
      return 1;
    }

    Println(ANSI_GREEN, "Export finish! {}", *exported);
    return 0;
  }

  if (command.name == "run" || command.name == "dev") {
    if (command.args.size() > 1) {
      Logger::Error("{} takes at most one argument", command.name);
      return 1;
    }

    if (command.name == "dev")
      Println(ANSI_BLUE, "dev: live reload is not implemented yet; running once");

    const string_view path =
      command.args.empty() ? string_view { "." } : command.args[0];
    return RunProject(path);
  }

  Logger::Error("unknown command '{}'", command.name);
  return 1;
}

} // namespace crumb

module;
#include <expected>
#include <system_error>

#include <string_view>
#include <vector>

#include <filesystem>
#include <fstream>


export module crumb.cli;
import crumb.type;
import crumb.logger;
import crumb.assets;
import crumb.engine;

export namespace crumb {

// version
struct Version {
  ui32 major, minor, patch;

  constexpr auto operator<=>(const Version&) const = default;

  constexpr bool compatible_with(const Version& req) const {
    return major == req.major && *this >= req;
  }

  constexpr std::expected<ui32, string> pack() const {
    if (major > 0x7FU || minor > 0x3FFU || patch > 0xFFFU)
      return std::unexpected<string> {"Version field overflow"};

    return (major << 22) | (minor << 12) | patch;
  }

  static constexpr Version unpack(ui32 v) {
    return { (v >> 22) & 0x7FU, (v >> 12) & 0x3FFU, v & 0xFFFU };
  }

  string ToStr() const {
    char buf[34];
    char* p = buf;

    auto put = [&](unsigned v) {
      char tmp[10];
      int n = 0;
      do { tmp[n++] = char('0' + v % 10); v /= 10; } while (v);
      while (n) *p++ = tmp[--n];
    };

    put(major); *p++ = '.';
    put(minor); *p++ = '.';
    put(patch);

    return string(buf, p);
  }

};
inline constexpr Version APP_VERSION {0, 0, 1};

struct Cmd {
  string_view name;
  std::vector<string_view> args;

  static Cmd Parse(int argc, char** argv) {
    Cmd c;
    c.name = argv[1];
    for (int i = 2; i < argc; ++i)
      c.args.emplace_back(argv[i]);

    return c;
  }
};

bool WriteFile(const std::filesystem::path& path, string_view content) {
  std::ofstream file(path, std::ios::out | std::ios::binary);
  if (!file)
    return false;

  file.write(content.data(), static_cast<std::streamsize>(content.size()));
  return file.good();
}

constexpr string_view kLuarcJson = R"JSON({
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
bool WriteLspFiles(const std::filesystem::path& projectDir) {
  namespace fs = std::filesystem;

  std::error_code err;
  const fs::path metaDir = projectDir / "meta";
  fs::create_directories(metaDir, err);
  if (err) {
    Logger::Error("cannot create '{}': {}", metaDir.string(), err.message());
    return false;
  }

  const assets::Asset stub = assets::LspStub();
  if (stub.Empty()) {
    Logger::Error("LSP stub asset is missing");
    return false;
  }

  const string_view stubText(
    reinterpret_cast<const char*>(stub.data), stub.size);

  if (!WriteFile(metaDir / "crumb.lua", stubText) ||
      !WriteFile(projectDir / ".luarc.json", kLuarcJson)) {
    Logger::Error("cannot write LSP files under '{}'", projectDir.string());
    return false;
  }

  return true;
}

// Resolve a project directory or main.lua and refresh its LSP files.
bool InstallLsp(string_view path) {
  namespace fs = std::filesystem;

  std::error_code err;
  const fs::path target = path.empty()
    ? fs::current_path(err)
    : fs::path(string(path));
  if (err) {
    Logger::Error("cannot resolve path: {}", err.message());
    return false;
  }

  fs::path projectDir;
  if (fs::is_directory(target, err)) {
    projectDir = target;
  } else if (fs::is_regular_file(target, err)) {
    if (target.extension() != fs::path(".lua")) {
      Logger::Error("not a Lua script: {}", target.string());
      return false;
    }
    projectDir = target.parent_path();
    if (projectDir.empty())
      projectDir = fs::current_path(err);
  } else {
    Logger::Error("project not found: {}", target.string());
    return false;
  }

  const fs::path absoluteDir = fs::absolute(projectDir, err);
  if (!err)
    projectDir = absoluteDir;

  return WriteLspFiles(projectDir);
}

// Scaffold a new game project in the current directory.
bool CreateProject(string_view name) {
  namespace fs = std::filesystem;

  std::error_code err;
  const fs::path cwd = fs::current_path(err);
  if (err) {
    Logger::Error("cannot get current directory: {}", err.message() );
    return false;
  }

  const fs::path proj = cwd / fs::path(string(name));

  if (fs::exists(proj, err)) {
    if (!fs::is_directory(proj, err)) {
      Logger::Error("'{}' already exists and is not a directory", proj.string());
      return false;
    }
    if (fs::exists(proj / "main.lua", err)) {
      Logger::Error("'{}' already has a main.lua", proj.string());
      return false;
    }
  }

  fs::create_directories(proj, err);
  if (err) {
    Logger::Error("cannot create '{}': {}", proj.string(), err.message());
    return false;
  }

  const string name_str(name);
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

  const string conf =
      "# crumb project config\n"
      "name = " + name_str + "\n"
      "width = 320\n"
      "height = 180\n"
      "sprite_size = 16\n"
      "window_scale = 2\n"
      "fixed_fps = 60\n"
      "max_fixed_steps = 5\n";

  if (!WriteFile(proj / "main.lua", main_lua) ||
      !WriteFile(proj / "crumb.cfg", conf)) {
    Logger::Error("project created but some files could not be written");
    return false;
  }

  if (!WriteLspFiles(proj))
    return false;

  return true;
}

// CLI entry point. Returns 0 on success, 1 on a command error.
int CliRun(int argc, char* argv[]) {
  if (argc < 2) {
    Println(A_Bule, "Usage: ");
    Println("  crumb <command> [args]\n");

    Println(A_Bule, "Commands:");
    Println(
      "  version            Show the crumb version\n"
      "  create [name]      Create a new project (default: NewGame)\n"
      "  lsp [path]         Write .luarc.json and meta/crumb.lua for LuaLS\n"
      "  run [path]         Run a project (default: current directory)\n"
      "  dev [path]         Run a project in dev mode (currently same as run)\n"
    );
    return 0;
  }

  const auto cmd = Cmd::Parse(argc, argv);

  if (cmd.name == "version") {
    if (!cmd.args.empty()) {
      Logger::Error("version takes no arguments");
      return 1;
    }

    Println("Version: {}", APP_VERSION.ToStr());
    return 0;
  }

  if (cmd.name == "create") {
    if (cmd.args.size() > 1) {
      Logger::Error("create takes at most one argument");
      return 1;
    }

    const string_view name = cmd.args.empty() ? string_view { "NewGame" } : cmd.args[0];
    if (!CreateProject(name))
      return 1;

    Println(A_Green, "Create finish! {}", name);
    return 0;
  }

  if (cmd.name == "lsp") {
    if (cmd.args.size() > 1) {
      Logger::Error("lsp takes at most one argument");
      return 1;
    }

    const string_view path = cmd.args.empty() ? string_view { "." } : cmd.args[0];
    if (!InstallLsp(path))
      return 1;

    Println(A_Green, "LSP files ready.");
    return 0;
  }

  if (cmd.name == "run" || cmd.name == "dev") {
    if (cmd.args.size() > 1) {
      Logger::Error("{} takes at most one argument", cmd.name);
      return 1;
    }

    if (cmd.name == "dev")
      Println(A_Bule, "dev: live reload is not implemented yet; running once");

    const string_view path = cmd.args.empty() ? string_view { "." } : cmd.args[0];
    return RunProject(path);
  }

  Logger::Error("unknown command '{}'", cmd.name);
  return 1;
}


} // namespace crumb

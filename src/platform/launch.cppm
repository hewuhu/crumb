module;
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

export module crumb.platform.launch;
import crumb.core.types;

export namespace crumb::platform {

// Where the exported template looks for its project. Windows GUI applications
// do not get a useful argv[] from main(), so the command line is parsed here.
struct LaunchOptions {
  string executablePath;
  string executableDir;

  // Empty means "inspect executablePath" (footer, sibling pak, or exe dir).
  string projectPath;
};

LaunchOptions MakeLaunchOptions();

}

#ifdef _WIN32

namespace {
using namespace crumb;

// Convert a null-terminated UTF-16 string to UTF-8.
string WideToUtf8(const wchar_t* text) {
  if (text == nullptr)
    return {};
  const int size = ::WideCharToMultiByte(
    CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (size <= 0)
    return {};
  string result(static_cast<crumb::usize>(size), '\0');
  ::WideCharToMultiByte(
    CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
  result.resize(static_cast<crumb::usize>(size - 1));
  return result;
}

string U8ToString(const std::u8string& text) {
  return string(reinterpret_cast<const char*>(text.data()), text.size());
}

// Grow the buffer until GetModuleFileNameW reports a non-truncated path.
std::filesystem::path ExecutablePath() {
  std::wstring buffer(MAX_PATH, L'\0');
  for (;;) {
    const DWORD length = ::GetModuleFileNameW(
      nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0)
      return {};
    if (length < buffer.size() - 1) {
      buffer.resize(length);
      break;
    }
    buffer.resize(buffer.size() * 2);
  }
  return std::filesystem::path(buffer);
}

// CommandLineToArgvW handles the quoting that raw GetCommandLineW does not.
// The last --project / --project= value wins.
string ProjectArgument() {
  int argc = 0;
  LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
  if (argv == nullptr)
    return {};

  string project;
  for (int i = 1; i < argc; ++i) {
    if (::wcscmp(argv[i], L"--project") == 0 && i + 1 < argc) {
      project = WideToUtf8(argv[++i]);
    } else if (::wcsncmp(argv[i], L"--project=", 10) == 0) {
      project = WideToUtf8(argv[i] + 10);
    }
  }

  ::LocalFree(argv);
  return project;
}

}

namespace crumb::platform {

// Windows: derive both paths from the running executable.
LaunchOptions MakeLaunchOptions() {
  LaunchOptions options;
  const std::filesystem::path exe = ExecutablePath();
  options.executablePath = U8ToString(exe.u8string());
  options.executableDir = U8ToString(exe.parent_path().u8string());
  options.projectPath = ProjectArgument();
  return options;
}

}

#else

namespace crumb::platform {

// Non-Windows fallback for callers that build on other platforms; pass
// --project explicitly, because the current build has no Windows template here.
LaunchOptions MakeLaunchOptions() {
  LaunchOptions options;
  std::error_code ec;
  const std::filesystem::path directory = std::filesystem::current_path(ec);
  if (!ec) {
    options.executableDir = directory.string();
    options.executablePath = (directory / "crumb_template").string();
  }
  return options;
}

}

#endif

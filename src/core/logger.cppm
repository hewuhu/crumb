module;
#include <print>
#include <format>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

export module crumb.logger;
import crumb.type;

export namespace crumb {

enum class Level {
  Debug,
  Info,
  Warn,
  Error,
};

struct AnsiColor {
  string_view code;
};

inline constexpr AnsiColor A_White {"\x1b[97m"};
inline constexpr AnsiColor A_Red   {"\x1b[31m"};
inline constexpr AnsiColor A_Green {"\x1b[32m"};
inline constexpr AnsiColor A_Reset {"\x1b[0m"};
inline constexpr AnsiColor A_Bule  {"\x1b[94m"};

} // namespace crumb

namespace {

using namespace crumb;

constexpr string_view ColorOf(Level lv) noexcept {
  switch (lv) {
    case Level::Debug: return "\x1b[90m";  // 亮黑（灰）
    case Level::Info:  return "\x1b[32m";  // 绿
    case Level::Warn:  return "\x1b[33m";  // 黄
    case Level::Error: return "\x1b[31m";  // 红
  }
  return {};
}

constexpr string_view Tag(Level lv) noexcept {
  switch (lv) {
    case Level::Debug: return "[Debug] ";
    case Level::Info:  return "[Info]  ";
    case Level::Warn:  return "[Warn]  ";
    case Level::Error: return "[Error] ";
  }
  return "[?]    ";
}

inline bool IsTty(std::FILE* f) noexcept {
#ifdef _WIN32
  return ::_isatty(::_fileno(f)) != 0;
#else
  return ::isatty(::fileno(f)) != 0;
#endif
}

inline void EnableVT() noexcept {
#ifdef _WIN32
  HANDLE h = ::GetStdHandle(STD_OUTPUT_HANDLE);
  if (h == INVALID_HANDLE_VALUE) return;
  DWORD mode = 0;
  if (!::GetConsoleMode(h, &mode)) return;
  if (!(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
    ::SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

inline bool DetectColors() noexcept {
  auto HasEnv = [](const char* name) -> bool {
#ifdef _WIN32
    char*  buf  = nullptr;
    size_t len  = 0;
    if (::_dupenv_s(&buf, &len, name) != 0 || buf == nullptr) {
      std::free(buf);
      return false;
    }
    std::free(buf);
    return true;
#else
    return std::getenv(name) != nullptr;
#endif
  };

  auto GetEnv = [](const char* name) -> const char* {
#ifdef _WIN32
    static thread_local char* buf = nullptr;
    if (buf) { std::free(buf); buf = nullptr; }
    size_t len = 0;
    if (::_dupenv_s(&buf, &len, name) != 0) return nullptr;
    return buf;
#else
    return std::getenv(name);
#endif
  };

  if (HasEnv("NO_COLOR"))
    return false;

  if (HasEnv("FORCE_COLOR"))
    return true;

  if (const char* term = GetEnv("TERM");
      term != nullptr && std::strcmp(term, "dumb") == 0)
    return false;

  EnableVT();
  return IsTty(stdout);
}

inline bool ColorsEnabled() noexcept {
  static const bool enabled = DetectColors();
  return enabled;
}

} // namespace


export namespace crumb {

class Logger {
private:
  inline static Level m_minLevel = Level::Debug;

  template <typename... Args>
  static void Emit(Level lv, std::format_string<Args...> fmt, Args&&... args) {
    auto body = std::format(fmt, std::forward<Args>(args)...);
    if (ColorsEnabled()) {
      if (lv == Level::Error) {
        std::println("{}{}{}{}", ColorOf(lv), Tag(lv), body, A_Reset.code);
      } else {
        std::println("{}{}{}{}", ColorOf(lv), Tag(lv), A_Reset.code, body);
      }
    } else {
      std::println("{}{}", Tag(lv), body);
    }
  }

public:
  static void WriteLine(AnsiColor color, string_view body) {
  if (ColorsEnabled())
    std::println("{}{}{}", color.code, body, A_Reset.code);
  else
    std::println("{}", body);
  }

  template <typename... Args>
  static void Debug(std::format_string<Args...> fmt, Args&&... args) {
    if (Level::Debug < m_minLevel) return;
    Emit(Level::Debug, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void Info(std::format_string<Args...> fmt, Args&&... args) {
    if (Level::Info < m_minLevel) return;
    Emit(Level::Info, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void Warn(std::format_string<Args...> fmt, Args&&... args) {
    if (Level::Warn < m_minLevel) return;
    Emit(Level::Warn, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void Error(std::format_string<Args...> fmt, Args&&... args) {
    if (Level::Error < m_minLevel) return;
    Emit(Level::Error, fmt, std::forward<Args>(args)...);
  }

};

template <typename... Args>
void Println(AnsiColor color, std::format_string<Args...> fmt, Args&&... args) {
  auto body = std::format(fmt, std::forward<Args>(args)...);
  Logger::WriteLine(color, body);
}

template <typename... Args>
void Println(std::format_string<Args...> fmt, Args&&... args) {
  Println(A_White, fmt, std::forward<Args>(args)...);
}

} // namespace crumb

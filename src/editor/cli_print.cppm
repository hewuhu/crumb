module;
#include <print>
#include <format>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

export module crumb.editor.print;
import crumb.core.types;

export namespace crumb {

// ANSI escape sequence wrapper; an empty code means plain output.
struct AnsiColor {
  string_view code;
};

inline constexpr AnsiColor ANSI_WHITE  {"\x1b[97m"};
inline constexpr AnsiColor ANSI_RED    {"\x1b[31m"};
inline constexpr AnsiColor ANSI_GREEN  {"\x1b[32m"};
inline constexpr AnsiColor ANSI_YELLOW {"\x1b[33m"};
inline constexpr AnsiColor ANSI_BLUE   {"\x1b[94m"};
inline constexpr AnsiColor ANSI_RESET  {"\x1b[0m"};

// Print one line, using color only when stdout is an interactive terminal.
// On Windows, virtual-terminal processing is enabled once on first use.
template <typename... Args>
void Println(AnsiColor color, std::format_string<Args...> fmt, Args&&... args) {
  static const bool use_color = [] {
#ifdef _WIN32
    HANDLE handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (handle == INVALID_HANDLE_VALUE || !::GetConsoleMode(handle, &mode))
      return false;
    ::SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    return true;
#else
    return ::isatty(STDOUT_FILENO) != 0;
#endif
  }();

  if (use_color)
    std::println("{}{}{}", color.code,
                 std::format(fmt, std::forward<Args>(args)...), ANSI_RESET.code);
  else
    std::println(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void Println(std::format_string<Args...> fmt, Args&&... args) {
  Println(ANSI_WHITE, fmt, std::forward<Args>(args)...);
}

}

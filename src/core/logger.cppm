module;
#include <format>
#include <ostream>
#include <string>
#include <utility>

#include <modlog/modlog.hpp>
export module crumb.core.log;

export namespace crumb {

// Thin wrapper over modlog. Two things it fixes:
//   1. the level check happens BEFORE std::format, so disabled logs cost one compare;
//   2. one place to flip the global level / stream / prefix.
// Not thread-safe; modlog itself is not synchronized either.
class Logger {
public:
  Logger() = delete;

  static void SetLevel(modlog::LogLevel level) {
    modlog::modlog_default.minlog = level;
  }

  static modlog::LogLevel Level() {
    return modlog::modlog_default.minlog;
  }

  static void SetOutput(std::ostream* stream) {
    modlog::modlog_default.os = stream;
  }

  static void SetPrefix(bool enabled) {
    modlog::modlog_default.prefix = enabled;
  }

  static void Flush() {
    if (modlog::modlog_default.os != nullptr)
      modlog::modlog_default.os->flush();
  }

  static bool Enabled(modlog::LogLevel level) {
    const auto minimum = modlog::modlog_default.minlog;
    if (minimum == modlog::LogLevel::Disabled)
      return false;
#ifdef NDEBUG
    if (level < modlog::LogLevel::Info)
      return false;
#endif
    return level >= minimum;
  }

  template <typename... Args>
  static void Debug(std::format_string<Args...> fmt, Args&&... args) {
    Emit(modlog::LogLevel::Debug, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void Info(std::format_string<Args...> fmt, Args&&... args) {
    Emit(modlog::LogLevel::Info, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void Warn(std::format_string<Args...> fmt, Args&&... args) {
    Emit(modlog::LogLevel::Warning, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void Error(std::format_string<Args...> fmt, Args&&... args) {
    Emit(modlog::LogLevel::Error, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void Log(modlog::LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
    Emit(level, fmt, std::forward<Args>(args)...);
  }

  template <typename Component, typename... Args>
  static void DebugFor(Component* obj, std::format_string<Args...> fmt, Args&&... args) {
    EmitFor(modlog::LogLevel::Debug, obj, fmt, std::forward<Args>(args)...);
  }

  template <typename Component, typename... Args>
  static void InfoFor(Component* obj, std::format_string<Args...> fmt, Args&&... args) {
    EmitFor(modlog::LogLevel::Info, obj, fmt, std::forward<Args>(args)...);
  }

  template <typename Component, typename... Args>
  static void WarnFor(Component* obj, std::format_string<Args...> fmt, Args&&... args) {
    EmitFor(modlog::LogLevel::Warning, obj, fmt, std::forward<Args>(args)...);
  }

  template <typename Component, typename... Args>
  static void ErrorFor(Component* obj, std::format_string<Args...> fmt, Args&&... args) {
    EmitFor(modlog::LogLevel::Error, obj, fmt, std::forward<Args>(args)...);
  }

  template <typename Component, typename... Args>
  static void LogFor(modlog::LogLevel level, Component* obj, std::format_string<Args...> fmt,
                     Args&&... args) {
    EmitFor(level, obj, fmt, std::forward<Args>(args)...);
  }

private:
  template <typename... Args>
  static void Emit(modlog::LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
    if (!Enabled(level))
      return;
    // MSVC modules: the free operator<<(std::ostream&, std::string) declared by
    // <string> lives in the global module fragment and is not visible when this
    // template is instantiated in another module unit. Passing a C string uses
    // the basic_ostream member overload, which is always visible.
    const std::string text = std::format(fmt, std::forward<Args>(args)...);
    modlog::Log(level) << text.c_str();
  }

  template <typename Component, typename... Args>
  static void EmitFor(modlog::LogLevel level, Component* obj, std::format_string<Args...> fmt,
                      Args&&... args) {
    if (level == modlog::LogLevel::Disabled)
      return;
    const std::string text = std::format(fmt, std::forward<Args>(args)...);
    modlog::Log(level, obj) << text.c_str();
  }
};

} // namespace crumb

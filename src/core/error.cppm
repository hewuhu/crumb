module;
#include <expected>
#include <string>
#include <utility>

export module crumb.core.error;
import crumb.core.types;

export namespace crumb {

// Stable error codes for boundaries that can fail (project config, files,
// future export). Values are uppercase snake_case per 规范.md rule 4.
// The ERROR_ prefix avoids collisions with platform macros such as OVERFLOW.
enum class ErrorCode : ui8 {
  ERROR_UNKNOWN = 0,
  ERROR_INVALID_ARGUMENT,
  ERROR_NOT_FOUND,
  ERROR_IO,
  ERROR_PARSE,
  ERROR_OVERFLOW,
  ERROR_UNSUPPORTED,
};

struct Error {
  ErrorCode code = ErrorCode::ERROR_UNKNOWN;
  string message;
};

// Result<T> is the non-throwing error channel used at config / file / export
// boundaries. Lua-facing calls keep their `string* error` signatures for now.
template <typename T>
using Result = std::expected<T, Error>;

using VoidResult = std::expected<void, Error>;

inline constexpr Error MakeError(ErrorCode code, string message) {
  return Error { code, std::move(message) };
}

} // namespace crumb

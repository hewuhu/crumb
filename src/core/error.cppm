module;
#include <expected>
#include <utility>

export module crumb.core.error;
import crumb.core.types;

export namespace crumb {

// Stable codes for fallible boundaries. The ERROR_ prefix avoids clashes with
// platform macros such as OVERFLOW.
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

// Non-throwing result channel; VoidResult is used when only success matters.
template <typename T>
using Result = std::expected<T, Error>;

using VoidResult = std::expected<void, Error>;

inline constexpr Error MakeError(ErrorCode code, string message) {
  return Error { code, std::move(message) };
}

}

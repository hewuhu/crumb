module;
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

export module crumb.core.types;

export namespace crumb {

// Fixed-width integer aliases. The width is identical on every platform, which
// keeps file formats, network payloads and GPU vertex layouts stable.
using i8  = std::int8_t;
using ui8 = std::uint8_t;
using i16 = std::int16_t;
using ui16 = std::uint16_t;
using i32 = std::int32_t;
using ui32 = std::uint32_t;
using i64 = std::int64_t;
using ui64 = std::uint64_t;

// Pointer-width aliases: 32-bit on x86/arm32, 64-bit on x64/arm64.
using isize = std::intptr_t;
using usize = std::size_t;

using string = std::string;
using string_view = std::string_view;
using c_string = char const*;

// Conversion helpers around static_cast. They exist so call sites stay short
// and name the target type once: to_i32(size) instead of static_cast<i32>(size)
inline constexpr i8 to_i8(auto value) {
  return static_cast<i8>(value);
}

inline constexpr ui8 to_ui8(auto value) {
  return static_cast<ui8>(value);
}

inline constexpr i16 to_i16(auto value) {
  return static_cast<i16>(value);
}

inline constexpr ui16 to_ui16(auto value) {
  return static_cast<ui16>(value);
}

inline constexpr i32 to_i32(auto value) {
  return static_cast<i32>(value);
}

inline constexpr ui32 to_ui32(auto value) {
  return static_cast<ui32>(value);
}

inline constexpr i64 to_i64(auto value) {
  return static_cast<i64>(value);
}

inline constexpr ui64 to_ui64(auto value) { 
  return static_cast<ui64>(value); 
}

inline constexpr isize to_isize(auto value) {
  return static_cast<isize>(value); 
}

inline constexpr usize to_usize(auto value) { 
  return static_cast<usize>(value); 
}

inline constexpr float to_float(auto value) {
  return static_cast<float>(value); 
}

inline constexpr double to_double(auto value) {
  return static_cast<double>(value); 
}

inline constexpr c_string to_c_string(c_string value) {
  return value;
}

inline c_string to_c_string(string const& value) {
  return value.c_str(); 
}

} // namespace crumb

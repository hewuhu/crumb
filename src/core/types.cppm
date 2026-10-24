module;
#include <cstdint>
#include <string>
#include <string_view>
export module crumb.type;

export namespace crumb {

using i8 = std::int8_t;
using ui8 = std::uint8_t;

using i32 = std::int32_t;
using ui32 = std::uint32_t;

using i64 = std::int64_t;
using ui64 = std::uint64_t;

using uchar = unsigned char;

using string = std::string;
using string_view = std::string_view;
using string_ref = string const&;

using c_string = char const*;

using size_t = std::size_t;

struct Color {
  uchar r = 0;
  uchar g = 0;
  uchar b = 0;
  uchar a = 255;
};

// PICO-8 compatible 16-color palette.
inline constexpr Color BLACK       { 0x00, 0x00, 0x00, 255 };
inline constexpr Color DARK_BLUE   { 0x1D, 0x2B, 0x53, 255 };
inline constexpr Color DARK_PURPLE { 0x7E, 0x25, 0x53, 255 };
inline constexpr Color DARK_GREEN  { 0x00, 0x87, 0x51, 255 };
inline constexpr Color BROWN       { 0xAB, 0x52, 0x36, 255 };
inline constexpr Color DARK_GRAY   { 0x5F, 0x57, 0x4F, 255 };
inline constexpr Color LIGHT_GRAY  { 0xC2, 0xC3, 0xC7, 255 };
inline constexpr Color WHITE       { 0xFF, 0xF1, 0xE8, 255 };
inline constexpr Color RED         { 0xFF, 0x00, 0x4D, 255 };
inline constexpr Color ORANGE      { 0xFF, 0xA3, 0x00, 255 };
inline constexpr Color YELLOW      { 0xFF, 0xEC, 0x27, 255 };
inline constexpr Color GREEN       { 0x00, 0xE4, 0x36, 255 };
inline constexpr Color BLUE        { 0x29, 0xAD, 0xFF, 255 };
inline constexpr Color INDIGO      { 0x83, 0x76, 0x9C, 255 };
inline constexpr Color PINK        { 0xFF, 0x77, 0xA8, 255 };
inline constexpr Color PEACH       { 0xFF, 0xCC, 0xAA, 255 };

// Utility colors outside the 16-color palette.
inline constexpr Color TRUE_WHITE  { 255, 255, 255, 255 };
inline constexpr Color TRANSPARENT { 0, 0, 0, 0 };


inline i32 to_i32(auto v) {
  return static_cast<i32>(v);
}

inline ui32 to_ui32(auto v) {
  return static_cast<ui32>(v);
}

inline float to_r32(auto v) {
  return static_cast<float>(v);
}

inline double to_r64(auto v) {
  return static_cast<double>(v);
}

inline size_t to_size_t(auto v) {
  return static_cast<size_t>(v);
}

inline uchar to_uchar(auto v) {
  return static_cast<uchar>(v);
}

inline c_string to_char(auto v) {
  return (c_string)v;
}

inline c_string c_str(string_ref v) {
  return v.c_str();
}

} // namespace crumb

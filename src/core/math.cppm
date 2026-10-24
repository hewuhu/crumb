module;
#include <cmath>

export module crumb.math;
import crumb.type;

// Small 2D math types used by C++ engine code.
// Game scripts normally use plain Lua numbers instead.
export namespace crumb {

// Floating-point 2D vector.
struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;

  Vec2 operator+(Vec2 other) const { return { x + other.x, y + other.y }; }
  Vec2 operator-(Vec2 other) const { return { x - other.x, y - other.y }; }
  Vec2 operator*(float scalar) const { return { x * scalar, y * scalar }; }
  Vec2 operator/(float scalar) const { return { x / scalar, y / scalar }; }

  Vec2& operator+=(Vec2 other) {
    x += other.x;
    y += other.y;
    return *this;
  }

  Vec2& operator-=(Vec2 other) {
    x -= other.x;
    y -= other.y;
    return *this;
  }

  Vec2& operator*=(float scalar) {
    x *= scalar;
    y *= scalar;
    return *this;
  }

  Vec2& operator/=(float scalar) {
    x /= scalar;
    y /= scalar;
    return *this;
  }

  float LengthSquared() const { return x * x + y * y; }
  float Length() const { return std::sqrt(LengthSquared()); }

  Vec2 Normalized() const {
    const float length = Length();
    if (length <= 0.000001f)
      return {};
    return *this / length;
  }

  float Dot(Vec2 other) const { return x * other.x + y * other.y; }
};

// Width/height pair.
struct Size {
  float w = 0.0f;
  float h = 0.0f;
};

// Axis-aligned rectangle in game coordinates.
struct Rect {
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;

  float Right() const { return x + w; }
  float Bottom() const { return y + h; }

  bool Contains(Vec2 point) const {
    return point.x >= x && point.x < Right() &&
           point.y >= y && point.y < Bottom();
  }

  bool Intersects(const Rect& other) const {
    return x < other.Right() && Right() > other.x &&
           y < other.Bottom() && Bottom() > other.y;
  }
};

// Common scalar helpers.
inline constexpr float PI = 3.14159265358979323846f;

inline float Clamp(float value, float lo, float hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

inline float Lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

inline float Approach(float current, float target, float maxDelta) {
  if (maxDelta < 0.0f)
    maxDelta = -maxDelta;

  if (current < target)
    return current + maxDelta < target ? current + maxDelta : target;
  return current - maxDelta > target ? current - maxDelta : target;
}

inline float Wrap(float value, float lo, float hi) {
  if (hi <= lo)
    return lo;

  const float range = hi - lo;
  float result = std::fmod(value - lo, range);
  if (result < 0.0f)
    result += range;
  return lo + result;
}

inline float Radians(float degrees) {
  return degrees * (PI / 180.0f);
}

inline float Degrees(float radians) {
  return radians * (180.0f / PI);
}

} // namespace crumb

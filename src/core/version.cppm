module;
#include <expected>
#include <format>

export module crumb.core.version;
import crumb.core.types;
import crumb.core.error;

export namespace crumb {

// Semantic version with a compact 29-bit packing: 7-bit major in bits 22..28,
// 10-bit minor in bits 12..21, 12-bit patch in bits 0..11. Bits 29..31 are
// unused.
struct Version {
  ui32 major = 0;
  ui32 minor = 0;
  ui32 patch = 0;

  constexpr auto operator<=>(const Version&) const = default;

  // Same major version and at least the required minor/patch.
  constexpr bool CompatibleWith(const Version& required) const {
    return major == required.major && *this >= required;
  }

  constexpr Result<ui32> Pack() const {
    if (major > 0x7FU || minor > 0x3FFU || patch > 0xFFFU) {
      return std::unexpected<Error> {
        Error { ErrorCode::ERROR_OVERFLOW, "Version field overflow" } };
    }

    return (major << 22) | (minor << 12) | patch;
  }

  static constexpr Version Unpack(ui32 packed) {
    return { (packed >> 22) & 0x7FU, (packed >> 12) & 0x3FFU, packed & 0xFFFU };
  }

  string ToStr() const {
    return std::format("v{}.{}.{}", major, minor, patch);
  }
};

// Bumped when on-disk or script-facing behavior changes.
inline constexpr Version ENGINE_VERSION { 0, 0, 2 };

}

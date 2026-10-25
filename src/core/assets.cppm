export module crumb.core.assets;
import crumb.core.types;

export namespace crumb {

// Read-only view over bytes embedded in the executable at build time.
struct Asset {
  const ui8* data = nullptr;
  usize      size = 0;

  bool Empty() const { return data == nullptr || size == 0; }
};

Asset FontPng();
Asset IconPng();
Asset LspStub();

} // namespace crumb

namespace crumb {

namespace {

// xmake's utils.bin2c rule generates these headers at build time. They contain
// only comma separated initializer lists, so they are included directly inside
// the arrays below. The bytes live in .rdata and never touch the filesystem.
const ui8 FONT_PNG[] = {
#include "font_16x16.png.h"
};

const ui8 ICON_PNG[] = {
#include "crumb-ico.png.h"
};

const ui8 LSP_STUB[] = {
#include "meta_crumb.lua.h"
};

} // namespace

Asset FontPng() {
  return { FONT_PNG, sizeof(FONT_PNG) };
}

Asset IconPng() {
  return { ICON_PNG, sizeof(ICON_PNG) };
}

Asset LspStub() {
  return { LSP_STUB, sizeof(LSP_STUB) };
}

} // namespace crumb

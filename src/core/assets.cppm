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

}

namespace crumb {

namespace {

// xmake's bin2c rule emits comma-separated initializer lists. The arrays live
// in .rdata and never touch the filesystem at runtime.
const ui8 FONT_PNG[] = {
#include "font_16x16.png.h"
};

const ui8 ICON_PNG[] = {
#include "crumb-ico.png.h"
};

const ui8 LSP_STUB[] = {
#include "meta_crumb.lua.h"
};

}

Asset FontPng() {
  return { FONT_PNG, sizeof(FONT_PNG) };
}

Asset IconPng() {
  return { ICON_PNG, sizeof(ICON_PNG) };
}

Asset LspStub() {
  return { LSP_STUB, sizeof(LSP_STUB) };
}

}

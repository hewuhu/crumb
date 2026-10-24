module crumb.assets;

import crumb.type;

namespace crumb::assets {

namespace {

// xmake's utils.bin2c rule generates these headers at build time. They contain
// only comma separated initializer lists, so they are included directly inside
// the arrays below. The bytes live in .rdata and never touch the filesystem.
const uchar FONT_PNG[] = {
#include "font_16x16.png.h"
};

const uchar ICON_PNG[] = {
#include "crumb-ico.png.h"
};

const uchar LSP_STUB[] = {
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

} // namespace crumb::assets

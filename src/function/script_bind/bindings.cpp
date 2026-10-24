module;
#include <lua/lua.hpp>
#include <string_view>

module crumb.bindings;

import crumb.type;
import crumb.lua;
import crumb.renderer;
import crumb.window;
import crumb.input;
import crumb.logger;

namespace {

using namespace crumb;

// Lua-facing tables. Convert all script values to engine types at this boundary.
// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

uchar ClampByte(double value) {
  if (value < 0.0) return 0;
  if (value > 255.0) return 255;
  return to_uchar(value);
}

uchar TableByte(lua_State* L, int table, const char* field, int index, uchar fallback) {
  lua_getfield(L, table, field);
  if (lua_isnumber(L, -1)) {
    const uchar value = ClampByte(lua_tonumber(L, -1));
    lua_pop(L, 1);
    return value;
  }
  lua_pop(L, 1);

  lua_rawgeti(L, table, index);
  if (lua_isnumber(L, -1)) {
    const uchar value = ClampByte(lua_tonumber(L, -1));
    lua_pop(L, 1);
    return value;
  }
  lua_pop(L, 1);
  return fallback;
}

// Colors are packed as 0xRRGGBBAA, or a table { r, g, b, a } / { r=, g=, b=, a= }.
Color ColorFromLua(lua_State* L, int index, Color fallback) {
  if (lua_isnoneornil(L, index))
    return fallback;

  if (lua_isnumber(L, index)) {
    const ui32 packed = to_ui32(lua_tointeger(L, index));
    Color color;
    color.r = to_uchar((packed >> 24) & 0xFF);
    color.g = to_uchar((packed >> 16) & 0xFF);
    color.b = to_uchar((packed >> 8) & 0xFF);
    color.a = to_uchar(packed & 0xFF);
    return color;
  }

  if (lua_istable(L, index)) {
    Color color;
    color.r = TableByte(L, index, "r", 1, 0);
    color.g = TableByte(L, index, "g", 2, 0);
    color.b = TableByte(L, index, "b", 3, 0);
    color.a = TableByte(L, index, "a", 4, 255);
    return color;
  }

  return fallback;
}

ui32 PackColor(uchar r, uchar g, uchar b, uchar a) {
  return (to_ui32(r) << 24)
       | (to_ui32(g) << 16)
       | (to_ui32(b) << 8)
       |  to_ui32(a);
}

void SetClosure(lua_State* L, int table, const char* name, lua_CFunction fn) {
  lua_pushcclosure(L, fn, 0);
  lua_setfield(L, table, name);
}

void SetClosure(lua_State* L, int table, const char* name, lua_CFunction fn, void* upvalue) {
  lua_pushlightuserdata(L, upvalue);
  lua_pushcclosure(L, fn, 1);
  lua_setfield(L, table, name);
}

void SetInt(lua_State* L, int table, const char* name, i32 value) {
  lua_pushinteger(L, static_cast<lua_Integer>(value));
  lua_setfield(L, table, name);
}

void SetColor(lua_State* L, int table, const char* name, Color color) {
  const ui32 packed = PackColor(color.r, color.g, color.b, color.a);
  lua_pushinteger(L, static_cast<lua_Integer>(packed));
  lua_setfield(L, table, name);
}

void SetKey(lua_State* L, int table, const char* name, string_view keyName) {
  SetInt(L, table, name, to_i32(input::KeyCode(keyName)));
}

Renderer* RendererUpvalue(lua_State* L) {
  return static_cast<Renderer*>(lua_touserdata(L, lua_upvalueindex(1)));
}

// ---------------------------------------------------------------------------
// gfx
// ---------------------------------------------------------------------------

int GfxClear(lua_State* L) {
  if (Renderer* renderer = RendererUpvalue(L)) {
    const Color color = ColorFromLua(L, 1, BLACK);
    renderer->Clear(color);
  }
  return 0;
}

int GfxText(lua_State* L) {
  Renderer* renderer = RendererUpvalue(L);
  if (!renderer)
    return 0;

  size_t length = 0;
  const char* text = luaL_checklstring(L, 1, &length);
  const float x = to_r32(luaL_checknumber(L, 2));
  const float y = to_r32(luaL_checknumber(L, 3));
  const Color color = ColorFromLua(L, 4, WHITE);
  const float scale = to_r32(luaL_optnumber(L, 5, 1.0));

  renderer->Text(string_view(text, length), x, y, color, scale);
  return 0;
}

int GfxRectFill(lua_State* L) {
  Renderer* renderer = RendererUpvalue(L);
  if (!renderer)
    return 0;

  const float x = to_r32(luaL_checknumber(L, 1));
  const float y = to_r32(luaL_checknumber(L, 2));
  const float w = to_r32(luaL_checknumber(L, 3));
  const float h = to_r32(luaL_checknumber(L, 4));
  const Color color = ColorFromLua(L, 5, WHITE);

  renderer->RectFill(x, y, w, h, color);
  return 0;
}

int GfxSpr(lua_State* L) {
  Renderer* renderer = RendererUpvalue(L);
  if (!renderer)
    return 0;

  const i32 index = to_i32(luaL_checkinteger(L, 1));
  const float x = to_r32(luaL_checknumber(L, 2));
  const float y = to_r32(luaL_checknumber(L, 3));

  i32 wCells = 1;
  i32 hCells = 1;
  bool flipX = false;
  bool flipY = false;
  Color tint = TRUE_WHITE;

  // PICO-8 form: spr(index, x, y, w, h, flip_x, flip_y, color)
  // Legacy form: spr(index, x, y, flip_x, flip_y, color)
  if (lua_isboolean(L, 4) || lua_isnoneornil(L, 4)) {
    flipX = lua_toboolean(L, 4) != 0;
    flipY = lua_toboolean(L, 5) != 0;
    tint = ColorFromLua(L, 6, TRUE_WHITE);
  } else {
    wCells = to_i32(luaL_optinteger(L, 4, 1));
    hCells = to_i32(luaL_optinteger(L, 5, 1));
    flipX = lua_toboolean(L, 6) != 0;
    flipY = lua_toboolean(L, 7) != 0;
    tint = ColorFromLua(L, 8, TRUE_WHITE);
  }

  renderer->Sprite(index, x, y, wCells, hCells, flipX, flipY, tint);
  return 0;
}

int GfxMeasureText(lua_State* L) {
  Renderer* renderer = RendererUpvalue(L);
  size_t length = 0;
  const char* text = luaL_checklstring(L, 1, &length);
  const float scale = to_r32(luaL_optnumber(L, 2, 1.0));

  const i32 width = renderer
    ? renderer->MeasureText(string_view(text, length), scale)
    : 0;
  lua_pushinteger(L, static_cast<lua_Integer>(width));
  return 1;
}

int GfxRgb(lua_State* L) {
  const uchar r = ClampByte(luaL_checknumber(L, 1));
  const uchar g = ClampByte(luaL_checknumber(L, 2));
  const uchar b = ClampByte(luaL_checknumber(L, 3));
  const uchar a = ClampByte(luaL_optnumber(L, 4, 255.0));

  lua_pushinteger(L, static_cast<lua_Integer>(PackColor(r, g, b, a)));
  return 1;
}

int GfxSetClearColor(lua_State* L) {
  if (Renderer* renderer = RendererUpvalue(L)) {
    renderer->SetClearColor(ColorFromLua(L, 1, BLACK));
  }
  return 0;
}

int GfxScreenToGameX(lua_State* L) {
  Renderer* renderer = RendererUpvalue(L);
  const float x = renderer
    ? renderer->ScreenToGameX(to_r32(luaL_checknumber(L, 1)))
    : 0.0f;
  lua_pushnumber(L, x);
  return 1;
}

int GfxScreenToGameY(lua_State* L) {
  Renderer* renderer = RendererUpvalue(L);
  const float y = renderer
    ? renderer->ScreenToGameY(to_r32(luaL_checknumber(L, 1)))
    : 0.0f;
  lua_pushnumber(L, y);
  return 1;
}

void BindGfx(lua_State* L, Renderer* renderer) {
  lua_newtable(L);
  const int table = lua_gettop(L);

  SetClosure(L, table, "cls",              GfxClear,           renderer);
  SetClosure(L, table, "text",             GfxText,            renderer);
  SetClosure(L, table, "rect_fill",        GfxRectFill,        renderer);
  SetClosure(L, table, "spr",              GfxSpr,             renderer);
  SetClosure(L, table, "measure_text",     GfxMeasureText,     renderer);
  SetClosure(L, table, "set_clear_color",  GfxSetClearColor,   renderer);
  SetClosure(L, table, "screen_to_game_x", GfxScreenToGameX,   renderer);
  SetClosure(L, table, "screen_to_game_y", GfxScreenToGameY,   renderer);
  SetClosure(L, table, "rgb",              GfxRgb);

  SetColor(L, table, "BLACK",       BLACK);
  SetColor(L, table, "DARK_BLUE",   DARK_BLUE);
  SetColor(L, table, "DARK_PURPLE", DARK_PURPLE);
  SetColor(L, table, "DARK_GREEN",  DARK_GREEN);
  SetColor(L, table, "BROWN",       BROWN);
  SetColor(L, table, "DARK_GRAY",   DARK_GRAY);
  SetColor(L, table, "LIGHT_GRAY",  LIGHT_GRAY);
  SetColor(L, table, "WHITE",       WHITE);
  SetColor(L, table, "RED",         RED);
  SetColor(L, table, "ORANGE",      ORANGE);
  SetColor(L, table, "YELLOW",      YELLOW);
  SetColor(L, table, "GREEN",       GREEN);
  SetColor(L, table, "BLUE",        BLUE);
  SetColor(L, table, "INDIGO",      INDIGO);
  SetColor(L, table, "PINK",        PINK);
  SetColor(L, table, "PEACH",       PEACH);
  SetColor(L, table, "TRUE_WHITE",  TRUE_WHITE);
  SetColor(L, table, "TRANSPARENT", TRANSPARENT);

  lua_setglobal(L, "gfx");
}

// ---------------------------------------------------------------------------
// input
// ---------------------------------------------------------------------------

int InputHeld(lua_State* L) {
  const i32 key = to_i32(luaL_checkinteger(L, 1));
  lua_pushboolean(L, input::KeyHeld(key));
  return 1;
}

int InputPressed(lua_State* L) {
  const i32 key = to_i32(luaL_checkinteger(L, 1));
  lua_pushboolean(L, input::KeyPressed(key));
  return 1;
}

int InputReleased(lua_State* L) {
  const i32 key = to_i32(luaL_checkinteger(L, 1));
  lua_pushboolean(L, input::KeyReleased(key));
  return 1;
}

int InputKeyCode(lua_State* L) {
  size_t length = 0;
  const char* name = luaL_checklstring(L, 1, &length);
  lua_pushinteger(L, static_cast<lua_Integer>(
    input::KeyCode(string_view(name, length))));
  return 1;
}

int InputMouse(lua_State* L) {
  lua_pushnumber(L, input::MouseX());
  lua_pushnumber(L, input::MouseY());
  return 2;
}

int InputMouseX(lua_State* L) {
  lua_pushnumber(L, input::MouseX());
  return 1;
}

int InputMouseY(lua_State* L) {
  lua_pushnumber(L, input::MouseY());
  return 1;
}

int InputMouseHeld(lua_State* L) {
  const i32 button = to_i32(luaL_checkinteger(L, 1));
  lua_pushboolean(L, input::MouseDown(button));
  return 1;
}

int InputMousePressed(lua_State* L) {
  const i32 button = to_i32(luaL_checkinteger(L, 1));
  lua_pushboolean(L, input::MousePressed(button));
  return 1;
}

int InputMouseReleased(lua_State* L) {
  const i32 button = to_i32(luaL_checkinteger(L, 1));
  lua_pushboolean(L, input::MouseReleased(button));
  return 1;
}

void BindInput(lua_State* L) {
  lua_newtable(L);
  const int table = lua_gettop(L);

  SetClosure(L, table, "held",     InputHeld);
  SetClosure(L, table, "pressed",  InputPressed);
  SetClosure(L, table, "released", InputReleased);
  SetClosure(L, table, "key_code",     InputKeyCode);

  SetClosure(L, table, "mouse",           InputMouse);
  SetClosure(L, table, "mouse_x",         InputMouseX);
  SetClosure(L, table, "mouse_y",         InputMouseY);
  SetClosure(L, table, "mouse_held",      InputMouseHeld);
  SetClosure(L, table, "mouse_pressed",   InputMousePressed);
  SetClosure(L, table, "mouse_released",  InputMouseReleased);

  SetKey(L, table, "LEFT",      "left");
  SetKey(L, table, "RIGHT",     "right");
  SetKey(L, table, "UP",        "up");
  SetKey(L, table, "DOWN",      "down");
  SetKey(L, table, "SPACE",     "space");
  SetKey(L, table, "ENTER",     "enter");
  SetKey(L, table, "ESCAPE",    "escape");
  SetKey(L, table, "TAB",       "tab");
  SetKey(L, table, "BACKSPACE", "backspace");
  SetKey(L, table, "SHIFT",     "shift");
  SetKey(L, table, "CTRL",      "ctrl");
  SetKey(L, table, "ALT",       "alt");

  // PICO-8-style action buttons mapped to the Z / X / C keys.
  SetKey(L, table, "BTN1", "z");
  SetKey(L, table, "BTN2", "x");
  SetKey(L, table, "BTN3", "c");

  SetInt(L, table, "MOUSE_LEFT",   0);
  SetInt(L, table, "MOUSE_RIGHT",  1);
  SetInt(L, table, "MOUSE_MIDDLE", 2);

  for (char c = 'a'; c <= 'z'; ++c) {
    char field[8] = { 'K', 'E', 'Y', '_', char(c - 'a' + 'A'), '\0' };
    char key[2]   = { c, '\0' };
    SetKey(L, table, field, string_view(key, 1));
  }

  for (char c = '0'; c <= '9'; ++c) {
    char field[8] = { 'K', 'E', 'Y', '_', c, '\0' };
    char key[2]   = { c, '\0' };
    SetKey(L, table, field, string_view(key, 1));
  }

  lua_setglobal(L, "input");
}

// ---------------------------------------------------------------------------
// crumb (engine / window)
// ---------------------------------------------------------------------------

int CrumbWidth(lua_State* L) {
  lua_pushinteger(L, static_cast<lua_Integer>(win::Width()));
  return 1;
}

int CrumbHeight(lua_State* L) {
  lua_pushinteger(L, static_cast<lua_Integer>(win::Height()));
  return 1;
}

int CrumbSetTitle(lua_State* L) {
  size_t length = 0;
  const char* title = luaL_checklstring(L, 1, &length);
  win::SetTitle(string_view(title, length));
  return 0;
}

int CrumbQuit(lua_State*) {
  win::Quit();
  return 0;
}

void BindCrumb(lua_State* L, Renderer* renderer) {
  lua_newtable(L);
  const int table = lua_gettop(L);

  SetClosure(L, table, "width",     CrumbWidth);
  SetClosure(L, table, "height",    CrumbHeight);
  SetClosure(L, table, "set_title", CrumbSetTitle);
  SetClosure(L, table, "quit",      CrumbQuit);
  SetClosure(L, table, "measure_text", GfxMeasureText, renderer);

  SetInt(L, table, "GAME_W",      renderer ? renderer->GameWidth()  : 0);
  SetInt(L, table, "GAME_H",      renderer ? renderer->GameHeight() : 0);
  SetInt(L, table, "SPRITE_SIZE", renderer ? renderer->SpriteSize() : 0);

  lua_setglobal(L, "crumb");
}

} // namespace

namespace crumb {

bool BindLuaApi(LuaVM& vm, const ScriptContext& context) {
  lua_State* L = vm.GetState();
  if (!L) {
    Logger::Error("bindings: LuaVM is not initialized");
    return false;
  }

  BindGfx(L, context.renderer);
  BindInput(L);
  BindCrumb(L, context.renderer);
  return true;
}

} // namespace crumb

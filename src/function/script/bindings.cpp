module;
#include <lua/lua.hpp>
#include <string_view>

module crumb.script.bindings;

import crumb.core.types;
import crumb.gfx.color;
import crumb.script.lua;
import crumb.gfx.renderer;
import crumb.platform.window;
import crumb.platform.input;
import crumb.core.log;

// Lua C functions. Every script value is converted to an engine type here; no
// engine pointer is exposed to Lua.
namespace {

using namespace crumb;

ui8 ClampByte(double value) {
  if (value < 0.0) return 0;
  if (value > 255.0) return 255;
  return to_ui8(value);
}

ui8 TableByte(lua_State* lua_state, int table, c_string field, int index, ui8 fallback) {
  lua_getfield(lua_state, table, field);
  if (lua_isnumber(lua_state, -1)) {
    const ui8 value = ClampByte(lua_tonumber(lua_state, -1));
    lua_pop(lua_state, 1);
    return value;
  }
  lua_pop(lua_state, 1);

  lua_rawgeti(lua_state, table, index);
  if (lua_isnumber(lua_state, -1)) {
    const ui8 value = ClampByte(lua_tonumber(lua_state, -1));
    lua_pop(lua_state, 1);
    return value;
  }
  lua_pop(lua_state, 1);
  return fallback;
}

// Colors are either 0xRRGGBBAA integers or tables { r, g, b, a } (named or
// positional).
Color ColorFromLua(lua_State* lua_state, int index, Color fallback) {
  if (lua_isnoneornil(lua_state, index))
    return fallback;

  if (lua_isnumber(lua_state, index)) {
    const ui32 packed = to_ui32(lua_tointeger(lua_state, index));
    Color color;
    color.r = to_ui8((packed >> 24) & 0xFF);
    color.g = to_ui8((packed >> 16) & 0xFF);
    color.b = to_ui8((packed >> 8) & 0xFF);
    color.a = to_ui8(packed & 0xFF);
    return color;
  }

  if (lua_istable(lua_state, index)) {
    Color color;
    color.r = TableByte(lua_state, index, "r", 1, 0);
    color.g = TableByte(lua_state, index, "g", 2, 0);
    color.b = TableByte(lua_state, index, "b", 3, 0);
    color.a = TableByte(lua_state, index, "a", 4, 255);
    return color;
  }

  return fallback;
}

// Pack for the Lua-facing 0xRRGGBBAA convention.
ui32 PackColor(ui8 r, ui8 g, ui8 b, ui8 a) {
  return (to_ui32(r) << 24)
       | (to_ui32(g) << 16)
       | (to_ui32(b) << 8)
       |  to_ui32(a);
}

void SetClosure(lua_State* lua_state, int table, c_string name, lua_CFunction fn) {
  lua_pushcclosure(lua_state, fn, 0);
  lua_setfield(lua_state, table, name);
}

void SetClosure(lua_State* lua_state, int table, c_string name, lua_CFunction fn,
                void* upvalue) {
  lua_pushlightuserdata(lua_state, upvalue);
  lua_pushcclosure(lua_state, fn, 1);
  lua_setfield(lua_state, table, name);
}

void SetInt(lua_State* lua_state, int table, c_string name, i32 value) {
  lua_pushinteger(lua_state, static_cast<lua_Integer>(value));
  lua_setfield(lua_state, table, name);
}

void SetColor(lua_State* lua_state, int table, c_string name, Color color) {
  const ui32 packed = PackColor(color.r, color.g, color.b, color.a);
  lua_pushinteger(lua_state, static_cast<lua_Integer>(packed));
  lua_setfield(lua_state, table, name);
}

void SetKey(lua_State* lua_state, int table, c_string name, string_view key_name) {
  SetInt(lua_state, table, name, to_i32(input::KeyCode(key_name)));
}

Renderer* RendererUpvalue(lua_State* lua_state) {
  return static_cast<Renderer*>(lua_touserdata(lua_state, lua_upvalueindex(1)));
}

input::InputState* InputUpvalue(lua_State* lua_state) {
  return static_cast<input::InputState*>(
    lua_touserdata(lua_state, lua_upvalueindex(1)));
}

int GfxClear(lua_State* lua_state) {
  if (Renderer* renderer = RendererUpvalue(lua_state)) {
    const Color color = ColorFromLua(lua_state, 1, BLACK);
    renderer->Clear(color);
  }
  return 0;
}

int GfxText(lua_State* lua_state) {
  Renderer* renderer = RendererUpvalue(lua_state);
  if (!renderer)
    return 0;

  usize length = 0;
  c_string text = luaL_checklstring(lua_state, 1, &length);
  const float x = to_float(luaL_checknumber(lua_state, 2));
  const float y = to_float(luaL_checknumber(lua_state, 3));
  const Color color = ColorFromLua(lua_state, 4, WHITE);
  const float scale = to_float(luaL_optnumber(lua_state, 5, 1.0));

  renderer->Text(string_view(text, length), x, y, color, scale);
  return 0;
}

int GfxRectFill(lua_State* lua_state) {
  Renderer* renderer = RendererUpvalue(lua_state);
  if (!renderer)
    return 0;

  const float x = to_float(luaL_checknumber(lua_state, 1));
  const float y = to_float(luaL_checknumber(lua_state, 2));
  const float w = to_float(luaL_checknumber(lua_state, 3));
  const float h = to_float(luaL_checknumber(lua_state, 4));
  const Color color = ColorFromLua(lua_state, 5, WHITE);

  renderer->RectFill(x, y, w, h, color);
  return 0;
}

int GfxSpr(lua_State* lua_state) {
  Renderer* renderer = RendererUpvalue(lua_state);
  if (!renderer)
    return 0;

  const i32 index = to_i32(luaL_checkinteger(lua_state, 1));
  const float x = to_float(luaL_checknumber(lua_state, 2));
  const float y = to_float(luaL_checknumber(lua_state, 3));

  i32 w_cells = 1;
  i32 h_cells = 1;
  bool flip_x = false;
  bool flip_y = false;
  Color tint = TRUE_WHITE;

  // PICO-8 form: spr(index, x, y, w, h, flip_x, flip_y, color).
  // Legacy form: spr(index, x, y, flip_x, flip_y, color).
  if (lua_isboolean(lua_state, 4) || lua_isnoneornil(lua_state, 4)) {
    flip_x = lua_toboolean(lua_state, 4) != 0;
    flip_y = lua_toboolean(lua_state, 5) != 0;
    tint = ColorFromLua(lua_state, 6, TRUE_WHITE);
  } else {
    w_cells = to_i32(luaL_optinteger(lua_state, 4, 1));
    h_cells = to_i32(luaL_optinteger(lua_state, 5, 1));
    flip_x = lua_toboolean(lua_state, 6) != 0;
    flip_y = lua_toboolean(lua_state, 7) != 0;
    tint = ColorFromLua(lua_state, 8, TRUE_WHITE);
  }

  renderer->Sprite(index, x, y, w_cells, h_cells, flip_x, flip_y, tint);
  return 0;
}

int GfxMeasureText(lua_State* lua_state) {
  Renderer* renderer = RendererUpvalue(lua_state);
  usize length = 0;
  c_string text = luaL_checklstring(lua_state, 1, &length);
  const float scale = to_float(luaL_optnumber(lua_state, 2, 1.0));

  const i32 width = renderer
    ? renderer->MeasureText(string_view(text, length), scale)
    : 0;
  lua_pushinteger(lua_state, static_cast<lua_Integer>(width));
  return 1;
}

// Build the packed 0xRRGGBBAA integer used by the color arguments.
int GfxRgb(lua_State* lua_state) {
  const ui8 r = ClampByte(luaL_checknumber(lua_state, 1));
  const ui8 g = ClampByte(luaL_checknumber(lua_state, 2));
  const ui8 b = ClampByte(luaL_checknumber(lua_state, 3));
  const ui8 a = ClampByte(luaL_optnumber(lua_state, 4, 255.0));

  lua_pushinteger(lua_state, static_cast<lua_Integer>(PackColor(r, g, b, a)));
  return 1;
}

int GfxSetClearColor(lua_State* lua_state) {
  if (Renderer* renderer = RendererUpvalue(lua_state)) {
    renderer->SetClearColor(ColorFromLua(lua_state, 1, BLACK));
  }
  return 0;
}

int GfxScreenToGameX(lua_State* lua_state) {
  Renderer* renderer = RendererUpvalue(lua_state);
  const float x = renderer
    ? renderer->ScreenToGameX(to_float(luaL_checknumber(lua_state, 1)))
    : 0.0f;
  lua_pushnumber(lua_state, x);
  return 1;
}

int GfxScreenToGameY(lua_State* lua_state) {
  Renderer* renderer = RendererUpvalue(lua_state);
  const float y = renderer
    ? renderer->ScreenToGameY(to_float(luaL_checknumber(lua_state, 1)))
    : 0.0f;
  lua_pushnumber(lua_state, y);
  return 1;
}

// The palette mirrors crumb.gfx.color. Each closure captures the Renderer.
void BindGfx(lua_State* lua_state, Renderer* renderer) {
  lua_newtable(lua_state);
  const int table = lua_gettop(lua_state);

  SetClosure(lua_state, table, "cls",              GfxClear,           renderer);
  SetClosure(lua_state, table, "text",             GfxText,            renderer);
  SetClosure(lua_state, table, "rect_fill",        GfxRectFill,        renderer);
  SetClosure(lua_state, table, "spr",              GfxSpr,             renderer);
  SetClosure(lua_state, table, "measure_text",     GfxMeasureText,     renderer);
  SetClosure(lua_state, table, "set_clear_color",  GfxSetClearColor,   renderer);
  SetClosure(lua_state, table, "screen_to_game_x", GfxScreenToGameX,   renderer);
  SetClosure(lua_state, table, "screen_to_game_y", GfxScreenToGameY,   renderer);
  SetClosure(lua_state, table, "rgb",              GfxRgb);

  SetColor(lua_state, table, "BLACK",       BLACK);
  SetColor(lua_state, table, "DARK_BLUE",   DARK_BLUE);
  SetColor(lua_state, table, "DARK_PURPLE", DARK_PURPLE);
  SetColor(lua_state, table, "DARK_GREEN",  DARK_GREEN);
  SetColor(lua_state, table, "BROWN",       BROWN);
  SetColor(lua_state, table, "DARK_GRAY",   DARK_GRAY);
  SetColor(lua_state, table, "LIGHT_GRAY",  LIGHT_GRAY);
  SetColor(lua_state, table, "WHITE",       WHITE);
  SetColor(lua_state, table, "RED",         RED);
  SetColor(lua_state, table, "ORANGE",      ORANGE);
  SetColor(lua_state, table, "YELLOW",      YELLOW);
  SetColor(lua_state, table, "GREEN",       GREEN);
  SetColor(lua_state, table, "BLUE",        BLUE);
  SetColor(lua_state, table, "INDIGO",      INDIGO);
  SetColor(lua_state, table, "PINK",        PINK);
  SetColor(lua_state, table, "PEACH",       PEACH);
  SetColor(lua_state, table, "TRUE_WHITE",  TRUE_WHITE);
  SetColor(lua_state, table, "TRANSPARENT", TRANSPARENT);

  lua_setglobal(lua_state, "gfx");
}

int InputHeld(lua_State* lua_state) {
  input::InputState* state = InputUpvalue(lua_state);
  if (state == nullptr) {
    lua_pushboolean(lua_state, 0);
    return 1;
  }
  const i32 key = to_i32(luaL_checkinteger(lua_state, 1));
  lua_pushboolean(lua_state, input::KeyHeld(*state, key));
  return 1;
}

int InputPressed(lua_State* lua_state) {
  input::InputState* state = InputUpvalue(lua_state);
  if (state == nullptr) {
    lua_pushboolean(lua_state, 0);
    return 1;
  }
  const i32 key = to_i32(luaL_checkinteger(lua_state, 1));
  lua_pushboolean(lua_state, input::KeyPressed(*state, key));
  return 1;
}

int InputReleased(lua_State* lua_state) {
  input::InputState* state = InputUpvalue(lua_state);
  if (state == nullptr) {
    lua_pushboolean(lua_state, 0);
    return 1;
  }
  const i32 key = to_i32(luaL_checkinteger(lua_state, 1));
  lua_pushboolean(lua_state, input::KeyReleased(*state, key));
  return 1;
}

int InputKeyCode(lua_State* lua_state) {
  usize length = 0;
  c_string name = luaL_checklstring(lua_state, 1, &length);
  lua_pushinteger(lua_state, static_cast<lua_Integer>(
    input::KeyCode(string_view(name, length))));
  return 1;
}

int InputMouse(lua_State* lua_state) {
  input::InputState* state = InputUpvalue(lua_state);
  if (state == nullptr) {
    lua_pushnumber(lua_state, 0.0);
    lua_pushnumber(lua_state, 0.0);
    return 2;
  }
  lua_pushnumber(lua_state, input::MouseX(*state));
  lua_pushnumber(lua_state, input::MouseY(*state));
  return 2;
}

int InputMouseX(lua_State* lua_state) {
  input::InputState* state = InputUpvalue(lua_state);
  lua_pushnumber(lua_state, state ? input::MouseX(*state) : 0.0);
  return 1;
}

int InputMouseY(lua_State* lua_state) {
  input::InputState* state = InputUpvalue(lua_state);
  lua_pushnumber(lua_state, state ? input::MouseY(*state) : 0.0);
  return 1;
}

int InputMouseHeld(lua_State* lua_state) {
  input::InputState* state = InputUpvalue(lua_state);
  if (state == nullptr) {
    lua_pushboolean(lua_state, 0);
    return 1;
  }
  const i32 button = to_i32(luaL_checkinteger(lua_state, 1));
  lua_pushboolean(lua_state, input::MouseDown(*state, button));
  return 1;
}

int InputMousePressed(lua_State* lua_state) {
  input::InputState* state = InputUpvalue(lua_state);
  if (state == nullptr) {
    lua_pushboolean(lua_state, 0);
    return 1;
  }
  const i32 button = to_i32(luaL_checkinteger(lua_state, 1));
  lua_pushboolean(lua_state, input::MousePressed(*state, button));
  return 1;
}

int InputMouseReleased(lua_State* lua_state) {
  input::InputState* state = InputUpvalue(lua_state);
  if (state == nullptr) {
    lua_pushboolean(lua_state, 0);
    return 1;
  }
  const i32 button = to_i32(luaL_checkinteger(lua_state, 1));
  lua_pushboolean(lua_state, input::MouseReleased(*state, button));
  return 1;
}

void BindInput(lua_State* lua_state, input::InputState* input_state) {
  lua_newtable(lua_state);
  const int table = lua_gettop(lua_state);

  SetClosure(lua_state, table, "held",     InputHeld,     input_state);
  SetClosure(lua_state, table, "pressed",  InputPressed,  input_state);
  SetClosure(lua_state, table, "released", InputReleased, input_state);
  SetClosure(lua_state, table, "key_code",     InputKeyCode);

  SetClosure(lua_state, table, "mouse",           InputMouse,          input_state);
  SetClosure(lua_state, table, "mouse_x",         InputMouseX,         input_state);
  SetClosure(lua_state, table, "mouse_y",         InputMouseY,         input_state);
  SetClosure(lua_state, table, "mouse_held",      InputMouseHeld,      input_state);
  SetClosure(lua_state, table, "mouse_pressed",   InputMousePressed,   input_state);
  SetClosure(lua_state, table, "mouse_released",  InputMouseReleased,  input_state);

  SetKey(lua_state, table, "LEFT",      "left");
  SetKey(lua_state, table, "RIGHT",     "right");
  SetKey(lua_state, table, "UP",        "up");
  SetKey(lua_state, table, "DOWN",      "down");
  SetKey(lua_state, table, "SPACE",     "space");
  SetKey(lua_state, table, "ENTER",     "enter");
  SetKey(lua_state, table, "ESCAPE",    "escape");
  SetKey(lua_state, table, "TAB",       "tab");
  SetKey(lua_state, table, "BACKSPACE", "backspace");
  SetKey(lua_state, table, "SHIFT",     "shift");
  SetKey(lua_state, table, "CTRL",      "ctrl");
  SetKey(lua_state, table, "ALT",       "alt");

  // PICO-8-style action buttons.
  SetKey(lua_state, table, "BTN1", "z");
  SetKey(lua_state, table, "BTN2", "x");
  SetKey(lua_state, table, "BTN3", "c");

  SetInt(lua_state, table, "MOUSE_LEFT",   0);
  SetInt(lua_state, table, "MOUSE_RIGHT",  1);
  SetInt(lua_state, table, "MOUSE_MIDDLE", 2);

  // Generate KEY_A..KEY_Z and KEY_0..KEY_9 from the same name map.
  for (char character = 'a'; character <= 'z'; ++character) {
    char field[8] = { 'K', 'E', 'Y', '_', char(character - 'a' + 'A'), '\0' };
    char key[2]   = { character, '\0' };
    SetKey(lua_state, table, field, string_view(key, 1));
  }

  for (char character = '0'; character <= '9'; ++character) {
    char field[8] = { 'K', 'E', 'Y', '_', character, '\0' };
    char key[2]   = { character, '\0' };
    SetKey(lua_state, table, field, string_view(key, 1));
  }

  lua_setglobal(lua_state, "input");
}

int CrumbWidth(lua_State* lua_state) {
  lua_pushinteger(lua_state, static_cast<lua_Integer>(win::Width()));
  return 1;
}

int CrumbHeight(lua_State* lua_state) {
  lua_pushinteger(lua_state, static_cast<lua_Integer>(win::Height()));
  return 1;
}

int CrumbSetTitle(lua_State* lua_state) {
  usize length = 0;
  c_string title = luaL_checklstring(lua_state, 1, &length);
  win::SetTitle(string_view(title, length));
  return 0;
}

int CrumbQuit(lua_State*) {
  win::Quit();
  return 0;
}

void BindCrumb(lua_State* lua_state, Renderer* renderer) {
  lua_newtable(lua_state);
  const int table = lua_gettop(lua_state);

  SetClosure(lua_state, table, "width",     CrumbWidth);
  SetClosure(lua_state, table, "height",    CrumbHeight);
  SetClosure(lua_state, table, "set_title", CrumbSetTitle);
  SetClosure(lua_state, table, "quit",      CrumbQuit);
  SetClosure(lua_state, table, "measure_text", GfxMeasureText, renderer);

  SetInt(lua_state, table, "GAME_W",      renderer ? renderer->GameWidth()  : 0);
  SetInt(lua_state, table, "GAME_H",      renderer ? renderer->GameHeight() : 0);
  SetInt(lua_state, table, "SPRITE_SIZE", renderer ? renderer->SpriteSize() : 0);

  lua_setglobal(lua_state, "crumb");
}

}

namespace crumb {

// Bind all three tables on an initialized VM.
bool BindLuaApi(LuaVM& vm, const ScriptContext& context) {
  lua_State* lua_state = vm.GetState();
  if (!lua_state) {
    Logger::Error("bindings: LuaVM is not initialized");
    return false;
  }

  BindGfx(lua_state, context.renderer);
  BindInput(lua_state, context.input);
  BindCrumb(lua_state, context.renderer);
  return true;
}

}

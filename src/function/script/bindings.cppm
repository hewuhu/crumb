export module crumb.script.bindings;

import crumb.core.types;
import crumb.platform.input;
import crumb.script.lua;
import crumb.gfx.renderer;

export namespace crumb {

// Pointers captured by Lua closures. Both objects must outlive the LuaVM.
struct ScriptContext {
  Renderer* renderer = nullptr;
  input::InputState* input = nullptr;
};

// Create the gfx / input / crumb globals. Call after the window and renderer exist.
bool BindLuaApi(LuaVM& vm, const ScriptContext& context);

}

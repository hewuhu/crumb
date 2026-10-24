export module crumb.bindings;

import crumb.type;
import crumb.lua;
import crumb.renderer;

export namespace crumb {

struct ScriptContext {
  // Must outlive the LuaVM; the gfx closures keep this pointer as an upvalue.
  Renderer* renderer = nullptr;
};

// Registers the Lua-facing tables (gfx / input / crumb) on the VM's globals.
// Call it after LuaVM::Init() and Renderer::Init().
bool BindLuaApi(LuaVM& vm, const ScriptContext& context);

} // namespace crumb

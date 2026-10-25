export module crumb.script.bindings;

import crumb.core.types;
import crumb.platform.input;
import crumb.script.lua;
import crumb.gfx.renderer;

export namespace crumb {

struct ScriptContext {
  // Both must outlive the LuaVM: closures keep these pointers as upvalues.
  Renderer* renderer = nullptr;
  input::InputState* input = nullptr;
};

// Registers the Lua-facing tables (gfx / input / crumb) on the VM's globals.
// Call it after LuaVM::Init(), Renderer::Init() and window creation.
bool BindLuaApi(LuaVM& vm, const ScriptContext& context);

} // namespace crumb

module;
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>

#include <lua/lua.hpp>
export module crumb.script.lua;
import crumb.core.types;
import crumb.core.log;

export namespace crumb {

// Small Lua 5.5 wrapper. One instance owns one lua_State.
class LuaVM {
private:
  lua_State* m_state { nullptr };

  // Restores the stack top when it goes out of scope.
  class StackGuard {
  public:
    explicit StackGuard(lua_State* lua_state)
      : m_state(lua_state), m_base(lua_gettop(lua_state)) {}

    ~StackGuard() { lua_settop(m_state, m_base); }

    StackGuard(const StackGuard&) = delete;
    StackGuard& operator=(const StackGuard&) = delete;

  private:
    lua_State* m_state;
    int m_base;
  };

  static int Panic(lua_State* lua_state) {
    c_string message = lua_tostring(lua_state, -1);
    Logger::Error("lua panic: {}", message ? message : "(no message)");
    std::fflush(stdout);
    return 0;
  }

  static int LoadLib(lua_State* lua_state) {
    c_string name = lua_tostring(lua_state, 1);
    auto fn = reinterpret_cast<lua_CFunction>(lua_touserdata(lua_state, 2));
    luaL_requiref(lua_state, name, fn, 1);
    lua_pop(lua_state, 1);
    return 0;
  }

  static int Traceback(lua_State* lua_state) {
    c_string message = lua_tostring(lua_state, 1);
    if (message) luaL_traceback(lua_state, lua_state, message, 1);
    else         lua_pushliteral(lua_state, "(non-string error)");
    return 1;
  }

  void SetError(string* error) {
    c_string message = lua_tostring(m_state, -1);
    if (error) *error = message ? message : "(unknown error)";
    else if (message) Logger::Error("lua: {}", message);
  }

public:
  LuaVM(const LuaVM&) = delete;
  LuaVM& operator=(const LuaVM&) = delete;
  LuaVM(LuaVM&&) = delete;
  LuaVM& operator=(LuaVM&&) = delete;

  LuaVM() = default;

  ~LuaVM() {
    if (m_state) {
      lua_close(m_state);
      m_state = nullptr;
    }
  }

  // Create the state and open the Lua libraries used by games.
  bool Init() {
    if (m_state) return true;

    lua_State* lua_state = luaL_newstate();
    if (!lua_state) {
      Logger::Error("luaL_newstate failed");
      return false;
    }

    lua_atpanic(lua_state, Panic);

    auto open = [lua_state](c_string name, lua_CFunction fn) {
      lua_pushcfunction(lua_state, LoadLib);
      lua_pushstring(lua_state, name);
      lua_pushlightuserdata(lua_state, reinterpret_cast<void*>(fn));
      if (lua_pcall(lua_state, 2, 0, 0) != LUA_OK) {
        c_string message = lua_tostring(lua_state, -1);
        Logger::Error("open {} failed: {}", name, message ? message : "(non-string)");
        return false;
      }
      return true;
    };

    if (!open(LUA_GNAME,       luaopen_base)      ||
        !open(LUA_STRLIBNAME,  luaopen_string)    ||
        !open(LUA_TABLIBNAME,  luaopen_table)     ||
        !open(LUA_MATHLIBNAME, luaopen_math)      ||
        !open(LUA_COLIBNAME,   luaopen_coroutine) ||
        !open(LUA_UTF8LIBNAME, luaopen_utf8))
    {
      lua_close(lua_state);
      return false;
    }

    m_state = lua_state;
    return true;
  }

  // Execute a string chunk.
  bool DoString(string_view code, string_view chunk = "=(crumb)",
                string* error = nullptr) {
    StackGuard guard(m_state);
    lua_pushcfunction(m_state, Traceback);
    const int message_handler = lua_gettop(m_state);

    const string chunk_name(chunk);
    if (luaL_loadbufferx(m_state, code.data(), code.size(),
                         to_c_string(chunk_name), "t") != LUA_OK ||
        lua_pcall(m_state, 0, 0, message_handler) != LUA_OK)
    {
      SetError(error);
      return false;
    }

    return true;
  }

  // Load and execute a .lua file (UTF-8 BOM is ignored).
  bool DoFile(string_view path, string* error = nullptr) {
    std::ifstream file{ string(path), std::ios::binary };

    if (!file) {
      if (error) *error = "cannot open " + string(path);
      return false;
    }

    string src{
      std::istreambuf_iterator<char>(file),
      std::istreambuf_iterator<char>()
    };

    // Accept UTF-8 BOM files created by Windows editors.
    if (src.size() >= 3 &&
        to_ui8(src[0]) == 0xEF &&
        to_ui8(src[1]) == 0xBB &&
        to_ui8(src[2]) == 0xBF) {
      src.erase(0, 3);
    }

    return DoString(src, "@" + string(path), error);
  }

  // Call a global function with numeric arguments.
  bool Call(string_view name, std::initializer_list<double> args = {},
            string* error = nullptr) {
    StackGuard guard(m_state);

    lua_pushcfunction(m_state, Traceback);
    const int message_handler = lua_gettop(m_state);

    const string function_name(name);
    lua_getglobal(m_state, to_c_string(function_name));
    if (!lua_isfunction(m_state, -1)) {
      if (error) *error = "not a function: " + function_name;
      return false;
    }

    for (double argument : args)
      lua_pushnumber(m_state, argument);

    if (lua_pcall(m_state, to_i32(args.size()), 0, message_handler) != LUA_OK) {
      SetError(error);
      return false;
    }

    return true;
  }

  // True when a global function with this name exists.
  bool HasFunction(string_view name) {
    if (!m_state) return false;

    StackGuard guard(m_state);
    const string function_name(name);
    lua_getglobal(m_state, to_c_string(function_name));
    return lua_isfunction(m_state, -1);
  }

  // Bind a C function as a Lua global.
  void Register(string_view name, lua_CFunction fn) {
    const string function_name(name);
    lua_pushcfunction(m_state, fn);
    lua_setglobal(m_state, to_c_string(function_name));
  }

  inline lua_State* GetState() noexcept { return m_state; }
};

} // namespace crumb

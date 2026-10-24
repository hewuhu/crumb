module;
#include <cstdio>
#include <fstream>
#include <initializer_list>

#include <lua/lua.hpp>
export module crumb.lua;
import crumb.type;
import crumb.logger;



export namespace crumb {
// Small Lua 5.5 wrapper. One instance owns one lua_State.
class LuaVM {
private:
  lua_State* m_state { nullptr };

  struct StackGuard {
    lua_State* L;
    int base;
    explicit StackGuard(lua_State* l) : L(l), base(lua_gettop(l)) {}
    ~StackGuard() { lua_settop(L, base); }
    StackGuard(const StackGuard&) = delete;
    StackGuard& operator=(const StackGuard&) = delete;
  };

  static int Panic(lua_State* L) {
    c_string message = lua_tostring(L, -1);
    Logger::Error("lua panic: {}", message ? message : "(no message)");
    std::fflush(stdout);
    return 0;
  }

  static int LoadLib(lua_State* L) {
    c_string name = lua_tostring(L, 1);
    auto fn = reinterpret_cast<lua_CFunction>(lua_touserdata(L, 2));
    luaL_requiref(L, name, fn, 1);
    lua_pop(L, 1);
    return 0;
  }

  static int Traceback(lua_State* L) {
    c_string msg = lua_tostring(L, 1);
    if (msg) luaL_traceback(L, L, msg, 1);
    else     lua_pushliteral(L, "(non-string error)");
    return 1;
  }

  void SetError(string* error) {
    c_string msg = lua_tostring(m_state, -1);
    if (error) *error = msg ? msg : "(unknown error)";
    else if (msg) Logger::Error("lua: {}", msg);
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

    lua_State* L = luaL_newstate();
    if (!L) {
        Logger::Error("luaL_newstate failed");
        return false;
    }

    lua_atpanic(L, Panic);

    auto open = [L](c_string name, lua_CFunction fn) {
      lua_pushcfunction(L, LoadLib);
      lua_pushstring(L, name);
      lua_pushlightuserdata(L, reinterpret_cast<void*>(fn));
      if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        c_string msg = lua_tostring(L, -1);
        Logger::Error("open {} failed: {}", name, msg ? msg : "(non-string)");
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
        lua_close(L);
        return false;
    }

    m_state = L;
    return true;
  }

  // Execute a string chunk.
  bool DoString(string_view code, string_view chunk = "=(crumb)", string* error = nullptr) {
    StackGuard guard(m_state);
    lua_pushcfunction(m_state, Traceback);
    const int msgh = lua_gettop(m_state);

    const string chunk_name(chunk);
    if (luaL_loadbufferx(m_state, code.data(), code.size(),
                         c_str(chunk_name), "t") != LUA_OK ||
                         lua_pcall(m_state, 0, 0, msgh) != LUA_OK)
    {
      SetError(error);
      return false;
    }

    return true;
  }

  // Load and execute a .lua file (UTF-8 BOM is ignored).
  bool DoFile(string_view path, string* error = nullptr) {
    std::ifstream file{string(path), std::ios::binary};
    
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
        to_uchar(src[0]) == 0xEF &&
        to_uchar(src[1]) == 0xBB &&
        to_uchar(src[2]) == 0xBF) {
      src.erase(0, 3);
    }

    return DoString(src, "@" + string(path), error);
  }

  // Call a global function with numeric arguments.
  bool Call(string_view name, std::initializer_list<double> args = {}, string* error = nullptr) {
    StackGuard guard(m_state);

    lua_pushcfunction(m_state, Traceback);
    const int msgh = lua_gettop(m_state);

    const string fn_name(name);
    lua_getglobal(m_state, c_str(fn_name));
    if (!lua_isfunction(m_state, -1)) {
      if (error) *error = "not a function: " + fn_name;
      return false;
    }

    for (double a : args) lua_pushnumber(m_state, a);

    if (lua_pcall(m_state, to_i32( args.size() ), 0, msgh) != LUA_OK) {
      SetError(error);
      return false;
    }

    return true;
  }

  // True when a global function with this name exists.
  bool HasFunction(string_view name) {
    if (!m_state) return false;

    StackGuard guard(m_state);
    const string fn_name(name);
    lua_getglobal(m_state, c_str(fn_name));
    return lua_isfunction(m_state, -1);
  }

  // Bind a C function as a Lua global.
  void Register(string_view name, lua_CFunction fn) {
    const string n(name);
    lua_pushcfunction(m_state, fn);
    lua_setglobal(m_state, c_str(n));
  }

  inline lua_State* GetState() noexcept { return m_state; }
};


} // namespace crumb

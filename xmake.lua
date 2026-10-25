add_rules("mode.debug", "mode.release")

set_languages("c++23")
set_policy("build.c++.modules", true)

set_policy("build.c++.modules.reuse", false)
set_policy("build.c++.modules.std", false)

add_defines("SOKOL_GLCORE")

local assets = {
  "src/assets/font_16x16.png",
  "src/assets/crumb-ico.png",
  "src/assets/meta_crumb.lua",
}

-- 把嵌入资源生成成 C 头文件，供 core.assets 直接 include。
rule("crumb.assets")
  on_load(function (target)
    -- xmake 的模块缓存不区分模式，切换 debug/release 时清掉。
    import("core.cache.localcache")
    localcache.clear("cxxmodules")

    local dir = path.join(target:autogendir(), "rules", "utils", "bin2c")
    os.mkdir(dir)
    target:add("includedirs", dir)

    import("utils.binary.bin2c")
    for _, asset in ipairs(assets) do
      local header = path.join(dir, path.filename(asset) .. ".h")
      if not os.isfile(header) or os.mtime(asset) > os.mtime(header) then
        bin2c.main(asset, header, { nozeroend = true })
      end
    end
  end)

-- Lua 5.5.1：只编译引擎实际用到的核心和 base/coroutine/math/string/table/utf8。
-- 不编译 lua.c / luac.c / linit.c / loadlib.c / iolib.c / oslib.c / dblib.c。
target("lua")
  set_kind("static")
  add_files("external/lua/*.c")
  remove_files(
    "external/lua/lua.c",
    "external/lua/luac.c",
    "external/lua/linit.c",
    "external/lua/loadlib.c",
    "external/lua/liolib.c",
    "external/lua/loslib.c",
    "external/lua/ldblib.c")
  add_includedirs("external/lua")
  set_languages("c99")
  -- MSVC 默认可能把 .c 当 C++ 编译；强制 C，保证符号是 C 链接。
  add_cflags("/TC", { force = true })
  add_cxxflags("/TC", { force = true })

target("crumb_editor")
  set_kind("binary")
  add_rules("crumb.assets")
  add_files("src/**.cpp", "src/**.cppm")
  remove_files("src/template_entry.cpp")
  add_defines("SOKOL_NO_ENTRY")

  add_includedirs("external")
  add_deps("lua")
  add_syslinks("shell32")
  set_encodings("utf-8")

  if is_mode("release") then
    set_optimize("smallest")
    set_strip("all")
  end

target("crumb_template_win64")
  set_kind("binary")
  add_rules("crumb.assets")
  add_files("src/**.cpp", "src/**.cppm")
  remove_files("src/main.cpp")
  remove_files("src/editor/**")
  add_defines("CRUMB_TEMPLATE")

  add_includedirs("external")
  add_deps("lua")
  add_syslinks("shell32")
  add_ldflags("/SUBSYSTEM:WINDOWS", { force = true })
  set_encodings("utf-8")

  if is_mode("release") then
    set_optimize("smallest")
    set_strip("all")
  end

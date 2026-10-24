add_rules("mode.debug", "mode.release")

set_languages("cxxlatest")
set_policy("build.c++.modules", true)

local crumb_assets = {
  "src/assets/font_16x16.png",
  "src/assets/crumb-ico.png",
  "src/assets/meta_crumb.lua",
}

-- Generate the C array headers during target load, before any module scan or
-- compilation starts. This avoids a build-order race between the generated
-- headers and crumb.assets.cpp, which includes them directly.
rule("crumb.assets")
  on_load(function (target)
    local headerdir = path.join(target:autogendir(), "rules", "utils", "bin2c")
    os.mkdir(headerdir)
    target:add("includedirs", headerdir)

    import("utils.binary.bin2c")
    for _, asset in ipairs(crumb_assets) do
      local header = path.join(headerdir, path.filename(asset) .. ".h")
      if not os.isfile(header) or os.mtime(asset) > os.mtime(header) then
        bin2c.main(asset, header, { nozeroend = true })
      end
    end
  end)

target("crumb")
  set_kind("binary")

  add_rules("crumb.assets")
  add_files('src/**.cpp', "src/**.cppm")

  add_includedirs("external")

  local third_dir = "C:/third_party/"
  add_linkdirs(third_dir .. "lua-5.5.1/src")
  add_links("lua")

  set_encodings("utf-8")

  if is_mode("release") then
    set_optimize("smallest")
    set_strip("all")
  end

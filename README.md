# Crumb Engine

Crumb 是一个轻量的 2D 游戏引擎：C++23 模块 + sokol + Lua 5.5，目标是占用小、内存小、零外部运行时依赖，并把游戏导出成单文件或“exe + pak”。

当前版本 `0.0.2`，仍处于早期开发阶段，接口可能变化。当前可构建平台是 **Windows x64**；跨平台是目标，不是现状。

## 特性

- 固定逻辑分辨率 + 整数倍初始窗口，窗口变化时自动 letterbox。
- 固定步长 `_fixed_update` 与可变步长 `_update` / `_draw`。
- 立即模式 2D 渲染：纯色矩形、16x16 位图字体、精灵表、颜色 tint。
- Lua 5.5 脚本接口：`gfx`、`input`、`crumb` 三张表。
- 字体、窗口图标和 LuaLS 元数据内嵌；游戏资源仍从项目读取。
- 只读 VFS：项目目录、`.pak`、带 footer 的自包含 `.exe`。
- CLI：`version`、`create`、`lsp`、`run`、`dev`、`export`。
- 导出：单文件 exe，或 exe + pak。

## 环境要求

- Windows 10/11 x64。
- Visual Studio 2022 或更新版本的 Build Tools，安装“使用 C++ 的桌面开发”和 Windows SDK。
- [xmake](https://xmake.io/) 3.x。
- PowerShell（示例命令）。

Lua、sokol、stb、modlog 的源码已经在 `external/` 中，不需要包管理器或额外下载。

## 构建

Debug：

```powershell
xmake f -c -m debug
xmake build
```

Release：

```powershell
xmake f -c -m release
xmake build
```

产物在：

```text
build/windows/x64/<mode>/crumb.exe
build/windows/x64/<mode>/crumb_template_win64.exe
```

两个可执行 target（另有 `lua` 静态库 target）：

| target | 用途 |
|---|---|
| `crumb` | 编辑器/CLI 可执行文件，控制台子系统，包含 CLI 和导出逻辑。 |
| `crumb_template_win64` | 导出的 GUI 模板，不包含 `src/editor/`，由 sokol-app 提供入口。 |

> 注：`skill/规范.md` 中写的 target 名是 `crumb_editor`，当前 xmake 实际目标叫 `crumb`；下面的命令按实际目标编写。

切换 debug/release 时，项目和 Lua 模块缓存会重新生成；首次构建或切换模式会比较慢。

## 快速开始

以下命令都在引擎根目录（`xmake.lua` 所在目录）执行。`xmake run` 默认在 target 所在目录（`build/...`）运行；加 `-w .` 让相对路径基于引擎根目录。创建一个项目：

```powershell
xmake run -w . crumb create MyGame
```

生成的结构：

```text
MyGame/
  main.lua
  crumb.cfg
  meta/crumb.lua      # LuaLS 类型提示
  .luarc.json         # LuaLS 工作区配置
  sprites.png         # 可选
```

刷新 LuaLS 文件：

```powershell
xmake run -w . crumb lsp MyGame
```

运行：

```powershell
xmake run -w . crumb run MyGame
```

`dev` 目前只是 `run` 的别名，实时重载还没有实现：

```powershell
xmake run -w . crumb dev MyGame
```

导出：

```powershell
# 单文件：<project>/dist/<name>.exe
xmake run -w . crumb export MyGame

# exe + pak
xmake run -w . crumb export MyGame --format pak

# 指定输出路径
xmake run -w . crumb export MyGame -o dist/MyGame.exe

# 指定模板（默认使用编辑器旁边的 crumb_template_win64.exe）
xmake run -w . crumb export MyGame --template build/windows/x64/release/crumb_template_win64.exe
```

`-o` 的规则：如果目标是已存在的目录，会自动拼上 `<项目名>.exe`；如果路径没有扩展名，会补 `.exe`。

## 项目配置

`crumb.cfg` 是简单的 `key = value` 文本，`#` 开始行内注释：

```ini
# crumb project config
name = MyGame
width = 320
height = 180
sprite_size = 16
window_scale = 2
fixed_fps = 60
max_fixed_steps = 5
```

| key | 默认值 | 说明 |
|---|---:|---|
| `name` | 目录名 | 窗口标题。 |
| `width` / `height` | 320 / 180 | 逻辑游戏分辨率。 |
| `sprite_size` | 16 | 精灵表中一个 cell 的边长。 |
| `window_scale` | 2 | 初始窗口 = 逻辑分辨率 × scale。 |
| `fixed_fps` | 60 | `_fixed_update` 的频率。 |
| `max_fixed_steps` | 5 | 单帧最多追赶多少个固定步。 |

注意：当前解析器不支持带引号的值，且 `#` 会被当注释；项目名里不要用 `#`（见 [问题.md](问题.md) M3）。非 ASCII 路径也有已知编码问题（H3）。

## Lua 接口

### 生命周期

`main.lua` 在窗口和渲染器创建后执行。以下全局函数都是可选的：

```lua
function _init() end
function _fixed_update(dt) end  -- 每个固定步调用，可能一帧多次
function _update(dt) end        -- 每帧一次
function _draw(dt) end          -- 每帧一次，在 BeginFrame 之后
function _cleanup() end         -- 退出时一次
```

固定步长由 `fixed_fps` 决定；一帧最多补 `max_fixed_steps` 次，超出部分会被丢弃，避免死亡螺旋。

### 示例

```lua
local player = { x = 160, y = 90 }

function _init()
  crumb.set_title("My Game")
end

function _update(dt)
  local speed = 100
  if input.held(input.LEFT)  then player.x = player.x - speed * dt end
  if input.held(input.RIGHT) then player.x = player.x + speed * dt end
  if input.held(input.UP)    then player.y = player.y - speed * dt end
  if input.held(input.DOWN)  then player.y = player.y + speed * dt end
end

function _draw(dt)
  gfx.cls(gfx.DARK_BLUE)
  gfx.rect_fill(player.x - 8, player.y - 8, 16, 16, gfx.YELLOW)
  gfx.text("hello crumb", 8, 8, gfx.WHITE, 0.5)
end
```

### `gfx`

| 函数 | 说明 |
|---|---|
| `gfx.cls([color])` | 按绘制顺序清屏；默认黑色。 |
| `gfx.text(text, x, y [, color [, scale]])` | 画 ASCII 文本；基础 cell 为 16x16，可缩放。 |
| `gfx.rect_fill(x, y, w, h [, color])` | 实心矩形。 |
| `gfx.spr(index, x, y, w, h, flip_x, flip_y, color)` | PICO-8 形式，w/h 是 cell 数；画 1-based 精灵。 |
| `gfx.spr(index, x, y, flip_x, flip_y, color)` | 旧式简写，w/h 默认为 1。 |
| `gfx.measure_text(text [, scale])` | 返回文本宽度（游戏像素，整数）。 |
| `gfx.set_clear_color([color])` | 设置下一帧背景色。 |
| `gfx.screen_to_game_x(x)` / `_y(y)` | 把 framebuffer 坐标换算成游戏坐标。 |
| `gfx.rgb(r, g, b [, a])` | 生成 `0xRRGGBBAA` 颜色整数。 |

颜色参数可以是 `0xRRGGBBAA` 整数，也可以是 `{ r, g, b, a }` 表（字段或 1/2/3/4 下标都行）。调色板常量：`BLACK`、`DARK_BLUE`、`DARK_PURPLE`、`DARK_GREEN`、`BROWN`、`DARK_GRAY`、`LIGHT_GRAY`、`WHITE`、`RED`、`ORANGE`、`YELLOW`、`GREEN`、`BLUE`、`INDIGO`、`PINK`、`PEACH`、`TRUE_WHITE`、`TRANSPARENT`。

`gfx.cls` 是在渲染队列里追加一个整屏矩形，`set_clear_color` 改的是 `BeginFrame` 的背景色；两者用途不同。当前每帧会额外画一层背景，见 [问题.md](问题.md) M9。

### `input`

| 函数 | 说明 |
|---|---|
| `input.held(key)` / `pressed(key)` / `released(key)` | 按住 / 本帧按下 / 本帧松开。 |
| `input.key_code(name)` | 名字转 sokol key code；未知返回 -1。 |
| `input.mouse()` | 返回 `x, y`（framebuffer 像素）。 |
| `input.mouse_x()` / `mouse_y()` | 分别返回坐标。 |
| `input.mouse_held(button)` / `mouse_pressed` / `mouse_released` | 鼠标键状态。 |

按键常量包括方向键、`SPACE`、`ENTER`、`ESCAPE`、`TAB`、`BACKSPACE`、`SHIFT`、`CTRL`、`ALT`，`KEY_A`..`KEY_Z`，`KEY_0`..`KEY_9`，动作键 `BTN1`/`BTN2`/`BTN3`（Z/X/C），以及 `MOUSE_LEFT`/`MOUSE_RIGHT`/`MOUSE_MIDDLE`。

`key_code` 的字符串形式只接受单个小写字母/数字和 `"left"`、`"space"` 等命名键；大写字母不会被识别。

### `crumb`

| 成员 | 说明 |
|---|---|
| `crumb.width()` / `height()` | 当前 framebuffer 尺寸（像素）。 |
| `crumb.set_title(text)` | 修改窗口标题。 |
| `crumb.quit()` | 请求退出。 |
| `crumb.measure_text(text [, scale])` | 与 `gfx.measure_text` 相同。 |
| `crumb.GAME_W` / `GAME_H` / `SPRITE_SIZE` | 逻辑分辨率与精灵 cell 大小。 |

### 文本与精灵

- 字体是内嵌的 160x160 atlas，10x10 个 16x16 cell，覆盖 ASCII 32..126。
- 文本按字节遍历，目前**只支持 ASCII**；中文会显示为空白并额外推进光标。
- `sprites.png` 可选；不存在时使用 16x16 棋盘格占位图（`sprite_size` 非 16 时占位图 UV 不准确，见问题 M7）。索引从 1 开始，按行优先排列。
- `spr` 的 `w`/`h` 是 cell 数，不是像素；翻转作用于整个块。

## 目录结构

```text
src/
  main.cpp                 # 编辑器入口
  template_entry.cpp       # 导出模板入口（sokol_main）
  app/runtime.cpp[m]       # 项目解析、配置、主循环
  core/
    assets.cppm            # 构建期嵌入资源
    error.cppm             # Error / Result
    logger.cppm            # modlog 封装
    math.cppm              # Vec2 / Rect / 数学助手
    time.cppm              # 固定步长时钟
    types.cppm             # 定宽整数、string、to_* 转换
    version.cppm           # 版本打包
  platform/
    dialog.cppm            # Win32 错误对话框 / stderr 兜底
    impl.cpp               # sokol + stb_image 唯一实现单元
    input.cppm             # 键盘鼠标状态
    launch.cppm            # 可执行文件路径与 --project
    pak.cppm               # CRUMBPAK / CRUMBEND
    vfs.cppm               # 目录 / pak / self 挂载
    window.cppm            # sokol-app 窗口封装
  function/
    gfx/color.cppm         # RGBA 与 PICO-8 调色板
    gfx/renderer.cpp[m]    # 立即模式 2D 渲染器
    script/lua_vm.cppm     # Lua 5.5 封装
    script/bindings.cpp[m] # gfx / input / crumb 绑定
  editor/
    cli.cppm               # 命令分发与项目创建
    cli_print.cppm         # 带颜色的控制台输出
    export.cppm            # 单文件 / exe+pak 导出
  assets/
    font_16x16.png
    crumb-ico.png
    meta_crumb.lua         # LuaLS 元数据
```

依赖方向（箭头表示“依赖”）：

```text
editor -> app -> function -> platform -> core
```

下层不反向依赖上层。模块名与目录对应，例如 `src/function/gfx/renderer.cppm` 导出 `crumb.gfx.renderer`。

更细的命名、分层和错误处理约定见本地 `skill/规范.md`（该目录被 git 忽略，不随仓库分发）。

## 导出与 pak 格式

### 单文件

```text
+------------------+------------------+------------------+
| template exe     | pak image        | CRUMBEND footer  |
+------------------+------------------+------------------+
```

模板启动时：

1. 如果命令行有 `--project`，优先使用它。
2. 否则读取自身末尾 24 字节的 `CRUMBEND` footer，取出 pak。
3. 如果 footer 不存在，尝试同目录的 `<exe名>.pak`。
4. 再失败则把 exe 所在目录当作项目目录（开发模式）。

### exe + pak

```text
MyGame.exe
MyGame.pak
```

### pak 布局

所有整数均为小端：

```text
header:
  8  bytes  "CRUMBPAK"
  4  bytes  version (1)
  4  bytes  entry count

entry (重复 count 次):
  2  bytes  name length
  N  bytes  name (UTF-8, '/' 分隔)
  4  bytes  payload offset
  4  bytes  payload size
  4  bytes  payload CRC32

payload: 各 entry 的原始字节

footer:
  8  bytes  "CRUMBEND"
  4  bytes  pak offset
  4  bytes  pak size
  4  bytes  pak CRC32
  4  bytes  reserved (0)
```

当前 loader 会校验每个 entry 的 CRC，但 footer 里的整包 CRC 还没有校验；见 [问题.md](问题.md) M1。

## 开发状态与已知限制

- 当前只支持 Windows x64；xmake 里有 MSVC 专用选项。
- “编辑器”目前是 CLI，不是图形化编辑器；没有内置场景/关卡编辑。
- 没有 live reload；`dev` 等同 `run`。
- 没有音频、手柄/触摸输入、ECS、资源注册表、动画、字体加载等。
- 文本只支持 ASCII；非 ASCII 路径存在编码问题。
- Lua 没有打开 `io`/`os`，但 base 库仍提供 `dofile`/`loadfile`；脚本应按可信代码对待。
- 当前版本建议项目名不要含 `#`，项目路径尽量使用 ASCII（见问题 M3/H3）。
- 没有编译器告警开关、sanitizer 和单元测试。
- 更完整的代码审查问题、复现和建议见 [问题.md](问题.md)。

## 验证

```powershell
xmake build
xmake run -w . crumb version
xmake run -w . crumb create _tmp/Game
xmake run -w . crumb lsp _tmp/Game
xmake run -w . crumb export _tmp/Game
xmake run -w . crumb export _tmp/Game --format pak
```

构建成功只说明编译/链接通过；GUI 渲染和实际游戏行为仍需在桌面环境运行模板验证。

## 注释与代码风格

- C++ 代码使用英文、简洁的意图注释；公共接口说明前置条件/所有权，非显然的边界写原因，不写变更历史。
- C++23 模块，标准库头文件放在 `module;` 全局模块片段，模块声明后只用 `import`；不引入 `import std`。
- 业务错误用 `Result<T>` / `VoidResult` 表达，不靠异常做控制流。
- 命名、分层、资源所有权等完整规则见 `skill/规范.md`。

## License

MIT，见 [LICENSE](LICENSE)。Copyright (c) 2026 hewuhu。

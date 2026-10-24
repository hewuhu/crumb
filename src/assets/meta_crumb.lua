---@meta
---@diagnostic disable: lowercase-global

---@alias CrumbColor integer

---@class CrumbGfx
---@field BLACK CrumbColor
---@field DARK_BLUE CrumbColor
---@field DARK_PURPLE CrumbColor
---@field DARK_GREEN CrumbColor
---@field BROWN CrumbColor
---@field DARK_GRAY CrumbColor
---@field LIGHT_GRAY CrumbColor
---@field WHITE CrumbColor
---@field RED CrumbColor
---@field ORANGE CrumbColor
---@field YELLOW CrumbColor
---@field GREEN CrumbColor
---@field BLUE CrumbColor
---@field INDIGO CrumbColor
---@field PINK CrumbColor
---@field PEACH CrumbColor
---@field TRUE_WHITE CrumbColor
---@field TRANSPARENT CrumbColor
gfx = {}

---Clear the screen.
---@param color? CrumbColor
function gfx.cls(color) end

---Draw a line of text.
---@param text string
---@param x number
---@param y number
---@param color? CrumbColor
---@param scale? number
function gfx.text(text, x, y, color, scale) end

---Draw a filled rectangle.
---@param x number
---@param y number
---@param w number
---@param h number
---@param color? CrumbColor
function gfx.rect_fill(x, y, w, h, color) end

---Draw a sprite block. w/h are sprite-cell counts (PICO-8 style).
---@param index integer
---@param x number
---@param y number
---@param w? integer
---@param h? integer
---@param flip_x? boolean
---@param flip_y? boolean
---@param color? CrumbColor
---@overload fun(index: integer, x: number, y: number, flip_x: boolean, flip_y: boolean, color?: CrumbColor)
function gfx.spr(index, x, y, w, h, flip_x, flip_y, color) end

---Measure text width in game pixels.
---@param text string
---@param scale? number
---@return integer
function gfx.measure_text(text, scale) end

---Change the background color used by the renderer.
---@param color? CrumbColor
function gfx.set_clear_color(color) end

---Convert a screen/framebuffer coordinate to game coordinates.
---@param x number
---@return number
function gfx.screen_to_game_x(x) end

---Convert a screen/framebuffer coordinate to game coordinates.
---@param y number
---@return number
function gfx.screen_to_game_y(y) end

---Build a packed color from RGBA components.
---@param r integer
---@param g integer
---@param b integer
---@param a? integer
---@return CrumbColor
function gfx.rgb(r, g, b, a) end

---@class CrumbInput
---@field LEFT integer
---@field RIGHT integer
---@field UP integer
---@field DOWN integer
---@field SPACE integer
---@field ENTER integer
---@field ESCAPE integer
---@field TAB integer
---@field BACKSPACE integer
---@field SHIFT integer
---@field CTRL integer
---@field ALT integer
---@field BTN1 integer
---@field BTN2 integer
---@field BTN3 integer
---@field MOUSE_LEFT integer
---@field MOUSE_RIGHT integer
---@field MOUSE_MIDDLE integer
---@field KEY_A integer
---@field KEY_B integer
---@field KEY_C integer
---@field KEY_D integer
---@field KEY_E integer
---@field KEY_F integer
---@field KEY_G integer
---@field KEY_H integer
---@field KEY_I integer
---@field KEY_J integer
---@field KEY_K integer
---@field KEY_L integer
---@field KEY_M integer
---@field KEY_N integer
---@field KEY_O integer
---@field KEY_P integer
---@field KEY_Q integer
---@field KEY_R integer
---@field KEY_S integer
---@field KEY_T integer
---@field KEY_U integer
---@field KEY_V integer
---@field KEY_W integer
---@field KEY_X integer
---@field KEY_Y integer
---@field KEY_Z integer
---@field KEY_0 integer
---@field KEY_1 integer
---@field KEY_2 integer
---@field KEY_3 integer
---@field KEY_4 integer
---@field KEY_5 integer
---@field KEY_6 integer
---@field KEY_7 integer
---@field KEY_8 integer
---@field KEY_9 integer
input = {}

---@param key integer
---@return boolean
function input.held(key) end

---@param key integer
---@return boolean
function input.pressed(key) end

---@param key integer
---@return boolean
function input.released(key) end

---@param name string
---@return integer
function input.key_code(name) end

---@return number x
---@return number y
function input.mouse() end

---@return number
function input.mouse_x() end

---@return number
function input.mouse_y() end

---@param button integer
---@return boolean
function input.mouse_held(button) end

---@param button integer
---@return boolean
function input.mouse_pressed(button) end

---@param button integer
---@return boolean
function input.mouse_released(button) end

---@class CrumbEngine
---@field GAME_W integer
---@field GAME_H integer
---@field SPRITE_SIZE integer
crumb = {}

---@return integer
function crumb.width() end

---@return integer
function crumb.height() end

---@param title string
function crumb.set_title(title) end

function crumb.quit() end

---@param text string
---@param scale? number
---@return integer
function crumb.measure_text(text, scale) end

---Called once when the game starts.
function _init() end

---Called at a fixed timestep for physics and deterministic logic.
---@param dt number
function _fixed_update(dt) end

---Called every frame before drawing.
---@param dt number
function _update(dt) end

---Called every frame after update.
---@param dt number
function _draw(dt) end

---Called once when the game exits.
function _cleanup() end

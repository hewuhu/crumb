module;
#include <cstring>
#include <string_view>
#include <utility>
#include <sokol/sokol_app.h>
export module crumb.input;
import crumb.type;

constexpr int kMaxKeys    = 512;
constexpr int kMouseCount = 3;

struct InputState {
  bool  keyDown[kMaxKeys] {};
  bool  keyPressed[kMaxKeys] {};
  bool  keyReleased[kMaxKeys] {};
  bool  mouseDown[kMouseCount] {};
  bool  mousePressed[kMouseCount] {};
  bool  mouseReleased[kMouseCount] {};
  float mouseX = 0.0f;
  float mouseY = 0.0f;
};

InputState state;

void ClearTransient() {
  std::memset(state.keyPressed,    0, sizeof(state.keyPressed));
  std::memset(state.keyReleased,   0, sizeof(state.keyReleased));
  std::memset(state.mousePressed,  0, sizeof(state.mousePressed));
  std::memset(state.mouseReleased, 0, sizeof(state.mouseReleased));
}

void ClearAll() {
  std::memset(&state, 0, sizeof(state));
}

export namespace crumb::input {

// Map a friendly key name ("left", "a", "space") to a sokol key code.
int   KeyCode(string_view name);
// Held, pressed this frame, released this frame.
bool  KeyHeld(int key);
bool  KeyPressed(int key);
bool  KeyReleased(int key);

// Mouse position is in framebuffer pixels.
float MouseX();
float MouseY();
bool  MouseDown(int button);
bool  MousePressed(int button);
bool  MouseReleased(int button);

// Reset all input state (window focus loss / startup).
void Reset();
// Clear per-frame pressed/released flags.
void EndFrame();
void HandleEvent(const sapp_event* event);
} // namespace crumb::input

namespace crumb::input {
int KeyCode(string_view name) {
  if (name.size() == 1) {
    const char c = name[0];
    if (c >= 'a' && c <= 'z') return SAPP_KEYCODE_A + (c - 'a');
    if (c >= '0' && c <= '9') return SAPP_KEYCODE_0 + (c - '0');
  }
  static constexpr std::pair<string_view, int> kTable[] = {
    {"left",      SAPP_KEYCODE_LEFT},
    {"right",     SAPP_KEYCODE_RIGHT},
    {"up",        SAPP_KEYCODE_UP},
    {"down",      SAPP_KEYCODE_DOWN},
    {"space",     SAPP_KEYCODE_SPACE},
    {"enter",     SAPP_KEYCODE_ENTER},
    {"escape",    SAPP_KEYCODE_ESCAPE},
    {"tab",       SAPP_KEYCODE_TAB},
    {"backspace", SAPP_KEYCODE_BACKSPACE},
    {"shift",     SAPP_KEYCODE_LEFT_SHIFT},
    {"ctrl",      SAPP_KEYCODE_LEFT_CONTROL},
    {"alt",       SAPP_KEYCODE_LEFT_ALT},
  };
  for (auto [k, v] : kTable)
    if (name == k) return v;
  return -1;
}

bool KeyHeld(int k)     { return k >= 0 && k < kMaxKeys && state.keyDown[k]; }
bool KeyPressed(int k)  { return k >= 0 && k < kMaxKeys && state.keyPressed[k]; }
bool KeyReleased(int k) { return k >= 0 && k < kMaxKeys && state.keyReleased[k]; }

float MouseX() { return state.mouseX; }
float MouseY() { return state.mouseY; }

bool MouseDown(int b)     { return b >= 0 && b < kMouseCount && state.mouseDown[b]; }
bool MousePressed(int b)  { return b >= 0 && b < kMouseCount && state.mousePressed[b]; }
bool MouseReleased(int b) { return b >= 0 && b < kMouseCount && state.mouseReleased[b]; }

void Reset()    { ClearAll(); }
void EndFrame() { ClearTransient(); }

void HandleEvent(const sapp_event* e) {
  switch (e->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
      if (e->key_code < kMaxKeys) {
        if (!e->key_repeat) state.keyPressed[e->key_code] = true;
        state.keyDown[e->key_code] = true;
      }
      break;

    case SAPP_EVENTTYPE_KEY_UP:
      if (e->key_code < kMaxKeys) {
        state.keyDown[e->key_code]     = false;
        state.keyReleased[e->key_code] = true;
      }
      break;

    case SAPP_EVENTTYPE_MOUSE_MOVE:
      state.mouseX = e->mouse_x;
      state.mouseY = e->mouse_y;
      break;

    case SAPP_EVENTTYPE_MOUSE_DOWN:
      if (e->mouse_button < kMouseCount) {
        state.mouseDown[e->mouse_button]    = true;
        state.mousePressed[e->mouse_button] = true;
      }
      break;

    case SAPP_EVENTTYPE_MOUSE_UP:
      if (e->mouse_button < kMouseCount) {
        state.mouseDown[e->mouse_button]     = false;
        state.mouseReleased[e->mouse_button] = true;
      }
      break;

    case SAPP_EVENTTYPE_UNFOCUSED:
      ClearAll();
      break;

    default: break;
  }
}

} // namespace crumb::input

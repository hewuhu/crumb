module;
#include <cstring>
#include <string_view>
#include <utility>

#include <sokol/sokol_app.h>
export module crumb.platform.input;
import crumb.core.types;

export namespace crumb::input {

// SAPP_MAX_KEYCODES is 512; sokol only reports codes below it.
inline constexpr int MAX_KEYS    = 512;
inline constexpr int MOUSE_COUNT = 3;

// Input owned by WindowContext, not a module-level global.
struct InputState {
  bool  keyDown[MAX_KEYS] {};
  bool  keyPressed[MAX_KEYS] {};
  bool  keyReleased[MAX_KEYS] {};
  bool  mouseDown[MOUSE_COUNT] {};
  bool  mousePressed[MOUSE_COUNT] {};
  bool  mouseReleased[MOUSE_COUNT] {};
  float mouseX = 0.0f;
  float mouseY = 0.0f;
};

// Map a friendly name ("a", "0", "left") to a sokol key code; -1 if unknown.
int   KeyCode(string_view name);

// Held, pressed this frame, released this frame.
bool  KeyHeld(const InputState& state, int key);
bool  KeyPressed(const InputState& state, int key);
bool  KeyReleased(const InputState& state, int key);

// Mouse position in framebuffer pixels.
float MouseX(const InputState& state);
float MouseY(const InputState& state);
bool  MouseDown(const InputState& state, int button);
bool  MousePressed(const InputState& state, int button);
bool  MouseReleased(const InputState& state, int button);

// Clear all input state (used on startup).
void Reset(InputState& state);

// Clear per-frame pressed/released flags after the frame callback.
void EndFrame(InputState& state);
void HandleEvent(InputState& state, const sapp_event* event);

}

namespace crumb::input {

namespace {

// Clear per-frame edges but keep held state and mouse position.
void ClearTransient(InputState& state) {
  std::memset(state.keyPressed,    0, sizeof(state.keyPressed));
  std::memset(state.keyReleased,   0, sizeof(state.keyReleased));
  std::memset(state.mousePressed,  0, sizeof(state.mousePressed));
  std::memset(state.mouseReleased, 0, sizeof(state.mouseReleased));
}

// Drop all held state and edges; mouse position is reset to zero as well.
void ClearAll(InputState& state) {
  std::memset(&state, 0, sizeof(state));
}

}

// Single lowercase letters/digits and a small named-key table. Uppercase names
// are not accepted.
int KeyCode(string_view name) {
  if (name.size() == 1) {
    const char character = name[0];
    if (character >= 'a' && character <= 'z')
      return SAPP_KEYCODE_A + (character - 'a');
    if (character >= '0' && character <= '9')
      return SAPP_KEYCODE_0 + (character - '0');
  }

  static constexpr std::pair<string_view, int> KEY_TABLE[] = {
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

  for (auto [key_name, key_value] : KEY_TABLE)
    if (name == key_name)
      return key_value;

  return -1;
}

bool KeyHeld(const InputState& state, int key) {
  return key >= 0 && key < MAX_KEYS && state.keyDown[key];
}

bool KeyPressed(const InputState& state, int key) {
  return key >= 0 && key < MAX_KEYS && state.keyPressed[key];
}

bool KeyReleased(const InputState& state, int key) {
  return key >= 0 && key < MAX_KEYS && state.keyReleased[key];
}

float MouseX(const InputState& state) { return state.mouseX; }
float MouseY(const InputState& state) { return state.mouseY; }

bool MouseDown(const InputState& state, int button) {
  return button >= 0 && button < MOUSE_COUNT && state.mouseDown[button];
}

bool MousePressed(const InputState& state, int button) {
  return button >= 0 && button < MOUSE_COUNT && state.mousePressed[button];
}

bool MouseReleased(const InputState& state, int button) {
  return button >= 0 && button < MOUSE_COUNT && state.mouseReleased[button];
}

void Reset(InputState& state)    { ClearAll(state); }
void EndFrame(InputState& state) { ClearTransient(state); }

// Translate sokol events into cumulative held state plus per-frame edges.
void HandleEvent(InputState& state, const sapp_event* event) {
  switch (event->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
      if (event->key_code < MAX_KEYS) {
        if (!event->key_repeat)
          state.keyPressed[event->key_code] = true;
        state.keyDown[event->key_code] = true;
      }
      break;

    case SAPP_EVENTTYPE_KEY_UP:
      if (event->key_code < MAX_KEYS) {
        state.keyDown[event->key_code]     = false;
        state.keyReleased[event->key_code] = true;
      }
      break;

    case SAPP_EVENTTYPE_MOUSE_MOVE:
      state.mouseX = event->mouse_x;
      state.mouseY = event->mouse_y;
      break;

    case SAPP_EVENTTYPE_MOUSE_DOWN:
      if (event->mouse_button < MOUSE_COUNT) {
        state.mouseDown[event->mouse_button]    = true;
        state.mousePressed[event->mouse_button] = true;
      }
      break;

    case SAPP_EVENTTYPE_MOUSE_UP:
      if (event->mouse_button < MOUSE_COUNT) {
        state.mouseDown[event->mouse_button]     = false;
        state.mouseReleased[event->mouse_button] = true;
      }
      break;

    case SAPP_EVENTTYPE_UNFOCUSED:
      ClearAll(state);
      break;

    default:
      break;
  }
}

}

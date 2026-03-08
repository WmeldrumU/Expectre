#pragma once

#include "observer.h"

#include <SDL3/SDL.h>

#include <NsCore/Noesis.h>
#include <NsGui/CoreApi.h>
#include <NsGui/IView.h>
#include <NsCore/ReflectionDeclareEnum.h>
#include <NsGui/InputEnums.h>

#include <spdlog/spdlog.h>

namespace Expectre {

// -----------------------------
// Modifiers: SDL -> Noesis
// -----------------------------
inline Noesis::ModifierKeys TranslateModifiers(SDL_Keymod mods) {
  using MK = Noesis::ModifierKeys;

  int out = 0;

  if (mods & SDL_KMOD_SHIFT)
    out |= MK::ModifierKeys_Shift;
  if (mods & SDL_KMOD_CTRL)
    out |= MK::ModifierKeys_Control;
  if (mods & SDL_KMOD_ALT)
    out |= MK::ModifierKeys_Alt;

  // Noesis has "Windows" key as MK_Windows in many versions.
  // SDL uses GUI for Command (macOS) / Windows key.
  if (mods & SDL_KMOD_GUI)
    out |= MK::ModifierKeys_Windows;

  return static_cast<Noesis::ModifierKeys>(out);
}

// -----------------------------
// Mouse buttons: SDL -> Noesis
// -----------------------------
inline Noesis::MouseButton TranslateMouseButton(uint8_t sdlButton) {
  switch (sdlButton) {
  case SDL_BUTTON_LEFT:
    return Noesis::MouseButton_Left;
  case SDL_BUTTON_MIDDLE:
    return Noesis::MouseButton_Middle;
  case SDL_BUTTON_RIGHT:
    return Noesis::MouseButton_Right;

#ifdef Noesis_MouseButton_XButton1
  case SDL_BUTTON_X1:
    return Noesis::MouseButton_XButton1;
  case SDL_BUTTON_X2:
    return Noesis::MouseButton_XButton2;
#endif
  default:
    return Noesis::MouseButton_Left; // fallback
  }
}

// -----------------------------
// Keys: SDL_Scancode -> Noesis::Key
// Use scancodes for physical keys; text input comes from SDL_TEXT_INPUT.
// -----------------------------
inline Noesis::Key TranslateKey(SDL_Scancode sc) {
  using K = Noesis::Key;

  switch (sc) {
    // Letters
    // clang-format off
  case SDL_SCANCODE_A:    return K::Key_A;
  case SDL_SCANCODE_B:    return K::Key_B;
  case SDL_SCANCODE_C:    return K::Key_C;
  case SDL_SCANCODE_D:    return K::Key_D;
  case SDL_SCANCODE_E:    return K::Key_E;
  case SDL_SCANCODE_F:    return K::Key_F;
  case SDL_SCANCODE_G:    return K::Key_G;
  case SDL_SCANCODE_H:    return K::Key_H;
  case SDL_SCANCODE_I:    return K::Key_I;
  case SDL_SCANCODE_J:    return K::Key_J;
  case SDL_SCANCODE_K:    return K::Key_K;
  case SDL_SCANCODE_L:    return K::Key_L;
  case SDL_SCANCODE_M:    return K::Key_M;
  case SDL_SCANCODE_N:    return K::Key_N;
  case SDL_SCANCODE_O:    return K::Key_O;
  case SDL_SCANCODE_P:    return K::Key_P;
  case SDL_SCANCODE_Q:    return K::Key_Q;
  case SDL_SCANCODE_R:    return K::Key_R;
  case SDL_SCANCODE_S:    return K::Key_S;
  case SDL_SCANCODE_T:    return K::Key_T;
  case SDL_SCANCODE_U:    return K::Key_U;
  case SDL_SCANCODE_V:    return K::Key_V;
  case SDL_SCANCODE_W:    return K::Key_W;
  case SDL_SCANCODE_X:    return K::Key_X;
  case SDL_SCANCODE_Y:    return K::Key_Y;
  case SDL_SCANCODE_Z:    return K::Key_Z;

  // Numbers (top row)
  case SDL_SCANCODE_0:    return K::Key_D0;
  case SDL_SCANCODE_1:    return K::Key_D1;
  case SDL_SCANCODE_2:    return K::Key_D2;
  case SDL_SCANCODE_3:    return K::Key_D3;
  case SDL_SCANCODE_4:    return K::Key_D4;
  case SDL_SCANCODE_5:    return K::Key_D5;
  case SDL_SCANCODE_6:    return K::Key_D6;
  case SDL_SCANCODE_7:    return K::Key_D7;
  case SDL_SCANCODE_8:    return K::Key_D8;
  case SDL_SCANCODE_9:    return K::Key_D9;

  // Common keys
  case SDL_SCANCODE_RETURN:    return K::Key_Return;
  case SDL_SCANCODE_ESCAPE:    return K::Key_Escape;
  case SDL_SCANCODE_BACKSPACE: return K::Key_Back;
  case SDL_SCANCODE_TAB:       return K::Key_Tab;
  case SDL_SCANCODE_SPACE:     return K::Key_Space;

  // Punctuation / OEM
  case SDL_SCANCODE_MINUS:        return K::Key_OemMinus;
  case SDL_SCANCODE_EQUALS:       return K::Key_OemPlus;
  case SDL_SCANCODE_LEFTBRACKET:  return K::Key_OemOpenBrackets;
  case SDL_SCANCODE_RIGHTBRACKET: return K::Key_OemCloseBrackets;
  case SDL_SCANCODE_BACKSLASH:    return K::Key_OemPipe;
  case SDL_SCANCODE_SEMICOLON:    return K::Key_OemSemicolon;
  case SDL_SCANCODE_APOSTROPHE:   return K::Key_OemQuotes;
  case SDL_SCANCODE_GRAVE:        return K::Key_OemTilde;
  case SDL_SCANCODE_COMMA:        return K::Key_OemComma;
  case SDL_SCANCODE_PERIOD:       return K::Key_OemPeriod;
  case SDL_SCANCODE_SLASH:        return K::Key_OemQuestion;

  // Navigation
  case SDL_SCANCODE_INSERT:   return K::Key_Insert;
  case SDL_SCANCODE_DELETE:   return K::Key_Delete;
  case SDL_SCANCODE_HOME:     return K::Key_Home;
  case SDL_SCANCODE_END:      return K::Key_End;
  case SDL_SCANCODE_PAGEUP:   return K::Key_PageUp;
  case SDL_SCANCODE_PAGEDOWN: return K::Key_PageDown;

  // Arrows
  case SDL_SCANCODE_UP:    return K::Key_Up;
  case SDL_SCANCODE_DOWN:  return K::Key_Down;
  case SDL_SCANCODE_LEFT:  return K::Key_Left;
  case SDL_SCANCODE_RIGHT: return K::Key_Right;

  // Modifiers
  case SDL_SCANCODE_LSHIFT: return K::Key_LeftShift;
  case SDL_SCANCODE_RSHIFT: return K::Key_RightShift;
  case SDL_SCANCODE_LCTRL:  return K::Key_LeftCtrl;
  case SDL_SCANCODE_RCTRL:  return K::Key_RightCtrl;
  case SDL_SCANCODE_LALT:   return K::Key_LeftAlt;
  case SDL_SCANCODE_RALT:   return K::Key_RightAlt;
  case SDL_SCANCODE_LGUI:   return K::Key_LWin;
  case SDL_SCANCODE_RGUI:   return K::Key_RWin;

  // Function keys
  case SDL_SCANCODE_F1:  return K::Key_F1;
  case SDL_SCANCODE_F2:  return K::Key_F2;
  case SDL_SCANCODE_F3:  return K::Key_F3;
  case SDL_SCANCODE_F4:  return K::Key_F4;
  case SDL_SCANCODE_F5:  return K::Key_F5;
  case SDL_SCANCODE_F6:  return K::Key_F6;
  case SDL_SCANCODE_F7:  return K::Key_F7;
  case SDL_SCANCODE_F8:  return K::Key_F8;
  case SDL_SCANCODE_F9:  return K::Key_F9;
  case SDL_SCANCODE_F10: return K::Key_F10;
  case SDL_SCANCODE_F11: return K::Key_F11;
  case SDL_SCANCODE_F12: return K::Key_F12;

  // Keypad
  case SDL_SCANCODE_KP_0: return K::Key_NumPad0;
  case SDL_SCANCODE_KP_1: return K::Key_NumPad1;
  case SDL_SCANCODE_KP_2: return K::Key_NumPad2;
  case SDL_SCANCODE_KP_3: return K::Key_NumPad3;
  case SDL_SCANCODE_KP_4: return K::Key_NumPad4;
  case SDL_SCANCODE_KP_5: return K::Key_NumPad5;
  case SDL_SCANCODE_KP_6: return K::Key_NumPad6;
  case SDL_SCANCODE_KP_7: return K::Key_NumPad7;
  case SDL_SCANCODE_KP_8: return K::Key_NumPad8;
  case SDL_SCANCODE_KP_9: return K::Key_NumPad9;

  case SDL_SCANCODE_KP_ENTER:    return K::Key_Return;
  case SDL_SCANCODE_KP_PLUS:     return K::Key_Add;
  case SDL_SCANCODE_KP_MINUS:    return K::Key_Subtract;
  case SDL_SCANCODE_KP_MULTIPLY: return K::Key_Multiply;
  case SDL_SCANCODE_KP_DIVIDE:   return K::Key_Divide;
  case SDL_SCANCODE_KP_DECIMAL:  return K::Key_Decimal;

  default: return K::Key_None;
    // clang-format on
  }
}

// -----------------------------
// Dispatch a single SDL event to a Noesis IView.
// -----------------------------
inline void HandleSdlEventForNoesis(const SDL_Event &e, Noesis::IView *view,
                                    float dpiScale = 1.0f) {
  if (!view)
    return;

  switch (e.type) {
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP: {
    const bool isDown = (e.type == SDL_EVENT_KEY_DOWN);
    const SDL_KeyboardEvent &ke = e.key;

    const Noesis::Key key = TranslateKey(ke.scancode);
    if (key == Noesis::Key_None)
      break;

    if (isDown)
      view->KeyDown(key);
    else
      view->KeyUp(key);

    break;
  }

  case SDL_EVENT_TEXT_INPUT: {
    const SDL_TextInputEvent &te = e.text;

    const char *utf8 = te.text;
    while (*utf8) {
      uint32_t cp = 0;

      auto decodeOne = [](const char *s, uint32_t &outCp) -> int {
        const unsigned char c0 = (unsigned char)s[0];
        if (c0 < 0x80) {
          outCp = c0;
          return 1;
        }
        if ((c0 >> 5) == 0x6) {
          outCp = ((c0 & 0x1F) << 6) | ((unsigned char)s[1] & 0x3F);
          return 2;
        }
        if ((c0 >> 4) == 0xE) {
          outCp = ((c0 & 0x0F) << 12) | (((unsigned char)s[1] & 0x3F) << 6) |
                  ((unsigned char)s[2] & 0x3F);
          return 3;
        }
        if ((c0 >> 3) == 0x1E) {
          outCp = ((c0 & 0x07) << 18) | (((unsigned char)s[1] & 0x3F) << 12) |
                  (((unsigned char)s[2] & 0x3F) << 6) |
                  ((unsigned char)s[3] & 0x3F);
          return 4;
        }
        outCp = 0xFFFD;
        return 1;
      };

      int adv = decodeOne(utf8, cp);
      utf8 += adv;

      if (cp != 0)
        view->Char(cp);
    }

    break;
  }

  case SDL_EVENT_MOUSE_MOTION: {
    const SDL_MouseMotionEvent &me = e.motion;

    const int x = static_cast<int>(me.x * dpiScale);
    const int y = static_cast<int>(me.y * dpiScale);

    view->MouseMove(x, y);
    break;
  }

  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP: {
    const bool isDown = (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
    const SDL_MouseButtonEvent &be = e.button;

    const int x = static_cast<int>(be.x * dpiScale);
    const int y = static_cast<int>(be.y * dpiScale);

    const Noesis::MouseButton btn = TranslateMouseButton(be.button);

    if (isDown)
      view->MouseButtonDown(x, y, btn);
    else
      view->MouseButtonUp(x, y, btn);

    break;
  }

  case SDL_EVENT_MOUSE_WHEEL: {
    const SDL_MouseWheelEvent &we = e.wheel;

    constexpr int kWheelStep = 120;

    int wheelDelta = static_cast<int>(we.y * kWheelStep);

    float mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    mx = static_cast<int>(mx * dpiScale);
    my = static_cast<int>(my * dpiScale);

    view->MouseWheel(mx, my, wheelDelta);
    break;
  }

  default:
    break;
  }
}

// -----------------------------
// Observer adapter: bridges the engine's InputObserver (SDL-only)
// to a Noesis IView. No Noesis types leak into the core engine.
// -----------------------------
class NoesisInputAdapter final : public InputObserver {
public:
  explicit NoesisInputAdapter(Noesis::IView *view, float dpiScale = 1.0f)
      : m_view(view), m_dpiScale(dpiScale) {}

  void on_input_event(const SDL_Event &event) override {
    if (!m_loggedFirstEvent) {
      spdlog::info("[NoesisInputAdapter] First event received, type={}", event.type);
      m_loggedFirstEvent = true;
    }
    if (event.type == SDL_EVENT_MOUSE_MOTION && !m_loggedFirstMouseMove) {
      spdlog::info("[NoesisInputAdapter] First MouseMove: x={}, y={}",
                   event.motion.x, event.motion.y);
      m_loggedFirstMouseMove = true;
    }
    HandleSdlEventForNoesis(event, m_view, m_dpiScale);
  }

private:
  Noesis::IView *m_view = nullptr;
  float m_dpiScale = 1.0f;
  bool m_loggedFirstEvent = false;
  bool m_loggedFirstMouseMove = false;
};

} // namespace Expectre

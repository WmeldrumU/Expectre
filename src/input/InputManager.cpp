#include "input/InputManager.h"

namespace Expectre {
bool InputManager::Update() {
  // Store previous mouse position
  glm::vec2 prevMousePos = m_mouse_pos;

  // Get current mouse state
  float x, y;
  SDL_GetMouseState(&x, &y);
  m_mouse_pos = glm::vec2(x, y);

  // Calculate delta
  m_mouse_delta = m_mouse_pos - prevMousePos;

  // Copy current keys to previous for next frame
  m_previousKeys = m_currentKeys;

  // Poll SDL events for keyboard
  SDL_Event event;
  while (SDL_PollEvent(&event)) {

    // Handle events on input queue

    if (event.type == SDL_EVENT_KEY_DOWN) {
      m_currentKeys[event.key.key] = true;
    } else if (event.type == SDL_EVENT_KEY_UP) {
      m_currentKeys[event.key.key] = false;
    } else if (event.type == SDL_EVENT_QUIT) {
      return true;
    }
  }
  return false;
}

bool InputManager::IsKeyDown(SDL_Keycode key) const {
  auto it = m_currentKeys.find(key);
  return it != m_currentKeys.end() && it->second;
}

bool InputManager::IsKeyPressed(SDL_Keycode key) const {
  // Pressed this frame = down now, but not down last frame
  auto currentIt = m_currentKeys.find(key);
  auto prevIt = m_previousKeys.find(key);

  bool isCurrentlyDown = currentIt != m_currentKeys.end() && currentIt->second;
  bool wasPreviouslyDown = prevIt != m_previousKeys.end() && prevIt->second;

  return isCurrentlyDown && !wasPreviouslyDown;
}

bool InputManager::IsKeyReleased(SDL_Keycode key) const {
  // Released this frame = not down now, but was down last frame
  auto currentIt = m_currentKeys.find(key);
  auto prevIt = m_previousKeys.find(key);

  bool isCurrentlyDown = currentIt != m_currentKeys.end() && currentIt->second;
  bool wasPreviouslyDown = prevIt != m_previousKeys.end() && prevIt->second;

  return !isCurrentlyDown && wasPreviouslyDown;
}

glm::vec2 InputManager::GetMousePosition() const { return m_mouse_pos; }
glm::vec2 InputManager::GetMouseDelta() const { return m_mouse_delta; }
} // namespace Expectre
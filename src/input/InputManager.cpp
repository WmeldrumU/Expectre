#include "input/InputManager.h"

namespace Expectre {

InputManager::InputManager() {
  Subscribe(SDL_EVENT_KEY_DOWN,
            [this](const SDL_Event &e) { this->OnKeyDown(e); });

  Subscribe(SDL_EVENT_KEY_UP, [this](const SDL_Event &e) { this->OnKeyUp(e); });
}
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
    if (event.type == SDL_EVENT_QUIT) {
      return true; // Quit application
    }

    // Broadcast raw event to all registered observers (e.g. Noesis UI)
    for (auto it = m_observers.begin(); it != m_observers.end();) {
      if (auto obs = it->lock()) {
        obs->on_input_event(event);
        ++it;
      } else {
        it = m_observers.erase(it); // expired, remove
      }
    }

    // Fire callbacks for the event we've polled from the event queue
    if (subscribers.count((SDL_EventType)event.type)) {
      for (auto &callback : subscribers[(SDL_EventType)event.type]) {
        callback(event);
      }
    }
  }
  return false;
}

void InputManager::AddObserver(std::shared_ptr<InputObserver> observer) {
  m_observers.push_back(observer);
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

void InputManager::OnKeyDown(SDL_Event e) { m_currentKeys[e.key.key] = true; }

void InputManager::OnKeyUp(SDL_Event e) { m_currentKeys[e.key.key] = false; }

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

bool InputManager::resize_pending() { return m_pending_resize; }

glm::uvec2 InputManager::get_pending_resize() {
  m_pending_resize = false;
  return m_pending_resize_resolution;
}
} // namespace Expectre
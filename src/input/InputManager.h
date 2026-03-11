#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H
#include <SDL3/SDL.h>
#include <functional>
#include <glm/vec2.hpp>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "observer.h"

namespace Expectre {
using EventCallback = std::function<void(const SDL_Event &)>;

class InputManager {
public:
  InputManager();
  // Called once per frame to poll SDL events
  bool Update();

  bool IsKeyDown(SDL_Keycode key) const;
  // Just this frame
  bool IsKeyPressed(SDL_Keycode key) const;
  bool IsKeyReleased(SDL_Keycode key) const;

  glm::vec2 GetMousePosition() const;
  glm::vec2 GetMouseDelta() const;

  // Register an observer that will receive every raw SDL_Event each frame.
  void AddObserver(std::shared_ptr<InputObserver> observer);

  void Subscribe(SDL_EventType type, EventCallback callback) {
    subscribers[type].push_back(callback);
  }

  bool resize_pending();
  glm::uvec2 get_pending_resize();

private:
  void OnKeyDown(SDL_Event e);
  void OnKeyUp(SDL_Event e);

  std::unordered_map<SDL_Keycode, bool> m_currentKeys;
  std::unordered_map<SDL_Keycode, bool> m_previousKeys;
  glm::vec2 m_mouse_pos{}, m_mouse_delta{};
  std::vector<std::weak_ptr<InputObserver>> m_observers;
  bool m_pending_resize;
  glm::uvec2 m_pending_resize_resolution;

  std::map<SDL_EventType, std::vector<EventCallback>> subscribers;
};
} // namespace Expectre
#endif // INPUTMANAGER_H
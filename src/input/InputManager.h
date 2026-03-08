#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H
#include <SDL3/SDL.h>
#include <glm/vec2.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

#include "observer.h"

namespace Expectre
{
  class InputManager
  {
  public:
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

  private:
    std::unordered_map<SDL_Keycode, bool> m_currentKeys;
    std::unordered_map<SDL_Keycode, bool> m_previousKeys;
    glm::vec2 m_mouse_pos{}, m_mouse_delta{};
    std::vector<std::weak_ptr<InputObserver>> m_observers;
  };
} // namespace Expectre
#endif // INPUTMANAGER_H
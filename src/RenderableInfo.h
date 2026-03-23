#ifndef RENDERABLE_INFO_H
#define RENDERABLE_INFO_H

#include "Material.h"
#include "Mesh.h"

#include <glm/glm.hpp>

namespace Expectre {

/// Description of a single draw call
/// Bridges Scene (ECS) → Renderer (GPU) without either side
/// knowing about the other's internals.
struct RenderableInfo {
  MeshHandle mesh{};
  MaterialHandle material{};
  glm::mat4 transform{1.0f};
};

} // namespace Expectre
#endif // RENDERABLE_INFO_H
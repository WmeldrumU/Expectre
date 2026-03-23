#ifndef SCENE_MESH_COMPONENT_H
#define SCENE_MESH_COMPONENT_H
#include "scene/Component.h"

#include "Material.h"
#include "Mesh.h"

namespace Expectre {

// Mesh component
// Manages the visual representation of an entity by handling its 3D mesh and
// material
class MeshComponent : public Component {
public:
  void set_mesh(MeshHandle m) { m_mesh = m; }
  void set_material(MaterialHandle mat) { m_material = mat; }

  MeshHandle get_mesh() const { return m_mesh; }
  MaterialHandle get_material() const { return m_material; }

private:
  MeshHandle m_mesh;
  MaterialHandle m_material;
};
} // namespace Expectre
#endif // SCENE_MESH_COMPONENT_H
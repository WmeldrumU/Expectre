#ifndef MESH_H
#define MESH_H

#include <assimp/Importer.hpp>
#include <assimp/defs.h>
#include <assimp/mesh.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Expectre {

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec3 normal;
  glm::vec2 tex_coord;
};

struct MeshHandle {
  uint64_t mesh_id = -1;
  explicit operator bool() const { return mesh_id != -1; }

  // Equality operator for use in unordered_map
  bool operator==(const MeshHandle &other) const {
    return mesh_id == other.mesh_id;
  }
};

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  std::string name; // Name of the mesh
                    // std::vector<Texture> textures; // Textures associated
                    // with the mesh
};

} // namespace Expectre

// Specialize std::hash for MeshHandle for use as a key in unordered maps
namespace std {
template <> struct hash<Expectre::MeshHandle> {
  std::size_t operator()(const Expectre::MeshHandle &handle) const {
    return std::hash<uint64_t>{}(handle.mesh_id);
  }
};
} // namespace std

#endif // MESH_H